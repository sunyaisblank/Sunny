#include <algorithm>
#include <cmath>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/infrastructure/ableton/native_mix_plan.hpp>

namespace sunny::infrastructure {
namespace {
using namespace sunny::core;
using nlohmann::json;
NativeMixFaderPlanResult fail(std::string reason, std::string diagnostic) {
    return std::unexpected(NativeMixFaderPlanError{std::move(reason), std::move(diagnostic)});
}
std::string escaped(std::string_view value) {
    std::string result;
    for (char c : value)
        result += c == '~' ? "~0" : c == '/' ? "~1" : std::string(1, c);
    return result;
}
bool leaves(const json& value,
            const std::string& path,
            const std::string& selected,
            std::vector<NativeMixFaderResidual>& residuals) {
    if (path == selected) return true;
    if (value.is_primitive() || value.empty()) {
        if (residuals.size() >= 65536) return false;
        residuals.push_back(
            {path,
             "Retained authored leaf is not applied by this selected effective volume intent"});
        return true;
    }
    for (const auto& [key, child] : value.items())
        if (!leaves(child, path + "/" + escaped(key), selected, residuals)) return false;
    return true;
}
json resolution(const FaderLevelResolution& value) {
    const char* status = value.status == FaderLevelResolutionStatus::Explicit   ? "Explicit"
                         : value.status == FaderLevelResolutionStatus::Resolved ? "Resolved"
                         : value.status == FaderLevelResolutionStatus::RequiresLoudnessMeasurement
                             ? "RequiresLoudnessMeasurement"
                             : "BlockedByUnresolvedReference";
    return json{{"target_type", static_cast<unsigned>(value.target_type)},
                {"target_id", value.target_id},
                {"explicit_level_db", value.explicit_level_db},
                {"resolved_level_db",
                 value.resolved_level_db ? json(*value.resolved_level_db) : json(nullptr)},
                {"status", status},
                {"offset_db", value.offset_db}};
}
} // namespace

NativeMixFaderPlanResult plan_native_mix_fader(const sunny::core::MixGraph& graph,
                                               sunny::core::ChannelStripId channel,
                                               sunny::core::PartId expected_part,
                                               double tolerance_db) {
    using namespace sunny::core;
    if (graph.id.value == 0 || channel.value == 0 || expected_part.value == 0 ||
        !std::isfinite(tolerance_db) || tolerance_db < 0)
        return fail("InvalidSelection",
                    "Positive graph/channel/Part identities and finite nonnegative dB tolerance "
                    "are required");
    const auto selected = std::ranges::find(graph.channels, channel, &ChannelStrip::id);
    if (selected == graph.channels.end() || selected->part_id != expected_part)
        return fail("OwnershipMismatch",
                    "Selected actual ChannelStrip does not belong to the retained Score Part");
    for (const auto& diagnostic : validate_mix(graph))
        if (diagnostic.severity == ValidationSeverity::Error)
            return fail("InvalidMix", "Authored MixGraph is invalid: " + diagnostic.message);
    const auto levels = resolve_relative_levels(graph);
    if (!levels)
        return fail("InvalidMix",
                    "Core effective fader-level resolution rejected the authored graph");
    const auto effective = std::ranges::find_if(levels->levels, [&](const auto& item) {
        return item.target_type == FaderTargetType::Channel && item.target_id == channel.value;
    });
    if (effective == levels->levels.end() || !effective->resolved_level_db ||
        (effective->status != FaderLevelResolutionStatus::Explicit &&
         effective->status != FaderLevelResolutionStatus::Resolved))
        return fail("UnresolvedLoudness",
                    "Selected effective fader depends on programme loudness or an unresolved "
                    "reference; stored level_db is not substituted");
    if (!std::isfinite(*effective->resolved_level_db))
        return fail("UnsupportedSilence",
                    "This finite native-volume intent requires a finite effective dB target");
    NativeMixFaderPlan result;
    result.mix_id = graph.id;
    result.channel_id = channel;
    result.part_id = expected_part;
    result.target_db = *effective->resolved_level_db;
    result.tolerance_db = tolerance_db;
    result.selected_resolution = *effective;
    result.level_resolution = *levels;
    const auto index = static_cast<std::size_t>(std::distance(graph.channels.begin(), selected));
    result.source_json_pointer = "/channels/" + std::to_string(index) + "/fader/" +
                                 (selected->fader.relative_level ? "relative" : "level_db");
    result.retained_mix = mix_to_json(graph);
    if (!leaves(result.retained_mix, "", result.source_json_pointer, result.residuals))
        return fail("ExcessiveResiduals", "Retained Mix residual leaf coverage exceeds65536 paths");
    return result;
}

nlohmann::json native_mix_fader_plan_to_json(const NativeMixFaderPlan& plan) {
    json residuals = json::array(), levels = json::array();
    for (const auto& item : plan.residuals)
        residuals.push_back({{"document_pointer", item.document_pointer}, {"reason", item.reason}});
    for (const auto& item : plan.level_resolution.levels)
        levels.push_back(resolution(item));
    return json{{"schema_version", 1},
                {"mix_id", plan.mix_id.value},
                {"channel_id", plan.channel_id.value},
                {"part_id", plan.part_id.value},
                {"parameter", {{"kind", "volume"}}},
                {"unit", "Decibels"},
                {"target", plan.target_db},
                {"tolerance", plan.tolerance_db},
                {"source_json_pointer", plan.source_json_pointer},
                {"selected_resolution", resolution(plan.selected_resolution)},
                {"level_resolution", levels},
                {"retained_mix", plan.retained_mix},
                {"residuals", residuals},
                {"native_knob_only", true},
                {"host_qualified", false},
                {"dsp_equivalence_qualified", false}};
}
NativeMixStaticPlanResult plan_native_mix_static(const sunny::core::MixGraph& graph,
                                                 sunny::core::ChannelStripId channel,
                                                 sunny::core::PartId expected_part,
                                                 const NativeMixStaticSelection& selection) {
    using namespace sunny::core;
    if (!selection.volume && !selection.pan && !selection.mute && !selection.solo)
        return std::unexpected(NativeMixFaderPlanError{
            "InvalidSelection", "Select at least one finite static Mixer role"});
    if (!graph.id.value || !channel.value || !expected_part.value ||
        !std::isfinite(selection.tolerance_db) || selection.tolerance_db < 0)
        return std::unexpected(NativeMixFaderPlanError{
            "InvalidSelection",
            "Positive owning identities and finite nonnegative tolerance required"});
    const auto selected = std::ranges::find(graph.channels, channel, &ChannelStrip::id);
    if (selected == graph.channels.end() || selected->part_id != expected_part)
        return std::unexpected(NativeMixFaderPlanError{
            "OwnershipMismatch", "Selected Channel does not belong to current Score Part"});
    for (const auto& error : validate_mix(graph))
        if (error.severity == ValidationSeverity::Error)
            return std::unexpected(NativeMixFaderPlanError{"InvalidMix", error.message});
    NativeMixStaticPlan result;
    result.mix_id = graph.id;
    result.channel_id = channel;
    result.part_id = expected_part;
    result.retained_mix = mix_to_json(graph);
    const auto index = std::distance(graph.channels.begin(), selected);
    const std::string prefix = "/channels/" + std::to_string(index);
    if (selection.volume) {
        auto fader = plan_native_mix_fader(graph, channel, expected_part, selection.tolerance_db);
        if (!fader) return std::unexpected(fader.error());
        result.desired.volume = ManagedMixerVolumeIntent{fader->target_db, fader->tolerance_db};
        result.source_json_pointers.push_back(fader->source_json_pointer);
        result.fader = std::move(*fader);
    }
    if (selection.pan) {
        result.desired.pan = static_cast<double>(selected->spatial.pan);
        result.source_json_pointers.push_back(prefix + "/spatial/pan");
    }
    if (selection.mute) {
        result.desired.mute = selected->mute;
        result.source_json_pointers.push_back(prefix + "/mute");
    }
    if (selection.solo) {
        result.desired.solo = selected->solo;
        result.source_json_pointers.push_back(prefix + "/solo");
    }
    for (const auto& [key, child] : result.retained_mix.items())
        if (!leaves(child, "/" + escaped(key), "", result.residuals))
            return std::unexpected(NativeMixFaderPlanError{"ExcessiveResiduals",
                                                           "Retained leaf coverage exceeds65536"});
    std::erase_if(result.residuals, [&](const auto& item) {
        return std::ranges::any_of(result.source_json_pointers, [&](const auto& applied) {
            return item.document_pointer == applied ||
                   item.document_pointer.starts_with(applied + "/");
        });
    });
    for (auto& residual : result.residuals)
        residual.reason =
            "Retained authored leaf is outside these selected native control-coordinate intents";
    return result;
}
nlohmann::json native_mix_static_plan_to_json(const NativeMixStaticPlan& plan) {
    auto residuals = json::array();
    for (const auto& item : plan.residuals)
        residuals.push_back({{"document_pointer", item.document_pointer}, {"reason", item.reason}});
    return json{
        {"schema_version", 1},
        {"mix_id", plan.mix_id.value},
        {"channel_id", plan.channel_id.value},
        {"part_id", plan.part_id.value},
        {"desired", managed_static_mixer_desired_to_json(plan.desired)},
        {"source_json_pointers", plan.source_json_pointers},
        {"retained_mix", plan.retained_mix},
        {"fader_resolution",
         plan.fader ? native_mix_fader_plan_to_json(*plan.fader) : json(nullptr)},
        {"residuals", residuals},
        {"native_knob_only", true},
        {"host_qualified", false},
        {"dsp_equivalence_qualified", false},
        {"pan_semantics",
         "native_stereo_control_coordinate; authored pan law/spatial response remains residual"}};
}
} // namespace sunny::infrastructure
