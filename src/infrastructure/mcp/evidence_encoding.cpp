/**
 * @file evidence_encoding.cpp
 * @brief Private MCP JSON authority for validation and Ableton deployment evidence
 */

#include "evidence_encoding.hpp"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <string_view>
#include <utility>

namespace sunny::infrastructure::mcp_detail {

namespace {

nlohmann::json
routing_option_j(const sunny::infrastructure::formats::AbletonRoutingOptionBinding& option) {
    return {{"display_name", option.display_name}, {"identifier", option.identifier}};
}

nlohmann::json
route_binding_j(const sunny::infrastructure::formats::AbletonOutputRouteBinding& binding) {
    return {{"type", routing_option_j(binding.type)},
            {"channel", routing_option_j(binding.channel)},
            {"mapping_provenance", binding.mapping_provenance}};
}

bool exact_fields(const nlohmann::json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::ranges::all_of(
        fields, [&](std::string_view field) { return value.contains(std::string{field}); });
}

std::optional<std::uint64_t> routing_id(const nlohmann::json& value) {
    if (!value.is_number_integer()) return std::nullopt;
    if (value.is_number_unsigned()) return value.get<std::uint64_t>();
    const auto signed_value = value.get<std::int64_t>();
    return signed_value < 0
               ? std::nullopt
               : std::optional<std::uint64_t>{static_cast<std::uint64_t>(signed_value)};
}

std::optional<sunny::infrastructure::formats::AbletonRoutingOptionBinding>
parse_routing_option(const nlohmann::json& value) {
    if (!exact_fields(value, {"display_name", "identifier"}) ||
        !value.at("display_name").is_string() || !value.at("identifier").is_string())
        return std::nullopt;
    return sunny::infrastructure::formats::AbletonRoutingOptionBinding{
        value.at("display_name").get<std::string>(), value.at("identifier").get<std::string>()};
}

std::optional<sunny::infrastructure::formats::AbletonOutputRouteBinding>
parse_route_binding(const nlohmann::json& value, std::string_view id_field, std::uint64_t& id) {
    if (!exact_fields(value, {id_field, "type", "channel", "mapping_provenance"}))
        return std::nullopt;
    auto parsed_id = routing_id(value.at(std::string{id_field}));
    auto type = parse_routing_option(value.at("type"));
    auto channel = parse_routing_option(value.at("channel"));
    if (!parsed_id || !type || !channel || !value.at("mapping_provenance").is_string() ||
        value.at("mapping_provenance").get_ref<const std::string&>().empty())
        return std::nullopt;
    id = *parsed_id;
    return sunny::infrastructure::formats::AbletonOutputRouteBinding{
        std::move(*type), std::move(*channel), value.at("mapping_provenance").get<std::string>()};
}

nlohmann::json optional_routing_option_j(
    const std::optional<sunny::infrastructure::formats::AbletonRoutingOptionBinding>& option) {
    return option ? routing_option_j(*option) : nlohmann::json(nullptr);
}

nlohmann::json routing_options_j(
    const std::vector<sunny::infrastructure::formats::AbletonRoutingOptionBinding>& options) {
    auto encoded = nlohmann::json::array();
    for (const auto& option : options)
        encoded.push_back(routing_option_j(option));
    return encoded;
}

} // namespace

nlohmann::json encode_diagnostic(const sunny::core::Diagnostic& diagnostic) {
    using sunny::core::ValidationSeverity;

    nlohmann::json encoded = {
        {"rule", diagnostic.rule},
        {"severity",
         diagnostic.severity == ValidationSeverity::Error     ? "error"
         : diagnostic.severity == ValidationSeverity::Warning ? "warning"
                                                              : "info"},
        {"message", diagnostic.message},
        {"error_code", static_cast<int>(diagnostic.error_code)},
    };
    if (diagnostic.location) {
        encoded["location"] = {
            {"bar", diagnostic.location->bar},
            {"beat_n", diagnostic.location->beat.numerator()},
            {"beat_d", diagnostic.location->beat.denominator()},
        };
    }
    if (diagnostic.part) encoded["part_id"] = diagnostic.part->value;
    return encoded;
}

nlohmann::json encode_diagnostics(const std::vector<sunny::core::Diagnostic>& diagnostics) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& diagnostic : diagnostics)
        encoded.push_back(encode_diagnostic(diagnostic));
    return encoded;
}

