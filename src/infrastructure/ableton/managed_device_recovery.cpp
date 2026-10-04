#include <algorithm>
#include <charconv>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>
#include <sunny/infrastructure/ableton/managed_device_recovery.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using Error = ManagedDeviceRecoveryError;
using PlanResult = std::expected<ManagedDeviceRecoveryPlan, Error>;
constexpr std::string_view insert = "sunny_managed_insert_device";
constexpr std::string_view update = "sunny_managed_update_device_parameters";
constexpr std::string_view modes = "sunny_managed_update_device_modes";
constexpr std::string_view adopt = "sunny_managed_adopt_devices";

struct Denied {
    Error error;
};
[[noreturn]] void deny(std::string reason,
                       std::string diagnostic,
                       std::optional<std::string> attempt = std::nullopt) {
    throw Denied{{std::move(reason), std::move(diagnostic), std::move(attempt)}};
}
bool hex(std::string_view text, std::size_t width) {
    return text.size() == width && std::ranges::all_of(text, [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
std::string hexadecimal(std::uint64_t number) {
    char buffer[16];
    const auto end = std::to_chars(buffer, buffer + sizeof(buffer), number, 16).ptr;
    return {buffer, end};
}
bool core_id(std::string_view text) {
    if (text.empty() || text.size() > 16 || text.front() == '0' || !hex(text, text.size()))
        return false;
    std::uint64_t number = 0;
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), number, 16);
    return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size() && number != 0;
}
bool same_context(const ManagedBridgeContext& a, const ManagedBridgeContext& b) {
    return a.bridge_instance == b.bridge_instance && a.document_token == b.document_token;
}
bool device_operation(std::string_view method) {
    return method == insert || method == update || method == modes || method == adopt;
}
const json& request_payload(const ManagedOperationReceipt& receipt) {
    return std::get<json>(receipt.request.args.at(0));
}
bool group_only(const ManagedOperationReceipt& receipt) {
    return receipt.request.property_or_method == "sunny_managed_apply_routing" &&
           request_payload(receipt).at("intent").at("kind") == "adopt_group";
}

bool musical_match(const RealizationAttemptIntent& intent, const json& observation) {
    const auto& projection = intent.desired_projection;
    const auto& manifest = observation.at("manifest");
    const auto& clip = manifest.at("clip");
    if (manifest.at("entire_clip_population_observed") != true ||
        !observation.contains("note_identity") ||
        observation.at("note_identity").at("entire_clip_population_observed") != true ||
        clip.at("start_marker") != 0.0 || clip.at("end_marker") != projection.at("clip_end") ||
        clip.at("signature_numerator") != projection.at("signature_numerator") ||
        clip.at("signature_denominator") != projection.at("signature_denominator") ||
        clip.at("looping") != false || clip.at("muted") != false || clip.at("launch_mode") != 0 ||
        clip.at("launch_quantization") != 1 || clip.at("legato") != false ||
        clip.at("velocity_amount") != 0.0 || clip.at("has_groove") != false)
        return false;
    // Distinct complete semantic notes admit an unambiguous attack-key/native-ID
    // association; neither insertion order nor note-array order is an oracle.
    std::set<std::string> desired, actual;
    for (const auto& note : projection.at("notes")) {
        const auto key =
            managed_detail::canonical_managed_bytes(managed_detail::semantic_note(note));
        if (!key || !desired.insert(*key).second) return false;
    }
    for (const auto& note : observation.at("note_identity").at("notes")) {
        const auto key =
            managed_detail::canonical_managed_bytes(managed_detail::semantic_note(note));
        if (!key || !actual.insert(*key).second) return false;
    }
    return desired == actual;
}
bool preservation_verified(const ManagedOperationReceipt& receipt) {
    const auto& result = receipt.journal->at("result");
    const auto method = receipt.request.property_or_method;
    const auto all = [&](std::string_view supplement, std::initializer_list<const char*> fields) {
        return std::ranges::all_of(
            fields, [&](const auto* field) { return result.at(supplement).at(field) == true; });
    };
    if (method == "sunny_managed_create_clip" || method == "sunny_managed_replace_clip")
        return result.at("observed_notes_match_request") == true &&
               result.at("observed_clip_properties_match_request") == true;
    if (method == "sunny_managed_update_clip_geometry")
        return all("clip_geometry_update",
                   {"observed_geometry_matches_request",
                    "note_values_preserved",
                    "note_ids_preserved",
                    "note_cardinality_preserved",
                    "other_finite_properties_preserved",
                    "device_identity_preserved"});
    if (method == "sunny_managed_update_notes")
        return all(
            "note_update",
            {"observed_updates_match_request", "untouched_notes_preserved", "note_ids_preserved"});
    if (method == "sunny_managed_revise_note_population")
        return all("note_population_update",
                   {"observed_changes_match_request",
                    "observed_deletions_absent",
                    "observed_additions_match_request",
                    "untouched_notes_preserved",
                    "retained_note_ids_preserved",
                    "observed_population_cardinality_match"});
    return true;
}

struct Entry {
    const RealizationStoredAttempt* stored;
    const ManagedOperationReceipt* acknowledged = nullptr;
    std::optional<ManagedBindingReceipt> binding;
};
Entry validate(const RealizationStoredAttempt& stored,
               std::string_view map_key,
               const ManagedDeviceRecoveryScope& scope) {
    const auto& intent = stored.intent;
    const auto id = intent.attempt_id;
    const auto project = "w" + scope.workspace_namespace + "_s" + hexadecimal(scope.score_id.value);
    const auto binding = "part_" + hexadecimal(scope.part_id.value);
    if (map_key != id || !hex(id, 32) || !intent.project_revision ||
        stored.evidence.size() > REALIZATION_STORE_MAX_EVIDENCE ||
        stored.bindings.size() > REALIZATION_STORE_MAX_EVIDENCE)
        deny("InvalidHistory", "Immutable attempt identity or evidence bounds are invalid", id);
    const auto prepared = managed_receipt_from_json(managed_receipt_to_json(intent.prepared));
    if (!prepared || prepared->outcome != ManagedOperationOutcome::Prepared ||
        prepared->delivery != LomDeliveryState::NotSent || prepared->journal || prepared->error)
        deny("InvalidHistory", "Original intent is not a closed unsent Prepared receipt", id);
    const auto& payload = request_payload(*prepared);
    if (payload.at("project_key") != project || payload.at("binding_key") != binding ||
        payload.at("operation_id") != id)
        deny("WrongScope", "Request logical keys do not close against namespace/Score/Part", id);
    const auto identity =
        realization_note_identity(intent.desired_note_keys, intent.desired_projection);
    if (!identity || *identity != intent.desired_note_identity)
        deny("InvalidMusicalSource",
             "Retained attack keys/projection fail their typed identity",
             id);
    if (prepared->request.property_or_method == "sunny_managed_create_clip" ||
        prepared->request.property_or_method == "sunny_managed_replace_clip") {
        const json requested{{"clip_end", payload.at("clip_end")},
                             {"signature_numerator", payload.at("signature_numerator")},
                             {"signature_denominator", payload.at("signature_denominator")},
                             {"notes", payload.at("notes")}};
        if (requested.dump() != intent.desired_projection.dump())
            deny("InvalidMusicalSource",
                 "Create/replace projection differs from immutable request",
                 id);
    }
    Entry entry{&stored, nullptr, std::nullopt};
    std::set<std::string> derived_bindings;
    std::optional<std::string> terminal;
    const auto original = managed_receipt_to_json(intent.prepared);
    for (const auto& evidence : stored.evidence) {
        const auto encoded = managed_receipt_to_json(evidence);
        const auto checked = managed_receipt_from_json(encoded);
        if (!checked || encoded.at("context") != original.at("context") ||
            encoded.at("request").dump() != original.at("request").dump())
            deny("InvalidHistory",
                 "Evidence does not retain the original typed context/request",
                 id);
        if (evidence.journal && evidence.outcome != ManagedOperationOutcome::UnknownEpoch &&
            evidence.outcome != ManagedOperationOutcome::UnknownOperation) {
            const auto journal = evidence.journal->dump();
            if (terminal && *terminal != journal)
                deny("ConflictingHistory", "Retained terminal journals disagree", id);
            terminal = journal;
        }
        if (evidence.outcome == ManagedOperationOutcome::Acknowledged) {
            entry.acknowledged = &evidence;
            if (!group_only(evidence)) {
                const auto actual = managed_binding_receipt(evidence);
                if (!actual) deny("InvalidHistory", "Terminal ACK has no closed binding", id);
                entry.binding = *actual;
                derived_bindings.insert(managed_binding_to_json(*actual).dump());
            }
        }
    }
    for (const auto& saved : stored.bindings) {
        if (!managed_binding_from_json(managed_binding_to_json(saved)))
            deny("InvalidHistory", "Saved binding codec rejected the observation", id);
        if (!derived_bindings.contains(managed_binding_to_json(saved).dump()))
            deny("InvalidHistory", "Saved binding does not derive from a retained ACK", id);
    }
    if (entry.acknowledged && !group_only(*entry.acknowledged)) {
        if (!entry.binding || !std::ranges::any_of(stored.bindings, [&](const auto& saved) {
                return managed_binding_to_json(saved) == managed_binding_to_json(*entry.binding);
            }))
            deny("IncompleteHistory", "Acknowledgement lacks its exact retained binding", id);
    }
    return entry;
}

struct IntentState {
    ManagedNativeDevice device;
    std::map<std::string, ManagedDevicePhysicalIntent> physical;
    std::map<std::string, ManagedDeviceModeIntent> enums;
    std::map<std::string, ManagedDevicePropertyIntent> properties;
};
ManagedNativeDevice native_device(const json& declaration) {
    const auto name = declaration.at("class_name").get<std::string>();
    if (name == "Drift") return ManagedNativeDevice::Drift;
    if (name == "StereoGain") return ManagedNativeDevice::Utility;
    if (name == "Eq8") return ManagedNativeDevice::EqEight;
    deny("UnsupportedDevice", "Saved class is outside Drift/Utility/EQ8");
}
std::string profile_key(const json& cohort) {
    const auto key = cohort.at(0).at("device_key").get<std::string>();
    if (!key.starts_with("source_") || !core_id(std::string_view{key}.substr(7)))
        deny("InvalidDeviceKey", "Saved Drift lacks a canonical source_<ProfileID> key");
    return key.substr(7);
}
void require_device_key(const std::string& key,
                        ManagedNativeDevice device,
                        std::size_t index,
                        const std::string& profile) {
    if (index == 0) {
        if (device == ManagedNativeDevice::Drift && key == "source_" + profile) return;
    } else if (device != ManagedNativeDevice::Drift) {
        const auto suffix = [&](std::string_view prefix,
                                std::string_view separator,
                                std::string_view end = {}) {
            if (!std::string_view{key}.starts_with(prefix) || !std::string_view{key}.ends_with(end))
                return false;
            auto body = std::string_view{key}.substr(prefix.size(),
                                                     key.size() - prefix.size() - end.size());
            const auto split = body.find(separator);
            return split != std::string_view::npos && core_id(body.substr(0, split)) &&
                   core_id(body.substr(split + separator.size()));
        };
        if ((device == ManagedNativeDevice::EqEight &&
             key.starts_with("timbre_" + profile + "_effect_") &&
             core_id(std::string_view{key}.substr(15 + profile.size()))) ||
            suffix("mix_", "_effect_") ||
            (device == ManagedNativeDevice::Utility && suffix("mix_", "_channel_", "_trim")))
            return;
    }
    deny("InvalidDeviceKey", "Saved key/type/order is not a canonical owning-device identity");
}
void physical_delta(IntentState& state, const json& intents) {
    for (const auto& intent : intents) {
        const auto capability = intent.at("capability_id").get<std::string>();
        state.physical[capability] = {capability, intent.at("target"), intent.at("tolerance")};
    }
}
void mode_delta(IntentState& state, const json& declarations) {
    for (const auto& intent : declarations.at("enum_intents")) {
        const auto capability = intent.at("capability_id").get<std::string>();
        state.enums[capability] = {capability, intent.at("label")};
    }
    for (const auto& intent : declarations.at("property_intents")) {
        const auto property = intent.at("property").get<std::string>();
        state.properties[property] = {property, intent.at("label")};
    }
}
const json* parameter(const json& member, std::string_view name) {
    const json* found = nullptr;
    for (const auto& item : member.at("parameters"))
        if (item.at("name") == name || item.at("original_name") == name) {
            if (found || item.at("original_name") != name) return nullptr;
            found = &item;
        }
    return found;
}
std::optional<std::string> label(const json& descriptor, std::uint32_t count) {
    if (descriptor.at("is_quantized") != true || descriptor.at("is_enabled") != true ||
        descriptor.at("state").get<int>() > 1 || descriptor.at("automation_state") != 0 ||
        descriptor.at("minimum") != 0.0 ||
        descriptor.at("maximum") != static_cast<double>(count - 1) ||
        descriptor.at("value_items").size() != count)
        return std::nullopt;
    const double value = descriptor.at("value");
    if (std::trunc(value) != value) return std::nullopt;
    std::set<std::string> labels;
    for (const auto& item : descriptor.at("value_items"))
        if (!labels.insert(item.get<std::string>()).second) return std::nullopt;
    if (count == 2 && labels != std::set<std::string>{"Off", "On"}) return std::nullopt;
    return descriptor.at("value_items").at(static_cast<std::size_t>(value)).get<std::string>();
}
template <class T> std::vector<T> ordered_values(const std::map<std::string, T>& map) {
    std::vector<T> result;
    for (const auto& [key, value] : map) {
        (void)key;
        result.push_back(value);
    }
    return result;
}
} // namespace

