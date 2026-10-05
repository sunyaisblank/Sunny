/** Genuine native MixerDevice role observations; no Device catalogue or write authority. */
#pragma once
#include <sunny/infrastructure/ableton/native_units.hpp>

namespace sunny::infrastructure {
namespace native_mixer_detail {
[[nodiscard]] bool capture_valid(const nlohmann::json&);
}
struct NativeMixerDisplayCandidate {
    std::string parameter_kind;
    std::string parameter_name;
    std::string parameter_original_name;
    double target = 0.0, tolerance = 0.0, internal_value = 0.0, display_value = 0.0;
    std::string display;
    std::optional<double> display_increment;
    double absolute_display_error = 0.0;
    NativeDisplayParameterDescriptor descriptor;
    std::uint32_t formatter_calls = 0;
    std::vector<NativeDisplaySample> samples;
    /** Current genuine native formatter reading; infinity is an explicit nullable value. */
    nlohmann::json current_display;
    nlohmann::json mixer_capture;
    nlohmann::json track_context;
    std::uint32_t device_cohort_count = 0;
    std::vector<std::string> coverage_limits;
    nlohmann::json evidence;
    bool native_knob_only = true;
    bool host_qualified = false;
};

/** Parse actual source-candidate JSON against the exact finite request. Currently
 * kind=volume with finite dB target/tolerance only. Ownership/host-version,
 * immutable send authority and envelope admission remain separate root joins.
 * No native Device path/type, normalized fader law or write is fabricated. */
[[nodiscard]] sunny::core::Result<NativeMixerDisplayCandidate>
parse_native_mixer_display_candidate(std::string_view parameter_kind,
                                     double target,
                                     double tolerance,
                                     const nlohmann::json& candidate);
} // namespace sunny::infrastructure
