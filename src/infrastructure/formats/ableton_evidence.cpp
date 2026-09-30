/**
 * @file ableton_evidence.cpp
 * @brief Shared current-protocol observation and set/readback evidence implementation
 */

#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <sunny/infrastructure/formats/ableton_evidence.hpp>

namespace sunny::infrastructure::formats {

using namespace sunny::core;

namespace {

nlohmann::json lom_value_j(const LomValue& value) {
    return std::visit([](const auto& item) { return nlohmann::json(item); }, value);
}

bool equivalent_readback(const nlohmann::json& expected, const nlohmann::json& observed) {
    if ((expected.is_number_integer() || expected.is_number_unsigned()) &&
        (observed.is_number_integer() || observed.is_number_unsigned()))
        return expected == observed;
    if (expected.is_number_float() && observed.is_number_float()) {
        const double left = expected.get<double>();
        const double right = observed.get<double>();
        const double tolerance = std::max(1.0e-9, std::abs(left) * 1.0e-6);
        return std::isfinite(left) && std::isfinite(right) && std::abs(left - right) <= tolerance;
    }
    return expected == observed;
}

bool compatible_readback_type(const nlohmann::json& expected, const nlohmann::json& observed) {
    if (expected.is_number_integer() || expected.is_number_unsigned())
        return observed.is_number_integer() || observed.is_number_unsigned();
    if (expected.is_number_float())
        return observed.is_number_float() && std::isfinite(observed.get<double>());
    if (expected.is_boolean()) return observed.is_boolean();
    if (expected.is_string()) return observed.is_string();
    if (expected.is_array()) return observed.is_array();
    if (expected.is_object()) return observed.is_object();
    return expected.is_null() && observed.is_null();
}

bool valid_device_parameter_state(const nlohmann::json& value) {
    if (value.is_number_unsigned()) return value.get<std::uint64_t>() <= 2;
    return value.is_number_integer() && value.get<std::int64_t>() >= 0 &&
           value.get<std::int64_t>() <= 2;
}

struct DeviceParameterDomain {
    std::optional<float> default_value;
    std::optional<std::vector<std::string>> value_items;
};

std::optional<DeviceParameterDomain> parse_device_parameter_domain(const nlohmann::json& encoded,
                                                                   bool is_quantized,
                                                                   float minimum,
                                                                   float maximum) {
    DeviceParameterDomain domain;
    if (is_quantized) {
        if (!encoded.at("default_value").is_null() || !encoded.at("value_items").is_array())
            return std::nullopt;
        std::vector<std::string> items;
        items.reserve(encoded.at("value_items").size());
        for (const auto& item : encoded.at("value_items")) {
            if (!item.is_string()) return std::nullopt;
            items.push_back(item.get<std::string>());
        }
        domain.value_items = std::move(items);
        return domain;
    }

    if (!encoded.at("default_value").is_number_float() ||
        !std::isfinite(encoded.at("default_value").get<double>()) ||
        !encoded.at("value_items").is_null())
        return std::nullopt;
    const float default_value = encoded.at("default_value").get<float>();
    if (default_value < minimum || default_value > maximum) return std::nullopt;
    domain.default_value = default_value;
    return domain;
}

bool exact_fields(const nlohmann::json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

std::optional<std::uint32_t> json_u32(const nlohmann::json& value) {
    if (value.is_number_unsigned()) {
        const auto item = value.get<std::uint64_t>();
        if (item <= std::numeric_limits<std::uint32_t>::max())
            return static_cast<std::uint32_t>(item);
        return std::nullopt;
    }
    if (!value.is_number_integer()) return std::nullopt;
    const auto item = value.get<std::int64_t>();
    if (item < 0 || static_cast<std::uint64_t>(item) > std::numeric_limits<std::uint32_t>::max())
        return std::nullopt;
    return static_cast<std::uint32_t>(item);
}

std::optional<std::uint8_t> json_device_type(const nlohmann::json& value) {
    const auto item = json_u32(value);
    if (!item || (*item != 0 && *item != 1 && *item != 2 && *item != 4)) return std::nullopt;
    return static_cast<std::uint8_t>(*item);
}

} // namespace

const char* ableton_parameter_action_name(AbletonParameterAction action) {
    switch (action) {
    case AbletonParameterAction::NotApplied:
        return "not_applied";
    case AbletonParameterAction::RecordedOnly:
        return "recorded_only";
    case AbletonParameterAction::Set:
        return "set";
    }
    return "not_applied";
}

Result<AbletonPropertyDeployment> set_property_with_readback(const LomPath& path,
                                                             const std::string& property,
                                                             const LomValue& value,
                                                             LomTransport& transport) {
    const auto requested = lom_value_j(value);
    auto response = transport.send(LomProtocol::set_property(path, property, value));
    if (!response.success) return std::unexpected(ErrorCode::SendFailed);

    AbletonPropertyDeployment deployment{
        path.to_string(), property, requested, std::nullopt, false};
    if (!response.value) {
        if (!transport.records_without_execution())
            return std::unexpected(ErrorCode::ProtocolError);
        return deployment;
    }

    const auto* evidence = std::get_if<nlohmann::json>(&*response.value);
    if (evidence == nullptr || !exact_fields(*evidence, {"property", "requested", "observed"}) ||
        !evidence->at("property").is_string() || evidence->at("property") != property ||
        !compatible_readback_type(requested, evidence->at("requested")) ||
        !equivalent_readback(requested, evidence->at("requested")) ||
        !compatible_readback_type(requested, evidence->at("observed")))
        return std::unexpected(ErrorCode::ProtocolError);

    deployment.observed = evidence->at("observed");
    deployment.verified = equivalent_readback(requested, *deployment.observed);
    return deployment;
}

Result<AbletonDeviceInsertionDeployment> insert_device_with_readback(const LomPath& track_path,
                                                                     const std::string& device_name,
                                                                     std::uint32_t target_index,
                                                                     std::uint8_t expected_type,
                                                                     LomTransport& transport) {
    if (device_name.empty() ||
        target_index > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        (expected_type != 1 && expected_type != 2))
        return std::unexpected(ErrorCode::ProtocolError);

    auto response = transport.send(LomProtocol::call_method(
        track_path, "insert_device", {device_name, static_cast<int>(target_index)}));
    if (!response.success) return std::unexpected(ErrorCode::SendFailed);

    AbletonDeviceInsertionDeployment deployment;
    deployment.device_path =
        track_path.child("devices").child(static_cast<int>(target_index)).to_string();
    deployment.requested_name = device_name;
    deployment.requested_index = target_index;
    deployment.requested_type = expected_type;
    if (!response.value) {
        if (!transport.records_without_execution())
            return std::unexpected(ErrorCode::ProtocolError);
        return deployment;
    }

    const auto* encoded = std::get_if<nlohmann::json>(&*response.value);
    if (encoded == nullptr ||
        !exact_fields(*encoded,
                      {"requested_name",
                       "requested_index",
                       "before_count",
                       "after_count",
                       "device_index",
                       "name",
                       "class_display_name",
                       "class_name",
                       "type",
                       "is_active",
                       "can_have_chains",
                       "latency_in_samples",
                       "latency_in_ms",
                       "track_has_audio_output",
                       "track_has_midi_output"}) ||
        !encoded->at("requested_name").is_string() ||
        encoded->at("requested_name").get<std::string>() != device_name ||
        !encoded->at("name").is_string() || !encoded->at("class_display_name").is_string() ||
        !encoded->at("class_name").is_string() || !encoded->at("is_active").is_boolean() ||
        !encoded->at("can_have_chains").is_boolean() ||
        !encoded->at("latency_in_ms").is_number_float() ||
        !std::isfinite(encoded->at("latency_in_ms").get<double>()) ||
        encoded->at("latency_in_ms").get<double>() < 0.0 ||
        !encoded->at("track_has_audio_output").is_boolean() ||
        !encoded->at("track_has_midi_output").is_boolean())
        return std::unexpected(ErrorCode::ProtocolError);

    const auto requested_index = json_u32(encoded->at("requested_index"));
    const auto before_count = json_u32(encoded->at("before_count"));
    const auto after_count = json_u32(encoded->at("after_count"));
    const auto device_index = json_u32(encoded->at("device_index"));
    const auto observed_type = json_device_type(encoded->at("type"));
    const auto observed_latency_in_samples = json_u32(encoded->at("latency_in_samples"));
    if (!requested_index || !before_count || !after_count || !device_index || !observed_type ||
        !observed_latency_in_samples ||
        *observed_latency_in_samples >
            static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
        *requested_index != target_index || *device_index != target_index ||
        target_index != *before_count ||
        *before_count == std::numeric_limits<std::uint32_t>::max() ||
        *after_count != *before_count + 1)
        return std::unexpected(ErrorCode::ProtocolError);

    deployment.before_count = *before_count;
    deployment.after_count = *after_count;
    deployment.observed_name = encoded->at("name").get<std::string>();
    deployment.observed_class_display_name = encoded->at("class_display_name").get<std::string>();
    deployment.observed_class_name = encoded->at("class_name").get<std::string>();
    deployment.observed_type = *observed_type;
    deployment.observed_active = encoded->at("is_active").get<bool>();
    deployment.observed_can_have_chains = encoded->at("can_have_chains").get<bool>();
    deployment.observed_latency_in_samples = *observed_latency_in_samples;
    deployment.observed_latency_in_ms = encoded->at("latency_in_ms").get<double>();
    deployment.reported_latency_observed = true;
    deployment.observed_track_has_audio_output = encoded->at("track_has_audio_output").get<bool>();
    deployment.observed_track_has_midi_output = encoded->at("track_has_midi_output").get<bool>();
    deployment.identity_verified = *deployment.observed_class_display_name == device_name;
    deployment.type_verified = *deployment.observed_type == expected_type;
    deployment.flat_device_verified = !*deployment.observed_can_have_chains;
    deployment.output_verified =
        *deployment.observed_track_has_audio_output && !*deployment.observed_track_has_midi_output;
    deployment.verified = deployment.identity_verified && deployment.type_verified &&
                          deployment.flat_device_verified && *deployment.observed_active &&
                          deployment.output_verified && deployment.reported_latency_observed;
    return deployment;
}

Result<AbletonDeviceParameterEvidence>
set_device_parameter_with_readback(const LomPath& device_path,
                                   const std::string& parameter_name,
                                   DeviceParameterValueProperty value_property,
                                   float range_min,
                                   float range_max,
                                   float value,
                                   LomTransport& transport) {
    if (parameter_name.empty() || !std::isfinite(range_min) || !std::isfinite(range_max) ||
        !std::isfinite(value) || !(range_max > range_min))
        return std::unexpected(ErrorCode::ProtocolError);
    const std::string property =
        value_property == DeviceParameterValueProperty::DisplayValue ? "display_value" : "value";
    auto response = transport.send(LomProtocol::call_method(device_path,
                                                            "sunny_set_device_parameter",
                                                            {parameter_name,
                                                             static_cast<double>(value),
                                                             property,
                                                             static_cast<double>(range_min),
                                                             static_cast<double>(range_max)}));
    if (!response.success) return std::unexpected(ErrorCode::SendFailed);

    AbletonDeviceParameterEvidence evidence;
    evidence.device_path = device_path.to_string();
    evidence.requested_name = parameter_name;
    evidence.value_property = value_property;
    evidence.requested = value;
    if (!response.value) {
        if (!transport.records_without_execution())
            return std::unexpected(ErrorCode::ProtocolError);
        evidence.action = AbletonParameterAction::RecordedOnly;
        return evidence;
    }

    const auto* encoded = std::get_if<nlohmann::json>(&*response.value);
    if (encoded == nullptr ||
        !exact_fields(*encoded,
                      {"matched_name",
                       "original_name",
                       "property",
                       "requested",
                       "observed",
                       "minimum",
                       "maximum",
                       "is_quantized",
                       "default_value",
                       "value_items",
                       "is_enabled",
                       "state",
                       "automation_state"}) ||
        !encoded->at("matched_name").is_string() || !encoded->at("original_name").is_string() ||
        !encoded->at("property").is_string() || !encoded->at("requested").is_number_float() ||
        !encoded->at("observed").is_number_float() || !encoded->at("minimum").is_number_float() ||
        !encoded->at("maximum").is_number_float() || !encoded->at("is_quantized").is_boolean() ||
        !encoded->at("is_enabled").is_boolean() ||
        !valid_device_parameter_state(encoded->at("state")) ||
        !valid_device_parameter_state(encoded->at("automation_state")))
        return std::unexpected(ErrorCode::ProtocolError);

    evidence.matched_name = encoded->at("matched_name").get<std::string>();
    evidence.original_name = encoded->at("original_name").get<std::string>();
    if ((*evidence.matched_name != parameter_name && *evidence.original_name != parameter_name) ||
        encoded->at("property").get<std::string>() != property)
        return std::unexpected(ErrorCode::ProtocolError);

    const float echoed = encoded->at("requested").get<float>();
    const float observed = encoded->at("observed").get<float>();
    const float minimum = encoded->at("minimum").get<float>();
    const float maximum = encoded->at("maximum").get<float>();
    const float request_tolerance = std::max(1.0e-6f, std::abs(value) * 1.0e-6f);
    if (!std::isfinite(echoed) || !std::isfinite(observed) || !std::isfinite(minimum) ||
        !std::isfinite(maximum) || maximum < minimum ||
        (property == "value" && (observed < minimum || observed > maximum)) ||
        std::abs(echoed - value) > request_tolerance)
        return std::unexpected(ErrorCode::ProtocolError);
    if (value_property == DeviceParameterValueProperty::InternalValue) {
        const float range_tolerance =
            std::max(1.0e-6f, std::max(std::abs(range_min), std::abs(range_max)) * 1.0e-6f);
        if (std::abs(minimum - range_min) > range_tolerance ||
            std::abs(maximum - range_max) > range_tolerance)
            return std::unexpected(ErrorCode::ProtocolError);
    }

    const bool is_quantized = encoded->at("is_quantized").get<bool>();
    const auto domain = parse_device_parameter_domain(*encoded, is_quantized, minimum, maximum);
    if (!domain) return std::unexpected(ErrorCode::ProtocolError);

    evidence.observed = observed;
    evidence.minimum = minimum;
    evidence.maximum = maximum;
    evidence.is_quantized = is_quantized;
    evidence.default_value = domain->default_value;
    evidence.value_items = domain->value_items;
    evidence.is_enabled = encoded->at("is_enabled").get<bool>();
    evidence.state = encoded->at("state").get<std::uint8_t>();
    evidence.automation_state = encoded->at("automation_state").get<std::uint8_t>();
    evidence.verified = std::abs(observed - value) <= request_tolerance;
    evidence.action = AbletonParameterAction::Set;
    return evidence;
}

Result<AbletonDeviceParameterObservation>
observe_device_parameter(const LomPath& device_path,
                         const std::string& parameter_name,
                         DeviceParameterValueProperty value_property,
                         LomTransport& transport) {
    if (parameter_name.empty()) return std::unexpected(ErrorCode::ProtocolError);
    const std::string property =
        value_property == DeviceParameterValueProperty::DisplayValue ? "display_value" : "value";
    auto response = transport.send(LomProtocol::call_method(
        device_path, "sunny_get_device_parameter", {parameter_name, property}));
    if (!response.success) return std::unexpected(ErrorCode::SendFailed);

    AbletonDeviceParameterObservation observation;
    observation.device_path = device_path.to_string();
    observation.requested_name = parameter_name;
    observation.value_property = value_property;
    if (!response.value) {
        if (!transport.records_without_execution())
            return std::unexpected(ErrorCode::ProtocolError);
        return observation;
    }

    const auto* encoded = std::get_if<nlohmann::json>(&*response.value);
    if (encoded == nullptr ||
        !exact_fields(*encoded,
                      {"matched_name",
                       "original_name",
                       "property",
                       "observed",
                       "minimum",
                       "maximum",
                       "is_quantized",
                       "default_value",
                       "value_items",
                       "is_enabled",
                       "state",
                       "automation_state"}) ||
        !encoded->at("matched_name").is_string() || !encoded->at("original_name").is_string() ||
        !encoded->at("property").is_string() || !encoded->at("observed").is_number_float() ||
        !encoded->at("minimum").is_number_float() || !encoded->at("maximum").is_number_float() ||
        !encoded->at("is_quantized").is_boolean() || !encoded->at("is_enabled").is_boolean() ||
        !valid_device_parameter_state(encoded->at("state")) ||
        !valid_device_parameter_state(encoded->at("automation_state")))
        return std::unexpected(ErrorCode::ProtocolError);

    observation.matched_name = encoded->at("matched_name").get<std::string>();
    observation.original_name = encoded->at("original_name").get<std::string>();
    if ((*observation.matched_name != parameter_name &&
         *observation.original_name != parameter_name) ||
        encoded->at("property").get<std::string>() != property)
        return std::unexpected(ErrorCode::ProtocolError);

    const float observed = encoded->at("observed").get<float>();
    const float minimum = encoded->at("minimum").get<float>();
    const float maximum = encoded->at("maximum").get<float>();
    if (!std::isfinite(observed) || !std::isfinite(minimum) || !std::isfinite(maximum) ||
        maximum < minimum || (property == "value" && (observed < minimum || observed > maximum)))
        return std::unexpected(ErrorCode::ProtocolError);

    const bool is_quantized = encoded->at("is_quantized").get<bool>();
    const auto domain = parse_device_parameter_domain(*encoded, is_quantized, minimum, maximum);
    if (!domain) return std::unexpected(ErrorCode::ProtocolError);

    observation.observed = observed;
    observation.minimum = minimum;
    observation.maximum = maximum;
    observation.is_quantized = is_quantized;
    observation.default_value = domain->default_value;
    observation.value_items = domain->value_items;
    observation.is_enabled = encoded->at("is_enabled").get<bool>();
    observation.state = encoded->at("state").get<std::uint8_t>();
    observation.automation_state = encoded->at("automation_state").get<std::uint8_t>();
    return observation;
}

} // namespace sunny::infrastructure::formats
