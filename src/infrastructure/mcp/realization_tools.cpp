/** Owning project projections, durable dispatch fencing and read-only recovery. */
#include <algorithm>
#include <cmath>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <sunny/infrastructure/mcp/realization_tools.hpp>
#include <tuple>

namespace sunny::infrastructure {
using json = nlohmann::json;
using namespace sunny::core;

namespace {

json decline(const std::string& message, const std::string& state = "declined") {
    return {{"success", false}, {"state", state}, {"error", message}};
}

std::string hexadecimal(std::uint64_t value) {
    std::ostringstream output;
    output << std::hex << value;
    return output.str();
}

std::string project_key(const McpSession& session, std::uint64_t score) {
    return "w" + session.realization->metadata.workspace_namespace + "_s" + hexadecimal(score);
}
std::string binding_key(std::uint64_t part) {
    return "part_" + hexadecimal(part);
}

std::shared_ptr<RealizationStore> history(const McpSession& session, bool mutation) {
    auto& runtime = *session.realization;
    if (mutation && !runtime.namespace_saved_durably)
        throw std::runtime_error("Save this workspace durably before native authoring");
    if (!runtime.metadata.history_base_directory)
        throw std::runtime_error(
            "Save this workspace durably through workspace_save; native history is unavailable");
    if (!runtime.store) {
        auto opened = RealizationStore::open(*runtime.metadata.history_base_directory,
                                             runtime.metadata.workspace_namespace,
                                             RealizationStoreMode::OpenExisting);
        if (!opened) {
            runtime.history_error = opened.error().message;
            throw std::runtime_error("Native history is unavailable: " + opened.error().message);
        }
        runtime.store = std::move(*opened);
        runtime.history_error.reset();
    }
    if (mutation && !runtime.store->native_writes_available())
        throw std::runtime_error(runtime.store->blocked_reason().value_or(
            "Native history cannot fence a write durably"));
    return runtime.store;
}

const RealizationStoredAttempt*
latest(const RealizationStore& store, std::uint64_t score, std::uint64_t part) {
    const RealizationStoredAttempt* result = nullptr;
    for (const auto& [id, candidate] : store.attempts()) {
        static_cast<void>(id);
        if (candidate.intent.score_id.value == score && candidate.intent.part_id.value == part &&
            (!result || candidate.dispatch_ordinal > result->dispatch_ordinal))
            result = &candidate;
    }
    return result;
}

const ManagedOperationReceipt& last_receipt(const RealizationStoredAttempt& attempt) {
    return attempt.evidence.empty() ? attempt.intent.prepared : attempt.evidence.back();
}

struct BindingHistory {
    const RealizationStoredAttempt* acknowledged = nullptr;
    const RealizationStoredAttempt* unresolved = nullptr;
};

BindingHistory
binding_history(const RealizationStore& store, std::uint64_t score, std::uint64_t part) {
    BindingHistory result;
    for (const auto& [id, attempt] : store.attempts()) {
        static_cast<void>(id);
        if (attempt.intent.score_id.value != score || attempt.intent.part_id.value != part)
            continue;
        const auto& receipt = last_receipt(attempt);
        const bool no_effect = receipt.outcome == ManagedOperationOutcome::Declined ||
                               (receipt.outcome == ManagedOperationOutcome::NotSent &&
                                receipt.delivery == LomDeliveryState::NotSent && !receipt.journal);
        if (no_effect) continue;
        if (receipt.outcome == ManagedOperationOutcome::Acknowledged && !attempt.bindings.empty()) {
            if (!result.acknowledged ||
                attempt.dispatch_ordinal > result.acknowledged->dispatch_ordinal)
                result.acknowledged = &attempt;
        } else if (!result.unresolved ||
                   attempt.dispatch_ordinal > result.unresolved->dispatch_ordinal)
            result.unresolved = &attempt;
    }
    return result;
}

json attempt_summary(const RealizationStoredAttempt& attempt) {
    json output = {{"attempt_id", attempt.intent.attempt_id},
                   {"dispatch_ordinal", attempt.dispatch_ordinal},
                   {"dispatch_fenced", true},
                   {"score_id", attempt.intent.score_id.value},
                   {"part_id", attempt.intent.part_id.value},
                   {"project_revision", attempt.intent.project_revision},
                   {"desired_note_identity", attempt.intent.desired_note_identity},
                   {"receipt", managed_receipt_to_json(last_receipt(attempt))},
                   {"retry_authorized", false}};
    if (!attempt.bindings.empty())
        output["binding"] = managed_binding_to_json(attempt.bindings.back());
    return output;
}

struct DesiredPart {
    std::uint64_t score_id = 0;
    std::uint64_t part_id = 0;
    std::uint64_t revision = 0;
    ManagedClipProjection projection;
    std::vector<std::string> keys;
    json encoded;
    std::string identity;
    json warnings = json::array();
};

bool projection_matches(const json& desired, const json& observation) {
    const auto& manifest = observation.at("manifest");
    const auto& clip = manifest.at("clip");
    if (manifest.at("entire_clip_population_observed") != true || clip.at("start_marker") != 0.0 ||
        clip.at("end_marker") != desired.at("clip_end") ||
        clip.at("signature_numerator") != desired.at("signature_numerator") ||
        clip.at("signature_denominator") != desired.at("signature_denominator") ||
        clip.at("looping") != false || clip.at("muted") != false || clip.at("launch_mode") != 0 ||
        clip.at("launch_quantization") != 1 || clip.at("legato") != false ||
        clip.at("velocity_amount") != 0.0 || clip.at("has_groove") != false)
        return false;
    using Note = std::tuple<int, double, double, double, bool, double, double, double>;
    const auto notes = [](const json& population) {
        std::multiset<Note> result;
        for (const auto& note : population)
            result.emplace(note.at("pitch").get<int>(),
                           note.at("start_time").get<double>(),
                           note.at("duration").get<double>(),
                           note.at("velocity").get<double>(),
                           note.at("mute").get<bool>(),
                           note.at("probability").get<double>(),
                           note.at("velocity_deviation").get<double>(),
                           note.at("release_velocity").get<double>());
        return result;
    };
    return notes(desired.at("notes")) == notes(manifest.at("notes"));
}

bool verified_outcome(const ManagedOperationReceipt& receipt, const DesiredPart& desired) {
    if (receipt.outcome != ManagedOperationOutcome::Acknowledged || !receipt.journal) return false;
    const auto& actual = receipt.journal->at("result");
    if (!projection_matches(desired.encoded, actual)) return false;
    if (receipt.request.property_or_method == "sunny_managed_create_clip")
        return actual.at("observed_notes_match_request") == true &&
               actual.at("observed_clip_properties_match_request") == true;
    if (receipt.request.property_or_method == "sunny_managed_update_notes") {
        const auto& update = actual.at("note_update");
        return update.at("observed_updates_match_request") == true &&
               update.at("untouched_notes_preserved") == true &&
               update.at("note_ids_preserved") == true;
    }
    return false;
}

DesiredPart owning_part(const McpSession& session, const json& arguments) {
    DesiredPart desired;
    desired.score_id = detail::checked_integer<std::uint64_t>(arguments.at("score_id"), "score_id");
    desired.part_id = detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id");
    const auto owner = session.project->projects.find(desired.score_id);
    if (owner == session.project->projects.end())
        throw std::runtime_error("Score has no owning project");
    desired.revision = owner->second.revision;
    if (detail::checked_integer<std::uint64_t>(arguments.at("expected_project_revision"),
                                               "expected_project_revision") != desired.revision)
        throw std::runtime_error(
            "Project revision changed; inspect it before realizing the new content");
    const auto* score = session.score->find(desired.score_id);
    const auto* mix = session.mix->find(owner->second.mix_graph_id);
    if (!score || !mix) throw std::runtime_error("Owning project references missing documents");
    std::vector<const TimbreProfile*> profiles;
    for (const auto id : owner->second.profile_ids) {
        const auto* profile = session.timbre->find(id);
        if (!profile)
            throw std::runtime_error("Owning project references a missing Timbre profile");
        profiles.push_back(profile);
    }
    const auto diagnostics = validate_project(ProjectView{*score, profiles, *mix});
    if (std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& item) {
            return item.severity == ValidationSeverity::Error;
        }))
        throw std::runtime_error("Owning project is invalid");
    const auto part =
        std::find_if(score->parts.begin(), score->parts.end(), [&](const auto& value) {
            return value.id.value == desired.part_id;
        });
    if (part == score->parts.end()) throw std::runtime_error("Part does not belong to this Score");
    const auto ppq = detail::checked_integer_or<int>(arguments, "ppq", 480, "ppq");
    if (ppq < 1 || ppq > 32767) throw std::runtime_error("ppq must be between 1 and 32767");
    return desired;
}

