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
#include <sunny/infrastructure/ableton/detail/realization_history.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>
#include <sunny/infrastructure/ableton/managed_envelope_revision.hpp>
#include <sunny/infrastructure/ableton/managed_recovery.hpp>
#include <sunny/infrastructure/ableton/managed_song_settings.hpp>
#include <sunny/infrastructure/ableton/native_effect_plan.hpp>
#include <sunny/infrastructure/ableton/native_timbre_plan.hpp>
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
    return realization_detail::last_receipt(attempt);
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
    return false;
}

const RealizationStoredAttempt* global_settings_blocker(const RealizationStore& store,
                                                        const ManagedBridgeContext& context) {
    return realization_detail::set_wide_settings_blocker(store, &context);
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
    // Explicit current authority can be recovered without claiming an earlier
    // global operation absent. Clip/device adoption performs no native setters;
    // fresh Set-wide approval is the only operation that can renew this boundary.
    if (request.property_or_method != "sunny_managed_apply_song_settings" &&
        request.property_or_method != "sunny_managed_adopt_clip" &&
        request.property_or_method != "sunny_managed_adopt_devices") {
        if (const auto* blocked = global_settings_blocker(*store, context)) {
            auto output = attempt_summary(*blocked);
            output["success"] = false;
            output["state"] = "set_wide_reconciliation_required";
            output["error"] = "Reconcile the original Set settings token, or explicitly approve "
                              "a fresh current Set-wide preview before native setters";
            output["mutation_dispatched"] = false;
            return output;
        }
    }
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

json owning_step_pan_lane(const McpSession& session,
                          const DesiredPart& desired,
                          const ManagedBindingReceipt& binding,
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
    const auto& mixer = binding.observation.at("manifest").at("mixer");
    const auto& parameter = mixer.at("panning");
    if (mixer.at("panning_mode") != 0 || parameter.at("min") != -1.0 || parameter.at("max") != 1.0)
        throw std::runtime_error("Native stereo panning requires the observed [-1,+1] domain");
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

struct OwningSongSettings {
    DesiredPart desired;
    std::shared_ptr<RealizationStore> store;
    const RealizationStoredAttempt* prior;
    ManagedSongSettings settings;
};

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
    auto binding = retained.acknowledged->bindings.back();
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
    const auto& binding = retained.acknowledged->bindings.back();
    if (!note_changes(*retained.acknowledged, desired, binding).empty())
        throw std::runtime_error("Revise native Score notes before Set settings");
    static_cast<void>(native_note_associations(desired.keys, desired.encoded, binding.observation));
    return {std::move(desired), std::move(store), retained.acknowledged, *settings};
}

} // namespace

