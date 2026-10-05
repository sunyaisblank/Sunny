/**
 * @file native_units.hpp
 * @brief Closed read-only native display resolution requests and observations.
 *
 * A candidate describes one observed native knob at display precision. It does
 * not authorize a write, identify an owned device, or qualify an effect's DSP,
 * envelopes, persistence, edition, operating system, or Python ABI.
 */
#pragma once

#include <sunny/core/timbre/live_capabilities.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>

namespace sunny::infrastructure {

inline constexpr std::uint32_t SUNNY_NATIVE_DISPLAY_SCHEMA_VERSION = 1;
inline constexpr std::uint32_t SUNNY_NATIVE_DISPLAY_FORMATTER_BUDGET = 64;

struct NativeDisplayParameterDescriptor {
    double minimum = 0.0;
    double maximum = 0.0;
    double value = 0.0;
    double default_value = 0.0;
    bool is_enabled = false;
    std::uint8_t state = 0;
    std::uint8_t automation_state = 0;
};

struct NativeDisplaySample {
    double internal_value = 0.0;
    std::string display;
    std::string phase;
    /** Null only for a source-observed Utility Gain negative-infinity display. */
    std::optional<double> display_value;
    bool negative_infinity = false;
};

struct NativeDisplayCandidate {
    std::string device_path;
    std::string capability_id;
    std::uint32_t registry_version = sunny::core::LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION;
    std::string device_class_name;
    std::string parameter_original_name;
    std::uint32_t parameter_index = 0;
    sunny::core::LiveNativePhysicalUnit unit = sunny::core::LiveNativePhysicalUnit::Decibels;
    double target = 0.0;
    double tolerance = 0.0;
    double internal_value = 0.0;
    std::string display;
    double display_value = 0.0;
    /** Nominal displayed decimal increment; no hidden rounding-error bound. */
    std::optional<double> display_increment;
    double absolute_display_error = 0.0;
    std::optional<double> balance_full_scale;
    NativeDisplayParameterDescriptor descriptor;
    std::uint32_t formatter_calls = 0;
    std::vector<NativeDisplaySample> samples;
    /** Exact observed modes, including EQ couplings and Drift voice/list properties. */
    nlohmann::json population;
    nlohmann::json modes;
    nlohmann::json eq8_scale_display;
    std::string qualification;
    std::string source_commit;
    bool host_qualified = false;
    bool native_knob_only = true;
    std::vector<std::string> coverage_limits;
    /** Preserve the complete source observation without discarding evidence. */
    nlohmann::json evidence;
};

enum class NativeDisplayResolutionStatus { Candidate, Declined, ObservationUnavailable };

struct NativeDisplayResolution {
    NativeDisplayResolutionStatus status = NativeDisplayResolutionStatus::ObservationUnavailable;
    std::optional<NativeDisplayCandidate> candidate;
    std::string reason;
    std::string diagnostic;
    std::uint32_t formatter_calls = 0;
    /** The complete closed response envelope, when an actual peer supplied one. */
    std::optional<nlohmann::json> evidence;
};

/**
 * Build one read-only request on a canonical flat native Device path.
 *
 * Only the 33 continuous identities in the existing finite core registry are
 * admitted. The peer owns class/name/unit/mode resolution; callers cannot
 * supply descriptors or modes. Both bridge validators admit only this closed
 * read-only contract; execution still requires an actual observed host peer.
 */
[[nodiscard]] sunny::core::Result<LomRequest> make_native_display_resolution_request(
    const LomPath& device_path, const std::string& capability_id, double target, double tolerance);

/**
 * Validate an actual peer response against the original closed request.
 *
 * Resolved envelope: schema_version, capability_id, target, tolerance, outcome
 * ("resolved"), candidate (the exact native_units.py schema1 result).
 * Declined envelope replaces candidate with reason, diagnostic, formatter_calls
 * and uses outcome="declined". A decline is evidence, not a fallback candidate.
 */
[[nodiscard]] sunny::core::Result<NativeDisplayResolution>
parse_native_display_resolution(const LomRequest& request, const nlohmann::json& response);

/**
 * Send one read-only request and validate the actual peer observation.
 * Recording-only transports return ObservationUnavailable without sending a
 * query or synthesizing native evidence. No function here writes to Live.
 */
[[nodiscard]] sunny::core::Result<NativeDisplayResolution>
resolve_native_display_value(const LomPath& device_path,
                             const std::string& capability_id,
                             double target,
                             double tolerance,
                             LomTransport& transport);

} // namespace sunny::infrastructure
