/** Native realization state shared by MCP authoring and recovery endpoints. */
#include "realization_state.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <sstream>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/infrastructure/ableton/detail/realization_history.hpp>
#include <sunny/infrastructure/ableton/managed_routing.hpp>
#include <sunny/infrastructure/formats/ableton_score.hpp>
#include <tuple>

namespace sunny::infrastructure::mcp_realization {
using namespace sunny::core;

// Logical keys and access to the process-held durable store.
json decline(const std::string& message, const std::string& state) {
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

// Select retained attempts by dispatch chronology, then join routing deltas
// against the original Part baseline. Saved receipts never restore authority.
namespace {
bool group_only(const RealizationStoredAttempt& attempt) {
    const auto& request = attempt.intent.prepared.request;
    return request.property_or_method == "sunny_managed_apply_routing" &&
           std::get<json>(request.args.at(0)).at("intent").at("kind") == "adopt_group";
}

} // namespace

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

ReconciliationRequired::ReconciliationRequired(const RealizationStoredAttempt& attempt)
    : std::runtime_error("Reconcile original Part operation " + attempt.intent.attempt_id),
      response(attempt_summary(attempt)) {
    response["success"] = false;
    response["state"] = "reconciliation_required";
    response["mutation_dispatched"] = false;
}

json workflow_failure(const std::exception& failure) {
    if (const auto* pending = dynamic_cast<const ReconciliationRequired*>(&failure))
        return pending->response;
    return decline(failure.what());
}

// Match compiled attacks to the full observed native note population.
namespace {
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

} // namespace

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

// Admit the owning document revision before compiling the selected Part.
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

// Recovery may use a retained musical projection after an offline edit or Part
// removal. Its current native objects still require separate explicit adoption.
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

// Fence first, consume one permit, send once, and retain the actual outcome.
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

// Preserve retained attack IDs while revising or adding/deleting whole Events.
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

} // namespace sunny::infrastructure::mcp_realization