nlohmann::json encode_compilation_report(const sunny::core::CompilationReport& report) {
    nlohmann::json diagnostics = nlohmann::json::array();
    for (const auto& diagnostic : report.diagnostics) {
        nlohmann::json encoded = {{"message", diagnostic.message}};
        if (diagnostic.location) {
            encoded["location"] = {
                {"bar", diagnostic.location->bar},
                {"beat_n", diagnostic.location->beat.numerator()},
                {"beat_d", diagnostic.location->beat.denominator()},
            };
        }
        if (diagnostic.part) encoded["part_id"] = diagnostic.part->value;
        diagnostics.push_back(std::move(encoded));
    }
    return {{"dropped_notes", report.dropped_notes},
            {"dropped_tempo_events", report.dropped_tempo_events},
            {"dropped_time_sig_events", report.dropped_time_sig_events},
            {"dropped_key_sig_events", report.dropped_key_sig_events},
            {"time_signature_events_requested", report.time_signature_events_requested},
            {"time_signature_events_written", report.time_signature_events_written},
            {"time_signature_groupings_requested", report.time_signature_groupings_requested},
            {"time_signature_groupings_written", report.time_signature_groupings_written},
            {"tuning_definitions_requested", report.tuning_definitions_requested},
            {"tuning_definitions_written", report.tuning_definitions_written},
            {"articulation_mappings_applied", report.articulation_mappings_applied},
            {"articulations_defaulted", report.articulations_defaulted},
            {"has_drops", report.has_drops()},
            {"has_residuals", report.has_residuals()},
            {"diagnostics", std::move(diagnostics)}};
}

nlohmann::json encode_score_tuning(const sunny::core::ScoreTuning& tuning) {
    return {{"name", tuning.name},
            {"reference_midi_note", tuning.reference_midi_note},
            {"reference_frequency_hz", tuning.reference_frequency_hz},
            {"cents_from_reference", tuning.cents_from_reference}};
}

nlohmann::json encode_property_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonPropertyDeployment>& deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back(
            {{"path", deployment.path},
             {"property", deployment.property},
             {"requested", deployment.requested},
             {"observed", deployment.observed ? *deployment.observed : nlohmann::json(nullptr)},
             {"verified", deployment.verified}});
    }
    return encoded;
}

nlohmann::json encode_cue_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonCueDeployment>& deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back(
            {{"requested_time", deployment.requested_time},
             {"requested_name", deployment.requested_name},
             {"observed_time",
              deployment.observed_time ? nlohmann::json(*deployment.observed_time)
                                       : nlohmann::json(nullptr)},
             {"observed_name",
              deployment.observed_name ? nlohmann::json(*deployment.observed_name)
                                       : nlohmann::json(nullptr)},
             {"action", sunny::infrastructure::formats::cue_action_name(deployment.action)},
             {"verified", deployment.verified}});
    }
    return encoded;
}

nlohmann::json encode_clip_envelope_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonClipEnvelopeDeployment>& deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back(
            {{"part_id", deployment.part_id.value},
             {"track_index", deployment.track_index},
             {"requested_has_envelopes", deployment.requested_has_envelopes},
             {"observed_has_envelopes",
              deployment.observed_has_envelopes ? nlohmann::json(*deployment.observed_has_envelopes)
                                                : nlohmann::json(nullptr)},
             {"action",
              sunny::infrastructure::formats::clip_envelope_action_name(deployment.action)},
             {"verified", deployment.verified}});
    }
    return encoded;
}

