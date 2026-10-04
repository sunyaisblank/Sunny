/** Explicit read-only realization selections for finite native Drift controls. */
#pragma once

#include <expected>
#include <nlohmann/json.hpp>
#include <span>
#include <sunny/core/timbre/document.hpp>
#include <sunny/core/timbre/live_capabilities.hpp>

namespace sunny::infrastructure {

inline constexpr std::size_t NATIVE_TIMBRE_MAX_SELECTIONS = 4;
inline constexpr std::size_t NATIVE_TIMBRE_MAX_RESIDUAL_PATHS = 65536;

/** Explicit opt-in: read the authored path's physical value directly in its IR
 * unit. The existing rendering binding supplies only device/name identity;
 * its range, curve and value_property remain unchanged and are not applied by
 * this separate realization selection. */
struct NativeTimbreSelection {
    std::string source_path;
    std::string capability_id;
    double tolerance = 0.0;
};

struct NativeTimbrePhysicalIntent {
    NativeTimbreSelection selection;
    std::string source_json_pointer;
    sunny::core::LiveNativePhysicalUnit unit = sunny::core::LiveNativePhysicalUnit::Hertz;
    double target = 0.0;
    sunny::core::DeviceParameter original_binding;
};

struct NativeTimbreResidual {
    /** RFC6901 pointer into retained_profile. Serialized field names can differ
     * from public get_parameter paths; these are explicitly codec pointers. */
    std::string document_pointer;
    std::string reason;
};

struct NativeTimbrePlan {
    sunny::core::TimbreProfileId profile_id;
    sunny::core::PartId part_id;
    std::string device_browser_name = "Drift";
    std::string device_class_name = "Drift";
    std::vector<NativeTimbrePhysicalIntent> intents;
    std::vector<NativeTimbreResidual> residuals;
    /** Exact original authored codec, including every legacy rendering mapping. */
    nlohmann::json retained_profile;
    bool native_knob_only = true;
    bool dsp_equivalence_qualified = false;
    bool host_qualified = false;
};

enum class NativeTimbrePlanFailure {
    InvalidProfile,
    UnsupportedDevice,
    UnsupportedSource,
    InvalidSelection,
    MissingBinding,
    BindingMismatch,
    UnsupportedEnvelope,
    ExcessiveResiduals,
};

struct NativeTimbrePlanError {
    NativeTimbrePlanFailure reason;
    std::string diagnostic;
};

using NativeTimbrePlanResult = std::expected<NativeTimbrePlan, NativeTimbrePlanError>;

/** At most four distinct caller-ordered selections: filter.cutoff Hz and the
 * three duration leaves of a non-looping three-stage ADSR amplifier in ms.
 * Requires explicit NativeAbleton Drift and exact source-device bindings.
 * Reports all unselected source/effect codec leaves and other unimplemented
 * authored domains. Never inserts, resolves a native curve or writes a value. */
[[nodiscard]] NativeTimbrePlanResult
plan_native_timbre(const sunny::core::TimbreProfile& profile,
                   std::span<const NativeTimbreSelection> selections);

/** Immutable plan payload for a later prepared owning-project request; contains
 * explicit selections, physical values, original bindings and exact residuals.
 * It grants no native send or ownership authority. */
[[nodiscard]] nlohmann::json native_timbre_plan_to_json(const NativeTimbrePlan& plan);

} // namespace sunny::infrastructure
