/**
 * @file ableton_evidence.hpp
 * @brief Shared current-protocol observation and set/readback evidence for Ableton compilers
 */

#pragma once

#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sunny/core/timbre/types.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

enum class AbletonParameterAction : std::uint8_t { NotApplied, RecordedOnly, Set };

[[nodiscard]] const char* ableton_parameter_action_name(AbletonParameterAction action);

struct AbletonPropertyDeployment {
    std::string path;
    std::string property;
    nlohmann::json requested;
    std::optional<nlohmann::json> observed;
    bool verified = false;
};

struct AbletonDeviceParameterEvidence {
    std::string device_path;
    std::string requested_name;
    std::optional<std::string> matched_name;
    std::optional<std::string> original_name;
    sunny::core::DeviceParameterValueProperty value_property =
        sunny::core::DeviceParameterValueProperty::InternalValue;
    float requested = 0.0f;
    std::optional<float> observed;
    std::optional<float> minimum;
    std::optional<float> maximum;
    std::optional<bool> is_quantized;
    /** Present only for a non-quantized DeviceParameter. */
    std::optional<float> default_value;
    /** Present only for a quantized DeviceParameter; labels remain opaque. */
    std::optional<std::vector<std::string>> value_items;
    std::optional<bool> is_enabled;
    std::optional<std::uint8_t> state;
    std::optional<std::uint8_t> automation_state;
    bool verified = false;
    AbletonParameterAction action = AbletonParameterAction::NotApplied;
};

struct AbletonDeviceParameterObservation {
    std::string device_path;
    std::string requested_name;
    std::optional<std::string> matched_name;
    std::optional<std::string> original_name;
    sunny::core::DeviceParameterValueProperty value_property =
        sunny::core::DeviceParameterValueProperty::InternalValue;
    std::optional<float> observed;
    std::optional<float> minimum;
    std::optional<float> maximum;
    std::optional<bool> is_quantized;
    /** Present only for a non-quantized DeviceParameter. */
    std::optional<float> default_value;
    /** Present only for a quantized DeviceParameter; labels remain opaque. */
    std::optional<std::vector<std::string>> value_items;
    std::optional<bool> is_enabled;
    std::optional<std::uint8_t> state;
    std::optional<std::uint8_t> automation_state;
};

struct AbletonDeviceInsertionDeployment {
    std::string device_path;
    std::string requested_name;
    std::uint32_t requested_index = 0;
    std::uint8_t requested_type = 0;
    std::optional<std::uint32_t> before_count;
    std::optional<std::uint32_t> after_count;
    std::optional<std::string> observed_name;
    std::optional<std::string> observed_class_display_name;
    std::optional<std::string> observed_class_name;
    std::optional<std::uint8_t> observed_type;
    std::optional<bool> observed_active;
    std::optional<bool> observed_can_have_chains;
    std::optional<std::uint32_t> observed_latency_in_samples;
    std::optional<double> observed_latency_in_ms;
    bool reported_latency_observed = false;
    /** Per-device reports omit compensation, routing, buffers, drivers, and external delay. */
    bool render_path_latency_fully_observed = false;
    std::optional<bool> observed_track_has_audio_output;
    std::optional<bool> observed_track_has_midi_output;
    bool identity_verified = false;
    bool type_verified = false;
    /** Sunny's device intent is serial and has no Rack-owned parallel/nested chain carrier. */
    bool flat_device_verified = false;
    bool output_verified = false;
    bool verified = false;
};

/**
 * Set one public LOM property and validate current-protocol readback evidence.
 *
 * A recording-only transport returns a deployment with no observation. A real
 * transport must return the exact property, an equivalent echo of the request,
 * and an observed value. Observed divergence is evidence, not a protocol error.
 */
[[nodiscard]] sunny::core::Result<AbletonPropertyDeployment>
set_property_with_readback(const LomPath& path,
                           const std::string& property,
                           const LomValue& value,
                           LomTransport& transport);

/** Insert one native device and validate exact identity/type/activity evidence. */
[[nodiscard]] sunny::core::Result<AbletonDeviceInsertionDeployment>
insert_device_with_readback(const LomPath& track_path,
                            const std::string& device_name,
                            std::uint32_t target_index,
                            std::uint8_t expected_type,
                            LomTransport& transport);

/** Set one exact-name DeviceParameter and validate identity/range/readback evidence. */
[[nodiscard]] sunny::core::Result<AbletonDeviceParameterEvidence>
set_device_parameter_with_readback(const LomPath& device_path,
                                   const std::string& parameter_name,
                                   sunny::core::DeviceParameterValueProperty value_property,
                                   float range_min,
                                   float range_max,
                                   float value,
                                   LomTransport& transport);

/** Observe one exact-name DeviceParameter without mutating or repairing it. */
[[nodiscard]] sunny::core::Result<AbletonDeviceParameterObservation>
observe_device_parameter(const LomPath& device_path,
                         const std::string& parameter_name,
                         sunny::core::DeviceParameterValueProperty value_property,
                         LomTransport& transport);

} // namespace sunny::infrastructure::formats
