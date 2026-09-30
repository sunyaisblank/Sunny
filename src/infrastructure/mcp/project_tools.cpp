/**
 * @file project_tools.cpp
 * @brief Cross-IR MCP validation and Ableton deployment
 */

#include "evidence_encoding.hpp"

#include <algorithm>
#include <limits>
#include <optional>
#include <string_view>
#include <sunny/core/detail/serialization_integer.hpp>
#include <sunny/core/project/validation.hpp>
#include <sunny/infrastructure/ableton/target_profile.hpp>
#include <sunny/infrastructure/ableton/validation_record.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <vector>

namespace sunny::infrastructure {

using json = nlohmann::json;
using namespace sunny::core;

namespace {

struct ValidationContext {
    AbletonOperatorEnvironment environment;
    std::vector<std::string> cleanup_steps;
};

bool valid_validation_text(const std::string& value, std::size_t maximum = 256) {
    if (value.empty() || value.size() > maximum) return false;
    return std::none_of(value.begin(), value.end(), [](const unsigned char character) {
        return character < 0x20 || character == 0x7f;
    });
}

json validation_context_schema() {
    return {
        {"type", "object"},
        {"properties",
         {{"live_edition", {{"type", "string"}}},
          {"operating_system", {{"type", "string"}}},
          {"architecture", {{"type", "string"}}},
          {"remote_script_revision", {{"type", "string"}}},
          {"max",
           {{"type", "object"},
            {"properties",
             {{"max_version", {{"type", "string"}}},
              {"max_for_live_version", {{"type", "string"}}},
              {"license_state", {{"type", "string"}}}}},
            {"required", json::array({"max_version", "max_for_live_version", "license_state"})}}},
          {"cleanup_steps", {{"type", "array"}, {"items", {{"type", "string"}}}}}}},
        {"required",
         json::array({"live_edition",
                      "operating_system",
                      "architecture",
                      "remote_script_revision",
                      "cleanup_steps"})}};
}

bool parse_validation_context(const json& params,
                              std::optional<ValidationContext>& result,
                              std::string& error) {
    if (!params.contains("validation_context")) {
        result.reset();
        return true;
    }
    const auto& encoded = params.at("validation_context");
    if (!encoded.is_object()) {
        error = "validation_context must be an object";
        return false;
    }
    const auto permitted = [](std::string_view name) {
        return name == "live_edition" || name == "operating_system" || name == "architecture" ||
               name == "remote_script_revision" || name == "max" || name == "cleanup_steps";
    };
    for (auto field = encoded.begin(); field != encoded.end(); ++field) {
        if (!permitted(field.key())) {
            error = "validation_context contains an unknown field";
            return false;
        }
    }
    for (const auto* required : {"live_edition",
                                 "operating_system",
                                 "architecture",
                                 "remote_script_revision",
                                 "cleanup_steps"}) {
        if (!encoded.contains(required)) {
            error = std::string{"validation_context."} + required + " is required";
            return false;
        }
    }
    if (!encoded.at("live_edition").is_string() || !encoded.at("operating_system").is_string() ||
        !encoded.at("architecture").is_string() ||
        !encoded.at("remote_script_revision").is_string() ||
        !encoded.at("cleanup_steps").is_array()) {
        error = "validation_context contains an invalid field type";
        return false;
    }

    ValidationContext context;
    context.environment.live_edition = encoded.at("live_edition").get<std::string>();
    context.environment.operating_system = encoded.at("operating_system").get<std::string>();
    context.environment.architecture = encoded.at("architecture").get<std::string>();
    context.environment.remote_script_revision =
        encoded.at("remote_script_revision").get<std::string>();
    if (!valid_validation_text(context.environment.live_edition) ||
        !valid_validation_text(context.environment.operating_system) ||
        !valid_validation_text(context.environment.architecture) ||
        !valid_validation_text(context.environment.remote_script_revision)) {
        error = "validation_context environment strings must be nonempty printable text";
        return false;
    }
    for (const auto& step : encoded.at("cleanup_steps")) {
        if (!step.is_string() || !valid_validation_text(step.get_ref<const std::string&>(), 2048)) {
            error = "validation_context.cleanup_steps must contain printable nonempty strings";
            return false;
        }
        context.cleanup_steps.push_back(step.get<std::string>());
    }
    if (encoded.contains("max")) {
        const auto& max = encoded.at("max");
        if (!max.is_object() || max.size() != 3 || !max.contains("max_version") ||
            !max.at("max_version").is_string() || !max.contains("max_for_live_version") ||
            !max.at("max_for_live_version").is_string() || !max.contains("license_state") ||
            !max.at("license_state").is_string()) {
            error = "validation_context.max must contain exactly three string fields";
            return false;
        }
        context.environment.max =
            AbletonMaxOperatorEnvironment{max.at("max_version").get<std::string>(),
                                          max.at("max_for_live_version").get<std::string>(),
                                          max.at("license_state").get<std::string>()};
        if (!valid_validation_text(context.environment.max->max_version) ||
            !valid_validation_text(context.environment.max->max_for_live_version) ||
            !valid_validation_text(context.environment.max->license_state)) {
            error = "validation_context.max strings must be nonempty printable text";
            return false;
        }
    }
    result = std::move(context);
    return true;
}

void attach_validation_record(json& output,
                              const formats::AbletonProjectDeploymentPlan& plan,
                              const formats::AbletonProjectDeploymentAttempt& attempt,
                              std::optional<ValidationContext> context,
                              const std::optional<json>& encoded_compilation) {
    output["validation_record"] = nullptr;
    output["validation_record_context_supplied"] = context.has_value();
    if (!context) return;
    auto record = make_ableton_validation_record(plan,
                                                 attempt,
                                                 std::move(context->environment),
                                                 encoded_compilation,
                                                 std::move(context->cleanup_steps));
    if (!record) {
        output["validation_record_error_code"] = static_cast<int>(record.error());
        return;
    }
    output["validation_record"] = ableton_validation_record_to_json(*record);
}

struct ResolvedProject {
    std::uint64_t score_id = 0;
    std::vector<std::uint64_t> profile_ids;
    std::uint64_t mix_graph_id = 0;
    const Score* score = nullptr;
    std::vector<const TimbreProfile*> profiles;
    const MixGraph* mix = nullptr;

