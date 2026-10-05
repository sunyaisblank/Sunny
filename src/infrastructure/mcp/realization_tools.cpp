/** Owning project projections, durable dispatch fencing and read-only recovery. */
#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/rhythm/meter.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/infrastructure/ableton/detail/managed_routing.hpp>
#include <sunny/infrastructure/ableton/detail/native_unit_decimal.hpp>
#include <sunny/infrastructure/ableton/detail/realization_history.hpp>
#include <sunny/infrastructure/ableton/managed_clip_revision.hpp>
#include <sunny/infrastructure/ableton/managed_device_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>
#include <sunny/infrastructure/ableton/managed_envelope_revision.hpp>
#include <sunny/infrastructure/ableton/managed_mixer.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_routing.hpp>
#include <sunny/infrastructure/ableton/managed_song_settings.hpp>
#include <sunny/infrastructure/ableton/native_effect_plan.hpp>
#include <sunny/infrastructure/ableton/native_mix_plan.hpp>
#include <sunny/infrastructure/ableton/native_timbre_plan.hpp>
#include <sunny/infrastructure/ableton/native_units.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <sunny/infrastructure/mcp/realization_coordinator.hpp>
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
    return realization_detail::last_receipt(attempt);
}

bool group_only(const RealizationStoredAttempt& attempt) {
    const auto& request = attempt.intent.prepared.request;
    return request.property_or_method == "sunny_managed_apply_routing" &&
           std::get<json>(request.args.at(0)).at("intent").at("kind") == "adopt_group";
}

struct BindingHistory {
    const RealizationStoredAttempt* acknowledged = nullptr;
    const RealizationStoredAttempt* unresolved = nullptr;
};

BindingHistory
binding_history(const RealizationStore& store, std::uint64_t score, std::uint64_t part) {
    BindingHistory result;
    std::uint64_t adoption_ordinal = 0;
    for (const auto& [id, attempt] : store.attempts()) {
        static_cast<void>(id);
        if (attempt.intent.score_id.value == score && attempt.intent.part_id.value == part &&
            attempt.intent.prepared.request.property_or_method == "sunny_managed_adopt_clip" &&
            last_receipt(attempt).outcome == ManagedOperationOutcome::Acknowledged &&
            !attempt.bindings.empty())
            adoption_ordinal = std::max(adoption_ordinal, attempt.dispatch_ordinal);
    }
    // Explicit current-object adoption establishes a new authority boundary.
    // Earlier uncertainty remains saved and queryable; it never proves absence.
    for (const auto& [id, attempt] : store.attempts()) {
        static_cast<void>(id);
        if (attempt.intent.score_id.value != score || attempt.intent.part_id.value != part)
            continue;
        if (group_only(attempt)) continue;
        const auto& receipt = last_receipt(attempt);
        const bool no_effect = receipt.outcome == ManagedOperationOutcome::Declined ||
                               (receipt.outcome == ManagedOperationOutcome::NotSent &&
                                receipt.delivery == LomDeliveryState::NotSent && !receipt.journal);
        if (no_effect) continue;
        if (receipt.outcome == ManagedOperationOutcome::Acknowledged && !attempt.bindings.empty()) {
            if (!result.acknowledged ||
                attempt.dispatch_ordinal > result.acknowledged->dispatch_ordinal)
                result.acknowledged = &attempt;
        } else if (attempt.intent.prepared.request.property_or_method !=
                       "sunny_managed_apply_song_settings" &&
                   attempt.dispatch_ordinal > adoption_ordinal &&
                   (!result.unresolved ||
                    attempt.dispatch_ordinal > result.unresolved->dispatch_ordinal))
            result.unresolved = &attempt;
    }
    return result;
}

ManagedBindingReceipt current_binding(const RealizationStore& store,
                                      const RealizationStoredAttempt& prior) {
    auto binding = prior.bindings.back();
    std::vector<const RealizationStoredAttempt*> routing;
    for (const auto& [id, attempt] : store.attempts()) {
        static_cast<void>(id);
        const auto& receipt = last_receipt(attempt);
        if (attempt.dispatch_ordinal > prior.dispatch_ordinal &&
            receipt.request.property_or_method == "sunny_managed_apply_routing" &&
            receipt.outcome == ManagedOperationOutcome::Acknowledged && receipt.journal &&
            receipt.context.bridge_instance == binding.context.bridge_instance &&
            receipt.context.document_token == binding.context.document_token &&
            !group_only(attempt))
            routing.push_back(&attempt);
    }
    std::ranges::sort(routing, {}, &RealizationStoredAttempt::dispatch_ordinal);
    for (const auto* attempt : routing) {
        const auto& receipt = last_receipt(*attempt);
        const auto& payload = std::get<json>(receipt.request.args.at(0));
        const auto affected = managed_routing_affected_bindings(
            receipt.context, payload, receipt.journal->at("result"));
        if (!affected) throw std::runtime_error("Retained routing delta is malformed");
        for (const auto& candidate : *affected) {
            if (candidate.project_key != binding.project_key ||
                candidate.binding_key != binding.binding_key)
                continue;
            const auto& before = payload.at("approved_preview").at("affected_bindings");
            const auto original = std::ranges::find_if(before, [&](const auto& value) {
                return value.at("project_key") == binding.project_key &&
                       value.at("binding_key") == binding.binding_key;
            });
            const auto& observed = binding.observation;
            if (original == before.end() ||
                original->at("track_index") != observed.at("track_index") ||
                original->at("slot_index") != observed.at("slot_index") ||
                original->at("before_manifest") != observed.at("manifest") ||
                original->at("guard").at("content_fingerprint") !=
                    observed.at("content_fingerprint") ||
                original->at("guard").at("note_identity_fingerprint") !=
                    observed.at("note_identity_fingerprint") ||
                original->at("guard").at("device_identity_fingerprint") !=
                    observed.value("device_identity_fingerprint", json(nullptr)) ||
                original->at("before_group_authority") !=
                    observed.value("group_authority", json(nullptr)) ||
                original->at("before_group_authority_fingerprint") !=
                    observed.value("group_authority_fingerprint", json(nullptr)))
                throw std::runtime_error("Retained routing delta does not join this Part baseline; "
                                         "explicitly adopt its current native state");
            binding = candidate;
        }
    }
    return binding;
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

class ReconciliationRequired final : public std::runtime_error {
  public:
    json response;
    explicit ReconciliationRequired(const RealizationStoredAttempt& attempt)
        : std::runtime_error("Reconcile original Part operation " + attempt.intent.attempt_id),
          response(attempt_summary(attempt)) {
        response["success"] = false;
        response["state"] = "reconciliation_required";
        response["mutation_dispatched"] = false;
    }
};

json workflow_failure(const std::exception& failure) {
    if (const auto* pending = dynamic_cast<const ReconciliationRequired*>(&failure))
        return pending->response;
    return decline(failure.what());
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
    std::optional<std::string> historical_projection_attempt;
};

using NativeNoteValues = std::tuple<int, double, double, double, bool, double, double, double>;

NativeNoteValues native_note_values(const json& note) {
    return {note.at("pitch").get<int>(),
            note.at("start_time").get<double>(),
            note.at("duration").get<double>(),
            note.at("velocity").get<double>(),
            note.at("mute").get<bool>(),
            note.at("probability").get<double>(),
            note.at("velocity_deviation").get<double>(),
            note.at("release_velocity").get<double>()};
}

std::map<std::string, json> native_note_associations(const std::vector<std::string>& keys,
                                                     const json& projection,
                                                     const json& observation) {
    const auto& identity = observation.at("note_identity");
    if (identity.at("entire_clip_population_observed") != true ||
        keys.size() != projection.at("notes").size())
        throw std::runtime_error("Full native population and compiled attack keys are required");
    std::map<NativeNoteValues, std::vector<const json*>> by_values;
    for (const auto& observed : identity.at("notes"))
        by_values[native_note_values(observed)].push_back(&observed);
    std::map<std::string, json> native;
    std::set<int> used;
    for (std::size_t index = 0; index < keys.size(); ++index) {
        const auto found = by_values.find(native_note_values(projection.at("notes").at(index)));
        if (found == by_values.end() || found->second.size() != 1 ||
            !used.insert(found->second.front()->at("note_id").get<int>()).second ||
            !native.emplace(keys.at(index), *found->second.front()).second)
            throw std::runtime_error("Native note association is ambiguous or differs from the "
                                     "compiled attack intent");
    }
    if (native.size() != identity.at("notes").size())
        throw std::runtime_error("The native population includes unassociated notes");
    return native;
}

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
    const auto notes = [](const json& population) {
        std::multiset<NativeNoteValues> result;
        for (const auto& note : population)
            result.insert(native_note_values(note));
        return result;
    };
    return notes(desired.at("notes")) == notes(manifest.at("notes"));
}

bool verified_outcome(const ManagedOperationReceipt& receipt, const DesiredPart& desired) {
    if (receipt.outcome != ManagedOperationOutcome::Acknowledged || !receipt.journal) return false;
    const auto& actual = receipt.journal->at("result");
    if (receipt.request.property_or_method == "sunny_managed_apply_routing") {
        if (actual.contains("group_adoption")) return true;
        if (!projection_matches(desired.encoded, actual)) return false;
        const auto& routing = actual.at("routing");
        return routing.at("desired_match") == true &&
               routing.at("untouched_observed_state_preserved") == true;
    }
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
    if (receipt.request.property_or_method == "sunny_managed_revise_note_population") {
        const auto& update = actual.at("note_population_update");
        return update.at("observed_changes_match_request") == true &&
               update.at("observed_deletions_absent") == true &&
               update.at("observed_additions_match_request") == true &&
               update.at("untouched_notes_preserved") == true &&
               update.at("retained_note_ids_preserved") == true &&
               update.at("observed_population_cardinality_match") == true;
    }
    if (receipt.request.property_or_method == "sunny_managed_insert_device" ||
        receipt.request.property_or_method == "sunny_managed_update_device_parameters") {
        const auto& update = actual.at("device_update");
        return update.at("clip_and_note_ids_preserved") == true &&
               std::all_of(
                   update.at("readbacks").begin(),
                   update.at("readbacks").end(),
                   [](const auto& readback) { return readback.at("matches_intent") == true; });
    }
    if (receipt.request.property_or_method == "sunny_managed_replace_envelope") {
        const auto& update = actual.at("envelope_replacement");
        return update.at("observed_step_samples_match_request") == true &&
               update.at("note_ids_and_values_preserved") == true &&
               update.at("other_finite_properties_preserved") == true &&
               update.at("device_identity_preserved") == true;
    }
    if (receipt.request.property_or_method == "sunny_managed_update_device_modes")
        return actual.at("device_mode_update").at("clip_and_note_ids_preserved") == true;
    if (receipt.request.property_or_method == "sunny_managed_adopt_devices")
        return actual.at("device_adoption").at("current_values_match_approved_intent") == true;
    if (receipt.request.property_or_method == "sunny_managed_adopt_clip")
        return actual.at("adoption").at("authority_origin") == "explicit_adoption";
    if (receipt.request.property_or_method == "sunny_managed_update_clip_geometry") {
        const auto& update = actual.at("clip_geometry_update");
        constexpr std::array flags{"observed_geometry_matches_request",
                                   "note_values_preserved",
                                   "note_ids_preserved",
                                   "note_cardinality_preserved",
                                   "other_finite_properties_preserved",
                                   "device_identity_preserved"};
        return std::all_of(
            flags.begin(), flags.end(), [&](const auto* flag) { return update.at(flag) == true; });
    }
    if (receipt.request.property_or_method == "sunny_managed_apply_song_settings") {
        const auto& update = actual.at("song_settings");
        return update.at("desired_settings_match") == true &&
               update.at("observed_untouched_state_preserved") == true &&
               update.at("clip_and_note_ids_preserved") == true;
    }
    if (receipt.request.property_or_method == "sunny_managed_adopt_static_mixer")
        return actual.contains("mixer_adoption");
    if (receipt.request.property_or_method == "sunny_managed_update_static_mixer") {
        const auto& update = actual.at("mixer_update");
        return update.at("observed_untouched_state_preserved") == true &&
               update.at("clip_and_note_ids_preserved") == true;
    }
    return false;
}

const RealizationStoredAttempt* global_settings_blocker(const RealizationStore& store,
                                                        const ManagedBridgeContext& context) {
    return realization_detail::set_wide_settings_blocker(store, &context);
}

DesiredPart owning_project_revision(const McpSession& session, const json& arguments) {
    DesiredPart desired;
    desired.score_id = detail::checked_integer<std::uint64_t>(arguments.at("score_id"), "score_id");
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
    return desired;
}

DesiredPart owning_part(const McpSession& session, const json& arguments) {
    auto desired = owning_project_revision(session, arguments);
    desired.part_id = detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id");
    const auto* score = session.score->find(desired.score_id);
    const auto part =
        std::find_if(score->parts.begin(), score->parts.end(), [&](const auto& value) {
            return value.id.value == desired.part_id;
        });
    if (part == score->parts.end()) throw std::runtime_error("Part does not belong to this Score");
    const auto ppq = detail::checked_integer_or<int>(arguments, "ppq", 480, "ppq");
    if (ppq < 1 || ppq > 32767) throw std::runtime_error("ppq must be between 1 and 32767");
    return desired;
}