DesiredPart desired_part(const McpSession& session,
                         const json& arguments,
                         DesiredPart desired,
                         LomTransport& transport) {
    const auto* score = session.score->find(desired.score_id);
    const auto part =
        std::find_if(score->parts.begin(), score->parts.end(), [&](const auto& value) {
            return value.id.value == desired.part_id;
        });
    const auto ppq = detail::checked_integer_or<int>(arguments, "ppq", 480, "ppq");
    auto target = transport.target_profile();
    if (!target || !*target)
        throw std::runtime_error("An actual matching bridge target profile is required");
    if (!validate_target_profile(**target))
        throw std::runtime_error("Bridge target profile contradicts its finite contract");
    if (!(**target).live_version.at_least(12, 3))
        throw std::runtime_error("Managed full-authoring operations require Live 12.3 or later");
    CommandBuffer recording;
    recording.set_target_profile(**target);
    recording.set_scene_count(1);
    auto compiled = formats::compile_to_ableton(*score, recording, ppq);
    if (!compiled)
        throw std::runtime_error("Score cannot be compiled to the finite native clip projection");
    auto projected =
        managed_clip_projection(recording, static_cast<int>(part - score->parts.begin()));
    if (!projected) throw std::runtime_error("Selected Part contains unsupported clip operations");
    desired.projection = std::move(*projected);
    auto midi = compile_to_midi(*score, ppq);
    if (!midi) throw std::runtime_error("Score MIDI provenance compilation failed");
    for (const auto& note : midi->midi.notes) {
        if (note.part_id.value != desired.part_id) continue;
        desired.keys.push_back("e" + std::to_string(note.attack_event_id.value) + "_n" +
                               std::to_string(note.attack_note_index));
    }
    desired.encoded = {{"clip_end", desired.projection.clip_end},
                       {"signature_numerator", desired.projection.signature_numerator},
                       {"signature_denominator", desired.projection.signature_denominator},
                       {"notes", desired.projection.notes}};
    auto identity = realization_note_identity(desired.keys, desired.encoded);
    if (!identity) throw std::runtime_error(identity.error().message);
    desired.identity = std::move(*identity);
    for (const auto& warning : compiled->warnings)
        desired.warnings.push_back(warning);
    return desired;
}

