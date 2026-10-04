/**
 * @file ableton_project.cpp
 * @brief Guarded two-phase Sunny project deployment to Ableton Live
 */

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string_view>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>
#include <tuple>

namespace sunny::infrastructure::formats {

using namespace sunny::core;
using nlohmann::json;

const char* ableton_parameter_origin_name(AbletonParameterOrigin origin) {
    switch (origin) {
    case AbletonParameterOrigin::Timbre:
        return "timbre";
    case AbletonParameterOrigin::Mix:
        return "mix";
    }
    return "timbre";
}

namespace {

bool has_error(const std::vector<Diagnostic>& diagnostics) {
    return std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.severity == ValidationSeverity::Error;
    });
}

bool valid_unobserved_output_routing_intent(const AbletonOutputRoutingEvidence& evidence) {
    const bool requested_destination_coherent =
        (evidence.requested_destination == AbletonOutputDestination::Master &&
         !evidence.requested_group_id) ||
        (evidence.requested_destination == AbletonOutputDestination::Group &&
         evidence.requested_group_id.has_value());
    const bool binding_coherent = evidence.requested_binding
                                      ? !evidence.requested_binding->mapping_provenance.empty() &&
                                            evidence.source_target_identity_mapped
                                      : !evidence.source_target_identity_mapped;
    return requested_destination_coherent && binding_coherent &&
           !evidence.selected_type_display_name && !evidence.selected_type_identifier &&
           !evidence.selected_channel_display_name && !evidence.selected_channel_identifier &&
           evidence.available_types.empty() && evidence.available_channels.empty() &&
           !evidence.selected_output_observed && !evidence.selected_type_available_verified &&
           !evidence.selected_channel_available_verified &&
           !evidence.selected_output_available_verified && !evidence.verified;
}

bool valid_unobserved_input_routing_intent(const AbletonInputRoutingEvidence& evidence) {
    return !evidence.selected_type_display_name && !evidence.selected_type_identifier &&
           !evidence.selected_channel_display_name && !evidence.selected_channel_identifier &&
           evidence.available_types.empty() && evidence.available_channels.empty() &&
           !evidence.selected_input_observed && !evidence.selected_type_available_verified &&
           !evidence.selected_channel_available_verified &&
           !evidence.selected_input_available_verified && !evidence.input_source_identity_mapped &&
           !evidence.external_input_neutrality_verified;
}

bool valid_unobserved_track_meter_intent(const AbletonTrackGateEvidence& evidence) {
    return evidence.requested_input_meter_level == 0.0 &&
           evidence.requested_output_meter_level == 0.0 &&
           evidence.requested_input_meter_left == 0.0 &&
           evidence.requested_input_meter_right == 0.0 &&
           evidence.requested_output_meter_left == 0.0 &&
           evidence.requested_output_meter_right == 0.0 && !evidence.observed_input_meter_level &&
           !evidence.observed_output_meter_level && !evidence.observed_input_meter_left &&
           !evidence.observed_input_meter_right && !evidence.observed_output_meter_left &&
           !evidence.observed_output_meter_right && !evidence.meter_levels_observed &&
           !evidence.input_meter_hold_quiescent_verified &&
           !evidence.output_meter_hold_quiescent_verified &&
           !evidence.meter_hold_quiescence_verified && !evidence.momentary_meter_levels_observed &&
           !evidence.input_stereo_momentary_quiescent_verified &&
           !evidence.output_stereo_momentary_quiescent_verified &&
           !evidence.meter_momentary_quiescence_verified && !evidence.meter_quiescence_verified &&
           !evidence.continuous_input_silence_verified &&
           !evidence.continuous_output_silence_verified;
}

void observe_routing_options(std::vector<AbletonRoutingOptionEvidence>& destination,
                             const json& collection,
                             std::string_view field) {
    const auto& options = collection.at(std::string(field));
    destination.reserve(options.size());
    for (const auto& option : options)
        destination.push_back({option.at("display_name").get<std::string>(),
                               option.at("identifier").get<std::string>()});
}

void observe_input_routing(AbletonInputRoutingEvidence& evidence, const json& track) {
    if (track.at("input_routing_type").is_null()) return;
    const auto& type = track.at("input_routing_type");
    const auto& channel = track.at("input_routing_channel");
    evidence.selected_type_display_name = type.at("display_name").get<std::string>();
    evidence.selected_type_identifier = type.at("identifier").get<std::string>();
    evidence.selected_channel_display_name = channel.at("display_name").get<std::string>();
    evidence.selected_channel_identifier = channel.at("identifier").get<std::string>();
    observe_routing_options(evidence.available_types,
                            track.at("available_input_routing_types"),
                            "available_input_routing_types");
    observe_routing_options(evidence.available_channels,
                            track.at("available_input_routing_channels"),
                            "available_input_routing_channels");
    evidence.selected_input_observed = true;
    evidence.selected_type_available_verified = std::any_of(
        evidence.available_types.begin(), evidence.available_types.end(), [&](const auto& option) {
            return option.display_name == *evidence.selected_type_display_name &&
                   option.identifier == *evidence.selected_type_identifier;
        });
    evidence.selected_channel_available_verified =
        std::any_of(evidence.available_channels.begin(),
                    evidence.available_channels.end(),
                    [&](const auto& option) {
                        return option.display_name == *evidence.selected_channel_display_name &&
                               option.identifier == *evidence.selected_channel_identifier;
                    });
    evidence.selected_input_available_verified =
        evidence.selected_type_available_verified && evidence.selected_channel_available_verified;
}

void observe_output_routing(AbletonOutputRoutingEvidence& evidence, const json& track) {
    const auto& type = track.at("output_routing_type");
    const auto& channel = track.at("output_routing_channel");
    evidence.selected_type_display_name = type.at("display_name").get<std::string>();
    evidence.selected_type_identifier = type.at("identifier").get<std::string>();
    evidence.selected_channel_display_name = channel.at("display_name").get<std::string>();
    evidence.selected_channel_identifier = channel.at("identifier").get<std::string>();
    observe_routing_options(evidence.available_types,
                            track.at("available_output_routing_types"),
                            "available_output_routing_types");
    observe_routing_options(evidence.available_channels,
                            track.at("available_output_routing_channels"),
                            "available_output_routing_channels");
    evidence.selected_output_observed = true;
    const auto selected_type_matches = [&](const auto& option) {
        return option.display_name == *evidence.selected_type_display_name &&
               option.identifier == *evidence.selected_type_identifier;
    };
    const auto selected_channel_matches = [&](const auto& option) {
        return option.display_name == *evidence.selected_channel_display_name &&
               option.identifier == *evidence.selected_channel_identifier;
    };
    evidence.selected_type_available_verified = std::any_of(
        evidence.available_types.begin(), evidence.available_types.end(), selected_type_matches);
    evidence.selected_channel_available_verified = std::any_of(evidence.available_channels.begin(),
                                                               evidence.available_channels.end(),
                                                               selected_channel_matches);
    evidence.selected_output_available_verified =
        evidence.selected_type_available_verified && evidence.selected_channel_available_verified;
    if (evidence.requested_binding) {
        const auto& requested = *evidence.requested_binding;
        evidence.verified =
            evidence.source_target_identity_mapped && evidence.selected_output_available_verified &&
            *evidence.selected_type_display_name == requested.type.display_name &&
            *evidence.selected_type_identifier == requested.type.identifier &&
            *evidence.selected_channel_display_name == requested.channel.display_name &&
            *evidence.selected_channel_identifier == requested.channel.identifier;
    }
}