AbletonTargetProfile reviewed_native_target(LomTransport& transport) {
    // This read-only probe reconnects and verifies source identity before any
    // managed dispatch fence. A lost connection never replays a native setter.
    if (!transport.ensure_connected())
        throw std::runtime_error("An actual bridge connection is required");
    auto target = transport.target_profile();
    if (!target || !*target)
        throw std::runtime_error("An actual matching bridge target profile is required");
    if (!validate_target_profile(**target))
        throw std::runtime_error("Bridge target profile contradicts its finite contract");
    const auto version = (**target).live_version;
    if (version.major != 12 || (version.minor != 3 && version.minor != 4))
        throw std::runtime_error("Managed authoring requires reviewed Live 12.3.x or 12.4.x; "
                                 "qualify a newer version before native writes");
    return **target;
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
    const auto target = reviewed_native_target(transport);
    CommandBuffer recording;
    recording.set_target_profile(target);
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

bool active_part(const McpSession& session, const DesiredPart& desired) {
    const auto* score = session.score->find(desired.score_id);
    return std::ranges::any_of(score->parts,
                               [&](const auto& part) { return part.id.value == desired.part_id; });
}

ManagedClipProjection retained_projection(const RealizationStoredAttempt& prior) {
    const auto& encoded = prior.intent.desired_projection;
    return {encoded.at("clip_end").get<double>(),
            encoded.at("signature_numerator").get<int>(),
            encoded.at("signature_denominator").get<int>(),
            encoded.at("notes"),
            {}};
}

DesiredPart stage_part(const DesiredPart& final, const ManagedClipRevisionStage& stage) {
    auto desired = final;
    desired.projection = stage.projection;
    desired.projection.outside_clip_requests = final.projection.outside_clip_requests;
    desired.keys = stage.desired_note_keys;
    desired.encoded = {{"clip_end", desired.projection.clip_end},
                       {"signature_numerator", desired.projection.signature_numerator},
                       {"signature_denominator", desired.projection.signature_denominator},
                       {"notes", desired.projection.notes}};
    auto identity = realization_note_identity(desired.keys, desired.encoded);
    if (!identity) throw std::runtime_error(identity.error().message);
    desired.identity = std::move(*identity);
    return desired;
}

DesiredPart historical_part(DesiredPart desired, const RealizationStoredAttempt& prior) {
    desired.encoded = prior.intent.desired_projection;
    desired.keys = prior.intent.desired_note_keys;
    desired.identity = prior.intent.desired_note_identity;
    desired.projection.clip_end = desired.encoded.at("clip_end");
    desired.projection.signature_numerator = desired.encoded.at("signature_numerator");
    desired.projection.signature_denominator = desired.encoded.at("signature_denominator");
    desired.projection.notes = desired.encoded.at("notes");
    // This historical source proves the musical projection only. An envelope
    // ACK, for example, retains complete notes/geometry while its unsampled
    // automation population remains outside the separately qualified lane.
    if (prior.bindings.empty() ||
        last_receipt(prior).outcome != ManagedOperationOutcome::Acknowledged ||
        !projection_matches(desired.encoded, prior.bindings.back().observation))
        throw std::runtime_error(
            "Part has no independently verified historical musical projection");
    static_cast<void>(
        native_note_associations(desired.keys, desired.encoded, prior.bindings.back().observation));
    desired.historical_projection_attempt = prior.intent.attempt_id;
    return desired;
}

DesiredPart
recovery_part(const McpSession& session, const json& arguments, LomTransport& transport) {
    auto desired = owning_project_revision(session, arguments);
    desired.part_id = detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id");
    const auto source = arguments.value("projection_source", std::string{"current_owning_score"});
    if (source != "current_owning_score" && source != "retained_verified_realization")
        throw std::runtime_error("Unknown recovery musical projection source");
    if (active_part(session, desired) && source == "current_owning_score")
        return desired_part(session, arguments, owning_part(session, arguments), transport);
    if (!active_part(session, desired) && arguments.contains("projection_source") &&
        source == "current_owning_score")
        throw std::runtime_error("Removed Part has no current owning Score projection");
    const auto store = history(session, false);
    const auto retained = binding_history(*store, desired.score_id, desired.part_id);
    if (!retained.acknowledged)
        throw std::runtime_error("Part has no independently verified native history");
    static_cast<void>(reviewed_native_target(transport));
    return historical_part(std::move(desired), *retained.acknowledged);
}

void recovery_provenance(json& output, const McpSession& session, const DesiredPart& desired) {
    output["projection_source"] =
        desired.historical_projection_attempt
            ? (active_part(session, desired) ? "retained_verified_realization"
                                             : "retained_verified_removed_part")
            : "current_owning_score";
    if (desired.historical_projection_attempt) {
        output["historical_projection_attempt"] = *desired.historical_projection_attempt;
        output["retained_projection_identity"] = desired.identity;
        const auto source = history(session, false)->find(*desired.historical_projection_attempt);
        if (!source) throw std::runtime_error("Selected historical musical source is unavailable");
        output["source_project_revision"] = source->intent.project_revision;
    }
}

json dispatch(const std::shared_ptr<RealizationStore>& store,
              const DesiredPart& desired,
              const ManagedBridgeContext& context,
              const LomRequest& request,
              LomTransport& transport) {
    // Explicit current authority can be recovered without claiming an earlier
    // global operation absent. Clip/device adoption performs no native setters;
    // fresh Set-wide approval is the only operation that can renew this boundary.
    const auto& payload = std::get<json>(request.args.at(0));
    const bool current_adoption =
        request.property_or_method == "sunny_managed_adopt_clip" ||
        request.property_or_method == "sunny_managed_adopt_devices" ||
        request.property_or_method == "sunny_managed_adopt_static_mixer" ||
        (request.property_or_method == "sunny_managed_apply_routing" &&
         (payload.at("intent").at("kind") == "adopt_return" ||
          payload.at("intent").at("kind") == "adopt_group"));
    if (!current_adoption) {
        const auto* blocked = request.property_or_method == "sunny_managed_apply_song_settings"
                                  ? realization_detail::return_creation_blocker(*store, &context)
                                  : global_settings_blocker(*store, context);
        if (blocked) {
            auto output = attempt_summary(*blocked);
            output["success"] = false;
            output["state"] = "set_wide_reconciliation_required";
            output["error"] =
                "Reconcile the original Set-wide token, or explicitly approve "
                "fresh current authority for that Set-wide family before native setters";
            output["mutation_dispatched"] = false;
            return output;
        }
    }
    auto prepared = prepare_managed_operation(context, request);
    if (!prepared) return decline("Managed operation intent rejected");
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
        output["mutation_dispatched"] = true;
        return output;
    }
    std::optional<ManagedBindingReceipt> binding;
    auto actual = managed_binding_receipt(*result);
    if (actual) binding = std::move(*actual);
    auto retained = store->append_evidence(token, *result, binding);
    auto output = attempt_summary(*store->find(token));
    output["actual_receipt"] = managed_receipt_to_json(*result);
    output["mutation_dispatched"] = true;
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

struct NotePopulationChanges {
    json changes = json::array();
    json deletions = json::array();
    json additions = json::array();
    bool empty() const { return changes.empty() && deletions.empty() && additions.empty(); }
};

NotePopulationChanges note_population_changes(const RealizationStoredAttempt& previous,
                                              const DesiredPart& desired,
                                              const ManagedBindingReceipt& binding) {
    const auto& old = previous.intent.desired_projection;
    if (old.at("clip_end") != desired.encoded.at("clip_end") ||
        old.at("signature_numerator") != desired.encoded.at("signature_numerator") ||
        old.at("signature_denominator") != desired.encoded.at("signature_denominator"))
        throw std::runtime_error("Clip length or meter revision requires a "
                                 "separately supported operation");
    if (!projection_matches(old, binding.observation))
        throw std::runtime_error("Retained native clip or notes differ from the "
                                 "desired projection; inspect the "
                                 "verification mismatch before revision");
    const auto native =
        native_note_associations(previous.intent.desired_note_keys, old, binding.observation);
    const auto event_cardinalities = [](const std::vector<std::string>& keys) {
        std::map<std::string, std::size_t> counts;
        for (const auto& key : keys)
            ++counts[key.substr(0, key.find("_n"))];
        return counts;
    };
    const auto old_cardinalities = event_cardinalities(previous.intent.desired_note_keys);
    const auto new_cardinalities = event_cardinalities(desired.keys);
    for (const auto& [event, count] : old_cardinalities) {
        const auto current = new_cardinalities.find(event);
        if (current != new_cardinalities.end() && current->second != count)
            throw std::runtime_error("UnsupportedChordCardinalityRevision: preserve "
                                     "existing ordinal note associations");
    }
    NotePopulationChanges result;
    std::set<std::string> wanted_keys(desired.keys.begin(), desired.keys.end());
    for (const auto& [key, observed] : native) {
        if (wanted_keys.contains(key)) continue;
        auto expected = observed;
        const auto id = expected.at("note_id");
        expected.erase("note_id");
        result.deletions.push_back({{"note_id", id}, {"expected", expected}});
    }
    for (std::size_t index = 0; index < desired.keys.size(); ++index) {
        const auto& wanted = desired.encoded.at("notes").at(index);
        const auto found = native.find(desired.keys.at(index));
        if (found == native.end()) {
            result.additions.push_back({{"note_key", desired.keys.at(index)}, {"note", wanted}});
            continue;
        }
        auto expected = found->second;
        const auto id = expected.at("note_id");
        expected.erase("note_id");
        json updates = json::object();
        for (const auto& [name, value] : wanted.items()) {
            if (expected.at(name) == value) continue;
            if (name == "probability" || name == "velocity_deviation")
                throw std::runtime_error("Probability and velocity deviation revisions are outside "
                                         "the finite update contract");
            updates[name] = value;
        }
        if (!updates.empty())
            result.changes.push_back(
                {{"note_id", id}, {"expected", expected}, {"updates", updates}});
    }
    return result;
}

json note_changes(const RealizationStoredAttempt& previous,
                  const DesiredPart& desired,
                  const ManagedBindingReceipt& binding) {
    auto result = note_population_changes(previous, desired, binding);
    if (!result.additions.empty() || !result.deletions.empty())
        throw std::runtime_error(
            "Revise the native attack population before authoring other domains");
    return result.changes;
}

std::vector<NativeTimbreSelection> timbre_selections(const json& arguments) {
    std::vector<NativeTimbreSelection> selections;
    const auto& encoded = arguments.at("selections");
    if (!encoded.is_array() || encoded.size() > NATIVE_TIMBRE_MAX_SELECTIONS)
        throw std::runtime_error("At most four explicit native Timbre selections are supported");
    for (const auto& item : encoded) {
        if (!item.is_object() || item.size() != 3 || !item.contains("source_path") ||
            !item.contains("capability_id") || !item.contains("tolerance") ||
            !item.at("tolerance").is_number())
            throw std::runtime_error(
                "Each selection requires source_path, capability_id and tolerance");
        selections.push_back({item.at("source_path").get<std::string>(),
                              item.at("capability_id").get<std::string>(),
                              item.at("tolerance").get<double>()});
    }
    return selections;
}

NativeTimbrePlan
part_timbre_plan(const McpSession& session, const DesiredPart& owner, const json& arguments) {
    const auto selections = timbre_selections(arguments);
    const auto& project = session.project->projects.at(owner.score_id);
    for (const auto id : project.profile_ids) {
        const auto* profile = session.timbre->find(id);
        if (profile->part_id.value != owner.part_id) continue;
        auto plan = plan_native_timbre(*profile, selections);
        if (!plan) throw std::runtime_error(plan.error().diagnostic);
        return std::move(*plan);
    }
    throw std::runtime_error("This Part has no owning Timbre profile");
}

std::vector<ManagedDevicePhysicalIntent> device_intents(const NativeTimbrePlan& plan) {
    std::vector<ManagedDevicePhysicalIntent> intents;
    for (const auto& intent : plan.intents)
        intents.push_back(
            {intent.selection.capability_id, intent.target, intent.selection.tolerance});
    return intents;
}

std::string source_device_key(const NativeTimbrePlan& plan) {
    return "source_" + hexadecimal(plan.profile_id.value);
}

std::vector<ManagedDeviceAdoptionSelection>
source_adoption_selections(const NativeTimbrePlan& plan, const ManagedBindingReceipt& binding) {
    if (binding.observation.at("manifest").at("devices_empty") == true) return {};
    return {{source_device_key(plan), 0, ManagedNativeDevice::Drift, device_intents(plan)}};
}

NativeEffectPlan
part_effect_plan(const McpSession& session, const DesiredPart& owner, const json& arguments) {
    std::vector<NativeEffectSelection> selections;
    const auto& declared = arguments.at("effect_selections");
    if (!declared.is_array() || declared.empty() || declared.size() >= SUNNY_MANAGED_MAX_DEVICES)
        throw std::runtime_error("Select 1..15 whole authored effect stages");
    for (const auto& item : declared) {
        if (!item.is_object() || item.size() != 3 || !item.contains("kind") ||
            !item.contains("authored_id") || !item.contains("tolerance"))
            throw std::runtime_error("Effect selection requires kind, authored_id and tolerance");
        const auto kind = item.at("kind").get<std::string>();
        NativeEffectSelectionKind typed;
        if (kind == "timbre_effect")
            typed = NativeEffectSelectionKind::TimbreEffect;
        else if (kind == "mix_input_trim")
            typed = NativeEffectSelectionKind::MixInputTrim;
        else if (kind == "mix_effect")
            typed = NativeEffectSelectionKind::MixEffect;
        else
            throw std::runtime_error("Unsupported authored effect selection kind");
        const auto& tolerance = item.at("tolerance");
        if (!tolerance.is_object() || tolerance.size() != 4)
            throw std::runtime_error("Declare effect tolerances in dB, Hz, Q and percent");
        selections.push_back(
            {typed,
             detail::checked_integer<std::uint64_t>(item.at("authored_id"), "authored_id"),
             {tolerance.at("decibels").get<double>(),
              tolerance.at("hertz").get<double>(),
              tolerance.at("quality_factor").get<double>(),
              tolerance.at("percent").get<double>()}});
    }
    const auto& project = session.project->projects.at(owner.score_id);
    const auto* mix = session.mix->find(project.mix_graph_id);
    const auto channel =
        std::find_if(mix->channels.begin(), mix->channels.end(), [&](const auto& strip) {
            return strip.part_id.value == owner.part_id;
        });
    if (channel == mix->channels.end())
        throw std::runtime_error("Owning Mix channel is unavailable");
    for (const auto id : project.profile_ids) {
        const auto* profile = session.timbre->find(id);
        if (profile->part_id.value != owner.part_id) continue;
        auto plan = plan_native_effects(*profile, mix, channel->id, selections);
        if (!plan) throw std::runtime_error(plan.error().diagnostic);
        return std::move(*plan);
    }
    throw std::runtime_error("This Part has no owning Timbre profile");
}

json effect_selection_schema() {
    return {{"type", "array"},
            {"minItems", 1},
            {"maxItems", 15},
            {"items",
             {{"type", "object"},
              {"additionalProperties", false},
              {"properties",
               {{"kind", {{"enum", {"timbre_effect", "mix_input_trim", "mix_effect"}}}},
                {"authored_id", {{"type", "integer"}, {"minimum", 1}}},
                {"tolerance",
                 {{"type", "object"},
                  {"additionalProperties", false},
                  {"properties",
                   {{"decibels", {{"type", "number"}, {"minimum", 0}}},
                    {"hertz", {{"type", "number"}, {"minimum", 0}}},
                    {"quality_factor", {{"type", "number"}, {"minimum", 0}}},
                    {"percent", {{"type", "number"}, {"minimum", 0}}}}},
                  {"required", {"decibels", "hertz", "quality_factor", "percent"}}}}}},
              {"required", {"kind", "authored_id", "tolerance"}}}}};
}

std::vector<ManagedDeviceAdoptionSelection>
chain_adoption_selections(const McpSession& session,
                          const DesiredPart& owner,
                          const json& arguments,
                          const NativeTimbrePlan& source,
                          const ManagedBindingReceipt& binding) {
    auto selections = source_adoption_selections(source, binding);
    if (!arguments.contains("effect_selections")) return selections;
    const auto effects = part_effect_plan(session, owner, arguments);
    if (selections.empty()) return selections;
    for (const auto& entry : effects.entries) {
        ManagedDeviceAdoptionSelection selected{
            entry.device_key, entry.desired_chain_index, entry.device, {}};
        selected.authored_bypass = !entry.final_modes.empty();
        if (selected.authored_bypass)
            selected.enum_intents = entry.final_modes;
        else {
            selected.enum_intents = entry.enable_modes;
            selected.enum_intents.insert(
                selected.enum_intents.end(), entry.setup_modes.begin(), entry.setup_modes.end());
            selected.property_intents = entry.setup_properties;
            selected.physical_intents = entry.setup_physical_intents;
            for (const auto& intent : entry.physical_intents) {
                const auto existing = std::find_if(
                    selected.physical_intents.begin(),
                    selected.physical_intents.end(),
                    [&](const auto& item) { return item.capability_id == intent.capability_id; });
                if (existing != selected.physical_intents.end())
                    *existing = intent;
                else
                    selected.physical_intents.push_back(intent);
            }
        }
        selections.push_back(std::move(selected));
    }
    return selections;
}

struct SelectedDeviceAdoption {
    std::vector<ManagedDeviceAdoptionSelection> selections;
    std::optional<ManagedDeviceRecoveryPlan> historical;
};

void current_device_prefix(const McpSession& session,
                           const DesiredPart& owner,
                           const NativeTimbrePlan& source,
                           std::span<const ManagedDeviceAdoptionSelection> saved) {
    const auto* profile = session.timbre->find(source.profile_id.value);
    const auto& project = session.project->projects.at(owner.score_id);
    const auto* graph = session.mix->find(project.mix_graph_id);
    const auto channel = std::ranges::find_if(
        graph->channels, [&](const auto& c) { return c.part_id.value == owner.part_id; });
    if (!profile || channel == graph->channels.end())
        throw std::runtime_error("Current owning Device chain references are unavailable");
    using Expected = std::pair<std::string, std::optional<ManagedNativeDevice>>;
    std::vector<Expected> current{{source_device_key(source), ManagedNativeDevice::Drift}};
    for (const auto& effect : profile->insert_chain.effects)
        current.emplace_back("timbre_" + hexadecimal(profile->id.value) + "_effect_" +
                                 hexadecimal(effect.id.value),
                             std::holds_alternative<EQEffect>(effect.parameters)
                                 ? std::optional{ManagedNativeDevice::EqEight}
                                 : std::nullopt);
    const auto trim = "mix_" + hexadecimal(graph->id.value) + "_channel_" +
                      hexadecimal(channel->id.value) + "_trim";
    if (channel->input_trim != 0.0f ||
        std::ranges::any_of(saved, [&](const auto& s) { return s.device_key == trim; }))
        current.emplace_back(trim, ManagedNativeDevice::Utility);
    for (const auto& effect : channel->insert_chain.effects) {
        std::optional<ManagedNativeDevice> device;
        if (std::holds_alternative<MixEQ>(effect.parameters)) device = ManagedNativeDevice::EqEight;
        if (std::holds_alternative<MixStereoProcessor>(effect.parameters))
            device = ManagedNativeDevice::Utility;
        current.emplace_back("mix_" + hexadecimal(graph->id.value) + "_effect_" +
                                 hexadecimal(effect.id.value),
                             device);
    }
    if (saved.size() > current.size())
        throw std::runtime_error("Saved native Device stages were removed from the current owning "
                                 "project; append-only recovery cannot delete them");
    for (std::size_t i = 0; i < saved.size(); ++i)
        if (saved[i].chain_index != i || saved[i].device_key != current[i].first ||
            !current[i].second || saved[i].device != *current[i].second)
            throw std::runtime_error(
                "Saved Device identities/classes/order conflict with the current owning project; "
                "recovery cannot remap or reorder them");
}

SelectedDeviceAdoption selected_device_adoption(const McpSession& session,
                                                const DesiredPart& owner,
                                                const json& arguments,
                                                const NativeTimbrePlan& source,
                                                const ManagedBindingReceipt& binding,
                                                const RealizationStore& store,
                                                const RealizationStoredAttempt& musical_source,
                                                bool adoption) {
    const auto projection_source =
        arguments.value("device_projection_source", std::string{"current_owning_timbre"});
    if (projection_source != "current_owning_timbre" &&
        projection_source != "retained_verified_realization")
        throw std::runtime_error("Unknown physical Device projection source");
    if (projection_source == "current_owning_timbre") {
        if (arguments.contains("device_history_attempt"))
            throw std::runtime_error(
                "Select retained Device history before supplying its snapshot identity");
        return {chain_adoption_selections(session, owner, arguments, source, binding),
                std::nullopt};
    }
    if (adoption && (!arguments.contains("device_history_attempt") ||
                     !arguments.at("device_history_attempt").is_string()))
        throw std::runtime_error(
            "Approve the exact Device history snapshot returned by the current preview");
    if (arguments.contains("effect_selections"))
        static_cast<void>(part_effect_plan(session, owner, arguments));
    ManagedDeviceRecoveryScope scope{session.realization->metadata.workspace_namespace,
                                     ScoreId{owner.score_id},
                                     PartId{owner.part_id},
                                     musical_source.intent.attempt_id,
                                     std::nullopt};
    if (arguments.contains("device_history_attempt"))
        scope.expected_device_history_attempt =
            arguments.at("device_history_attempt").get<std::string>();
    auto folded = fold_managed_device_recovery(store.attempts(), scope);
    if (!folded) throw std::runtime_error(folded.error().reason + ": " + folded.error().diagnostic);
    current_device_prefix(session, owner, source, folded->selections);
    auto selections = folded->selections;
    return {std::move(selections), std::move(*folded)};
}

void device_adoption_provenance(json& output, const SelectedDeviceAdoption& selected) {
    output["device_projection_source"] =
        selected.historical ? "retained_verified_realization" : "current_owning_timbre";
    output["current_owning_physical_intent_verified"] = !selected.historical.has_value();
    if (!selected.historical) return;
    const auto& historical = *selected.historical;
    output["device_history_attempt"] = historical.device_history_attempt;
    output["contributing_device_attempt_ids"] = historical.contributing_attempt_ids;
    output["saved_device_identity_fingerprint"] = historical.device_identity_fingerprint;
    output["saved_device_context"] = {{"bridge_instance", historical.context.bridge_instance},
                                      {"document_token", historical.context.document_token}};
    json retained = json::array();
    for (const auto& member : historical.selections) {
        json physical = json::array(), enums = json::array(), properties = json::array();
        for (const auto& intent : member.physical_intents)
            physical.push_back({{"capability_id", intent.capability_id},
                                {"target", intent.target},
                                {"tolerance", intent.tolerance}});
        for (const auto& intent : member.enum_intents)
            enums.push_back({{"capability_id", intent.capability_id}, {"label", intent.label}});
        for (const auto& intent : member.property_intents)
            properties.push_back({{"property", intent.property}, {"label", intent.label}});
        retained.push_back({{"device_key", member.device_key},
                            {"chain_index", member.chain_index},
                            {"physical_intents", physical},
                            {"enum_intents", enums},
                            {"property_intents", properties},
                            {"authored_bypass", member.authored_bypass}});
    }
    output["retained_device_intents"] = std::move(retained);
    json residuals = json::array();
    for (const auto& residual : historical.residuals) {
        json bypass = json::array(), guards = json::array();
        for (const auto& intent : residual.bypassed_physical_intents)
            bypass.push_back({{"capability_id", intent.capability_id},
                              {"target", intent.target},
                              {"tolerance", intent.tolerance}});
        for (const auto& intent : residual.observed_mode_guards)
            guards.push_back({{"capability_id", intent.capability_id}, {"label", intent.label}});
        residuals.push_back(
            {{"device_key", residual.device_key},
             {"bypassed_physical_intents", bypass},
             {"bypassed_physical_intents_unknown", residual.bypassed_physical_intents_unknown},
             {"observed_mode_guards", guards},
             {"saved_observed_modes", residual.observed_modes},
             {"opaque_state_observed", false}});
    }
    output["historical_device_residuals"] = std::move(residuals);
    output["scope"] = "saved_finite_physical_baseline_on_current_device_objects";
    output["current_authored_values_require_separate_writes"] = true;
}

std::vector<ManagedDeviceAdoptionSelection>
observed_device_selections(const ManagedBindingReceipt& binding) {
    std::vector<ManagedDeviceAdoptionSelection> selections;
    if (!binding.observation.contains("device_identity")) return selections;
    for (const auto& member : binding.observation.at("device_identity").at("cohort")) {
        ManagedNativeDevice device;
        const auto& name = member.at("class_name");
        if (name == "Drift")
            device = ManagedNativeDevice::Drift;
        else if (name == "StereoGain")
            device = ManagedNativeDevice::Utility;
        else if (name == "Eq8")
            device = ManagedNativeDevice::EqEight;
        else
            throw std::runtime_error("Current finite Device class is unsupported");
        ManagedDeviceAdoptionSelection selection{member.at("device_key").get<std::string>(),
                                                 static_cast<std::uint32_t>(selections.size()),
                                                 device,
                                                 {}};
        if (device != ManagedNativeDevice::Drift) {
            selection.authored_bypass = member.at("is_active") == false;
            selection.enum_intents = {
                {device == ManagedNativeDevice::Utility ? "utility.enabled" : "eq8.enabled",
                 selection.authored_bypass ? "Off" : "On"}};
        }
        selections.push_back(std::move(selection));
    }
    return selections;
}

ManagedDeviceAdoptionSelection effect_inspection_selection(const NativeEffectPlanEntry& entry) {
    ManagedDeviceAdoptionSelection selected{
        entry.device_key, entry.desired_chain_index, entry.device, {}};
    selected.authored_bypass = !entry.final_modes.empty();
    if (selected.authored_bypass)
        selected.enum_intents = entry.final_modes;
    else {
        selected.enum_intents = entry.enable_modes;
        selected.enum_intents.insert(
            selected.enum_intents.end(), entry.setup_modes.begin(), entry.setup_modes.end());
        selected.property_intents = entry.setup_properties;
        selected.physical_intents = entry.setup_physical_intents;
        for (const auto& intent : entry.physical_intents) {
            const auto existing = std::ranges::find(selected.physical_intents,
                                                    intent.capability_id,
                                                    &ManagedDevicePhysicalIntent::capability_id);
            if (existing == selected.physical_intents.end())
                selected.physical_intents.push_back(intent);
            else
                *existing = intent;
        }
    }
    return selected;
}

std::optional<json>
inspect_current_device_intent(const ManagedBridgeContext& context,
                              const ManagedBindingReceipt& binding,
                              std::span<const ManagedDeviceAdoptionSelection> selections,
                              LomTransport& transport) {
    const auto request = make_managed_device_inspection_request(context, binding, selections);
    if (!request) return std::nullopt;
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        return std::nullopt;
    const auto& raw = std::get<json>(*response.value);
    const auto parsed = parse_managed_device_inspection(*request, context, raw);
    if (!parsed) return std::nullopt;
    const auto& observed = raw.at("inspection").at("binding_observation");
    for (const auto* field : {"manifest",
                              "note_identity",
                              "note_identity_fingerprint",
                              "content_fingerprint",
                              "device_identity",
                              "device_identity_fingerprint",
                              "group_authority",
                              "group_authority_fingerprint"})
        if (observed.value(field, json(nullptr)) != binding.observation.value(field, json(nullptr)))
            return std::nullopt;
    return raw;
}

json unchanged_device_result(const DesiredPart& desired,
                             const json& plan,
                             const json& observed,
                             const std::string& scope) {
    return {{"success", true},
            {"state", "selected_device_intent_already_observed"},
            {"score_id", desired.score_id},
            {"part_id", desired.part_id},
            {"project_revision", desired.revision},
            {"plan", plan},
            {"native_inspection", observed},
            {"mutation_dispatched", false},
            {"authority_granted", false},
            {"scope", scope},
            {"native_knob_only", true},
            {"dsp_equivalence_qualified", false},
            {"host_qualified", false},
            {"complete_project_realization", false}};
}

json authored_step_pan_lane(const McpSession& session,
                            const DesiredPart& desired,
                            const json& arguments) {
    const auto* score = session.score->find(desired.score_id);
    const auto& owner = session.project->projects.at(desired.score_id);
    const auto* mix = session.mix->find(owner.mix_graph_id);
    const auto lane_index =
        detail::checked_integer<std::uint64_t>(arguments.at("lane_index"), "lane_index");
    if (lane_index >= mix->automation.size()) throw std::runtime_error("Mix lane does not exist");
    const auto& authored = mix->automation.at(static_cast<std::size_t>(lane_index));
    if (authored.target != "channels[" + std::to_string(desired.part_id) + "].spatial.pan" ||
        authored.interpolation != InterpolationMode::Step)
        throw std::runtime_error("This operation supports the selected Part's Step panning lane");
    json points = json::array();
    double previous = -1.0;
    for (const auto& point : authored.breakpoints) {
        auto absolute = score_time_to_absolute_beat(point.time, score->time_map);
        if (!absolute) throw std::runtime_error("Mix breakpoint has invalid Score time");
        const double time = absolute->to_float() * 4.0;
        const double value = static_cast<double>(point.value);
        if (!std::isfinite(time) || time <= previous || time >= desired.projection.clip_end ||
            !std::isfinite(value) || value < -1.0 || value > 1.0)
            throw std::runtime_error("Mix lane times or values exceed the native panning interval");
        if (points.empty() && time != 0.0)
            throw std::runtime_error("The finite Step lane must start at Score origin");
        points.push_back({{"time", time}, {"value", value}});
        previous = time;
    }
    if (points.empty()) throw std::runtime_error("Mix lane has no breakpoints");
    const json selector = {{"kind", "panning"}};
    const json lane = {{"parameter", selector},
                       {"interpolation", "step"},
                       {"clip_end", desired.projection.clip_end},
                       {"points", points}};
    return lane;
}

json owning_step_pan_lane(const McpSession& session,
                          const DesiredPart& desired,
                          const ManagedBindingReceipt& binding,
                          const json& arguments) {
    const auto& mixer = binding.observation.at("manifest").at("mixer");
    const auto& parameter = mixer.at("panning");
    if (mixer.at("panning_mode") != 0 || parameter.at("min") != -1.0 || parameter.at("max") != 1.0)
        throw std::runtime_error("Native stereo panning requires the observed [-1,+1] domain");
    return authored_step_pan_lane(session, desired, arguments);
}

struct OwningSongSettings {
    DesiredPart desired;
    std::shared_ptr<RealizationStore> store;
    const RealizationStoredAttempt* prior;
    ManagedSongSettings settings;
};

NativeMixStaticPlan
part_static_mix_plan(const McpSession& session, const DesiredPart& owner, const json& arguments) {
    const auto& encoded = arguments.at("domains");
    if (!encoded.is_array() || encoded.empty() || encoded.size() > 4)
        throw std::runtime_error("Select 1..4 static Mixer domains");
    NativeMixStaticSelection selection{false, false, false, false, 0.0};
    std::set<std::string> selected;
    for (const auto& domain : encoded) {
        if (!domain.is_string() || !selected.insert(domain.get<std::string>()).second)
            throw std::runtime_error("Mixer domains must be unique strings");
        const auto name = domain.get<std::string>();
        if (name == "volume")
            selection.volume = true;
        else if (name == "pan")
            selection.pan = true;
        else if (name == "mute")
            selection.mute = true;
        else if (name == "solo")
            selection.solo = true;
        else
            throw std::runtime_error("Unsupported static Mixer domain: " + name);
    }
    if (selection.volume) {
        if (!arguments.contains("volume_tolerance_db") ||
            !arguments.at("volume_tolerance_db").is_number())
            throw std::runtime_error("Volume needs an explicit finite dB tolerance");
        selection.tolerance_db = arguments.at("volume_tolerance_db").get<double>();
    } else if (arguments.contains("volume_tolerance_db"))
        throw std::runtime_error("Select volume before supplying its dB tolerance");
    const auto& project = session.project->projects.at(owner.score_id);
    const auto* graph = session.mix->find(project.mix_graph_id);
    const ChannelStrip* channel = nullptr;
    for (const auto& candidate : graph->channels) {
        if (candidate.part_id.value != owner.part_id) continue;
        if (channel) throw std::runtime_error("Part has ambiguous owning Mix Channels");
        channel = &candidate;
    }
    if (!channel) throw std::runtime_error("Part has no owning Mix Channel");
    auto plan = plan_native_mix_static(*graph, channel->id, PartId{owner.part_id}, selection);
    if (!plan) throw std::runtime_error(plan.error().diagnostic);
    return std::move(*plan);
}

struct OwningStaticMixer {
    DesiredPart desired;
    json plan;
    ManagedStaticMixerDesired mixer_desired;
    std::shared_ptr<RealizationStore> store;
    ManagedBindingReceipt binding;
};

OwningStaticMixer
owning_static_mixer(const McpSession& session, const json& arguments, LomTransport* transport) {
    auto owner = owning_project_revision(session, arguments);
    owner.part_id = detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id");
    const bool retired = !active_part(session, owner);
    json plan;
    ManagedStaticMixerDesired mixer_desired;
    if (retired) {
        if (arguments.at("domains") != json::array({"mute"}) ||
            arguments.contains("volume_tolerance_db"))
            throw std::runtime_error(
                "Removed Part supports only explicit mute retirement authority");
        mixer_desired.mute = true;
        plan = {{"desired", {{"mute", true}}},
                {"projection_source", "retained_verified_removed_part"},
                {"retirement_policy", "mute"},
                {"current_mix_channel", nullptr}};
    } else {
        auto current = part_static_mix_plan(session, owning_part(session, arguments), arguments);
        mixer_desired = current.desired;
        plan = native_mix_static_plan_to_json(current);
    }
    auto store = history(session, false);
    const auto retained = binding_history(*store, owner.score_id, owner.part_id);
    if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
    if (!retained.acknowledged)
        throw std::runtime_error("Create or explicitly adopt the current native Part first");
    if (!transport || transport->records_without_execution())
        throw std::runtime_error("An actual bridge connection is required");
    auto desired = retired ? recovery_part(session, arguments, *transport)
                           : desired_part(session, arguments, std::move(owner), *transport);
    if (retired) plan["historical_projection_attempt"] = *desired.historical_projection_attempt;
    auto binding = current_binding(*store, *retained.acknowledged);
    if (!projection_matches(desired.encoded, binding.observation))
        throw std::runtime_error(
            "Revise native Score notes and geometry before static Mixer values");
    static_cast<void>(native_note_associations(desired.keys, desired.encoded, binding.observation));
    return {std::move(desired),
            std::move(plan),
            std::move(mixer_desired),
            std::move(store),
            std::move(binding)};
}

json static_mixer_schema() {
    auto schema = revision_schema();
    schema["additionalProperties"] = false;
    schema["properties"]["domains"] = {
        {"type", "array"},
        {"minItems", 1},
        {"maxItems", 4},
        {"uniqueItems", true},
        {"items", {{"type", "string"}, {"enum", {"volume", "pan", "mute", "solo"}}}}};
    schema["properties"]["volume_tolerance_db"] = {{"type", "number"}, {"minimum", 0}};
    schema["required"].push_back("domains");
    return schema;
}

json static_mixer_result(json output, const json& plan, bool adoption) {
    output["scope"] =
        adoption ? "selected_current_static_mixer_authority" : "selected_part_static_mixer_knobs";
    output["plan"] = plan;
    output["native_knob_only"] = true;
    output["dsp_equivalence_qualified"] = false;
    output["host_qualified"] = false;
    output["complete_project_realization"] = false;
    return output;
}

struct OwningLane {
    DesiredPart desired;
    std::shared_ptr<RealizationStore> store;
    ManagedBindingReceipt binding;
    json lane;
};

OwningLane owning_lane(const McpSession& session, const json& arguments, LomTransport* transport) {
    auto owner = owning_part(session, arguments);
    auto store = history(session, false);
    const auto retained = binding_history(*store, owner.score_id, owner.part_id);
    if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
    if (!retained.acknowledged)
        throw std::runtime_error("Create or explicitly adopt the current native Part first");
    if (!transport || transport->records_without_execution())
        throw std::runtime_error("An actual bridge connection is required");
    auto desired = desired_part(session, arguments, std::move(owner), *transport);
    auto binding = current_binding(*store, *retained.acknowledged);
    if (!projection_matches(desired.encoded, binding.observation))
        throw std::runtime_error(
            "Revise native Score notes and geometry before Mix lane replacement");
    static_cast<void>(native_note_associations(desired.keys, desired.encoded, binding.observation));
    auto lane = owning_step_pan_lane(session, desired, binding, arguments);
    if (lane.at("points").size() > SUNNY_MANAGED_ENVELOPE_REPLACEMENT_MAX_STEPS)
        throw std::runtime_error("EnvelopeReplacementUnavailable: selected lane exceeds the "
                                 "64 native Step-call budget; no dispatch fence was created");
    return {std::move(desired), std::move(store), std::move(binding), std::move(lane)};
}

OwningSongSettings
owning_song_settings(const McpSession& session, const json& arguments, LomTransport* transport) {
    auto owner = owning_part(session, arguments);
    auto settings = plan_managed_song_settings(*session.score->find(owner.score_id));
    if (!settings) throw std::runtime_error(settings.error());
    auto store = history(session, false);
    const auto retained = binding_history(*store, owner.score_id, owner.part_id);
    if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
    if (!retained.acknowledged)
        throw std::runtime_error("Create or explicitly adopt the current native Part first");
    if (!transport || transport->records_without_execution())
        throw std::runtime_error("An actual bridge connection is required");
    auto desired = desired_part(session, arguments, std::move(owner), *transport);
    const auto& binding = current_binding(*store, *retained.acknowledged);
    if (!note_changes(*retained.acknowledged, desired, binding).empty())
        throw std::runtime_error("Revise native Score notes before Set settings");
    static_cast<void>(native_note_associations(desired.keys, desired.encoded, binding.observation));
    return {std::move(desired), std::move(store), retained.acknowledged, *settings};
}

struct OwningRoutingPlan {
    DesiredPart owner;
    json intent;
    json plan;
};

OwningRoutingPlan owning_routing_plan(const McpSession& session, const json& arguments) {
    auto owner = owning_part(session, arguments);
    const auto& project = session.project->projects.at(owner.score_id);
    const auto* graph = session.mix->find(project.mix_graph_id);
    // One Return tap policy governs all senders. No native fence may precede
    // this owning whole-graph consistency check, even for a selected Part.
    std::map<std::uint64_t, bool> taps;
    const auto check_sends = [&taps](const auto& sends) {
        for (const auto& send : sends) {
            if (!send.enabled) continue;
            const auto [found, inserted] = taps.emplace(send.aux_bus_id.value, send.pre_fader);
            if (!inserted && found->second != send.pre_fader)
                throw std::runtime_error("Conflicting enabled per-Aux Pre/Post requests: one "
                                         "native Return policy applies to all senders");
        }
    };
    for (const auto& channel : graph->channels)
        check_sends(channel.sends);
    for (const auto& group : graph->group_buses)
        check_sends(group.sends);
    const auto channel = std::ranges::find_if(
        graph->channels, [&](const auto& value) { return value.part_id.value == owner.part_id; });
    if (channel == graph->channels.end())
        throw std::runtime_error("Part has no owning Mix Channel");
    const auto kind = arguments.at("kind").get<std::string>();
    std::set<std::string> fields{"score_id", "part_id", "expected_project_revision", "ppq", "kind"};
    json intent{{"kind", kind}};
    if (kind == "create_return" || kind == "adopt_return" || kind == "send_level") {
        fields.insert("aux_id");
        const auto aux_id =
            detail::checked_integer<std::uint64_t>(arguments.at("aux_id"), "aux_id");
        const auto aux = std::ranges::find_if(
            graph->aux_buses, [&](const auto& value) { return value.id.value == aux_id; });
        if (aux == graph->aux_buses.end())
            throw std::runtime_error("Aux does not belong to this Mix");
        intent["aux_key"] = "aux_" + hexadecimal(aux_id);
        if (kind == "adopt_return") {
            fields.insert("return_index");
            intent["return_index"] =
                detail::checked_integer<int>(arguments.at("return_index"), "return_index");
        } else if (kind == "send_level") {
            fields.insert("send_tolerance_db");
            const auto send = std::ranges::find_if(channel->sends, [&](const auto& value) {
                return value.aux_bus_id.value == aux_id;
            });
            if (send == channel->sends.end() || !send->enabled)
                throw std::runtime_error("Select an enabled owning Channel Aux send");
            if (!arguments.at("send_tolerance_db").is_number())
                throw std::runtime_error("Send needs an explicit finite dB tolerance");
            const auto tolerance = arguments.at("send_tolerance_db").get<double>();
            if (!std::isfinite(tolerance) || tolerance < 0.0)
                throw std::runtime_error("Send tolerance must be finite and nonnegative");
            intent["level_db"] = static_cast<double>(send->level_db);
            intent["tolerance_db"] = tolerance;
            intent["requested_pre_fader"] = send->pre_fader;
        }
    } else if (kind == "adopt_group") {
        fields.insert("group_id");
        fields.insert("group_track_index");
        fields.insert("selector");
        fields.insert("projection_source");
        fields.insert("historical_projection_attempt");
        const auto projection_source =
            arguments.value("projection_source", std::string{"current_owning_score"});
        if (projection_source != "current_owning_score" &&
            projection_source != "retained_verified_realization")
            throw std::runtime_error("Unknown Group musical projection source");
        if (arguments.contains("historical_projection_attempt") &&
            projection_source != "retained_verified_realization")
            throw std::runtime_error(
                "Select retained music before supplying its historical Group source");
        const auto group_id =
            detail::checked_integer<std::uint64_t>(arguments.at("group_id"), "group_id");
        const auto group = std::ranges::find_if(
            graph->group_buses, [&](const auto& value) { return value.id.value == group_id; });
        if (group == graph->group_buses.end() || !group->member_groups.empty() ||
            group->output.type != GroupOutputType::Master ||
            std::ranges::any_of(graph->group_buses, [&](const auto& value) {
                return std::ranges::find(value.member_groups, GroupBusId{group_id}) !=
                       value.member_groups.end();
            }))
            throw std::runtime_error(
                "Select a flat owning Group with direct Channels and Main output");
        json keys = json::array();
        for (const auto id : group->member_channels) {
            const auto member = std::ranges::find(graph->channels, id, &ChannelStrip::id);
            if (member == graph->channels.end() || member->group_assignment != group->id)
                throw std::runtime_error("Owning Group member relationships are inconsistent");
            keys.push_back(binding_key(member->part_id.value));
        }
        if (std::ranges::find(keys, json(binding_key(owner.part_id))) == keys.end())
            throw std::runtime_error("Anchor Part is not a direct member of this owning Group");
        intent["group_key"] = "group_" + hexadecimal(group_id);
        intent["track_index"] =
            detail::checked_integer<int>(arguments.at("group_track_index"), "group_track_index");
        intent["member_binding_keys"] = std::move(keys);
        const auto& selector = arguments.at("selector");
        if (!selector.is_object() || selector.size() != 2 || !selector.contains("track_index") ||
            !selector.contains("slot_index"))
            throw std::runtime_error("Group bootstrap needs exact current Track/Slot selection");
        for (const auto* name : {"track_index", "slot_index"})
            if (detail::checked_integer<int>(selector.at(name), name) < 0)
                throw std::runtime_error("Group selector indices must be nonnegative");
    } else if (kind == "output_type" || kind == "output_channel") {
        fields.insert("route_identifier");
        intent["route_identifier"] = arguments.at("route_identifier");
        intent["destination"] = channel->group_assignment ? "group" : "main";
        intent["group_key"] = channel->group_assignment
                                  ? json("group_" + hexadecimal(channel->group_assignment->value))
                                  : json(nullptr);
    } else {
        throw std::runtime_error("Unknown finite native routing kind");
    }
    for (const auto* name : {"preview", "explicit_current_routing_approval"})
        fields.insert(name);
    for (const auto& [name, value] : arguments.items()) {
        static_cast<void>(value);
        if (!fields.contains(name)) throw std::runtime_error("Unused routing argument: " + name);
    }
    if (!managed_routing_detail::intent_valid(intent))
        throw std::runtime_error("Owning routing intent exceeds the finite native contract");
    json plan{{"intent", intent},
              {"score_id", owner.score_id},
              {"part_id", owner.part_id},
              {"project_revision", owner.revision},
              {"tap_policy_observed", false},
              {"logical_send_complete", false},
              {"unapplied_domains",
               {"Return Pre/Post policy",
                "Return mixer/effects/output",
                "Group mixer/effects and creation"}},
              {"host_qualified", false}};
    return {std::move(owner), std::move(intent), std::move(plan)};
}

json routing_schema() {
    auto schema = revision_schema();
    schema["additionalProperties"] = false;
    schema["properties"]["kind"] = {{"type", "string"},
                                    {"enum",
                                     {"create_return",
                                      "adopt_return",
                                      "send_level",
                                      "output_type",
                                      "output_channel",
                                      "adopt_group"}}};
    schema["required"].push_back("kind");
    schema["properties"]["projection_source"] = {
        {"type", "string"}, {"enum", {"current_owning_score", "retained_verified_realization"}}};
    schema["properties"]["historical_projection_attempt"] = {{"type", "string"}};
    for (const auto* name : {"aux_id", "group_id"})
        schema["properties"][name] = {{"type", "integer"}, {"minimum", 1}};
    for (const auto* name : {"return_index", "group_track_index"})
        schema["properties"][name] = {{"type", "integer"}, {"minimum", 0}};
    schema["properties"]["send_tolerance_db"] = {{"type", "number"}, {"minimum", 0}};
    schema["properties"]["route_identifier"] = {{"type", "string"},
                                                {"pattern", "^(0|-?[1-9][0-9]{0,18})$"}};
    schema["properties"]["selector"] = {{"type", "object"},
                                        {"additionalProperties", false},
                                        {"properties",
                                         {{"track_index", {{"type", "integer"}, {"minimum", 0}}},
                                          {"slot_index", {{"type", "integer"}, {"minimum", 0}}}}},
                                        {"required", {"track_index", "slot_index"}}};
    return schema;
}

struct OwningRouting {
    DesiredPart desired;
    OwningRoutingPlan owning;
    LomRequest preview_request;
};

OwningRouting
owning_routing(const McpSession& session, const json& arguments, LomTransport& transport) {
    auto plan = owning_routing_plan(session, arguments);
    auto desired = plan.intent.at("kind") == "adopt_group"
                       ? recovery_part(session, arguments, transport)
                       : desired_part(session, arguments, plan.owner, transport);
    const auto context = managed_bridge_context(transport);
    if (!context) throw std::runtime_error("Current native context is unavailable");
    Result<LomRequest> request = std::unexpected(ErrorCode::ProtocolError);
    if (plan.intent.at("kind") == "adopt_group") {
        request = make_managed_group_preview_request(*context,
                                                     project_key(session, desired.score_id),
                                                     binding_key(desired.part_id),
                                                     arguments.at("selector"),
                                                     plan.intent);
    } else {
        const auto store = history(session, false);
        const auto retained = binding_history(*store, desired.score_id, desired.part_id);
        if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
        if (!retained.acknowledged)
            throw std::runtime_error("Create or explicitly adopt this native Part first");
        const auto binding = current_binding(*store, *retained.acknowledged);
        if (!projection_matches(desired.encoded, binding.observation))
            throw std::runtime_error("Revise native Score notes and geometry before routing");
        static_cast<void>(
            native_note_associations(desired.keys, desired.encoded, binding.observation));
        request = make_managed_routing_preview_request(*context, binding, plan.intent);
    }
    if (!request) throw std::runtime_error("Current finite routing preview is unavailable");
    return {std::move(desired), std::move(plan), std::move(*request)};
}

json selected_part_arguments(const json& selection, const json& id) {
    json arguments{{"score_id", selection.at("score_id")},
                   {"part_id", id},
                   {"expected_project_revision", selection.at("expected_project_revision")}};
    if (selection.contains("ppq")) arguments["ppq"] = selection.at("ppq");
    return arguments;
}

json native_observation_fields(const json& observation) {
    json result = json::object();
    for (const auto* name : {"track_index",
                             "slot_index",
                             "manifest",
                             "content_fingerprint",
                             "track_tag",
                             "clip_tag",
                             "structural_boundary_complete",
                             "content_boundary_complete",
                             "unavailable_reasons",
                             "note_identity",
                             "note_identity_fingerprint",
                             "device_identity",
                             "device_identity_fingerprint",
                             "group_authority",
                             "group_authority_fingerprint"})
        if (observation.contains(name)) result[name] = observation.at(name);
    return result;
}

ManagedBindingReceipt current_native_baseline(const RealizationStore& store,
                                              const RealizationStoredAttempt& prior,
                                              const ManagedBridgeContext& context,
                                              LomTransport& transport) {
    const auto binding = current_binding(store, prior);
    if (binding.context.bridge_instance != context.bridge_instance ||
        binding.context.document_token != context.document_token)
        throw std::runtime_error(
            "Explicitly adopt current Clip and selected Device authority after reopening the Set");
    const auto observed =
        observe_managed_binding(context, binding.project_key, binding.binding_key, transport);
    const auto actual =
        observed ? managed_observed_binding(*observed)
                 : Result<ManagedBindingReceipt>{std::unexpected(ErrorCode::ProtocolError)};
    if (!actual || native_observation_fields(actual->observation) !=
                       native_observation_fields(binding.observation))
        throw std::runtime_error("Actual current native cohort differs from retained history; "
                                 "inspect and explicitly adopt it before planning");
    return binding;
}

json inspect_current_mixer(const ManagedBridgeContext& context,
                           const ManagedBindingReceipt& binding,
                           const ManagedStaticMixerDesired& desired,
                           LomTransport& transport,
                           bool require_targets) {
    const auto request = make_managed_static_mixer_inspection_request(context, binding, desired);
    if (!request) throw std::runtime_error("Selected current Mixer inspection is unavailable");
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        throw std::runtime_error("Selected actual Mixer controls cannot be inspected: " +
                                 response.error.value_or("Native readback unavailable"));
    const auto& raw = std::get<json>(*response.value);
    const auto parsed = parse_managed_static_mixer_inspection(*request, context, raw);
    if (!parsed) throw std::runtime_error("Selected actual Mixer inspection is malformed");
    const auto& body = raw.at("inspection");
    const auto& before = body.at("before");
    if (native_observation_fields(before.at("binding_observation")) !=
        native_observation_fields(binding.observation))
        throw std::runtime_error("Actual Mixer inspection differs from the current Part cohort");
    if (require_targets) {
        if (desired.volume) {
            const auto& current = body.at("candidates").at("volume").at("current_display");
            const auto reading = native_unit_detail::display_reading(
                current.at("display").get<std::string>(), LiveNativePhysicalUnit::Decibels, true);
            const auto compared =
                reading ? native_unit_detail::decimal_display_comparison(
                              *reading, desired.volume->target, desired.volume->tolerance, 1.0, 28)
                        : std::nullopt;
            if (!compared || !compared->within_tolerance)
                throw std::runtime_error(
                    "Final actual Mixer volume differs from the owning dB target");
        }
        const auto& parameters = before.at("mixer_capture").at("parameters");
        if ((desired.pan && parameters.at(1).at("descriptor").at("value") != *desired.pan) ||
            (desired.mute && before.at("track_context").at("mute") != *desired.mute) ||
            (desired.solo && before.at("track_context").at("solo") != *desired.solo))
            throw std::runtime_error(
                "Final actual selected Mixer control differs from owning intent");
    }
    return raw;
}

json inspect_current_send(const ManagedBridgeContext& context,
                          const ManagedBindingReceipt& binding,
                          const json& intent,
                          LomTransport& transport,
                          bool require_target) {
    const auto request = make_managed_send_inspection_request(context, binding, intent);
    if (!request) throw std::runtime_error("Selected current Send inspection is unavailable");
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        throw std::runtime_error("Selected actual Send cannot be inspected: " +
                                 response.error.value_or("Native readback unavailable"));
    const auto& raw = std::get<json>(*response.value);
    if (!parse_managed_send_inspection(*request, context, raw) ||
        native_observation_fields(raw.at("observation")) !=
            native_observation_fields(binding.observation))
        throw std::runtime_error("Actual Send inspection differs from the current Part cohort");
    if (require_target && raw.at("inspection").at("send_readback").at("matches_intent") != true)
        throw std::runtime_error("Final actual Send differs from the owning dB target");
    return raw;
}

json inspect_current_song_settings(const ManagedBridgeContext& context,
                                   const ManagedBindingReceipt& binding,
                                   const ManagedSongSettings& desired,
                                   LomTransport& transport,
                                   bool require_targets) {
    const auto request = make_managed_song_settings_inspection_request(context, binding, desired);
    if (!request) throw std::runtime_error("Selected current Song inspection is unavailable");
    const auto response = transport.send(*request);
    if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
        throw std::runtime_error("Actual Song settings cannot be inspected: " +
                                 response.error.value_or("Native readback unavailable"));
    const auto& raw = std::get<json>(*response.value);
    if (!parse_managed_song_settings_inspection(*request, context, raw) ||
        native_observation_fields(raw.at("observation")) !=
            native_observation_fields(binding.observation))
        throw std::runtime_error("Actual Song inspection differs from the current Part cohort");
    if (require_targets &&
        raw.at("inspection").at("before").at("settings") != managed_song_settings_to_json(desired))
        throw std::runtime_error(
            "Final actual Song settings differ from the owning tempo or meter");
    return raw;
}

json prepare_project_realization(const McpSession& session,
                                 const json& selection,
                                 LomTransport* transport) {
    const auto owner = owning_project_revision(session, selection);
    if (!transport || transport->records_without_execution())
        throw std::runtime_error(
            "An actual matching bridge connection is required for native project planning");
    static_cast<void>(reviewed_native_target(*transport));
    const auto context = managed_bridge_context(*transport);
    if (!context) throw std::runtime_error("Current native context is unavailable");
    auto store = history(session, true);
    if (const auto* blocked = global_settings_blocker(*store, *context))
        throw ReconciliationRequired(*blocked);

    json parts = json::array(), retired = json::array(), routes = json::array();
    std::map<std::uint64_t, DesiredPart> desired_parts;
    // All authored planners execute first. An unsupported later selection cannot
    // leave the earlier Parts created merely because they were individually valid.
    for (const auto& selected : selection.at("parts")) {
        const auto arguments = selected_part_arguments(selection, selected.at("part_id"));
        auto desired =
            desired_part(session, arguments, owning_part(session, arguments), *transport);
        json part{{"part_id", desired.part_id},
                  {"desired_projection", desired.encoded},
                  {"desired_note_keys", desired.keys},
                  {"desired_note_identity", desired.identity},
                  {"warnings", desired.warnings},
                  {"outside_clip_requests_applied", false},
                  {"outside_clip_request_count", desired.projection.outside_clip_requests.size()}};
        if (selected.contains("source_selections")) {
            auto input = arguments;
            input["selections"] = selected.at("source_selections");
            if (!input.at("selections").is_array() || input.at("selections").empty())
                throw std::runtime_error(
                    "Select 1..4 physical source controls when source authoring is requested");
            part["source_plan"] =
                native_timbre_plan_to_json(part_timbre_plan(session, desired, input));
        }
        if (selected.contains("effect_selections")) {
            auto input = arguments;
            input["effect_selections"] = selected.at("effect_selections");
            part["effect_plan"] =
                native_effect_plan_to_json(part_effect_plan(session, desired, input));
        }
        if (selected.contains("static_mixer")) {
            auto input = arguments;
            input.update(selected.at("static_mixer"));
            part["static_mixer_plan"] =
                native_mix_static_plan_to_json(part_static_mix_plan(session, desired, input));
        }
        if (selected.contains("pan_lane")) {
            auto input = arguments;
            input["lane_index"] = selected.at("pan_lane").at("lane_index");
            part["pan_lane"] = authored_step_pan_lane(session, desired, input);
            if (part.at("pan_lane").at("points").size() > SUNNY_MANAGED_ENVELOPE_MAX_STEPS)
                throw std::runtime_error("Selected pan lane exceeds the 64 native Step-call bound");
        }
        desired_parts.emplace(desired.part_id, std::move(desired));
        parts.push_back(std::move(part));
    }
    for (const auto& selected : selection.at("routing")) {
        auto input = selected_part_arguments(selection, selected.at("part_id"));
        input.update(selected);
        const auto candidate = owning_routing_plan(session, input);
        routes.push_back(
            {{"selection", selected}, {"plan", candidate.plan}, {"resolved_selection", selected}});
    }
    json song = nullptr;
    if (selection.contains("song_settings_part_id")) {
        const auto settings = plan_managed_song_settings(*session.score->find(owner.score_id));
        if (!settings)
            throw std::runtime_error("Owning Song settings exceed the constant quarter-tempo and "
                                     "flat origin-meter contract");
        song = managed_song_settings_to_json(*settings);
    }
    // Native reads close every selected baseline before the first possible fence.
    std::map<std::uint64_t, ManagedBindingReceipt> bindings;
    for (auto& part : parts) {
        const auto id = part.at("part_id").get<std::uint64_t>();
        const auto retained = binding_history(*store, owner.score_id, id);
        if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
        part["clip_action"] = retained.acknowledged ? "update" : "create";
        part["retained_attempt_id"] =
            retained.acknowledged ? json(retained.acknowledged->intent.attempt_id) : json(nullptr);
        part["native_baseline"] = nullptr;
        const auto& selected = *std::ranges::find_if(
            selection.at("parts"), [&](const auto& p) { return p.at("part_id") == id; });
        const auto& desired = desired_parts.at(id);
        if (!retained.acknowledged) {
            const bool audio_mixer =
                selected.contains("static_mixer") &&
                std::ranges::any_of(
                    selected.at("static_mixer").at("domains"),
                    [](const auto& domain) { return domain == "volume" || domain == "pan"; });
            const bool audio_routing =
                std::ranges::any_of(selection.at("routing"), [&](const auto& route) {
                    return route.at("part_id") == id &&
                           (route.at("kind") == "send_level" || route.at("kind") == "output_type" ||
                            route.at("kind") == "output_channel");
                });
            if ((audio_mixer || audio_routing || selected.contains("pan_lane")) &&
                !selected.contains("source_selections"))
                throw std::runtime_error("Select an owning instrument Source before audio Mixer, "
                                         "pan lane or routing on a new MIDI Part");
            const auto request = make_managed_clip_request(*context,
                                                           std::string(32, 'a'),
                                                           project_key(session, owner.score_id),
                                                           binding_key(id),
                                                           desired.projection);
            if (!request)
                throw std::runtime_error(
                    "ReplyCapacityUnavailable or unsupported selected native Clip creation");
            if (selected.contains("effect_selections") && !selected.contains("source_selections"))
                throw std::runtime_error(
                    "Select the owning source before effects on a new native Part");
            continue;
        }
        const auto& prior = *retained.acknowledged;
        const auto binding = current_native_baseline(*store, prior, *context, *transport);
        if (!projection_matches(prior.intent.desired_projection, binding.observation))
            throw std::runtime_error("Retained musical baseline is not verified");
        static_cast<void>(native_note_associations(
            prior.intent.desired_note_keys, prior.intent.desired_projection, binding.observation));
        const auto stages = plan_managed_clip_revision(retained_projection(prior),
                                                       prior.intent.desired_note_keys,
                                                       desired.projection,
                                                       desired.keys);
        if (!stages)
            throw std::runtime_error("Unsupported retained Event cardinality/probability revision "
                                     "or malformed Clip geometry");
        part["native_baseline"] = managed_binding_to_json(binding);
        part["clip_phase_count"] = stages->size();
        if (selected.contains("source_selections") || selected.contains("effect_selections")) {
            if (!binding.observation.contains("device_identity") &&
                binding.observation.at("manifest").at("devices_empty") != true)
                throw std::runtime_error("Explicitly adopt the whole current finite Device chain "
                                         "before selected physical revisions");
            if (binding.observation.contains("device_identity")) {
                const auto& cohort = binding.observation.at("device_identity").at("cohort");
                const auto source_profile = selected.contains("source_selections")
                                                ? part.at("source_plan").at("profile_id")
                                                : part.at("effect_plan").at("profile_id");
                const auto source_key =
                    "source_" + hexadecimal(source_profile.get<std::uint64_t>());
                if (!cohort.empty() &&
                    (cohort[0].at("device_key") != source_key ||
                     cohort[0].at("class_name") != "Drift" || cohort[0].at("role") != "source"))
                    throw std::runtime_error(
                        "Existing source identity conflicts with current owning Timbre; native "
                        "replacement is outside append-only authoring");
                if (!cohort.empty() && selected.contains("source_selections")) {
                    auto input = selected_part_arguments(selection, selected.at("part_id"));
                    input["selections"] = selected.at("source_selections");
                    const auto source = part_timbre_plan(session, desired, input);
                    const LomPath source_path{
                        {"song",
                         "tracks",
                         std::to_string(binding.observation.at("track_index").get<std::uint32_t>()),
                         "devices",
                         "0"}};
                    json candidates = json::array();
                    for (const auto& intent : source.intents) {
                        const auto resolved =
                            resolve_native_display_value(source_path,
                                                         intent.selection.capability_id,
                                                         intent.target,
                                                         intent.selection.tolerance,
                                                         *transport);
                        if (!resolved ||
                            resolved->status != NativeDisplayResolutionStatus::Candidate ||
                            !resolved->candidate || !resolved->evidence)
                            throw std::runtime_error("Selected existing Source physical target is "
                                                     "unavailable before project dispatch");
                        candidates.push_back(*resolved->evidence);
                    }
                    // Formatter searches do not grant identity. Recheck actual
                    // handles and the entire retained cohort after their reads.
                    static_cast<void>(current_native_baseline(*store, prior, *context, *transport));
                    part["source_native_candidates"] = std::move(candidates);
                }
                if (selected.contains("effect_selections")) {
                    auto input = selected_part_arguments(selection, selected.at("part_id"));
                    input["effect_selections"] = selected.at("effect_selections");
                    const auto effects = part_effect_plan(session, desired, input);
                    if (cohort.empty() && !selected.contains("source_selections"))
                        throw std::runtime_error("Select the owning source before native effects");
                    if (cohort.size() > effects.entries.size() + 1)
                        throw std::runtime_error(
                            "Select the complete existing finite native effect order");
                    for (std::size_t index = 1; index < cohort.size(); ++index) {
                        const auto& entry = effects.entries.at(index - 1);
                        if (entry.desired_chain_index != index ||
                            cohort[index].at("device_key") != entry.device_key ||
                            cohort[index].at("class_name") !=
                                (entry.device == ManagedNativeDevice::Utility ? "StereoGain"
                                                                              : "Eq8"))
                            throw std::runtime_error(
                                "Existing native effects cannot be deleted, reordered or remapped "
                                "by this append-only selection");
                    }
                    json candidates = json::array();
                    for (std::size_t index = 1; index < cohort.size(); ++index) {
                        const auto& entry = effects.entries.at(index - 1);
                        if (!entry.final_modes.empty() && cohort[index].at("is_active") == false) {
                            candidates.push_back({{"device_key", entry.device_key},
                                                  {"admission", "after_selected_enable_phase"}});
                            continue;
                        }
                        // Requested final bypass still requires configuring its
                        // physical values. Admit them now when current modes
                        // already expose the selected controls.
                        auto active_entry = entry;
                        active_entry.final_modes.clear();
                        auto controls = effect_inspection_selection(active_entry);
                        auto mode_guards = observed_device_selections(binding);
                        auto admitted = controls;
                        admitted.physical_intents.clear();
                        // EQ Scale changes the meaning of displayed band gain.
                        // Its planned canonical setup must already be actual
                        // before targets can be admitted in the current mode.
                        for (const auto& intent : entry.setup_physical_intents)
                            if (intent.capability_id == "eq8.scale")
                                admitted.physical_intents.push_back(intent);
                        mode_guards.at(index) = std::move(admitted);
                        if (!inspect_current_device_intent(
                                *context, binding, mode_guards, *transport)) {
                            candidates.push_back(
                                {{"device_key", entry.device_key},
                                 {"admission", "after_selected_mode_or_scale_phases"}});
                            continue;
                        }
                        const LomPath path{
                            {"song",
                             "tracks",
                             std::to_string(
                                 binding.observation.at("track_index").get<std::uint32_t>()),
                             "devices",
                             std::to_string(index)}};
                        json physical = json::array();
                        for (const auto& intent : controls.physical_intents) {
                            const auto resolved = resolve_native_display_value(path,
                                                                               intent.capability_id,
                                                                               intent.target,
                                                                               intent.tolerance,
                                                                               *transport);
                            if (!resolved ||
                                resolved->status != NativeDisplayResolutionStatus::Candidate ||
                                !resolved->candidate || !resolved->evidence)
                                throw std::runtime_error("Selected existing Effect physical target "
                                                         "is unavailable before project dispatch");
                            physical.push_back(*resolved->evidence);
                        }
                        static_cast<void>(
                            current_native_baseline(*store, prior, *context, *transport));
                        candidates.push_back(
                            {{"device_key", entry.device_key}, {"physical_candidates", physical}});
                    }
                    part["effect_native_preflight"] = std::move(candidates);
                }
            } else if (selected.contains("effect_selections") &&
                       !selected.contains("source_selections"))
                throw std::runtime_error("Select the owning source before native effects");
        }
        bool static_pan = false;
        if (selected.contains("static_mixer"))
            static_pan =
                std::ranges::find(selected.at("static_mixer").at("domains"), json("pan")) !=
                selected.at("static_mixer").at("domains").end();
        if (static_pan || selected.contains("pan_lane")) {
            const auto sampled = sample_managed_envelope(*context,
                                                         binding.project_key,
                                                         binding.binding_key,
                                                         {{"kind", "panning"}},
                                                         {0.0},
                                                         *transport);
            if (!sampled || sampled->binding.outcome != ManagedObservationOutcome::Observed)
                throw std::runtime_error(
                    "Selected native pan envelope cannot be observed before project admission");
            const auto sampled_binding = managed_observed_binding(sampled->binding);
            if (!sampled_binding || native_observation_fields(sampled_binding->observation) !=
                                        native_observation_fields(binding.observation))
                throw std::runtime_error(
                    "Current pan inspection differs from the retained Part cohort");
            part["current_pan_envelope"] = sampled->evidence.at("envelope");
            const bool present = sampled->evidence.at("envelope").at("has_envelope").get<bool>();
            if (static_pan && present)
                throw std::runtime_error(
                    "Mandatory static pan is unavailable on an existing selected envelope; select "
                    "its explicit replacement separately");
            if (selected.contains("pan_lane") && selected.at("pan_lane").at("mode") == "absent" &&
                present)
                throw std::runtime_error(
                    "Existing selected pan envelope requires explicit whole-lane replacement");
            if (selected.contains("pan_lane")) {
                auto input = selected_part_arguments(selection, selected.at("part_id"));
                input["lane_index"] = selected.at("pan_lane").at("lane_index");
                static_cast<void>(owning_step_pan_lane(session, desired, binding, input));
            }
        }
        if (selected.contains("static_mixer")) {
            const bool future_source =
                selected.contains("source_selections") &&
                binding.observation.at("manifest").at("devices_empty") == true;
            if (future_source) {
                // An instrument changes a MIDI Track's audio-output eligibility.
                // Its future controls must be observed after insertion.
                part["late_native_mixer_admission_required"] = true;
            } else {
                auto input = selected_part_arguments(selection, selected.at("part_id"));
                input.update(selected.at("static_mixer"));
                const auto mixer = part_static_mix_plan(session, desired, input);
                part["current_static_mixer_inspection"] =
                    inspect_current_mixer(*context, binding, mixer.desired, *transport, false);
            }
        }
        bindings.emplace(id, binding);
    }
    for (const auto& id : selection.value("retire_part_ids", json::array())) {
        auto inactive = owner;
        inactive.part_id = detail::checked_integer<std::uint64_t>(id, "retire_part_id");
        if (active_part(session, inactive))
            throw std::runtime_error("Only removed Parts support mute retirement");
        const auto retained = binding_history(*store, owner.score_id, inactive.part_id);
        if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
        if (!retained.acknowledged)
            throw std::runtime_error("Removed Part has no verified historical native baseline");
        auto desired = historical_part(inactive, *retained.acknowledged);
        const auto binding =
            current_native_baseline(*store, *retained.acknowledged, *context, *transport);
        if (!projection_matches(desired.encoded, binding.observation))
            throw std::runtime_error("Removed Part's historical music is not currently verified");
        static_cast<void>(
            native_note_associations(desired.keys, desired.encoded, binding.observation));
        retired.push_back(
            {{"part_id", inactive.part_id},
             {"historical_projection_attempt", *desired.historical_projection_attempt},
             {"native_baseline", managed_binding_to_json(binding)},
             {"retirement_action", "mute"},
             {"retained_solo", binding.observation.at("manifest").at("track").at("solo")},
             {"solo_policy", "preserve"},
             {"audible_residual",
              "Muting a retained soloed Track can leave other Tracks muted via solo"}});
    }
    for (auto& route : routes) {
        if (route.at("selection").at("kind") != "adopt_group") continue;
        const auto anchor = route.at("selection").at("part_id").get<std::uint64_t>();
        if (!bindings.contains(anchor))
            throw std::runtime_error("Establish current Group hierarchy and explicit Clip adoption "
                                     "separately before coordinating its native revisions");
        const auto retained = binding_history(*store, owner.score_id, anchor);
        if (!projection_matches(desired_parts.at(anchor).encoded,
                                bindings.at(anchor).observation)) {
            auto resolved = route.at("resolved_selection");
            resolved["projection_source"] = "retained_verified_realization";
            resolved["historical_projection_attempt"] = retained.acknowledged->intent.attempt_id;
            route["resolved_selection"] = std::move(resolved);
        }
        for (const auto& key : route.at("plan").at("intent").at("member_binding_keys")) {
            const auto text = key.get<std::string>();
            const auto member = std::stoull(text.substr(5), nullptr, 16);
            const auto history = binding_history(*store, owner.score_id, member);
            if (history.unresolved) throw ReconciliationRequired(*history.unresolved);
            if (!history.acknowledged)
                throw std::runtime_error(
                    "Every selected existing native Group member needs separate current Clip "
                    "authority; managed creation cannot group a new Track");
            static_cast<void>(
                current_native_baseline(*store, *history.acknowledged, *context, *transport));
        }
    }
    // Repeated selected creation becomes a separately visible current adoption
    // only after a strict successful same-context own Aux creation/adoption proof.
    // Names locate that approved current selection; they never grant it.
    for (auto& route : routes) {
        const auto& selected = route.at("selection");
        if (selected.at("kind") != "create_return") continue;
        const auto aux_key = "aux_" + hexadecimal(selected.at("aux_id").get<std::uint64_t>());
        const RealizationStoredAttempt* found = nullptr;
        for (const auto& [token, attempt] : store->attempts()) {
            static_cast<void>(token);
            const auto& receipt = last_receipt(attempt);
            if (attempt.intent.score_id.value != owner.score_id ||
                receipt.outcome != ManagedOperationOutcome::Acknowledged ||
                receipt.context.bridge_instance != context->bridge_instance ||
                receipt.context.document_token != context->document_token ||
                receipt.request.property_or_method != "sunny_managed_apply_routing" ||
                !receipt.journal)
                continue;
            const auto& intent = std::get<json>(receipt.request.args.at(0)).at("intent");
            if ((intent.at("kind") == "create_return" || intent.at("kind") == "adopt_return") &&
                intent.at("aux_key") == aux_key &&
                receipt.journal->at("result").at("routing").at("desired_match") == true &&
                receipt.journal->at("result").at("routing").at(
                    "untouched_observed_state_preserved") == true &&
                (!found || attempt.dispatch_ordinal > found->dispatch_ordinal))
                found = &attempt;
        }
        if (!found) continue;
        const auto anchor = selected.at("part_id").get<std::uint64_t>();
        if (!bindings.contains(anchor))
            throw std::runtime_error(
                "Use a retained active Part to approve an existing own Return");
        const auto request = make_managed_routing_candidates_request(*context, bindings.at(anchor));
        if (!request) throw std::runtime_error("Current Return cohort cannot be inspected");
        const auto response = transport->send(*request);
        if (!response.success || !response.value || !std::holds_alternative<json>(*response.value))
            throw std::runtime_error("Current Return cohort is unavailable");
        const auto candidates =
            parse_managed_routing_candidates(*request, *context, std::get<json>(*response.value));
        if (!candidates) throw std::runtime_error("Actual current Return cohort is malformed");
        const auto& old_rows =
            last_receipt(*found).journal->at("result").at("routing").at("after").at("mixers");
        const auto& rows = candidates->at("frame").at("mixers");
        const auto tag =
            "Sunny|" + project_key(session, owner.score_id) + "|" + aux_key + "|return";
        auto row = std::ranges::find_if(
            rows, [&](const auto& r) { return r.at("kind") == "return" && r.at("name") == tag; });
        if (row == rows.end() ||
            std::ranges::count_if(rows,
                                  [&](const auto& r) {
                                      return r.at("kind") == "return" && r.at("name") == tag;
                                  }) != 1 ||
            std::ranges::none_of(old_rows, [&](const auto& r) {
                return r.at("kind") == "return" && r.at("name") == tag &&
                       r.at("index") == row->at("index");
            }))
            throw std::runtime_error("Saved own Return selection changed; explicitly select its "
                                     "current actual adoption");
        auto resolved = selected;
        resolved["kind"] = "adopt_return";
        resolved["return_index"] = row->at("index");
        route["resolved_selection"] = std::move(resolved);
        route["current_return_frame"] = candidates->at("frame");
    }
    for (auto& route : routes) {
        const auto& selected = route.at("selection");
        if (selected.at("kind") != "send_level") continue;
        const auto anchor = selected.at("part_id").get<std::uint64_t>();
        const bool future_return = std::ranges::any_of(selection.at("routing"), [&](const auto& r) {
            return (r.at("kind") == "create_return" || r.at("kind") == "adopt_return") &&
                   r.at("aux_id") == selected.at("aux_id");
        });
        const auto& part = *std::ranges::find_if(selection.at("parts"), [&](const auto& p) {
            return p.at("part_id") == selected.at("part_id");
        });
        const bool future_source =
            bindings.contains(anchor) && part.contains("source_selections") &&
            bindings.at(anchor).observation.at("manifest").at("devices_empty") == true;
        if (!bindings.contains(anchor) || future_return || future_source) {
            route["late_native_send_admission_required"] = true;
        } else {
            route["current_send_inspection"] = inspect_current_send(
                *context, bindings.at(anchor), route.at("plan").at("intent"), *transport, false);
        }
    }
    json song_inspection = nullptr;
    if (selection.contains("song_settings_part_id")) {
        const auto anchor = selection.at("song_settings_part_id").get<std::uint64_t>();
        if (bindings.contains(anchor)) {
            const auto desired = plan_managed_song_settings(*session.score->find(owner.score_id));
            song_inspection = inspect_current_song_settings(
                *context, bindings.at(anchor), *desired, *transport, false);
        }
    }
    return {{"success", true},
            {"mutation_dispatched", false},
            {"authority_granted", false},
            {"plan",
             {{"score_id", owner.score_id},
              {"project_revision", owner.revision},
              {"native_context",
               {{"bridge_instance", context->bridge_instance},
                {"document_token", context->document_token}}},
              {"parts", parts},
              {"routing", routes},
              {"retirements", retired},
              {"song_settings", song},
              {"current_song_settings_inspection", song_inspection},
              {"late_native_descriptor_admission_required", true},
              {"complete_project_realization", false},
              {"host_qualified", false},
              {"unapplied_domains",
               {"unselected Parts and authored leaves",
                "Return Pre/Post policy and processing",
                "complete automation breakpoint/modulation populations",
                "DSP equivalence and audible judgment"}}}}};
}

json selected_fields(const json& value, std::initializer_list<const char*> fields) {
    json selected = json::object();
    for (const auto* field : fields)
        if (value.contains(field)) selected[field] = value.at(field);
    return selected;
}

// Final summaries retain the actual guard and selected readback, rather than
// repeating entire note, Device, Set and formatter-search populations per phase.
json final_binding_guard(const ManagedBindingReceipt& binding) {
    auto guard = selected_fields(binding.observation,
                                 {"track_index",
                                  "slot_index",
                                  "content_fingerprint",
                                  "note_identity_fingerprint",
                                  "device_identity_fingerprint",
                                  "group_authority_fingerprint"});
    guard.update({{"bridge_instance", binding.context.bridge_instance},
                  {"document_token", binding.context.document_token},
                  {"project_key", binding.project_key},
                  {"binding_key", binding.binding_key},
                  {"note_count", binding.observation.at("note_identity").at("notes").size()}});
    guard["devices_empty"] = binding.observation.at("manifest").at("devices_empty");
    guard["device_count"] =
        binding.observation.contains("device_identity")
            ? json(binding.observation.at("device_identity").at("cohort").size())
        : guard.at("devices_empty") == true ? json(0)
                                            : json(nullptr);
    return guard;
}

json final_device_inspection(const json& raw, const ManagedBindingReceipt& binding) {
    const auto& body = raw.at("inspection");
    auto output = selected_fields(body,
                                  {"schema_version",
                                   "devices",
                                   "authority_origin",
                                   "native_mutation_started",
                                   "native_knob_only",
                                   "host_qualified",
                                   "opaque_state_observed"});
    output["binding_guard"] = final_binding_guard(binding);
    output["resolutions"] = json::array();
    for (const auto& resolution : body.at("resolutions")) {
        auto compact = selected_fields(resolution, {"device_key", "intent", "current_readback"});
        compact.update(selected_fields(resolution.at("candidate"),
                                       {"parameter_index", "parameter_original_name", "unit"}));
        output["resolutions"].push_back(std::move(compact));
    }
    return output;
}

json final_mixer_inspection(const json& raw, const ManagedBindingReceipt& binding) {
    const auto& body = raw.at("inspection");
    const auto& before = body.at("before");
    auto output = selected_fields(body,
                                  {"schema_version",
                                   "desired",
                                   "selected_domains",
                                   "scope",
                                   "authority_origin",
                                   "native_mutation_started"});
    output["binding_guard"] = final_binding_guard(binding);
    json current = json::object();
    const auto& desired = body.at("desired");
    if (desired.contains("volume"))
        current["volume"] = body.at("candidates").at("volume").at("current_display");
    if (desired.contains("pan"))
        current["pan"] =
            before.at("mixer_capture").at("parameters").at(1).at("descriptor").at("value");
    for (const auto* control : {"mute", "solo"})
        if (desired.contains(control)) current[control] = before.at("track_context").at(control);
    output["current_readback"] = std::move(current);
    output["solo_cohort"] = json::array();
    for (const auto& member : before.at("solo_cohort"))
        output["solo_cohort"].push_back(
            selected_fields(member, {"kind", "index", "mute", "solo", "muted_via_solo"}));
    return output;
}

json final_send_inspection(const json& raw, const ManagedBindingReceipt& binding) {
    auto output = selected_fields(raw.at("inspection"),
                                  {"schema_version",
                                   "intent",
                                   "send_readback",
                                   "scope",
                                   "authority_origin",
                                   "native_mutation_started"});
    output["binding_guard"] = final_binding_guard(binding);
    output["logical_send_complete"] = false;
    return output;
}

json final_song_inspection(const json& raw, const ManagedBindingReceipt& binding) {
    const auto& body = raw.at("inspection");
    auto output = selected_fields(
        body,
        {"schema_version", "desired", "scope", "authority_origin", "native_mutation_started"});
    output["binding_guard"] = final_binding_guard(binding);
    output["current_settings"] = body.at("before").at("settings");
    return output;
}

json final_bypassed_configuration(const RealizationStore& store,
                                  const ManagedDeviceRecoveryScope& scope,
                                  const ManagedBindingReceipt& binding,
                                  const NativeEffectPlan& effects) {
    auto folded = fold_managed_device_recovery(store.attempts(), scope);
    if (!folded || folded->context.bridge_instance != binding.context.bridge_instance ||
        folded->context.document_token != binding.context.document_token ||
        folded->device_identity_fingerprint !=
            binding.observation.at("device_identity_fingerprint").get<std::string>())
        throw std::runtime_error(
            "Final bypass configuration history does not join the current Device cohort");
    json devices = json::array();
    for (const auto& entry : effects.entries) {
        if (entry.final_modes.empty()) continue;
        const auto member = std::ranges::find(
            folded->selections, entry.device_key, &ManagedDeviceAdoptionSelection::device_key);
        const auto residual = std::ranges::find(
            folded->residuals, entry.device_key, &ManagedDeviceRecoveryResidual::device_key);
        if (member == folded->selections.end() ||
            member->chain_index != entry.desired_chain_index || member->device != entry.device ||
            !member->authored_bypass || residual == folded->residuals.end() ||
            residual->bypassed_physical_intents_unknown)
            throw std::runtime_error(
                "Final selected bypass configuration is unavailable in typed history");
        auto active = entry;
        active.final_modes.clear();
        const auto selected = effect_inspection_selection(active);
        json physical = json::array();
        for (const auto& intent : selected.physical_intents) {
            const auto saved = std::ranges::find(residual->bypassed_physical_intents,
                                                 intent.capability_id,
                                                 &ManagedDevicePhysicalIntent::capability_id);
            if (saved == residual->bypassed_physical_intents.end() ||
                saved->target != intent.target || saved->tolerance != intent.tolerance)
                throw std::runtime_error("Final selected bypass physical targets differ from "
                                         "verified configuration history");
            physical.push_back({{"capability_id", intent.capability_id},
                                {"target", intent.target},
                                {"tolerance", intent.tolerance}});
        }
        devices.push_back({{"device_key", entry.device_key},
                           {"chain_index", entry.desired_chain_index},
                           {"physical_intents", physical}});
    }
    return {{"device_history_attempt", folded->device_history_attempt},
            {"device_identity_fingerprint", folded->device_identity_fingerprint},
            {"devices", devices}};
}

json verify_project_realization(const McpSession& session,
                                const json& selection,
                                LomTransport* transport) {
    const auto owner = owning_project_revision(session, selection);
    if (!transport || !transport->ensure_connected())
        throw std::runtime_error("Final native readback is unavailable");
    const auto context = managed_bridge_context(*transport);
    if (!context) throw std::runtime_error("Final native context is unavailable");
    const auto store = history(session, false);
    json evidence = json::array();
    std::map<std::uint64_t, ManagedBindingReceipt> final_bindings;
    const auto verify_part = [&](const json& id, bool retirement) {
        auto input = selected_part_arguments(selection, id);
        auto desired = retirement
                           ? recovery_part(session, input, *transport)
                           : desired_part(session, input, owning_part(session, input), *transport);
        const auto retained = binding_history(*store, owner.score_id, desired.part_id);
        if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
        if (!retained.acknowledged)
            throw std::runtime_error("Selected final native history is unavailable");
        const auto binding =
            current_native_baseline(*store, *retained.acknowledged, *context, *transport);
        if (!projection_matches(desired.encoded, binding.observation))
            throw std::runtime_error(
                "Final actual native notes or geometry differ from the selected owning projection");
        static_cast<void>(
            native_note_associations(desired.keys, desired.encoded, binding.observation));
        if (retirement && binding.observation.at("manifest").at("track").at("mute") != true)
            throw std::runtime_error("Selected removed Part is not actually muted");
        json proof{{"part_id", desired.part_id},
                   {"retirement", retirement},
                   {"binding_guard", final_binding_guard(binding)},
                   {"selected_notes_and_geometry_verified", true},
                   {"retained_current_cohort_verified", true}};
        if (retirement) {
            ManagedStaticMixerDesired mute;
            mute.mute = true;
            proof["static_mixer_inspection"] = final_mixer_inspection(
                inspect_current_mixer(*context, binding, mute, *transport, true), binding);
            proof["retained_solo"] = binding.observation.at("manifest").at("track").at("solo");
            proof["solo_policy"] = "preserve";
            proof["audible_residual"] =
                "Mute-only retirement preserves Solo, which can suppress other Tracks";
        }

        if (!retirement) {
            const auto& selected = *std::ranges::find_if(
                selection.at("parts"), [&](const auto& p) { return p.at("part_id") == id; });
            if (selected.contains("source_selections") || selected.contains("effect_selections")) {
                auto devices = observed_device_selections(binding);
                bool bypassed = false;
                if (devices.empty())
                    throw std::runtime_error("Final selected Device cohort is empty");
                if (selected.contains("source_selections")) {
                    auto source_input = input;
                    source_input["selections"] = selected.at("source_selections");
                    const auto source = part_timbre_plan(session, desired, source_input);
                    if (devices.at(0).device_key != source_device_key(source) ||
                        devices.at(0).device != ManagedNativeDevice::Drift)
                        throw std::runtime_error(
                            "Final selected Source identity differs from owning Timbre");
                    devices.at(0).physical_intents = device_intents(source);
                }
                if (selected.contains("effect_selections")) {
                    auto effect_input = input;
                    effect_input["effect_selections"] = selected.at("effect_selections");
                    const auto effects = part_effect_plan(session, desired, effect_input);
                    if (devices.size() != effects.entries.size() + 1)
                        throw std::runtime_error("Final selected effect chain population differs");
                    for (const auto& entry : effects.entries) {
                        auto& member = devices.at(entry.desired_chain_index);
                        if (member.device_key != entry.device_key || member.device != entry.device)
                            throw std::runtime_error(
                                "Final selected effect identity or order differs");
                        member = effect_inspection_selection(entry);
                    }
                    bypassed = std::ranges::any_of(effects.entries, [](const auto& entry) {
                        return !entry.final_modes.empty();
                    });
                    if (bypassed) {
                        ManagedDeviceRecoveryScope scope{
                            session.realization->metadata.workspace_namespace,
                            ScoreId{owner.score_id},
                            PartId{desired.part_id},
                            retained.acknowledged->intent.attempt_id,
                            retained.acknowledged->intent.attempt_id};
                        proof["bypassed_device_configuration"] =
                            final_bypassed_configuration(*store, scope, binding, effects);
                        proof["bypassed_physical_targets_verified_before_disable"] = true;
                        proof["current_bypassed_display_verified"] = false;
                    }
                }
                const auto inspected =
                    inspect_current_device_intent(*context, binding, devices, *transport);
                if (!inspected)
                    throw std::runtime_error(
                        "Final actual selected Device physical intent differs or is unavailable");
                proof["device_inspection"] = final_device_inspection(*inspected, binding);
                proof["active_selected_device_physical_intent_verified"] = true;
                proof["selected_device_physical_intent_verified"] = !bypassed;
            }
            if (selected.contains("static_mixer")) {
                auto mixer_input = input;
                mixer_input.update(selected.at("static_mixer"));
                auto mixer = part_static_mix_plan(session, desired, mixer_input);
                if (selected.contains("pan_lane")) {
                    mixer.desired.pan.reset();
                    proof["static_pan_scope"] =
                        "typed static phase acknowledgement before the selected pan lane";
                }
                if (mixer.desired.volume || mixer.desired.pan || mixer.desired.mute ||
                    mixer.desired.solo)
                    proof["static_mixer_inspection"] = final_mixer_inspection(
                        inspect_current_mixer(*context, binding, mixer.desired, *transport, true),
                        binding);
            }
            if (selected.contains("pan_lane")) {
                input["lane_index"] = selected.at("pan_lane").at("lane_index");
                const auto lane = owning_step_pan_lane(session, desired, binding, input);
                const auto& points = lane.at("points");
                std::vector<double> times, expected;
                for (std::size_t i = 0; i < points.size(); ++i) {
                    const double start = points[i].at("time").get<double>();
                    const double stop = i + 1 < points.size()
                                            ? points[i + 1].at("time").get<double>()
                                            : desired.projection.clip_end;
                    times.push_back(start);
                    expected.push_back(points[i].at("value").get<double>());
                    const double midpoint = start + (stop - start) / 2.0;
                    if (midpoint > start && midpoint < stop) {
                        times.push_back(midpoint);
                        expected.push_back(expected.back());
                    }
                }
                const auto sampled = sample_managed_envelope(*context,
                                                             binding.project_key,
                                                             binding.binding_key,
                                                             lane.at("parameter"),
                                                             times,
                                                             *transport);
                if (!sampled || sampled->binding.outcome != ManagedObservationOutcome::Observed ||
                    sampled->evidence.at("envelope").at("has_envelope") != true)
                    throw std::runtime_error("Final selected pan lane cannot be observed");
                const auto sampled_binding = managed_observed_binding(sampled->binding);
                if (!sampled_binding || native_observation_fields(sampled_binding->observation) !=
                                            native_observation_fields(binding.observation))
                    throw std::runtime_error(
                        "Final pan readback differs from the verified Part cohort");
                const auto& samples = sampled->evidence.at("envelope").at("samples");
                if (samples.size() != expected.size())
                    throw std::runtime_error("Final selected pan lane sample population differs");
                for (std::size_t i = 0; i < expected.size(); ++i)
                    if (samples[i].at("time") != times[i] ||
                        std::abs(samples[i].at("value").get<double>() - expected[i]) > 1.0e-6)
                        throw std::runtime_error("Final actual selected pan lane differs at a "
                                                 "boundary or interval midpoint");
                proof["pan_lane_observation"] = {
                    {"binding_guard", final_binding_guard(*sampled_binding)},
                    {"parameter", lane.at("parameter")},
                    {"envelope",
                     selected_fields(sampled->evidence.at("envelope"),
                                     {"has_envelope", "samples"})}};
                proof["pan_lane_interval_end"] = desired.projection.clip_end;
                proof["complete_envelope_population_observed"] = false;
            }
        }
        if (!retirement) final_bindings.emplace(desired.part_id, binding);
        evidence.push_back(std::move(proof));
    };
    for (const auto& part : selection.at("parts"))
        verify_part(part.at("part_id"), false);
    for (const auto& id : selection.value("retire_part_ids", json::array()))
        verify_part(id, true);
    json sends = json::array();
    for (const auto& selected : selection.at("routing")) {
        if (selected.at("kind") != "send_level") continue;
        auto input = selected_part_arguments(selection, selected.at("part_id"));
        input.update(selected);
        const auto owning = owning_routing_plan(session, input);
        sends.push_back({{"part_id", selected.at("part_id")},
                         {"aux_id", selected.at("aux_id")},
                         {"inspection",
                          final_send_inspection(
                              inspect_current_send(
                                  *context,
                                  final_bindings.at(selected.at("part_id").get<std::uint64_t>()),
                                  owning.intent,
                                  *transport,
                                  true),
                              final_bindings.at(selected.at("part_id").get<std::uint64_t>()))}});
    }
    json song = nullptr;
    if (selection.contains("song_settings_part_id")) {
        const auto desired = plan_managed_song_settings(*session.score->find(owner.score_id));
        if (!desired) throw std::runtime_error("Final owning Song settings are unsupported");
        const auto& binding =
            final_bindings.at(selection.at("song_settings_part_id").get<std::uint64_t>());
        song = final_song_inspection(
            inspect_current_song_settings(*context, binding, *desired, *transport, true), binding);
    }
    return {{"success", true},
            {"project_revision", owner.revision},
            {"final_native_observations", evidence},
            {"final_send_inspections", sends},
            {"final_song_settings_inspection", song},
            {"final_current_cohorts_verified", true},
            {"sample_tolerance_internal", 1.0e-6},
            {"physical_readback_scope",
             "fresh selected active Device, static Mixer, Send and Song intent, typed bypass "
             "configuration before disable, typed routing history, retained current cohorts "
             "and finite Step samples"},
            {"logical_send_complete", false},
            {"dsp_equivalence_qualified", false}};
}

} // namespace