void register_project_realization_tools(McpServer& server,
                                        const McpSession& session,
                                        LomTransport* transport) {
    auto domain = server.registration_scope(McpDocumentDomain::None);
    auto effect_schema = revision_schema();
    effect_schema["additionalProperties"] = false;
    effect_schema["properties"]["effect_selections"] = effect_selection_schema();
    effect_schema["required"].push_back("effect_selections");
    server.register_tool(
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
    server.register_tool(
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
                auto binding = retained.acknowledged->bindings.back();
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
                    binding = actual->bindings.back();
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
    server.register_tool(
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
    server.register_tool(
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
    server.register_tool(
        "project_realization_preview_song_settings",
        "Preview owning constant quarter-note tempo and initial flat meter against the current "
        "Set and Part; disclose effects on all tracks and grant no authority",
        std::move(song_preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto candidate = owning_song_settings(session, arguments, transport);
                const auto& binding = candidate.prior->bindings.back();
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
    server.register_tool(
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
                const auto& binding = candidate.prior->bindings.back();
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
    server.register_tool(
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
                const auto& binding = prior->bindings.back();
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
    auto author_timbre_schema = selections_schema;
    auto device_preview_schema = selections_schema;
    auto device_adopt_schema = selections_schema;
    device_preview_schema["properties"]["effect_selections"] = effect_selection_schema();
    device_adopt_schema["properties"]["effect_selections"] = effect_selection_schema();
    server.register_tool(
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
    server.register_tool(
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
                const auto& binding = prior->bindings.back();
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
                store = history(session, true);
                auto token = store->new_attempt_id();
                if (!context || !token)
                    return decline("Native context or operation identity unavailable");
                const auto intents = device_intents(plan);
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
    server.register_tool(
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
                const auto& binding = prior->bindings.back();
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise native Score notes before previewing device ownership");
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                const auto selections =
                    chain_adoption_selections(session, desired, arguments, plan, binding);
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
                return {{"success", true},
                        {"preview", raw},
                        {"plan", native_timbre_plan_to_json(plan)},
                        {"project_revision", desired.revision},
                        {"authority_granted", false},
                        {"mutation_dispatched", false},
                        {"historical_native_identity_restored", false},
                        {"scope", "empty_or_whole_current_finite_device_preview"}};
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    device_adopt_schema["properties"]["preview"] = {{"type", "object"}};
    device_adopt_schema["properties"]["explicit_adoption"] = {{"type", "boolean"}, {"const", true}};
    device_adopt_schema["required"].push_back("preview");
    device_adopt_schema["required"].push_back("explicit_adoption");
    server.register_tool(
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
                const auto& binding = prior->bindings.back();
                if (!note_changes(*prior, desired, binding).empty())
                    return decline("Revise native Score notes before adopting device ownership");
                auto context = managed_bridge_context(*transport);
                if (!context) return decline("Current native context is unavailable");
                const auto selections =
                    chain_adoption_selections(session, desired, arguments, plan, binding);
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
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto preview_schema = revision_schema();
    preview_schema["additionalProperties"] = false;
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
    server.register_tool(
        "project_realization_preview_adoption",
        "Inspect a selected current native Part for explicit ownership recovery; preview grants "
        "no authority and never refreshes mutation guards",
        std::move(preview_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
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
                    {"current_score_projection_matches", matches},
                    {"attack_associations_available", associations_available},
                    {"eligible_for_explicit_adoption", matches && associations_available},
                    {"authority_granted", false},
                    {"mutation_dispatched", false},
                    {"historical_native_identity_restored", false}};
                if (association_error) output["association_error"] = *association_error;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
    auto adopt_schema = revision_schema();
    adopt_schema["additionalProperties"] = false;
    adopt_schema["properties"]["preview"] = {{"type", "object"}};
    adopt_schema["properties"]["explicit_adoption"] = {{"type", "boolean"}, {"const", true}};
    adopt_schema["required"].push_back("preview");
    adopt_schema["required"].push_back("explicit_adoption");
    server.register_tool(
        "project_realization_adopt",
        "Explicitly adopt the exact approved current Part preview after matching current Score "
        "content; retain all earlier uncertainty and establish fresh current-object authority",
        std::move(adopt_schema),
        [session, transport](const json& arguments) -> json {
            try {
                auto owner = owning_part(session, arguments);
                if (!arguments.at("explicit_adoption").is_boolean() ||
                    arguments.at("explicit_adoption") != true)
                    return decline("Explicit approval of the exact current preview is required");
                if (!transport || transport->records_without_execution())
                    return decline("An actual bridge connection is required");
                auto desired = desired_part(session, arguments, std::move(owner), *transport);
                auto preview = managed_adoption_preview_from_json(arguments.at("preview"));
                auto context = managed_bridge_context(*transport);
                if (!preview || !context ||
                    preview->context.bridge_instance != context->bridge_instance ||
                    preview->context.document_token != context->document_token ||
                    preview->project_key != project_key(session, desired.score_id) ||
                    preview->binding_key != binding_key(desired.part_id))
                    return decline("Approved preview does not belong to this current Part context");
                if (!projection_matches(desired.encoded, preview->evidence.at("observation")))
                    return decline("Revise the authored Score to match the approved current native "
                                   "clip before explicit adoption");
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
                output["historical_native_identity_restored"] = false;
                output["earlier_attempt_history_retained"] = true;
                output["complete_project_realization"] = false;
                return output;
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
            }
        });
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
    server.register_tool(
        "project_realization_update",
        "Revise native attacks and existing note IDs from current owning project content",
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
                auto changes = note_population_changes(*prior, desired, prior->bindings.back());
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
                auto request = changes.deletions.empty() && changes.additions.empty()
                                   ? make_managed_note_update_request(
                                         *context, *token, prior->bindings.back(), changes.changes)
                                   : make_managed_note_population_request(*context,
                                                                          *token,
                                                                          prior->bindings.back(),
                                                                          changes.changes,
                                                                          changes.deletions,
                                                                          changes.additions);
                if (!request)
                    return decline(
                        request.error() == ErrorCode::ManagedReplyCapacityExceeded
                            ? "ReplyCapacityUnavailable: native note evidence exceeds the finite "
                              "bridge reply limit; no dispatch fence was created"
                            : "Native note revision is unsupported for this binding");
                return dispatch(store, desired, *context, *request, *transport);
            } catch (const std::exception& failure) {
                return workflow_failure(failure);
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
                if (!transport || transport->records_without_execution())
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
    server.register_tool(
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
}

} // namespace sunny::infrastructure
