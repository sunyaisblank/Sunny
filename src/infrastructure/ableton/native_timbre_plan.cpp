#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/native_timbre_plan.hpp>

namespace sunny::infrastructure {
namespace {
using namespace sunny::core;
using nlohmann::json;
using Unit = LiveNativePhysicalUnit;

struct Control {
    const char* source;
    const char* capability;
    const char* native_name;
    const char* pointer;
    Unit unit;
};
constexpr std::array controls{Control{"source.filter.cutoff",
                                      "drift.lp.frequency",
                                      "LP Freq",
                                      "/source/data/filter/cutoff",
                                      Unit::Hertz},
                              Control{"source.amplifier.stages[0].duration",
                                      "drift.env.1.attack",
                                      "Env 1 Attack",
                                      "/source/data/amplifier/stages/0/dur",
                                      Unit::Milliseconds},
                              Control{"source.amplifier.stages[1].duration",
                                      "drift.env.1.decay",
                                      "Env 1 Decay",
                                      "/source/data/amplifier/stages/1/dur",
                                      Unit::Milliseconds},
                              Control{"source.amplifier.stages[2].duration",
                                      "drift.env.1.release",
                                      "Env 1 Release",
                                      "/source/data/amplifier/stages/2/dur",
                                      Unit::Milliseconds}};

NativeTimbrePlanResult failure(NativeTimbrePlanFailure reason, std::string diagnostic) {
    return std::unexpected(NativeTimbrePlanError{reason, std::move(diagnostic)});
}

std::string escaped_pointer(std::string_view segment) {
    std::string result;
    for (const char value : segment) {
        if (value == '~')
            result += "~0";
        else if (value == '/')
            result += "~1";
        else
            result += value;
    }
    return result;
}

bool residual_leaves(const json& node,
                     const std::string& path,
                     const std::set<std::string>& covered,
                     std::string_view reason,
                     std::vector<NativeTimbreResidual>& output) {
    if (covered.contains(path)) return true;
    if (node.is_primitive() || node.empty()) {
        if (output.size() == NATIVE_TIMBRE_MAX_RESIDUAL_PATHS) return false;
        output.push_back({path, std::string{reason}});
        return true;
    }
    for (const auto& [name, child] : node.items())
        if (!residual_leaves(child, path + "/" + escaped_pointer(name), covered, reason, output))
            return false;
    return true;
}
} // namespace

NativeTimbrePlanResult plan_native_timbre(const sunny::core::TimbreProfile& profile,
                                          std::span<const NativeTimbreSelection> selections) {
    using namespace sunny::core;
    if (profile.id.value == 0 || profile.part_id.value == 0)
        return failure(NativeTimbrePlanFailure::InvalidProfile,
                       "Authored profile and owning Part identities must be positive");
    if (profile.rendering.device_type.tag != DeviceTypeTag::NativeAbleton ||
        profile.rendering.device_type.device_name != "Drift")
        return failure(NativeTimbrePlanFailure::UnsupportedDevice,
                       "Select NativeAbleton Drift explicitly in the authored rendering config");
    const auto* source = std::get_if<SubtractiveSynth>(&profile.source.data);
    if (source == nullptr)
        return failure(NativeTimbrePlanFailure::UnsupportedSource,
                       "The finite physical source paths require a SubtractiveSynth source");
    if (selections.size() > NATIVE_TIMBRE_MAX_SELECTIONS)
        return failure(NativeTimbrePlanFailure::InvalidSelection,
                       "At most four distinct native Drift physical selections are supported");
    for (const auto& diagnostic : validate_timbre(profile))
        if (diagnostic.severity == ValidationSeverity::Error)
            return failure(NativeTimbrePlanFailure::InvalidProfile,
                           "Authored TimbreProfile is invalid: " + diagnostic.message);

    NativeTimbrePlan plan;
    plan.profile_id = profile.id;
    plan.part_id = profile.part_id;
    plan.retained_profile = timbre_to_json(profile);
    std::set<std::string> selected_paths, selected_capabilities, covered;
    for (const auto& selection : selections) {
        const auto control = std::find_if(controls.begin(), controls.end(), [&](const auto& item) {
            return selection.source_path == item.source &&
                   selection.capability_id == item.capability;
        });
        if (control == controls.end() || !std::isfinite(selection.tolerance) ||
            selection.tolerance < 0.0 || !selected_paths.insert(selection.source_path).second ||
            !selected_capabilities.insert(selection.capability_id).second)
            return failure(NativeTimbrePlanFailure::InvalidSelection,
                           "Selection must be a distinct registered source/control pair with "
                           "finite tolerance");
        const auto binding = profile.rendering.parameter_map.find(selection.source_path);
        if (binding == profile.rendering.parameter_map.end())
            return failure(NativeTimbrePlanFailure::MissingBinding,
                           "An explicit authored rendering binding is required: " +
                               selection.source_path);
        if (binding->second.device_index != 0 ||
            binding->second.parameter_name != control->native_name)
            return failure(NativeTimbrePlanFailure::BindingMismatch,
                           "The binding must name the exact native control on source device0");
        if (control->unit == Unit::Milliseconds &&
            (source->amplifier.loop || source->amplifier.stages.size() != 3 ||
             source->amplifier.stages[0].target_level != 1.0f ||
             source->amplifier.stages[2].target_level != 0.0f))
            return failure(NativeTimbrePlanFailure::UnsupportedEnvelope,
                           "ADSR time selection requires three non-looping stages with attack "
                           "peak1 and release0");
        const auto target = get_parameter(profile, selection.source_path);
        if (!target || !std::isfinite(*target) ||
            (control->unit == Unit::Hertz ? *target <= 0.0f : *target < 0.0f))
            return failure(
                NativeTimbrePlanFailure::InvalidProfile,
                "The authored selected physical value is outside its finite unit domain");
        plan.intents.push_back({selection,
                                control->pointer,
                                control->unit,
                                static_cast<double>(*target),
                                binding->second});
        covered.insert(control->pointer);
    }
    for (const auto* domain :
         {"source", "insert_chain", "modulation", "automation", "morphs", "presets", "rendering"}) {
        const auto path = std::string{"/"} + domain;
        if (!residual_leaves(plan.retained_profile.at(domain),
                             path,
                             covered,
                             "This authored leaf is retained; finite native physical selections do "
                             "not apply or qualify it",
                             plan.residuals))
            return failure(NativeTimbrePlanFailure::ExcessiveResiduals,
                           "Residual coverage exceeds the finite65536-path limit");
    }
    std::sort(
        plan.residuals.begin(), plan.residuals.end(), [](const auto& first, const auto& second) {
            return first.document_pointer < second.document_pointer;
        });
    return plan;
}

nlohmann::json native_timbre_plan_to_json(const NativeTimbrePlan& plan) {
    using nlohmann::json;
    json intents = json::array(), residuals = json::array();
    for (const auto& intent : plan.intents)
        intents.push_back(
            {{"source_path", intent.selection.source_path},
             {"capability_id", intent.selection.capability_id},
             {"tolerance", intent.selection.tolerance},
             {"target", intent.target},
             {"unit",
              intent.unit == sunny::core::LiveNativePhysicalUnit::Hertz ? "Hertz" : "Milliseconds"},
             {"source_json_pointer", intent.source_json_pointer},
             {"original_binding",
              plan.retained_profile.at("rendering")
                  .at("param_map")
                  .at(intent.selection.source_path)}});
    for (const auto& residual : plan.residuals)
        residuals.push_back(
            {{"document_pointer", residual.document_pointer}, {"reason", residual.reason}});
    return {{"schema_version", 1},
            {"registry_version", sunny::core::LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION},
            {"profile_id", plan.profile_id.value},
            {"part_id", plan.part_id.value},
            {"device_browser_name", plan.device_browser_name},
            {"device_class_name", plan.device_class_name},
            {"selection_semantics", "ReadAuthoredPhysicalValueWithoutApplyingLegacyMapping"},
            {"intents", std::move(intents)},
            {"residuals", std::move(residuals)},
            {"retained_profile", plan.retained_profile},
            {"native_knob_only", plan.native_knob_only},
            {"dsp_equivalence_qualified", plan.dsp_equivalence_qualified},
            {"host_qualified", plan.host_qualified}};
}
} // namespace sunny::infrastructure