nlohmann::json encode_note_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonNoteDeployment>& deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        nlohmann::json requested = nlohmann::json::array();
        for (const auto& note : deployment.requested_notes) {
            requested.push_back({{"pitch", note.pitch},
                                 {"start_time", note.start_time},
                                 {"duration", note.duration},
                                 {"velocity", note.velocity},
                                 {"mute", note.muted},
                                 {"probability", note.probability},
                                 {"velocity_deviation", note.velocity_deviation},
                                 {"release_velocity", note.release_velocity}});
        }
        nlohmann::json observed = nlohmann::json::array();
        for (const auto& note : deployment.observed_notes) {
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
            {{"part_id", deployment.part_id.value},
             {"track_index", deployment.track_index},
             {"notes_requested", deployment.notes_requested},
             {"requested_notes", std::move(requested)},
             {"created_note_ids", deployment.created_note_ids},
             {"observed_notes", std::move(observed)},
             {"action", sunny::infrastructure::formats::note_action_name(deployment.action)},
             {"cardinality_verified", deployment.cardinality_verified},
             {"properties_verified", deployment.properties_verified}});
    }
    return encoded;
}

nlohmann::json encode_timbre_parameter_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonParameterDeployment>& deployments) {
    using sunny::core::DeviceParameterValueProperty;

    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        nlohmann::json item = {
            {"ir_path", deployment.ir_path},
            {"device_index", deployment.device_index},
            {"parameter_name", deployment.parameter_name},
            {"value_property",
             deployment.value_property == DeviceParameterValueProperty::DisplayValue
                 ? "display_value"
                 : "value"},
            {"source_value", deployment.source_value},
            {"requested_value", deployment.requested_value},
            {"range_min", deployment.range_min},
            {"range_max", deployment.range_max},
            {"action",
             sunny::infrastructure::formats::ableton_parameter_action_name(deployment.action)},
            {"verified", deployment.verified},
        };
        item["matched_name"] = deployment.matched_name ? nlohmann::json(*deployment.matched_name)
                                                       : nlohmann::json(nullptr);
        item["original_name"] = deployment.original_name ? nlohmann::json(*deployment.original_name)
                                                         : nlohmann::json(nullptr);
        item["observed_value"] = deployment.observed_value
                                     ? nlohmann::json(*deployment.observed_value)
                                     : nlohmann::json(nullptr);
        item["observed_minimum"] = deployment.observed_minimum
                                       ? nlohmann::json(*deployment.observed_minimum)
                                       : nlohmann::json(nullptr);
        item["observed_maximum"] = deployment.observed_maximum
                                       ? nlohmann::json(*deployment.observed_maximum)
                                       : nlohmann::json(nullptr);
        item["is_quantized"] = deployment.is_quantized ? nlohmann::json(*deployment.is_quantized)
                                                       : nlohmann::json(nullptr);
        item["default_value"] = deployment.default_value ? nlohmann::json(*deployment.default_value)
                                                         : nlohmann::json(nullptr);
        item["value_items"] = deployment.value_items ? nlohmann::json(*deployment.value_items)
                                                     : nlohmann::json(nullptr);
        item["is_enabled"] = deployment.is_enabled ? nlohmann::json(*deployment.is_enabled)
                                                   : nlohmann::json(nullptr);
        item["parameter_state"] = deployment.parameter_state
                                      ? nlohmann::json(*deployment.parameter_state)
                                      : nlohmann::json(nullptr);
        item["automation_state"] = deployment.automation_state
                                       ? nlohmann::json(*deployment.automation_state)
                                       : nlohmann::json(nullptr);
        encoded.push_back(std::move(item));
    }
    return encoded;
}