    [[nodiscard]] ProjectView view() const { return {*score, profiles, *mix}; }
};

json error_response(const std::string& message) {
    return {{"error", message}};
}

json mix_fader_level_resolution_j(const RelativeLevelResolution& resolution) {
    const auto target_name = [](FaderTargetType type) {
        if (type == FaderTargetType::Channel) return "channel";
        if (type == FaderTargetType::Group) return "group";
        return "master";
    };
    const auto status_name = [](FaderLevelResolutionStatus status) {
        if (status == FaderLevelResolutionStatus::Explicit) return "explicit";
        if (status == FaderLevelResolutionStatus::Resolved) return "resolved";
        if (status == FaderLevelResolutionStatus::RequiresLoudnessMeasurement)
            return "requires_loudness_measurement";
        return "blocked_by_unresolved_reference";
    };
    const auto reference_j = [](const std::optional<LevelReference>& reference) -> json {
        if (!reference) return nullptr;
        if (reference->type == LevelReferenceType::MasterTarget)
            return {{"type", "master_target"}, {"lufs", reference->lufs}};
        if (reference->type == LevelReferenceType::Channel)
            return {{"type", "channel"},
                    {"id", reference->channel_id.value},
                    {"relationship", reference->relationship}};
        return {{"type", "group"}, {"id", reference->group_id.value}};
    };

    json levels = json::array();
    for (const auto& level : resolution.levels) {
        levels.push_back(
            {{"target_type", target_name(level.target_type)},
             {"target_id", level.target_id},
             {"explicit_level_db", level.explicit_level_db},
             {"resolved_level_db",
              level.resolved_level_db ? json(*level.resolved_level_db) : json(nullptr)},
             {"status", status_name(level.status)},
             {"reference", reference_j(level.reference)},
             {"offset_db", level.offset_db}});
    }
    return {{"complete", resolution.complete()},
            {"relative_levels_total", resolution.relative_levels_total},
            {"relative_levels_resolved", resolution.relative_levels_resolved},
            {"relative_levels_unresolved", resolution.relative_levels_unresolved},
            {"levels", std::move(levels)}};
}

std::optional<ResolvedProject> resolve_project(const json& params,
                                               const std::shared_ptr<ScoreSession>& scores,
                                               const std::shared_ptr<TimbreSession>& timbres,
                                               const std::shared_ptr<MixSession>& mixes,
                                               std::string& error) {
    if (!params.contains("score_id") || !params.contains("timbre_profile_ids") ||
        !params.contains("mix_graph_id")) {
        error = "score_id, timbre_profile_ids, and mix_graph_id are required";
        return std::nullopt;
    }
    if (!params.at("timbre_profile_ids").is_array()) {
        error = "timbre_profile_ids must be an array";
        return std::nullopt;
    }

    ResolvedProject resolved;
    const auto score_id = detail::checked_integer<std::uint64_t>(params.at("score_id"), "score id");
    resolved.score_id = score_id;
    resolved.score = scores->find(score_id);
    if (resolved.score == nullptr) {
        error = "Score not found: " + std::to_string(score_id);
        return std::nullopt;
    }

    resolved.profiles.reserve(params.at("timbre_profile_ids").size());
    for (const auto& encoded_id : params.at("timbre_profile_ids")) {
        const auto profile_id =
            detail::checked_integer<std::uint64_t>(encoded_id, "timbre profile id");
        resolved.profile_ids.push_back(profile_id);
        const auto* profile = timbres->find(profile_id);
        if (profile == nullptr) {
            error = "Timbre profile not found: " + std::to_string(profile_id);
            return std::nullopt;
        }
        resolved.profiles.push_back(profile);
    }

    const auto graph_id =
        detail::checked_integer<std::uint64_t>(params.at("mix_graph_id"), "mix graph id");
    resolved.mix_graph_id = graph_id;
    resolved.mix = mixes->find(graph_id);
    if (resolved.mix == nullptr) {
        error = "Mix graph not found: " + std::to_string(graph_id);
        return std::nullopt;
    }
    return resolved;
}

json routing_options_j(const std::vector<formats::AbletonRoutingOptionEvidence>& options) {
    json encoded = json::array();
    for (const auto& option : options)
        encoded.push_back(
            {{"display_name", option.display_name}, {"identifier", option.identifier}});
    return encoded;
}

json input_routing_evidence_j(const formats::AbletonInputRoutingEvidence& evidence) {
    return {
        {"selected_type_display_name",
         evidence.selected_type_display_name ? json(*evidence.selected_type_display_name)
                                             : json(nullptr)},
        {"selected_type_identifier",
         evidence.selected_type_identifier ? json(*evidence.selected_type_identifier)
                                           : json(nullptr)},
        {"selected_channel_display_name",
         evidence.selected_channel_display_name ? json(*evidence.selected_channel_display_name)
                                                : json(nullptr)},
        {"selected_channel_identifier",
         evidence.selected_channel_identifier ? json(*evidence.selected_channel_identifier)
                                              : json(nullptr)},
        {"available_types", routing_options_j(evidence.available_types)},
        {"available_channels", routing_options_j(evidence.available_channels)},
        {"selected_input_observed", evidence.selected_input_observed},
        {"selected_type_available_verified", evidence.selected_type_available_verified},
        {"selected_channel_available_verified", evidence.selected_channel_available_verified},
        {"selected_input_available_verified", evidence.selected_input_available_verified},
        {"input_source_identity_mapped", evidence.input_source_identity_mapped},
        {"external_input_neutrality_verified", evidence.external_input_neutrality_verified},
    };
}

json clip_slot_state_evidence_j(const std::vector<formats::AbletonClipSlotStateEvidence>& slots) {
    json encoded = json::array();
    for (const auto& slot : slots) {
        encoded.push_back({{"slot", slot.slot},
                           {"has_clip", slot.has_clip},
                           {"has_stop_button", slot.has_stop_button},
                           {"is_group_slot", slot.is_group_slot},
                           {"controls_other_clips", slot.controls_other_clips},
                           {"is_playing", slot.is_playing},
                           {"is_recording", slot.is_recording},
                           {"is_triggered", slot.is_triggered},
                           {"playing_status", slot.playing_status},
                           {"will_record_on_start", slot.will_record_on_start}});
    }
    return encoded;
}

json output_routing_evidence_j(const formats::AbletonOutputRoutingEvidence& evidence) {
    const auto binding = [&]() -> json {
        if (!evidence.requested_binding) return nullptr;
        return {{"type",
                 {{"display_name", evidence.requested_binding->type.display_name},
                  {"identifier", evidence.requested_binding->type.identifier}}},
                {"channel",
                 {{"display_name", evidence.requested_binding->channel.display_name},
                  {"identifier", evidence.requested_binding->channel.identifier}}},
                {"mapping_provenance", evidence.requested_binding->mapping_provenance}};
    }();
    return {
        {"requested_destination",
         evidence.requested_destination == formats::AbletonOutputDestination::Master ? "master"
                                                                                     : "group"},
        {"requested_group_id",
         evidence.requested_group_id ? json(evidence.requested_group_id->value) : json(nullptr)},
        {"requested_binding", binding},
        {"selected_type_display_name",
         evidence.selected_type_display_name ? json(*evidence.selected_type_display_name)
                                             : json(nullptr)},
        {"selected_type_identifier",
         evidence.selected_type_identifier ? json(*evidence.selected_type_identifier)
                                           : json(nullptr)},
        {"selected_channel_display_name",
         evidence.selected_channel_display_name ? json(*evidence.selected_channel_display_name)
                                                : json(nullptr)},
        {"selected_channel_identifier",
         evidence.selected_channel_identifier ? json(*evidence.selected_channel_identifier)
                                              : json(nullptr)},
        {"available_types", routing_options_j(evidence.available_types)},
        {"available_channels", routing_options_j(evidence.available_channels)},
        {"selected_output_observed", evidence.selected_output_observed},
        {"selected_type_available_verified", evidence.selected_type_available_verified},
        {"selected_channel_available_verified", evidence.selected_channel_available_verified},
        {"selected_output_available_verified", evidence.selected_output_available_verified},
        {"source_target_identity_mapped", evidence.source_target_identity_mapped},
        {"verified", evidence.verified},
    };
}

json track_gate_evidence_j(const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.track_gates) {
        encoded.push_back(
            {{"part_id", evidence.part_id.value},
             {"track_index", evidence.track_index},
             {"requested_name", evidence.requested_name},
             {"requested_mute", evidence.requested_mute},
             {"requested_solo", evidence.requested_solo},
             {"requested_has_audio_input", evidence.requested_has_audio_input},
             {"requested_has_midi_input", evidence.requested_has_midi_input},
             {"requested_input_meter_level", evidence.requested_input_meter_level},
             {"requested_output_meter_level", evidence.requested_output_meter_level},
             {"requested_is_frozen", evidence.requested_is_frozen},
             {"requested_arm", evidence.requested_arm},
             {"requested_implicit_arm", evidence.requested_implicit_arm},
             {"requested_back_to_arranger", evidence.requested_back_to_arranger},
             {"requested_fired_slot_index", evidence.requested_fired_slot_index},
             {"requested_playing_slot_index", evidence.requested_playing_slot_index},
             {"requested_clip_slot_is_group_slot", evidence.requested_clip_slot_is_group_slot},
             {"requested_clip_slot_controls_other_clips",
              evidence.requested_clip_slot_controls_other_clips},
             {"requested_clip_slot_is_playing", evidence.requested_clip_slot_is_playing},
             {"requested_clip_slot_is_recording", evidence.requested_clip_slot_is_recording},
             {"requested_clip_slot_is_triggered", evidence.requested_clip_slot_is_triggered},
             {"requested_clip_slot_playing_status", evidence.requested_clip_slot_playing_status},
             {"requested_clip_slot_will_record_on_start",
              evidence.requested_clip_slot_will_record_on_start},
             {"requested_arrangement_clip_count", evidence.requested_arrangement_clip_count},
             {"requested_take_lane_count", evidence.requested_take_lane_count},
             {"requested_group_track_index",
              evidence.requested_group_track_index ? json(*evidence.requested_group_track_index)
                                                   : json(nullptr)},
             {"requested_crossfade_assign", evidence.requested_crossfade_assign},
             {"requested_panning_mode", evidence.requested_panning_mode},
             {"requested_track_activator", evidence.requested_track_activator},
             {"requested_input_meter_left", evidence.requested_input_meter_left},
             {"requested_input_meter_right", evidence.requested_input_meter_right},
             {"requested_output_meter_left", evidence.requested_output_meter_left},
             {"requested_output_meter_right", evidence.requested_output_meter_right},
             {"expected_mixer_enabled", evidence.expected_mixer_enabled},
             {"observed_name",
              evidence.observed_name ? json(*evidence.observed_name) : json(nullptr)},
             {"observed_has_audio_input",
              evidence.observed_has_audio_input ? json(*evidence.observed_has_audio_input)
                                                : json(nullptr)},
             {"observed_has_midi_input",
              evidence.observed_has_midi_input ? json(*evidence.observed_has_midi_input)
                                               : json(nullptr)},
             {"input_routing", input_routing_evidence_j(evidence.input_routing)},
             {"observed_input_meter_level",
              evidence.observed_input_meter_level ? json(*evidence.observed_input_meter_level)
                                                  : json(nullptr)},
             {"observed_output_meter_level",
              evidence.observed_output_meter_level ? json(*evidence.observed_output_meter_level)
                                                   : json(nullptr)},
             {"observed_input_meter_left",
              evidence.observed_input_meter_left ? json(*evidence.observed_input_meter_left)
                                                 : json(nullptr)},
             {"observed_input_meter_right",
              evidence.observed_input_meter_right ? json(*evidence.observed_input_meter_right)
                                                  : json(nullptr)},
             {"observed_output_meter_left",
              evidence.observed_output_meter_left ? json(*evidence.observed_output_meter_left)
                                                  : json(nullptr)},
             {"observed_output_meter_right",
              evidence.observed_output_meter_right ? json(*evidence.observed_output_meter_right)
                                                   : json(nullptr)},
             {"observed_has_audio_output",
              evidence.observed_has_audio_output ? json(*evidence.observed_has_audio_output)
                                                 : json(nullptr)},
             {"observed_has_midi_output",
              evidence.observed_has_midi_output ? json(*evidence.observed_has_midi_output)
                                                : json(nullptr)},
             {"output_routing", output_routing_evidence_j(evidence.output_routing)},
             {"observed_is_frozen",
              evidence.observed_is_frozen ? json(*evidence.observed_is_frozen) : json(nullptr)},
             {"observed_arm", evidence.observed_arm ? json(*evidence.observed_arm) : json(nullptr)},
             {"observed_implicit_arm",
              evidence.observed_implicit_arm ? json(*evidence.observed_implicit_arm)
                                             : json(nullptr)},
             {"observed_back_to_arranger",
              evidence.observed_back_to_arranger ? json(*evidence.observed_back_to_arranger)
                                                 : json(nullptr)},
             {"observed_fired_slot_index",
              evidence.observed_fired_slot_index ? json(*evidence.observed_fired_slot_index)
                                                 : json(nullptr)},
             {"observed_playing_slot_index",
              evidence.observed_playing_slot_index ? json(*evidence.observed_playing_slot_index)
                                                   : json(nullptr)},
             {"observed_arrangement_clip_count",
              evidence.observed_arrangement_clip_count
                  ? json(*evidence.observed_arrangement_clip_count)
                  : json(nullptr)},
             {"observed_take_lane_count",
              evidence.observed_take_lane_count ? json(*evidence.observed_take_lane_count)
                                                : json(nullptr)},
             {"observed_clip_slots", clip_slot_state_evidence_j(evidence.observed_clip_slots)},
             {"group_membership_observed", evidence.group_membership_observed},
             {"observed_group_track_index",
              evidence.observed_group_track_index ? json(*evidence.observed_group_track_index)
                                                  : json(nullptr)},
             {"observed_crossfade_assign",
              evidence.observed_crossfade_assign ? json(*evidence.observed_crossfade_assign)
                                                 : json(nullptr)},
             {"observed_panning_mode",
              evidence.observed_panning_mode ? json(*evidence.observed_panning_mode)
                                             : json(nullptr)},
             {"observed_mute",
              evidence.observed_mute ? json(*evidence.observed_mute) : json(nullptr)},
             {"observed_solo",
              evidence.observed_solo ? json(*evidence.observed_solo) : json(nullptr)},
             {"observed_muted_via_solo",
              evidence.observed_muted_via_solo ? json(*evidence.observed_muted_via_solo)
                                               : json(nullptr)},
             {"observed_track_activator",
              evidence.observed_track_activator ? json(*evidence.observed_track_activator)
                                                : json(nullptr)},
             {"observed_track_activator_minimum",
              evidence.observed_track_activator_minimum
                  ? json(*evidence.observed_track_activator_minimum)
                  : json(nullptr)},
             {"observed_track_activator_maximum",
              evidence.observed_track_activator_maximum
                  ? json(*evidence.observed_track_activator_maximum)
                  : json(nullptr)},
             {"observed_track_activator_is_quantized",
              evidence.observed_track_activator_is_quantized
                  ? json(*evidence.observed_track_activator_is_quantized)
                  : json(nullptr)},
             {"observed_track_activator_is_enabled",
              evidence.observed_track_activator_is_enabled
                  ? json(*evidence.observed_track_activator_is_enabled)
                  : json(nullptr)},
             {"observed_track_activator_state",
              evidence.observed_track_activator_state
                  ? json(*evidence.observed_track_activator_state)
                  : json(nullptr)},
             {"observed_track_activator_automation_state",
              evidence.observed_track_activator_automation_state
                  ? json(*evidence.observed_track_activator_automation_state)
                  : json(nullptr)},
             {"input_classification_verified", evidence.input_classification_verified},
             {"meter_levels_observed", evidence.meter_levels_observed},
             {"input_meter_hold_quiescent_verified", evidence.input_meter_hold_quiescent_verified},
             {"output_meter_hold_quiescent_verified",
              evidence.output_meter_hold_quiescent_verified},
             {"meter_hold_quiescence_verified", evidence.meter_hold_quiescence_verified},
             {"momentary_meter_levels_observed", evidence.momentary_meter_levels_observed},
             {"input_stereo_momentary_quiescent_verified",
              evidence.input_stereo_momentary_quiescent_verified},
             {"output_stereo_momentary_quiescent_verified",
              evidence.output_stereo_momentary_quiescent_verified},
             {"meter_momentary_quiescence_verified", evidence.meter_momentary_quiescence_verified},
             {"meter_quiescence_verified", evidence.meter_quiescence_verified},
             {"continuous_input_silence_verified", evidence.continuous_input_silence_verified},
             {"continuous_output_silence_verified", evidence.continuous_output_silence_verified},
             {"unfrozen_verified", evidence.unfrozen_verified},
             {"disarmed_verified", evidence.disarmed_verified},
             {"implicitly_disarmed_verified", evidence.implicitly_disarmed_verified},
             {"session_launch_quiescent_verified", evidence.session_launch_quiescent_verified},
             {"track_arrangement_playback_aligned_verified",
              evidence.track_arrangement_playback_aligned_verified},
             {"clip_slot_states_observed", evidence.clip_slot_states_observed},
             {"clip_slot_non_group_semantics_verified",
              evidence.clip_slot_non_group_semantics_verified},
             {"clip_slot_playback_idle_verified", evidence.clip_slot_playback_idle_verified},
             {"clip_slot_recording_quiescence_verified",
              evidence.clip_slot_recording_quiescence_verified},
             {"clip_slot_launch_quiescence_verified",
              evidence.clip_slot_launch_quiescence_verified},
             {"clip_slot_runtime_quiescence_verified",
              evidence.clip_slot_runtime_quiescence_verified},
             {"arrangement_clip_topology_observed", evidence.arrangement_clip_topology_observed},
             {"arrangement_content_absent_verified", evidence.arrangement_content_absent_verified},
             {"take_lane_topology_observed", evidence.take_lane_topology_observed},
             {"take_lanes_absent_verified", evidence.take_lanes_absent_verified},
             {"ungrouped_verified", evidence.ungrouped_verified},
             {"crossfade_neutral_verified", evidence.crossfade_neutral_verified},
             {"stereo_panning_verified", evidence.stereo_panning_verified},
             {"track_activator_range_verified", evidence.track_activator_range_verified},
             {"track_activator_quantization_verified",
              evidence.track_activator_quantization_verified},
             {"track_activator_verified", evidence.track_activator_verified},
             {"monitoring_state_observed", evidence.monitoring_state_observed},
             {"clip_output_not_suppressed_by_monitoring_verified",
              evidence.clip_output_not_suppressed_by_monitoring_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json return_track_gate_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.return_track_gates) {
        encoded.push_back(
            {{"aux_bus_id", evidence.aux_bus_id.value},
             {"track_index", evidence.track_index},
             {"requested_name", evidence.requested_name},
             {"requested_mute", evidence.requested_mute},
             {"requested_solo", evidence.requested_solo},
             {"requested_crossfade_assign", evidence.requested_crossfade_assign},
             {"requested_panning_mode", evidence.requested_panning_mode},
             {"requested_track_activator", evidence.requested_track_activator},
             {"expected_mixer_enabled", evidence.expected_mixer_enabled},
             {"observed_name",
              evidence.observed_name ? json(*evidence.observed_name) : json(nullptr)},
             {"output_routing", output_routing_evidence_j(evidence.output_routing)},
             {"observed_mute",
              evidence.observed_mute ? json(*evidence.observed_mute) : json(nullptr)},
             {"observed_solo",
              evidence.observed_solo ? json(*evidence.observed_solo) : json(nullptr)},
             {"observed_muted_via_solo",
              evidence.observed_muted_via_solo ? json(*evidence.observed_muted_via_solo)
                                               : json(nullptr)},
             {"observed_crossfade_assign",
              evidence.observed_crossfade_assign ? json(*evidence.observed_crossfade_assign)
                                                 : json(nullptr)},
             {"observed_panning_mode",
              evidence.observed_panning_mode ? json(*evidence.observed_panning_mode)
                                             : json(nullptr)},
             {"observed_track_activator",
              evidence.observed_track_activator ? json(*evidence.observed_track_activator)
                                                : json(nullptr)},
             {"observed_track_activator_minimum",
              evidence.observed_track_activator_minimum
                  ? json(*evidence.observed_track_activator_minimum)
                  : json(nullptr)},
             {"observed_track_activator_maximum",
              evidence.observed_track_activator_maximum
                  ? json(*evidence.observed_track_activator_maximum)
                  : json(nullptr)},
             {"observed_track_activator_is_quantized",
              evidence.observed_track_activator_is_quantized
                  ? json(*evidence.observed_track_activator_is_quantized)
                  : json(nullptr)},
             {"observed_track_activator_is_enabled",
              evidence.observed_track_activator_is_enabled
                  ? json(*evidence.observed_track_activator_is_enabled)
                  : json(nullptr)},
             {"observed_track_activator_state",
              evidence.observed_track_activator_state
                  ? json(*evidence.observed_track_activator_state)
                  : json(nullptr)},
             {"observed_track_activator_automation_state",
              evidence.observed_track_activator_automation_state
                  ? json(*evidence.observed_track_activator_automation_state)
                  : json(nullptr)},
             {"crossfade_neutral_verified", evidence.crossfade_neutral_verified},
             {"stereo_panning_verified", evidence.stereo_panning_verified},
             {"track_activator_range_verified", evidence.track_activator_range_verified},
             {"track_activator_quantization_verified",
              evidence.track_activator_quantization_verified},
             {"track_activator_verified", evidence.track_activator_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json master_track_gate_evidence_j(
    const std::optional<formats::AbletonMasterTrackGateEvidence>& evidence) {
    if (!evidence) return nullptr;
    return {
        {"requested_track_activator", evidence->requested_track_activator},
        {"requested_panning_mode", evidence->requested_panning_mode},
        {"requested_pan", evidence->requested_pan},
        {"observed_track_activator",
         evidence->observed_track_activator ? json(*evidence->observed_track_activator)
                                            : json(nullptr)},
        {"observed_track_activator_minimum",
         evidence->observed_track_activator_minimum
             ? json(*evidence->observed_track_activator_minimum)
             : json(nullptr)},
        {"observed_track_activator_maximum",
         evidence->observed_track_activator_maximum
             ? json(*evidence->observed_track_activator_maximum)
             : json(nullptr)},
        {"observed_track_activator_is_quantized",
         evidence->observed_track_activator_is_quantized
             ? json(*evidence->observed_track_activator_is_quantized)
             : json(nullptr)},
        {"observed_track_activator_is_enabled",
         evidence->observed_track_activator_is_enabled
             ? json(*evidence->observed_track_activator_is_enabled)
             : json(nullptr)},
        {"observed_track_activator_state",
         evidence->observed_track_activator_state ? json(*evidence->observed_track_activator_state)
                                                  : json(nullptr)},
        {"observed_track_activator_automation_state",
         evidence->observed_track_activator_automation_state
             ? json(*evidence->observed_track_activator_automation_state)
             : json(nullptr)},
        {"observed_panning_mode",
         evidence->observed_panning_mode ? json(*evidence->observed_panning_mode) : json(nullptr)},
        {"observed_pan", evidence->observed_pan ? json(*evidence->observed_pan) : json(nullptr)},
        {"observed_pan_minimum",
         evidence->observed_pan_minimum ? json(*evidence->observed_pan_minimum) : json(nullptr)},
        {"observed_pan_maximum",
         evidence->observed_pan_maximum ? json(*evidence->observed_pan_maximum) : json(nullptr)},
        {"observed_pan_is_quantized",
         evidence->observed_pan_is_quantized ? json(*evidence->observed_pan_is_quantized)
                                             : json(nullptr)},
        {"observed_pan_is_enabled",
         evidence->observed_pan_is_enabled ? json(*evidence->observed_pan_is_enabled)
                                           : json(nullptr)},
        {"observed_pan_state",
         evidence->observed_pan_state ? json(*evidence->observed_pan_state) : json(nullptr)},
        {"observed_pan_automation_state",
         evidence->observed_pan_automation_state ? json(*evidence->observed_pan_automation_state)
                                                 : json(nullptr)},
        {"track_activator_range_verified", evidence->track_activator_range_verified},
        {"track_activator_quantization_verified", evidence->track_activator_quantization_verified},
        {"track_activator_verified", evidence->track_activator_verified},
        {"stereo_panning_verified", evidence->stereo_panning_verified},
        {"pan_range_verified", evidence->pan_range_verified},
        {"pan_quantization_verified", evidence->pan_quantization_verified},
        {"neutral_pan_verified", evidence->neutral_pan_verified},
        {"verified", evidence->verified},
    };
}

json device_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.devices) {
        encoded.push_back(
            {{"device_path", evidence.device_path},
             {"requested_name", evidence.requested_name},
             {"requested_index", evidence.requested_index},
             {"requested_type", evidence.requested_type},
             {"expected_chain_size", evidence.expected_chain_size},
             {"observed_chain_size",
              evidence.observed_chain_size ? json(*evidence.observed_chain_size) : json(nullptr)},
             {"observed_name",
              evidence.observed_name ? json(*evidence.observed_name) : json(nullptr)},
             {"observed_class_display_name",
              evidence.observed_class_display_name ? json(*evidence.observed_class_display_name)
                                                   : json(nullptr)},
             {"observed_class_name",
              evidence.observed_class_name ? json(*evidence.observed_class_name) : json(nullptr)},
             {"observed_type",
              evidence.observed_type ? json(*evidence.observed_type) : json(nullptr)},
             {"observed_active",
              evidence.observed_active ? json(*evidence.observed_active) : json(nullptr)},
             {"observed_can_have_chains",
              evidence.observed_can_have_chains ? json(*evidence.observed_can_have_chains)
                                                : json(nullptr)},
             {"observed_latency_in_samples",
              evidence.observed_latency_in_samples ? json(*evidence.observed_latency_in_samples)
                                                   : json(nullptr)},
             {"observed_latency_in_ms",
              evidence.observed_latency_in_ms ? json(*evidence.observed_latency_in_ms)
                                              : json(nullptr)},
             {"reported_latency_observed", evidence.reported_latency_observed},
             {"flat_device_verified", evidence.flat_device_verified},
             {"render_path_latency_fully_observed", evidence.render_path_latency_fully_observed},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json clip_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.clips) {
        encoded.push_back(
            {{"part_id", evidence.part_id.value},
             {"track_index", evidence.track_index},
             {"slot_index", evidence.slot_index},
             {"requested_occupied_slot_indices", evidence.requested_occupied_slot_indices},
             {"requested_name", evidence.requested_name},
             {"requested_is_audio_clip", evidence.requested_is_audio_clip},
             {"requested_is_midi_clip", evidence.requested_is_midi_clip},
             {"requested_is_arrangement_clip", evidence.requested_is_arrangement_clip},
             {"location_identity_available", evidence.location_identity_available},
             {"requested_is_session_clip", evidence.requested_is_session_clip},
             {"requested_is_take_lane_clip", evidence.requested_is_take_lane_clip},
             {"requested_length", evidence.requested_length},
             {"requested_signature_numerator", evidence.requested_signature_numerator},
             {"requested_signature_denominator", evidence.requested_signature_denominator},
             {"requested_start_marker", evidence.requested_start_marker},
             {"requested_end_marker", evidence.requested_end_marker},
             {"requested_end_time", evidence.requested_end_time},
             {"requested_looping", evidence.requested_looping},
             {"requested_muted", evidence.requested_muted},
             {"requested_has_envelopes", evidence.requested_has_envelopes},
             {"requested_is_playing", evidence.requested_is_playing},
             {"requested_is_recording", evidence.requested_is_recording},
             {"requested_is_overdubbing", evidence.requested_is_overdubbing},
             {"requested_is_triggered", evidence.requested_is_triggered},
             {"requested_will_record_on_start", evidence.requested_will_record_on_start},
             {"groove_state_available", evidence.groove_state_available},
             {"requested_has_groove", evidence.requested_has_groove},
             {"launch_state_available", evidence.launch_state_available},
             {"requested_launch_mode", evidence.requested_launch_mode},
             {"requested_launch_quantization", evidence.requested_launch_quantization},
             {"requested_legato", evidence.requested_legato},
             {"requested_velocity_amount", evidence.requested_velocity_amount},
             {"observed_occupied_slot_indices", evidence.observed_occupied_slot_indices},
             {"exact_slot_occupancy_verified", evidence.exact_slot_occupancy_verified},
             {"observed_name",
              evidence.observed_name ? json(*evidence.observed_name) : json(nullptr)},
             {"observed_is_audio_clip",
              evidence.observed_is_audio_clip ? json(*evidence.observed_is_audio_clip)
                                              : json(nullptr)},
             {"observed_is_midi_clip",
              evidence.observed_is_midi_clip ? json(*evidence.observed_is_midi_clip)
                                             : json(nullptr)},
             {"observed_is_arrangement_clip",
              evidence.observed_is_arrangement_clip ? json(*evidence.observed_is_arrangement_clip)
                                                    : json(nullptr)},
             {"observed_is_session_clip",
              evidence.observed_is_session_clip ? json(*evidence.observed_is_session_clip)
                                                : json(nullptr)},
             {"observed_is_take_lane_clip",
              evidence.observed_is_take_lane_clip ? json(*evidence.observed_is_take_lane_clip)
                                                  : json(nullptr)},
             {"clip_identity_verified", evidence.clip_identity_verified},
             {"observed_length",
              evidence.observed_length ? json(*evidence.observed_length) : json(nullptr)},
             {"observed_signature_numerator",
              evidence.observed_signature_numerator ? json(*evidence.observed_signature_numerator)
                                                    : json(nullptr)},
             {"observed_signature_denominator",
              evidence.observed_signature_denominator
                  ? json(*evidence.observed_signature_denominator)
                  : json(nullptr)},
             {"observed_start_marker",
              evidence.observed_start_marker ? json(*evidence.observed_start_marker)
                                             : json(nullptr)},
             {"observed_end_marker",
              evidence.observed_end_marker ? json(*evidence.observed_end_marker) : json(nullptr)},
             {"observed_end_time",
              evidence.observed_end_time ? json(*evidence.observed_end_time) : json(nullptr)},
             {"playback_end_verified", evidence.playback_end_verified},
             {"observed_looping",
              evidence.observed_looping ? json(*evidence.observed_looping) : json(nullptr)},
             {"observed_muted",
              evidence.observed_muted ? json(*evidence.observed_muted) : json(nullptr)},
             {"observed_has_envelopes",
              evidence.observed_has_envelopes ? json(*evidence.observed_has_envelopes)
                                              : json(nullptr)},
             {"envelope_absence_verified", evidence.envelope_absence_verified},
             {"observed_is_playing",
              evidence.observed_is_playing ? json(*evidence.observed_is_playing) : json(nullptr)},
             {"observed_is_recording",
              evidence.observed_is_recording ? json(*evidence.observed_is_recording)
                                             : json(nullptr)},
             {"observed_is_overdubbing",
              evidence.observed_is_overdubbing ? json(*evidence.observed_is_overdubbing)
                                               : json(nullptr)},
             {"observed_is_triggered",
              evidence.observed_is_triggered ? json(*evidence.observed_is_triggered)
                                             : json(nullptr)},
             {"observed_will_record_on_start",
              evidence.observed_will_record_on_start ? json(*evidence.observed_will_record_on_start)
                                                     : json(nullptr)},
             {"runtime_state_observed", evidence.runtime_state_observed},
             {"recording_quiescence_verified", evidence.recording_quiescence_verified},
             {"playback_idle_verified", evidence.playback_idle_verified},
             {"observed_has_groove",
              evidence.observed_has_groove ? json(*evidence.observed_has_groove) : json(nullptr)},
             {"groove_absence_verified", evidence.groove_absence_verified},
             {"observed_launch_mode",
              evidence.observed_launch_mode ? json(*evidence.observed_launch_mode) : json(nullptr)},
             {"observed_launch_quantization",
              evidence.observed_launch_quantization ? json(*evidence.observed_launch_quantization)
                                                    : json(nullptr)},
             {"observed_legato",
              evidence.observed_legato ? json(*evidence.observed_legato) : json(nullptr)},
             {"observed_velocity_amount",
              evidence.observed_velocity_amount ? json(*evidence.observed_velocity_amount)
                                                : json(nullptr)},
             {"launch_behavior_verified", evidence.launch_behavior_verified},
             {"follow_actions_observed", evidence.follow_actions_observed},
             {"one_shot_playback_verified", evidence.one_shot_playback_verified},
             {"midi_bank_program_state_observed", evidence.midi_bank_program_state_observed},
             {"program_change_suppression_verified", evidence.program_change_suppression_verified},
             {"mpe_note_expression_state_observed", evidence.mpe_note_expression_state_observed},
             {"mpe_note_expression_neutrality_verified",
              evidence.mpe_note_expression_neutrality_verified},
             {"audible_timing_verified", evidence.audible_timing_verified},
             {"audible_velocity_verified", evidence.audible_velocity_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json note_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.note_batches) {
        json requested = json::array();
        for (const auto& note : evidence.requested_notes) {
            requested.push_back({{"pitch", note.pitch},
                                 {"start_time", note.start_time},
                                 {"duration", note.duration},
                                 {"velocity", note.velocity},
                                 {"mute", note.muted},
                                 {"probability", note.probability},
                                 {"velocity_deviation", note.velocity_deviation},
                                 {"release_velocity", note.release_velocity}});
        }
        json observed = json::array();
        for (const auto& note : evidence.observed_notes) {
            observed.push_back({{"note_id", note.note_id},
                                {"pitch", note.pitch},
                                {"start_time", note.start_time},
                                {"duration", note.duration},
                                {"velocity", note.velocity},
                                {"mute", note.muted},
                                {"probability", note.probability},
                                {"velocity_deviation", note.velocity_deviation},
                                {"release_velocity", note.release_velocity}});
        }
        encoded.push_back(
            {{"part_id", evidence.part_id.value},
             {"track_index", evidence.track_index},
             {"slot_index", evidence.slot_index},
             {"deployment_action", formats::note_action_name(evidence.deployment_action)},
             {"requested_notes", std::move(requested)},
             {"requested_note_ids", evidence.requested_note_ids},
             {"observed_notes", std::move(observed)},
             {"identity_verified", evidence.identity_verified},
             {"properties_verified", evidence.properties_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json song_postcondition_evidence_j(
    const std::optional<formats::AbletonSongPostconditionEvidence>& evidence) {
    if (!evidence) return nullptr;
    return {
        {"requested_tempo", evidence->requested_tempo},
        {"requested_signature_numerator", evidence->requested_signature_numerator},
        {"requested_signature_denominator", evidence->requested_signature_denominator},
        {"requested_scene_name", evidence->requested_scene_name},
        {"requested_scene_triggered", evidence->requested_scene_triggered},
        {"requested_scene_tempo_enabled", evidence->requested_scene_tempo_enabled},
        {"requested_scene_signature_enabled", evidence->requested_scene_signature_enabled},
        {"requested_is_counting_in", evidence->requested_is_counting_in},
        {"requested_arrangement_overdub", evidence->requested_arrangement_overdub},
        {"requested_overdub", evidence->requested_overdub},
        {"requested_record_mode", evidence->requested_record_mode},
        {"requested_session_record", evidence->requested_session_record},
        {"requested_session_automation_record", evidence->requested_session_automation_record},
        {"requested_is_ableton_link_enabled", evidence->requested_is_ableton_link_enabled},
        {"requested_is_ableton_link_start_stop_sync_enabled",
         evidence->requested_is_ableton_link_start_stop_sync_enabled},
        {"requested_tempo_follower_enabled", evidence->requested_tempo_follower_enabled},
        {"requested_nudge_down", evidence->requested_nudge_down},
        {"requested_nudge_up", evidence->requested_nudge_up},
        {"requested_back_to_arranger", evidence->requested_back_to_arranger},
        {"requested_re_enable_automation_enabled",
         evidence->requested_re_enable_automation_enabled},
        {"requested_arrangement_loop", evidence->requested_arrangement_loop},
        {"requested_metronome", evidence->requested_metronome},
        {"observed_tempo",
         evidence->observed_tempo ? json(*evidence->observed_tempo) : json(nullptr)},
        {"observed_signature_numerator",
         evidence->observed_signature_numerator ? json(*evidence->observed_signature_numerator)
                                                : json(nullptr)},
        {"observed_signature_denominator",
         evidence->observed_signature_denominator ? json(*evidence->observed_signature_denominator)
                                                  : json(nullptr)},
        {"observed_scene_name",
         evidence->observed_scene_name ? json(*evidence->observed_scene_name) : json(nullptr)},
        {"observed_scene_triggered_states", evidence->observed_scene_triggered_states},
        {"scene_trigger_states_observed", evidence->scene_trigger_states_observed},
        {"all_scene_launches_quiescent_verified", evidence->all_scene_launches_quiescent_verified},
        {"observed_scene_tempo_enabled",
         evidence->observed_scene_tempo_enabled ? json(*evidence->observed_scene_tempo_enabled)
                                                : json(nullptr)},
        {"observed_scene_signature_enabled",
         evidence->observed_scene_signature_enabled
             ? json(*evidence->observed_scene_signature_enabled)
             : json(nullptr)},
        {"observed_is_playing",
         evidence->observed_is_playing ? json(*evidence->observed_is_playing) : json(nullptr)},
        {"observed_is_counting_in",
         evidence->observed_is_counting_in ? json(*evidence->observed_is_counting_in)
                                           : json(nullptr)},
        {"observed_arrangement_overdub",
         evidence->observed_arrangement_overdub ? json(*evidence->observed_arrangement_overdub)
                                                : json(nullptr)},
        {"observed_overdub",
         evidence->observed_overdub ? json(*evidence->observed_overdub) : json(nullptr)},
        {"observed_record_mode",
         evidence->observed_record_mode ? json(*evidence->observed_record_mode) : json(nullptr)},
        {"observed_session_record",
         evidence->observed_session_record ? json(*evidence->observed_session_record)
                                           : json(nullptr)},
        {"observed_session_automation_record",
         evidence->observed_session_automation_record
             ? json(*evidence->observed_session_automation_record)
             : json(nullptr)},
        {"observed_is_ableton_link_enabled",
         evidence->observed_is_ableton_link_enabled
             ? json(*evidence->observed_is_ableton_link_enabled)
             : json(nullptr)},
        {"observed_is_ableton_link_start_stop_sync_enabled",
         evidence->observed_is_ableton_link_start_stop_sync_enabled
             ? json(*evidence->observed_is_ableton_link_start_stop_sync_enabled)
             : json(nullptr)},
        {"observed_tempo_follower_enabled",
         evidence->observed_tempo_follower_enabled
             ? json(*evidence->observed_tempo_follower_enabled)
             : json(nullptr)},
        {"observed_nudge_down",
         evidence->observed_nudge_down ? json(*evidence->observed_nudge_down) : json(nullptr)},
        {"observed_nudge_up",
         evidence->observed_nudge_up ? json(*evidence->observed_nudge_up) : json(nullptr)},
        {"observed_back_to_arranger",
         evidence->observed_back_to_arranger ? json(*evidence->observed_back_to_arranger)
                                             : json(nullptr)},
        {"observed_re_enable_automation_enabled",
         evidence->observed_re_enable_automation_enabled
             ? json(*evidence->observed_re_enable_automation_enabled)
             : json(nullptr)},
        {"observed_arrangement_loop",
         evidence->observed_arrangement_loop ? json(*evidence->observed_arrangement_loop)
                                             : json(nullptr)},
        {"observed_metronome",
         evidence->observed_metronome ? json(*evidence->observed_metronome) : json(nullptr)},
        {"transport_stopped_verified", evidence->transport_stopped_verified},
        {"recording_modes_quiescent_verified", evidence->recording_modes_quiescent_verified},
        {"public_tempo_controls_quiescent_verified",
         evidence->public_tempo_controls_quiescent_verified},
        {"external_midi_sync_state_observed", evidence->external_midi_sync_state_observed},
        {"tempo_automation_state_observed", evidence->tempo_automation_state_observed},
        {"tempo_stability_verified", evidence->tempo_stability_verified},
        {"arrangement_playback_aligned_verified", evidence->arrangement_playback_aligned_verified},
        {"automation_overrides_quiescent_verified",
         evidence->automation_overrides_quiescent_verified},
        {"arrangement_loop_disabled_verified", evidence->arrangement_loop_disabled_verified},
        {"metronome_disabled_verified", evidence->metronome_disabled_verified},
        {"observed_scale_root_note",
         evidence->observed_scale_root_note ? json(*evidence->observed_scale_root_note)
                                            : json(nullptr)},
        {"observed_scale_name",
         evidence->observed_scale_name ? json(*evidence->observed_scale_name) : json(nullptr)},
        {"observed_scale_intervals", evidence->observed_scale_intervals},
        {"observed_scale_mode",
         evidence->observed_scale_mode ? json(*evidence->observed_scale_mode) : json(nullptr)},
        {"observed_tuning_name",
         evidence->observed_tuning_name ? json(*evidence->observed_tuning_name) : json(nullptr)},
        {"observed_pseudo_octave_in_cents",
         evidence->observed_pseudo_octave_in_cents
             ? json(*evidence->observed_pseudo_octave_in_cents)
             : json(nullptr)},
        {"observed_tuning_lowest_note",
         evidence->observed_tuning_lowest_note ? *evidence->observed_tuning_lowest_note
                                               : json(nullptr)},
        {"observed_tuning_highest_note",
         evidence->observed_tuning_highest_note ? *evidence->observed_tuning_highest_note
                                                : json(nullptr)},
        {"observed_tuning_reference_pitch",
         evidence->observed_tuning_reference_pitch ? *evidence->observed_tuning_reference_pitch
                                                   : json(nullptr)},
        {"observed_tuning_note_tunings",
         evidence->observed_tuning_note_tunings ? *evidence->observed_tuning_note_tunings
                                                : json(nullptr)},
        {"tuning_dictionary_payloads_observed", evidence->tuning_dictionary_payloads_observed},
        {"tuning_definition_fully_observed", evidence->tuning_definition_fully_observed},
        {"per_track_tuning_bypass_observed", evidence->per_track_tuning_bypass_observed},
        {"instrument_tuning_support_observed", evidence->instrument_tuning_support_observed},
        {"audible_pitch_verified", evidence->audible_pitch_verified},
        {"verified", evidence->verified}};
}

json cue_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.cues) {
        encoded.push_back({{"requested_time", evidence.requested_time},
                           {"requested_name", evidence.requested_name},
                           {"observed_time",
                            evidence.observed_time ? json(*evidence.observed_time) : json(nullptr)},
                           {"observed_name",
                            evidence.observed_name ? json(*evidence.observed_name) : json(nullptr)},
                           {"verified", evidence.verified}});
    }
    return encoded;
}

json mixer_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.mixer_properties) {
        encoded.push_back(
            {{"parameter_path", evidence.parameter_path},
             {"value_property", evidence.value_property},
             {"requested_value", evidence.requested_value},
             {"requested_is_quantized", evidence.requested_is_quantized},
             {"observed_value",
              evidence.observed_value ? json(*evidence.observed_value) : json(nullptr)},
             {"observed_minimum",
              evidence.observed_minimum ? json(*evidence.observed_minimum) : json(nullptr)},
             {"observed_maximum",
              evidence.observed_maximum ? json(*evidence.observed_maximum) : json(nullptr)},
             {"observed_is_quantized",
              evidence.observed_is_quantized ? json(*evidence.observed_is_quantized)
                                             : json(nullptr)},
             {"observed_default_value",
              evidence.observed_default_value ? json(*evidence.observed_default_value)
                                              : json(nullptr)},
             {"observed_value_items",
              evidence.observed_value_items ? json(*evidence.observed_value_items) : json(nullptr)},
             {"observed_state",
              evidence.observed_state ? json(*evidence.observed_state) : json(nullptr)},
             {"observed_automation_state",
              evidence.observed_automation_state ? json(*evidence.observed_automation_state)
                                                 : json(nullptr)},
             {"observed_is_enabled",
              evidence.observed_is_enabled ? json(*evidence.observed_is_enabled) : json(nullptr)},
             {"range_verified",
              evidence.range_verified ? json(*evidence.range_verified) : json(nullptr)},
             {"quantization_verified", evidence.quantization_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json parameter_postcondition_evidence_j(
    const formats::AbletonProjectPostconditionEvidence& postconditions) {
    json encoded = json::array();
    for (const auto& evidence : postconditions.device_parameters) {
        encoded.push_back(
            {{"origin", formats::ableton_parameter_origin_name(evidence.origin)},
             {"part_id", evidence.part_id ? json(evidence.part_id->value) : json(nullptr)},
             {"track_index", evidence.track_index ? json(*evidence.track_index) : json(nullptr)},
             {"source_path", evidence.source_path},
             {"effect_id", evidence.effect_id ? json(evidence.effect_id->value) : json(nullptr)},
             {"device_path", evidence.device_path},
             {"requested_name", evidence.requested_name},
             {"value_property",
              evidence.value_property == DeviceParameterValueProperty::DisplayValue
                  ? "display_value"
                  : "value"},
             {"requested_value", evidence.requested_value},
             {"range_min", evidence.range_min},
             {"range_max", evidence.range_max},
             {"deployment_action",
              formats::ableton_parameter_action_name(evidence.deployment_action)},
             {"matched_name", evidence.matched_name ? json(*evidence.matched_name) : json(nullptr)},
             {"original_name",
              evidence.original_name ? json(*evidence.original_name) : json(nullptr)},
             {"observed_value",
              evidence.observed_value ? json(*evidence.observed_value) : json(nullptr)},
             {"observed_minimum",
              evidence.observed_minimum ? json(*evidence.observed_minimum) : json(nullptr)},
             {"observed_maximum",
              evidence.observed_maximum ? json(*evidence.observed_maximum) : json(nullptr)},
             {"observed_is_quantized",
              evidence.observed_is_quantized ? json(*evidence.observed_is_quantized)
                                             : json(nullptr)},
             {"observed_default_value",
              evidence.observed_default_value ? json(*evidence.observed_default_value)
                                              : json(nullptr)},
             {"observed_value_items",
              evidence.observed_value_items ? json(*evidence.observed_value_items) : json(nullptr)},
             {"observed_is_enabled",
              evidence.observed_is_enabled ? json(*evidence.observed_is_enabled) : json(nullptr)},
             {"observed_state",
              evidence.observed_state ? json(*evidence.observed_state) : json(nullptr)},
             {"observed_automation_state",
              evidence.observed_automation_state ? json(*evidence.observed_automation_state)
                                                 : json(nullptr)},
             {"identity_verified", evidence.identity_verified},
             {"value_verified", evidence.value_verified},
             {"range_verified",
              evidence.range_verified ? json(*evidence.range_verified) : json(nullptr)},
             {"enabled_verified", evidence.enabled_verified},
             {"state_verified", evidence.state_verified},
             {"automation_verified", evidence.automation_verified},
             {"verified", evidence.verified}});
    }
    return encoded;
}

json compilation_j(const formats::AbletonProjectCompilationResult& result) {
    json tracks = json::array();
    for (const auto& [part_id, track_index] : result.part_tracks)
        tracks.push_back({{"part_id", part_id.value}, {"track_index", track_index}});

    json timbre = json::array();
    std::vector<std::string> warnings = result.score.warnings;
    for (const auto& part : result.timbre) {
        timbre.push_back(
            {{"part_id", part.part_id.value},
             {"track_index", part.track_index},
             {"devices_requested", part.compilation.devices_requested},
             {"devices_created", part.compilation.devices_created},
             {"devices_verified", part.compilation.devices_verified},
             {"device_deployments",
              mcp_detail::encode_device_deployments(part.compilation.device_deployments)},
             {"effects_requested", part.compilation.effects_requested},
             {"effects_inserted", part.compilation.effects_inserted},
             {"effects_verified", part.compilation.effects_verified},
             {"parameters_mapped", part.compilation.parameters_mapped},
             {"parameters_verified", part.compilation.parameters_verified},
             {"parameter_deployments",
              mcp_detail::encode_timbre_parameter_deployments(
                  part.compilation.parameter_deployments)},
             {"automation_lanes_requested", part.compilation.automation_lanes_requested},
             {"automation_lanes_written", part.compilation.automation_lanes_written},
             {"warnings", part.compilation.warnings}});
        warnings.insert(
            warnings.end(), part.compilation.warnings.begin(), part.compilation.warnings.end());
    }
    warnings.insert(warnings.end(), result.mix.warnings.begin(), result.mix.warnings.end());
    warnings.insert(warnings.end(),
                    result.postconditions.warnings.begin(),
                    result.postconditions.warnings.end());

    return {
        {"success", true},
        {"connected", true},
        {"complete",
         warnings.empty() && !result.score.midi_report.has_residuals() &&
             result.score.tuning_definitions_written == result.score.tuning_definitions_requested},
        {"target_profile", target_profile_to_json(result.score.target_profile)},
        {"part_tracks", std::move(tracks)},
        {"score",
         {{"scenes_created", result.score.scenes_created},
          {"tracks_created", result.score.tracks_created},
          {"clips_created", result.score.clips_created},
          {"clip_envelope_clears_requested", result.score.clip_envelope_clears_requested},
          {"clip_envelope_clears_executed", result.score.clip_envelope_clears_executed},
          {"clip_envelope_clears_verified", result.score.clip_envelope_clears_verified},
          {"clip_envelope_deployments",
           mcp_detail::encode_clip_envelope_deployments(result.score.clip_envelope_deployments)},
          {"notes_requested", result.score.notes_requested},
          {"notes_written", result.score.notes_written},
          {"note_batches_requested", result.score.note_batches_requested},
          {"note_batches_executed", result.score.note_batches_executed},
          {"note_ids_returned", result.score.note_ids_returned},
          {"note_batches_verified", result.score.note_batches_verified},
          {"notes_verified", result.score.notes_verified},
          {"note_deployments", mcp_detail::encode_note_deployments(result.score.note_deployments)},
          {"articulation_control_events_requested",
           result.score.articulation_control_events_requested},
          {"articulation_control_events_written", result.score.articulation_control_events_written},
          {"tempo_events_requested", result.score.tempo_events_requested},
          {"tempo_events_written", result.score.tempo_events_written},
          {"time_signature_events_requested", result.score.time_signature_events_requested},
          {"time_signature_events_written", result.score.time_signature_events_written},
          {"time_signature_groupings_requested", result.score.time_signature_groupings_requested},
          {"time_signature_groupings_written", result.score.time_signature_groupings_written},
          {"key_signature_events_requested", result.score.key_signature_events_requested},
          {"key_signature_events_written", result.score.key_signature_events_written},
          {"tuning_definitions_requested", result.score.tuning_definitions_requested},
          {"tuning_definitions_written", result.score.tuning_definitions_written},
          {"requested_tuning", mcp_detail::encode_score_tuning(result.score.requested_tuning)},
          {"section_nodes_total", result.score.section_nodes_total},
          {"section_nodes_projected", result.score.section_nodes_projected},
          {"section_nodes_unprojected", result.score.section_nodes_unprojected},
          {"markers_requested", result.score.markers_requested},
          {"markers_created", result.score.markers_created},
          {"markers_updated", result.score.markers_updated},
          {"markers_verified", result.score.markers_verified},
          {"marker_deployments",
           mcp_detail::encode_cue_deployments(result.score.marker_deployments)},
          {"property_writes", result.score.property_writes},
          {"property_writes_verified", result.score.property_writes_verified},
          {"property_deployments",
           mcp_detail::encode_property_deployments(result.score.property_deployments)},
          {"report", mcp_detail::encode_compilation_report(result.score.midi_report)},
          {"warnings", result.score.warnings}}},
        {"timbre", std::move(timbre)},
        {"mix",
         {{"group_tracks_requested", result.mix.group_tracks_requested},
          {"group_tracks_created", result.mix.group_tracks_created},
          {"return_tracks_requested", result.mix.return_tracks_requested},
          {"return_tracks_created", result.mix.return_tracks_created},
          {"return_track_deployments",
           mcp_detail::encode_return_track_deployments(result.mix.return_track_deployments)},
          {"master_track_deployment",
           mcp_detail::encode_master_track_deployment(result.mix.master_track_deployment)},
          {"effects_requested", result.mix.effects_requested},
          {"effects_inserted", result.mix.effects_inserted},
          {"effects_verified", result.mix.effects_verified},
          {"device_deployments",
           mcp_detail::encode_device_deployments(result.mix.device_deployments)},
          {"sends_requested", result.mix.sends_requested},
          {"sends_configured", result.mix.sends_configured},
          {"send_levels_requested", result.mix.send_levels_requested},
          {"send_levels_configured", result.mix.send_levels_configured},
          {"send_modes_requested", result.mix.send_modes_requested},
          {"send_modes_configured", result.mix.send_modes_configured},
          {"output_routes_requested", result.mix.output_routes_requested},
          {"output_routes_written", result.mix.output_routes_written},
          {"output_routes_verified", result.mix.output_routes_verified},
          {"output_routing_bindings",
           mcp_detail::encode_output_routing_bindings(result.output_routing_bindings)},
          {"output_route_deployments",
           mcp_detail::encode_output_route_deployments(result.mix.output_route_deployments)},
          {"output_route_residuals", result.mix.output_route_residuals},
          {"automation_lanes_requested", result.mix.automation_lanes_requested},
          {"automation_lanes_written", result.mix.automation_lanes_written},
          {"channels_requested", result.mix.channels_requested},
          {"channels_configured", result.mix.channels_configured},
          {"property_writes", result.mix.property_writes},
          {"property_writes_verified", result.mix.property_writes_verified},
          {"property_deployments",
           mcp_detail::encode_property_deployments(result.mix.property_deployments)},
          {"parameter_sources_total", result.mix.parameter_sources_total},
          {"parameter_sources_explicitly_mapped", result.mix.parameter_sources_explicitly_mapped},
          {"parameter_sources_unmapped", result.mix.parameter_sources_unmapped},
          {"parameters_mapped", result.mix.parameters_mapped},
          {"parameters_verified", result.mix.parameters_verified},
          {"fader_level_resolution",
           mix_fader_level_resolution_j(result.mix.fader_level_resolution)},
          {"parameter_deployments",
           mcp_detail::encode_mix_parameter_deployments(result.mix.parameter_deployments)},
          {"parameter_coverage",
           mcp_detail::encode_mix_parameter_coverage(result.mix.parameter_coverage)},
          {"warnings", result.mix.warnings}}},
        {"postconditions",
         {{"observed", result.postconditions.observed},
          {"song_states_requested", result.postconditions.song_states_requested},
          {"song_states_verified", result.postconditions.song_states_verified},
          {"song_state", song_postcondition_evidence_j(result.postconditions.song_state)},
          {"track_gates_requested", result.postconditions.track_gates_requested},
          {"track_gates_verified", result.postconditions.track_gates_verified},
          {"track_gates", track_gate_evidence_j(result.postconditions)},
          {"return_track_gates_requested", result.postconditions.return_track_gates_requested},
          {"return_track_gates_verified", result.postconditions.return_track_gates_verified},
          {"return_track_gates", return_track_gate_evidence_j(result.postconditions)},
          {"master_track_gates_requested", result.postconditions.master_track_gates_requested},
          {"master_track_gates_verified", result.postconditions.master_track_gates_verified},
          {"master_track_gate",
           master_track_gate_evidence_j(result.postconditions.master_track_gate)},
          {"devices_requested", result.postconditions.devices_requested},
          {"devices_verified", result.postconditions.devices_verified},
          {"devices", device_postcondition_evidence_j(result.postconditions)},
          {"clips_requested", result.postconditions.clips_requested},
          {"clips_verified", result.postconditions.clips_verified},
          {"clips", clip_postcondition_evidence_j(result.postconditions)},
          {"note_batches_requested", result.postconditions.note_batches_requested},
          {"note_batches_verified", result.postconditions.note_batches_verified},
          {"notes_requested", result.postconditions.notes_requested},
          {"notes_verified", result.postconditions.notes_verified},
          {"note_batches", note_postcondition_evidence_j(result.postconditions)},
          {"cues_requested", result.postconditions.cues_requested},
          {"cues_verified", result.postconditions.cues_verified},
          {"cues", cue_postcondition_evidence_j(result.postconditions)},
          {"mixer_properties_requested", result.postconditions.mixer_properties_requested},
          {"mixer_properties_verified", result.postconditions.mixer_properties_verified},
          {"mixer_properties", mixer_postcondition_evidence_j(result.postconditions)},
          {"device_parameters_requested", result.postconditions.device_parameters_requested},
          {"device_parameters_verified", result.postconditions.device_parameters_verified},
          {"device_parameters", parameter_postcondition_evidence_j(result.postconditions)},
          {"warnings", result.postconditions.warnings}}},
        {"diagnostics", mcp_detail::encode_diagnostics(result.diagnostics)},
        {"warnings", std::move(warnings)}};
}

const char* deployment_phase_name(AbletonDeploymentPhase phase) {
    if (phase == AbletonDeploymentPhase::Score) return "score";
    if (phase == AbletonDeploymentPhase::Timbre) return "timbre";
    return "mix";
}

const char* mutation_outcome_name(AbletonMutationOutcome outcome) {
    if (outcome == AbletonMutationOutcome::RecordedOnly) return "recorded_only";
    if (outcome == AbletonMutationOutcome::Acknowledged) return "acknowledged";
    if (outcome == AbletonMutationOutcome::DeclinedBeforeSend) return "declined_before_send";
    return "indeterminate";
}

const char* deployment_status_name(formats::AbletonProjectDeploymentStatus status) {
    using Status = formats::AbletonProjectDeploymentStatus;
    if (status == Status::Completed) return "completed";
    if (status == Status::ProjectChanged) return "project_changed";
    if (status == Status::TargetChanged) return "target_changed";
    if (status == Status::PlanDiverged) return "plan_diverged";
    if (status == Status::PlanConsumed) return "plan_consumed";
    if (status == Status::PostSnapshotUnavailable) return "post_snapshot_unavailable";
    return "apply_failed";
}

json request_j(const LomRequest& request) {
    return json::parse(LomProtocol::serialize_request(request));
}

json lom_value_j(const std::optional<LomValue>& value) {
    if (!value) return nullptr;
    return std::visit([](const auto& item) { return json(item); }, *value);
}

json planned_mutations_j(const std::vector<AbletonPlannedMutation>& mutations) {
    json encoded = json::array();
    for (std::size_t index = 0; index < mutations.size(); ++index) {
        encoded.push_back({{"sequence", index},
                           {"phase", deployment_phase_name(mutations[index].phase)},
                           {"request", request_j(mutations[index].request)}});
    }
    return encoded;
}

json mutation_journal_j(const std::vector<AbletonMutationJournalEntry>& journal) {
    json encoded = json::array();
    for (const auto& entry : journal) {
        encoded.push_back(
            {{"sequence", entry.sequence},
             {"phase", deployment_phase_name(entry.phase)},
             {"request", request_j(entry.request)},
             {"outcome", mutation_outcome_name(entry.outcome)},
             {"response_value", lom_value_j(entry.response_value)},
             {"response_error", entry.response_error ? json(*entry.response_error) : json(nullptr)},
             {"target_may_have_mutated", entry.target_may_have_mutated()}});
    }
    return encoded;
}

json part_tracks_j(const formats::AbletonPartTrackMap& part_tracks) {
    json encoded = json::array();
    for (const auto& [part_id, track_index] : part_tracks)
        encoded.push_back({{"part_id", part_id.value}, {"track_index", track_index}});
    return encoded;
}

json deployment_attempt_j(const formats::AbletonProjectDeploymentAttempt& attempt) {
    return {
        {"status", deployment_status_name(attempt.status)},
        {"error_code", attempt.error ? json(static_cast<int>(*attempt.error)) : json(nullptr)},
        {"target_may_be_partially_modified", attempt.target_may_be_partially_modified()},
        {"target_before",
         attempt.target_before ? target_snapshot_to_json(*attempt.target_before) : json(nullptr)},
        {"target_after",
         attempt.target_after ? target_snapshot_to_json(*attempt.target_after) : json(nullptr)},
        {"mutation_journal", mutation_journal_j(attempt.mutation_journal)}};
}

json plan_j(std::uint64_t plan_id, const formats::AbletonProjectDeploymentPlan& plan) {
    auto preview = compilation_j(plan.preview);
    preview["mode"] = "dry_run";
    preview["executed"] = false;
    return {{"success", true},
            {"connected", true},
            {"plan_id", plan_id},
            {"one_shot", true},
            {"ppq", plan.ppq},
            {"target_snapshot", target_snapshot_to_json(plan.target_snapshot)},
            {"part_tracks", part_tracks_j(plan.part_tracks)},
            {"output_routing_bindings",
             mcp_detail::encode_output_routing_bindings(plan.output_routing_bindings)},
            {"planned_mutations", planned_mutations_j(plan.mutations)},
            {"preview", std::move(preview)}};
}

std::optional<ResolvedProject> resolve_stored_project(const StoredProjectDeploymentPlan& stored,
                                                      const std::shared_ptr<ScoreSession>& scores,
                                                      const std::shared_ptr<TimbreSession>& timbres,
                                                      const std::shared_ptr<MixSession>& mixes,
                                                      std::string& error) {
    ResolvedProject result;
    result.score_id = stored.score_id;
    result.profile_ids = stored.timbre_profile_ids;
    result.mix_graph_id = stored.mix_graph_id;
    result.score = scores->find(stored.score_id);
    result.mix = mixes->find(stored.mix_graph_id);
    if (result.score == nullptr || result.mix == nullptr) {
        error = "A project document referenced by the plan no longer exists";
        return std::nullopt;
    }
    result.profiles.reserve(stored.timbre_profile_ids.size());
    for (const auto id : stored.timbre_profile_ids) {
        const auto* profile = timbres->find(id);
        if (profile == nullptr) {
            error = "A Timbre profile referenced by the plan no longer exists";
            return std::nullopt;
        }
        result.profiles.push_back(profile);
    }
    return result;
}

} // namespace

void register_project_tools(McpServer& server, const McpSession& session, LomTransport* transport) {
    const auto schema =
        json{{"type", "object"},
             {"properties",
              {{"score_id", {{"type", "integer"}}},
               {"timbre_profile_ids", {{"type", "array"}, {"items", {{"type", "integer"}}}}},
               {"mix_graph_id", {{"type", "integer"}}},
               {"ppq", {{"type", "integer"}}},
               {"output_routing_bindings", mcp_detail::output_routing_bindings_schema()}}},
             {"required", json::array({"score_id", "timbre_profile_ids", "mix_graph_id"})}};

    server.register_tool(
        "project_validate",
        "Validate Score, Timbre, and Mix documents plus exact PartId correspondence",
        schema,
        [scores = session.score, timbres = session.timbre, mixes = session.mix](
            const json& params) {
            std::string error;
            auto project = resolve_project(params, scores, timbres, mixes, error);
            if (!project) return error_response(error);
            const auto diagnostics = validate_project(project->view());
            const bool valid =
                std::none_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
                    return diagnostic.severity == ValidationSeverity::Error;
                });
            return json{{"valid", valid},
                        {"diagnostics", mcp_detail::encode_diagnostics(diagnostics)}};
        });

    server.register_tool(
        "project_plan_to_ableton",
        "Validate, snapshot Ableton, and dry-run an exact one-shot project deployment plan",
        schema,
        [scores = session.score,
         timbres = session.timbre,
         mixes = session.mix,
         plans = session.deployment,
         transport](const json& params) {
            std::string error;
            auto project = resolve_project(params, scores, timbres, mixes, error);
            if (!project) return error_response(error);
            if (transport == nullptr || !transport->ensure_connected())
                return json{{"success", false},
                            {"connected", false},
                            {"error", "Ableton transport unavailable"}};

            const auto ppq = detail::checked_integer_or<int>(params, "ppq", 480, "MIDI PPQ");
            if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max())
                return error_response("ppq must be between 1 and 65535");
            formats::AbletonOutputRoutingBindings routing_bindings;
            std::string routing_error;
            if (!mcp_detail::parse_output_routing_bindings(params, routing_bindings, routing_error))
                return error_response(routing_error);
            auto plan = formats::plan_project_to_ableton(
                project->view(), routing_bindings, *transport, ppq);
            if (!plan)
                return json{{"success", false},
                            {"connected", transport->is_connected()},
                            {"error_code", static_cast<int>(plan.error())},
                            {"error", "Ableton project planning failed"}};
            if (plans->next_plan_id == std::numeric_limits<std::uint64_t>::max())
                return error_response("Project deployment plan ID space exhausted");
            constexpr std::size_t max_retained_plans = 256;
            for (auto item = plans->plans.begin();
                 plans->plans.size() >= max_retained_plans && item != plans->plans.end();) {
                if (item->second.consumed)
                    item = plans->plans.erase(item);
                else
                    ++item;
            }
            if (plans->plans.size() >= max_retained_plans)
                return error_response("Too many unconsumed project deployment plans");

            const auto plan_id = plans->next_plan_id;
            StoredProjectDeploymentPlan stored;
            stored.score_id = project->score_id;
            stored.timbre_profile_ids = project->profile_ids;
            stored.mix_graph_id = project->mix_graph_id;
            stored.plan = std::move(*plan);
            const auto [position, inserted] = plans->plans.emplace(plan_id, std::move(stored));
            if (!inserted) return error_response("Project deployment plan ID collision");
            ++plans->next_plan_id;
            return plan_j(plan_id, *position->second.plan);
        });

    const auto apply_schema = json{{"type", "object"},
                                   {"properties",
                                    {{"plan_id", {{"type", "integer"}, {"minimum", 1}}},
                                     {"validation_context", validation_context_schema()}}},
                                   {"required", json::array({"plan_id"})}};
    server.register_tool(
        "project_apply_ableton_plan",
        "Apply an unconsumed project plan only if project and Ableton snapshots still match",
        apply_schema,
        [scores = session.score,
         timbres = session.timbre,
         mixes = session.mix,
         plans = session.deployment,
         transport](const json& params) {
            std::optional<ValidationContext> validation_context;
            std::string validation_error;
            if (!parse_validation_context(params, validation_context, validation_error))
                return error_response(validation_error);
            const auto plan_id =
                detail::checked_integer<std::uint64_t>(params.at("plan_id"), "deployment plan id");
            const auto found = plans->plans.find(plan_id);
            if (found == plans->plans.end()) return error_response("Deployment plan not found");
            if (found->second.consumed || !found->second.plan)
                return json{{"success", false},
                            {"connected", transport != nullptr && transport->is_connected()},
                            {"plan_id", plan_id},
                            {"plan_consumed", true},
                            {"error_code", static_cast<int>(ErrorCode::ProjectPlanConsumed)},
                            {"error", "Deployment plan is one-shot and has already been consumed"}};
            if (transport == nullptr || !transport->ensure_connected())
                return json{{"success", false},
                            {"connected", false},
                            {"plan_id", plan_id},
                            {"plan_consumed", false},
                            {"error", "Ableton transport unavailable"}};

            std::string error;
            auto project = resolve_stored_project(found->second, scores, timbres, mixes, error);
            found->second.consumed = true;
            auto plan = std::move(*found->second.plan);
            found->second.plan.reset();
            if (!project)
                return json{{"success", false},
                            {"connected", transport->is_connected()},
                            {"plan_id", plan_id},
                            {"plan_consumed", true},
                            {"error_code", static_cast<int>(ErrorCode::ProjectPlanProjectChanged)},
                            {"error", error}};

            auto attempt = formats::apply_project_ableton_plan(plan, project->view(), *transport);
            std::optional<json> encoded_compilation;
            if (attempt.compilation) encoded_compilation = compilation_j(*attempt.compilation);
            json output = encoded_compilation.value_or(json::object());
            output["success"] =
                attempt.status == formats::AbletonProjectDeploymentStatus::Completed;
            output["connected"] = transport->is_connected();
            output["plan_id"] = plan_id;
            output["plan_consumed"] = true;
            output["deployment"] = deployment_attempt_j(attempt);
            attach_validation_record(
                output, plan, attempt, std::move(validation_context), encoded_compilation);
            if (!output["success"].get<bool>()) output["error"] = "Ableton plan apply failed";
            return output;
        });

    auto compile_schema = schema;
    compile_schema["properties"]["validation_context"] = validation_context_schema();
    server.register_tool(
        "project_compile_to_ableton",
        "Plan and immediately apply a guarded correspondent project deployment",
        compile_schema,
        [scores = session.score, timbres = session.timbre, mixes = session.mix, transport](
            const json& params) {
            std::optional<ValidationContext> validation_context;
            std::string validation_error;
            if (!parse_validation_context(params, validation_context, validation_error))
                return error_response(validation_error);
            std::string error;
            auto project = resolve_project(params, scores, timbres, mixes, error);
            if (!project) return error_response(error);

            const auto diagnostics = validate_project(project->view());
            if (std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
                    return diagnostic.severity == ValidationSeverity::Error;
                })) {
                return json{{"success", false},
                            {"connected", transport != nullptr && transport->is_connected()},
                            {"error", "Project is not valid for compilation"},
                            {"error_code", static_cast<int>(ErrorCode::ProjectValidationFailed)},
                            {"diagnostics", mcp_detail::encode_diagnostics(diagnostics)}};
            }
            if (transport == nullptr || !transport->ensure_connected()) {
                return json{{"success", false},
                            {"connected", false},
                            {"error", "Ableton transport unavailable"},
                            {"diagnostics", mcp_detail::encode_diagnostics(diagnostics)}};
            }

            const auto ppq = detail::checked_integer_or<int>(params, "ppq", 480, "MIDI PPQ");
            if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max())
                return error_response("ppq must be between 1 and 65535");

            formats::AbletonOutputRoutingBindings routing_bindings;
            std::string routing_error;
            if (!mcp_detail::parse_output_routing_bindings(params, routing_bindings, routing_error))
                return error_response(routing_error);
            auto plan = formats::plan_project_to_ableton(
                project->view(), routing_bindings, *transport, ppq);
            if (!plan) {
                return json{{"success", false},
                            {"connected", transport->is_connected()},
                            {"error_code", static_cast<int>(plan.error())},
                            {"error", "Ableton project planning failed"}};
            }
            auto attempt = formats::apply_project_ableton_plan(*plan, project->view(), *transport);
            std::optional<json> encoded_compilation;
            if (attempt.compilation) encoded_compilation = compilation_j(*attempt.compilation);
            json output = encoded_compilation.value_or(json::object());
            output["success"] =
                attempt.status == formats::AbletonProjectDeploymentStatus::Completed;
            output["connected"] = transport->is_connected();
            output["plan"] = {{"target_snapshot", target_snapshot_to_json(plan->target_snapshot)},
                              {"planned_mutations", planned_mutations_j(plan->mutations)}};
            output["deployment"] = deployment_attempt_j(attempt);
            attach_validation_record(
                output, *plan, attempt, std::move(validation_context), encoded_compilation);
            if (!output["success"].get<bool>())
                output["error"] = "Ableton project compilation failed";
            return output;
        });
}

} // namespace sunny::infrastructure
