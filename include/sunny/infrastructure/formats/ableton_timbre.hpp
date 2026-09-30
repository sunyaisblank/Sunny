/**
 * @file ableton_timbre.hpp
 * @brief Ableton Live Compiler — Timbre IR to device chains
 *
 *
 * Compiles a validated TimbreProfile into LOM commands that create
 * an Ableton instrument device chain on the specified track:
 *   1. Insert a native Ableton instrument device
 *   2. Preserve unsupported preset/plugin requests as warnings
 *   3. Insert EffectChain as audio effects
 *   4. Report automation that LOM cannot author
 *
 * Precondition:  TimbreProfile passes validation (no Error diagnostics)
 * Postcondition: Supported native devices are inserted via transport and any
 * unsupported requested operations are reported as warnings.
 */

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <sunny/core/timbre/document.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/formats/ableton_evidence.hpp>
#include <vector>

namespace sunny::infrastructure::formats {

struct AbletonParameterDeployment {
    std::string ir_path;
    std::uint32_t device_index = 0;
    std::string parameter_name;
    sunny::core::DeviceParameterValueProperty value_property =
        sunny::core::DeviceParameterValueProperty::InternalValue;
    float source_value = 0.0f;
    float requested_value = 0.0f;
    float range_min = 0.0f;
    float range_max = 1.0f;
    std::optional<std::string> matched_name;
    std::optional<std::string> original_name;
    std::optional<float> observed_value;
    std::optional<float> observed_minimum;
    std::optional<float> observed_maximum;
    std::optional<bool> is_quantized;
    std::optional<float> default_value;
    std::optional<std::vector<std::string>> value_items;
    std::optional<bool> is_enabled;
    bool verified = false;
    std::optional<std::uint8_t> parameter_state;
    std::optional<std::uint8_t> automation_state;
    AbletonParameterAction action = AbletonParameterAction::NotApplied;
};

struct TimbreCompilationResult {
    AbletonTargetProfile target_profile;
    std::uint64_t devices_requested = 0;
    std::uint64_t devices_created = 0;
    std::uint64_t devices_verified = 0;
    std::uint64_t effects_requested = 0;
    std::uint64_t effects_inserted = 0;
    std::uint64_t effects_verified = 0;
    std::uint64_t parameters_mapped = 0;
    std::uint64_t parameters_verified = 0;
    std::uint64_t automation_lanes_requested = 0;
    std::uint64_t automation_lanes_written = 0;
    std::vector<AbletonDeviceInsertionDeployment> device_deployments;
    std::vector<AbletonParameterDeployment> parameter_deployments;
    std::vector<std::string> warnings;
};

/**
 * @brief Compile a TimbreProfile to an Ableton track via LOM transport
 *
 * @param profile   Validated TimbreProfile
 * @param track_index  Ableton track index to configure
 * @param transport LOM transport (CommandBuffer for testing)
 * @return Compilation result or error
 */
[[nodiscard]] sunny::core::Result<TimbreCompilationResult> compile_timbre_to_ableton(
    const sunny::core::TimbreProfile& profile, int track_index, LomTransport& transport);

} // namespace sunny::infrastructure::formats
