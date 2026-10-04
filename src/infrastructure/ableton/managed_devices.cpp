#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <sunny/infrastructure/ableton/detail/managed_capacity.hpp>
#include <sunny/infrastructure/ableton/detail/managed_devices.hpp>
#include <sunny/infrastructure/ableton/detail/managed_fingerprint.hpp>
#include <sunny/infrastructure/ableton/detail/managed_group.hpp>
#include <sunny/infrastructure/ableton/detail/managed_notes.hpp>
#include <sunny/infrastructure/ableton/detail/native_unit_numeric.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>

namespace sunny::infrastructure {
namespace {
using nlohmann::json;
using sunny::core::ErrorCode;
using sunny::core::Result;
using Unit = sunny::core::LiveNativePhysicalUnit;
constexpr std::string_view insert_method = "sunny_managed_insert_device";
constexpr std::string_view update_method = "sunny_managed_update_device_parameters";
constexpr std::string_view preview_method = "sunny_managed_preview_devices";
constexpr std::string_view inspection_method = "sunny_managed_inspect_devices";
constexpr std::string_view mode_method = "sunny_managed_update_device_modes";
constexpr std::string_view adopt_method = "sunny_managed_adopt_devices";

bool fields(const json& value, std::initializer_list<std::string_view> names) {
    return value.is_object() && value.size() == names.size() &&
           std::ranges::all_of(names, [&](auto name) { return value.contains(name); });
}
bool text(const json& value, std::size_t maximum = 4096) {
    return value.is_string() && !value.get_ref<const std::string&>().empty() &&
           value.get_ref<const std::string&>().size() <= maximum;
}
bool key(const json& value) {
    return text(value, 64) && std::ranges::all_of(value.get_ref<const std::string&>(), [](char c) {
               return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                      c == '_' || c == '-';
           });
}
bool finite(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}
bool integer(const json& value, std::uint64_t maximum) {
    return value.is_number_integer() &&
           (value.is_number_unsigned()
                ? value.get<std::uint64_t>() <= maximum
                : value.get<std::int64_t>() >= 0 &&
                      static_cast<std::uint64_t>(value.get<std::int64_t>()) <= maximum);
}
bool fingerprint(const json& value) {
    return text(value, 64) && value.get_ref<const std::string&>().size() == 64 &&
           std::ranges::all_of(value.get_ref<const std::string&>(), [](char c) {
               return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
           });
}
json device_descriptor(ManagedNativeDevice device) {
    switch (device) {
    case ManagedNativeDevice::Drift:
        return {{"browser_name", "Drift"},
                {"class_name", "Drift"},
                {"type", 1},
                {"role", "source"},
                {"insertion_policy", "AppendOwnedChain"}};
    case ManagedNativeDevice::Utility:
        return {{"browser_name", "Utility"},
                {"class_name", "StereoGain"},
                {"type", 2},
                {"role", "effect"},
                {"insertion_policy", "AppendOwnedChain"}};
    case ManagedNativeDevice::EqEight:
        return {{"browser_name", "EQ Eight"},
                {"class_name", "Eq8"},
                {"type", 2},
                {"role", "effect"},
                {"insertion_policy", "AppendOwnedChain"}};
    }
    return nullptr;
}
bool descriptor_valid(const json& value) {
    return std::ranges::any_of(std::array{ManagedNativeDevice::Drift,
                                          ManagedNativeDevice::Utility,
                                          ManagedNativeDevice::EqEight},
                               [&](auto device) { return value == device_descriptor(device); }) &&
           value.at("type").is_number_integer();
}
const sunny::core::LiveNativeParameterCapability* capability(const std::string& id) {
    const auto catalogue = sunny::core::live_native_parameter_registry();
    const auto found =
        std::ranges::find(catalogue, id, &sunny::core::LiveNativeParameterCapability::id);
    return found != catalogue.end() && found->physical_unit &&
                   found->kind == sunny::core::LiveNativeParameterKind::Continuous
               ? &*found
               : nullptr;
}
bool parameter_descriptor(const json& value) {
    if (!fields(value,
                {"minimum",
                 "maximum",
                 "value",
                 "default_value",
                 "is_quantized",
                 "is_enabled",
                 "state",
                 "automation_state",
                 "value_items"}) ||
        !finite(value.at("minimum")) || !finite(value.at("maximum")) ||
        !finite(value.at("value")) || !value.at("is_quantized").is_boolean() ||
        !value.at("is_enabled").is_boolean() || !integer(value.at("state"), 2) ||
        !integer(value.at("automation_state"), 2) || !value.at("value_items").is_array() ||
        value.at("value_items").size() > 512 ||
        !std::ranges::all_of(value.at("value_items"), [](const auto& item) { return text(item); }))
        return false;
    for (const auto* field : {"minimum", "maximum", "value"})
        if (!value.at(field).is_number_float()) return false;
    if (value.at("is_quantized") == true
            ? !value.at("default_value").is_null()
            : (!value.at("default_value").is_number_float() || !finite(value.at("default_value"))))
        return false;
    const double low = value.at("minimum"), high = value.at("maximum"), current = value.at("value");
    return low <= current && current <= high &&
           (value.at("is_quantized") == true || value.at("value_items").empty());
}
const sunny::core::LiveNativeParameterCapability* mode_capability(const std::string& id) {
    const auto catalogue = sunny::core::live_native_parameter_registry();
    const auto found =
        std::ranges::find(catalogue, id, &sunny::core::LiveNativeParameterCapability::id);
    return found != catalogue.end() &&
                   found->kind == sunny::core::LiveNativeParameterKind::Quantized &&
                   found->device_class_name != "Drift"
               ? &*found
               : nullptr;
}
bool enum_choice(const json& descriptor, std::uint32_t count, const json& label) {
    if (!parameter_descriptor(descriptor) || descriptor.at("is_quantized") != true ||
        descriptor.at("is_enabled") != true || descriptor.at("state").get<int>() > 1 ||
        descriptor.at("automation_state") != 0 || descriptor.at("minimum") != 0.0 ||
        descriptor.at("maximum") != static_cast<double>(count - 1) ||
        std::trunc(descriptor.at("value").get<double>()) != descriptor.at("value").get<double>() ||
        descriptor.at("value_items").size() != count || !text(label, 256))
        return false;
    std::set<std::string> labels;
    for (const auto& item : descriptor.at("value_items"))
        if (!labels.insert(item.template get<std::string>()).second) return false;
    if (!labels.contains(label.get<std::string>())) return false;
    return count != 2 || labels == std::set<std::string>{"Off", "On"};
}
bool proven_effect_bypass(const json& entry) {
    if (entry.at("role") != "effect") return false;
    const json* on = nullptr;
    for (const auto& parameter : entry.at("parameters"))
        if (parameter.at("name") == "Device On" || parameter.at("original_name") == "Device On") {
            if (on || parameter.at("original_name") != "Device On") return false;
            on = &parameter.at("descriptor");
        }
    // Bypass capture itself does not require writable/unaudited parameters; it
    // proves actual Off only. Mode setter admission separately checks
    // eligibility.
    if (!on || on->at("is_quantized") != true || on->at("minimum") != 0.0 ||
        on->at("maximum") != 1.0 || on->at("value_items").size() != 2 ||
        (on->at("value") != 0.0 && on->at("value") != 1.0))
        return false;
    const auto& items = on->at("value_items");
    return ((items[0] == "Off" && items[1] == "On") || (items[0] == "On" && items[1] == "Off")) &&
           items[on->at("value").get<std::size_t>()] == "Off";
}
bool voice(const json& value) {
    if (!fields(value, {"index", "value_items", "label"}) || !value.at("value_items").is_array() ||
        value.at("value_items").empty() || value.at("value_items").size() > 64 ||
        !integer(value.at("index"), value.at("value_items").size() - 1))
        return false;
    std::set<std::string> labels;
    for (const auto& label : value.at("value_items"))
        if (!text(label) || !labels.insert(label.template get<std::string>()).second) return false;
    return value.at("label") == value.at("value_items").at(value.at("index").get<std::size_t>());
}
bool close(double left, double right) {
    return std::isfinite(left) && std::isfinite(right) &&
           std::abs(left - right) <= 8 * std::numeric_limits<double>::epsilon() *
                                         std::max({1.0, std::abs(left), std::abs(right)});
}

bool group_candidate(const json& observation) {
    try {
        if (!managed_detail::group_supplement_valid(observation)) return false;
        const auto& grouped = observation.at("manifest").at("track").at("is_grouped");
        return grouped.is_boolean() &&
               (grouped == false || managed_detail::group_current_proof(observation));
    } catch (const json::exception&) {
        return false;
    }
}
bool group_payload_closes(const json& observation, const json& payload) {
    if (!group_candidate(observation)) return false;
    if (!observation.contains("group_authority")) return true;
    const auto& proof = observation.at("group_authority");
    return proof.at("context").at("document_token") == payload.at("document_token") &&
           proof.at("project_key") == payload.at("project_key") &&
           proof.at("selected_binding_key") == payload.at("binding_key");
}
bool same_group_context(const json& before, const json& after) {
    if (before.contains("group_authority") != after.contains("group_authority")) return false;
    if (!before.contains("group_authority")) return true;
    // A Device setter authorizes no Group or Track-index transition within
    // this operation. A permitted pre-dispatch reindex is captured in both
    // before/after observations. Routing Return rebases use their own proof.
    return before.at("group_authority") == after.at("group_authority");
}
bool observation_digest(const json& observation) {
    if (!observation.is_object() || !group_candidate(observation) ||
        !observation.contains("manifest") || !observation.contains("content_fingerprint") ||
        !observation.contains("note_identity") ||
        !observation.contains("note_identity_fingerprint") ||
        !managed_detail::note_identity_valid(observation.at("note_identity")))
        return false;
    const auto content = managed_detail::managed_digest(observation.at("manifest"));
    const auto notes = managed_detail::managed_digest(observation.at("note_identity"));
    return content && notes && observation.at("content_fingerprint") == *content &&
           observation.at("note_identity_fingerprint") == *notes;
}
bool binding_changes(const json& before, const json& after, bool source_insert) {
    auto first = before.at("manifest"), second = after.at("manifest");
    if (source_insert) {
        if (first.at("track").at("has_midi_input") != true ||
            second.at("track").at("has_midi_input") != true ||
            first.at("track").at("has_audio_output") != false ||
            second.at("track").at("has_audio_output") != true ||
            second.at("track").at("has_midi_output") != false)
            return false;
        for (const auto* field : {"has_audio_output", "has_midi_output"})
            second["track"][field] = first.at("track").at(field);
        for (const auto* field : {"output_routing_type", "output_routing_channel"})
            second["routing"][field] = first.at("routing").at(field);
        second["mixer"] = first.at("mixer");
    }
    second["devices_empty"] = first.at("devices_empty");
    return first == second && before.at("note_identity") == after.at("note_identity");
}
bool candidate_matches_population(const NativeDisplayCandidate& candidate, const json& member) {
    const auto& parameters = member.at("parameters");
    if (candidate.parameter_index >= parameters.size() ||
        candidate.population.size() != parameters.size())
        return false;
    for (std::size_t i = 0; i < parameters.size(); ++i)
        if (candidate.population[i].at("name") != parameters[i].at("name") ||
            candidate.population[i].at("original_name") != parameters[i].at("original_name"))
            return false;
    const auto& target = parameters[candidate.parameter_index];
    if (target.at("original_name") != candidate.parameter_original_name) return false;
    const auto& observed_descriptor = candidate.evidence.at("descriptor");
    for (const auto* field : {"minimum", "maximum", "value", "default_value"})
        if (!observed_descriptor.at(field).is_number_float()) return false;
    auto expected = target.at("descriptor");
    expected.erase("value_items");
    if (expected != candidate.evidence.at("descriptor")) return false;
    const auto& modes = candidate.modes;
    if (member.at("class_name") == "Drift")
        for (const auto* name : {"voice_mode", "voice_count"})
            if (modes.at(name) != member.at("modes").at(name)) return false;
    if (member.at("class_name") == "Eq8" &&
        modes.at("global_mode") != member.at("modes").at("global_mode"))
        return false;
    for (const auto& [name, mode] : modes.items()) {
        if (name == "voice_mode" || name == "voice_count" || name == "global_mode") continue;
        const auto found = std::ranges::find_if(
            parameters, [&](const auto& item) { return item.at("original_name") == name; });
        if (found == parameters.end()) {
            if (name == "Mid/Side Balance") return false;
            return false;
        }
        const auto& descriptor = found->at("descriptor");
        for (const auto& [field, value] : mode.items())
            if (field != "label" && (!descriptor.contains(field) || descriptor.at(field) != value))
                return false;
    }
    return true;
}
std::optional<NativeDisplayCandidate>
resolved_candidate(const json& intent, const json& value, int track, std::size_t selected) {
    const auto path =
        LomPath{{"song", "tracks", std::to_string(track), "devices", std::to_string(selected)}};
    const auto query = make_native_display_resolution_request(
        path, intent.at("capability_id"), intent.at("target"), intent.at("tolerance"));
    if (!query) return std::nullopt;
    const auto parsed =
        parse_native_display_resolution(*query,
                                        {{"schema_version", 1},
                                         {"capability_id", intent.at("capability_id")},
                                         {"target", intent.at("target")},
                                         {"tolerance", intent.at("tolerance")},
                                         {"outcome", "resolved"},
                                         {"candidate", value}});
    return parsed && parsed->candidate ? parsed->candidate : std::nullopt;
}
bool readback_matches(const json& readback,
                      const NativeDisplayCandidate& candidate,
                      bool has_identifier) {
    auto value = readback;
    if (has_identifier) value.erase("capability_id");
    if (!fields(value,
                {"internal_value",
                 "display",
                 "display_value",
                 "display_increment",
                 "absolute_display_error",
                 "matches_intent",
                 "formatter_calls"}) ||
        !finite(value.at("internal_value")) || !finite(value.at("display_value")) ||
        !finite(value.at("absolute_display_error")) || !text(value.at("display"), 80) ||
        value.at("matches_intent") != true || value.at("formatter_calls") != 1)
        return false;
    const auto display = native_unit_detail::display_reading(
        value.at("display").get_ref<const std::string&>(), candidate.unit, false);
    const double scale = candidate.balance_full_scale.value_or(1.0), target = candidate.target;
    const auto decimal = display ? native_unit_detail::decimal_display_comparison(
                                       *display, target, candidate.tolerance, scale, 28)
                                 : std::nullopt;
    const double physical = value.at("display_value");
    if (((candidate.unit == Unit::Hertz || candidate.unit == Unit::QualityFactor) &&
         physical <= 0.0) ||
        ((candidate.unit == Unit::Percent || candidate.unit == Unit::Milliseconds) &&
         physical < 0.0) ||
        (candidate.unit == Unit::StereoBalance && (physical < -1.0 || physical > 1.0)))
        return false;
    return display && decimal && decimal->within_tolerance &&
           close(decimal->display_value, value.at("display_value")) &&
           display->increment.has_value() == !value.at("display_increment").is_null() &&
           (!display->increment ||
            (finite(value.at("display_increment")) &&
             close(*decimal->display_increment, value.at("display_increment")))) &&
           native_unit_detail::arithmetic_equal(decimal->absolute_display_error,
                                                value.at("absolute_display_error")) &&
           value.at("internal_value").get<double>() >= candidate.descriptor.minimum &&
           value.at("internal_value").get<double>() <= candidate.descriptor.maximum;
}
Result<LomRequest> make_request(std::string_view method,
                                const ManagedBridgeContext& context,
                                const std::string& operation_id,
                                const ManagedBindingReceipt& binding,
                                const std::string& device_key,
                                ManagedNativeDevice device,
                                std::span<const ManagedDevicePhysicalIntent> intents) {
    if (!key(context.bridge_instance) || !key(context.document_token) || !key(operation_id) ||
        context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !observation_digest(binding.observation) ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key) ||
        !binding.observation.contains("device_identity") ||
        !managed_device_detail::device_supplement_valid(binding.observation))
        return std::unexpected(ErrorCode::ProtocolError);
    json physical = json::array();
    for (const auto& intent : intents)
        physical.push_back({{"capability_id", intent.capability_id},
                            {"target", intent.target},
                            {"tolerance", intent.tolerance}});
    json payload{{"document_token", context.document_token},
                 {"operation_id", operation_id},
                 {"project_key", binding.project_key},
                 {"binding_key", binding.binding_key},
                 {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
                 {"expected_device_identity_fingerprint",
                  binding.observation.at("device_identity_fingerprint")},
                 {"device_key", device_key},
                 {"device", device_descriptor(device)},
                 {"physical_intents", std::move(physical)}};
    if (!managed_device_detail::device_request_valid(method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::operation_reservation_fits(payload, method))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    const auto& cohort = binding.observation.at("device_identity").at("cohort");
    const auto found = std::ranges::find_if(
        cohort, [&](const auto& entry) { return entry.at("device_key") == device_key; });
    if (method == insert_method) {
        if (found != cohort.end() || cohort.size() >= SUNNY_MANAGED_MAX_DEVICES ||
            (device == ManagedNativeDevice::Drift && !cohort.empty()) ||
            (device != ManagedNativeDevice::Drift && cohort.empty()))
            return std::unexpected(ErrorCode::ProtocolError);
    } else if (found == cohort.end() ||
               found->at("browser_name") != payload.at("device").at("browser_name"))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(LomPaths::song(), std::string{method}, {std::move(payload)});
}
} // namespace

namespace managed_device_detail {
bool device_identity_valid(const json& identity) {
    try {
        if (!fields(identity, {"schema_version", "cohort", "opaque_state_observed"}) ||
            !integer(identity.at("schema_version"), 1) || identity.at("schema_version") != 1 ||
            identity.at("opaque_state_observed") != false || !identity.at("cohort").is_array() ||
            identity.at("cohort").size() > SUNNY_MANAGED_MAX_DEVICES)
            return false;
        std::set<std::string> keys;
        for (std::size_t i = 0; i < identity.at("cohort").size(); ++i) {
            const auto& entry = identity.at("cohort")[i];
            if (!fields(entry,
                        {"device_key",
                         "browser_name",
                         "class_name",
                         "class_display_name",
                         "name",
                         "type",
                         "role",
                         "is_active",
                         "can_have_chains",
                         "parameters",
                         "modes"}) ||
                !key(entry.at("device_key")) ||
                !keys.insert(entry.at("device_key").get<std::string>()).second ||
                !text(entry.at("class_display_name")) || !text(entry.at("name")) ||
                !entry.at("is_active").is_boolean() || entry.at("can_have_chains") != false ||
                !descriptor_valid({{"browser_name", entry.at("browser_name")},
                                   {"class_name", entry.at("class_name")},
                                   {"type", entry.at("type")},
                                   {"role", entry.at("role")},
                                   {"insertion_policy", "AppendOwnedChain"}}) ||
                (i == 0 ? entry.at("role") != "source" : entry.at("role") != "effect") ||
                !entry.at("parameters").is_array() || entry.at("parameters").empty() ||
                entry.at("parameters").size() > SUNNY_MANAGED_MAX_DEVICE_PARAMETERS)
                return false;
            for (const auto& parameter : entry.at("parameters"))
                if (!fields(parameter, {"name", "original_name", "descriptor"}) ||
                    !text(parameter.at("name")) || !text(parameter.at("original_name")) ||
                    !parameter_descriptor(parameter.at("descriptor")))
                    return false;
            if (entry.at("is_active") == false && !proven_effect_bypass(entry)) return false;
            const auto& modes = entry.at("modes");
            if (entry.at("class_name") == "Drift") {
                if (!fields(modes, {"voice_mode", "voice_count"}) ||
                    !voice(modes.at("voice_mode")) || !voice(modes.at("voice_count")))
                    return false;
            } else if (entry.at("class_name") == "Eq8") {
                if ((!fields(modes, {"global_mode"}) &&
                     !(fields(modes, {"global_mode", "edit_mode", "oversample"}) &&
                       modes.at("edit_mode").is_boolean() &&
                       modes.at("oversample").is_boolean())) ||
                    !integer(modes.at("global_mode"), 2))
                    return false;
            } else if (!modes.is_object() || !modes.empty())
                return false;
        }
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
bool device_supplement_valid(const json& observation) {
    try {
        if (!observation.is_object()) return false;
        if (!observation.contains("device_identity") &&
            !observation.contains("device_identity_fingerprint"))
            return true;
        if (!observation.contains("device_identity") ||
            !observation.contains("device_identity_fingerprint") ||
            !device_identity_valid(observation.at("device_identity")) ||
            !fingerprint(observation.at("device_identity_fingerprint")))
            return false;
        const auto digest = managed_detail::managed_digest(observation.at("device_identity"));
        if (observation.contains("manifest") &&
            (!observation.at("manifest").is_object() ||
             !observation.at("manifest").contains("devices_empty") ||
             observation.at("manifest").at("devices_empty") !=
                 observation.at("device_identity").at("cohort").empty()))
            return false;
        return digest && observation.at("device_identity_fingerprint") == *digest;
    } catch (const json::exception&) {
        return false;
    }
}
bool device_request_valid(std::string_view method, const json& payload) {
    try {
        if (method == mode_method) {
            if (!fields(payload,
                        {"document_token",
                         "operation_id",
                         "project_key",
                         "binding_key",
                         "expected_content_fingerprint",
                         "expected_device_identity_fingerprint",
                         "device_key",
                         "device",
                         "enum_intents",
                         "property_intents"}))
                return false;
            auto probe = payload;
            probe.erase("enum_intents");
            probe.erase("property_intents");
            probe["physical_intents"] = json::array();
            if (!device_request_valid(update_method, probe) ||
                payload.at("device").at("role") != "effect" ||
                !payload.at("enum_intents").is_array() ||
                !payload.at("property_intents").is_array() ||
                payload.at("enum_intents").size() + payload.at("property_intents").size() == 0 ||
                payload.at("enum_intents").size() + payload.at("property_intents").size() >
                    SUNNY_MANAGED_MAX_DEVICE_TARGETS)
                return false;
            std::set<std::string> ids;
            for (const auto& intent : payload.at("enum_intents")) {
                if (!fields(intent, {"capability_id", "label"}) ||
                    !text(intent.at("capability_id"), 64) || !text(intent.at("label"), 256) ||
                    !ids.insert(intent.at("capability_id").get<std::string>()).second)
                    return false;
                const auto* entry = mode_capability(intent.at("capability_id").get<std::string>());
                if (!entry || payload.at("device").at("class_name") != entry->device_class_name)
                    return false;
            }
            const auto& properties = payload.at("property_intents");
            return properties.empty() ||
                   (payload.at("device").at("class_name") == "Eq8" &&
                    properties ==
                        json::array({json{{"property", "global_mode"}, {"label", "Stereo"}}}));
        }
        if (method == adopt_method) {
            return fields(payload,
                          {"document_token",
                           "operation_id",
                           "project_key",
                           "binding_key",
                           "preview_token",
                           "approved_preview"}) &&
                   key(payload.at("document_token")) && key(payload.at("operation_id")) &&
                   key(payload.at("project_key")) && key(payload.at("binding_key")) &&
                   key(payload.at("preview_token")) &&
                   device_preview_valid(payload.at("approved_preview"));
        }
        if (method == preview_method || method == inspection_method) {
            if (!fields(payload,
                        {"document_token",
                         "project_key",
                         "binding_key",
                         "expected_content_fingerprint",
                         "devices"}) ||
                !key(payload.at("document_token")) || !key(payload.at("project_key")) ||
                !key(payload.at("binding_key")) ||
                !fingerprint(payload.at("expected_content_fingerprint")) ||
                !payload.at("devices").is_array() ||
                payload.at("devices").size() > SUNNY_MANAGED_MAX_DEVICES)
                return false;
            std::set<std::string> keys;
            for (std::size_t index = 0; index < payload.at("devices").size(); ++index) {
                const auto& entry = payload.at("devices")[index];
                if ((!fields(entry, {"device_key", "chain_index", "device", "physical_intents"}) &&
                     !fields(entry,
                             {"device_key",
                              "chain_index",
                              "device",
                              "physical_intents",
                              "enum_intents",
                              "property_intents",
                              "authored_bypass"})) ||
                    !integer(entry.at("chain_index"), index) || entry.at("chain_index") != index ||
                    !key(entry.at("device_key")) ||
                    !keys.insert(entry.at("device_key").get<std::string>()).second ||
                    entry.at("device").at("role") != (index == 0 ? "source" : "effect"))
                    return false;
                const json probe{
                    {"document_token", payload.at("document_token")},
                    {"operation_id", "preview"},
                    {"project_key", payload.at("project_key")},
                    {"binding_key", payload.at("binding_key")},
                    {"expected_content_fingerprint", payload.at("expected_content_fingerprint")},
                    {"expected_device_identity_fingerprint", std::string(64, '0')},
                    {"device_key", entry.at("device_key")},
                    {"device", entry.at("device")},
                    {"physical_intents", entry.at("physical_intents")}};
                if (!device_request_valid(insert_method, probe)) return false;
                if (entry.contains("authored_bypass")) {
                    auto mode_probe = probe;
                    mode_probe.erase("physical_intents");
                    mode_probe["enum_intents"] = entry.at("enum_intents");
                    mode_probe["property_intents"] = entry.at("property_intents");
                    if (!entry.at("authored_bypass").is_boolean() ||
                        !device_request_valid(mode_method, mode_probe))
                        return false;
                    const auto expected = json::array({json{
                        {"capability_id",
                         entry.at("device").at("class_name") == "StereoGain" ? "utility.enabled"
                                                                             : "eq8.enabled"},
                        {"label", "Off"}}});
                    if (entry.at("authored_bypass") == true &&
                        (!entry.at("physical_intents").empty() ||
                         !entry.at("property_intents").empty() ||
                         entry.at("enum_intents") != expected))
                        return false;
                }
            }
            return true;
        }
        if ((method != insert_method && method != update_method) ||
            !fields(payload,
                    {"document_token",
                     "operation_id",
                     "project_key",
                     "binding_key",
                     "expected_content_fingerprint",
                     "expected_device_identity_fingerprint",
                     "device_key",
                     "device",
                     "physical_intents"}))
            return false;
        for (const auto* field :
             {"document_token", "operation_id", "project_key", "binding_key", "device_key"})
            if (!key(payload.at(field))) return false;
        if (!fingerprint(payload.at("expected_content_fingerprint")) ||
            !fingerprint(payload.at("expected_device_identity_fingerprint")) ||
            !descriptor_valid(payload.at("device")) || !payload.at("physical_intents").is_array() ||
            payload.at("physical_intents").size() > SUNNY_MANAGED_MAX_DEVICE_TARGETS)
            return false;
        std::set<std::string> selected;
        for (const auto& intent : payload.at("physical_intents")) {
            if (!fields(intent, {"capability_id", "target", "tolerance"}) ||
                !text(intent.at("capability_id"), 64) || !finite(intent.at("target")) ||
                !finite(intent.at("tolerance")) || intent.at("tolerance").get<double>() < 0.0 ||
                !selected.insert(intent.at("capability_id").get<std::string>()).second)
                return false;
            const auto* entry = capability(intent.at("capability_id").get<std::string>());
            if (!entry || entry->device_class_name !=
                              payload.at("device").at("class_name").get<std::string>())
                return false;
            const double target = intent.at("target");
            const auto unit = *entry->physical_unit;
            if (((unit == Unit::Hertz || unit == Unit::QualityFactor) && target <= 0.0) ||
                ((unit == Unit::Percent || unit == Unit::Milliseconds) && target < 0.0) ||
                (unit == Unit::StereoBalance && (target < -1.0 || target > 1.0)))
                return false;
        }
        return true;
    } catch (const json::exception&) {
        return false;
    }
}
bool device_readonly_body_valid(const json& preview, std::string_view authority_origin) {
    try {
        if (!fields(preview,
                    {"schema_version",
                     "binding_observation",
                     "devices",
                     "resolutions",
                     "authority_origin",
                     "native_mutation_started",
                     "native_knob_only",
                     "host_qualified",
                     "opaque_state_observed"}) ||
            !integer(preview.at("schema_version"), 1) || preview.at("schema_version") != 1 ||
            preview.at("authority_origin") != authority_origin ||
            preview.at("native_mutation_started") != false ||
            preview.at("native_knob_only") != true || preview.at("host_qualified") != false ||
            preview.at("opaque_state_observed") != false)
            return false;
        const auto& observed = preview.at("binding_observation");
        if (!observation_digest(observed) || !device_supplement_valid(observed) ||
            !observed.contains("device_identity") || !observed.contains("track_index") ||
            !integer(observed.at("track_index"), INT32_MAX))
            return false;
        const json probe{{"document_token", "preview"},
                         {"project_key", "preview"},
                         {"binding_key", "preview"},
                         {"expected_content_fingerprint", observed.at("content_fingerprint")},
                         {"devices", preview.at("devices")}};
        if (!device_request_valid(preview_method, probe) || !preview.at("resolutions").is_array())
            return false;
        const auto& cohort = observed.at("device_identity").at("cohort");
        const auto& declarations = preview.at("devices");
        if (cohort.size() != declarations.size()) return false;
        std::size_t resolution_index = 0;
        for (std::size_t index = 0; index < cohort.size(); ++index) {
            const auto& member = cohort[index];
            const auto& declared = declarations[index];
            if (member.at("device_key") != declared.at("device_key")) return false;
            for (const auto* field : {"browser_name", "class_name", "type", "role"})
                if (member.at(field) != declared.at("device").at(field)) return false;
            if (declared.contains("authored_bypass")) {
                if (declared.at("authored_bypass") != !member.at("is_active").get<bool>())
                    return false;
                for (const auto& intent : declared.at("enum_intents")) {
                    const auto* entry =
                        mode_capability(intent.at("capability_id").get<std::string>());
                    if (!entry) return false;
                    const json* parameter = nullptr;
                    for (const auto& candidate : member.at("parameters"))
                        if (candidate.at("name") == entry->parameter_original_name ||
                            candidate.at("original_name") == entry->parameter_original_name) {
                            if (parameter ||
                                candidate.at("original_name") != entry->parameter_original_name)
                                return false;
                            parameter = &candidate;
                        }
                    if (!parameter || !enum_choice(parameter->at("descriptor"),
                                                   *entry->expected_enum_items,
                                                   intent.at("label")))
                        return false;
                    const auto& descriptor = parameter->at("descriptor");
                    if (descriptor.at("value_items")[descriptor.at("value").get<std::size_t>()] !=
                        intent.at("label"))
                        return false;
                }
                for (const auto& intent : declared.at("property_intents")) {
                    (void)intent;
                    if (member.at("modes").at("global_mode") != 0) return false;
                }
            }
            for (const auto& intent : declared.at("physical_intents")) {
                if (resolution_index >= preview.at("resolutions").size()) return false;
                const auto& evidence = preview.at("resolutions")[resolution_index++];
                if (!fields(evidence, {"device_key", "intent", "candidate", "current_readback"}) ||
                    evidence.at("device_key") != member.at("device_key") ||
                    evidence.at("intent") != intent)
                    return false;
                const auto candidate = resolved_candidate(
                    intent, evidence.at("candidate"), observed.at("track_index"), index);
                if (!candidate || !candidate_matches_population(*candidate, member) ||
                    !readback_matches(evidence.at("current_readback"), *candidate, false) ||
                    evidence.at("current_readback").at("internal_value") !=
                        candidate->descriptor.value)
                    return false;
            }
        }
        return resolution_index == preview.at("resolutions").size();
    } catch (const json::exception&) {
        return false;
    }
}
bool device_preview_valid(const json& preview) {
    return device_readonly_body_valid(preview, "explicit_current_device_adoption");
}
bool device_adoption_result_matches_request(const json& payload, const json& result) {
    try {
        if (!device_request_valid(adopt_method, payload) || !observation_digest(result) ||
            !group_payload_closes(result, payload) || !device_supplement_valid(result) ||
            !result.contains("device_adoption"))
            return false;
        const auto& supplement = result.at("device_adoption");
        if (!fields(supplement,
                    {"preview_token",
                     "preview_fingerprint",
                     "authority_origin",
                     "native_mutation_started",
                     "current_values_match_approved_intent",
                     "native_knob_only",
                     "host_qualified",
                     "opaque_state_observed"}) ||
            supplement.at("preview_token") != payload.at("preview_token") ||
            supplement.at("authority_origin") != "explicit_current_device_adoption" ||
            supplement.at("native_mutation_started") != false ||
            supplement.at("current_values_match_approved_intent") != true ||
            supplement.at("native_knob_only") != true || supplement.at("host_qualified") != false ||
            supplement.at("opaque_state_observed") != false)
            return false;
        const auto digest = managed_detail::managed_digest(payload.at("approved_preview"));
        const auto& before = payload.at("approved_preview").at("binding_observation");
        return digest && group_payload_closes(before, payload) &&
               same_group_context(before, result) &&
               supplement.at("preview_fingerprint") == *digest &&
               result.at("manifest") == before.at("manifest") &&
               result.at("note_identity") == before.at("note_identity") &&
               result.at("device_identity") == before.at("device_identity");
    } catch (const json::exception&) {
        return false;
    }
}
bool device_mode_result_matches_request(const json& payload, const json& result) {
    try {
        if (!device_request_valid(mode_method, payload) || !device_supplement_valid(result) ||
            !observation_digest(result) || !group_payload_closes(result, payload) ||
            !result.contains("device_mode_update"))
            return false;
        const auto& update = result.at("device_mode_update");
        if (!fields(update,
                    {"before_observation",
                     "before_device_identity",
                     "before_device_identity_fingerprint",
                     "device_key",
                     "admitted_modes",
                     "admitted_properties",
                     "readbacks",
                     "property_readbacks",
                     "clip_and_note_ids_preserved",
                     "native_knob_only",
                     "host_qualified",
                     "opaque_state_observed"}) ||
            update.at("device_key") != payload.at("device_key") ||
            update.at("clip_and_note_ids_preserved") != true ||
            update.at("native_knob_only") != true || update.at("host_qualified") != false ||
            update.at("opaque_state_observed") != false)
            return false;
        const auto& before = update.at("before_observation");
        if (!observation_digest(before) || !group_payload_closes(before, payload) ||
            !same_group_context(before, result) || !device_supplement_valid(before) ||
            before.at("content_fingerprint") != payload.at("expected_content_fingerprint") ||
            before.at("device_identity_fingerprint") !=
                payload.at("expected_device_identity_fingerprint") ||
            update.at("before_device_identity") != before.at("device_identity") ||
            update.at("before_device_identity_fingerprint") !=
                before.at("device_identity_fingerprint") ||
            !binding_changes(before, result, false))
            return false;
        auto expected = before.at("device_identity");
        const auto& final = result.at("device_identity");
        if (expected.at("cohort").size() != final.at("cohort").size()) return false;
        std::size_t index = expected.at("cohort").size();
        for (std::size_t i = 0; i < expected.at("cohort").size(); ++i)
            if (expected.at("cohort")[i].at("device_key") == payload.at("device_key")) index = i;
        if (index == expected.at("cohort").size()) return false;
        auto& member = expected["cohort"][index];
        const auto& actual = final.at("cohort")[index];
        for (const auto* field : {"browser_name", "class_name", "type", "role"})
            if (member.at(field) != payload.at("device").at(field)) return false;
        if (member.at("class_name") == "Eq8" &&
            !fields(member.at("modes"), {"global_mode", "edit_mode", "oversample"}))
            return false;
        const auto& intents = payload.at("enum_intents");
        const auto& admitted = update.at("admitted_modes");
        const auto& readbacks = update.at("readbacks");
        if (!admitted.is_array() || !readbacks.is_array() || admitted.size() != intents.size() ||
            readbacks.size() != intents.size())
            return false;
        std::set<std::size_t> parameters;
        for (std::size_t i = 0; i < intents.size(); ++i) {
            const auto& item = admitted[i];
            const auto& readback = readbacks[i];
            const auto* entry = mode_capability(intents[i].at("capability_id").get<std::string>());
            if (!entry ||
                !fields(item,
                        {"intent",
                         "parameter_index",
                         "parameter_original_name",
                         "descriptor",
                         "target_internal"}) ||
                !fields(readback,
                        {"capability_id", "parameter_index", "internal_value", "label"}) ||
                item.at("intent") != intents[i] ||
                !integer(item.at("parameter_index"), SUNNY_MANAGED_MAX_DEVICE_PARAMETERS - 1) ||
                !item.at("target_internal").is_number_float() ||
                !finite(item.at("target_internal")) ||
                item.at("parameter_original_name") != entry->parameter_original_name ||
                readback.at("capability_id") != intents[i].at("capability_id") ||
                readback.at("label") != intents[i].at("label") ||
                readback.at("parameter_index") != item.at("parameter_index") ||
                !integer(readback.at("parameter_index"), SUNNY_MANAGED_MAX_DEVICE_PARAMETERS - 1) ||
                !readback.at("internal_value").is_number_float() ||
                readback.at("internal_value") != item.at("target_internal"))
                return false;
            const auto selected = item.at("parameter_index").get<std::size_t>();
            if (selected >= member.at("parameters").size() || !parameters.insert(selected).second)
                return false;
            auto& parameter = member["parameters"][selected];
            std::size_t aliases = 0;
            for (const auto& candidate : member.at("parameters"))
                aliases += candidate.at("name") == entry->parameter_original_name ||
                           candidate.at("original_name") == entry->parameter_original_name;
            if (aliases != 1 || parameter.at("original_name") != entry->parameter_original_name ||
                parameter.at("descriptor") != item.at("descriptor") ||
                !enum_choice(
                    item.at("descriptor"), *entry->expected_enum_items, intents[i].at("label")))
                return false;
            const auto& labels = item.at("descriptor").at("value_items");
            const auto found = std::ranges::find(labels, intents[i].at("label"));
            if (item.at("target_internal") != static_cast<double>(found - labels.begin()))
                return false;
            parameter["descriptor"]["value"] = readback.at("internal_value");
            if (entry->parameter_original_name == "Device On")
                member["is_active"] = intents[i].at("label") == "On";
        }
        const auto& properties = payload.at("property_intents");
        const auto& admitted_properties = update.at("admitted_properties");
        const auto& property_readbacks = update.at("property_readbacks");
        if (!admitted_properties.is_array() || !property_readbacks.is_array() ||
            admitted_properties.size() != properties.size() ||
            property_readbacks.size() != properties.size())
            return false;
        for (std::size_t i = 0; i < properties.size(); ++i) {
            if (!fields(admitted_properties[i], {"intent", "before", "target"}) ||
                admitted_properties[i].at("intent") != properties[i] ||
                !integer(admitted_properties[i].at("before"), 2) ||
                admitted_properties[i].at("before") != member.at("modes").at("global_mode") ||
                !integer(admitted_properties[i].at("target"), 0) ||
                !fields(property_readbacks[i], {"property", "value"}) ||
                property_readbacks[i].at("property") != "global_mode" ||
                !integer(property_readbacks[i].at("value"), 0))
                return false;
            member["modes"]["global_mode"] = 0;
        }
        if (member.at("parameters").size() != actual.at("parameters").size()) return false;
        // Availability can follow the explicit mode phase only on this same Device.
        for (std::size_t i = 0; i < member.at("parameters").size(); ++i)
            for (const auto* field : {"is_enabled", "state"})
                member["parameters"][i]["descriptor"][field] =
                    actual.at("parameters")[i].at("descriptor").at(field);
        return expected == final;
    } catch (const json::exception&) {
        return false;
    }
}
bool device_result_matches_request(std::string_view method,
                                   const json& payload,
                                   const json& result) {
    try {
        if (method == mode_method) return device_mode_result_matches_request(payload, result);
        if ((method != insert_method && method != update_method) ||
            !device_request_valid(method, payload) || !device_supplement_valid(result) ||
            !result.contains("device_identity") || !result.contains("track_index") ||
            !integer(result.at("track_index"), INT32_MAX) || !observation_digest(result) ||
            !group_payload_closes(result, payload) || !result.contains("device_update"))
            return false;
        const auto& update = result.at("device_update");
        if (!fields(update,
                    {"before_observation",
                     "before_device_identity",
                     "before_device_identity_fingerprint",
                     "inserted",
                     "device_key",
                     "resolutions",
                     "readbacks",
                     "clip_and_note_ids_preserved",
                     "output_role_transition",
                     "native_knob_only",
                     "host_qualified",
                     "opaque_state_observed"}) ||
            update.at("inserted") != (method == insert_method) ||
            update.at("device_key") != payload.at("device_key") ||
            update.at("clip_and_note_ids_preserved") != true ||
            update.at("native_knob_only") != true || update.at("host_qualified") != false ||
            update.at("opaque_state_observed") != false)
            return false;
        const auto& before = update.at("before_observation");
        if (!observation_digest(before) || !group_payload_closes(before, payload) ||
            !same_group_context(before, result) || !device_supplement_valid(before) ||
            before.at("content_fingerprint") != payload.at("expected_content_fingerprint") ||
            before.at("device_identity_fingerprint") !=
                payload.at("expected_device_identity_fingerprint") ||
            update.at("before_device_identity") != before.at("device_identity") ||
            update.at("before_device_identity_fingerprint") !=
                before.at("device_identity_fingerprint"))
            return false;
        const bool inserted = method == insert_method;
        const bool source_insert = inserted && payload.at("device").at("role") == "source";
        if (update.at("output_role_transition") != source_insert ||
            !binding_changes(before, result, source_insert))
            return false;
        const auto& old = before.at("device_identity").at("cohort");
        auto final = result.at("device_identity").at("cohort");
        if (final.size() != old.size() + (inserted ? 1 : 0)) return false;
        std::size_t selected = final.size();
        for (std::size_t i = 0; i < final.size(); ++i)
            if (final[i].at("device_key") == payload.at("device_key")) selected = i;
        if (selected == final.size() || (inserted && selected != old.size())) return false;
        const auto& member = final[selected];
        for (const auto* name : {"browser_name", "class_name", "type", "role"})
            if (member.at(name) != payload.at("device").at(name)) return false;
        const auto& intents = payload.at("physical_intents");
        const auto& resolutions = update.at("resolutions");
        const auto& readbacks = update.at("readbacks");
        if (!resolutions.is_array() || !readbacks.is_array() ||
            resolutions.size() != intents.size() || readbacks.size() != intents.size())
            return false;
        auto preflight_member = member;
        std::set<std::size_t> target_indices;
        for (std::size_t i = 0; i < intents.size(); ++i) {
            const auto& resolution = resolutions[i];
            const auto& readback = readbacks[i];
            if (!fields(resolution, {"intent", "candidate"}) ||
                resolution.at("intent") != intents[i] ||
                !fields(readback,
                        {"capability_id",
                         "internal_value",
                         "display",
                         "display_value",
                         "display_increment",
                         "absolute_display_error",
                         "matches_intent",
                         "formatter_calls"}) ||
                readback.at("capability_id") != intents[i].at("capability_id") ||
                readback.at("matches_intent") != true || readback.at("formatter_calls") != 1 ||
                !finite(readback.at("internal_value")) || !finite(readback.at("display_value")) ||
                !finite(readback.at("absolute_display_error")) || !text(readback.at("display"), 80))
                return false;
            const auto path = LomPath{{"song",
                                       "tracks",
                                       std::to_string(result.at("track_index").get<int>()),
                                       "devices",
                                       std::to_string(selected)}};
            auto query = make_native_display_resolution_request(path,
                                                                intents[i].at("capability_id"),
                                                                intents[i].at("target"),
                                                                intents[i].at("tolerance"));
            if (!query) return false;
            const json envelope{{"schema_version", 1},
                                {"capability_id", intents[i].at("capability_id")},
                                {"target", intents[i].at("target")},
                                {"tolerance", intents[i].at("tolerance")},
                                {"outcome", "resolved"},
                                {"candidate", resolution.at("candidate")}};
            const auto parsed = parse_native_display_resolution(*query, envelope);
            if (!parsed || !parsed->candidate) return false;
            const auto& candidate = *parsed->candidate;
            if (!target_indices.insert(candidate.parameter_index).second ||
                candidate.parameter_index >= member.at("parameters").size())
                return false;
            preflight_member["parameters"][candidate.parameter_index]["descriptor"]["value"] =
                candidate.descriptor.value;
            if (!readback_matches(readback, candidate, true) ||
                member.at("parameters")[candidate.parameter_index].at("descriptor").at("value") !=
                    readback.at("internal_value"))
                return false;
        }
        // Every candidate belongs to the same pre-setter snapshot; a prior
        // target's write cannot have been used to resolve a later target.
        for (std::size_t i = 0; i < intents.size(); ++i) {
            const auto path = LomPath{{"song",
                                       "tracks",
                                       std::to_string(result.at("track_index").get<int>()),
                                       "devices",
                                       std::to_string(selected)}};
            const auto query =
                make_native_display_resolution_request(path,
                                                       intents[i].at("capability_id"),
                                                       intents[i].at("target"),
                                                       intents[i].at("tolerance"));
            const auto parsed =
                parse_native_display_resolution(*query,
                                                {{"schema_version", 1},
                                                 {"capability_id", intents[i].at("capability_id")},
                                                 {"target", intents[i].at("target")},
                                                 {"tolerance", intents[i].at("tolerance")},
                                                 {"outcome", "resolved"},
                                                 {"candidate", resolutions[i].at("candidate")}});
            if (!candidate_matches_population(*parsed->candidate, preflight_member)) return false;
        }
        if (inserted)
            final.erase(final.begin() + static_cast<json::difference_type>(selected));
        else
            final[selected] = preflight_member;
        return final == old;
    } catch (const json::exception&) {
        return false;
    }
}
} // namespace managed_device_detail

Result<LomRequest>
make_managed_device_insert_request(const ManagedBridgeContext& context,
                                   const std::string& operation_id,
                                   const ManagedBindingReceipt& binding,
                                   const std::string& device_key,
                                   ManagedNativeDevice device,
                                   std::span<const ManagedDevicePhysicalIntent> intents) {
    return make_request(insert_method, context, operation_id, binding, device_key, device, intents);
}
Result<LomRequest>
make_managed_device_update_request(const ManagedBridgeContext& context,
                                   const std::string& operation_id,
                                   const ManagedBindingReceipt& binding,
                                   const std::string& device_key,
                                   ManagedNativeDevice device,
                                   std::span<const ManagedDevicePhysicalIntent> intents) {
    return make_request(update_method, context, operation_id, binding, device_key, device, intents);
}
namespace {
Result<LomRequest>
make_device_readonly_request(const ManagedBridgeContext& context,
                             const ManagedBindingReceipt& binding,
                             std::span<const ManagedDeviceAdoptionSelection> selections,
                             std::string_view method) {
    if (!key(context.bridge_instance) ||
        context.bridge_instance != binding.context.bridge_instance ||
        context.document_token != binding.context.document_token ||
        !observation_digest(binding.observation) ||
        !managed_detail::group_touched_boundary(
            binding.observation, context, binding.project_key, binding.binding_key))
        return std::unexpected(ErrorCode::ProtocolError);
    json devices = json::array();
    for (const auto& selection : selections) {
        json intents = json::array();
        for (const auto& intent : selection.physical_intents)
            intents.push_back({{"capability_id", intent.capability_id},
                               {"target", intent.target},
                               {"tolerance", intent.tolerance}});
        devices.push_back({{"device_key", selection.device_key},
                           {"chain_index", selection.chain_index},
                           {"device", device_descriptor(selection.device)},
                           {"physical_intents", std::move(intents)}});
        if (!selection.enum_intents.empty() || !selection.property_intents.empty() ||
            selection.authored_bypass) {
            auto& added = devices.back();
            added["enum_intents"] = json::array();
            added["property_intents"] = json::array();
            for (const auto& intent : selection.enum_intents)
                added["enum_intents"].push_back(
                    {{"capability_id", intent.capability_id}, {"label", intent.label}});
            for (const auto& intent : selection.property_intents)
                added["property_intents"].push_back(
                    {{"property", intent.property}, {"label", intent.label}});
            added["authored_bypass"] = selection.authored_bypass;
        }
    }
    json payload{{"document_token", context.document_token},
                 {"project_key", binding.project_key},
                 {"binding_key", binding.binding_key},
                 {"expected_content_fingerprint", binding.observation.at("content_fingerprint")},
                 {"devices", std::move(devices)}};
    if (!managed_device_detail::device_request_valid(method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    return LomProtocol::call_method(LomPaths::song(), std::string{method}, {std::move(payload)});
}
} // namespace
Result<LomRequest>
make_managed_device_preview_request(const ManagedBridgeContext& context,
                                    const ManagedBindingReceipt& binding,
                                    std::span<const ManagedDeviceAdoptionSelection> selections) {
    return make_device_readonly_request(context, binding, selections, preview_method);
}
Result<LomRequest>
make_managed_device_inspection_request(const ManagedBridgeContext& context,
                                       const ManagedBindingReceipt& binding,
                                       std::span<const ManagedDeviceAdoptionSelection> selections) {
    return make_device_readonly_request(context, binding, selections, inspection_method);
}
Result<json> parse_managed_device_inspection(const LomRequest& request,
                                             const ManagedBridgeContext& context,
                                             const json& response) {
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != inspection_method || request.args.size() != 1 ||
            !std::holds_alternative<json>(request.args[0]) || !key(context.bridge_instance) ||
            !fields(response,
                    {"outcome", "document_token", "project_key", "binding_key", "inspection"}) ||
            response.at("outcome") != "observed")
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args[0]);
        const auto& body = response.at("inspection");
        if (!managed_device_detail::device_request_valid(inspection_method, payload) ||
            context.document_token != payload.at("document_token").get<std::string>() ||
            !managed_device_detail::device_readonly_body_valid(body, "none"))
            return std::unexpected(ErrorCode::ProtocolError);
        for (const auto* field : {"document_token", "project_key", "binding_key"})
            if (response.at(field) != payload.at(field))
                return std::unexpected(ErrorCode::ProtocolError);
        const ManagedBindingReceipt observed{context,
                                             payload.at("project_key"),
                                             payload.at("binding_key"),
                                             body.at("binding_observation")};
        if (!managed_binding_from_json(managed_binding_to_json(observed)) ||
            !managed_detail::group_touched_boundary(
                observed.observation, context, observed.project_key, observed.binding_key) ||
            body.at("devices") != payload.at("devices") ||
            observed.observation.at("content_fingerprint") !=
                payload.at("expected_content_fingerprint"))
            return std::unexpected(ErrorCode::ProtocolError);
        return response;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<ManagedDeviceAdoptionPreview> parse_managed_device_preview(
    const LomRequest& request, const ManagedBridgeContext& context, const json& response) {
    try {
        if (request.type != LomRequestType::CallMethod ||
            request.path.segments != LomPaths::song().segments ||
            request.property_or_method != preview_method || request.args.size() != 1 ||
            !std::holds_alternative<json>(request.args[0]) || !key(context.bridge_instance))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& payload = std::get<json>(request.args[0]);
        if (!managed_device_detail::device_request_valid(preview_method, payload) ||
            context.document_token != payload.at("document_token").get<std::string>() ||
            !fields(response,
                    {"outcome",
                     "document_token",
                     "project_key",
                     "binding_key",
                     "preview_token",
                     "preview_fingerprint",
                     "preview"}) ||
            response.at("outcome") != "previewed" || !key(response.at("preview_token")) ||
            !managed_device_detail::device_preview_valid(response.at("preview")))
            return std::unexpected(ErrorCode::ProtocolError);
        for (const auto* field : {"document_token", "project_key", "binding_key"})
            if (response.at(field) != payload.at(field))
                return std::unexpected(ErrorCode::ProtocolError);
        if (!managed_detail::group_touched_boundary(
                response.at("preview").at("binding_observation"),
                context,
                payload.at("project_key"),
                payload.at("binding_key")) ||
            response.at("preview").at("devices") != payload.at("devices") ||
            response.at("preview").at("binding_observation").at("content_fingerprint") !=
                payload.at("expected_content_fingerprint"))
            return std::unexpected(ErrorCode::ProtocolError);
        const auto digest = managed_detail::managed_digest(response.at("preview"));
        if (!digest || response.at("preview_fingerprint") != *digest)
            return std::unexpected(ErrorCode::ProtocolError);
        return ManagedDeviceAdoptionPreview{context,
                                            payload.at("project_key"),
                                            payload.at("binding_key"),
                                            response.at("preview_token"),
                                            *digest,
                                            response.at("preview")};
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
}
Result<LomRequest>
make_managed_device_mode_request(const ManagedBridgeContext& context,
                                 const std::string& operation_id,
                                 const ManagedBindingReceipt& binding,
                                 const std::string& device_key,
                                 ManagedNativeDevice device,
                                 std::span<const ManagedDeviceModeIntent> enum_intents,
                                 std::span<const ManagedDevicePropertyIntent> property_intents) {
    const auto base =
        make_request(update_method, context, operation_id, binding, device_key, device, {});
    if (!base) return std::unexpected(base.error());
    auto payload = std::get<json>(base->args[0]);
    payload.erase("physical_intents");
    payload["enum_intents"] = json::array();
    payload["property_intents"] = json::array();
    for (const auto& intent : enum_intents)
        payload["enum_intents"].push_back(
            {{"capability_id", intent.capability_id}, {"label", intent.label}});
    for (const auto& intent : property_intents)
        payload["property_intents"].push_back(
            {{"property", intent.property}, {"label", intent.label}});
    if (!managed_device_detail::device_request_valid(mode_method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    const auto& cohort = binding.observation.at("device_identity").at("cohort");
    const auto found = std::ranges::find_if(
        cohort, [&](const auto& entry) { return entry.at("device_key") == device_key; });
    if (found == cohort.end() ||
        (device == ManagedNativeDevice::EqEight &&
         !fields(found->at("modes"), {"global_mode", "edit_mode", "oversample"})))
        return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::operation_reservation_fits(payload, mode_method))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{mode_method}, {std::move(payload)});
}
Result<LomRequest>
make_managed_device_adoption_request(const ManagedBridgeContext& context,
                                     const std::string& operation_id,
                                     const ManagedDeviceAdoptionPreview& preview) {
    if (context.bridge_instance != preview.context.bridge_instance ||
        context.document_token != preview.context.document_token || !key(context.bridge_instance) ||
        !preview.approved_preview.is_object() ||
        !preview.approved_preview.contains("binding_observation") ||
        !managed_detail::group_touched_boundary(preview.approved_preview.at("binding_observation"),
                                                context,
                                                preview.project_key,
                                                preview.binding_key) ||
        managed_detail::managed_digest(preview.approved_preview) !=
            std::optional{preview.preview_fingerprint})
        return std::unexpected(ErrorCode::ProtocolError);
    json payload{{"document_token", context.document_token},
                 {"operation_id", operation_id},
                 {"project_key", preview.project_key},
                 {"binding_key", preview.binding_key},
                 {"preview_token", preview.preview_token},
                 {"approved_preview", preview.approved_preview}};
    if (!managed_device_detail::device_request_valid(adopt_method, payload))
        return std::unexpected(ErrorCode::ProtocolError);
    if (!managed_detail::operation_reservation_fits(payload, adopt_method))
        return std::unexpected(ErrorCode::ManagedReplyCapacityExceeded);
    return LomProtocol::call_method(
        LomPaths::song(), std::string{adopt_method}, {std::move(payload)});
}
} // namespace sunny::infrastructure