json dispatch(const std::shared_ptr<RealizationStore>& store,
              const DesiredPart& desired,
              const ManagedBridgeContext& context,
              const LomRequest& request,
              LomTransport& transport) {
    auto prepared = prepare_managed_operation(context, request);
    if (!prepared) return decline("Managed operation intent rejected");
    const auto payload = std::get<json>(request.args.at(0));
    const auto token = payload.at("operation_id").get<std::string>();
    RealizationAttemptIntent intent{token,
                                    ScoreId{desired.score_id},
                                    PartId{desired.part_id},
                                    desired.revision,
                                    desired.identity,
                                    desired.keys,
                                    desired.encoded,
                                    *prepared};
    auto fence = store->fence(intent);
    if (!fence)
        return decline("Native dispatch fence failed: " + fence.error().message, "not_dispatched");
    auto permit = fence->take_prepared();
    if (!permit) return decline("Native dispatch permit was already consumed", "not_dispatched");
    auto result = execute_managed_operation(*permit, transport);
    if (!result) {
        auto output = attempt_summary(*store->find(token));
        output["success"] = false;
        output["state"] = "reconciliation_required";
        output["error"] = "Native dispatch failed; the durable fence remains query-only";
        return output;
    }
    std::optional<ManagedBindingReceipt> binding;
    auto actual = managed_binding_receipt(*result);
    if (actual) binding = std::move(*actual);
    auto retained = store->append_evidence(token, *result, binding);
    auto output = attempt_summary(*store->find(token));
    output["actual_receipt"] = managed_receipt_to_json(*result);
    output["history_saved"] = retained.has_value();
    const bool verified = verified_outcome(*result, desired);
    output["success"] = retained && verified;
    output["state"] = !retained ? "history_write_failed"
                      : result->outcome == ManagedOperationOutcome::Acknowledged && !verified
                          ? "verification_mismatch"
                          : "observed_native_outcome";
    output["native_projection_verified"] = verified;
    output["warnings"] = desired.warnings;
    output["outside_clip_requests_applied"] = false;
    output["outside_clip_request_count"] = desired.projection.outside_clip_requests.size();
    output["scope"] = "selected_part_clip_and_notes";
    output["unapplied_domains"] = {"global Song settings",
                                   "other Parts",
                                   "Timbre devices and parameters",
                                   "Mix routing and automation"};
    if (!retained)
        output["error"] =
            "Native outcome obtained but evidence persistence failed: " + retained.error().message;
    return output;
}