Result<AbletonPartTrackMap> derive_part_tracks(const Score& score) {
    AbletonPartTrackMap result;
    for (std::size_t index = 0; index < score.parts.size(); ++index) {
        if (index > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return std::unexpected(ErrorCode::ProjectValidationFailed);
        result.emplace(score.parts[index].id, static_cast<int>(index));
    }
    return result;
}

Result<AbletonProjectPostconditionEvidence>
project_postcondition_intent(const ProjectView& project, const AbletonPartTrackMap& part_tracks) {
    AbletonProjectPostconditionEvidence result;
    result.master_track_gates_requested = 1;
    result.master_track_gate = AbletonMasterTrackGateEvidence{};
    result.track_gates_requested = static_cast<std::uint64_t>(project.score.parts.size());
    result.track_gates.reserve(project.score.parts.size());
    const bool any_project_solo = std::any_of(project.mix.channels.begin(),
                                              project.mix.channels.end(),
                                              [](const auto& channel) { return channel.solo; });

    for (const auto& part : project.score.parts) {
        const auto track = part_tracks.find(part.id);
        const auto channel =
            std::find_if(project.mix.channels.begin(),
                         project.mix.channels.end(),
                         [&](const auto& candidate) { return candidate.part_id == part.id; });
        if (track == part_tracks.end() || channel == project.mix.channels.end() ||
            track->second < 0)
            return std::unexpected(ErrorCode::ProjectValidationFailed);

        AbletonTrackGateEvidence evidence;
        evidence.part_id = part.id;
        evidence.track_index = track->second;
        evidence.requested_name = part.definition.name;
        evidence.requested_mute = channel->mute;
        evidence.requested_solo = channel->solo;
        evidence.requested_track_activator = channel->mute ? 0.0 : 1.0;
        evidence.expected_mixer_enabled = !channel->mute && (!any_project_solo || channel->solo);
        if (channel->group_assignment) {
            evidence.output_routing.requested_destination = AbletonOutputDestination::Group;
            evidence.output_routing.requested_group_id = *channel->group_assignment;
        }
        result.track_gates.push_back(std::move(evidence));
    }
    return result;
}

std::optional<std::uint32_t> decimal_index(const std::string& text) {
    std::uint32_t result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return result;
}

std::string device_parent_path(const std::string& device_path) {
    const auto marker = device_path.rfind("/devices/");
    return marker == std::string::npos ? std::string{} : device_path.substr(0, marker);
}

void append_device_intent(AbletonProjectPostconditionEvidence& postconditions,
                          const AbletonDeviceInsertionDeployment& deployment) {
    AbletonDevicePostconditionEvidence evidence;
    evidence.device_path = deployment.device_path;
    evidence.requested_name = deployment.requested_name;
    evidence.requested_index = deployment.requested_index;
    evidence.requested_type = deployment.requested_type;
    postconditions.devices.push_back(std::move(evidence));
}

void close_device_chain_intent(AbletonProjectPostconditionEvidence& postconditions) {
    std::map<std::string, std::uint32_t> expected_sizes;
    for (const auto& evidence : postconditions.devices) {
        const auto parent = device_parent_path(evidence.device_path);
        if (parent.empty()) continue;
        expected_sizes[parent] = std::max(expected_sizes[parent],
                                          static_cast<std::uint32_t>(evidence.requested_index + 1));
    }
    for (auto& evidence : postconditions.devices) {
        const auto expected = expected_sizes.find(device_parent_path(evidence.device_path));
        if (expected != expected_sizes.end()) evidence.expected_chain_size = expected->second;
    }
    postconditions.devices_requested = static_cast<std::uint64_t>(postconditions.devices.size());
}

void append_compilation_device_intent(AbletonProjectPostconditionEvidence& postconditions,
                                      const AbletonProjectCompilationResult& compilation) {
    for (const auto& part : compilation.timbre)
        for (const auto& deployment : part.compilation.device_deployments)
            append_device_intent(postconditions, deployment);
    for (const auto& deployment : compilation.mix.device_deployments)
        append_device_intent(postconditions, deployment);
    close_device_chain_intent(postconditions);
}

Result<void>
append_compilation_return_track_intent(AbletonProjectPostconditionEvidence& postconditions,
                                       const ProjectView& project,
                                       const AbletonProjectCompilationResult& compilation) {
    if (compilation.mix.return_tracks_requested != project.mix.aux_buses.size() ||
        compilation.mix.return_tracks_created != project.mix.aux_buses.size() ||
        compilation.mix.return_track_deployments.size() != project.mix.aux_buses.size())
        return std::unexpected(ErrorCode::ProtocolError);

    std::set<std::uint64_t> aux_ids;
    std::set<int> track_indices;
    postconditions.return_track_gates.reserve(compilation.mix.return_track_deployments.size());
    for (const auto& deployment : compilation.mix.return_track_deployments) {
        const auto aux = std::find_if(
            project.mix.aux_buses.begin(), project.mix.aux_buses.end(), [&](const auto& candidate) {
                return candidate.id == deployment.aux_bus_id;
            });
        if (aux == project.mix.aux_buses.end() || deployment.track_index < 0 ||
            !aux_ids.insert(deployment.aux_bus_id.value).second ||
            !track_indices.insert(deployment.track_index).second ||
            deployment.requested_name != aux->name || deployment.requested_mute ||
            deployment.requested_solo || deployment.requested_track_activator != 1.0 ||
            deployment.requested_crossfade_assign != 1 || deployment.requested_panning_mode != 0 ||
            deployment.requested_pan != aux->return_spatial.pan)
            return std::unexpected(ErrorCode::ProtocolError);

        AbletonReturnTrackGateEvidence evidence;
        evidence.aux_bus_id = deployment.aux_bus_id;
        evidence.track_index = deployment.track_index;
        evidence.requested_name = deployment.requested_name;
        evidence.requested_mute = deployment.requested_mute;
        evidence.requested_solo = deployment.requested_solo;
        evidence.requested_track_activator = deployment.requested_track_activator;
        evidence.requested_crossfade_assign = deployment.requested_crossfade_assign;
        evidence.requested_panning_mode = deployment.requested_panning_mode;
        if (aux->output.type == AuxOutputType::Group) {
            evidence.output_routing.requested_destination = AbletonOutputDestination::Group;
            evidence.output_routing.requested_group_id = aux->output.group_id;
        }
        postconditions.return_track_gates.push_back(std::move(evidence));
    }
    postconditions.return_track_gates_requested =
        static_cast<std::uint64_t>(postconditions.return_track_gates.size());
    return {};
}

Result<void>
append_compilation_output_route_intent(AbletonProjectPostconditionEvidence& postconditions,
                                       const AbletonProjectCompilationResult& compilation) {
    if (compilation.mix.output_route_deployments.size() !=
            compilation.mix.output_routes_requested ||
        compilation.mix.output_routes_verified > compilation.mix.output_routes_written ||
        compilation.mix.output_routes_written > compilation.mix.output_routes_requested)
        return std::unexpected(ErrorCode::ProtocolError);

    std::set<PartId> part_ids;
    std::set<AuxBusId> aux_ids;
    AbletonOutputRoutingBindings retained_bindings;
    for (const auto& deployment : compilation.mix.output_route_deployments) {
        if (deployment.part_id) {
            if (deployment.aux_bus_id || !part_ids.insert(*deployment.part_id).second)
                return std::unexpected(ErrorCode::ProtocolError);
            const auto gate =
                std::ranges::find_if(postconditions.track_gates, [&](const auto& item) {
                    return item.part_id == *deployment.part_id;
                });
            if (gate == postconditions.track_gates.end())
                return std::unexpected(ErrorCode::ProtocolError);
            if (deployment.binding) {
                if (gate->output_routing.requested_destination !=
                        AbletonOutputDestination::Master ||
                    gate->output_routing.requested_group_id ||
                    deployment.destination != "master_bus" ||
                    deployment.binding->mapping_provenance.empty())
                    return std::unexpected(ErrorCode::ProtocolError);
                gate->output_routing.requested_binding = deployment.binding;
                gate->output_routing.source_target_identity_mapped = true;
                retained_bindings.part_tracks.emplace(*deployment.part_id, *deployment.binding);
            }
            continue;
        }
        if (deployment.aux_bus_id) {
            if (!aux_ids.insert(*deployment.aux_bus_id).second)
                return std::unexpected(ErrorCode::ProtocolError);
            const auto gate =
                std::ranges::find_if(postconditions.return_track_gates, [&](const auto& item) {
                    return item.aux_bus_id == *deployment.aux_bus_id;
                });
            if (gate == postconditions.return_track_gates.end())
                return std::unexpected(ErrorCode::ProtocolError);
            if (deployment.binding) {
                if (gate->output_routing.requested_destination !=
                        AbletonOutputDestination::Master ||
                    gate->output_routing.requested_group_id ||
                    deployment.destination != "master_bus" ||
                    deployment.binding->mapping_provenance.empty())
                    return std::unexpected(ErrorCode::ProtocolError);
                gate->output_routing.requested_binding = deployment.binding;
                gate->output_routing.source_target_identity_mapped = true;
                retained_bindings.aux_returns.emplace(*deployment.aux_bus_id, *deployment.binding);
            }
            continue;
        }
        if (deployment.binding) return std::unexpected(ErrorCode::ProtocolError);
    }
    if (part_ids.size() != postconditions.track_gates.size() ||
        aux_ids.size() != postconditions.return_track_gates.size() ||
        retained_bindings != compilation.output_routing_bindings)
        return std::unexpected(ErrorCode::ProtocolError);
    return {};
}

Result<void>
validate_compilation_master_track_intent(const AbletonProjectCompilationResult& compilation) {
    if (!compilation.mix.master_track_deployment) return std::unexpected(ErrorCode::ProtocolError);
    const auto& deployment = *compilation.mix.master_track_deployment;
    if (deployment.requested_track_activator != 1.0 || deployment.requested_panning_mode != 0 ||
        deployment.requested_pan != 0.0)
        return std::unexpected(ErrorCode::ProtocolError);
    return {};
}

Result<const AbletonPropertyDeployment*> unique_property_intent(
    const AbletonCompilationResult& score, const std::string& path, const std::string& property) {
    const AbletonPropertyDeployment* result = nullptr;
    for (const auto& deployment : score.property_deployments) {
        if (deployment.path != path || deployment.property != property) continue;
        if (result != nullptr) return std::unexpected(ErrorCode::ProtocolError);
        result = &deployment;
    }
    if (result == nullptr) return std::unexpected(ErrorCode::ProtocolError);
    return result;
}

Result<void> append_compilation_clip_intent(AbletonProjectPostconditionEvidence& postconditions,
                                            const ProjectView& project,
                                            const AbletonPartTrackMap& part_tracks,
                                            const AbletonCompilationResult& score) {
    postconditions.clips.reserve(project.score.parts.size());
    for (const auto& part : project.score.parts) {
        const auto track = part_tracks.find(part.id);
        if (track == part_tracks.end() || track->second < 0)
            return std::unexpected(ErrorCode::ProjectValidationFailed);
        const auto path = LomPaths::clip(track->second, 0).to_string();
        const AbletonClipEnvelopeDeployment* envelope_intent = nullptr;
        for (const auto& deployment : score.clip_envelope_deployments) {
            if (deployment.part_id != part.id || deployment.track_index != track->second) continue;
            if (envelope_intent != nullptr) return std::unexpected(ErrorCode::ProtocolError);
            envelope_intent = &deployment;
        }
        if (envelope_intent == nullptr || envelope_intent->requested_has_envelopes)
            return std::unexpected(ErrorCode::ProtocolError);
        auto name = unique_property_intent(score, path, "name");
        auto start = unique_property_intent(score, path, "start_marker");
        auto end = unique_property_intent(score, path, "end_marker");
        auto looping = unique_property_intent(score, path, "looping");
        auto muted = unique_property_intent(score, path, "muted");
        const bool live_11_clip_state_available = score.target_profile.live_version.at_least(11, 0);
        Result<const AbletonPropertyDeployment*> launch_mode =
            std::unexpected(ErrorCode::ProtocolError);
        Result<const AbletonPropertyDeployment*> launch_quantization =
            std::unexpected(ErrorCode::ProtocolError);
        Result<const AbletonPropertyDeployment*> legato = std::unexpected(ErrorCode::ProtocolError);
        Result<const AbletonPropertyDeployment*> velocity_amount =
            std::unexpected(ErrorCode::ProtocolError);
        Result<const AbletonPropertyDeployment*> groove = std::unexpected(ErrorCode::ProtocolError);
        if (live_11_clip_state_available) {
            launch_mode = unique_property_intent(score, path, "launch_mode");
            launch_quantization = unique_property_intent(score, path, "launch_quantization");
            legato = unique_property_intent(score, path, "legato");
            velocity_amount = unique_property_intent(score, path, "velocity_amount");
            groove = unique_property_intent(score, path, "groove");
        }
        auto numerator = unique_property_intent(score, path, "signature_numerator");
        auto denominator = unique_property_intent(score, path, "signature_denominator");
        if (!name || !start || !end || !looping || !muted || !numerator || !denominator ||
            (live_11_clip_state_available &&
             (!launch_mode || !launch_quantization || !legato || !velocity_amount || !groove)))
            return std::unexpected(ErrorCode::ProtocolError);

        const auto& name_value = (*name)->requested;
        const auto& start_value = (*start)->requested;
        const auto& end_value = (*end)->requested;
        const auto& looping_value = (*looping)->requested;
        const auto& muted_value = (*muted)->requested;
        const auto& numerator_value = (*numerator)->requested;
        const auto& denominator_value = (*denominator)->requested;
        if (!name_value.is_string() || name_value.get<std::string>() != part.definition.name ||
            !start_value.is_number_float() || !end_value.is_number_float() ||
            !looping_value.is_boolean() || looping_value.get<bool>() || !muted_value.is_boolean() ||
            muted_value.get<bool>() ||
            (live_11_clip_state_available &&
             (!(*launch_mode)->requested.is_number_integer() ||
              (*launch_mode)->requested.get<int>() != 0 ||
              !(*launch_quantization)->requested.is_number_integer() ||
              (*launch_quantization)->requested.get<int>() != 1 ||
              !(*legato)->requested.is_boolean() || (*legato)->requested.get<bool>() ||
              !(*velocity_amount)->requested.is_number_float() ||
              !std::isfinite((*velocity_amount)->requested.get<double>()) ||
              (*velocity_amount)->requested.get<double>() != 0.0 ||
              !(*groove)->requested.is_null())) ||
            !(numerator_value.is_number_integer() || numerator_value.is_number_unsigned()) ||
            !(denominator_value.is_number_integer() || denominator_value.is_number_unsigned()))
            return std::unexpected(ErrorCode::ProtocolError);

        const double requested_start = start_value.get<double>();
        const double requested_end = end_value.get<double>();
        const auto requested_numerator = numerator_value.get<int>();
        const auto requested_denominator = denominator_value.get<int>();
        if (!std::isfinite(requested_start) || !std::isfinite(requested_end) ||
            requested_start != 0.0 || requested_end <= requested_start || requested_numerator < 1 ||
            requested_numerator > 99 ||
            (requested_denominator != 1 && requested_denominator != 2 &&
             requested_denominator != 4 && requested_denominator != 8 &&
             requested_denominator != 16))
            return std::unexpected(ErrorCode::ProtocolError);

        AbletonClipPostconditionEvidence evidence;
        evidence.part_id = part.id;
        evidence.track_index = track->second;
        evidence.requested_name = name_value.get<std::string>();
        evidence.requested_length = requested_end - requested_start;
        evidence.requested_signature_numerator = requested_numerator;
        evidence.requested_signature_denominator = requested_denominator;
        evidence.requested_start_marker = requested_start;
        evidence.requested_end_marker = requested_end;
        evidence.requested_end_time = requested_end;
        evidence.location_identity_available = live_11_clip_state_available;
        evidence.groove_state_available = live_11_clip_state_available;
        evidence.launch_state_available = live_11_clip_state_available;
        postconditions.clips.push_back(std::move(evidence));
    }
    postconditions.clips_requested = static_cast<std::uint64_t>(postconditions.clips.size());
    return {};
}

bool valid_requested_note(const AbletonNoteDeployment::RequestedNote& note) {
    return note.pitch >= 0 && note.pitch <= 127 && std::isfinite(note.start_time) &&
           note.start_time >= 0.0 && std::isfinite(note.duration) && note.duration > 0.0 &&
           std::isfinite(note.velocity) && note.velocity >= 0.0 && note.velocity <= 127.0 &&
           note.probability == 1.0 && note.velocity_deviation == 0.0 &&
           std::isfinite(note.release_velocity) && note.release_velocity >= 0.0 &&
           note.release_velocity <= 127.0 &&
           std::trunc(note.release_velocity) == note.release_velocity;
}

bool valid_observed_note(const AbletonNoteDeployment::ObservedNote& note) {
    return note.pitch >= 0 && note.pitch <= 127 && std::isfinite(note.start_time) &&
           note.start_time >= 0.0 && std::isfinite(note.duration) && note.duration > 0.0 &&
           std::isfinite(note.velocity) && note.velocity >= 0.0 && note.velocity <= 127.0 &&
           std::isfinite(note.probability) && note.probability >= 0.0 && note.probability <= 1.0 &&
           std::isfinite(note.velocity_deviation) && note.velocity_deviation >= -127.0 &&
           note.velocity_deviation <= 127.0 && std::isfinite(note.release_velocity) &&
           note.release_velocity >= 0.0 && note.release_velocity <= 127.0;
}

Result<void> append_compilation_note_intent(AbletonProjectPostconditionEvidence& postconditions,
                                            const AbletonPartTrackMap& part_tracks,
                                            const AbletonCompilationResult& score) {
    if (score.note_deployments.size() != score.note_batches_requested)
        return std::unexpected(ErrorCode::ProtocolError);

    std::set<std::uint64_t> deployed_parts;
    std::uint64_t requested_notes = 0;
    postconditions.note_batches.reserve(score.note_deployments.size());
    for (const auto& deployment : score.note_deployments) {
        const auto track = part_tracks.find(deployment.part_id);
        if (track == part_tracks.end() || track->second < 0 ||
            deployment.track_index != track->second || deployment.requested_notes.empty() ||
            deployment.notes_requested != deployment.requested_notes.size() ||
            !deployed_parts.insert(deployment.part_id.value).second ||
            !std::all_of(deployment.requested_notes.begin(),
                         deployment.requested_notes.end(),
                         valid_requested_note))
            return std::unexpected(ErrorCode::ProtocolError);

        const std::set<int> created_ids(deployment.created_note_ids.begin(),
                                        deployment.created_note_ids.end());
        std::set<int> observed_ids;
        for (const auto& note : deployment.observed_notes) {
            if (!valid_observed_note(note) || !observed_ids.insert(note.note_id).second)
                return std::unexpected(ErrorCode::ProtocolError);
        }

        if (deployment.action == AbletonNoteAction::Inserted) {
            if (!deployment.cardinality_verified ||
                deployment.created_note_ids.size() != deployment.requested_notes.size() ||
                created_ids.size() != deployment.created_note_ids.size() ||
                deployment.observed_notes.size() != deployment.requested_notes.size() ||
                observed_ids != created_ids)
                return std::unexpected(ErrorCode::ProtocolError);
        } else if (!deployment.created_note_ids.empty() || !deployment.observed_notes.empty() ||
                   deployment.cardinality_verified || deployment.properties_verified) {
            return std::unexpected(ErrorCode::ProtocolError);
        }

        AbletonNotePostconditionEvidence evidence;
        evidence.part_id = deployment.part_id;
        evidence.track_index = deployment.track_index;
        evidence.deployment_action = deployment.action;
        evidence.requested_notes = deployment.requested_notes;
        evidence.requested_note_ids = deployment.created_note_ids;
        postconditions.note_batches.push_back(std::move(evidence));
        requested_notes += deployment.notes_requested;
    }
    if (requested_notes != score.notes_requested) return std::unexpected(ErrorCode::ProtocolError);
    postconditions.note_batches_requested =
        static_cast<std::uint64_t>(postconditions.note_batches.size());
    postconditions.notes_requested = requested_notes;
    return {};
}

Result<void> append_compilation_song_intent(AbletonProjectPostconditionEvidence& postconditions,
                                            const ProjectView& project,
                                            const AbletonCompilationResult& score) {
    auto tempo = unique_property_intent(score, LomPaths::song().to_string(), "tempo");
    auto numerator =
        unique_property_intent(score, LomPaths::song().to_string(), "signature_numerator");
    auto denominator =
        unique_property_intent(score, LomPaths::song().to_string(), "signature_denominator");
    auto scene_name = unique_property_intent(score, LomPaths::scene(0).to_string(), "name");
    auto scene_tempo =
        unique_property_intent(score, LomPaths::scene(0).to_string(), "tempo_enabled");
    auto scene_signature =
        unique_property_intent(score, LomPaths::scene(0).to_string(), "time_signature_enabled");
    if (!tempo || !numerator || !denominator || !scene_name || !scene_tempo || !scene_signature)
        return std::unexpected(ErrorCode::ProtocolError);

    const auto& tempo_value = (*tempo)->requested;
    const auto& numerator_value = (*numerator)->requested;
    const auto& denominator_value = (*denominator)->requested;
    const auto& scene_name_value = (*scene_name)->requested;
    const auto& scene_tempo_value = (*scene_tempo)->requested;
    const auto& scene_signature_value = (*scene_signature)->requested;
    if (!tempo_value.is_number_float() || !std::isfinite(tempo_value.get<double>()) ||
        tempo_value.get<double>() < 20.0 || tempo_value.get<double>() > 999.0 ||
        !(numerator_value.is_number_integer() || numerator_value.is_number_unsigned()) ||
        !(denominator_value.is_number_integer() || denominator_value.is_number_unsigned()) ||
        !scene_name_value.is_string() ||
        scene_name_value.get<std::string>() != project.score.metadata.title ||
        !scene_tempo_value.is_boolean() || scene_tempo_value.get<bool>() ||
        !scene_signature_value.is_boolean() || scene_signature_value.get<bool>())
        return std::unexpected(ErrorCode::ProtocolError);

    AbletonSongPostconditionEvidence evidence;
    evidence.requested_tempo = tempo_value.get<double>();
    evidence.requested_signature_numerator = numerator_value.get<int>();
    evidence.requested_signature_denominator = denominator_value.get<int>();
    evidence.requested_scene_name = scene_name_value.get<std::string>();
    if (evidence.requested_signature_numerator < 1 || evidence.requested_signature_numerator > 99 ||
        (evidence.requested_signature_denominator != 1 &&
         evidence.requested_signature_denominator != 2 &&
         evidence.requested_signature_denominator != 4 &&
         evidence.requested_signature_denominator != 8 &&
         evidence.requested_signature_denominator != 16))
        return std::unexpected(ErrorCode::ProtocolError);
    postconditions.song_states_requested = 1;
    postconditions.song_state = std::move(evidence);

    postconditions.cues.reserve(score.marker_deployments.size());
    for (const auto& marker : score.marker_deployments) {
        if (!std::isfinite(marker.requested_time) || marker.requested_time < 0.0)
            return std::unexpected(ErrorCode::ProtocolError);
        postconditions.cues.push_back(
            {marker.requested_time, marker.requested_name, std::nullopt, std::nullopt, false});
    }
    postconditions.cues_requested = static_cast<std::uint64_t>(postconditions.cues.size());
    return {};
}

Result<void> append_compilation_mixer_intent(AbletonProjectPostconditionEvidence& postconditions,
                                             const AbletonProjectCompilationResult& compilation) {
    std::map<std::pair<std::string, std::string>, double> final_values;
    const auto append = [&](const std::vector<AbletonPropertyDeployment>& deployments) -> bool {
        for (const auto& deployment : deployments) {
            if (deployment.path.find("/mixer_device/") == std::string::npos) continue;
            if ((deployment.property != "value" && deployment.property != "display_value") ||
                !deployment.requested.is_number_float() ||
                !std::isfinite(deployment.requested.get<double>()))
                return false;
            final_values[{deployment.path, deployment.property}] =
                deployment.requested.get<double>();
        }
        return true;
    };
    if (!append(compilation.score.property_deployments) ||
        !append(compilation.mix.property_deployments))
        return std::unexpected(ErrorCode::ProtocolError);

    postconditions.mixer_properties.reserve(final_values.size());
    for (const auto& [key, value] : final_values) {
        const auto path = LomPath::parse(key.first);
        if (!path.is_canonical()) return std::unexpected(ErrorCode::ProtocolError);
        const auto& segments = path.segments;
        const auto parameter = segments.empty() ? std::string{} : segments.back();
        const bool requested_is_quantized = parameter == "track_activator";
        if (parameter != "volume" && parameter != "panning" && parameter != "track_activator" &&
            !(segments.size() >= 2 && segments[segments.size() - 2] == "sends" &&
              decimal_index(parameter).has_value()))
            return std::unexpected(ErrorCode::ProtocolError);
        AbletonMixerPostconditionEvidence evidence;
        evidence.parameter_path = key.first;
        evidence.value_property = key.second;
        evidence.requested_value = value;
        evidence.requested_is_quantized = requested_is_quantized;
        postconditions.mixer_properties.push_back(std::move(evidence));
    }
    postconditions.mixer_properties_requested =
        static_cast<std::uint64_t>(postconditions.mixer_properties.size());
    return {};
}

template <typename Deployment>
bool valid_parameter_deployment(const Deployment& deployment, const LomPath& device_path) {
    if ((deployment.action != AbletonParameterAction::NotApplied &&
         deployment.action != AbletonParameterAction::RecordedOnly &&
         deployment.action != AbletonParameterAction::Set) ||
        (deployment.value_property != DeviceParameterValueProperty::InternalValue &&
         deployment.value_property != DeviceParameterValueProperty::DisplayValue))
        return false;
    const auto query = LomProtocol::call_method(
        device_path,
        "sunny_get_device_parameter",
        {deployment.parameter_name,
         deployment.value_property == DeviceParameterValueProperty::DisplayValue
             ? std::string{"display_value"}
             : std::string{"value"}});
    if (!LomProtocol::validate_request(query) || !std::isfinite(deployment.requested_value) ||
        !std::isfinite(deployment.range_min) || !std::isfinite(deployment.range_max) ||
        !(deployment.range_max > deployment.range_min))
        return false;

    const bool has_any_observation =
        deployment.matched_name || deployment.original_name || deployment.observed_value ||
        deployment.observed_minimum || deployment.observed_maximum || deployment.is_quantized ||
        deployment.default_value || deployment.value_items || deployment.is_enabled ||
        deployment.parameter_state || deployment.automation_state;
    const bool has_conditional_domain =
        deployment.is_quantized &&
        (*deployment.is_quantized
             ? (!deployment.default_value && deployment.value_items.has_value())
             : (deployment.default_value.has_value() && !deployment.value_items));
    const bool has_complete_observation =
        deployment.matched_name && deployment.original_name && deployment.observed_value &&
        deployment.observed_minimum && deployment.observed_maximum && deployment.is_quantized &&
        has_conditional_domain && deployment.is_enabled && deployment.parameter_state &&
        deployment.automation_state;
    if (deployment.action == AbletonParameterAction::Set) {
        if (!has_complete_observation || (*deployment.matched_name != deployment.parameter_name &&
                                          *deployment.original_name != deployment.parameter_name))
            return false;
    } else if (has_any_observation || deployment.verified) {
        return false;
    }
    return !deployment.verified || deployment.action == AbletonParameterAction::Set;
}

std::optional<int> device_track_index(const LomPath& path) {
    if (path.segments.size() != 5 || path.segments[0] != "song" || path.segments[1] != "tracks" ||
        path.segments[3] != "devices")
        return std::nullopt;
    const auto index = decimal_index(path.segments[2]);
    if (!index || *index > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
        return std::nullopt;
    return static_cast<int>(*index);
}

Result<void>
append_compilation_parameter_intent(AbletonProjectPostconditionEvidence& postconditions,
                                    const AbletonProjectCompilationResult& compilation) {
    std::set<std::tuple<std::string, std::string, DeviceParameterValueProperty>> targets;
    const auto append = [&](AbletonParameterPostconditionEvidence evidence) -> Result<void> {
        const auto key =
            std::tuple{evidence.device_path, evidence.requested_name, evidence.value_property};
        if (!targets.insert(key).second) return std::unexpected(ErrorCode::ProtocolError);
        postconditions.device_parameters.push_back(std::move(evidence));
        return {};
    };

    for (const auto& part : compilation.timbre) {
        std::uint64_t mapped = 0;
        std::uint64_t verified = 0;
        for (const auto& deployment : part.compilation.parameter_deployments) {
            if (part.track_index < 0 ||
                deployment.device_index >
                    static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
                return std::unexpected(ErrorCode::ProtocolError);
            const auto path = LomPaths::track(part.track_index)
                                  .child("devices")
                                  .child(static_cast<int>(deployment.device_index));
            if (!valid_parameter_deployment(deployment, path))
                return std::unexpected(ErrorCode::ProtocolError);
            mapped += deployment.action != AbletonParameterAction::NotApplied ? 1U : 0U;
            verified += deployment.verified ? 1U : 0U;

            AbletonParameterPostconditionEvidence evidence;
            evidence.origin = AbletonParameterOrigin::Timbre;
            evidence.part_id = part.part_id;
            evidence.track_index = part.track_index;
            evidence.source_path = deployment.ir_path;
            evidence.device_path = path.to_string();
            evidence.requested_name = deployment.parameter_name;
            evidence.value_property = deployment.value_property;
            evidence.requested_value = deployment.requested_value;
            evidence.range_min = deployment.range_min;
            evidence.range_max = deployment.range_max;
            evidence.deployment_action = deployment.action;
            auto appended = append(std::move(evidence));
            if (!appended) return std::unexpected(appended.error());
        }
        if (mapped != part.compilation.parameters_mapped ||
            verified != part.compilation.parameters_verified)
            return std::unexpected(ErrorCode::ProtocolError);
    }

    std::uint64_t mix_mapped = 0;
    std::uint64_t mix_verified = 0;
    for (const auto& deployment : compilation.mix.parameter_deployments) {
        const auto path = LomPath::parse(deployment.device_path);
        if (!valid_parameter_deployment(deployment, path))
            return std::unexpected(ErrorCode::ProtocolError);
        mix_mapped += deployment.action != AbletonParameterAction::NotApplied ? 1U : 0U;
        mix_verified += deployment.verified ? 1U : 0U;

        AbletonParameterPostconditionEvidence evidence;
        evidence.origin = AbletonParameterOrigin::Mix;
        evidence.track_index = device_track_index(path);
        if (evidence.track_index) {
            const auto part = std::find_if(
                compilation.part_tracks.begin(),
                compilation.part_tracks.end(),
                [&](const auto& item) { return item.second == *evidence.track_index; });
            if (part != compilation.part_tracks.end()) evidence.part_id = part->first;
        }
        evidence.source_path = deployment.source_path;
        evidence.effect_id = deployment.effect_id;
        evidence.device_path = deployment.device_path;
        evidence.requested_name = deployment.parameter_name;
        evidence.value_property = deployment.value_property;
        evidence.requested_value = deployment.requested_value;
        evidence.range_min = deployment.range_min;
        evidence.range_max = deployment.range_max;
        evidence.deployment_action = deployment.action;
        auto appended = append(std::move(evidence));
        if (!appended) return std::unexpected(appended.error());
    }
    if (mix_mapped != compilation.mix.parameters_mapped ||
        mix_verified != compilation.mix.parameters_verified)
        return std::unexpected(ErrorCode::ProtocolError);

    postconditions.device_parameters_requested =
        static_cast<std::uint64_t>(postconditions.device_parameters.size());
    return {};
}

bool equivalent_float(double expected, double observed) {
    const double tolerance = std::max(1.0e-9, std::abs(expected) * 1.0e-6);
    return std::isfinite(expected) && std::isfinite(observed) &&
           std::abs(expected - observed) <= tolerance;
}

using NotePropertyState = std::tuple<int, double, double, double, bool, double, double, double>;

std::multiset<NotePropertyState>
requested_note_states(const std::vector<AbletonNoteDeployment::RequestedNote>& notes) {
    std::multiset<NotePropertyState> result;
    for (const auto& note : notes)
        result.emplace(note.pitch,
                       note.start_time,
                       note.duration,
                       note.velocity,
                       note.muted,
                       note.probability,
                       note.velocity_deviation,
                       note.release_velocity);
    return result;
}

Result<void> observe_final_notes(AbletonProjectPostconditionEvidence& result,
                                 LomTransport& transport) {
    if (!result.observed) return std::unexpected(ErrorCode::ProtocolError);
    auto target_profile = transport.target_profile();
    if (!target_profile || !*target_profile) return std::unexpected(ErrorCode::ProtocolError);

    for (auto& evidence : result.note_batches) {
        if (evidence.deployment_action != AbletonNoteAction::Inserted) {
            result.warnings.push_back(
                "Post-deployment notes for Part " + std::to_string(evidence.part_id.value) +
                " cannot be verified because the requested batch was not executed");
            continue;
        }

        const auto clip =
            std::find_if(result.clips.begin(), result.clips.end(), [&](const auto& c) {
                return c.part_id == evidence.part_id && c.track_index == evidence.track_index &&
                       c.slot_index == evidence.slot_index;
            });
        if (clip == result.clips.end() || clip->observed_is_midi_clip != true) {
            result.warnings.push_back(
                "Post-deployment notes for Part " + std::to_string(evidence.part_id.value) +
                " cannot be queried because the final snapshot does not contain its MIDI Clip");
            continue;
        }

        const auto response = transport.send(ableton_note_population_request(
            LomPaths::clip(evidence.track_index, static_cast<int>(evidence.slot_index)),
            **target_profile,
            clip->requested_end_marker));
        if (!response.success) return std::unexpected(ErrorCode::SendFailed);
        auto observed = parse_ableton_note_readback(response);
        if (!observed) return std::unexpected(observed.error());
        evidence.observed_notes = std::move(*observed);

        const std::set<int> requested_ids(evidence.requested_note_ids.begin(),
                                          evidence.requested_note_ids.end());
        std::set<int> observed_ids;
        auto unmatched = requested_note_states(evidence.requested_notes);
        std::uint64_t matching_notes = 0;
        for (const auto& note : evidence.observed_notes) {
            observed_ids.insert(note.note_id);
            const auto requested = unmatched.find({note.pitch,
                                                   note.start_time,
                                                   note.duration,
                                                   note.velocity,
                                                   note.muted,
                                                   note.probability,
                                                   note.velocity_deviation,
                                                   note.release_velocity});
            if (requested != unmatched.end()) {
                unmatched.erase(requested);
                ++matching_notes;
            }
        }
        evidence.identity_verified = requested_ids.size() == evidence.requested_note_ids.size() &&
                                     observed_ids == requested_ids;
        evidence.properties_verified =
            unmatched.empty() && evidence.observed_notes.size() == evidence.requested_notes.size();
        evidence.entire_clip_population_observed = (**target_profile).live_version.at_least(11, 1);
        if (!evidence.entire_clip_population_observed) {
            evidence.observed_time_span = clip->requested_end_marker;
            result.warnings.push_back(
                "Live 11.0 final note readback for Part " + std::to_string(evidence.part_id.value) +
                " covers all pitches within [0, " + std::to_string(clip->requested_end_marker) +
                ") quarter-note beats; notes outside this interval remain unobserved");
        }
        evidence.verified = evidence.identity_verified && evidence.properties_verified &&
                            evidence.entire_clip_population_observed;
        if (evidence.identity_verified) result.notes_verified += matching_notes;
        if (evidence.verified) {
            ++result.note_batches_verified;
        } else if (!evidence.identity_verified || !evidence.properties_verified) {
            result.warnings.push_back(
                "Post-deployment Clip notes for Part " + std::to_string(evidence.part_id.value) +
                " do not prove the exact created-ID set and requested property multiset");
        }
    }
    return {};
}

Result<void> observe_final_device_parameters(AbletonProjectPostconditionEvidence& result,
                                             LomTransport& transport) {
    if (!result.observed) return std::unexpected(ErrorCode::ProtocolError);
    for (auto& evidence : result.device_parameters) {
        if (evidence.deployment_action != AbletonParameterAction::Set) {
            result.warnings.push_back("Post-deployment DeviceParameter '" + evidence.device_path +
                                      "/" + evidence.requested_name +
                                      "' cannot be verified because its write was not set");
            continue;
        }

        const auto device =
            std::find_if(result.devices.begin(), result.devices.end(), [&](const auto& candidate) {
                return candidate.device_path == evidence.device_path;
            });
        if (device == result.devices.end() || !device->verified) {
            result.warnings.push_back(
                "Post-deployment DeviceParameter '" + evidence.device_path + "/" +
                evidence.requested_name +
                "' cannot be queried because its final device structure is unverified");
            continue;
        }

        auto observation = observe_device_parameter(LomPath::parse(evidence.device_path),
                                                    evidence.requested_name,
                                                    evidence.value_property,
                                                    transport);
        if (!observation) return std::unexpected(observation.error());
        evidence.matched_name = observation->matched_name;
        evidence.original_name = observation->original_name;
        evidence.observed_value = observation->observed;
        evidence.observed_minimum = observation->minimum;
        evidence.observed_maximum = observation->maximum;
        evidence.observed_is_quantized = observation->is_quantized;
        evidence.observed_default_value = observation->default_value;
        evidence.observed_value_items = observation->value_items;
        evidence.observed_is_enabled = observation->is_enabled;
        evidence.observed_state = observation->state;
        evidence.observed_automation_state = observation->automation_state;

        evidence.identity_verified = evidence.matched_name && evidence.original_name &&
                                     (*evidence.matched_name == evidence.requested_name ||
                                      *evidence.original_name == evidence.requested_name);
        evidence.value_verified =
            evidence.observed_value &&
            equivalent_float(evidence.requested_value, *evidence.observed_value);
        if (evidence.value_property == DeviceParameterValueProperty::InternalValue) {
            evidence.range_verified =
                evidence.observed_minimum && evidence.observed_maximum &&
                equivalent_float(evidence.range_min, *evidence.observed_minimum) &&
                equivalent_float(evidence.range_max, *evidence.observed_maximum);
        }
        evidence.enabled_verified = evidence.observed_is_enabled == true;
        evidence.state_verified = evidence.observed_state == 0;
        evidence.automation_verified = evidence.observed_automation_state == 0;
        evidence.verified = evidence.identity_verified && evidence.value_verified &&
                            evidence.range_verified != false && evidence.enabled_verified &&
                            evidence.state_verified && evidence.automation_verified;
        if (evidence.verified) {
            ++result.device_parameters_verified;
        } else {
            result.warnings.push_back(
                "Post-deployment DeviceParameter '" + evidence.device_path + "/" +
                evidence.requested_name +
                "' does not prove the requested identity, value, applicable range, enabled, "
                "active, and unautomated state");
        }
    }
    return {};
}

const json* observed_device_chain(const json& song, const LomPath& path) {
    const auto& segments = path.segments;
    if (segments.size() == 5 && segments[0] == "song" && segments[3] == "devices") {
        const auto target_index = decimal_index(segments[2]);
        const char* collection = nullptr;
        if (segments[1] == "tracks") collection = "tracks";
        if (segments[1] == "return_tracks") collection = "return_tracks";
        if (!target_index || collection == nullptr || *target_index >= song.at(collection).size())
            return nullptr;
        return &song.at(collection).at(*target_index).at("devices");
    }
    if (segments.size() == 4 && segments[0] == "song" && segments[1] == "master_track" &&
        segments[2] == "devices")
        return &song.at("master_track").at("devices");
    return nullptr;
}

bool valid_device_intent(const AbletonDevicePostconditionEvidence& evidence) {
    const auto path = LomPath::parse(evidence.device_path);
    if (!path.is_canonical() || (evidence.requested_type != 1 && evidence.requested_type != 2) ||
        evidence.expected_chain_size == 0 ||
        evidence.requested_index >= evidence.expected_chain_size)
        return false;

    const auto& segments = path.segments;
    const bool track_device = segments.size() == 5 && segments[0] == "song" &&
                              (segments[1] == "tracks" || segments[1] == "return_tracks") &&
                              decimal_index(segments[2]).has_value() && segments[3] == "devices";
    const bool master_device = segments.size() == 4 && segments[0] == "song" &&
                               segments[1] == "master_track" && segments[2] == "devices";
    if (!track_device && !master_device) return false;
    const auto device_index = decimal_index(segments.back());
    return device_index && *device_index == evidence.requested_index;
}

const json* observed_mixer_parameter(const json& song, const LomPath& path) {
    const auto& segments = path.segments;
    const json* mixer = nullptr;
    std::size_t property_index = 0;
    if (segments.size() >= 5 && segments[0] == "song" &&
        (segments[1] == "tracks" || segments[1] == "return_tracks") &&
        segments[3] == "mixer_device") {
        const auto target_index = decimal_index(segments[2]);
        if (!target_index || *target_index >= song.at(segments[1]).size()) return nullptr;
        mixer = &song.at(segments[1]).at(*target_index).at("mixer");
        property_index = 4;
    } else if (segments.size() >= 4 && segments[0] == "song" && segments[1] == "master_track" &&
               segments[2] == "mixer_device") {
        mixer = &song.at("master_track").at("mixer");
        property_index = 3;
    } else {
        return nullptr;
    }

    if (segments.size() == property_index + 1 &&
        (segments[property_index] == "volume" || segments[property_index] == "panning" ||
         segments[property_index] == "track_activator"))
        return &mixer->at(segments[property_index]);
    if (segments.size() == property_index + 2 && segments[property_index] == "sends") {
        const auto send_index = decimal_index(segments[property_index + 1]);
        if (!send_index || *send_index >= mixer->at("sends").size()) return nullptr;
        return &mixer->at("sends").at(*send_index);
    }
    return nullptr;
}

bool valid_mixer_intent(const AbletonMixerPostconditionEvidence& evidence) {
    if (!std::isfinite(evidence.requested_value) ||
        (evidence.value_property != "value" && evidence.value_property != "display_value"))
        return false;
    const auto path = LomPath::parse(evidence.parameter_path);
    if (!path.is_canonical()) return false;
    const auto& segments = path.segments;
    const bool track_parameter = segments.size() >= 5 && segments[0] == "song" &&
                                 (segments[1] == "tracks" || segments[1] == "return_tracks") &&
                                 decimal_index(segments[2]).has_value() &&
                                 segments[3] == "mixer_device";
    const bool master_parameter = segments.size() >= 4 && segments[0] == "song" &&
                                  segments[1] == "master_track" && segments[2] == "mixer_device";
    if (!track_parameter && !master_parameter) return false;
    const std::size_t property_index = track_parameter ? 4 : 3;
    if (segments.size() == property_index + 1) {
        const auto& parameter = segments[property_index];
        if (parameter != "volume" && parameter != "panning" && parameter != "track_activator")
            return false;
        return evidence.requested_is_quantized == (parameter == "track_activator");
    }
    return !evidence.requested_is_quantized && segments.size() == property_index + 2 &&
           segments[property_index] == "sends" &&
           decimal_index(segments[property_index + 1]).has_value();
}

Result<AbletonProjectPostconditionEvidence>
evaluate_postcondition_intent(AbletonProjectPostconditionEvidence result,
                              const AbletonTargetSnapshot& snapshot) {
    auto validated = target_snapshot_from_json(target_snapshot_to_json(snapshot));
    if (!validated) return std::unexpected(validated.error());

    result.observed = true;
    const auto& song = validated->song_state;
    const auto& tracks = song.at("tracks");
    // Live 11 already has take lanes (Live 11 manual, Comping). The adapter
    // cannot inspect their topology on that version; a null snapshot field is
    // unavailable evidence, never evidence that the Set has no take lanes.
    if (result.song_state) {
        auto& evidence = *result.song_state;
        if (evidence.requested_is_counting_in || evidence.requested_arrangement_overdub ||
            evidence.requested_overdub || evidence.requested_record_mode ||
            evidence.requested_session_record || evidence.requested_session_automation_record ||
            evidence.requested_is_ableton_link_enabled ||
            evidence.requested_is_ableton_link_start_stop_sync_enabled ||
            evidence.requested_tempo_follower_enabled || evidence.requested_nudge_down ||
            evidence.requested_nudge_up || evidence.requested_back_to_arranger ||
            evidence.requested_re_enable_automation_enabled ||
            evidence.requested_arrangement_loop || evidence.requested_metronome ||
            evidence.requested_scene_triggered)
            return std::unexpected(ErrorCode::ProtocolError);
        evidence.observed_tempo = song.at("tempo").get<double>();
        evidence.observed_signature_numerator = song.at("signature_numerator").get<int>();
        evidence.observed_signature_denominator = song.at("signature_denominator").get<int>();
        evidence.observed_is_playing = song.at("is_playing").get<bool>();
        evidence.observed_is_counting_in = song.at("is_counting_in").get<bool>();
        evidence.observed_arrangement_overdub = song.at("arrangement_overdub").get<bool>();
        evidence.observed_overdub = song.at("overdub").get<bool>();
        evidence.observed_record_mode = song.at("record_mode").get<bool>();
        evidence.observed_session_record = song.at("session_record").get<bool>();
        evidence.observed_session_automation_record =
            song.at("session_automation_record").get<bool>();
        evidence.observed_is_ableton_link_enabled = song.at("is_ableton_link_enabled").get<bool>();
        evidence.observed_is_ableton_link_start_stop_sync_enabled =
            song.at("is_ableton_link_start_stop_sync_enabled").get<bool>();
        evidence.observed_tempo_follower_enabled = song.at("tempo_follower_enabled").get<bool>();
        evidence.observed_nudge_down = song.at("nudge_down").get<bool>();
        evidence.observed_nudge_up = song.at("nudge_up").get<bool>();
        evidence.observed_back_to_arranger = song.at("back_to_arranger").get<bool>();
        evidence.observed_re_enable_automation_enabled =
            song.at("re_enable_automation_enabled").get<bool>();
        evidence.observed_arrangement_loop = song.at("loop").get<bool>();
        evidence.observed_metronome = song.at("metronome").get<bool>();
        evidence.transport_stopped_verified = !*evidence.observed_is_playing;
        evidence.recording_modes_quiescent_verified =
            !*evidence.observed_is_counting_in && !*evidence.observed_arrangement_overdub &&
            !*evidence.observed_overdub && !*evidence.observed_record_mode &&
            !*evidence.observed_session_record && !*evidence.observed_session_automation_record;
        evidence.public_tempo_controls_quiescent_verified =
            !*evidence.observed_is_ableton_link_enabled &&
            !*evidence.observed_is_ableton_link_start_stop_sync_enabled &&
            !*evidence.observed_tempo_follower_enabled && !*evidence.observed_nudge_down &&
            !*evidence.observed_nudge_up;
        evidence.arrangement_playback_aligned_verified = !*evidence.observed_back_to_arranger;
        evidence.automation_overrides_quiescent_verified =
            !*evidence.observed_re_enable_automation_enabled;
        evidence.arrangement_loop_disabled_verified = !*evidence.observed_arrangement_loop;
        evidence.metronome_disabled_verified = !*evidence.observed_metronome;
        const auto& scenes = song.at("scenes");
        evidence.observed_scene_triggered_states.reserve(scenes.size());
        for (const auto& scene : scenes)
            evidence.observed_scene_triggered_states.push_back(
                scene.at("is_triggered").get<bool>());
        evidence.scene_trigger_states_observed =
            evidence.observed_scene_triggered_states.size() == scenes.size();
        evidence.all_scene_launches_quiescent_verified =
            evidence.scene_trigger_states_observed &&
            std::none_of(evidence.observed_scene_triggered_states.begin(),
                         evidence.observed_scene_triggered_states.end(),
                         [](bool is_triggered) { return is_triggered; });
        if (!song.at("scale").is_null()) {
            const auto& scale = song.at("scale");
            evidence.observed_scale_root_note = scale.at("root_note").get<int>();
            evidence.observed_scale_name = scale.at("name").get<std::string>();
            evidence.observed_scale_intervals = scale.at("intervals").get<std::vector<int>>();
            evidence.observed_scale_mode = scale.at("mode").get<bool>();
        }
        if (!song.at("tuning_system").is_null()) {
            const auto& tuning = song.at("tuning_system");
            evidence.observed_tuning_name = tuning.at("name").get<std::string>();
            evidence.observed_pseudo_octave_in_cents =
                tuning.at("pseudo_octave_in_cents").get<double>();
            evidence.observed_tuning_lowest_note = tuning.at("lowest_note");
            evidence.observed_tuning_highest_note = tuning.at("highest_note");
            evidence.observed_tuning_reference_pitch = tuning.at("reference_pitch");
            evidence.observed_tuning_note_tunings = tuning.at("note_tunings");
            evidence.tuning_dictionary_payloads_observed = true;
        }
        if (!song.at("scenes").empty()) {
            const auto& scene = song.at("scenes").at(0);
            evidence.observed_scene_name = scene.at("name").get<std::string>();
            evidence.observed_scene_tempo_enabled = scene.at("tempo_enabled").get<bool>();
            evidence.observed_scene_signature_enabled =
                scene.at("time_signature_enabled").get<bool>();
            evidence.verified =
                equivalent_float(evidence.requested_tempo, *evidence.observed_tempo) &&
                evidence.requested_signature_numerator == *evidence.observed_signature_numerator &&
                evidence.requested_signature_denominator ==
                    *evidence.observed_signature_denominator &&
                evidence.requested_scene_name == *evidence.observed_scene_name &&
                evidence.requested_scene_tempo_enabled == *evidence.observed_scene_tempo_enabled &&
                evidence.requested_scene_signature_enabled ==
                    *evidence.observed_scene_signature_enabled &&
                evidence.recording_modes_quiescent_verified &&
                evidence.public_tempo_controls_quiescent_verified &&
                evidence.arrangement_playback_aligned_verified &&
                evidence.automation_overrides_quiescent_verified &&
                evidence.arrangement_loop_disabled_verified &&
                evidence.metronome_disabled_verified &&
                evidence.all_scene_launches_quiescent_verified;
        }
        if (evidence.verified) {
            result.song_states_verified = 1;
        } else {
            result.warnings.push_back(
                "Post-deployment Song/Scene state does not prove the requested tempo, meter, row "
                "name, disabled launch overrides, quiescent recording modes, quiescent public "
                "tempo controls, Arrangement playback/looping, automation-override state, "
                "disabled metronome, and absence of pending Scene launches");
        }
        if (result.notes_requested != 0 && validated->target_profile.live_version.at_least(12, 0))
            result.warnings.push_back(
                "Final pitch context retains version-available Song scale state and exact opaque "
                "TuningSystem dictionaries, but their Score semantics, per-track bypass, "
                "instrument/MPE support, and audible pitch remain unverified");
    } else if (result.song_states_requested != 0) {
        return std::unexpected(ErrorCode::ProtocolError);
    }
    for (auto& evidence : result.track_gates) {
        if (evidence.requested_has_audio_input || !evidence.requested_has_midi_input ||
            evidence.requested_is_frozen || evidence.requested_arm ||
            evidence.requested_implicit_arm || evidence.requested_back_to_arranger ||
            evidence.requested_fired_slot_index != -1 ||
            evidence.requested_playing_slot_index != -1 ||
            evidence.requested_clip_slot_is_group_slot ||
            evidence.requested_clip_slot_controls_other_clips ||
            evidence.requested_clip_slot_is_playing || evidence.requested_clip_slot_is_recording ||
            evidence.requested_clip_slot_is_triggered ||
            evidence.requested_clip_slot_playing_status != 0 ||
            evidence.requested_clip_slot_will_record_on_start ||
            evidence.requested_arrangement_clip_count != 0 ||
            evidence.requested_take_lane_count != 0 ||
            evidence.requested_group_track_index.has_value() ||
            evidence.requested_crossfade_assign != 1 || evidence.requested_panning_mode != 0 ||
            evidence.requested_track_activator != (evidence.requested_mute ? 0.0 : 1.0) ||
            !valid_unobserved_input_routing_intent(evidence.input_routing) ||
            !valid_unobserved_track_meter_intent(evidence) ||
            !valid_unobserved_output_routing_intent(evidence.output_routing) ||
            evidence.observed_arrangement_clip_count || evidence.observed_take_lane_count ||
            !evidence.observed_clip_slots.empty() || evidence.clip_slot_states_observed ||
            evidence.clip_slot_non_group_semantics_verified ||
            evidence.clip_slot_playback_idle_verified ||
            evidence.clip_slot_recording_quiescence_verified ||
            evidence.clip_slot_launch_quiescence_verified ||
            evidence.clip_slot_runtime_quiescence_verified ||
            evidence.arrangement_clip_topology_observed ||
            evidence.arrangement_content_absent_verified || evidence.take_lane_topology_observed ||
            evidence.take_lanes_absent_verified || evidence.monitoring_state_observed ||
            evidence.clip_output_not_suppressed_by_monitoring_verified)
            return std::unexpected(ErrorCode::ProtocolError);
        const auto target_index = static_cast<std::size_t>(evidence.track_index);
        if (target_index < tracks.size()) {
            const auto& observed = tracks.at(target_index);
            evidence.observed_name = observed.at("name").get<std::string>();
            evidence.observed_has_audio_input = observed.at("has_audio_input").get<bool>();
            evidence.observed_has_midi_input = observed.at("has_midi_input").get<bool>();
            observe_input_routing(evidence.input_routing, observed);
            if (!observed.at("input_meter_level").is_null()) {
                evidence.observed_input_meter_level =
                    observed.at("input_meter_level").get<double>();
                evidence.observed_output_meter_level =
                    observed.at("output_meter_level").get<double>();
                evidence.meter_levels_observed = true;
            }
            evidence.observed_has_audio_output = observed.at("has_audio_output").get<bool>();
            evidence.observed_has_midi_output = observed.at("has_midi_output").get<bool>();
            if (!observed.at("input_meter_left").is_null()) {
                evidence.observed_input_meter_left = observed.at("input_meter_left").get<double>();
                evidence.observed_input_meter_right =
                    observed.at("input_meter_right").get<double>();
                evidence.observed_output_meter_left =
                    observed.at("output_meter_left").get<double>();
                evidence.observed_output_meter_right =
                    observed.at("output_meter_right").get<double>();
                evidence.momentary_meter_levels_observed = true;
            }
            observe_output_routing(evidence.output_routing, observed);
            evidence.observed_is_frozen = observed.at("is_frozen").get<bool>();
            evidence.observed_arm = observed.at("arm").get<bool>();
            evidence.observed_implicit_arm = observed.at("implicit_arm").get<bool>();
            evidence.observed_back_to_arranger = observed.at("back_to_arranger").get<bool>();
            evidence.observed_fired_slot_index = observed.at("fired_slot_index").get<int>();
            evidence.observed_playing_slot_index = observed.at("playing_slot_index").get<int>();
            if (!observed.at("arrangement_clip_count").is_null()) {
                evidence.observed_arrangement_clip_count =
                    observed.at("arrangement_clip_count").get<std::uint32_t>();
                evidence.arrangement_clip_topology_observed = true;
            }
            if (!observed.at("take_lane_count").is_null()) {
                evidence.observed_take_lane_count =
                    observed.at("take_lane_count").get<std::uint32_t>();
                evidence.take_lane_topology_observed = true;
            }
            const auto& clip_slots = observed.at("clip_slots");
            evidence.observed_clip_slots.reserve(clip_slots.size());
            for (const auto& slot : clip_slots) {
                evidence.observed_clip_slots.push_back(
                    {slot.at("slot").get<std::uint32_t>(),
                     slot.at("has_clip").get<bool>(),
                     slot.at("has_stop_button").get<bool>(),
                     slot.at("is_group_slot").get<bool>(),
                     slot.at("controls_other_clips").get<bool>(),
                     slot.at("is_playing").get<bool>(),
                     slot.at("is_recording").get<bool>(),
                     slot.at("is_triggered").get<bool>(),
                     slot.at("playing_status").get<std::uint8_t>(),
                     slot.at("will_record_on_start").get<bool>()});
            }
            evidence.clip_slot_states_observed =
                evidence.observed_clip_slots.size() ==
                observed.at("clip_slot_count").get<std::uint32_t>();
            evidence.group_membership_observed = true;
            if (!observed.at("group_track_index").is_null())
                evidence.observed_group_track_index =
                    observed.at("group_track_index").get<std::uint32_t>();
            evidence.observed_crossfade_assign =
                observed.at("mixer").at("crossfade_assign").get<int>();
            evidence.observed_panning_mode = observed.at("mixer").at("panning_mode").get<int>();
            evidence.observed_mute = observed.at("mute").get<bool>();
            evidence.observed_solo = observed.at("solo").get<bool>();
            evidence.observed_muted_via_solo = observed.at("muted_via_solo").get<bool>();
            const auto& activator = observed.at("mixer").at("track_activator");
            evidence.observed_track_activator = activator.at("value").get<double>();
            evidence.observed_track_activator_minimum = activator.at("minimum").get<double>();
            evidence.observed_track_activator_maximum = activator.at("maximum").get<double>();
            evidence.observed_track_activator_is_quantized =
                activator.at("is_quantized").get<bool>();
            evidence.observed_track_activator_is_enabled = activator.at("is_enabled").get<bool>();
            evidence.observed_track_activator_state = activator.at("state").get<std::uint8_t>();
            evidence.observed_track_activator_automation_state =
                activator.at("automation_state").get<std::uint8_t>();
            evidence.input_classification_verified =
                *evidence.observed_has_audio_input == evidence.requested_has_audio_input &&
                *evidence.observed_has_midi_input == evidence.requested_has_midi_input;
            evidence.input_meter_hold_quiescent_verified =
                evidence.observed_input_meter_level &&
                *evidence.observed_input_meter_level == evidence.requested_input_meter_level;
            evidence.output_meter_hold_quiescent_verified =
                evidence.observed_output_meter_level &&
                *evidence.observed_output_meter_level == evidence.requested_output_meter_level;
            evidence.meter_hold_quiescence_verified =
                evidence.input_meter_hold_quiescent_verified &&
                evidence.output_meter_hold_quiescent_verified;
            evidence.input_stereo_momentary_quiescent_verified =
                evidence.observed_input_meter_left && evidence.observed_input_meter_right &&
                *evidence.observed_input_meter_left == evidence.requested_input_meter_left &&
                *evidence.observed_input_meter_right == evidence.requested_input_meter_right;
            evidence.output_stereo_momentary_quiescent_verified =
                evidence.observed_output_meter_left && evidence.observed_output_meter_right &&
                *evidence.observed_output_meter_left == evidence.requested_output_meter_left &&
                *evidence.observed_output_meter_right == evidence.requested_output_meter_right;
            evidence.meter_momentary_quiescence_verified =
                evidence.input_stereo_momentary_quiescent_verified &&
                evidence.output_stereo_momentary_quiescent_verified;
            evidence.meter_quiescence_verified = evidence.meter_hold_quiescence_verified &&
                                                 evidence.meter_momentary_quiescence_verified;
            evidence.unfrozen_verified =
                *evidence.observed_is_frozen == evidence.requested_is_frozen;
            evidence.disarmed_verified = *evidence.observed_arm == evidence.requested_arm;
            evidence.implicitly_disarmed_verified =
                *evidence.observed_implicit_arm == evidence.requested_implicit_arm;
            evidence.session_launch_quiescent_verified =
                *evidence.observed_fired_slot_index == evidence.requested_fired_slot_index;
            evidence.track_arrangement_playback_aligned_verified =
                *evidence.observed_back_to_arranger == evidence.requested_back_to_arranger &&
                *evidence.observed_playing_slot_index == evidence.requested_playing_slot_index;
            evidence.clip_slot_non_group_semantics_verified =
                evidence.clip_slot_states_observed &&
                std::all_of(
                    evidence.observed_clip_slots.begin(),
                    evidence.observed_clip_slots.end(),
                    [&](const auto& slot) {
                        return slot.is_group_slot == evidence.requested_clip_slot_is_group_slot &&
                               slot.controls_other_clips ==
                                   evidence.requested_clip_slot_controls_other_clips &&
                               slot.playing_status == evidence.requested_clip_slot_playing_status;
                    });
            evidence.clip_slot_playback_idle_verified =
                evidence.clip_slot_states_observed &&
                std::all_of(evidence.observed_clip_slots.begin(),
                            evidence.observed_clip_slots.end(),
                            [&](const auto& slot) {
                                return slot.is_playing == evidence.requested_clip_slot_is_playing;
                            });
            evidence.clip_slot_recording_quiescence_verified =
                evidence.clip_slot_states_observed &&
                std::all_of(evidence.observed_clip_slots.begin(),
                            evidence.observed_clip_slots.end(),
                            [&](const auto& slot) {
                                return slot.is_recording ==
                                           evidence.requested_clip_slot_is_recording &&
                                       slot.will_record_on_start ==
                                           evidence.requested_clip_slot_will_record_on_start;
                            });
            evidence.clip_slot_launch_quiescence_verified =
                evidence.clip_slot_states_observed &&
                std::all_of(evidence.observed_clip_slots.begin(),
                            evidence.observed_clip_slots.end(),
                            [&](const auto& slot) {
                                return slot.is_triggered ==
                                       evidence.requested_clip_slot_is_triggered;
                            });
            evidence.clip_slot_runtime_quiescence_verified =
                evidence.clip_slot_non_group_semantics_verified &&
                evidence.clip_slot_playback_idle_verified &&
                evidence.clip_slot_recording_quiescence_verified &&
                evidence.clip_slot_launch_quiescence_verified;
            evidence.arrangement_content_absent_verified =
                evidence.arrangement_clip_topology_observed &&
                *evidence.observed_arrangement_clip_count ==
                    evidence.requested_arrangement_clip_count;
            evidence.take_lanes_absent_verified =
                evidence.take_lane_topology_observed &&
                *evidence.observed_take_lane_count == evidence.requested_take_lane_count;
            evidence.ungrouped_verified = !evidence.observed_group_track_index.has_value();
            evidence.crossfade_neutral_verified =
                *evidence.observed_crossfade_assign == evidence.requested_crossfade_assign;
            evidence.stereo_panning_verified =
                *evidence.observed_panning_mode == evidence.requested_panning_mode;
            evidence.track_activator_range_verified =
                *evidence.observed_track_activator_minimum <= evidence.requested_track_activator &&
                evidence.requested_track_activator <= *evidence.observed_track_activator_maximum;
            evidence.track_activator_quantization_verified =
                *evidence.observed_track_activator_is_quantized;
            evidence.track_activator_verified =
                equivalent_float(*evidence.observed_track_activator,
                                 evidence.requested_track_activator) &&
                evidence.track_activator_range_verified &&
                evidence.track_activator_quantization_verified &&
                *evidence.observed_track_activator_is_enabled &&
                *evidence.observed_track_activator_state == 0 &&
                *evidence.observed_track_activator_automation_state == 0;
            evidence.verified =
                *evidence.observed_name == evidence.requested_name &&
                evidence.input_classification_verified &&
                evidence.input_routing.selected_input_available_verified &&
                evidence.meter_quiescence_verified && *evidence.observed_has_audio_output &&
                !*evidence.observed_has_midi_output && evidence.unfrozen_verified &&
                evidence.disarmed_verified && evidence.implicitly_disarmed_verified &&
                evidence.session_launch_quiescent_verified &&
                evidence.track_arrangement_playback_aligned_verified &&
                evidence.clip_slot_runtime_quiescence_verified &&
                evidence.arrangement_content_absent_verified &&
                evidence.take_lanes_absent_verified && evidence.ungrouped_verified &&
                evidence.crossfade_neutral_verified && evidence.stereo_panning_verified &&
                evidence.track_activator_verified &&
                *evidence.observed_mute == evidence.requested_mute &&
                *evidence.observed_solo == evidence.requested_solo &&
                (!evidence.expected_mixer_enabled || !*evidence.observed_muted_via_solo);
        }

        if (evidence.verified) {
            ++result.track_gates_verified;
        } else {
            result.warnings.push_back(
                "Post-deployment Track " + std::to_string(evidence.track_index) +
                " does not prove the requested Part name, MIDI-input/audio-output role, Live-valid "
                "input selection, zero one-second hold peaks and stereo momentary meters, "
                "disarmed, "
                "unfrozen/ungrouped state, quiescent Track and ClipSlot launch/record state, "
                "no Arrangement Clips or Take Lanes, Arrangement-aligned Track playback, neutral "
                "crossfade "
                "assignment, Stereo Pan "
                "mode, active/unautomated Track Activator, and mixer gate state");
        }
        if (evidence.output_routing.requested_binding && !evidence.output_routing.verified)
            result.warnings.push_back(
                "Post-deployment Track " + std::to_string(evidence.track_index) +
                " does not retain the exact semantically admitted output type/channel binding");
    }

    const auto& return_tracks = song.at("return_tracks");
    for (auto& evidence : result.return_track_gates) {
        if (evidence.track_index < 0 || evidence.requested_mute || evidence.requested_solo ||
            evidence.requested_crossfade_assign != 1 || evidence.requested_panning_mode != 0 ||
            evidence.requested_track_activator != 1.0 || !evidence.expected_mixer_enabled ||
            !valid_unobserved_output_routing_intent(evidence.output_routing))
            return std::unexpected(ErrorCode::ProtocolError);

        const auto target_index = static_cast<std::size_t>(evidence.track_index);
        if (target_index < return_tracks.size()) {
            const auto& observed = return_tracks.at(target_index);
            evidence.observed_name = observed.at("name").get<std::string>();
            observe_output_routing(evidence.output_routing, observed);
            evidence.observed_mute = observed.at("mute").get<bool>();
            evidence.observed_solo = observed.at("solo").get<bool>();
            evidence.observed_muted_via_solo = observed.at("muted_via_solo").get<bool>();
            evidence.observed_crossfade_assign =
                observed.at("mixer").at("crossfade_assign").get<int>();
            evidence.observed_panning_mode = observed.at("mixer").at("panning_mode").get<int>();
            const auto& activator = observed.at("mixer").at("track_activator");
            evidence.observed_track_activator = activator.at("value").get<double>();
            evidence.observed_track_activator_minimum = activator.at("minimum").get<double>();
            evidence.observed_track_activator_maximum = activator.at("maximum").get<double>();
            evidence.observed_track_activator_is_quantized =
                activator.at("is_quantized").get<bool>();
            evidence.observed_track_activator_is_enabled = activator.at("is_enabled").get<bool>();
            evidence.observed_track_activator_state = activator.at("state").get<std::uint8_t>();
            evidence.observed_track_activator_automation_state =
                activator.at("automation_state").get<std::uint8_t>();
            evidence.crossfade_neutral_verified =
                *evidence.observed_crossfade_assign == evidence.requested_crossfade_assign;
            evidence.stereo_panning_verified =
                *evidence.observed_panning_mode == evidence.requested_panning_mode;
            evidence.track_activator_range_verified =
                *evidence.observed_track_activator_minimum <= evidence.requested_track_activator &&
                evidence.requested_track_activator <= *evidence.observed_track_activator_maximum;
            evidence.track_activator_quantization_verified =
                *evidence.observed_track_activator_is_quantized;
            evidence.track_activator_verified =
                equivalent_float(*evidence.observed_track_activator,
                                 evidence.requested_track_activator) &&
                evidence.track_activator_range_verified &&
                evidence.track_activator_quantization_verified &&
                *evidence.observed_track_activator_is_enabled &&
                *evidence.observed_track_activator_state == 0 &&
                *evidence.observed_track_activator_automation_state == 0;
            evidence.verified =
                *evidence.observed_name == evidence.requested_name &&
                *evidence.observed_mute == evidence.requested_mute &&
                *evidence.observed_solo == evidence.requested_solo &&
                !*evidence.observed_muted_via_solo && evidence.crossfade_neutral_verified &&
                evidence.stereo_panning_verified && evidence.track_activator_verified;
        }

        if (evidence.verified) {
            ++result.return_track_gates_verified;
        } else {
            result.warnings.push_back(
                "Post-deployment Return Track " + std::to_string(evidence.track_index) +
                " does not prove the requested Aux name, enabled mute/solo gate, freedom from "
                "external solo suppression, neutral crossfade assignment, Stereo Pan mode, and "
                "active/unautomated Track Activator");
        }
        if (evidence.output_routing.requested_binding && !evidence.output_routing.verified)
            result.warnings.push_back(
                "Post-deployment Return Track " + std::to_string(evidence.track_index) +
                " does not retain the exact semantically admitted output type/channel binding");
    }

    if (result.master_track_gates_requested != 1 || !result.master_track_gate)
        return std::unexpected(ErrorCode::ProtocolError);
    {
        auto& evidence = *result.master_track_gate;
        if (evidence.requested_track_activator != 1.0 || evidence.requested_panning_mode != 0 ||
            evidence.requested_pan != 0.0)
            return std::unexpected(ErrorCode::ProtocolError);
        const auto& mixer = song.at("master_track").at("mixer");
        const auto& activator = mixer.at("track_activator");
        const auto& panning = mixer.at("panning");
        evidence.observed_track_activator = activator.at("value").get<double>();
        evidence.observed_track_activator_minimum = activator.at("minimum").get<double>();
        evidence.observed_track_activator_maximum = activator.at("maximum").get<double>();
        evidence.observed_track_activator_is_quantized = activator.at("is_quantized").get<bool>();
        evidence.observed_track_activator_is_enabled = activator.at("is_enabled").get<bool>();
        evidence.observed_track_activator_state = activator.at("state").get<std::uint8_t>();
        evidence.observed_track_activator_automation_state =
            activator.at("automation_state").get<std::uint8_t>();
        evidence.observed_panning_mode = mixer.at("panning_mode").get<int>();
        evidence.observed_pan = panning.at("value").get<double>();
        evidence.observed_pan_minimum = panning.at("minimum").get<double>();
        evidence.observed_pan_maximum = panning.at("maximum").get<double>();
        evidence.observed_pan_is_quantized = panning.at("is_quantized").get<bool>();
        evidence.observed_pan_is_enabled = panning.at("is_enabled").get<bool>();
        evidence.observed_pan_state = panning.at("state").get<std::uint8_t>();
        evidence.observed_pan_automation_state = panning.at("automation_state").get<std::uint8_t>();
        evidence.track_activator_range_verified =
            *evidence.observed_track_activator_minimum <= evidence.requested_track_activator &&
            evidence.requested_track_activator <= *evidence.observed_track_activator_maximum;
        evidence.track_activator_quantization_verified =
            *evidence.observed_track_activator_is_quantized;
        evidence.track_activator_verified =
            equivalent_float(*evidence.observed_track_activator,
                             evidence.requested_track_activator) &&
            evidence.track_activator_range_verified &&
            evidence.track_activator_quantization_verified &&
            *evidence.observed_track_activator_is_enabled &&
            *evidence.observed_track_activator_state == 0 &&
            *evidence.observed_track_activator_automation_state == 0;
        evidence.stereo_panning_verified =
            *evidence.observed_panning_mode == evidence.requested_panning_mode;
        evidence.pan_range_verified = *evidence.observed_pan_minimum <= evidence.requested_pan &&
                                      evidence.requested_pan <= *evidence.observed_pan_maximum;
        evidence.pan_quantization_verified = !*evidence.observed_pan_is_quantized;
        evidence.neutral_pan_verified =
            equivalent_float(*evidence.observed_pan, evidence.requested_pan) &&
            evidence.pan_range_verified && evidence.pan_quantization_verified &&
            *evidence.observed_pan_is_enabled && *evidence.observed_pan_state == 0 &&
            *evidence.observed_pan_automation_state == 0;
        evidence.verified = evidence.track_activator_verified && evidence.stereo_panning_verified &&
                            evidence.neutral_pan_verified;
        if (evidence.verified) {
            result.master_track_gates_verified = 1;
        } else {
            result.warnings.push_back(
                "Post-deployment Main Track does not prove an enabled, active/unautomated output "
                "activator and centered active/unautomated Stereo Pan stage");
        }
    }

    for (auto& evidence : result.devices) {
        if (!valid_device_intent(evidence)) return std::unexpected(ErrorCode::ProtocolError);
        const auto path = LomPath::parse(evidence.device_path);
        const auto* chain = observed_device_chain(song, path);
        if (chain != nullptr && chain->size() <= std::numeric_limits<std::uint32_t>::max()) {
            evidence.observed_chain_size = static_cast<std::uint32_t>(chain->size());
            if (evidence.requested_index < chain->size()) {
                const auto& observed = chain->at(evidence.requested_index);
                evidence.observed_name = observed.at("name").get<std::string>();
                evidence.observed_class_display_name =
                    observed.at("class_display_name").get<std::string>();
                evidence.observed_class_name = observed.at("class_name").get<std::string>();
                evidence.observed_type = observed.at("type").get<std::uint8_t>();
                evidence.observed_active = observed.at("is_active").get<bool>();
                evidence.observed_can_have_chains = observed.at("can_have_chains").get<bool>();
                evidence.observed_latency_in_samples =
                    observed.at("latency_in_samples").get<std::uint32_t>();
                evidence.observed_latency_in_ms = observed.at("latency_in_ms").get<double>();
                evidence.reported_latency_observed = true;
                evidence.flat_device_verified = !*evidence.observed_can_have_chains;
                evidence.verified =
                    *evidence.observed_chain_size == evidence.expected_chain_size &&
                    *evidence.observed_class_display_name == evidence.requested_name &&
                    *evidence.observed_type == evidence.requested_type &&
                    *evidence.observed_active && evidence.flat_device_verified &&
                    evidence.reported_latency_observed;
            }
        }

        if (evidence.verified) {
            ++result.devices_verified;
        } else {
            result.warnings.push_back("Post-deployment device '" + evidence.device_path +
                                      "' does not prove the requested exact chain size, display "
                                      "identity, type, non-Rack shape, active state, and bounded "
                                      "latency report");
        }
    }

    for (auto& evidence : result.clips) {
        if (evidence.track_index < 0 || evidence.slot_index != 0 ||
            evidence.requested_occupied_slot_indices != std::vector<std::uint32_t>{0} ||
            !std::isfinite(evidence.requested_length) || evidence.requested_length <= 0.0 ||
            !std::isfinite(evidence.requested_start_marker) ||
            !std::isfinite(evidence.requested_end_marker) ||
            evidence.requested_end_marker <= evidence.requested_start_marker ||
            !std::isfinite(evidence.requested_end_time) ||
            !equivalent_float(evidence.requested_end_time, evidence.requested_end_marker) ||
            !equivalent_float(evidence.requested_length,
                              evidence.requested_end_marker - evidence.requested_start_marker) ||
            evidence.requested_looping || evidence.requested_muted ||
            evidence.requested_has_groove || evidence.requested_is_audio_clip ||
            !evidence.requested_is_midi_clip || evidence.requested_is_arrangement_clip ||
            !evidence.requested_is_session_clip || evidence.requested_is_take_lane_clip ||
            evidence.requested_has_envelopes || evidence.requested_is_playing ||
            evidence.requested_is_recording || evidence.requested_is_overdubbing ||
            evidence.requested_is_triggered || evidence.requested_will_record_on_start ||
            evidence.location_identity_available != evidence.groove_state_available ||
            evidence.launch_state_available != evidence.groove_state_available ||
            evidence.requested_launch_mode != 0 || evidence.requested_launch_quantization != 1 ||
            evidence.requested_legato || !std::isfinite(evidence.requested_velocity_amount) ||
            evidence.requested_velocity_amount != 0.0 || evidence.follow_actions_observed ||
            evidence.one_shot_playback_verified || evidence.midi_bank_program_state_observed ||
            evidence.program_change_suppression_verified ||
            evidence.mpe_note_expression_state_observed ||
            evidence.mpe_note_expression_neutrality_verified)
            return std::unexpected(ErrorCode::ProtocolError);

        const auto track_index = static_cast<std::size_t>(evidence.track_index);
        if (track_index < tracks.size()) {
            const auto& observed_clips = tracks.at(track_index).at("clips");
            for (const auto& observed_clip : observed_clips)
                evidence.observed_occupied_slot_indices.push_back(
                    observed_clip.at("slot").get<std::uint32_t>());
            std::sort(evidence.observed_occupied_slot_indices.begin(),
                      evidence.observed_occupied_slot_indices.end());
            evidence.exact_slot_occupancy_verified =
                evidence.observed_occupied_slot_indices == evidence.requested_occupied_slot_indices;
            const auto observed = std::find_if(
                observed_clips.begin(), observed_clips.end(), [&](const auto& candidate) {
                    return candidate.at("slot").template get<std::uint32_t>() ==
                           evidence.slot_index;
                });
            if (observed != observed_clips.end()) {
                evidence.observed_name = observed->at("name").get<std::string>();
                evidence.observed_is_audio_clip = observed->at("is_audio_clip").get<bool>();
                evidence.observed_is_midi_clip = observed->at("is_midi_clip").get<bool>();
                evidence.observed_is_arrangement_clip =
                    observed->at("is_arrangement_clip").get<bool>();
                if (!observed->at("is_session_clip").is_null())
                    evidence.observed_is_session_clip = observed->at("is_session_clip").get<bool>();
                if (!observed->at("is_take_lane_clip").is_null())
                    evidence.observed_is_take_lane_clip =
                        observed->at("is_take_lane_clip").get<bool>();
                evidence.clip_identity_verified =
                    evidence.observed_is_audio_clip &&
                    *evidence.observed_is_audio_clip == evidence.requested_is_audio_clip &&
                    evidence.observed_is_midi_clip &&
                    *evidence.observed_is_midi_clip == evidence.requested_is_midi_clip &&
                    evidence.observed_is_arrangement_clip &&
                    *evidence.observed_is_arrangement_clip ==
                        evidence.requested_is_arrangement_clip &&
                    (!evidence.location_identity_available ||
                     (evidence.observed_is_session_clip &&
                      *evidence.observed_is_session_clip == evidence.requested_is_session_clip &&
                      evidence.observed_is_take_lane_clip &&
                      *evidence.observed_is_take_lane_clip ==
                          evidence.requested_is_take_lane_clip));
                evidence.observed_length = observed->at("length").get<double>();
                evidence.observed_signature_numerator =
                    observed->at("signature_numerator").get<int>();
                evidence.observed_signature_denominator =
                    observed->at("signature_denominator").get<int>();
                evidence.observed_start_marker = observed->at("start_marker").get<double>();
                evidence.observed_end_marker = observed->at("end_marker").get<double>();
                evidence.observed_end_time = observed->at("end_time").get<double>();
                evidence.playback_end_verified =
                    equivalent_float(evidence.requested_end_time, *evidence.observed_end_time);
                evidence.observed_looping = observed->at("looping").get<bool>();
                evidence.observed_muted = observed->at("muted").get<bool>();
                evidence.observed_has_envelopes = observed->at("has_envelopes").get<bool>();
                evidence.envelope_absence_verified =
                    evidence.observed_has_envelopes && !*evidence.observed_has_envelopes;
                evidence.observed_is_playing = observed->at("is_playing").get<bool>();
                evidence.observed_is_recording = observed->at("is_recording").get<bool>();
                evidence.observed_is_overdubbing = observed->at("is_overdubbing").get<bool>();
                evidence.observed_is_triggered = observed->at("is_triggered").get<bool>();
                evidence.observed_will_record_on_start =
                    observed->at("will_record_on_start").get<bool>();
                evidence.runtime_state_observed = true;
                evidence.recording_quiescence_verified = !*evidence.observed_is_recording &&
                                                         !*evidence.observed_is_overdubbing &&
                                                         !*evidence.observed_will_record_on_start;
                evidence.playback_idle_verified =
                    !*evidence.observed_is_playing && !*evidence.observed_is_triggered;
                if (!observed->at("has_groove").is_null())
                    evidence.observed_has_groove = observed->at("has_groove").get<bool>();
                if (!observed->at("launch_mode").is_null())
                    evidence.observed_launch_mode = observed->at("launch_mode").get<int>();
                if (!observed->at("launch_quantization").is_null())
                    evidence.observed_launch_quantization =
                        observed->at("launch_quantization").get<int>();
                if (!observed->at("legato").is_null())
                    evidence.observed_legato = observed->at("legato").get<bool>();
                if (!observed->at("velocity_amount").is_null())
                    evidence.observed_velocity_amount =
                        observed->at("velocity_amount").get<double>();
                evidence.groove_absence_verified =
                    !evidence.groove_state_available ||
                    (evidence.observed_has_groove && !*evidence.observed_has_groove);
                evidence.launch_behavior_verified =
                    evidence.launch_state_available && evidence.observed_launch_mode &&
                    *evidence.observed_launch_mode == evidence.requested_launch_mode &&
                    evidence.observed_launch_quantization &&
                    *evidence.observed_launch_quantization ==
                        evidence.requested_launch_quantization &&
                    evidence.observed_legato &&
                    *evidence.observed_legato == evidence.requested_legato &&
                    evidence.observed_velocity_amount &&
                    equivalent_float(evidence.requested_velocity_amount,
                                     *evidence.observed_velocity_amount);
                evidence.verified =
                    *evidence.observed_name == evidence.requested_name &&
                    evidence.exact_slot_occupancy_verified && evidence.clip_identity_verified &&
                    equivalent_float(evidence.requested_length, *evidence.observed_length) &&
                    *evidence.observed_signature_numerator ==
                        evidence.requested_signature_numerator &&
                    *evidence.observed_signature_denominator ==
                        evidence.requested_signature_denominator &&
                    equivalent_float(evidence.requested_start_marker,
                                     *evidence.observed_start_marker) &&
                    equivalent_float(evidence.requested_end_marker,
                                     *evidence.observed_end_marker) &&
                    evidence.playback_end_verified &&
                    *evidence.observed_looping == evidence.requested_looping &&
                    *evidence.observed_muted == evidence.requested_muted &&
                    evidence.envelope_absence_verified && evidence.groove_absence_verified &&
                    evidence.runtime_state_observed && evidence.recording_quiescence_verified &&
                    evidence.playback_idle_verified &&
                    (!evidence.launch_state_available || evidence.launch_behavior_verified);
            }
        }

        if (evidence.verified) {
            ++result.clips_verified;
        } else {
            result.warnings.push_back("Post-deployment Session Clip for Part " +
                                      std::to_string(evidence.part_id.value) +
                                      " does not prove the requested MIDI role, name, finite "
                                      "structural interval, meter, loop, activator, no-groove, "
                                      "no-envelope, exact Session/MIDI identity and sole-slot "
                                      "occupancy, derived playback end, bounded launch "
                                      "state, and idle public runtime state");
        }
    }

    const auto& observed_cues = song.at("cue_points");
    for (auto& evidence : result.cues) {
        const json* matched = nullptr;
        for (const auto& observed : observed_cues) {
            if (std::abs(observed.at("time").get<double>() - evidence.requested_time) >= 1.0e-7)
                continue;
            if (matched != nullptr) {
                matched = nullptr;
                break;
            }
            matched = &observed;
        }
        if (matched != nullptr) {
            evidence.observed_time = matched->at("time").get<double>();
            evidence.observed_name = matched->at("name").get<std::string>();
            evidence.verified = evidence.observed_name == evidence.requested_name;
        }
        if (evidence.verified) {
            ++result.cues_verified;
        } else {
            result.warnings.push_back("Post-deployment CuePoint at beat " +
                                      std::to_string(evidence.requested_time) +
                                      " does not uniquely prove the requested section name");
        }
    }
    for (auto& evidence : result.mixer_properties) {
        if (!valid_mixer_intent(evidence)) return std::unexpected(ErrorCode::ProtocolError);
        const auto path = LomPath::parse(evidence.parameter_path);
        const auto* observed = observed_mixer_parameter(song, path);
        if (observed != nullptr) {
            evidence.observed_value = observed->at(evidence.value_property).get<double>();
            evidence.observed_minimum = observed->at("minimum").get<double>();
            evidence.observed_maximum = observed->at("maximum").get<double>();
            evidence.observed_is_quantized = observed->at("is_quantized").get<bool>();
            if (*evidence.observed_is_quantized) {
                evidence.observed_value_items =
                    observed->at("value_items").get<std::vector<std::string>>();
            } else {
                evidence.observed_default_value = observed->at("default_value").get<double>();
            }
            evidence.observed_state = observed->at("state").get<std::uint8_t>();
            evidence.observed_automation_state =
                observed->at("automation_state").get<std::uint8_t>();
            evidence.observed_is_enabled = observed->at("is_enabled").get<bool>();
            if (evidence.value_property == "value")
                evidence.range_verified = *evidence.observed_minimum <= evidence.requested_value &&
                                          evidence.requested_value <= *evidence.observed_maximum;
            evidence.quantization_verified =
                *evidence.observed_is_quantized == evidence.requested_is_quantized;
            evidence.verified =
                equivalent_float(evidence.requested_value, *evidence.observed_value) &&
                (!evidence.range_verified || *evidence.range_verified) &&
                evidence.quantization_verified && *evidence.observed_is_enabled &&
                *evidence.observed_state == 0 && *evidence.observed_automation_state == 0;
        }
        if (evidence.verified) {
            ++result.mixer_properties_verified;
        } else {
            result.warnings.push_back(
                "Post-deployment mixer parameter '" + evidence.parameter_path + "/" +
                evidence.value_property +
                "' does not prove the requested value/domain with enabled, active, unautomated "
                "state");
        }
    }
    return result;
}

Result<json> canonical_project_state(const ProjectView& project) {
    auto score = score_from_json(score_to_json(project.score));
    auto mix = mix_from_json(mix_to_json(project.mix));
    if (!score) return std::unexpected(score.error());
    if (!mix) return std::unexpected(mix.error());

    std::vector<const TimbreProfile*> profiles(project.timbre_profiles.begin(),
                                               project.timbre_profiles.end());
    std::sort(profiles.begin(), profiles.end(), [](const auto* lhs, const auto* rhs) {
        if (lhs == nullptr || rhs == nullptr) return lhs == nullptr && rhs != nullptr;
        if (lhs->part_id != rhs->part_id) return lhs->part_id < rhs->part_id;
        return lhs->id < rhs->id;
    });

    json timbre = json::array();
    for (const auto* profile : profiles) {
        if (profile == nullptr) return std::unexpected(ErrorCode::ProjectMissingComponent);
        auto normalized = timbre_from_json(timbre_to_json(*profile));
        if (!normalized) return std::unexpected(normalized.error());
        timbre.push_back(timbre_to_json(*normalized));
    }
    return json{{"score", score_to_json(*score)},
                {"timbre", std::move(timbre)},
                {"mix", mix_to_json(*mix)}};
}

json canonical_output_routing_binding_state(const AbletonOutputRoutingBindings& bindings) {
    const auto encode_binding = [](std::uint64_t id, const AbletonOutputRouteBinding& binding) {
        return json{{"id", id},
                    {"type",
                     {{"display_name", binding.type.display_name},
                      {"identifier", binding.type.identifier}}},
                    {"channel",
                     {{"display_name", binding.channel.display_name},
                      {"identifier", binding.channel.identifier}}},
                    {"mapping_provenance", binding.mapping_provenance}};
    };

    json part_tracks = json::array();
    for (const auto& [id, binding] : bindings.part_tracks)
        part_tracks.push_back(encode_binding(id.value, binding));
    json aux_returns = json::array();
    for (const auto& [id, binding] : bindings.aux_returns)
        aux_returns.push_back(encode_binding(id.value, binding));
    return json{{"part_tracks", std::move(part_tracks)}, {"aux_returns", std::move(aux_returns)}};
}

Result<AbletonProjectCompilationResult>
compile_project_impl(const ProjectView& project,
                     JournaledLomTransport& transport,
                     int ppq,
                     const AbletonOutputRoutingBindings& output_routing_bindings) {
    AbletonProjectCompilationResult result;
    result.diagnostics = validate_project(project);
    if (has_error(result.diagnostics)) return std::unexpected(ErrorCode::ProjectValidationFailed);

    auto tracks = derive_part_tracks(project.score);
    if (!tracks) return std::unexpected(tracks.error());
    result.part_tracks = std::move(*tracks);
    result.output_routing_bindings = output_routing_bindings;

    transport.set_phase(AbletonDeploymentPhase::Score);
    auto score = compile_to_ableton(project.score, transport, ppq);
    if (!score) return std::unexpected(score.error());
    result.score = std::move(*score);

    transport.set_phase(AbletonDeploymentPhase::Timbre);
    result.timbre.reserve(project.score.parts.size());
    for (const auto& part : project.score.parts) {
        const auto profile = std::find_if(
            project.timbre_profiles.begin(), project.timbre_profiles.end(), [&](const auto* item) {
                return item != nullptr && item->part_id == part.id;
            });
        if (profile == project.timbre_profiles.end())
            return std::unexpected(ErrorCode::ProjectMissingComponent);

        const auto track_index = result.part_tracks.at(part.id);
        auto timbre = compile_timbre_to_ableton(**profile, track_index, transport);
        if (!timbre) return std::unexpected(timbre.error());
        result.timbre.push_back({part.id, track_index, std::move(*timbre)});
    }

    transport.set_phase(AbletonDeploymentPhase::Mix);
    auto mix =
        compile_mix_to_ableton(project.mix, result.part_tracks, transport, output_routing_bindings);
    if (!mix) return std::unexpected(mix.error());
    result.mix = std::move(*mix);
    auto postconditions = project_postcondition_intent(project, result.part_tracks);
    if (!postconditions) return std::unexpected(postconditions.error());
    result.postconditions = std::move(*postconditions);
    append_compilation_device_intent(result.postconditions, result);
    auto return_track_postconditions =
        append_compilation_return_track_intent(result.postconditions, project, result);
    if (!return_track_postconditions) return std::unexpected(return_track_postconditions.error());
    auto output_route_postconditions =
        append_compilation_output_route_intent(result.postconditions, result);
    if (!output_route_postconditions) return std::unexpected(output_route_postconditions.error());
    auto master_track_postconditions = validate_compilation_master_track_intent(result);
    if (!master_track_postconditions) return std::unexpected(master_track_postconditions.error());
    auto clip_postconditions = append_compilation_clip_intent(
        result.postconditions, project, result.part_tracks, result.score);
    if (!clip_postconditions) return std::unexpected(clip_postconditions.error());
    auto note_postconditions =
        append_compilation_note_intent(result.postconditions, result.part_tracks, result.score);
    if (!note_postconditions) return std::unexpected(note_postconditions.error());
    auto song_postconditions =
        append_compilation_song_intent(result.postconditions, project, result.score);
    if (!song_postconditions) return std::unexpected(song_postconditions.error());
    auto mixer_postconditions = append_compilation_mixer_intent(result.postconditions, result);
    if (!mixer_postconditions) return std::unexpected(mixer_postconditions.error());
    auto parameter_postconditions =
        append_compilation_parameter_intent(result.postconditions, result);
    if (!parameter_postconditions) return std::unexpected(parameter_postconditions.error());
    return result;
}

Result<AbletonTargetSnapshot> inspect_target(LomTransport& transport) {
    auto snapshot = transport.target_snapshot();
    if (!snapshot) return std::unexpected(snapshot.error());
    if (!*snapshot) return std::unexpected(ErrorCode::TargetSnapshotUnavailable);
    return std::move(**snapshot);
}

} // namespace

bool AbletonProjectDeploymentAttempt::target_may_be_partially_modified() const {
    return std::any_of(mutation_journal.begin(), mutation_journal.end(), [](const auto& mutation) {
        return mutation.target_may_have_mutated();
    });
}

Result<AbletonProjectDeploymentPlan>
plan_project_to_ableton(const ProjectView& project,
                        const AbletonOutputRoutingBindings& output_routing_bindings,
                        LomTransport& transport,
                        int ppq) {
    if (ppq <= 0 || ppq > std::numeric_limits<std::uint16_t>::max())
        return std::unexpected(ErrorCode::InvalidMidiPPQ);

    auto diagnostics = validate_project(project);
    if (has_error(diagnostics)) return std::unexpected(ErrorCode::ProjectValidationFailed);

    auto target = inspect_target(transport);
    if (!target) return std::unexpected(target.error());

    CommandBuffer simulation;
    simulation.set_target_snapshot(*target);
    JournaledLomTransport recorder{simulation, target->target_profile};
    auto preview = compile_project_impl(project, recorder, ppq, output_routing_bindings);
    if (!preview) return std::unexpected(preview.error());

    auto part_tracks = derive_part_tracks(project.score);
    if (!part_tracks) return std::unexpected(part_tracks.error());
    AbletonProjectDeploymentPlan plan;
    plan.ppq = ppq;
    auto project_state = canonical_project_state(project);
    if (!project_state) return std::unexpected(project_state.error());
    plan.project_state = std::move(*project_state);
    plan.output_routing_binding_state =
        canonical_output_routing_binding_state(output_routing_bindings);
    plan.target_snapshot = std::move(*target);
    plan.part_tracks = std::move(*part_tracks);
    plan.output_routing_bindings = output_routing_bindings;
    plan.diagnostics = std::move(diagnostics);
    plan.preview = std::move(*preview);
    plan.mutations = planned_mutations_from_journal(recorder.journal());
    return plan;
}

Result<AbletonProjectPostconditionEvidence>
evaluate_project_ableton_postconditions(const ProjectView& project,
                                        const AbletonPartTrackMap& part_tracks,
                                        const AbletonTargetSnapshot& snapshot,
                                        const AbletonProjectCompilationResult* compilation) {
    auto intent = project_postcondition_intent(project, part_tracks);
    if (!intent) return std::unexpected(intent.error());
    if (compilation != nullptr) {
        append_compilation_device_intent(*intent, *compilation);
        auto return_tracks = append_compilation_return_track_intent(*intent, project, *compilation);
        if (!return_tracks) return std::unexpected(return_tracks.error());
        auto output_routes = append_compilation_output_route_intent(*intent, *compilation);
        if (!output_routes) return std::unexpected(output_routes.error());
        auto master_track = validate_compilation_master_track_intent(*compilation);
        if (!master_track) return std::unexpected(master_track.error());
        auto clips =
            append_compilation_clip_intent(*intent, project, part_tracks, compilation->score);
        if (!clips) return std::unexpected(clips.error());
        auto notes = append_compilation_note_intent(*intent, part_tracks, compilation->score);
        if (!notes) return std::unexpected(notes.error());
        auto song = append_compilation_song_intent(*intent, project, compilation->score);
        if (!song) return std::unexpected(song.error());
        auto mixer = append_compilation_mixer_intent(*intent, *compilation);
        if (!mixer) return std::unexpected(mixer.error());
        auto parameters = append_compilation_parameter_intent(*intent, *compilation);
        if (!parameters) return std::unexpected(parameters.error());
    }
    return evaluate_postcondition_intent(std::move(*intent), snapshot);
}

AbletonProjectDeploymentAttempt apply_project_ableton_plan(AbletonProjectDeploymentPlan& plan,
                                                           const ProjectView& project,
                                                           LomTransport& transport) {
    AbletonProjectDeploymentAttempt attempt;
    if (plan.consumed) {
        attempt.status = AbletonProjectDeploymentStatus::PlanConsumed;
        attempt.error = ErrorCode::ProjectPlanConsumed;
        return attempt;
    }
    plan.consumed = true;

    const auto diagnostics = validate_project(project);
    auto project_state = canonical_project_state(project);
    const auto output_routing_binding_state =
        canonical_output_routing_binding_state(plan.output_routing_bindings);
    if (has_error(diagnostics) || !project_state || *project_state != plan.project_state ||
        output_routing_binding_state != plan.output_routing_binding_state) {
        attempt.status = AbletonProjectDeploymentStatus::ProjectChanged;
        attempt.error = ErrorCode::ProjectPlanProjectChanged;
        return attempt;
    }

    auto current = inspect_target(transport);
    if (!current) {
        attempt.status = AbletonProjectDeploymentStatus::ApplyFailed;
        attempt.error = current.error();
        return attempt;
    }
    attempt.target_before = *current;
    if (!equivalent_target_snapshot(*current, plan.target_snapshot)) {
        attempt.status = AbletonProjectDeploymentStatus::TargetChanged;
        attempt.error = ErrorCode::ProjectPlanTargetChanged;
        return attempt;
    }

    JournaledLomTransport guarded{
        transport, plan.target_snapshot.target_profile, plan.mutations, true};
    auto compilation =
        compile_project_impl(project, guarded, plan.ppq, plan.output_routing_bindings);
    attempt.mutation_journal = guarded.journal();
    if (!compilation) {
        attempt.status = guarded.plan_diverged() ? AbletonProjectDeploymentStatus::PlanDiverged
                                                 : AbletonProjectDeploymentStatus::ApplyFailed;
        attempt.error =
            guarded.plan_diverged() ? ErrorCode::ProjectPlanDiverged : compilation.error();
    } else if (guarded.plan_diverged() || !guarded.plan_consumed()) {
        attempt.status = AbletonProjectDeploymentStatus::PlanDiverged;
        attempt.error = ErrorCode::ProjectPlanDiverged;
    } else {
        attempt.status = AbletonProjectDeploymentStatus::Completed;
        attempt.compilation = std::move(*compilation);
    }

    auto after = inspect_target(transport);
    if (after) {
        attempt.target_after = std::move(*after);
        if (attempt.status == AbletonProjectDeploymentStatus::Completed && attempt.compilation &&
            !transport.records_without_execution()) {
            auto postconditions = evaluate_project_ableton_postconditions(
                project, plan.part_tracks, *attempt.target_after, &*attempt.compilation);
            if (!postconditions) {
                attempt.status = AbletonProjectDeploymentStatus::ApplyFailed;
                attempt.error = postconditions.error();
            } else {
                attempt.compilation->postconditions = std::move(*postconditions);
                auto final_notes =
                    observe_final_notes(attempt.compilation->postconditions, transport);
                if (!final_notes) {
                    attempt.status = AbletonProjectDeploymentStatus::ApplyFailed;
                    attempt.error = final_notes.error();
                } else {
                    auto final_parameters = observe_final_device_parameters(
                        attempt.compilation->postconditions, transport);
                    if (!final_parameters) {
                        attempt.status = AbletonProjectDeploymentStatus::ApplyFailed;
                        attempt.error = final_parameters.error();
                    }
                }
            }
        }
    } else if (attempt.status == AbletonProjectDeploymentStatus::Completed) {
        attempt.status = AbletonProjectDeploymentStatus::PostSnapshotUnavailable;
        attempt.error = after.error();
    }
    return attempt;
}

Result<AbletonProjectCompilationResult>
compile_project_to_ableton(const ProjectView& project, LomTransport& transport, int ppq) {
    return compile_project_to_ableton(project, {}, transport, ppq);
}

Result<AbletonProjectDeploymentPlan>
plan_project_to_ableton(const ProjectView& project, LomTransport& transport, int ppq) {
    return plan_project_to_ableton(project, {}, transport, ppq);
}

Result<AbletonProjectCompilationResult>
compile_project_to_ableton(const ProjectView& project,
                           const AbletonOutputRoutingBindings& output_routing_bindings,
                           LomTransport& transport,
                           int ppq) {
    auto plan = plan_project_to_ableton(project, output_routing_bindings, transport, ppq);
    if (!plan) return std::unexpected(plan.error());
    auto attempt = apply_project_ableton_plan(*plan, project, transport);
    if (attempt.status != AbletonProjectDeploymentStatus::Completed || !attempt.compilation)
        return std::unexpected(attempt.error.value_or(ErrorCode::ProtocolError));
    return std::move(*attempt.compilation);
}

} // namespace sunny::infrastructure::formats