PlanResult
fold_managed_device_recovery(const std::map<std::string, RealizationStoredAttempt>& history,
                             const ManagedDeviceRecoveryScope& scope) {
    try {
        if (!hex(scope.workspace_namespace, 32) || !scope.score_id.value || !scope.part_id.value ||
            !hex(scope.through_attempt_id, 32) || history.empty() ||
            history.size() > REALIZATION_STORE_MAX_ATTEMPTS ||
            (scope.expected_device_history_attempt &&
             !hex(*scope.expected_device_history_attempt, 32)))
            deny("InvalidScope", "Bounded namespace/Score/Part/source identities are required");
        const auto through = history.find(scope.through_attempt_id);
        if (through == history.end() || through->second.intent.score_id != scope.score_id ||
            through->second.intent.part_id != scope.part_id)
            deny("WrongScope",
                 "The selected musical history ceiling is absent or belongs elsewhere");
        std::set<std::uint64_t> ordinals;
        std::vector<Entry> entries;
        for (const auto& [key, attempt] : history) {
            if (!attempt.dispatch_ordinal || attempt.dispatch_ordinal > history.size() ||
                !ordinals.insert(attempt.dispatch_ordinal).second)
                deny("InvalidHistory",
                     "Dispatch ordinals must be distinct dense retained ordinals",
                     key);
            if (attempt.intent.score_id == scope.score_id &&
                attempt.intent.part_id == scope.part_id &&
                attempt.dispatch_ordinal <= through->second.dispatch_ordinal)
                entries.push_back(validate(attempt, key, scope));
        }
        std::ranges::sort(entries, {}, [](const Entry& e) { return e.stored->dispatch_ordinal; });
        const Entry* snapshot = nullptr;
        for (const auto& entry : entries)
            if (entry.binding && entry.binding->observation.contains("device_identity") &&
                !entry.binding->observation.at("device_identity").at("cohort").empty())
                snapshot = &entry;
        if (!snapshot)
            deny("MissingDeviceSnapshot", "No typed successful nonempty full saved cohort exists");
        const auto snapshot_id = snapshot->stored->intent.attempt_id;
        if (!musical_match(snapshot->stored->intent, snapshot->binding->observation) ||
            !preservation_verified(*snapshot->acknowledged))
            deny("InvalidMusicalSource",
                 "Selected saved cohort does not prove its complete musical projection",
                 snapshot_id);
        if (scope.expected_device_history_attempt &&
            *scope.expected_device_history_attempt != snapshot_id)
            deny("StaleDeviceSnapshot",
                 "The explicitly selected Device snapshot is no longer latest",
                 snapshot_id);
        const auto context = snapshot->binding->context;
        const Entry* seed = nullptr;
        for (const auto& entry : entries)
            if (entry.stored->dispatch_ordinal <= snapshot->stored->dispatch_ordinal &&
                entry.binding && same_context(entry.binding->context, context) &&
                entry.acknowledged->request.property_or_method == adopt)
                seed = &entry;
        if (!seed)
            for (const auto& entry : entries)
                if (entry.stored->dispatch_ordinal <= snapshot->stored->dispatch_ordinal &&
                    entry.binding && same_context(entry.binding->context, context) &&
                    entry.acknowledged->request.property_or_method == "sunny_managed_create_clip" &&
                    entry.binding->observation.contains("device_identity") &&
                    entry.binding->observation.at("device_identity").at("cohort").empty()) {
                    seed = &entry;
                    break;
                }
        if (!seed)
            deny("MissingEmptyOriginOrAdoption",
                 "Chain lacks successful empty creation or whole-chain adoption",
                 snapshot_id);
        if (!musical_match(seed->stored->intent, seed->binding->observation) ||
            !preservation_verified(*seed->acknowledged))
            deny("InvalidMusicalSource",
                 "Device intent seed lacks complete musical proof",
                 seed->stored->intent.attempt_id);
        auto running = seed->binding->observation.at("device_identity");
        auto running_fingerprint = seed->binding->observation.at("device_identity_fingerprint");
        std::map<std::string, IntentState> states;
        if (seed->acknowledged->request.property_or_method == adopt)
            for (const auto& selected :
                 request_payload(*seed->acknowledged).at("approved_preview").at("devices")) {
                IntentState state{native_device(selected.at("device")), {}, {}, {}};
                physical_delta(state, selected.at("physical_intents"));
                if (selected.contains("enum_intents")) mode_delta(state, selected);
                states.emplace(selected.at("device_key").get<std::string>(), std::move(state));
            }
        ManagedDeviceRecoveryPlan plan{
            snapshot_id,
            context,
            snapshot->binding->observation.at("device_identity_fingerprint"),
            {},
            {},
            {}};
        plan.contributing_attempt_ids.push_back(seed->stored->intent.attempt_id);
        for (const auto& entry : entries) {
            if (entry.stored->dispatch_ordinal <= seed->stored->dispatch_ordinal ||
                !same_context(entry.stored->intent.prepared.context, context))
                continue;
            const auto id = entry.stored->intent.attempt_id;
            const auto& receipt = entry.acknowledged ? *entry.acknowledged
                                                     : (entry.stored->evidence.empty()
                                                            ? entry.stored->intent.prepared
                                                            : entry.stored->evidence.back());
            if (!entry.acknowledged) {
                const bool no_effect =
                    (receipt.outcome == ManagedOperationOutcome::Declined && receipt.journal &&
                     receipt.journal->at("native_mutation_started") == false) ||
                    (receipt.outcome == ManagedOperationOutcome::NotSent &&
                     receipt.delivery == LomDeliveryState::NotSent && !receipt.journal);
                if (no_effect) continue;
                deny("UnresolvedHistoryGap",
                     "A fenced operation lacks a proven completed/no-effect outcome",
                     id);
            }
            if (group_only(receipt)) continue;
            const auto& observed = entry.binding->observation;
            if (!musical_match(entry.stored->intent, observed) || !preservation_verified(receipt))
                deny("InvalidMusicalSource",
                     "Intervening ACK lacks complete musical preservation",
                     id);
            if (!observed.contains("device_identity"))
                deny("IncompleteDeviceObservation",
                     "An intervening ACK omitted the full saved device cohort",
                     id);
            const auto method = receipt.request.property_or_method;
            const auto& payload = request_payload(receipt);
            if (device_operation(method)) {
                if (method == adopt)
                    deny("InvalidHistory",
                         "Whole-chain adoption seed selection is inconsistent",
                         id);
                const auto& supplement =
                    observed.at(method == modes ? "device_mode_update" : "device_update");
                if (supplement.at("before_device_identity") != running ||
                    supplement.at("before_device_identity_fingerprint") != running_fingerprint)
                    deny("BrokenDeviceJoin",
                         "Device delta does not join the exact running saved cohort",
                         id);
                if (!managed_device_detail::device_result_matches_request(
                        method, payload, observed))
                    deny("InvalidDeviceReceipt",
                         "Device delta fails its existing strict typed ACK codec",
                         id);
                const auto key = payload.at("device_key").get<std::string>();
                if (method == insert) {
                    if (!states
                             .emplace(key,
                                      IntentState{native_device(payload.at("device")), {}, {}, {}})
                             .second)
                        deny(
                            "BrokenDeviceJoin", "Append reused an existing logical Device key", id);
                }
                const auto state = states.find(key);
                if (state == states.end())
                    deny("BrokenDeviceJoin", "Delta targets an unseeded Device", id);
                if (method == modes)
                    mode_delta(state->second, payload);
                else
                    physical_delta(state->second, payload.at("physical_intents"));
                running = observed.at("device_identity");
                running_fingerprint = observed.at("device_identity_fingerprint");
            } else if (observed.at("device_identity") != running ||
                       observed.at("device_identity_fingerprint") != running_fingerprint)
                deny(
                    "UncommandedDeviceDrift", "Non-device ACK changed the saved device cohort", id);
            plan.contributing_attempt_ids.push_back(id);
        }
        const auto& cohort = snapshot->binding->observation.at("device_identity").at("cohort");
        if (running != snapshot->binding->observation.at("device_identity") ||
            running_fingerprint !=
                snapshot->binding->observation.at("device_identity_fingerprint") ||
            states.size() != cohort.size())
            deny("BrokenDeviceJoin",
                 "Fold does not end at the selected complete saved cohort",
                 snapshot_id);
        const auto profile = profile_key(cohort);
        for (std::size_t index = 0; index < cohort.size(); ++index) {
            const auto& member = cohort[index];
            const auto key = member.at("device_key").get<std::string>();
            const auto found = states.find(key);
            if (found == states.end() || found->second.device != native_device(member))
                deny("BrokenDeviceJoin",
                     "A saved whole-chain member has no typed intent state",
                     snapshot_id);
            auto& state = found->second;
            require_device_key(key, state.device, index, profile);
            ManagedDeviceRecoveryResidual residual{key, {}, false, {}, member.at("modes"), false};
            ManagedDeviceAdoptionSelection selection{
                key, static_cast<std::uint32_t>(index), state.device, {}};
            const bool bypassed = member.at("is_active") == false;
            if (state.device == ManagedNativeDevice::EqEight &&
                (!member.at("modes").contains("edit_mode") ||
                 !member.at("modes").contains("oversample")))
                deny("IncompleteSavedModes",
                     "Legacy EQ edit/oversample state is unknown, not a default",
                     snapshot_id);
            if (bypassed) {
                selection.authored_bypass = true;
                selection.enum_intents = {{state.device == ManagedNativeDevice::Utility
                                               ? "utility.enabled"
                                               : "eq8.enabled",
                                           "Off"}};
                residual.bypassed_physical_intents = ordered_values(state.physical);
                residual.bypassed_physical_intents_unknown = state.physical.empty();
            } else {
                if (state.physical.empty())
                    deny("IncompletePhysicalIntent",
                         "Active inserted/adopted Device has no saved physical target",
                         snapshot_id);
                for (const auto& [capability, intent] : state.physical) {
                    (void)intent;
                    const auto registry = sunny::core::live_native_parameter_registry();
                    const auto entry = std::ranges::find(
                        registry, capability, &sunny::core::LiveNativeParameterCapability::id);
                    if (entry == registry.end())
                        deny("UnsupportedCapability",
                             "Saved physical capability is unknown",
                             snapshot_id);
                    const auto* row = parameter(member, entry->parameter_original_name);
                    if (!row || row->at("descriptor").at("is_quantized") != false ||
                        row->at("descriptor").at("is_enabled") != true ||
                        row->at("descriptor").at("state").get<int>() > 1 ||
                        row->at("descriptor").at("automation_state") != 0)
                        deny("UnavailableSavedControl",
                             "Selected saved physical control is missing/disabled/automated",
                             snapshot_id);
                }
                selection.physical_intents = ordered_values(state.physical);
                if (state.device != ManagedNativeDevice::Drift) {
                    const auto registry = sunny::core::live_native_parameter_registry();
                    for (const auto& entry : registry) {
                        if (entry.device_class_name != member.at("class_name").get<std::string>() ||
                            entry.kind != sunny::core::LiveNativeParameterKind::Quantized)
                            continue;
                        const auto* row = parameter(member, entry.parameter_original_name);
                        const auto actual =
                            row ? label(row->at("descriptor"), *entry.expected_enum_items)
                                : std::nullopt;
                        const auto authored = state.enums.find(entry.id);
                        if (authored != state.enums.end() &&
                            (!actual || authored->second.label != *actual))
                            deny("SavedIntentMismatch",
                                 "Saved mode intent differs from actual unique advertised label",
                                 snapshot_id);
                        if (actual && authored == state.enums.end()) {
                            state.enums.emplace(entry.id,
                                                ManagedDeviceModeIntent{entry.id, *actual});
                            residual.observed_mode_guards.push_back({entry.id, *actual});
                        }
                    }
                    const auto needs = [&](std::string_view capability, std::string_view expected) {
                        const auto mode = state.enums.find(std::string{capability});
                        return mode != state.enums.end() && mode->second.label == expected;
                    };
                    if (state.device == ManagedNativeDevice::Utility &&
                        (!needs("utility.enabled", "On") ||
                         !needs("utility.channel_mode", "Stereo") ||
                         !needs("utility.mono", "Off") || !needs("utility.mute", "Off")))
                        deny("IncompleteSavedModes",
                             "Utility lacks actual On/Stereo/Mono Off/Mute Off guards",
                             snapshot_id);
                    if (state.device == ManagedNativeDevice::EqEight) {
                        if (!needs("eq8.enabled", "On") ||
                            member.at("modes").at("global_mode") != 0)
                            deny("IncompleteSavedModes",
                                 "EQ requires actual On and Stereo mode",
                                 snapshot_id);
                        state.properties["global_mode"] = {"global_mode", "Stereo"};
                    }
                    selection.enum_intents = ordered_values(state.enums);
                    selection.property_intents = ordered_values(state.properties);
                }
            }
            plan.selections.push_back(std::move(selection));
            plan.residuals.push_back(std::move(residual));
        }
        if (!make_managed_device_preview_request(context, *snapshot->binding, plan.selections))
            deny("UnsupportedSavedSelection",
                 "Complete saved selections fail the existing finite preview request codec",
                 snapshot_id);
        return plan;
    } catch (const Denied& denied) {
        return std::unexpected(denied.error);
    } catch (const std::exception&) {
        return std::unexpected(Error{
            "InvalidHistory", "Malformed or unbounded typed history is unavailable", std::nullopt});
    }
}
} // namespace sunny::infrastructure