json revision_schema() {
    return {{"type", "object"},
            {"properties",
             {{"score_id", {{"type", "integer"}, {"minimum", 1}}},
              {"part_id", {{"type", "integer"}, {"minimum", 1}}},
              {"expected_project_revision", {{"type", "integer"}, {"minimum", 1}}},
              {"ppq", {{"type", "integer"}, {"minimum", 1}, {"maximum", 32767}}}}},
            {"required", {"score_id", "part_id", "expected_project_revision"}}};
}

json note_changes(const RealizationStoredAttempt& previous,
                  const DesiredPart& desired,
                  const ManagedBindingReceipt& binding) {
    const auto& old = previous.intent.desired_projection;
    if (old.at("clip_end") != desired.encoded.at("clip_end") ||
        old.at("signature_numerator") != desired.encoded.at("signature_numerator") ||
        old.at("signature_denominator") != desired.encoded.at("signature_denominator"))
        throw std::runtime_error(
            "Clip length or meter revision requires a separately supported operation");
    if (!projection_matches(old, binding.observation))
        throw std::runtime_error(
            "Retained native clip or notes differ from the desired projection; inspect the "
            "verification mismatch before revision");
    const auto& identity = binding.observation.at("note_identity");
    if (identity.at("entire_clip_population_observed") != true)
        throw std::runtime_error("Existing note revision requires full native population evidence");
    std::map<std::string, json> native;
    std::set<int> used;
    for (std::size_t index = 0; index < previous.intent.desired_note_keys.size(); ++index) {
        const auto& expected = old.at("notes").at(index);
        std::vector<json> matches;
        for (const auto& observed : identity.at("notes")) {
            auto semantic = observed;
            semantic.erase("note_id");
            if (semantic == expected) matches.push_back(observed);
        }
        if (matches.size() != 1 || !used.insert(matches.front().at("note_id").get<int>()).second)
            throw std::runtime_error(
                "Native note association is ambiguous or differs from the retained intent");
        native.emplace(previous.intent.desired_note_keys.at(index), matches.front());
    }
    if (native.size() != identity.at("notes").size() || native.size() != desired.keys.size())
        throw std::runtime_error(
            "Adding or removing attacks requires the native note population revision operation");
    json changes = json::array();
    for (std::size_t index = 0; index < desired.keys.size(); ++index) {
        const auto found = native.find(desired.keys.at(index));
        if (found == native.end()) throw std::runtime_error("The desired attack identity is new");
        auto expected = found->second;
        const auto id = expected.at("note_id");
        expected.erase("note_id");
        const auto& wanted = desired.encoded.at("notes").at(index);
        json updates = json::object();
        for (const auto& [name, value] : wanted.items()) {
            if (expected.at(name) == value) continue;
            if (name == "probability" || name == "velocity_deviation")
                throw std::runtime_error("Probability and velocity deviation revisions are outside "
                                         "the finite update contract");
            updates[name] = value;
        }
        if (!updates.empty())
            changes.push_back({{"note_id", id}, {"expected", expected}, {"updates", updates}});
    }
    return changes;
}

} // namespace

