/** Pure selected owning Mix fader intent; no native objects or write authority. */
#pragma once
#include <expected>
#include <nlohmann/json.hpp>
#include <sunny/core/mix/document.hpp>
#include <sunny/core/mix/types.hpp>
#include <sunny/infrastructure/ableton/managed_mixer.hpp>

namespace sunny::infrastructure {
struct NativeMixFaderResidual {
    std::string document_pointer;
    std::string reason;
};
struct NativeMixFaderPlan {
    sunny::core::MixGraphId mix_id;
    sunny::core::ChannelStripId channel_id;
    sunny::core::PartId part_id;
    double target_db = 0.0;
    double tolerance_db = 0.0;
    sunny::core::FaderLevelResolution selected_resolution;
    sunny::core::RelativeLevelResolution level_resolution;
    std::string source_json_pointer;
    nlohmann::json retained_mix;
    std::vector<NativeMixFaderResidual> residuals;
    bool native_knob_only = true;
    bool host_qualified = false;
};
struct NativeMixFaderPlanError {
    std::string reason;
    std::string diagnostic;
};
using NativeMixFaderPlanResult = std::expected<NativeMixFaderPlan, NativeMixFaderPlanError>;

/** Read selected actual Channel effective dB after core relative-level resolution.
 * expected_part is the owning retained Score Part; unresolved LUFS/dependencies,
 * invalid graph/ownership and nonfinite intent decline, without fallback. Input
 * trim and all other authored leaves remain retained residuals. No native range
 * ceiling, normalized formula, pan law or audio/loudness equivalence is assumed. */
[[nodiscard]] NativeMixFaderPlanResult plan_native_mix_fader(const sunny::core::MixGraph& graph,
                                                             sunny::core::ChannelStripId channel,
                                                             sunny::core::PartId expected_part,
                                                             double tolerance_db);
[[nodiscard]] nlohmann::json native_mix_fader_plan_to_json(const NativeMixFaderPlan& plan);
struct NativeMixStaticSelection {
    bool volume = true, pan = false, mute = false, solo = false;
    double tolerance_db = 0.0;
};
struct NativeMixStaticPlan {
    sunny::core::MixGraphId mix_id;
    sunny::core::ChannelStripId channel_id;
    sunny::core::PartId part_id;
    ManagedStaticMixerDesired desired;
    nlohmann::json retained_mix;
    std::optional<NativeMixFaderPlan> fader;
    std::vector<std::string> source_json_pointers;
    std::vector<NativeMixFaderResidual> residuals;
};
using NativeMixStaticPlanResult = std::expected<NativeMixStaticPlan, NativeMixFaderPlanError>;
/** Select actual authored fader/pan/mute/solo leaves. Pan explicitly projects
 * native Stereo coordinate only; all pan-law/spatial/DSP obligations remain residual.
 * Effects/input trim/routing/automation are never silently flattened or applied. */
[[nodiscard]] NativeMixStaticPlanResult plan_native_mix_static(const sunny::core::MixGraph&,
                                                               sunny::core::ChannelStripId,
                                                               sunny::core::PartId,
                                                               const NativeMixStaticSelection&);
[[nodiscard]] nlohmann::json native_mix_static_plan_to_json(const NativeMixStaticPlan&);
} // namespace sunny::infrastructure