nlohmann::json encode_device_deployments(
    const std::vector<sunny::infrastructure::formats::AbletonDeviceInsertionDeployment>&
        deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        nlohmann::json item = {
            {"device_path", deployment.device_path},
            {"requested_name", deployment.requested_name},
            {"requested_index", deployment.requested_index},
            {"requested_type", deployment.requested_type},
            {"identity_verified", deployment.identity_verified},
            {"type_verified", deployment.type_verified},
            {"flat_device_verified", deployment.flat_device_verified},
            {"output_verified", deployment.output_verified},
            {"verified", deployment.verified},
        };
        item["before_count"] = deployment.before_count ? nlohmann::json(*deployment.before_count)
                                                       : nlohmann::json(nullptr);
        item["after_count"] = deployment.after_count ? nlohmann::json(*deployment.after_count)
                                                     : nlohmann::json(nullptr);
        item["observed_name"] = deployment.observed_name ? nlohmann::json(*deployment.observed_name)
                                                         : nlohmann::json(nullptr);
        item["observed_class_display_name"] =
            deployment.observed_class_display_name
                ? nlohmann::json(*deployment.observed_class_display_name)
                : nlohmann::json(nullptr);
        item["observed_class_name"] = deployment.observed_class_name
                                          ? nlohmann::json(*deployment.observed_class_name)
                                          : nlohmann::json(nullptr);
        item["observed_type"] = deployment.observed_type ? nlohmann::json(*deployment.observed_type)
                                                         : nlohmann::json(nullptr);
        item["observed_active"] = deployment.observed_active
                                      ? nlohmann::json(*deployment.observed_active)
                                      : nlohmann::json(nullptr);
        item["observed_can_have_chains"] =
            deployment.observed_can_have_chains
                ? nlohmann::json(*deployment.observed_can_have_chains)
                : nlohmann::json(nullptr);
        item["observed_latency_in_samples"] =
            deployment.observed_latency_in_samples
                ? nlohmann::json(*deployment.observed_latency_in_samples)
                : nlohmann::json(nullptr);
        item["observed_latency_in_ms"] = deployment.observed_latency_in_ms
                                             ? nlohmann::json(*deployment.observed_latency_in_ms)
                                             : nlohmann::json(nullptr);
        item["reported_latency_observed"] = deployment.reported_latency_observed;
        item["render_path_latency_fully_observed"] = deployment.render_path_latency_fully_observed;
        item["observed_track_has_audio_output"] =
            deployment.observed_track_has_audio_output
                ? nlohmann::json(*deployment.observed_track_has_audio_output)
                : nlohmann::json(nullptr);
        item["observed_track_has_midi_output"] =
            deployment.observed_track_has_midi_output
                ? nlohmann::json(*deployment.observed_track_has_midi_output)
                : nlohmann::json(nullptr);
        encoded.push_back(std::move(item));
    }
    return encoded;
}

nlohmann::json encode_return_track_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ReturnTrackDeployment>&
        deployments) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back({{"aux_bus_id", deployment.aux_bus_id.value},
                           {"track_index", deployment.track_index},
                           {"requested_name", deployment.requested_name},
                           {"requested_mute", deployment.requested_mute},
                           {"requested_solo", deployment.requested_solo},
                           {"requested_track_activator", deployment.requested_track_activator},
                           {"requested_crossfade_assign", deployment.requested_crossfade_assign},
                           {"requested_panning_mode", deployment.requested_panning_mode},
                           {"requested_pan", deployment.requested_pan}});
    }
    return encoded;
}

nlohmann::json encode_master_track_deployment(
    const std::optional<
        sunny::infrastructure::formats::MixCompilationResult::MasterTrackDeployment>& deployment) {
    if (!deployment) return nullptr;
    return {{"requested_track_activator", deployment->requested_track_activator},
            {"requested_panning_mode", deployment->requested_panning_mode},
            {"requested_pan", deployment->requested_pan}};
}

