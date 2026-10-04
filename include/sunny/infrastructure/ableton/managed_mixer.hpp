/** Closed actual current-Mixer grants and static writes; no second journal. */
#pragma once
#include <sunny/infrastructure/ableton/managed_realization.hpp>
#include <sunny/infrastructure/ableton/native_mixer_units.hpp>
namespace sunny::infrastructure {
struct ManagedMixerVolumeIntent {
    double target = 0.0, tolerance = 0.0;
};
struct ManagedStaticMixerDesired {
    std::optional<ManagedMixerVolumeIntent> volume;
    std::optional<double> pan; // Explicit native Stereo control coordinate, no pan-law DSP claim.
    std::optional<bool> mute, solo;
};
[[nodiscard]] nlohmann::json managed_static_mixer_desired_to_json(const ManagedStaticMixerDesired&);
struct ManagedStaticMixerPreview {
    ManagedBridgeContext context;
    std::string project_key, binding_key, preview_token, preview_fingerprint;
    nlohmann::json approved_preview;
    nlohmann::json observation;
};
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_static_mixer_preview_request(const ManagedBridgeContext&,
                                          const ManagedBindingReceipt&,
                                          const ManagedStaticMixerDesired&,
                                          bool adoption);
[[nodiscard]] sunny::core::Result<ManagedStaticMixerPreview> parse_managed_static_mixer_preview(
    const LomRequest&, const ManagedBridgeContext&, const nlohmann::json&);
/** Read current selected control/formatter/Solo cohort evidence with usual
 * write-eligibility preconditions. No token, grant, baseline refresh or journal.
 * Current controls need not equal desired targets; callers compare final state. */
[[nodiscard]] sunny::core::Result<LomRequest> make_managed_static_mixer_inspection_request(
    const ManagedBridgeContext&, const ManagedBindingReceipt&, const ManagedStaticMixerDesired&);
[[nodiscard]] sunny::core::Result<nlohmann::json> parse_managed_static_mixer_inspection(
    const LomRequest&, const ManagedBridgeContext&, const nlohmann::json&);
/** Prepared request is immutable; owning product fences it before dispatch.
 * Adoption grants only exact selected CURRENT handles, no setters or historical
 * ownership. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_static_mixer_request(const ManagedBridgeContext&,
                                  const std::string& operation,
                                  const ManagedStaticMixerPreview&,
                                  bool explicit_set_wide_audible_approval = false);
} // namespace sunny::infrastructure