void register_project_realization_tools(McpServer& server,
                                        const McpSession& session,
                                        LomTransport* transport) {
    auto domain = server.registration_scope(McpDocumentDomain::None);
    server.register_tool(
        "ordinary_clip_history",
        "Inspect durable ordinary Clip attempts; restored records provide query authority only, "
        "without restoring undo stacks",
        {{"type", "object"},
         {"additionalProperties", false},
         {"properties", {{"attempt_id", {{"type", "string"}, {"pattern", "^[0-9a-f]{32}$"}}}}}},
        [session](const json& arguments) -> json {
            try {
                auto store = history(session, false);
                json attempts = json::array();
                for (const auto& [id, attempt] : store->ordinary_attempts()) {
                    if (arguments.contains("attempt_id") && arguments.at("attempt_id") != id)
                        continue;
                    json evidence = json::array();
                    for (const auto& receipt : attempt.evidence)
                        evidence.push_back(ordinary_receipt_to_json(receipt));
                    attempts.push_back({{"attempt_id", id},
                                        {"dispatch_ordinal", attempt.dispatch_ordinal},
                                        {"dispatch_state", "may_have_sent"},
                                        {"prepared", ordinary_receipt_to_json(attempt.prepared)},
                                        {"evidence", evidence}});
                }
                return {{"success", true},
                        {"workspace_namespace", store->workspace_namespace()},
                        {"attempts", attempts},
                        {"mutation_dispatched", false},
                        {"undo_stack_restored", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    server.register_tool(
        "ordinary_clip_reconcile",
        "Query the original fenced ordinary Clip operation without retrying preparation or any "
        "native setter",
        {{"type", "object"},
         {"additionalProperties", false},
         {"required", {"attempt_id"}},
         {"properties", {{"attempt_id", {{"type", "string"}, {"pattern", "^[0-9a-f]{32}$"}}}}}},
        [session, transport](const json& arguments) -> json {
            try {
                auto store = history(session, false);
                const auto id = arguments.at("attempt_id").get<std::string>();
                const auto* attempt = store->find_ordinary(id);
                if (!attempt)
                    return decline("Original ordinary Clip dispatch fence is unavailable");
                if (!transport || transport->records_without_execution() ||
                    !transport->ensure_connected())
                    return decline(
                        "An executing bridge connection is required for read-only reconciliation");
                const auto& original =
                    attempt->evidence.empty() ? attempt->prepared : attempt->evidence.back();
                const auto result = reconcile_ordinary_clip(original, *transport);
                if (!result)
                    return decline(
                        "Ordinary query response rejected; retained history remains unchanged");
                const auto saved = store->append_ordinary_evidence(id, *result);
                json output{{"success", true},
                            {"attempt_id", id},
                            {"receipt", ordinary_receipt_to_json(*result)},
                            {"history_saved", saved.has_value()},
                            {"mutation_retried", false},
                            {"mutation_dispatched", false},
                            {"undo_stack_restored", false}};
                if (!saved) output["history_error"] = saved.error().message;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto native_tools = std::make_shared<NativeAuthoringTools>();
    const auto register_native_tool = [&server, native_tools](std::string name,
                                                              std::string description,
                                                              json schema,
                                                              McpToolHandler handler) {
        native_tools->emplace(name, handler);
        server.register_tool(
            std::move(name), std::move(description), std::move(schema), std::move(handler));
    };
    auto inspection_schema = revision_schema();
    inspection_schema["additionalProperties"] = false;
    register_native_tool(
        "project_realization_inspect_routing",
        "Inspect actual current routing identifiers and attached native destinations for an "
        "owning Part; this read creates no preview, native mutation or authority",
        std::move(inspection_schema),
        [session, transport](const json& arguments) -> json {
            try {
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                const auto desired =
                    desired_part(session, arguments, owning_part(session, arguments), *transport);
                const auto store = history(session, false);
                const auto retained = binding_history(*store, desired.score_id, desired.part_id);
                if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
                if (!retained.acknowledged)
                    return decline("Create or explicitly adopt this native Part first");
                const auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                const auto binding = current_binding(*store, *retained.acknowledged);
                if (!projection_matches(desired.encoded, binding.observation))
                    return decline(
                        "Revise native Score notes and geometry before routing inspection");
                static_cast<void>(
                    native_note_associations(desired.keys, desired.encoded, binding.observation));
                const auto request = make_managed_routing_candidates_request(*context, binding);
                if (!request) return decline("Current routing inspection is unavailable");
                const auto response = transport->send(*request);
                if (!response.success || !response.value ||
                    !std::holds_alternative<json>(*response.value))
                    return decline(
                        response.error.value_or("Actual routing inspection is unavailable"));
                const auto candidates = parse_managed_routing_candidates(
                    *request, *context, std::get<json>(*response.value));
                if (!candidates ||
                    !projection_matches(desired.encoded, candidates->at("observation")))
                    return decline(
                        "Current routing inspection differs from the owning musical projection");
                static_cast<void>(native_note_associations(
                    desired.keys, desired.encoded, candidates->at("observation")));
                return {{"success", true},
                        {"candidates", *candidates},
                        {"authority_granted", false},
                        {"mutation_dispatched", false},
                        {"project_revision", desired.revision},
                        {"host_qualified", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_plan_routing",
        "Plan an owning finite Return, output, send or current Group operation; reject whole-Mix "
        "Pre/Post conflicts before native fences and retain unavailable routing domains",
        routing_schema(),
        [session](const json& arguments) -> json {
            try {
                auto owning = owning_routing_plan(session, arguments);
                return {{"success", true}, {"plan", owning.plan}, {"mutation_dispatched", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    for (const bool apply : {false, true}) {
        auto schema = routing_schema();
        if (apply) {
            schema["properties"]["preview"] = {{"type", "object"}};
            schema["properties"]["explicit_current_routing_approval"] = {{"type", "boolean"},
                                                                         {"const", true}};
            schema["required"].push_back("preview");
            schema["required"].push_back("explicit_current_routing_approval");
        }
        register_native_tool(
            apply ? "project_realization_apply_routing" : "project_realization_preview_routing",
            apply ? "Apply the exact approved owning routing preview once behind a durable fence; "
                    "current Group adoption grants hierarchy only and Return append preserves all "
                    "Parts"
                  : "Read the actual finite routing cohort for owning intent; Group bootstrap "
                    "requires exact current native selection and grants no Part authority",
            std::move(schema),
            [session, transport, apply](const json& arguments) -> json {
                try {
                    if (apply && (!arguments.at("explicit_current_routing_approval").is_boolean() ||
                                  arguments.at("explicit_current_routing_approval") != true))
                        return decline(
                            "Explicit approval of the exact current routing preview is required");
                    if (!transport || transport->records_without_execution())
                        return decline("An actual bridge connection is required");
                    auto candidate = owning_routing(session, arguments, *transport);
                    if (apply && candidate.desired.historical_projection_attempt &&
                        (!arguments.contains("historical_projection_attempt") ||
                         arguments.at("historical_projection_attempt") !=
                             *candidate.desired.historical_projection_attempt))
                        return decline("Approve the exact retained musical source returned by the "
                                       "Group preview");
                    const auto context = managed_bridge_context(*transport);
                    if (!context) return decline("Current native context is unavailable");
                    json raw;
                    if (apply)
                        raw = arguments.at("preview");
                    else {
                        auto response = transport->send(candidate.preview_request);
                        if (!response.success || !response.value ||
                            !std::holds_alternative<json>(*response.value))
                            return decline(
                                response.error.value_or("Actual routing preview is unavailable"));
                        raw = std::get<json>(*response.value);
                    }
                    auto preview =
                        parse_managed_routing_preview(candidate.preview_request, *context, raw);
                    if (!preview ||
                        !projection_matches(candidate.desired.encoded, preview->observation))
                        return decline("Current routing preview differs from owning intent or "
                                       "native musical projection");
                    static_cast<void>(native_note_associations(
                        candidate.desired.keys, candidate.desired.encoded, preview->observation));
                    json output;
                    if (!apply) {
                        output = {{"success", true},
                                  {"preview", raw},
                                  {"authority_granted", false},
                                  {"mutation_dispatched", false}};
                    } else {
                        auto store = history(session, true);
                        auto token = store->new_attempt_id();
                        if (!token)
                            return decline("Native routing operation identity is unavailable");
                        auto request = make_managed_routing_request(*context, *token, *preview);
                        if (!request)
                            return decline(
                                request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                    ? "ReplyCapacityUnavailable: complete routing evidence exceeds "
                                      "the finite bridge limit; no fence was created"
                                    : "Approved routing request is unavailable");
                        output = dispatch(store, candidate.desired, *context, *request, *transport);
                    }
                    output["plan"] = candidate.owning.plan;
                    if (candidate.owning.intent.at("kind") == "adopt_group") {
                        recovery_provenance(output, session, candidate.desired);
                        output.erase("native_projection_verified");
                        output["current_group_hierarchy_verified"] =
                            apply && output.value("success", false);
                    }
                    output["scope"] = candidate.owning.intent.at("kind") == "adopt_group"
                                          ? "explicit_current_group_hierarchy"
                                          : "selected_finite_native_routing";
                    output["part_authority_granted"] = false;
                    output["tap_policy_observed"] = false;
                    output["logical_send_complete"] = false;
                    output["other_native_sender_tap_policy_observed"] = false;
                    output["host_qualified"] = false;
                    output["complete_project_realization"] = false;
                    return output;
                } catch (const std::exception& failure) {
                    return workflow_failure(failure);
                }
            });
    }
    register_native_tool(
        "project_realization_plan_static_mixer",
        "Plan selected owning Channel fader, native Stereo pan, mute and solo; retain residual "
        "Mix intent and decline unresolved programme loudness without a fallback",
        static_mixer_schema(),
        [session](const json& arguments) -> json {
            try {
                const auto owner = owning_part(session, arguments);
                const auto plan = part_static_mix_plan(session, owner, arguments);
                return static_mixer_result({{"success", true},
                                            {"project_revision", owner.revision},
                                            {"mutation_dispatched", false}},
                                           native_mix_static_plan_to_json(plan),
                                           false);
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto mixer_preview_schema = static_mixer_schema();
    mixer_preview_schema["properties"]["purpose"] = {{"type", "string"},
                                                     {"enum", {"update", "adopt"}}};
    mixer_preview_schema["required"].push_back("purpose");
    register_native_tool(
        "project_realization_preview_static_mixer",
        "Preview selected owning static Mixer intent on exact current native handles without "
        "setters or new authority; choose update or explicit current-object adoption",
        std::move(mixer_preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                const auto purpose = arguments.at("purpose").get<std::string>();
                if (purpose != "adopt" && purpose != "update")
                    return decline("Unknown Mixer purpose");
                auto candidate = owning_static_mixer(session, arguments, transport);
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                auto request = make_managed_static_mixer_preview_request(
                    *context, candidate.binding, candidate.mixer_desired, purpose == "adopt");
                if (!request) return decline("Current static Mixer admission is unavailable");
                auto response = transport->send(*request);
                if (!response.success || !response.value ||
                    !std::holds_alternative<json>(*response.value))
                    return decline("Actual static Mixer preview is unavailable");
                const auto& raw = std::get<json>(*response.value);
                auto preview = parse_managed_static_mixer_preview(*request, *context, raw);
                if (!preview ||
                    !projection_matches(candidate.desired.encoded, preview->observation))
                    return decline("Native Mixer preview contradicts owning Score or Mix intent");
                static_cast<void>(native_note_associations(
                    candidate.desired.keys, candidate.desired.encoded, preview->observation));
                return static_mixer_result({{"success", true},
                                            {"preview", raw},
                                            {"project_revision", candidate.desired.revision},
                                            {"authority_granted", false},
                                            {"mutation_dispatched", false}},
                                           candidate.plan,
                                           purpose == "adopt");
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    for (const bool adoption : {false, true}) {
        auto schema = static_mixer_schema();
        schema["properties"]["preview"] = {{"type", "object"}};
        schema["properties"]["explicit_current_mixer_approval"] = {{"type", "boolean"},
                                                                   {"const", true}};
        schema["properties"]["explicit_set_wide_audible_approval"] = {{"type", "boolean"}};
        schema["required"].push_back("preview");
        schema["required"].push_back("explicit_current_mixer_approval");
        register_native_tool(
            adoption ? "project_realization_adopt_static_mixer"
                     : "project_realization_apply_static_mixer",
            adoption
                ? "Adopt exactly selected current native Mixer domains with explicit preview "
                  "approval; no native setters"
                : "Apply exact approved owning static Mixer values once, with a durable fence, "
                  "per-phase guards and actual readback; solo needs Set-wide audible approval",
            std::move(schema),
            [session, transport, adoption](const json& arguments) -> json {
                try {
                    if (!arguments.at("explicit_current_mixer_approval").is_boolean() ||
                        arguments.at("explicit_current_mixer_approval") != true)
                        return decline(
                            "Explicit approval of the current Mixer preview is required");
                    auto candidate = owning_static_mixer(session, arguments, transport);
                    auto context = managed_bridge_context(*transport);
                    if (!context) return decline("Current native context is unavailable");
                    auto expected = make_managed_static_mixer_preview_request(
                        *context, candidate.binding, candidate.mixer_desired, adoption);
                    if (!expected) return decline("Current static Mixer admission is unavailable");
                    auto preview = parse_managed_static_mixer_preview(
                        *expected, *context, arguments.at("preview"));
                    if (!preview ||
                        !projection_matches(candidate.desired.encoded, preview->observation))
                        return decline("Approved Mixer preview differs from current owning intent");
                    static_cast<void>(native_note_associations(
                        candidate.desired.keys, candidate.desired.encoded, preview->observation));
                    const bool setwide =
                        arguments.contains("explicit_set_wide_audible_approval") &&
                        arguments.at("explicit_set_wide_audible_approval").is_boolean() &&
                        arguments.at("explicit_set_wide_audible_approval") == true;
                    if (candidate.mixer_desired.solo && !setwide)
                        return decline("Selected solo requires explicit Set-wide audible approval");
                    auto store = history(session, true);
                    auto token = store->new_attempt_id();
                    if (!token) return decline("Native operation identity is unavailable");
                    auto request =
                        make_managed_static_mixer_request(*context, *token, *preview, setwide);
                    if (!request)
                        return decline(request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                           ? "ReplyCapacityUnavailable: complete Mixer evidence "
                                             "exceeds the finite bridge limit; no fence was created"
                                           : "Current approved Mixer request is unavailable");
                    return static_mixer_result(
                        dispatch(store, candidate.desired, *context, *request, *transport),
                        candidate.plan,
                        adoption);
                } catch (const std::exception& failure) {
                    return workflow_failure(failure);
                }
            });
    }
    auto retirement_schema = revision_schema();
    retirement_schema["additionalProperties"] = false;
    retirement_schema["properties"].erase("ppq");
    retirement_schema["properties"]["explicit_mute_retirement"] = {{"type", "boolean"},
                                                                   {"const", true}};
    retirement_schema["required"].push_back("explicit_mute_retirement");
    register_native_tool(
        "project_realization_retire_part",
        "Mute an explicitly approved removed Part on its retained native Track; preserve its "
        "Clip, notes, devices and history, using the last verified historical projection",
        std::move(retirement_schema),
        [session, transport](const json& arguments) -> json {
            try {
                if (!arguments.at("explicit_mute_retirement").is_boolean() ||
                    arguments.at("explicit_mute_retirement") != true)
                    return decline("Explicit native mute retirement approval is required");
                auto desired = owning_project_revision(session, arguments);
                desired.part_id =
                    detail::checked_integer<std::uint64_t>(arguments.at("part_id"), "part_id");
                const auto* score = session.score->find(desired.score_id);
                if (std::any_of(score->parts.begin(), score->parts.end(), [&](const auto& part) {
                        return part.id.value == desired.part_id;
                    }))
                    return decline("Part is still active; revise its owning Mix mute flag instead");
                auto store = history(session, false);
                const auto retained = binding_history(*store, desired.score_id, desired.part_id);
                if (retained.unresolved) throw ReconciliationRequired(*retained.unresolved);
                if (!retained.acknowledged)
                    return decline("Removed Part has no independently verified native binding");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                static_cast<void>(reviewed_native_target(*transport));
                const auto& prior = *retained.acknowledged;
                desired = historical_part(std::move(desired), prior);
                const auto binding = current_binding(*store, prior);
                if (!projection_matches(desired.encoded, binding.observation))
                    return decline(
                        "Historical removed-Part projection is not verified; reconcile it first");
                static_cast<void>(
                    native_note_associations(desired.keys, desired.encoded, binding.observation));
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                ManagedStaticMixerDesired mute;
                mute.mute = true;
                auto request =
                    make_managed_static_mixer_preview_request(*context, binding, mute, false);
                if (!request)
                    return decline("Current retained removed-Part Mixer authority is unavailable");
                auto response = transport->send(*request);
                if (!response.success || !response.value ||
                    !std::holds_alternative<json>(*response.value))
                    return decline("Actual removed-Part mute preview is unavailable");
                auto preview = parse_managed_static_mixer_preview(
                    *request, *context, std::get<json>(*response.value));
                if (!preview || !projection_matches(desired.encoded, preview->observation))
                    return decline(
                        "Current native removed-Part state differs from retained history");
                static_cast<void>(
                    native_note_associations(desired.keys, desired.encoded, preview->observation));
                store = history(session, true);
                auto token = store->new_attempt_id();
                if (!token) return decline("Native retirement operation identity is unavailable");
                auto mutation = make_managed_static_mixer_request(*context, *token, *preview);
                if (!mutation)
                    return decline(mutation.error() == ErrorCode::ManagedReplyCapacityExceeded
                                       ? "ReplyCapacityUnavailable: retirement evidence exceeds "
                                         "the finite bridge limit; no fence was created"
                                       : "Native mute retirement is unavailable");
                auto output = dispatch(store, desired, *context, *mutation, *transport);
                output["scope"] = "removed_part_native_mute_retirement";
                output["retirement_action"] = "mute";
                output["retained_solo"] = binding.observation.at("manifest").at("track").at("solo");
                output["solo_policy"] = "preserve";
                output["audible_residual"] =
                    "Mute-only retirement preserves Solo, which can suppress other Tracks";
                output["native_objects_deleted"] = false;
                output["historical_projection_retained"] = true;
                output["earlier_attempt_history_retained"] = true;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto effect_schema = revision_schema();
    effect_schema["additionalProperties"] = false;
    effect_schema["properties"]["effect_selections"] = effect_selection_schema();
    effect_schema["required"].push_back("effect_selections");
    register_native_tool(
        "project_realization_plan_effects",
        "Plan explicitly selected owning Timbre EQ and Mix EQ/Utility stages in native order, "
        "including separate input trim; retain residual intent and physical units",
        effect_schema,
        [session](const json& arguments) -> json {
            try {
                const auto owner = owning_part(session, arguments);
                const auto plan = part_effect_plan(session, owner, arguments);
                return {{"success", true},
                        {"project_revision", owner.revision},
                        {"plan", native_effect_plan_to_json(plan)},
                        {"mutation_dispatched", false},
                        {"complete_project_realization", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_author_effects",
        "Insert or revise the finite owning EQ/Utility chain after retained Drift; each mode "
        "and physical phase has its own durable fence and actual readback, stopping on uncertainty",
        std::move(effect_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                const auto plan = part_effect_plan(session, owner, arguments);
                auto store = history(session, false);
                const auto retained = binding_history(*store, owner.score_id, owner.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                if (!retained.acknowledged)
                    return decline("Create or adopt the current native Part first");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                auto binding = current_binding(*store, *retained.acknowledged);
                if (!projection_matches(desired.encoded, binding.observation))
                    return decline("Revise native Score notes and geometry before effect phases");
                static_cast<void>(
                    native_note_associations(desired.keys, desired.encoded, binding.observation));
                if (!binding.observation.contains("device_identity"))
                    return decline("Author or explicitly adopt the current Drift source first");
                const auto& cohort = binding.observation.at("device_identity").at("cohort");
                if (cohort.empty() ||
                    cohort[0].at("device_key") != "source_" + hexadecimal(plan.profile_id.value) ||
                    cohort[0].at("class_name") != "Drift" || cohort[0].at("role") != "source")
                    return decline("Author or explicitly adopt the owning Drift source first");
                if (cohort.size() > plan.entries.size() + 1)
                    return decline("Native chain has additional stages; select its complete "
                                   "current finite order");
                for (std::size_t index = 1; index < cohort.size(); ++index) {
                    const auto& entry = plan.entries.at(index - 1);
                    if (entry.desired_chain_index != index ||
                        cohort[index].at("device_key") != entry.device_key ||
                        cohort[index].at("class_name") !=
                            (entry.device == ManagedNativeDevice::Utility ? "StereoGain" : "Eq8"))
                        return decline("Native effect order conflicts with owning intent; "
                                       "append-only authoring cannot reorder existing stages");
                }
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                if (cohort.size() == plan.entries.size() + 1 &&
                    std::ranges::none_of(plan.entries, [](const auto& entry) {
                        return !entry.final_modes.empty();
                    })) {
                    auto selected = observed_device_selections(binding);
                    for (const auto& entry : plan.entries)
                        selected.at(entry.desired_chain_index) = effect_inspection_selection(entry);
                    if (const auto observed = inspect_current_device_intent(
                            *context, binding, selected, *transport)) {
                        auto output =
                            unchanged_device_result(desired,
                                                    native_effect_plan_to_json(plan),
                                                    *observed,
                                                    "selected_part_ordered_native_effect_knobs");
                        output["selected_effect_phases_completed"] = true;
                        output["phase_attempt_ids"] = json::array();
                        return output;
                    }
                }
                store = history(session, true);
                json attempts = json::array();
                json output;
                const auto run =
                    [&](const NativeEffectPlanEntry& entry,
                        std::string_view phase,
                        std::span<const ManagedDeviceModeIntent> modes,
                        std::span<const ManagedDevicePropertyIntent> properties,
                        std::span<const ManagedDevicePhysicalIntent> physical) -> bool {
                    auto token = store->new_attempt_id();
                    if (!token) {
                        output = decline("New native effect phase identity is unavailable");
                        return false;
                    }
                    Result<LomRequest> request = std::unexpected(ErrorCode::ProtocolError);
                    if (phase == "insert")
                        request = make_managed_device_insert_request(
                            *context, *token, binding, entry.device_key, entry.device, {});
                    else if (!modes.empty() || !properties.empty())
                        request = make_managed_device_mode_request(*context,
                                                                   *token,
                                                                   binding,
                                                                   entry.device_key,
                                                                   entry.device,
                                                                   modes,
                                                                   properties);
                    else
                        request = make_managed_device_update_request(
                            *context, *token, binding, entry.device_key, entry.device, physical);
                    if (!request) {
                        output =
                            decline(request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                        ? "ReplyCapacityUnavailable: effect phase evidence exceeds "
                                          "the finite bridge limit; no new fence was created"
                                        : "Current native effect phase admission is unavailable");
                        return false;
                    }
                    output = dispatch(store, desired, *context, *request, *transport);
                    output["effect_phase"] = phase;
                    output["device_key"] = entry.device_key;
                    if (output.contains("attempt_id") && output.at("attempt_id") == *token &&
                        store->find(*token))
                        attempts.push_back(*token);
                    if (output.at("success") != true) return false;
                    const auto* actual = store->find(*token);
                    if (!actual || actual->bindings.empty()) {
                        output["success"] = false;
                        output["error"] = "Native phase ACK did not retain its binding; later "
                                          "phases were stopped";
                        return false;
                    }
                    binding = current_binding(*store, *actual);
                    return true;
                };
                bool complete = true;
                for (const auto& entry : plan.entries) {
                    const auto current_size =
                        binding.observation.at("device_identity").at("cohort").size();
                    if (entry.desired_chain_index == current_size &&
                        !run(entry, "insert", {}, {}, {})) {
                        complete = false;
                        break;
                    }
                    if ((!entry.enable_modes.empty() &&
                         !run(entry, "enable", entry.enable_modes, {}, {})) ||
                        ((!entry.setup_modes.empty() || !entry.setup_properties.empty()) &&
                         !run(entry,
                              "setup_modes",
                              entry.setup_modes,
                              entry.setup_properties,
                              {})) ||
                        (!entry.setup_physical_intents.empty() &&
                         !run(entry, "setup_physical", {}, {}, entry.setup_physical_intents)) ||
                        (!entry.physical_intents.empty() &&
                         !run(entry, "authored_physical", {}, {}, entry.physical_intents)) ||
                        (!entry.final_modes.empty() &&
                         !run(entry, "authored_bypass", entry.final_modes, {}, {}))) {
                        complete = false;
                        break;
                    }
                }
                if (output.is_null())
                    output = {{"success", complete}, {"mutation_dispatched", false}};
                output["selected_effect_phases_completed"] = complete;
                output["phase_attempt_ids"] = std::move(attempts);
                output["scope"] = "selected_part_ordered_native_effect_knobs";
                output["plan"] = native_effect_plan_to_json(plan);
                output["native_knob_only"] = true;
                output["dsp_equivalence_qualified"] = false;
                output["host_qualified"] = false;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto replacement_preview_schema = revision_schema();
    replacement_preview_schema["additionalProperties"] = false;
    replacement_preview_schema["properties"]["lane_index"] = {{"type", "integer"}, {"minimum", 0}};
    replacement_preview_schema["required"].push_back("lane_index");
    auto replacement_schema = replacement_preview_schema;
    replacement_schema["properties"]["preview"] = {{"type", "object"}};
    for (const auto* approval :
         {"explicit_selected_envelope_replacement", "allow_unsampled_selected_state_overwrite"}) {
        replacement_schema["properties"][approval] = {{"type", "boolean"}, {"const", true}};
        replacement_schema["required"].push_back(approval);
    }
    replacement_schema["required"].push_back("preview");
    register_native_tool(
        "project_realization_preview_mix_lane_replacement",
        "Preview the selected owning Mix Step pan lane and actual native interior samples; "
        "samples do not reveal all breakpoints or grant write authority",
        std::move(replacement_preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto candidate = owning_lane(session, arguments, transport);
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                auto preview = preview_managed_envelope_replacement(
                    *context, candidate.binding, candidate.lane, *transport);
                if (!preview || !projection_matches(candidate.desired.encoded,
                                                    preview->evidence.at("observation")))
                    return decline("Current selected lane preview is unavailable or differs from "
                                   "owning Score intent");
                return {{"success", true},
                        {"preview", preview->evidence},
                        {"project_revision", candidate.desired.revision},
                        {"lane_index", arguments.at("lane_index")},
                        {"authority_granted", false},
                        {"mutation_dispatched", false},
                        {"scope", preview->evidence.at("scope")},
                        {"complete_project_realization", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_replace_mix_lane",
        "Replace the explicitly approved entire selected native pan envelope from owning Step "
        "intent, including unsampled state; preserve other lanes and report actual samples",
        std::move(replacement_schema),
        [session, transport](const json& arguments) -> json {
            try {
                for (const auto* approval : {"explicit_selected_envelope_replacement",
                                             "allow_unsampled_selected_state_overwrite"})
                    if (!arguments.at(approval).is_boolean() || arguments.at(approval) != true)
                        return decline("Explicit approval of the whole selected envelope and "
                                       "its unsampled state overwrite is required");
                auto candidate = owning_lane(session, arguments, transport);
                auto context = managed_bridge_context(*transport);
                auto preview =
                    managed_envelope_replacement_preview_from_json(arguments.at("preview"));
                if (!context || !preview || preview->project_key != candidate.binding.project_key ||
                    preview->binding_key != candidate.binding.binding_key ||
                    preview->evidence.at("lane") != candidate.lane ||
                    !projection_matches(candidate.desired.encoded,
                                        preview->evidence.at("observation")))
                    return decline("Approved lane preview differs from current owning intent");
                const auto& observed = preview->evidence.at("observation");
                for (const auto* name : {"content_fingerprint",
                                         "note_identity_fingerprint",
                                         "device_identity_fingerprint"})
                    if (observed.value(name, json(nullptr)) !=
                        candidate.binding.observation.value(name, json(nullptr)))
                        return decline(
                            "Retained Part/device state changed; preview the lane again");
                candidate.store = history(session, true);
                auto token = candidate.store->new_attempt_id();
                if (!token) return decline("New lane replacement identity is unavailable");
                auto request =
                    make_managed_envelope_replacement_request(*context, *token, *preview);
                if (!request)
                    return decline(request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                       ? "ReplyCapacityUnavailable: selected lane evidence exceeds "
                                         "the finite bridge limit; no dispatch fence was created"
                                       : "Approved current lane replacement is unavailable");
                auto output =
                    dispatch(candidate.store, candidate.desired, *context, *request, *transport);
                output["scope"] = preview->evidence.at("scope");
                output["lane_index"] = arguments.at("lane_index");
                output["complete_envelope_population_observed"] = false;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto song_preview_schema = revision_schema();
    song_preview_schema["additionalProperties"] = false;
    register_native_tool(
        "project_realization_preview_song_settings",
        "Preview owning constant quarter-note tempo and initial flat meter against the current "
        "Set and Part; disclose effects on all tracks and grant no authority",
        std::move(song_preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto candidate = owning_song_settings(session, arguments, transport);
                const auto& binding = current_binding(*candidate.store, *candidate.prior);
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                auto request = make_managed_song_settings_preview_request(
                    *context, binding, candidate.settings);
                if (!request)
                    return decline("Current Set settings preview is unavailable; explicitly "
                                   "adopt a reopened Part before previewing global changes");
                const auto response = transport->send(*request);
                if (!response.success || !response.value ||
                    !std::holds_alternative<json>(*response.value))
                    return decline(response.error.value_or("Current Set preview is unavailable"));
                const auto& raw = std::get<json>(*response.value);
                auto preview = parse_managed_song_settings_preview(*request, *context, raw);
                if (!preview ||
                    !projection_matches(candidate.desired.encoded, preview->observation))
                    return decline("Current Set preview contradicts the owning Score/Part");
                const auto* score = session.score->find(candidate.desired.score_id);
                return {{"success", true},
                        {"preview", raw},
                        {"project_revision", candidate.desired.revision},
                        {"desired_settings", managed_song_settings_to_json(candidate.settings)},
                        {"scope", "explicit_current_set_settings"},
                        {"all_tracks_affected", true},
                        {"authority_granted", false},
                        {"mutation_dispatched", false},
                        {"authored_beat_groups_unapplied",
                         has_distinct_meter_grouping(score->time_map.front().time_signature)},
                        {"complete_project_realization", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto song_apply_schema = revision_schema();
    song_apply_schema["additionalProperties"] = false;
    song_apply_schema["properties"]["preview"] = {{"type", "object"}};
    song_apply_schema["properties"]["explicit_set_wide_approval"] = {{"type", "boolean"},
                                                                     {"const", true}};
    song_apply_schema["required"].push_back("preview");
    song_apply_schema["required"].push_back("explicit_set_wide_approval");
    register_native_tool(
        "project_realization_apply_song_settings",
        "Apply the exact explicitly approved current Set preview from owning constant tempo and "
        "initial meter; preserve other observed Set properties and fence once-only scalar writes",
        std::move(song_apply_schema),
        [session, transport](const json& arguments) -> json {
            try {
                if (!arguments.at("explicit_set_wide_approval").is_boolean() ||
                    arguments.at("explicit_set_wide_approval") != true)
                    return decline("Explicit approval of the exact Set-wide preview is required");
                auto candidate = owning_song_settings(session, arguments, transport);
                const auto& binding = current_binding(*candidate.store, *candidate.prior);
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                auto expected = make_managed_song_settings_preview_request(
                    *context, binding, candidate.settings);
                if (!expected) return decline("Current Set settings context is unavailable");
                auto preview = parse_managed_song_settings_preview(
                    *expected, *context, arguments.at("preview"));
                if (!preview ||
                    !projection_matches(candidate.desired.encoded, preview->observation) ||
                    binding.observation.value("device_identity_fingerprint", json(nullptr)) !=
                        preview->approved_preview.at("binding_guard")
                            .at("device_identity_fingerprint"))
                    return decline("Approved Set preview differs from current owning intent or "
                                   "retained Part/device state; preview the current Set again");
                static_cast<void>(native_note_associations(
                    candidate.desired.keys, candidate.desired.encoded, preview->observation));
                candidate.store = history(session, true);
                auto token = candidate.store->new_attempt_id();
                if (!token) return decline("New Set settings operation identity is unavailable");
                auto request = make_managed_song_settings_request(*context, *token, *preview);
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: Set evidence exceeds the finite bridge "
                              "limit; no dispatch fence was created"
                            : "Approved Set settings intent is unavailable");
                auto output =
                    dispatch(candidate.store, candidate.desired, *context, *request, *transport);
                output["scope"] = "explicit_current_set_settings";
                output["all_tracks_affected"] = true;
                output["historical_song_identity_proven"] = false;
                output["earlier_attempt_history_retained"] = true;
                const auto* score = session.score->find(candidate.desired.score_id);
                output["authored_beat_groups_unapplied"] =
                    has_distinct_meter_grouping(score->time_map.front().time_signature);
                output["unapplied_domains"] = {"Arrangement tempo/meter populations",
                                               "other Parts",
                                               "Timbre devices and parameters",
                                               "Mix routing and automation"};
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto geometry_schema = revision_schema();
    geometry_schema["additionalProperties"] = false;
    register_native_tool(
        "project_realization_update_geometry",
        "Revise only the owning Clip length and flat meter while retaining unchanged attack "
        "keys, native IDs and values; revise notes separately before shrinking or after extending",
        std::move(geometry_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained = binding_history(*store, owner.score_id, owner.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                if (!retained.acknowledged)
                    return decline("This Part has no current native binding");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                const auto* prior = retained.acknowledged;
                const auto& binding = current_binding(*store, *prior);
                if (!projection_matches(prior->intent.desired_projection, binding.observation) ||
                    desired.keys != prior->intent.desired_note_keys ||
                    desired.encoded.at("notes") != prior->intent.desired_projection.at("notes"))
                    return decline("GeometryOnlyRevisionRequired: keep attack keys and values "
                                   "unchanged; revise note content in a separate authored phase");
                static_cast<void>(native_note_associations(prior->intent.desired_note_keys,
                                                           prior->intent.desired_projection,
                                                           binding.observation));
                if (projection_matches(desired.encoded, binding.observation)) {
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
                    json output = {{"success", unchanged},
                                   {"state",
                                    unchanged ? "unchanged_desired_geometry"
                                              : "native_drift_or_observation_unavailable"},
                                   {"mutation_dispatched", false},
                                   {"current_native_state_observed", actual.has_value()},
                                   {"complete_project_realization", false}};
                    if (observed) output["native_observation"] = observed->evidence;
                    return output;
                }
                auto context = managed_bridge_context(*transport);
                store = history(session, true);
                auto token = store->new_attempt_id();
                if (!context || !token)
                    return decline("Native context or operation identity unavailable");
                auto request =
                    make_managed_clip_geometry_request(*context,
                                                       *token,
                                                       binding,
                                                       desired.projection.clip_end,
                                                       desired.projection.signature_numerator,
                                                       desired.projection.signature_denominator);
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: Clip geometry evidence exceeds the "
                              "finite bridge limit; no dispatch fence was created"
                            : "Native Clip geometry intent is unsupported; all retained note "
                              "tails must fit within the desired end");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "selected_part_clip_extent_and_flat_meter";
                output["complete_envelope_population_observed"] = false;
                output["linked_envelope_playback_extent_may_follow_clip"] = true;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto selections_schema = revision_schema();
    selections_schema["additionalProperties"] = false;
    selections_schema["properties"]["selections"] = {
        {"type", "array"},
        {"maxItems", NATIVE_TIMBRE_MAX_SELECTIONS},
        {"items",
         {{"type", "object"},
          {"additionalProperties", false},
          {"properties",
           {{"source_path", {{"type", "string"}}},
            {"capability_id", {{"type", "string"}}},
            {"tolerance", {{"type", "number"}, {"minimum", 0}}}}},
          {"required", {"source_path", "capability_id", "tolerance"}}}}};
    selections_schema["required"].push_back("selections");
    const auto source_selection_schema = selections_schema.at("properties").at("selections");
    auto author_timbre_schema = selections_schema;
    auto device_preview_schema = selections_schema;
    auto device_adopt_schema = selections_schema;
    device_preview_schema["properties"]["effect_selections"] = effect_selection_schema();
    device_adopt_schema["properties"]["effect_selections"] = effect_selection_schema();
    for (auto* schema : {&device_preview_schema, &device_adopt_schema}) {
        (*schema)["properties"]["device_projection_source"] = {
            {"type", "string"},
            {"enum", {"current_owning_timbre", "retained_verified_realization"}}};
        (*schema)["properties"]["device_history_attempt"] = {{"type", "string"}};
    }
    register_native_tool(
        "project_realization_plan_timbre",
        "Plan explicitly selected Drift physical controls from owning Timbre content without "
        "native writes; retain original bindings and all residual fields",
        std::move(selections_schema),
        [session](const json& arguments) -> json {
            try {
                const auto owner = owning_part(session, arguments);
                const auto plan = part_timbre_plan(session, owner, arguments);
                return {{"success", true},
                        {"score_id", owner.score_id},
                        {"part_id", owner.part_id},
                        {"project_revision", owner.revision},
                        {"plan", native_timbre_plan_to_json(plan)},
                        {"mutation_dispatched", false},
                        {"native_observation_available", false},
                        {"complete_project_realization", false}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_author_timbre",
        "Insert or revise the owning Drift source's explicitly selected physical controls; "
        "resolve all actual native targets before setters and retain residual authored fields",
        std::move(author_timbre_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained = binding_history(*store, owner.score_id, owner.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                const auto plan = part_timbre_plan(session, owner, arguments);
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior) return decline("This Part has no current native binding");
                const auto& binding = current_binding(*store, *prior);
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise native Score notes before authoring Timbre controls");
                const auto key = source_device_key(plan);
                bool source_retained = false;
                if (binding.observation.contains("device_identity")) {
                    const auto& cohort = binding.observation.at("device_identity").at("cohort");
                    for (std::size_t index = 0; index < cohort.size(); ++index) {
                        const auto& device = cohort.at(index);
                        if (device.at("device_key") != key) continue;
                        if (index != 0 || device.at("class_name") != "Drift" ||
                            device.at("role") != "source")
                            return decline(
                                "The owning source key identifies a different native device");
                        source_retained = true;
                    }
                }
                if (!source_retained &&
                    binding.observation.at("manifest").at("devices_empty") != true)
                    return decline(
                        "Explicitly adopt the current native device chain before revising "
                        "its source; preserved devices are not insertion authority");
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Native context unavailable");
                const auto intents = device_intents(plan);
                if (source_retained) {
                    auto selected = observed_device_selections(binding);
                    selected.at(0).physical_intents = intents;
                    if (const auto observed = inspect_current_device_intent(
                            *context, binding, selected, *transport)) {
                        auto output =
                            unchanged_device_result(desired,
                                                    native_timbre_plan_to_json(plan),
                                                    *observed,
                                                    "selected_part_drift_physical_controls");
                        output["device_key"] = key;
                        output["source_insertion_requested"] = false;
                        return output;
                    }
                }
                store = history(session, true);
                auto token = store->new_attempt_id();
                if (!token) return decline("Native operation identity unavailable");
                auto request =
                    source_retained
                        ? make_managed_device_update_request(
                              *context, *token, binding, key, ManagedNativeDevice::Drift, intents)
                        : make_managed_device_insert_request(
                              *context, *token, binding, key, ManagedNativeDevice::Drift, intents);
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: native device evidence exceeds the "
                              "finite bridge limit; no dispatch fence was created"
                            : "Native owning source intent is unsupported for this binding");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "selected_part_drift_physical_controls";
                output["plan"] = native_timbre_plan_to_json(plan);
                output["device_key"] = key;
                output["source_insertion_requested"] = !source_retained;
                output["native_knob_only"] = true;
                output["dsp_equivalence_qualified"] = false;
                output["host_qualified"] = false;
                output["complete_project_realization"] = false;
                output["unapplied_domains"] = {"unselected Timbre fields and effects",
                                               "global Song settings",
                                               "other Parts",
                                               "Mix routing and automation"};
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_preview_device_adoption",
        "Preview explicit current Drift and selected finite effect ownership against owning "
        "physical intent; require whole current chain agreement and grant no authority",
        std::move(device_preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                auto store = history(session, false);
                const auto retained = binding_history(*store, owner.score_id, owner.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                const auto plan = part_timbre_plan(session, owner, arguments);
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior)
                    return decline("Explicitly establish the current native Clip binding first");
                const auto& binding = current_binding(*store, *prior);
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise native Score notes before previewing device ownership");
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                const auto selected = selected_device_adoption(
                    session, desired, arguments, plan, binding, *store, *prior, false);
                const auto& selections = selected.selections;
                auto request = make_managed_device_preview_request(*context, binding, selections);
                if (!request) return decline("Current source device preview intent is unsupported");
                const auto response = transport->send(*request);
                if (!response.success || !response.value ||
                    !std::holds_alternative<json>(*response.value))
                    return decline(
                        response.error.value_or("Current native device preview is unavailable"));
                const auto& raw = std::get<json>(*response.value);
                auto preview = parse_managed_device_preview(*request, *context, raw);
                if (!preview ||
                    !projection_matches(desired.encoded,
                                        preview->approved_preview.at("binding_observation")))
                    return decline(
                        "Current device preview contradicts owning Score or Timbre intent");
                json output = {{"success", true},
                               {"preview", raw},
                               {"plan", native_timbre_plan_to_json(plan)},
                               {"project_revision", desired.revision},
                               {"authority_granted", false},
                               {"mutation_dispatched", false},
                               {"historical_native_identity_restored", false},
                               {"scope", "empty_or_whole_current_finite_device_preview"}};
                device_adoption_provenance(output, selected);
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    device_adopt_schema["properties"]["preview"] = {{"type", "object"}};
    device_adopt_schema["properties"]["explicit_adoption"] = {{"type", "boolean"}, {"const", true}};
    device_adopt_schema["required"].push_back("preview");
    device_adopt_schema["required"].push_back("explicit_adoption");
    register_native_tool(
        "project_realization_adopt_devices",
        "Explicitly adopt the exact approved current finite device chain after matching owning "
        "Score, Timbre and selected effects; grant fresh current-object authority",
        std::move(device_adopt_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                if (!arguments.at("explicit_adoption").is_boolean() ||
                    arguments.at("explicit_adoption") != true)
                    return decline(
                        "Explicit approval of the exact current device preview is required");
                auto store = history(session, false);
                const auto retained = binding_history(*store, owner.score_id, owner.part_id);
                if (retained.unresolved) {
                    auto output = attempt_summary(*retained.unresolved);
                    output["success"] = false;
                    output["state"] = "reconciliation_required";
                    output["mutation_dispatched"] = false;
                    return output;
                }
                const auto plan = part_timbre_plan(session, owner, arguments);
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior)
                    return decline("Explicitly establish the current native Clip binding first");
                const auto& binding = current_binding(*store, *prior);
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise native Score notes before adopting device ownership");
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                const auto selected = selected_device_adoption(
                    session, desired, arguments, plan, binding, *store, *prior, true);
                const auto& selections = selected.selections;
                auto preview_request =
                    make_managed_device_preview_request(*context, binding, selections);
                if (!preview_request)
                    return decline("Current source device preview intent is unsupported");
                auto preview = parse_managed_device_preview(
                    *preview_request, *context, arguments.at("preview"));
                if (!preview ||
                    !projection_matches(desired.encoded,
                                        preview->approved_preview.at("binding_observation")))
                    return decline("Approved device preview differs from current owning Score or "
                                   "Timbre intent");
                store = history(session, true);
                auto token = store->new_attempt_id();
                if (!token) return decline("New device adoption identity is unavailable");
                auto request = make_managed_device_adoption_request(*context, *token, *preview);
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: device adoption evidence exceeds "
                              "the finite bridge limit; no dispatch fence was created"
                            : "Approved current native device adoption is unsupported");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "explicit_empty_or_whole_current_finite_device_ownership";
                output["plan"] = native_timbre_plan_to_json(plan);
                output["historical_native_identity_restored"] = false;
                output["native_knob_only"] = true;
                output["dsp_equivalence_qualified"] = false;
                output["complete_project_realization"] = false;
                device_adoption_provenance(output, selected);
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto preview_schema = revision_schema();
    preview_schema["additionalProperties"] = false;
    preview_schema["properties"]["projection_source"] = {
        {"type", "string"}, {"enum", {"current_owning_score", "retained_verified_realization"}}};
    preview_schema["properties"]["selector"] = {
        {"oneOf",
         {{{"type", "object"},
           {"additionalProperties", false},
           {"properties",
            {{"track_index", {{"type", "integer"}, {"minimum", 0}}},
             {"slot_index", {{"type", "integer"}, {"minimum", 0}}}}},
           {"required", {"track_index", "slot_index"}}},
          {{"type", "object"},
           {"additionalProperties", false},
           {"properties",
            {{"track_tag", {{"type", "string"}}}, {"clip_tag", {{"type", "string"}}}}},
           {"required", {"track_tag", "clip_tag"}}}}}};
    preview_schema["required"].push_back("selector");
    register_native_tool(
        "project_realization_preview_adoption",
        "Inspect a selected current native Part for explicit ownership recovery; preview grants "
        "no authority and never refreshes mutation guards",
        std::move(preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = recovery_part(session, arguments, *transport);
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                auto preview = preview_managed_adoption(*context,
                                                        project_key(session, desired.score_id),
                                                        binding_key(desired.part_id),
                                                        arguments.at("selector"),
                                                        *transport);
                if (!preview) return decline("Current native adoption preview is unavailable");
                const auto& observation = preview->evidence.at("observation");
                const bool matches = projection_matches(desired.encoded, observation);
                bool associations_available = false;
                std::optional<std::string> association_error;
                if (matches) {
                    try {
                        static_cast<void>(
                            native_note_associations(desired.keys, desired.encoded, observation));
                        associations_available = true;
                    } catch (const std::exception& failure) {
                        association_error = failure.what();
                    }
                }
                json output = {
                    {"success", true},
                    {"preview", preview->evidence},
                    {"score_id", desired.score_id},
                    {"part_id", desired.part_id},
                    {"project_revision", desired.revision},
                    {"current_score_projection_matches",
                     desired.historical_projection_attempt ? json(nullptr) : json(matches)},
                    {"attack_associations_available", associations_available},
                    {"eligible_for_explicit_adoption", matches && associations_available},
                    {"authority_granted", false},
                    {"mutation_dispatched", false},
                    {"historical_native_identity_restored", false}};
                recovery_provenance(output, session, desired);
                if (desired.historical_projection_attempt)
                    output["historical_projection_matches"] = matches;
                if (association_error) output["association_error"] = *association_error;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto adopt_schema = revision_schema();
    adopt_schema["additionalProperties"] = false;
    adopt_schema["properties"]["projection_source"] = {
        {"type", "string"}, {"enum", {"current_owning_score", "retained_verified_realization"}}};
    adopt_schema["properties"]["historical_projection_attempt"] = {{"type", "string"},
                                                                   {"minLength", 1}};
    adopt_schema["properties"]["preview"] = {{"type", "object"}};
    adopt_schema["properties"]["explicit_adoption"] = {{"type", "boolean"}, {"const", true}};
    adopt_schema["required"].push_back("preview");
    adopt_schema["required"].push_back("explicit_adoption");
    register_native_tool(
        "project_realization_adopt",
        "Explicitly adopt the exact current Part preview matching current Score or an explicitly "
        "selected verified historical musical baseline; retain uncertainty and grant current "
        "authority",
        std::move(adopt_schema),
        [session, transport](const json& arguments) -> json {
            try {
                if (!arguments.at("explicit_adoption").is_boolean() ||
                    arguments.at("explicit_adoption") != true)
                    return decline("Explicit approval of the exact current preview is required");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = recovery_part(session, arguments, *transport);
                if (arguments.value("projection_source", std::string{"current_owning_score"}) ==
                    "retained_verified_realization") {
                    if (!desired.historical_projection_attempt ||
                        !arguments.contains("historical_projection_attempt") ||
                        arguments.at("historical_projection_attempt") !=
                            *desired.historical_projection_attempt)
                        return decline("Approve the exact retained historical projection attempt "
                                       "returned by the current recovery preview");
                } else if (arguments.contains("historical_projection_attempt")) {
                    return decline("Historical projection identity requires explicit historical "
                                   "musical source selection");
                }

                auto preview = managed_adoption_preview_from_json(arguments.at("preview"));
                auto context = managed_bridge_context(*transport);
                if (!preview || !context ||
                    preview->context.bridge_instance != context->bridge_instance ||
                    preview->context.document_token != context->document_token ||
                    preview->project_key != project_key(session, desired.score_id) ||
                    preview->binding_key != binding_key(desired.part_id))
                    return decline("Approved preview does not belong to this current Part context");
                if (!projection_matches(desired.encoded, preview->evidence.at("observation")))
                    return decline("Approved native Clip differs from authored Score or the "
                                   "selected verified historical musical projection");
                static_cast<void>(native_note_associations(
                    desired.keys, desired.encoded, preview->evidence.at("observation")));
                auto store = history(session, true);
                auto token = store->new_attempt_id();
                if (!token) return decline("New adoption operation identity is unavailable");
                auto request = make_managed_adoption_request(*context, *token, *preview);
                if (!request)
                    return decline(request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                       ? "ReplyCapacityUnavailable: adoption evidence exceeds the "
                                         "finite bridge limit; no dispatch fence was created"
                                       : "Approved current native adoption intent is unsupported");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "explicit_current_part_ownership";
                recovery_provenance(output, session, desired);
                output["historical_native_identity_restored"] = false;
                output["earlier_attempt_history_retained"] = true;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
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
                if (!transport || transport->records_without_execution()) {
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
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: native clip evidence exceeds the "
                              "finite bridge limit; no dispatch fence was created"
                            : "Native clip intent rejected");
                return dispatch(store, desired, *context, *request, *transport);
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_update",
        "Revise native attacks, existing note IDs, Clip extent and flat meter in durable stages "
        "under one unchanged final owning project revision",
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
                if (!transport || transport->records_without_execution())
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
                auto binding = current_binding(*store, *prior);
                const auto& prior_method = last_receipt(*prior).request.property_or_method;
                if (prior_method == "sunny_managed_update_clip_geometry" ||
                    prior_method == "sunny_managed_update_notes" ||
                    prior_method == "sunny_managed_revise_note_population") {
                    auto previous_desired = desired;
                    previous_desired.encoded = prior->intent.desired_projection;
                    previous_desired.keys = prior->intent.desired_note_keys;
                    previous_desired.projection = retained_projection(*prior);
                    if (!verified_outcome(last_receipt(*prior), previous_desired))
                        return decline("Inspect the latest native revision mismatch before "
                                       "continuing; an ACK alone does not verify its preservation "
                                       "or intermediate projection");
                }
                if (!projection_matches(prior->intent.desired_projection, binding.observation))
                    return decline("Retained native notes or geometry differ from verified intent; "
                                   "inspect or explicitly adopt the current objects first");
                static_cast<void>(native_note_associations(prior->intent.desired_note_keys,
                                                           prior->intent.desired_projection,
                                                           binding.observation));
                const auto stages = plan_managed_clip_revision(retained_projection(*prior),
                                                               prior->intent.desired_note_keys,
                                                               desired.projection,
                                                               desired.keys);
                if (!stages)
                    return decline(
                        stages.error() == ErrorCode::TargetValueUnrepresentable
                            ? "UnsupportedChordCardinalityRevision or retained probability/"
                              "velocity deviation change: preserve Event ordinal associations"
                            : "Native note/geometry revision is malformed or a note tail "
                              "exceeds its owning Clip extent");
                if (stages->empty()) {
                    auto output = attempt_summary(*prior);
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
                    output["phase_attempt_ids"] = json::array();
                    output["selected_clip_phases_completed"] = unchanged;
                    if (observed) output["native_observation"] = observed->evidence;
                    return output;
                }
                // Every stage persists its complete intermediate projection under
                // the unchanged final owning revision. Replanning after original-
                // token reconciliation resumes from that verified projection.
                std::string prior_id = prior->intent.attempt_id;
                json attempts = json::array();
                json output;
                std::size_t completed = 0;
                for (const auto& stage : *stages) {
                    try {
                        auto staged = stage_part(desired, stage);
                        prior = store->find(prior_id);
                        if (!prior || prior->bindings.empty())
                            throw std::runtime_error(
                                "Verified native revision stage history is unavailable");
                        binding = current_binding(*store, *prior);
                        auto context = managed_bridge_context(*transport);
                        auto token = store->new_attempt_id();
                        if (!context || !token) {
                            output = decline("Native context or operation identity unavailable");
                            break;
                        }
                        Result<LomRequest> request = std::unexpected(ErrorCode::ProtocolError);
                        if (stage.phase == ManagedClipRevisionPhase::UpdateNotePopulation) {
                            const auto changes = note_population_changes(*prior, staged, binding);
                            request = changes.deletions.empty() && changes.additions.empty()
                                          ? make_managed_note_update_request(
                                                *context, *token, binding, changes.changes)
                                          : make_managed_note_population_request(*context,
                                                                                 *token,
                                                                                 binding,
                                                                                 changes.changes,
                                                                                 changes.deletions,
                                                                                 changes.additions);
                        } else {
                            request = make_managed_clip_geometry_request(
                                *context,
                                *token,
                                binding,
                                staged.projection.clip_end,
                                staged.projection.signature_numerator,
                                staged.projection.signature_denominator);
                        }
                        if (!request) {
                            output = decline(
                                request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                    ? "ReplyCapacityUnavailable: complete stage evidence exceeds "
                                      "the finite bridge limit; this stage was not fenced"
                                    : "Native revision stage is unsupported for this binding");
                            break;
                        }
                        output = dispatch(store, staged, *context, *request, *transport);
                        if (output.value("attempt_id", std::string{}) == *token &&
                            store->find(*token))
                            attempts.push_back(*token);
                        if (!output.value("success", false)) break;
                        ++completed;
                        prior_id = *token;
                    } catch (const std::exception& failure) {
                        output = workflow_failure(failure);
                        break;
                    }
                }
                output["phase_attempt_ids"] = std::move(attempts);
                output["selected_clip_phases_completed"] = completed == stages->size();
                output["mutation_dispatched"] = !output.at("phase_attempt_ids").empty();
                output["scope"] = "selected_part_notes_extent_and_flat_meter";
                output["complete_envelope_population_observed"] = false;
                output["linked_envelope_playback_extent_may_follow_clip"] = true;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto lane_schema = revision_schema();
    lane_schema["properties"]["lane_index"] = {{"type", "integer"}, {"minimum", 0}};
    lane_schema["required"].push_back("lane_index");
    register_native_tool(
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
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                store = history(session, true);
                auto desired =
                    desired_part(session, arguments, std::move(owning_input), *transport);
                const auto* prior = retained.acknowledged;
                if (!prior || prior->bindings.empty())
                    return decline("Create and inspect this Part's native clip first");
                const auto binding = current_binding(*store, *prior);
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise the native notes before authoring the current Mix lane");
                const auto lane = owning_step_pan_lane(session, desired, binding, arguments);
                if (lane.at("points").size() > SUNNY_MANAGED_ENVELOPE_MAX_STEPS)
                    return decline("EnvelopeAuthoringUnavailable: selected lane exceeds the "
                                   "64 native Step-call budget; no dispatch fence was created");
                const auto& selector = lane.at("parameter");
                const auto& points = lane.at("points");
                const auto lane_index = arguments.at("lane_index").get<std::uint64_t>();
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
                            "The existing native lane is retained; complete selected-envelope "
                            "replacement requires project_realization_preview_mix_lane_replacement "
                            "and explicit unsampled-state overwrite approval";
                        return output;
                    }
                }
                auto context = managed_bridge_context(*transport);
                auto token = store->new_attempt_id();
                if (!context || !token) return decline("Native context or token unavailable");
                auto request = make_managed_envelope_request(*context, *token, binding, lane);
                if (!request)
                    return decline(request.error() == ErrorCode::ManagedReplyCapacityExceeded
                                       ? "ReplyCapacityUnavailable: initial lane evidence exceeds "
                                         "the finite bridge limit; no dispatch fence was created"
                                       : "Native lane admission is unavailable for this binding");
                auto output = dispatch(store, desired, *context, *request, *transport);
                output["scope"] = "selected_part_mix_panning_step_lane";
                output["unapplied_domains"] = {"global Song settings",
                                               "other Parts",
                                               "Timbre devices and parameters",
                                               "Mix routing, static mixer values and other lanes"};
                output["complete_project_realization"] = false;
                output["lane_index"] = lane_index;
                output["lane_target"] =
                    "channels[" + std::to_string(desired.part_id) + "].spatial.pan";
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
                return workflow_failure(failure);
            }
        });
    register_native_tool(
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
                if (!transport->ensure_connected()) {
                    output["native_observation_error"] = "Current bridge connection unavailable";
                    return output;
                }
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
                return workflow_failure(failure);
            }
        });
    register_native_tool(
        "project_realization_reconcile",
        "Query an uncertain fenced attempt and retain evidence without any mutation retry",
        {{"type", "object"},
         {"properties", {{"attempt_id", {{"type", "string"}}}}},
         {"required", {"attempt_id"}}},
        [session, transport](const json& arguments) -> json {
            try {
                auto store = history(session, false);
                const auto token = arguments.at("attempt_id").get<std::string>();
                const auto* attempt = store->find(token);
                if (!attempt) return decline("No retained native attempt exists");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                if (!transport->ensure_connected()) {
                    auto output = attempt_summary(*attempt);
                    output["success"] = false;
                    output["query_succeeded"] = false;
                    output["history_saved"] = false;
                    output["mutation_retried"] = false;
                    output["error"] = "Current bridge connection unavailable";
                    return output;
                }
                std::optional<ManagedOperationReceipt> query_evidence;
                auto observed = reconcile_managed_operation(
                    last_receipt(*attempt), *transport, &query_evidence);
                if (!query_evidence) {
                    auto output = attempt_summary(*attempt);
                    output["success"] = false;
                    output["query_succeeded"] = false;
                    output["history_saved"] = false;
                    output["mutation_retried"] = false;
                    output["error"] = observed
                                          ? observed->error.value_or("Native query unavailable")
                                          : "Reconciliation response rejected; retained history "
                                            "remains unchanged";
                    return output;
                }
                if (!observed)
                    return decline(
                        "Reconciliation response rejected; retained history remains unchanged");
                std::optional<ManagedBindingReceipt> binding;
                auto actual = managed_binding_receipt(*observed);
                if (actual) binding = std::move(*actual);
                auto saved = store->append_evidence(token, *observed, binding);
                auto output = attempt_summary(*store->find(token));
                output["success"] = saved.has_value();
                output["query_succeeded"] = true;
                output["actual_receipt"] = managed_receipt_to_json(*query_evidence);
                output["history_saved"] = saved.has_value();
                output["mutation_retried"] = false;
                if (!saved) output["error"] = saved.error().message;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    register_project_realization_coordinator_tools(
        server,
        native_tools,
        [session, transport](const json& input) -> json {
            try {
                return prepare_project_realization(session, input, transport);
            } catch (const std::exception& e) {
                return workflow_failure(e);
            }
        },
        [session, transport](const json& input) -> json {
            try {
                return verify_project_realization(session, input, transport);
            } catch (const std::exception& e) {
                return workflow_failure(e);
            }
        },
        source_selection_schema,
        effect_selection_schema(),
        static_mixer_schema(),
        routing_schema());
}

} // namespace sunny::infrastructure