nlohmann::json encode_mix_parameter_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ParameterDeployment>&
        deployments) {
    using sunny::core::DeviceParameterValueProperty;

    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back(
            {{"effect_id", deployment.effect_id.value},
             {"source_path", deployment.source_path},
             {"device_path", deployment.device_path},
             {"parameter_name", deployment.parameter_name},
             {"value_property",
              deployment.value_property == DeviceParameterValueProperty::DisplayValue
                  ? "display_value"
                  : "value"},
             {"source_value", deployment.source_value},
             {"requested_value", deployment.requested_value},
             {"range_min", deployment.range_min},
             {"range_max", deployment.range_max},
             {"action",
              sunny::infrastructure::formats::ableton_parameter_action_name(deployment.action)},
             {"matched_name",
              deployment.matched_name ? nlohmann::json(*deployment.matched_name)
                                      : nlohmann::json(nullptr)},
             {"original_name",
              deployment.original_name ? nlohmann::json(*deployment.original_name)
                                       : nlohmann::json(nullptr)},
             {"observed_value",
              deployment.observed_value ? nlohmann::json(*deployment.observed_value)
                                        : nlohmann::json(nullptr)},
             {"observed_minimum",
              deployment.observed_minimum ? nlohmann::json(*deployment.observed_minimum)
                                          : nlohmann::json(nullptr)},
             {"observed_maximum",
              deployment.observed_maximum ? nlohmann::json(*deployment.observed_maximum)
                                          : nlohmann::json(nullptr)},
             {"is_quantized",
              deployment.is_quantized ? nlohmann::json(*deployment.is_quantized)
                                      : nlohmann::json(nullptr)},
             {"default_value",
              deployment.default_value ? nlohmann::json(*deployment.default_value)
                                       : nlohmann::json(nullptr)},
             {"value_items",
              deployment.value_items ? nlohmann::json(*deployment.value_items)
                                     : nlohmann::json(nullptr)},
             {"is_enabled",
              deployment.is_enabled ? nlohmann::json(*deployment.is_enabled)
                                    : nlohmann::json(nullptr)},
             {"parameter_state",
              deployment.parameter_state ? nlohmann::json(*deployment.parameter_state)
                                         : nlohmann::json(nullptr)},
             {"automation_state",
              deployment.automation_state ? nlohmann::json(*deployment.automation_state)
                                          : nlohmann::json(nullptr)},
             {"verified", deployment.verified}});
    }
    return encoded;
}

nlohmann::json encode_mix_parameter_coverage(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::ParameterCoverage>&
        coverage) {
    nlohmann::json encoded = nlohmann::json::array();
    for (const auto& item : coverage) {
        encoded.push_back({{"effect_id", item.effect_id.value},
                           {"source_parameters", item.source_parameters},
                           {"explicitly_mapped", item.explicitly_mapped},
                           {"target_deployable", item.target_deployable},
                           {"missing_paths", item.missing_paths},
                           {"non_parameter_residual_paths", item.non_parameter_residual_paths}});
    }
    return encoded;
}

nlohmann::json output_routing_bindings_schema() {
    const auto option = nlohmann::json{
        {"type", "object"},
        {"additionalProperties", false},
        {"properties",
         {{"display_name", {{"type", "string"}}}, {"identifier", {{"type", "string"}}}}},
        {"required", nlohmann::json::array({"display_name", "identifier"})}};
    const auto item = [&](std::string_view id) {
        return nlohmann::json{
            {"type", "object"},
            {"additionalProperties", false},
            {"properties",
             {{std::string{id}, {{"type", "integer"}, {"minimum", 0}}},
              {"type", option},
              {"channel", option},
              {"mapping_provenance", {{"type", "string"}, {"minLength", 1}}}}},
            {"required",
             nlohmann::json::array({std::string{id}, "type", "channel", "mapping_provenance"})}};
    };
    return {{"type", "object"},
            {"additionalProperties", false},
            {"properties",
             {{"part_tracks", {{"type", "array"}, {"items", item("part_id")}}},
              {"aux_returns", {{"type", "array"}, {"items", item("aux_bus_id")}}}}},
            {"required", nlohmann::json::array({"part_tracks", "aux_returns"})}};
}