void register_project_realization_tools(McpServer& server,
                                        const McpSession& session,
                                        LomTransport* transport) {
    auto domain = server.registration_scope(McpDocumentDomain::None);
    server.register_tool(
        "project_realization_create",
        "Create one owning project's native Part clip once, with a durable dispatch fence",
        revision_schema(),
        [session, transport](const json& arguments) -> json {
            try {
                auto owning_input = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained =
                    binding_history(*store, owning_input.score_id, owning_input.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                if (!transport || transport->records_without_execution() ||
                    !transport->is_connected()) {
                    if (const auto* prior = retained.acknowledged) {
                        auto output = attempt_summary(*prior);
                        output["success"] = false;
                        output["state"] = "existing_attempt";
                        output["current_desired_content_compared"] = false;
                        output["mutation_dispatched"] = false;
                        return output;
                    }
                    return decline("An actual bridge connection is required");
                }
                auto desired =
                    desired_part(session, arguments, std::move(owning_input), *transport);
                if (const auto* prior = retained.acknowledged) {
                    auto output = attempt_summary(*prior);
                    output["current_desired_content_compared"] = true;
                    output["success"] = false;
                    output["state"] = prior->intent.desired_note_identity == desired.identity
                                          ? "existing_attempt"
                                          : "update_required";
                    output["message"] = "Creation is already fenced; inspect or reconcile it, then "
                                        "revise the retained binding";
                    return output;
                }
                store = history(session, true);
                auto context = managed_bridge_context(*transport);
                auto token = store->new_attempt_id();
                if (!context || !token)
                    return decline("Native context or operation identity unavailable");
                auto request = make_managed_clip_request(*context,
                                                         *token,
                                                         project_key(session, desired.score_id),
                                                         binding_key(desired.part_id),
                                                         desired.projection);
                if (!request) return decline("Native clip intent rejected");
                return dispatch(store, desired, *context, *request, *transport);
            } catch (const std::exception& failure) {
                return decline(failure.what());
            }
        });
    server.register_tool(
        "project_realization_update",
        "Revise existing native note IDs from current owning project content",
        revision_schema(),
        [session, transport](const json& arguments) -> json {
            try {
                auto owning_input = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained =
                    binding_history(*store, owning_input.score_id, owning_input.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                if (!transport || transport->records_without_execution() ||
                    !transport->is_connected())
                    return decline("An actual bridge connection is required");
                store = history(session, true);
                auto desired =
                    desired_part(session, arguments, std::move(owning_input), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior) return decline("This Part has no native creation attempt");
                if (last_receipt(*prior).outcome != ManagedOperationOutcome::Acknowledged ||
                    prior->bindings.empty())
                    return decline(
                        "Reconcile the latest fenced operation before dependent mutation",
                        "reconciliation_required");
                auto changes = note_changes(*prior, desired, prior->bindings.back());
                if (changes.empty()) {
                    auto output = attempt_summary(*prior);
                    const auto& binding = prior->bindings.back();
                    auto observed = observe_managed_binding(
                        binding.context, binding.project_key, binding.binding_key, *transport);
                    auto actual = observed ? managed_observed_binding(*observed)
                                           : Result<ManagedBindingReceipt>{
                                                 std::unexpected(ErrorCode::ProtocolError)};
                    const bool unchanged =
                        actual && projection_matches(desired.encoded, actual->observation) &&
                        actual->observation.at("content_fingerprint") ==
                            binding.observation.at("content_fingerprint") &&
                        actual->observation.at("note_identity_fingerprint") ==
                            binding.observation.at("note_identity_fingerprint");
                    output["success"] = unchanged;
                    output["state"] = unchanged ? "unchanged_desired_notes"
                                                : "native_drift_or_observation_unavailable";
                    output["current_native_state_observed"] = actual.has_value();
                    output["mutation_dispatched"] = false;
                    if (observed) output["native_observation"] = observed->evidence;
                    return output;
                }
                auto context = managed_bridge_context(*transport);
                auto token = store->new_attempt_id();
                if (!context || !token)
                    return decline("Native context or operation identity unavailable");
                auto request = make_managed_note_update_request(
                    *context, *token, prior->bindings.back(), changes);
                if (!request)
                    return decline("Native in-place note update is unsupported for this binding");
                return dispatch(store, desired, *context, *request, *transport);
            } catch (const std::exception& failure) {
                return decline(failure.what());
            }
        });
    auto lane_schema = revision_schema();
    lane_schema["properties"]["lane_index"] = {{"type", "integer"}, {"minimum", 0}};
    lane_schema["required"].push_back("lane_index");
    server.register_tool(
        "project_realization_author_mix_lane",
        "Author one owning Mix Step panning lane on retained native Part identities; existing "
        "envelopes are preserved",
        std::move(lane_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owning_input = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained =
                    binding_history(*store, owning_input.score_id, owning_input.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                if (!transport || transport->records_without_execution() ||
                    !transport->is_connected())
                    return decline("An actual bridge connection is required");
                store = history(session, true);
                auto desired =
                    desired_part(session, arguments, std::move(owning_input), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior || prior->bindings.empty())
                    return decline("Create and inspect this Part's native clip first");
                const auto binding = prior->bindings.back();
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise the native notes before authoring the current Mix lane");
                const auto* score = session.score->find(desired.score_id);
                const auto& owner = session.project->projects.at(desired.score_id);
                const auto* mix = session.mix->find(owner.mix_graph_id);
                const auto lane_index = detail::checked_integer<std::uint64_t>(
                    arguments.at("lane_index"), "lane_index");
                if (lane_index >= mix->automation.size()) return decline("Mix lane does not exist");
                const auto& authored = mix->automation.at(static_cast<std::size_t>(lane_index));
                if (authored.target !=
                        "channels[" + std::to_string(desired.part_id) + "].spatial.pan" ||
                    authored.interpolation != InterpolationMode::Step)
                    return decline("This operation supports the selected Part's Step panning lane");
                const auto& mixer = binding.observation.at("manifest").at("mixer");
                const auto& parameter = mixer.at("panning");
                if (mixer.at("panning_mode") != 0 || parameter.at("min") != -1.0 ||
                    parameter.at("max") != 1.0)
                    return decline("Native stereo panning requires the observed [-1,+1] domain");
                json points = json::array();
                double previous = -1.0;
                for (const auto& point : authored.breakpoints) {
                    auto absolute = score_time_to_absolute_beat(point.time, score->time_map);
                    if (!absolute) return decline("Mix breakpoint has invalid Score time");
                    const double time = absolute->to_float() * 4.0;
                    const double value = static_cast<double>(point.value);
                    if (!std::isfinite(time) || time <= previous ||
                        time >= desired.projection.clip_end || !std::isfinite(value) ||
                        value < -1.0 || value > 1.0)
                        return decline(
                            "Mix lane times or values exceed the native panning interval");
                    if (points.empty() && time != 0.0)
                        return decline("The finite Step lane must start at Score origin");
                    points.push_back({{"time", time}, {"value", value}});
                    previous = time;
                }
                if (points.empty()) return decline("Mix lane has no breakpoints");
                const json selector = {{"kind", "panning"}};
                const json lane = {{"parameter", selector},
                                   {"interpolation", "step"},
                                   {"clip_end", desired.projection.clip_end},
                                   {"points", points}};
                for (const auto& [id, attempt] : store->attempts()) {
                    static_cast<void>(id);
                    if (attempt.intent.score_id != ScoreId{desired.score_id} ||
                        attempt.intent.part_id != PartId{desired.part_id} ||
                        last_receipt(attempt).outcome != ManagedOperationOutcome::Acknowledged ||
                        attempt.intent.prepared.request.property_or_method !=
                            "sunny_managed_author_envelope")
                        continue;
                    const auto& payload =
                        std::get<json>(attempt.intent.prepared.request.args.at(0));
                    if (payload.at("lane").at("parameter") == selector) {
                        auto output = attempt_summary(attempt);
                        output["success"] = false;
                        output["state"] = "existing_envelope_preserved";
                        output["message"] =
                            "The existing native lane is retained; complete envelope "
                            "revision is not supported by sampled readback";
                        return output;
                    }
                }
                auto context = managed_bridge_context(*transport);
                auto token = store->new_attempt_id();
                if (!context || !token) return decline("Native context or token unavailable");
                auto request = make_managed_envelope_request(*context, *token, binding, lane);
                if (!request)
                    return decline("Native lane admission is unavailable for this binding");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "selected_part_mix_panning_step_lane";
                output["unapplied_domains"] = {"global Song settings",
                                               "other Parts",
                                               "Timbre devices and parameters",
                                               "Mix routing, static mixer values and other lanes"};
                output["complete_project_realization"] = false;
                output["lane_index"] = lane_index;
                output["lane_target"] = authored.target;
                output["complete_envelope_population_observed"] = false;
                output["native_lane_samples_verified"] = false;
                if (!output.contains("actual_receipt")) return output;
                auto actual_receipt = managed_receipt_from_json(output.at("actual_receipt"));
                if (!actual_receipt ||
                    actual_receipt->outcome != ManagedOperationOutcome::Acknowledged)
                    return output;
                std::vector<double> times, expected;
                for (std::size_t index = 0; index < points.size(); ++index) {
                    const double start = points.at(index).at("time").get<double>();
                    const double stop = index + 1 < points.size()
                                            ? points.at(index + 1).at("time").get<double>()
                                            : desired.projection.clip_end;
                    const double value = points.at(index).at("value").get<double>();
                    times.push_back(start);
                    expected.push_back(value);
                    const double midpoint = start + (stop - start) / 2.0;
                    if (midpoint > start && midpoint < stop) {
                        times.push_back(midpoint);
                        expected.push_back(value);
                    }
                }
                auto sampled = sample_managed_envelope(binding.context,
                                                       binding.project_key,
                                                       binding.binding_key,
                                                       selector,
                                                       times,
                                                       *transport);
                bool verified =
                    sampled && sampled->binding.outcome == ManagedObservationOutcome::Observed &&
                    sampled->evidence.at("envelope").at("has_envelope") == true &&
                    projection_matches(desired.encoded, sampled->evidence.at("observation"));
                if (sampled) output["native_lane_observation"] = sampled->evidence;
                if (verified) {
                    const auto& samples = sampled->evidence.at("envelope").at("samples");
                    verified = samples.size() == expected.size();
                    for (std::size_t index = 0; verified && index < expected.size(); ++index)
                        verified = samples.at(index).at("time") == times.at(index) &&
                                   std::abs(samples.at(index).at("value").get<double>() -
                                            expected.at(index)) <= 1.0e-6;
                }
                output["native_calls_acknowledged"] = true;
                output["native_lane_samples_verified"] = verified;
                output["sample_tolerance_internal"] = 1.0e-6;
                output["sample_coverage"] = "step_boundaries_and_interval_midpoints";
                output["success"] = output.at("history_saved") == true && verified;
                output["state"] = output.at("history_saved") != true ? "history_write_failed"
                                  : verified ? "native_lane_samples_verified"
                                             : "verification_mismatch_or_readback_unavailable";
                return output;
            } catch (const std::exception& failure) {
                return decline(failure.what());
            }
        });
    server.register_tool(
        "project_realization_inspect",
        "Inspect retained native history and actual content; attempt_id also addresses retired "
        "Parts",
        {{"type", "object"},
         {"properties",
          {{"attempt_id", {{"type", "string"}}},
           {"score_id", {{"type", "integer"}, {"minimum", 1}}},
           {"part_id", {{"type", "integer"}, {"minimum", 1}}}}}},
        [session, transport](const json& arguments) -> json {
            try {
                auto store = history(session, false);
                const RealizationStoredAttempt* attempt = nullptr;
                if (arguments.contains("attempt_id")) {
                    if (arguments.contains("score_id") || arguments.contains("part_id"))
                        return decline("Supply attempt_id or the Score/Part pair");
                    attempt = store->find(arguments.at("attempt_id").get<std::string>());
                } else {
                    if (!arguments.contains("score_id") || !arguments.contains("part_id"))
                        return decline("Supply attempt_id or the Score/Part pair");
                    attempt = latest(
                        *store,
                        detail::checked_integer<std::uint64_t>(arguments.at("score_id"),
                                                               "score_id"),
                        detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id"));
                }
                if (!attempt) return decline("No retained native attempt exists");
                auto output = attempt_summary(*attempt);
                output["success"] = true;
                output["current_native_state_observed"] = false;
                if (!transport || transport->records_without_execution()) return output;
                auto observed =
                    observe_managed_binding(attempt->intent.prepared.context,
                                            project_key(session, attempt->intent.score_id.value),
                                            binding_key(attempt->intent.part_id.value),
                                            *transport);
                if (observed) {
                    output["native_observation"] = observed->evidence;
                    output["current_native_state_observed"] =
                        observed->outcome == ManagedObservationOutcome::Observed;
                } else
                    output["native_observation_error"] = "Current native observation unavailable";
                return output;
            } catch (const std::exception& failure) {
                return decline(failure.what());
            }
        });
    server.register_tool(
        "project_realization_reconcile",
        "Query an uncertain fenced attempt and retain evidence without any mutation retry",
        {{"type", "object"},
         {"properties", {{"attempt_id", {{"type", "string"}}}}},
         {"required", {"attempt_id"}}},
        [session, transport](const json& arguments) -> json {
            try {
                auto store = history(session, true);
                const auto token = arguments.at("attempt_id").get<std::string>();
                const auto* attempt = store->find(token);
                if (!attempt) return decline("No retained native attempt exists");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto observed = reconcile_managed_operation(last_receipt(*attempt), *transport);
                if (!observed)
                    return decline(
                        "Reconciliation response rejected; retained history remains unchanged");
                std::optional<ManagedBindingReceipt> binding;
                auto actual = managed_binding_receipt(*observed);
                if (actual) binding = std::move(*actual);
                auto saved = store->append_evidence(token, *observed, binding);
                auto output = attempt_summary(*store->find(token));
                output["success"] = saved.has_value();
                output["history_saved"] = saved.has_value();
                output["mutation_retried"] = false;
                if (!saved) output["error"] = saved.error().message;
                return output;
            } catch (const std::exception& failure) {
                return decline(failure.what());
            }
        });
}

} // namespace sunny::infrastructure
