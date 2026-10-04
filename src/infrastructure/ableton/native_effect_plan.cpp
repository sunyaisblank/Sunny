#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <sstream>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/native_effect_plan.hpp>

namespace sunny::infrastructure {
namespace {
using namespace sunny::core;
using nlohmann::json;
using Kind = NativeEffectSelectionKind;

NativeEffectPlanResult fail(std::string reason, std::string diagnostic) {
    return std::unexpected(NativeEffectPlanError{std::move(reason), std::move(diagnostic)});
}
std::string hex(std::uint64_t value) {
    std::ostringstream result;
    result << std::hex << value;
    return result.str();
}
std::string escaped(std::string_view value) {
    std::string result;
    for (char c : value)
        result += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
    return result;
}
bool leaves(const json& node,
            const std::string& document,
            const std::string& path,
            const std::set<std::string>& covered,
            std::vector<NativeEffectResidual>& output) {
    if (covered.contains(path)) return true;
    if (node.is_primitive() || node.empty()) {
        if (output.size() >= NATIVE_TIMBRE_MAX_RESIDUAL_PATHS) return false;
        output.push_back({document,
                          path,
                          "Retained authored leaf is outside the selected finite "
                          "native knob intent; no DSP equivalence is qualified"});
        return true;
    }
    for (const auto& [name, child] : node.items())
        if (!leaves(child, document, path + "/" + escaped(name), covered, output)) return false;
    return true;
}
bool tolerances(const NativeEffectTolerances& value) {
    return std::ranges::all_of(
        std::array{value.decibels, value.hertz, value.quality_factor, value.percent},
        [](double v) { return std::isfinite(v) && v >= 0; });
}
const NativeEffectSelection*
selected(std::span<const NativeEffectSelection> selections, Kind kind, std::uint64_t id) {
    const auto found = std::ranges::find_if(
        selections, [&](const auto& item) { return item.kind == kind && item.authored_id == id; });
    return found == selections.end() ? nullptr : &*found;
}
void utility(NativeEffectPlanEntry& entry,
             double gain,
             double width,
             const NativeEffectTolerances& tolerance) {
    entry.device = ManagedNativeDevice::Utility;
    entry.enable_modes = {{"utility.enabled", "On"}};
    for (const auto& [id, label] : std::array{std::pair{"utility.channel_mode", "Stereo"},
                                              std::pair{"utility.mono", "Off"},
                                              std::pair{"utility.mute", "Off"},
                                              std::pair{"utility.left_invert", "Off"},
                                              std::pair{"utility.right_invert", "Off"},
                                              std::pair{"utility.bass_mono", "Off"},
                                              std::pair{"utility.dc_filter", "Off"}})
        entry.setup_modes.push_back({id, label});
    entry.physical_intents = {{"utility.gain", gain, tolerance.decibels},
                              {"utility.width", width, tolerance.percent},
                              {"utility.balance", 0.0, 0.0}};
}

template <class Band> std::optional<std::string> type_label(const Band& band) {
    if (static_cast<int>(band.band_type) == 0) return "Bell";
    if (static_cast<int>(band.band_type) == 1) return "Low Shelf";
    if (static_cast<int>(band.band_type) == 2) return "High Shelf";
    return std::nullopt;
}
template <class Band>
bool equalizer(NativeEffectPlanEntry& entry,
               const std::vector<Band>& bands,
               const NativeEffectTolerances& tolerance) {
    if (bands.size() > 8) return false;
    entry.device = ManagedNativeDevice::EqEight;
    entry.enable_modes = {{"eq8.enabled", "On"}};
    entry.setup_modes = {{"eq8.adaptive_q", "Off"}};
    entry.setup_properties = {{"global_mode", "Stereo"}};
    entry.setup_physical_intents = {{"eq8.scale", 100.0, tolerance.percent},
                                    {"eq8.output_gain", 0.0, tolerance.decibels}};
    for (std::size_t index = 0; index < 8; ++index) {
        const auto id = "eq8.band." + std::to_string(index + 1) + ".";
        entry.setup_modes.push_back({id + "enabled", index < bands.size() ? "On" : "Off"});
        if (index == bands.size() || index > bands.size()) continue;
        const auto& band = bands[index];
        const auto label = type_label(band);
        if (!label || !std::isfinite(band.frequency) || band.frequency <= 0 ||
            !std::isfinite(band.gain) || !std::isfinite(band.q) || band.q <= 0)
            return false;
        entry.setup_modes.push_back({id + "type", *label});
        entry.physical_intents.push_back({id + "frequency", band.frequency, tolerance.hertz});
        entry.physical_intents.push_back({id + "gain", band.gain, tolerance.decibels});
        entry.physical_intents.push_back({id + "q", band.q, tolerance.quality_factor});
    }
    return true;
}

bool mix_bindings(const MixEffect& effect, const NativeEffectPlanEntry& entry) {
    for (const auto& intent : entry.physical_intents) {
        std::string path, original;
        if (intent.capability_id == "utility.width") {
            path = "width";
            original = "Stereo Width";
        } else if (intent.capability_id.starts_with("eq8.band.")) {
            const auto number = intent.capability_id[9] - '1';
            const auto suffix = intent.capability_id.substr(11);
            path = "bands[" + std::to_string(number) + "]." + (suffix == "q" ? "q" : suffix);
            original = std::to_string(number + 1) + (suffix == "frequency" ? " Frequency A"
                                                     : suffix == "gain"    ? " Gain A"
                                                                           : " Resonance A");
        } else
            continue;
        const auto found = effect.parameter_map.find(path);
        if (found != effect.parameter_map.end() && found->second.parameter_name != original)
            return false;
    }
    return true;
}
bool timbre_bindings(const TimbreProfile& profile,
                     const NativeEffectPlanEntry& entry,
                     std::size_t effect_index) {
    for (const auto& intent : entry.physical_intents) {
        if (!intent.capability_id.starts_with("eq8.band.")) continue;
        const auto number = intent.capability_id[9] - '1';
        const auto suffix = intent.capability_id.substr(11);
        const auto path = "insert_chain.effects[" + std::to_string(effect_index) + "].bands[" +
                          std::to_string(number) + "]." + suffix;
        const auto original = std::to_string(number + 1) + (suffix == "frequency" ? " Frequency A"
                                                            : suffix == "gain"    ? " Gain A"
                                                                                  : " Resonance A");
        const auto found = profile.rendering.parameter_map.find(path);
        if (found != profile.rendering.parameter_map.end() &&
            (found->second.device_index != entry.desired_chain_index ||
             found->second.parameter_name != original))
            return false;
    }
    return true;
}
} // namespace

NativeEffectPlanResult plan_native_effects(const sunny::core::TimbreProfile& profile,
                                           const sunny::core::MixGraph* mix,
                                           std::optional<sunny::core::ChannelStripId> channel,
                                           std::span<const NativeEffectSelection> selections) {
    if (!profile.id.value || !profile.part_id.value ||
        profile.rendering.device_type.tag != DeviceTypeTag::NativeAbleton ||
        profile.rendering.device_type.device_name != "Drift")
        return fail("UnsupportedSource",
                    "Select the owning NativeAbleton Drift profile explicitly");
    for (const auto& diagnostic : validate_timbre(profile))
        if (diagnostic.severity == ValidationSeverity::Error)
            return fail("InvalidProfile", diagnostic.message);
    if (selections.empty() || selections.size() >= SUNNY_MANAGED_MAX_DEVICES)
        return fail("InvalidSelection", "Select 1..15 physical effect stages on one owning Part");
    std::set<std::pair<Kind, std::uint64_t>> distinct;
    for (const auto& selection : selections)
        if (!selection.authored_id || !tolerances(selection.tolerance) ||
            !distinct.emplace(selection.kind, selection.authored_id).second)
            return fail("InvalidSelection",
                        "Selections require distinct typed IDs and finite tolerances");
    const ChannelStrip* strip = nullptr;
    std::size_t channel_index = 0;
    if (mix) {
        if (!mix->id.value || !channel || !channel->value)
            return fail("InvalidOwner",
                        "The actual MixGraph and selected ChannelStrip IDs are required");
        for (const auto& diagnostic : validate_mix(*mix))
            if (diagnostic.severity == ValidationSeverity::Error)
                return fail("InvalidMix", diagnostic.message);
        for (std::size_t index = 0; index < mix->channels.size(); ++index)
            if (mix->channels[index].id == *channel) {
                strip = &mix->channels[index];
                channel_index = index;
            }
        if (!strip || strip->part_id != profile.part_id)
            return fail("InvalidOwner",
                        "The selected ChannelStrip must belong to this authored Part");
    } else if (channel)
        return fail("InvalidOwner",
                    "A ChannelStrip cannot be supplied without its actual MixGraph");
    NativeEffectPlan result;
    result.profile_id = profile.id;
    result.part_id = profile.part_id;
    result.retained_profile = timbre_to_json(profile);
    if (mix) {
        result.mix_id = mix->id;
        result.channel_id = strip->id;
        result.retained_mix = mix_to_json(*mix);
    }
    std::set<std::string> profile_covered, mix_covered;
    std::set<std::pair<Kind, std::uint64_t>> consumed;
    bool active_gap = false;
    auto append = [&](NativeEffectPlanEntry entry, Kind kind, std::uint64_t id) {
        consumed.emplace(kind, id);
        result.entries.push_back(std::move(entry));
    };
    for (std::size_t index = 0; index < profile.insert_chain.effects.size(); ++index) {
        const auto& effect = profile.insert_chain.effects[index];
        const bool bypassed = !effect.enabled || profile.insert_chain.bypass_all || effect.mix == 0;
        const auto selection = selected(selections, Kind::TimbreEffect, effect.id.value);
        if (!selection) {
            active_gap |= !bypassed;
            continue;
        }
        if (active_gap)
            return fail("UnrealizedEarlierStage", "An earlier active Timbre effect is unselected");
        if (effect.mix != 0 && effect.mix != 1)
            return fail("UnsupportedWet",
                        "Native Utility/EQ8 has no general fractional Dry/Wet control");
        const auto* eq = std::get_if<EQEffect>(&effect.parameters);
        if (!eq)
            return fail("UnsupportedEffect",
                        "The selected Timbre effect must be the finite EQ subset");
        NativeEffectPlanEntry entry;
        entry.device_key = "timbre_" + hex(profile.id.value) + "_effect_" + hex(effect.id.value);
        entry.desired_chain_index = static_cast<std::uint32_t>(result.entries.size() + 1);
        entry.authored_document = "Timbre";
        entry.source_json_pointer = "/insert_chain/effects/" + std::to_string(index);
        entry.original_bindings = result.retained_profile.at("rendering").at("param_map");
        if (!equalizer(entry, eq->bands, selection->tolerance))
            return fail("UnsupportedEQ",
                        "Use at most8 Peak/LowShelf/HighShelf bands with finite "
                        "Hz/dB/Q; cut slope is undeclared");
        if (!timbre_bindings(profile, entry, index))
            return fail("BindingMismatch",
                        "An existing selected Timbre binding "
                        "disagrees with native identity/order");
        if (bypassed || eq->bands.empty())
            entry.final_modes.push_back(
                {entry.device == ManagedNativeDevice::Utility ? "utility.enabled" : "eq8.enabled",
                 "Off"});
        // Identities and bindings are retained metadata; selected authored values
        // are represented.
        profile_covered.insert(entry.source_json_pointer);
        append(std::move(entry), Kind::TimbreEffect, effect.id.value);
    }
    if (strip) {
        const auto base = "/channels/" + std::to_string(channel_index);
        const auto trim_selection = selected(selections, Kind::MixInputTrim, strip->id.value);
        const bool mix_selected = std::ranges::any_of(
            selections, [](const auto& item) { return item.kind == Kind::MixEffect; });
        if (trim_selection) {
            if (active_gap)
                return fail("UnrealizedEarlierStage",
                            "An earlier active Timbre effect blocks Mix trim placement");
            NativeEffectPlanEntry entry;
            entry.device_key =
                "mix_" + hex(mix->id.value) + "_channel_" + hex(strip->id.value) + "_trim";
            entry.desired_chain_index = static_cast<std::uint32_t>(result.entries.size() + 1);
            entry.authored_document = "Mix";
            entry.source_json_pointer = base + "/input_trim";
            entry.original_bindings = json::object();
            utility(entry, strip->input_trim, 100.0, trim_selection->tolerance);
            mix_covered.insert(entry.source_json_pointer);
            append(std::move(entry), Kind::MixInputTrim, strip->id.value);
        } else if (mix_selected && strip->input_trim != 0)
            return fail("UnrealizedEarlierStage",
                        "Nonzero input trim must be selected before Mix effects");
        if (mix_selected && strip->polarity_invert)
            return fail("UnsupportedPolarity",
                        "This finite planner does not realize channel polarity inversion");
        for (std::size_t index = 0; index < strip->insert_chain.effects.size(); ++index) {
            const auto& effect = strip->insert_chain.effects[index];
            const auto selection = selected(selections, Kind::MixEffect, effect.id.value);
            if (!selection) {
                active_gap |= effect.enabled;
                continue;
            }
            if (active_gap)
                return fail("UnrealizedEarlierStage",
                            "An earlier active effect is unselected before this Mix stage");
            NativeEffectPlanEntry entry;
            entry.device_key = "mix_" + hex(mix->id.value) + "_effect_" + hex(effect.id.value);
            entry.desired_chain_index = static_cast<std::uint32_t>(result.entries.size() + 1);
            entry.authored_document = "Mix";
            entry.source_json_pointer = base + "/insert_chain/" + std::to_string(index);
            entry.original_bindings =
                result.retained_mix->at("channels")[channel_index].at("insert_chain")[index].at(
                    "parameter_map");
            if (const auto* eq = std::get_if<MixEQ>(&effect.parameters)) {
                if (eq->linear_phase || eq->auto_gain ||
                    std::ranges::any_of(
                        eq->bands, [](const auto& band) { return band.dynamic.has_value(); }) ||
                    !equalizer(entry, eq->bands, selection->tolerance))
                    return fail("UnsupportedEQ",
                                "Mix EQ requires at most8 static Peak/LowShelf/HighShelf "
                                "bands, linear_phase=false and auto_gain=false");
                if (eq->bands.empty())
                    entry.final_modes.push_back({entry.device == ManagedNativeDevice::Utility
                                                     ? "utility.enabled"
                                                     : "eq8.enabled",
                                                 "Off"});
            } else if (const auto* width = std::get_if<MixStereoProcessor>(&effect.parameters)) {
                if (width->mid_side_balance != 0.5f || width->mono_below)
                    return fail("UnsupportedStereo",
                                "Finite width requires equal Mid/Side balance and no "
                                "mono_below processing");
                utility(
                    entry, 0.0, 100.0 * static_cast<double>(width->width), selection->tolerance);
            } else
                return fail("UnsupportedEffect",
                            "Select only finite Mix EQ or stereo-width effects");
            if (!mix_bindings(effect, entry))
                return fail("BindingMismatch",
                            "An existing selected Mix binding disagrees with native "
                            "original identity");
            if (!effect.enabled && entry.final_modes.empty())
                entry.final_modes.push_back({entry.device == ManagedNativeDevice::Utility
                                                 ? "utility.enabled"
                                                 : "eq8.enabled",
                                             "Off"});
            mix_covered.insert(entry.source_json_pointer);
            append(std::move(entry), Kind::MixEffect, effect.id.value);
        }
    }
    if (consumed.size() != selections.size())
        return fail("InvalidSelection",
                    "Every selection must identify a real "
                    "effect/trim on the selected owning Part");
    if (!leaves(result.retained_profile, "Timbre", "", profile_covered, result.residuals) ||
        (result.retained_mix &&
         !leaves(*result.retained_mix, "Mix", "", mix_covered, result.residuals)))
        return fail("ExcessiveResiduals",
                    "Residual leaf coverage exceeds the finite65536-path limit");
    return result;
}

nlohmann::json native_effect_plan_to_json(const NativeEffectPlan& plan) {
    json entries = json::array(), residuals = json::array();
    for (const auto& entry : plan.entries) {
        auto modes = [](const auto& values) {
            json output = json::array();
            for (const auto& item : values)
                output.push_back({{"capability_id", item.capability_id}, {"label", item.label}});
            return output;
        };
        json physical = json::array(), properties = json::array(), setup_physical = json::array();
        for (const auto& item : entry.setup_physical_intents)
            setup_physical.push_back({{"capability_id", item.capability_id},
                                      {"target", item.target},
                                      {"tolerance", item.tolerance}});
        for (const auto& item : entry.physical_intents)
            physical.push_back({{"capability_id", item.capability_id},
                                {"target", item.target},
                                {"tolerance", item.tolerance}});
        for (const auto& item : entry.setup_properties)
            properties.push_back({{"property", item.property}, {"label", item.label}});
        entries.push_back({{"device_key", entry.device_key},
                           {"desired_chain_index", entry.desired_chain_index},
                           {"device_browser_name",
                            entry.device == ManagedNativeDevice::Utility ? "Utility" : "EQ Eight"},
                           {"authored_document", entry.authored_document},
                           {"source_json_pointer", entry.source_json_pointer},
                           {"enable_modes", modes(entry.enable_modes)},
                           {"setup_modes", modes(entry.setup_modes)},
                           {"setup_properties", properties},
                           {"setup_physical_intents", setup_physical},
                           {"physical_intents", physical},
                           {"final_modes", modes(entry.final_modes)},
                           {"original_bindings", entry.original_bindings}});
    }
    for (const auto& item : plan.residuals)
        residuals.push_back({{"authored_document", item.authored_document},
                             {"document_pointer", item.document_pointer},
                             {"reason", item.reason}});
    json result{{"schema_version", 1},
                {"registry_version", LIVE_NATIVE_CAPABILITY_REGISTRY_VERSION},
                {"profile_id", plan.profile_id.value},
                {"part_id", plan.part_id.value},
                {"entries", entries},
                {"residuals", residuals},
                {"retained_profile", plan.retained_profile},
                {"selection_semantics", "ReadAuthoredPhysicalValueWithoutApplyingLegacyMapping"},
                {"native_knob_only", true},
                {"dsp_equivalence_qualified", false},
                {"host_qualified", false}};
    if (plan.retained_mix) {
        result["mix_id"] = plan.mix_id->value;
        result["channel_id"] = plan.channel_id->value;
        result["retained_mix"] = *plan.retained_mix;
    }
    return result;
}
} // namespace sunny::infrastructure