bool parse_output_routing_bindings(
    const nlohmann::json& params,
    sunny::infrastructure::formats::AbletonOutputRoutingBindings& bindings,
    std::string& error) {
    bindings = {};
    if (!params.contains("output_routing_bindings")) return true;
    const auto& encoded = params.at("output_routing_bindings");
    if (!exact_fields(encoded, {"part_tracks", "aux_returns"}) ||
        !encoded.at("part_tracks").is_array() || !encoded.at("aux_returns").is_array()) {
        error = "output_routing_bindings must be a closed object with part_tracks and "
                "aux_returns arrays";
        return false;
    }
    for (const auto& item : encoded.at("part_tracks")) {
        std::uint64_t id = 0;
        auto binding = parse_route_binding(item, "part_id", id);
        if (!binding ||
            !bindings.part_tracks.emplace(sunny::core::PartId{id}, std::move(*binding)).second) {
            error = "part track routing bindings require unique uint64 part_id values, exact "
                    "dictionaries, and non-empty provenance";
            return false;
        }
    }
    for (const auto& item : encoded.at("aux_returns")) {
        std::uint64_t id = 0;
        auto binding = parse_route_binding(item, "aux_bus_id", id);
        if (!binding ||
            !bindings.aux_returns.emplace(sunny::core::AuxBusId{id}, std::move(*binding)).second) {
            error = "Aux Return routing bindings require unique uint64 aux_bus_id values, exact "
                    "dictionaries, and non-empty provenance";
            return false;
        }
    }
    return true;
}

nlohmann::json encode_output_routing_bindings(
    const sunny::infrastructure::formats::AbletonOutputRoutingBindings& bindings) {
    auto parts = nlohmann::json::array();
    for (const auto& [part_id, binding] : bindings.part_tracks) {
        auto encoded = route_binding_j(binding);
        encoded["part_id"] = part_id.value;
        parts.push_back(std::move(encoded));
    }
    auto aux = nlohmann::json::array();
    for (const auto& [aux_id, binding] : bindings.aux_returns) {
        auto encoded = route_binding_j(binding);
        encoded["aux_bus_id"] = aux_id.value;
        aux.push_back(std::move(encoded));
    }
    return {{"part_tracks", std::move(parts)}, {"aux_returns", std::move(aux)}};
}

nlohmann::json encode_output_route_deployments(
    const std::vector<sunny::infrastructure::formats::MixCompilationResult::OutputRouteDeployment>&
        deployments) {
    auto encoded = nlohmann::json::array();
    for (const auto& deployment : deployments) {
        encoded.push_back(
            {{"source", deployment.source},
             {"destination", deployment.destination},
             {"target_path", deployment.target_path},
             {"part_id",
              deployment.part_id ? nlohmann::json(deployment.part_id->value)
                                 : nlohmann::json(nullptr)},
             {"aux_bus_id",
              deployment.aux_bus_id ? nlohmann::json(deployment.aux_bus_id->value)
                                    : nlohmann::json(nullptr)},
             {"binding",
              deployment.binding ? route_binding_j(*deployment.binding) : nlohmann::json(nullptr)},
             {"observed_type_after_type_stage",
              optional_routing_option_j(deployment.observed_type_after_type_stage)},
             {"observed_type", optional_routing_option_j(deployment.observed_type)},
             {"observed_channel", optional_routing_option_j(deployment.observed_channel)},
             {"available_types", routing_options_j(deployment.available_types)},
             {"available_channels", routing_options_j(deployment.available_channels)},
             {"requested_type_available_verified", deployment.requested_type_available_verified},
             {"type_stage_verified", deployment.type_stage_verified},
             {"requested_channel_available_verified",
              deployment.requested_channel_available_verified},
             {"channel_stage_verified", deployment.channel_stage_verified},
             {"final_membership_verified", deployment.final_membership_verified},
             {"action",
              sunny::infrastructure::formats::ableton_output_route_action_name(deployment.action)},
             {"verified", deployment.verified}});
    }
    return encoded;
}

} // namespace sunny::infrastructure::mcp_detail
