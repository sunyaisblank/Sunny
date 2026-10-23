/** Pure finite physical-intent plans for ordered native Utility/EQ Eight
 * effects. */
#pragma once

#include <expected>
#include <optional>
#include <span>
#include <sunny/core/mix/document.hpp>
#include <sunny/infrastructure/ableton/managed_devices.hpp>
#include <sunny/infrastructure/ableton/native_timbre_plan.hpp>

namespace sunny::infrastructure {

/** Explicit tolerances in the declared physical units, never internal-domain
 * ranges. */
struct NativeEffectTolerances {
    double decibels = 0.0;
    double hertz = 0.0;
    double quality_factor = 0.0;
    double percent = 0.0;
};

enum class NativeEffectSelectionKind { TimbreEffect, MixInputTrim, MixEffect };

struct NativeEffectSelection {
    NativeEffectSelectionKind kind = NativeEffectSelectionKind::TimbreEffect;
    /** EffectId / ChannelStripId / MixEffectId selected according to kind. */
    std::uint64_t authored_id = 0;
    NativeEffectTolerances tolerance{};
};

using NativeEffectEnumIntent = ManagedDeviceModeIntent;
using NativeEffectPropertyIntent = ManagedDevicePropertyIntent;

struct NativeEffectPlanEntry {
    std::string device_key;
    std::uint32_t desired_chain_index = 0;
    ManagedNativeDevice device = ManagedNativeDevice::Utility;
    std::string authored_document;
    std::string source_json_pointer;
    /** A separate enabling ACK precedes all remaining mode admission when bypassed. */
    std::vector<NativeEffectEnumIntent> enable_modes;
    /** Type/setup follows enabling; continuous resolution follows its actual ACK. */
    std::vector<NativeEffectEnumIntent> setup_modes;
    std::vector<NativeEffectPropertyIntent> setup_properties;
    /** Neutral EQ Scale/output values receive their own ACK before band resolution. */
    std::vector<ManagedDevicePhysicalIntent> setup_physical_intents;
    std::vector<ManagedDevicePhysicalIntent> physical_intents;
    /** Only bypassed supported effects have this final Device On Off phase. */
    std::vector<NativeEffectEnumIntent> final_modes;
    /** Legacy authored mappings are retained exactly and are never applied here.
     */
    nlohmann::json original_bindings;
};

struct NativeEffectResidual {
    std::string authored_document;
    std::string document_pointer;
    std::string reason;
};

struct NativeEffectPlan {
    sunny::core::TimbreProfileId profile_id;
    sunny::core::PartId part_id;
    std::optional<sunny::core::MixGraphId> mix_id;
    std::optional<sunny::core::ChannelStripId> channel_id;
    std::vector<NativeEffectPlanEntry> entries;
    std::vector<NativeEffectResidual> residuals;
    nlohmann::json retained_profile;
    std::optional<nlohmann::json> retained_mix;
    bool native_knob_only = true;
    bool dsp_equivalence_qualified = false;
    bool host_qualified = false;
};

struct NativeEffectPlanError {
    std::string reason;
    std::string diagnostic;
};
using NativeEffectPlanResult = std::expected<NativeEffectPlan, NativeEffectPlanError>;

/** Whole-effect opt-in reads authored units directly. Selected EQ supports
 * Peak/LowShelf/HighShelf only; all unused native bands are explicitly Off.
 * Mix width maps its declared unity ratio to percent. Selected input trim is
 * always a separate Utility before Mix effects. Missing earlier active stages
 * reject an ordered realization. Existing native order is checked separately
 * against the retained actual cohort before any append/setter.
 *
 * Passing no MixGraph requires no channel; the full original profile/graph
 * codecs and every unimplemented leaf are retained. This is a pure plan, with
 * no native objects, ownership authority, mode assumptions or DSP equivalence.
 */
[[nodiscard]] NativeEffectPlanResult
plan_native_effects(const sunny::core::TimbreProfile& profile,
                    const sunny::core::MixGraph* mix,
                    std::optional<sunny::core::ChannelStripId> channel,
                    std::span<const NativeEffectSelection> selections);

[[nodiscard]] nlohmann::json native_effect_plan_to_json(const NativeEffectPlan& plan);

} // namespace sunny::infrastructure
