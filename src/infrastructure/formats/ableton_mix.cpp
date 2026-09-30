/**
 * @file ableton_mix.cpp
 * @brief Ableton Live Compiler — Mix IR to mixer infrastructure
 *
 *
 * Maps MixGraph to Ableton mixer:
 *   ChannelStrip     → Track mixer (volume, pan, mute, inserts)
 *   GroupBus         → reported capability gap (LOM cannot create groups)
 *   AuxBus           → Return Track
 *   MasterBus        → Master Track effects
 *   MixAutomation    → reported capability gap (LOM cannot author envelopes)
 *
 * Device mapping (defaults to Ableton native):
 *   MixEQ            → EQ Eight
 *   MixCompressor    → Compressor
 *   MixGate          → Gate
 *   MixLimiter       → Limiter
 *   MixMultibandDynamics → Multiband Dynamics
 *   MixSaturation    → Saturator
 *   MixStereoProcessor → Utility
 *   MixDelay         → Delay
 *   MixReverb        → Reverb
 */

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <sunny/core/mix/validation.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/infrastructure/formats/ableton_mix.hpp>

namespace sunny::infrastructure::formats {

using namespace sunny::core;

const char* ableton_output_route_action_name(AbletonOutputRouteAction action) {
    switch (action) {
    case AbletonOutputRouteAction::NotApplied:
        return "not_applied";
    case AbletonOutputRouteAction::RecordedOnly:
        return "recorded_only";
    case AbletonOutputRouteAction::Set:
        return "set";
    }
    return "not_applied";
}

namespace {

/// Map MixEffect variant to Ableton device name
std::string device_for_effect(const MixEffectParameters& params) {
    return std::visit(
        [](const auto& p) -> std::string {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, MixEQ>)
                return "EQ Eight";
            else if constexpr (std::is_same_v<T, MixCompressor>)
                return "Compressor";
            else if constexpr (std::is_same_v<T, MixGate>)
                return "Gate";
            else if constexpr (std::is_same_v<T, MixLimiter>)
                return "Limiter";
            else if constexpr (std::is_same_v<T, MixMultibandDynamics>)
                return "Multiband Dynamics";
            else if constexpr (std::is_same_v<T, MixSaturation>)
                return "Saturator";
            else if constexpr (std::is_same_v<T, MixStereoProcessor>)
                return "Utility";
            else if constexpr (std::is_same_v<T, MixDelay>)
                return "Delay";
            else if constexpr (std::is_same_v<T, MixReverb>)
                return "Reverb";
            else
                return "Audio Effect Rack";
        },
        params);
}

/// Insert an effect chain on a track path, return count of effects inserted.
/// Returns std::unexpected on transport failure.
Result<std::uint64_t> insert_effect_chain(const MixEffectChain& chain,
                                          const LomPath& track_path,
                                          LomTransport& transport,
                                          bool can_insert_native_devices,
                                          MixCompilationResult& result,
                                          bool& unverified_recording) {
    std::uint64_t count = 0;
    const bool has_enabled_effects =
        can_insert_native_devices &&
        std::any_of(chain.effects.begin(), chain.effects.end(), [](const MixEffect& effect) {
            return effect.enabled;
        });
    std::uint32_t first_device_index = 0;
    if (has_enabled_effects) {
        auto existing = transport.device_count(track_path);
        if (!existing || !*existing) return std::unexpected(ErrorCode::ProtocolError);
        first_device_index = **existing;
        if (first_device_index > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
    }

    for (const auto& effect : chain.effects) {
        if (!effect.enabled) continue;
        if (!can_insert_native_devices) continue;
        std::string device = device_for_effect(effect.parameters);
        const std::uint64_t device_index = static_cast<std::uint64_t>(first_device_index) + count;
        if (device_index > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
            return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
        auto device_deployment = insert_device_with_readback(
            track_path, device, static_cast<std::uint32_t>(device_index), 2, transport);
        if (!device_deployment) return std::unexpected(device_deployment.error());
        const bool device_materialized =
            device_deployment->verified || transport.records_without_execution();
        if (device_deployment->verified) {
            result.effects_verified++;
        } else if (device_deployment->observed_type) {
            if (!device_deployment->identity_verified)
                result.warnings.push_back("Inserted Mix effect identity differs from requested '" +
                                          device + "'");
            if (!device_deployment->type_verified)
                result.warnings.push_back("Inserted Mix device is not reported as an audio effect");
            if (device_deployment->observed_active && !*device_deployment->observed_active)
                result.warnings.push_back("Inserted Mix effect is inactive in Live");
            if (!device_deployment->flat_device_verified)
                result.warnings.push_back(
                    "Inserted Mix effect is a Rack whose nested/parallel chains are outside "
                    "Sunny's flat device intent");
            if (!device_deployment->output_verified)
                result.warnings.push_back(
                    "The Mix effect target did not report an audio-output chain");
        }
        result.device_deployments.push_back(std::move(*device_deployment));

        if (!effect.parameter_map.empty()) {
            const auto device_path =
                track_path.child("devices").child(static_cast<int>(device_index));
            for (const auto& [source_path, mapping] : effect.parameter_map) {
                const auto source = get_mix_effect_parameter(effect, source_path);
                if (!source) return std::unexpected(ErrorCode::MixInvalidParameter);
                const auto target = map_mix_device_parameter_value(*source, mapping);
                if (!target) return std::unexpected(target.error());

                MixCompilationResult::ParameterDeployment deployment;
                deployment.effect_id = effect.id;
                deployment.source_path = source_path;
                deployment.device_path = device_path.to_string();
                deployment.parameter_name = mapping.parameter_name;
                deployment.value_property = mapping.value_property;
                deployment.source_value = *source;
                deployment.requested_value = *target;
                deployment.range_min = mapping.range_min;
                deployment.range_max = mapping.range_max;
                if (!device_materialized) {
                    result.warnings.push_back(
                        "Mix effect " + std::to_string(effect.id.value) + " parameter '" +
                        source_path +
                        "' was not applied because its inserted target device was unverified");
                    result.parameter_deployments.push_back(std::move(deployment));
                    continue;
                }

                auto evidence = set_device_parameter_with_readback(device_path,
                                                                   mapping.parameter_name,
                                                                   mapping.value_property,
                                                                   mapping.range_min,
                                                                   mapping.range_max,
                                                                   *target,
                                                                   transport);
                if (!evidence) return std::unexpected(evidence.error());
                deployment.observed_value = evidence->observed;
                deployment.matched_name = evidence->matched_name;
                deployment.original_name = evidence->original_name;
                deployment.observed_minimum = evidence->minimum;
                deployment.observed_maximum = evidence->maximum;
                deployment.is_quantized = evidence->is_quantized;
                deployment.default_value = evidence->default_value;
                deployment.value_items = evidence->value_items;
                deployment.is_enabled = evidence->is_enabled;
                deployment.verified = evidence->verified;
                deployment.parameter_state = evidence->state;
                deployment.automation_state = evidence->automation_state;
                deployment.action = evidence->action;
                if (deployment.is_quantized == true) {
                    result.warnings.push_back(
                        "Live readback for Mix effect " + std::to_string(effect.id.value) +
                        " parameter '" + source_path +
                        "' targets a quantized domain; retained value_items do not prove "
                        "continuous mapping semantics");
                }
                result.parameters_mapped++;
                if (deployment.verified) {
                    result.parameters_verified++;
                } else if (deployment.observed_value) {
                    result.warnings.push_back("Live readback for Mix effect " +
                                              std::to_string(effect.id.value) + " parameter '" +
                                              source_path + "' differed from the requested value");
                } else {
                    unverified_recording = true;
                }
                if (deployment.parameter_state && *deployment.parameter_state != 0) {
                    result.warnings.push_back(
                        "Live readback for Mix effect " + std::to_string(effect.id.value) +
                        " parameter '" + source_path +
                        "' matched, but DeviceParameter.state reports it inactive");
                }
                if (deployment.automation_state && *deployment.automation_state != 0) {
                    result.warnings.push_back("Live readback for Mix effect " +
                                              std::to_string(effect.id.value) + " parameter '" +
                                              source_path +
                                              "' has active or overridden automation; immediate "
                                              "readback is not durable state");
                }
                result.parameter_deployments.push_back(std::move(deployment));
            }
        }
        count++;
    }
    return count;
}

Result<void> record_parameter_coverage(const MixEffectChain& chain,
                                       bool target_deployable,
                                       MixCompilationResult& result) {
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    for (const auto& effect : chain.effects) {
        if (!effect.enabled) continue;
        if (result.effects_requested == limit)
            return std::unexpected(ErrorCode::MixInvalidParameter);
        result.effects_requested++;
        auto source_paths = mix_effect_parameter_paths(effect);
        const auto source_count = static_cast<std::uint64_t>(source_paths.size());
        const auto mapped_count = static_cast<std::uint64_t>(effect.parameter_map.size());

        MixCompilationResult::ParameterCoverage coverage;
        coverage.effect_id = effect.id;
        coverage.source_parameters = source_count;
        coverage.explicitly_mapped = mapped_count;
        coverage.target_deployable = target_deployable;
        for (const auto& path : source_paths) {
            if (!effect.parameter_map.contains(path)) coverage.missing_paths.push_back(path);
        }
        coverage.non_parameter_residual_paths = mix_effect_non_scalar_paths(effect);

        const auto missing = static_cast<std::uint64_t>(coverage.missing_paths.size());
        if (result.parameter_sources_total > limit - source_count ||
            result.parameter_sources_explicitly_mapped > limit - mapped_count ||
            result.parameter_sources_unmapped > limit - missing)
            return std::unexpected(ErrorCode::MixInvalidParameter);
        result.parameter_sources_total += coverage.source_parameters;
        result.parameter_sources_explicitly_mapped += coverage.explicitly_mapped;
        result.parameter_sources_unmapped += missing;
        result.parameter_coverage.push_back(std::move(coverage));
    }
    return {};
}

bool has_unmapped_spatial_fields(const SpatialPosition& spatial) {
    return spatial.depth != 0.0f || spatial.elevation != 0.0f || spatial.width != 0.0f ||
           spatial.pan_law != PanLaw::ConstantPower ||
           spatial.spatial_mode != SpatialMode::SimplePan;
}

bool valid_binding(const AbletonOutputRouteBinding& binding) {
    return !binding.mapping_provenance.empty();
}

Result<void> record_output_route_intent(const MixGraph& graph,
                                        const AbletonOutputRoutingBindings& bindings,
                                        MixCompilationResult& result) {
    constexpr auto limit = std::numeric_limits<std::uint64_t>::max();
    std::set<PartId> part_ids;
    std::set<AuxBusId> aux_ids;
    const auto append =
        [&](MixCompilationResult::OutputRouteDeployment deployment) -> Result<void> {
        if (result.output_routes_requested == limit)
            return std::unexpected(ErrorCode::MixInvalidParameter);
        ++result.output_routes_requested;
        result.output_route_residuals.push_back(deployment.source + " -> " +
                                                deployment.destination);
        result.output_route_deployments.push_back(std::move(deployment));
        return {};
    };

    for (const auto& channel : graph.channels) {
        if (!part_ids.insert(channel.part_id).second)
            return std::unexpected(ErrorCode::MixInvalidParameter);
        const auto target = channel.group_assignment
                                ? "group_buses/" + std::to_string(channel.group_assignment->value)
                                : std::string{"master_bus"};
        MixCompilationResult::OutputRouteDeployment deployment;
        deployment.source = "channels/" + std::to_string(channel.id.value);
        deployment.destination = target;
        deployment.part_id = channel.part_id;
        if (const auto binding = bindings.part_tracks.find(channel.part_id);
            binding != bindings.part_tracks.end()) {
            if (channel.group_assignment || !valid_binding(binding->second))
                return std::unexpected(ErrorCode::MixInvalidParameter);
            deployment.binding = binding->second;
        }
        auto recorded = append(std::move(deployment));
        if (!recorded) return recorded;
    }
    for (const auto& group : graph.group_buses) {
        const auto target =
            group.output.type == GroupOutputType::Group
                ? "group_buses/" + std::to_string(group.output.parent_group_id.value)
                : std::string{"master_bus"};
        MixCompilationResult::OutputRouteDeployment deployment;
        deployment.source = "group_buses/" + std::to_string(group.id.value);
        deployment.destination = target;
        auto recorded = append(std::move(deployment));
        if (!recorded) return recorded;
    }
    for (const auto& aux : graph.aux_buses) {
        if (!aux_ids.insert(aux.id).second) return std::unexpected(ErrorCode::MixInvalidParameter);
        const auto target = aux.output.type == AuxOutputType::Group
                                ? "group_buses/" + std::to_string(aux.output.group_id.value)
                                : std::string{"master_bus"};
        MixCompilationResult::OutputRouteDeployment deployment;
        deployment.source = "aux_buses/" + std::to_string(aux.id.value);
        deployment.destination = target;
        deployment.aux_bus_id = aux.id;
        if (const auto binding = bindings.aux_returns.find(aux.id);
            binding != bindings.aux_returns.end()) {
            if (aux.output.type != AuxOutputType::Master || !valid_binding(binding->second))
                return std::unexpected(ErrorCode::MixInvalidParameter);
            deployment.binding = binding->second;
        }
        auto recorded = append(std::move(deployment));
        if (!recorded) return recorded;
    }
    if (std::ranges::any_of(
            bindings.part_tracks,
            [&](const auto& binding) { return !part_ids.contains(binding.first); }) ||
        std::ranges::any_of(bindings.aux_returns,
                            [&](const auto& binding) { return !aux_ids.contains(binding.first); }))
        return std::unexpected(ErrorCode::MixInvalidParameter);
    return {};
}

nlohmann::json routing_option_json(const AbletonRoutingOptionBinding& option) {
    return {{"display_name", option.display_name}, {"identifier", option.identifier}};
}

std::optional<AbletonRoutingOptionBinding> routing_option(const nlohmann::json& value) {
    if (!value.is_object() || value.size() != 2 || !value.contains("display_name") ||
        !value.at("display_name").is_string() || !value.contains("identifier") ||
        !value.at("identifier").is_string())
        return std::nullopt;
    return AbletonRoutingOptionBinding{value.at("display_name").get<std::string>(),
                                       value.at("identifier").get<std::string>()};
}

std::optional<std::vector<AbletonRoutingOptionBinding>> routing_options(const nlohmann::json& value,
                                                                        std::string_view field) {
    const auto key = std::string{field};
    if (!value.is_object() || value.size() != 1 || !value.contains(key) ||
        !value.at(key).is_array())
        return std::nullopt;
    std::vector<AbletonRoutingOptionBinding> result;
    result.reserve(value.at(key).size());
    for (const auto& encoded : value.at(key)) {
        auto option = routing_option(encoded);
        if (!option) return std::nullopt;
        result.push_back(std::move(*option));
    }
    return result;
}

bool contains_option(const std::vector<AbletonRoutingOptionBinding>& options,
                     const AbletonRoutingOptionBinding& requested) {
    return std::ranges::find(options, requested) != options.end();
}

bool exact_fields(const nlohmann::json& value, std::initializer_list<std::string_view> expected) {
    if (!value.is_object() || value.size() != expected.size()) return false;
    return std::ranges::all_of(
        expected, [&](std::string_view field) { return value.contains(std::string{field}); });
}

Result<void> deploy_output_route(MixCompilationResult::OutputRouteDeployment& deployment,
                                 const LomPath& track_path,
                                 LomTransport& transport,
                                 MixCompilationResult& result) {
    if (!deployment.binding) return {};
    deployment.target_path = track_path.to_string();
    const auto requested_type = routing_option_json(deployment.binding->type);
    const auto requested_channel = routing_option_json(deployment.binding->channel);

    auto type_response = transport.send(
        LomProtocol::call_method(track_path, "sunny_set_output_routing_type", {requested_type}));
    if (!type_response.success) return std::unexpected(ErrorCode::SendFailed);
    if (type_response.value) {
        const auto* evidence = std::get_if<nlohmann::json>(&*type_response.value);
        if (evidence == nullptr ||
            !exact_fields(*evidence,
                          {"requested_type",
                           "available_output_routing_types_before",
                           "output_routing_type",
                           "output_routing_channel",
                           "available_output_routing_types",
                           "available_output_routing_channels"}) ||
            evidence->at("requested_type") != requested_type)
            return std::unexpected(ErrorCode::ProtocolError);
        auto available_before =
            routing_options(evidence->at("available_output_routing_types_before"),
                            "available_output_routing_types");
        auto observed_type = routing_option(evidence->at("output_routing_type"));
        auto observed_channel = routing_option(evidence->at("output_routing_channel"));
        auto available_types = routing_options(evidence->at("available_output_routing_types"),
                                               "available_output_routing_types");
        auto available_channels = routing_options(evidence->at("available_output_routing_channels"),
                                                  "available_output_routing_channels");
        if (!available_before || !observed_type || !observed_channel || !available_types ||
            !available_channels)
            return std::unexpected(ErrorCode::ProtocolError);
        deployment.requested_type_available_verified =
            contains_option(*available_before, deployment.binding->type);
        deployment.type_stage_verified = deployment.requested_type_available_verified &&
                                         *observed_type == deployment.binding->type &&
                                         contains_option(*available_types, *observed_type) &&
                                         contains_option(*available_channels, *observed_channel);
        if (!deployment.type_stage_verified) return std::unexpected(ErrorCode::ProtocolError);
        deployment.observed_type_after_type_stage = std::move(*observed_type);
    } else if (!transport.records_without_execution()) {
        return std::unexpected(ErrorCode::ProtocolError);
    }

    auto channel_response = transport.send(LomProtocol::call_method(
        track_path, "sunny_set_output_routing_channel", {requested_type, requested_channel}));
    if (!channel_response.success) return std::unexpected(ErrorCode::SendFailed);
    if (channel_response.value) {
        const auto* evidence = std::get_if<nlohmann::json>(&*channel_response.value);
        if (evidence == nullptr ||
            !exact_fields(*evidence,
                          {"requested_type",
                           "requested_channel",
                           "output_routing_type_before",
                           "available_output_routing_types_before",
                           "available_output_routing_channels_before",
                           "output_routing_type",
                           "output_routing_channel",
                           "available_output_routing_types",
                           "available_output_routing_channels"}) ||
            evidence->at("requested_type") != requested_type ||
            evidence->at("requested_channel") != requested_channel)
            return std::unexpected(ErrorCode::ProtocolError);
        auto type_before = routing_option(evidence->at("output_routing_type_before"));
        auto available_types_before =
            routing_options(evidence->at("available_output_routing_types_before"),
                            "available_output_routing_types");
        auto available_channels_before =
            routing_options(evidence->at("available_output_routing_channels_before"),
                            "available_output_routing_channels");
        auto observed_type = routing_option(evidence->at("output_routing_type"));
        auto observed_channel = routing_option(evidence->at("output_routing_channel"));
        auto available_types = routing_options(evidence->at("available_output_routing_types"),
                                               "available_output_routing_types");
        auto available_channels = routing_options(evidence->at("available_output_routing_channels"),
                                                  "available_output_routing_channels");
        if (!type_before || !available_types_before || !available_channels_before ||
            !observed_type || !observed_channel || !available_types || !available_channels)
            return std::unexpected(ErrorCode::ProtocolError);
        deployment.requested_channel_available_verified =
            *type_before == deployment.binding->type &&
            contains_option(*available_types_before, deployment.binding->type) &&
            contains_option(*available_channels_before, deployment.binding->channel);
        deployment.channel_stage_verified = deployment.requested_channel_available_verified &&
                                            *observed_type == deployment.binding->type &&
                                            *observed_channel == deployment.binding->channel;
        deployment.final_membership_verified =
            contains_option(*available_types, *observed_type) &&
            contains_option(*available_channels, *observed_channel);
        deployment.verified = deployment.type_stage_verified && deployment.channel_stage_verified &&
                              deployment.final_membership_verified;
        if (!deployment.verified) return std::unexpected(ErrorCode::ProtocolError);
        deployment.observed_type = std::move(*observed_type);
        deployment.observed_channel = std::move(*observed_channel);
        deployment.available_types = std::move(*available_types);
        deployment.available_channels = std::move(*available_channels);
    } else if (!transport.records_without_execution()) {
        return std::unexpected(ErrorCode::ProtocolError);
    }

    ++result.output_routes_written;
    deployment.action = transport.records_without_execution()
                            ? AbletonOutputRouteAction::RecordedOnly
                            : AbletonOutputRouteAction::Set;
    if (deployment.verified) {
        ++result.output_routes_verified;
        const auto residual = deployment.source + " -> " + deployment.destination;
        const auto found = std::ranges::find(result.output_route_residuals, residual);
        if (found == result.output_route_residuals.end())
            return std::unexpected(ErrorCode::InvariantViolation);
        result.output_route_residuals.erase(found);
    }
    return {};
}

const FaderLevelResolution* find_fader_resolution(const RelativeLevelResolution& resolution,
                                                  FaderTargetType target_type,
                                                  std::uint64_t target_id) {
    const auto found =
        std::find_if(resolution.levels.begin(), resolution.levels.end(), [&](const auto& entry) {
            return entry.target_type == target_type && entry.target_id == target_id;
        });
    return found == resolution.levels.end() ? nullptr : &*found;
}

std::string fader_target_name(const FaderLevelResolution& entry) {
    if (entry.target_type == FaderTargetType::Master) return "Master";
    const auto kind = entry.target_type == FaderTargetType::Channel ? "Channel " : "Group ";
    return std::string{kind} + std::to_string(entry.target_id);
}

Result<void> record_property_write(const LomPath& path,
                                   const std::string& property,
                                   const LomValue& value,
                                   LomTransport& transport,
                                   MixCompilationResult& result) {
    auto deployment = set_property_with_readback(path, property, value, transport);
    if (!deployment) return std::unexpected(deployment.error());
    result.property_writes++;
    if (deployment->verified) {
        result.property_writes_verified++;
    } else if (deployment->observed) {
        result.warnings.push_back("Live readback for '" + path.to_string() + "/" + property +
                                  "' differed from the requested value");
    }
    result.property_deployments.push_back(std::move(*deployment));
    return {};
}

} // anonymous namespace

Result<std::vector<std::string>> ableton_unrepresentable_mix_levels(const MixGraph& graph) {
    auto resolution = resolve_relative_levels(graph);
    if (!resolution) return std::unexpected(resolution.error());
    std::vector<std::string> violations;
    const auto format_db = [](float value) {
        auto text = std::to_string(value);
        text.erase(text.find_last_not_of('0') + 1);
        if (!text.empty() && text.back() == '.') text.pop_back();
        return text;
    };
    for (const auto& entry : resolution->levels) {
        // Group Tracks are not materialised, so their faders are never written.
        if (entry.target_type == FaderTargetType::Group) continue;
        const float written = entry.resolved_level_db.value_or(entry.explicit_level_db);
        if (written > ABLETON_FADER_CEILING_DB)
            violations.push_back(fader_target_name(entry) + " fader " + format_db(written) +
                                 " dB exceeds Live's +6 dB fader ceiling");
    }
    for (const auto& aux : graph.aux_buses) {
        if (aux.return_level > ABLETON_FADER_CEILING_DB)
            violations.push_back("Aux bus '" + aux.name + "' return level " +
                                 format_db(aux.return_level) +
                                 " dB exceeds Live's +6 dB fader ceiling");
    }
    for (const auto& channel : graph.channels) {
        for (const auto& send : channel.sends) {
            if (send.enabled && send.level_db > ABLETON_SEND_CEILING_DB)
                violations.push_back("Channel " + std::to_string(channel.id.value) +
                                     " send to aux bus " + std::to_string(send.aux_bus_id.value) +
                                     " at " + format_db(send.level_db) +
                                     " dB exceeds Live's 0 dB send ceiling");
        }
    }
    return violations;
}

Result<MixCompilationResult>
compile_mix_to_ableton_impl(const MixGraph& graph,
                            int base_track,
                            const AbletonPartTrackMap* part_tracks,
                            LomTransport& transport,
                            const AbletonOutputRoutingBindings& routing_bindings) {
    MixCompilationResult result;
    bool unverified_recording = false;

    if (base_track < 0) return std::unexpected(ErrorCode::MixInvalidParameter);
    if (!is_mix_valid(graph)) return std::unexpected(ErrorCode::InvariantViolation);
    // Refuse levels Live cannot represent before any query or mutation, so a
    // plan never fails part-way through applying.
    auto unrepresentable = ableton_unrepresentable_mix_levels(graph);
    if (!unrepresentable) return std::unexpected(unrepresentable.error());
    if (!unrepresentable->empty()) return std::unexpected(ErrorCode::TargetValueUnrepresentable);
    static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
    result.group_tracks_requested = static_cast<std::uint64_t>(graph.group_buses.size());
    result.return_tracks_requested = static_cast<std::uint64_t>(graph.aux_buses.size());
    result.channels_requested = static_cast<std::uint64_t>(graph.channels.size());
    auto output_routes = record_output_route_intent(graph, routing_bindings, result);
    if (!output_routes) return std::unexpected(output_routes.error());
    if (part_tracks == nullptr && !graph.channels.empty() &&
        graph.channels.size() - 1 >
            static_cast<std::size_t>(std::numeric_limits<int>::max() - base_track))
        return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
    auto fader_resolution = resolve_relative_levels(graph);
    if (!fader_resolution) return std::unexpected(fader_resolution.error());
    result.fader_level_resolution = std::move(*fader_resolution);
    for (const auto& entry : result.fader_level_resolution.levels) {
        if (!entry.reference || entry.resolved_level_db) continue;
        const auto reason = entry.status == FaderLevelResolutionStatus::RequiresLoudnessMeasurement
                                ? " requires measured programme loudness"
                                : " depends on another unresolved loudness-relative fader";
        result.warnings.push_back(fader_target_name(entry) + " relative fader intent" + reason +
                                  "; the explicit fallback level was applied");
    }
    if (part_tracks != nullptr) {
        std::set<int> target_tracks;
        for (const auto& entry : *part_tracks) {
            const int target_track = entry.second;
            if (target_track < 0 || !target_tracks.insert(target_track).second)
                return std::unexpected(ErrorCode::MixInvalidParameter);
        }
        for (const auto& channel : graph.channels) {
            const auto target = part_tracks->find(channel.part_id);
            if (target == part_tracks->end())
                return std::unexpected(ErrorCode::MixInvalidParameter);
        }
    }
    auto target_profile = transport.target_profile();
    if (!target_profile) return std::unexpected(target_profile.error());
    if (!*target_profile) return std::unexpected(ErrorCode::ProtocolError);
    auto valid_target_profile = validate_target_profile(**target_profile);
    if (!valid_target_profile) return std::unexpected(valid_target_profile.error());
    result.target_profile = std::move(**target_profile);
    const bool can_insert_native_devices =
        result.target_profile.native_device_insertion == CapabilityState::Available;

    for (const auto& channel : graph.channels) {
        auto coverage =
            record_parameter_coverage(channel.insert_chain, can_insert_native_devices, result);
        if (!coverage) return std::unexpected(coverage.error());
    }
    for (const auto& group : graph.group_buses) {
        auto coverage = record_parameter_coverage(group.insert_chain, false, result);
        if (!coverage) return std::unexpected(coverage.error());
    }
    for (const auto& aux : graph.aux_buses) {
        auto coverage =
            record_parameter_coverage(aux.effect_chain, can_insert_native_devices, result);
        if (!coverage) return std::unexpected(coverage.error());
    }
    auto coverage =
        record_parameter_coverage(graph.master_bus.insert_chain, can_insert_native_devices, result);
    if (!coverage) return std::unexpected(coverage.error());

    // Track index allocation:
    // base_track .. base_track + channels.size() - 1: channel tracks created
    // by the Score compiler. Return tracks are created here.
    int first_return_track = 0;
    if (!graph.aux_buses.empty()) {
        auto return_track_count = transport.return_track_count();
        if (!return_track_count) return std::unexpected(return_track_count.error());
        if (!*return_track_count) return std::unexpected(ErrorCode::ProtocolError);
        if (**return_track_count > static_cast<std::uint32_t>(std::numeric_limits<int>::max()))
            return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
        first_return_track = static_cast<int>(**return_track_count);
        if (graph.aux_buses.size() - 1 >
            static_cast<std::size_t>(std::numeric_limits<int>::max() - first_return_track))
            return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
    }
    std::map<std::uint64_t, int> return_indices;
    for (std::size_t index = 0; index < graph.aux_buses.size(); ++index) {
        const auto return_index = first_return_track + static_cast<int>(index);
        if (!return_indices.emplace(graph.aux_buses[index].id.value, return_index).second)
            return std::unexpected(ErrorCode::InvariantViolation);
    }

    if (graph.output_format != OutputFormat::Stereo ||
        graph.master_bus.output_format != OutputFormat::Stereo) {
        result.warnings.push_back(
            "Non-stereo output format was preserved in the IR but not configured in Ableton");
    }
    // Song has no public group-track creation method. Do not silently create
    // an audio track, which would have different routing and folding semantics.
    for (const auto& group : graph.group_buses) {
        result.warnings.push_back("Group bus '" + group.name +
                                  "' was not materialised: the public Live Object Model cannot "
                                  "create or assign Group Tracks");
        std::uint64_t enabled_sends = 0;
        for (const auto& send : group.sends) {
            if (!send.enabled) continue;
            if (result.sends_requested == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::MixInvalidParameter);
            ++result.sends_requested;
            ++result.send_levels_requested;
            ++result.send_modes_requested;
            ++enabled_sends;
        }
        if (enabled_sends != 0)
            result.warnings.push_back(
                std::to_string(enabled_sends) + " enabled send(s) on Group bus '" + group.name +
                "' were not configured because the Group Track was not materialised");
    }

    // Step 2: Create return tracks
    for (const auto& aux : graph.aux_buses) {
        const int return_index = return_indices.at(aux.id.value);
        if (has_unmapped_spatial_fields(aux.return_spatial)) {
            result.warnings.push_back("Aux bus '" + aux.name +
                                      "' non-pan return spatial settings were not applied");
        }
        if (aux.output.type != AuxOutputType::Master) {
            result.warnings.push_back("Aux bus '" + aux.name +
                                      "' group output routing was not applied");
        }

        auto resp =
            transport.send(LomProtocol::call_method(LomPaths::song(), "create_return_track", {}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        auto property_result =
            record_property_write(LomPath{{"song", "return_tracks", std::to_string(return_index)}},
                                  "name",
                                  aux.name,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());

        auto route = std::ranges::find_if(result.output_route_deployments, [&](const auto& item) {
            return item.aux_bus_id && *item.aux_bus_id == aux.id;
        });
        if (route == result.output_route_deployments.end())
            return std::unexpected(ErrorCode::InvariantViolation);
        auto route_result =
            deploy_output_route(*route,
                                LomPath{{"song", "return_tracks", std::to_string(return_index)}},
                                transport,
                                result);
        if (!route_result) return std::unexpected(route_result.error());

        // AuxBus has no mute/solo or crossfader intent: the generated return is
        // an enabled wet path whose gain is controlled by sends and return_level.
        property_result =
            record_property_write(LomPath{{"song", "return_tracks", std::to_string(return_index)}},
                                  "mute",
                                  false,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result =
            record_property_write(LomPath{{"song", "return_tracks", std::to_string(return_index)}},
                                  "solo",
                                  false,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());

        const auto return_mixer =
            LomPath{{"song", "return_tracks", std::to_string(return_index), "mixer_device"}};
        property_result = record_property_write(
            return_mixer.child("track_activator"), "value", 1.0, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result =
            record_property_write(return_mixer, "crossfade_assign", 1, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(return_mixer, "panning_mode", 0, transport, result);
        if (!property_result) return std::unexpected(property_result.error());

        // Insert aux effect chain
        auto chain_result =
            insert_effect_chain(aux.effect_chain,
                                LomPath{{"song", "return_tracks", std::to_string(return_index)}},
                                transport,
                                can_insert_native_devices,
                                result,
                                unverified_recording);
        if (!chain_result) return std::unexpected(chain_result.error());
        result.effects_inserted += *chain_result;

        // Set return level
        auto return_volume = LomPath{{"song", "return_tracks", std::to_string(return_index)}}
                                 .child("mixer_device")
                                 .child("volume");
        property_result = record_property_write(return_volume,
                                                "display_value",
                                                static_cast<double>(aux.return_level),
                                                transport,
                                                result);
        if (!property_result) return std::unexpected(property_result.error());

        auto return_panning = return_mixer.child("panning");
        property_result = record_property_write(return_panning,
                                                "value",
                                                static_cast<double>(aux.return_spatial.pan),
                                                transport,
                                                result);
        if (!property_result) return std::unexpected(property_result.error());

        result.return_track_deployments.push_back(
            {aux.id, return_index, aux.name, false, false, 1.0, 1, 0, aux.return_spatial.pan});

        result.return_tracks_created++;
    }

    // Step 3: Configure channel strips
    for (std::size_t i = 0; i < graph.channels.size(); ++i) {
        const auto& ch = graph.channels[i];
        const int track_idx =
            part_tracks != nullptr ? part_tracks->at(ch.part_id) : base_track + static_cast<int>(i);
        auto track_path = LomPaths::track(track_idx);

        auto route = std::ranges::find_if(result.output_route_deployments, [&](const auto& item) {
            return item.part_id && *item.part_id == ch.part_id;
        });
        if (route == result.output_route_deployments.end())
            return std::unexpected(ErrorCode::InvariantViolation);
        auto route_result = deploy_output_route(*route, track_path, transport, result);
        if (!route_result) return std::unexpected(route_result.error());

        if (ch.input_trim != 0.0f)
            result.warnings.push_back("Channel " + std::to_string(ch.id.value) +
                                      " input trim was not applied");
        if (ch.polarity_invert)
            result.warnings.push_back("Channel " + std::to_string(ch.id.value) +
                                      " polarity inversion was not applied");
        if (has_unmapped_spatial_fields(ch.spatial))
            result.warnings.push_back("Channel " + std::to_string(ch.id.value) +
                                      " non-pan spatial settings were not applied");

        // Insert effects
        auto chain_result = insert_effect_chain(ch.insert_chain,
                                                track_path,
                                                transport,
                                                can_insert_native_devices,
                                                result,
                                                unverified_recording);
        if (!chain_result) return std::unexpected(chain_result.error());
        result.effects_inserted += *chain_result;

        // Set fader level
        auto volume_parameter = track_path.child("mixer_device").child("volume");
        const auto* fader = find_fader_resolution(
            result.fader_level_resolution, FaderTargetType::Channel, ch.id.value);
        if (fader == nullptr) return std::unexpected(ErrorCode::InvariantViolation);
        auto property_result = record_property_write(
            volume_parameter,
            "display_value",
            static_cast<double>(fader->resolved_level_db.value_or(fader->explicit_level_db)),
            transport,
            result);
        if (!property_result) return std::unexpected(property_result.error());

        // Set pan
        auto pan_parameter = track_path.child("mixer_device").child("panning");
        property_result = record_property_write(
            pan_parameter, "value", static_cast<double>(ch.spatial.pan), transport, result);
        if (!property_result) return std::unexpected(property_result.error());

        // Apply both truth values so the target cannot retain stale Live state.
        property_result = record_property_write(track_path, "mute", ch.mute, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result = record_property_write(track_path, "solo", ch.solo, transport, result);
        if (!property_result) return std::unexpected(property_result.error());
        property_result =
            record_property_write(track_path.child("mixer_device").child("track_activator"),
                                  "value",
                                  ch.mute ? 0.0 : 1.0,
                                  transport,
                                  result);
        if (!property_result) return std::unexpected(property_result.error());

        // Configure sends
        for (const auto& send : ch.sends) {
            if (!send.enabled) continue;
            if (result.sends_requested == std::numeric_limits<std::uint64_t>::max())
                return std::unexpected(ErrorCode::MixInvalidParameter);
            result.sends_requested++;
            result.send_levels_requested++;
            result.send_modes_requested++;
            result.warnings.push_back(
                "Channel " + std::to_string(ch.id.value) + " " +
                (send.pre_fader ? "pre-fader" : "post-fader") +
                " send mode was not applied: the admitted public Live surface configures the "
                "send level but not its pre/post mode");
            auto return_it = return_indices.find(send.aux_bus_id.value);
            if (return_it == return_indices.end())
                return std::unexpected(ErrorCode::InvariantViolation);
            auto send_path =
                track_path.child("mixer_device").child("sends").child(return_it->second);
            property_result = record_property_write(
                send_path, "display_value", static_cast<double>(send.level_db), transport, result);
            if (!property_result) return std::unexpected(property_result.error());
            result.send_levels_configured++;
        }

        result.channels_configured++;
    }

    // Step 4: Master bus processing
    auto master_path = LomPath{{"song", "master_track"}};
    auto master_chain = insert_effect_chain(graph.master_bus.insert_chain,
                                            master_path,
                                            transport,
                                            can_insert_native_devices,
                                            result,
                                            unverified_recording);
    if (!master_chain) return std::unexpected(master_chain.error());
    result.effects_inserted += *master_chain;

    // MasterBus has no mute or pan intent. Project it to an enabled, centered
    // Stereo Pan stage so pre-existing Main state cannot silently alter output.
    const auto master_mixer = master_path.child("mixer_device");
    auto master_result = record_property_write(
        master_mixer.child("track_activator"), "value", 1.0, transport, result);
    if (!master_result) return std::unexpected(master_result.error());
    master_result = record_property_write(master_mixer, "panning_mode", 0, transport, result);
    if (!master_result) return std::unexpected(master_result.error());
    master_result =
        record_property_write(master_mixer.child("panning"), "value", 0.0, transport, result);
    if (!master_result) return std::unexpected(master_result.error());
    result.master_track_deployment = MixCompilationResult::MasterTrackDeployment{};

    // Set master fader.
    auto master_volume = master_mixer.child("volume");
    const auto* master_fader =
        find_fader_resolution(result.fader_level_resolution, FaderTargetType::Master, 0);
    if (master_fader == nullptr) return std::unexpected(ErrorCode::InvariantViolation);
    master_result =
        record_property_write(master_volume,
                              "display_value",
                              static_cast<double>(master_fader->resolved_level_db.value_or(
                                  master_fader->explicit_level_db)),
                              transport,
                              result);
    if (!master_result) return std::unexpected(master_result.error());

    if (graph.master_bus.atmos_config)
        result.warnings.push_back("Atmos output configuration was not applied");
    if (graph.master_bus.dithering)
        result.warnings.push_back("Master dithering configuration was not applied");
    if (graph.master_bus.target_loudness)
        result.warnings.push_back("Master target loudness was not applied automatically");

    if (!graph.automation.empty()) {
        static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
        result.automation_lanes_requested = static_cast<std::uint64_t>(graph.automation.size());
        result.warnings.push_back(
            "Mix automation was not written: the public Live Object Model can clear but cannot "
            "author clip automation envelopes");
    }

    const bool has_effect_parameters = !result.parameter_coverage.empty();
    const bool has_undeployable_mappings =
        std::any_of(result.parameter_coverage.begin(),
                    result.parameter_coverage.end(),
                    [](const MixCompilationResult::ParameterCoverage& item) {
                        return !item.target_deployable && item.explicitly_mapped != 0;
                    });
    const bool has_non_parameter_residuals =
        std::any_of(result.parameter_coverage.begin(),
                    result.parameter_coverage.end(),
                    [](const MixCompilationResult::ParameterCoverage& item) {
                        return !item.non_parameter_residual_paths.empty();
                    });
    if (has_effect_parameters && !can_insert_native_devices) {
        result.warnings.push_back(
            "Native effects were not inserted: Track.insert_device requires Live 12.3+");
    }
    if (result.parameter_sources_unmapped != 0) {
        result.warnings.push_back(
            "Explicit DeviceParameter mappings cover " +
            std::to_string(result.parameter_sources_explicitly_mapped) + " of " +
            std::to_string(result.parameter_sources_total) +
            " scalar Mix effect sources; exact missing paths are reported in parameter_coverage");
    }
    if (has_undeployable_mappings) {
        result.warnings.push_back(
            "One or more explicit Mix effect mappings target a chain that this Live target "
            "cannot materialise");
    }
    if (has_non_parameter_residuals) {
        result.warnings.push_back(
            "Non-DeviceParameter Mix effect fields require target-specific routing or resource "
            "loading; exact residual paths are reported in parameter_coverage");
    }
    if (unverified_recording) {
        result.warnings.push_back(
            "Mix effect parameter writes were recorded without Live readback evidence");
    }
    if (result.output_routes_requested != result.output_routes_verified) {
        result.warnings.push_back(
            "One or more Mix output routes remain unverified: each materialisable edge requires "
            "an explicit semantic binding with provenance to exact Live-advertised type/channel "
            "dictionaries; exact residual edges are reported in output_route_residuals");
    }

    return result;
}

Result<MixCompilationResult>
compile_mix_to_ableton(const MixGraph& graph, int base_track, LomTransport& transport) {
    return compile_mix_to_ableton_impl(graph, base_track, nullptr, transport, {});
}

Result<MixCompilationResult>
compile_mix_to_ableton(const MixGraph& graph,
                       int base_track,
                       LomTransport& transport,
                       const AbletonOutputRoutingBindings& routing_bindings) {
    return compile_mix_to_ableton_impl(graph, base_track, nullptr, transport, routing_bindings);
}

Result<MixCompilationResult> compile_mix_to_ableton(const MixGraph& graph,
                                                    const AbletonPartTrackMap& part_tracks,
                                                    LomTransport& transport) {
    return compile_mix_to_ableton_impl(graph, 0, &part_tracks, transport, {});
}

Result<MixCompilationResult>
compile_mix_to_ableton(const MixGraph& graph,
                       const AbletonPartTrackMap& part_tracks,
                       LomTransport& transport,
                       const AbletonOutputRoutingBindings& routing_bindings) {
    return compile_mix_to_ableton_impl(graph, 0, &part_tracks, transport, routing_bindings);
}

} // namespace sunny::infrastructure::formats
