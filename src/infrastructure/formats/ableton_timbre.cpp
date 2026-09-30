/**
 * @file ableton_timbre.cpp
 * @brief Ableton Live Compiler — Timbre IR to device chains
 *
 *
 * Maps TimbreProfile source variants to Ableton devices:
 *   SubtractiveSynth → Analog
 *   FMSynth          → Operator
 *   WavetableSynth   → Wavetable
 *   SamplerSource    → Simpler
 *   Hybrid           → Instrument Rack
 *
 * EffectChain entries map to Ableton audio effects in order.
 * Native devices are inserted through Track.insert_device (Live 12.3+).
 * Requests that public LOM cannot represent are surfaced as warnings.
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <sunny/core/timbre/validation.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/formats/ableton_evidence.hpp>
#include <sunny/infrastructure/formats/ableton_timbre.hpp>

namespace sunny::infrastructure::formats {

using namespace sunny::core;

namespace {

struct ResolvedParameterMapping {
    std::string path;
    const DeviceParameter* mapping = nullptr;
    float source_value = 0.0f;
    float target_value = 0.0f;
};

Result<std::vector<ResolvedParameterMapping>>
resolve_parameter_mappings(const TimbreProfile& profile) {
    std::vector<ResolvedParameterMapping> resolved;
    resolved.reserve(profile.rendering.parameter_map.size());
    for (const auto& [path, mapping] : profile.rendering.parameter_map) {
        if (path.empty() || mapping.parameter_name.empty())
            return std::unexpected(ErrorCode::TimbreInvalidParameter);
        auto source_value = get_parameter(profile, path);
        if (!source_value) return std::unexpected(ErrorCode::TimbreInvalidParameter);
        auto target_value = map_device_parameter_value(*source_value, mapping);
        if (!target_value) return std::unexpected(target_value.error());
        resolved.push_back({path, &mapping, *source_value, *target_value});
    }
    return resolved;
}

Result<void> deploy_parameter_mapping(const LomPath& device_path,
                                      const std::string& ir_path,
                                      const DeviceParameter& mapping,
                                      float source_value,
                                      float target_value,
                                      LomTransport& transport,
                                      TimbreCompilationResult& result,
                                      bool& unverified_recording) {
    auto evidence = set_device_parameter_with_readback(device_path,
                                                       mapping.parameter_name,
                                                       mapping.value_property,
                                                       mapping.range_min,
                                                       mapping.range_max,
                                                       target_value,
                                                       transport);
    if (!evidence) return std::unexpected(evidence.error());
    result.parameters_mapped++;

    AbletonParameterDeployment deployment;
    deployment.ir_path = ir_path;
    deployment.device_index = mapping.device_index;
    deployment.parameter_name = mapping.parameter_name;
    deployment.value_property = mapping.value_property;
    deployment.source_value = source_value;
    deployment.requested_value = target_value;
    deployment.range_min = mapping.range_min;
    deployment.range_max = mapping.range_max;
    deployment.matched_name = evidence->matched_name;
    deployment.original_name = evidence->original_name;
    deployment.observed_value = evidence->observed;
    deployment.observed_minimum = evidence->minimum;
    deployment.observed_maximum = evidence->maximum;
    deployment.is_quantized = evidence->is_quantized;
    deployment.default_value = evidence->default_value;
    deployment.value_items = evidence->value_items;
    deployment.is_enabled = evidence->is_enabled;
    if (deployment.observed_value) {
        deployment.verified = evidence->verified;
        if (deployment.verified) {
            result.parameters_verified++;
        } else {
            result.warnings.push_back("Live readback for parameter mapping '" + ir_path +
                                      "' differed from the requested value");
        }
    } else {
        unverified_recording = true;
    }
    deployment.parameter_state = evidence->state;
    deployment.automation_state = evidence->automation_state;
    deployment.action = evidence->action;
    if (deployment.is_quantized == true) {
        result.warnings.push_back(
            "Live parameter mapping '" + ir_path +
            "' targets a quantized domain; retained value_items do not prove continuous "
            "mapping semantics");
    }
    if (deployment.parameter_state && *deployment.parameter_state != 0) {
        result.warnings.push_back("Live parameter mapping '" + ir_path +
                                  "' was written but DeviceParameter.state reports it inactive");
    }
    if (deployment.automation_state && *deployment.automation_state != 0) {
        result.warnings.push_back(
            "Live parameter mapping '" + ir_path +
            "' has active or overridden automation; immediate readback is not durable state");
    }
    result.parameter_deployments.push_back(std::move(deployment));
    return {};
}

/// Default Ableton device name for a sound source variant
std::optional<std::string> default_device_for_source(const SoundSourceData& source) {
    return std::visit(
        [](const auto& s) -> std::optional<std::string> {
            using T = std::decay_t<decltype(s)>;
            if constexpr (std::is_same_v<T, SubtractiveSynth>)
                return "Analog";
            else if constexpr (std::is_same_v<T, FMSynth>)
                return "Operator";
            else if constexpr (std::is_same_v<T, WavetableSynth>)
                return "Wavetable";
            else if constexpr (std::is_same_v<T, SamplerSource>)
                return "Simpler";
            else if constexpr (std::is_same_v<T, GranularSynth>)
                return std::nullopt;
            else if constexpr (std::is_same_v<T, AdditiveSynth>)
                return "Operator";
            else if constexpr (std::is_same_v<T, PhysicalModelSource>)
                return "Collision";
            else if constexpr (std::is_same_v<T, HybridSource>)
                return std::nullopt;
            else
                return std::nullopt;
        },
        source.data);
}

/// Effect type name for Ableton device selection
std::string ableton_effect_name(const EffectParameters& params) {
    return std::visit(
        [](const auto& p) -> std::string {
            using T = std::decay_t<decltype(p)>;
            if constexpr (std::is_same_v<T, DistortionEffect>)
                return "Saturator";
            else if constexpr (std::is_same_v<T, DelayEffect>)
                return "Delay";
            else if constexpr (std::is_same_v<T, ReverbEffect>)
                return "Reverb";
            else if constexpr (std::is_same_v<T, ChorusEffect>)
                return "Chorus-Ensemble";
            else if constexpr (std::is_same_v<T, PhaserEffect>)
                return "Phaser-Flanger";
            else if constexpr (std::is_same_v<T, FlangerEffect>)
                return "Phaser-Flanger";
            else if constexpr (std::is_same_v<T, EQEffect>)
                return "EQ Eight";
            else if constexpr (std::is_same_v<T, CompressorEffect>)
                return "Compressor";
            else
                return "Audio Effect Rack";
        },
        params);
}

} // anonymous namespace

Result<TimbreCompilationResult>
compile_timbre_to_ableton(const TimbreProfile& profile, int track_index, LomTransport& transport) {
    TimbreCompilationResult result;
    if (track_index < 0) return std::unexpected(ErrorCode::TimbreInvalidParameter);
    // Resolve every declarative mapping before the first target mutation.
    // This gives malformed paths, domains, and curves transaction-like
    // all-or-nothing behaviour at Sunny's compilation boundary.
    auto resolved_mappings = resolve_parameter_mappings(profile);
    if (!resolved_mappings) return std::unexpected(resolved_mappings.error());
    const auto diagnostics = validate_timbre(profile);
    if (std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
            return diagnostic.severity == ValidationSeverity::Error && diagnostic.rule == "T10";
        }))
        return std::unexpected(ErrorCode::TimbreInvalidParameter);
    if (std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
            return diagnostic.severity == ValidationSeverity::Error;
        }))
        return std::unexpected(ErrorCode::InvariantViolation);
    result.devices_requested = 1;
    if (!profile.insert_chain.bypass_all) {
        result.effects_requested = static_cast<std::uint64_t>(
            std::count_if(profile.insert_chain.effects.begin(),
                          profile.insert_chain.effects.end(),
                          [](const Effect& effect) { return effect.enabled; }));
    }
    auto target_profile = transport.target_profile();
    if (!target_profile) return std::unexpected(target_profile.error());
    if (!*target_profile) return std::unexpected(ErrorCode::ProtocolError);
    auto valid_target_profile = validate_target_profile(**target_profile);
    if (!valid_target_profile) return std::unexpected(valid_target_profile.error());
    result.target_profile = std::move(**target_profile);
    const bool can_insert_native_devices =
        result.target_profile.native_device_insertion == CapabilityState::Available;
    auto track = LomPaths::track(track_index);

    // Step 1: Load instrument device
    const auto& rc = profile.rendering;
    std::string device_name;

    bool insert_source = true;
    if (rc.device_type.tag == DeviceTypeTag::NativeAbleton && !rc.device_type.device_name.empty()) {
        device_name = rc.device_type.device_name;
    } else if (rc.device_type.tag == DeviceTypeTag::Plugin) {
        insert_source = false;
        result.warnings.push_back(
            "Plugin source '" + rc.device_type.plugin_identifier +
            "' was not inserted: Track.insert_device supports native Live devices only");
    } else {
        auto default_device = default_device_for_source(profile.source);
        if (default_device) {
            device_name = std::move(*default_device);
        } else {
            insert_source = false;
            result.warnings.push_back(
                "The source was not inserted because no faithful native Live device mapping "
                "exists for this source type");
        }
    }

    if (insert_source) {
        std::size_t enabled_effect_count = 0;
        if (!profile.insert_chain.bypass_all) {
            enabled_effect_count = static_cast<std::size_t>(
                std::count_if(profile.insert_chain.effects.begin(),
                              profile.insert_chain.effects.end(),
                              [](const Effect& effect) { return effect.enabled; }));
        }
        if (enabled_effect_count > static_cast<std::size_t>(std::numeric_limits<int>::max()))
            return std::unexpected(ErrorCode::TargetAddressUnrepresentable);
        const std::size_t planned_device_count = enabled_effect_count + 1;
        for (const auto& resolved : *resolved_mappings) {
            if (resolved.mapping->device_index >
                    static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
                resolved.mapping->device_index >= planned_device_count)
                return std::unexpected(ErrorCode::TimbreInvalidParameter);
        }
    }

    if (insert_source && can_insert_native_devices) {
        auto existing_device_count = transport.device_count(track);
        if (!existing_device_count || !*existing_device_count)
            return std::unexpected(ErrorCode::ProtocolError);
        if (**existing_device_count != 0)
            return std::unexpected(ErrorCode::TargetValueUnrepresentable);
        auto deployment = insert_device_with_readback(track, device_name, 0, 1, transport);
        if (!deployment) return std::unexpected(deployment.error());
        result.devices_created++;
        if (deployment->verified) {
            result.devices_verified++;
        } else if (deployment->observed_type) {
            if (!deployment->identity_verified)
                result.warnings.push_back(
                    "Inserted source device identity differs from requested '" + device_name + "'");
            if (!deployment->type_verified)
                result.warnings.push_back(
                    "Inserted source device is not reported as an instrument");
            if (deployment->observed_active && !*deployment->observed_active)
                result.warnings.push_back("Inserted source device is inactive in Live");
            if (!deployment->flat_device_verified)
                result.warnings.push_back(
                    "Inserted source device is a Rack whose nested/parallel chains are outside "
                    "Sunny's flat device intent");
            if (!deployment->output_verified)
                result.warnings.push_back(
                    "The source track did not report instrument-generated audio output");
        }
        result.device_deployments.push_back(std::move(*deployment));
    } else if (insert_source) {
        result.warnings.push_back(
            "The native source device was not inserted: Track.insert_device requires Live 12.3+");
    }

    if (rc.preset_path) {
        result.warnings.push_back(
            "Preset path was not loaded: arbitrary browser/preset loading is not exposed by the "
            "public Live Object Model");
    }

    // Step 3: Insert effect chain
    const bool source_materialized =
        result.devices_created > 0 && (transport.records_without_execution() ||
                                       result.devices_verified == result.devices_created);
    std::uint64_t device_offset = result.devices_created;
    bool unverified_offline_mapping = false;
    for (std::size_t effect_index = 0; effect_index < profile.insert_chain.effects.size();
         ++effect_index) {
        const auto& effect = profile.insert_chain.effects[effect_index];
        if (profile.insert_chain.bypass_all) continue;
        if (!effect.enabled) continue;

        if (!can_insert_native_devices || !source_materialized) continue;

        std::string effect_device = ableton_effect_name(effect.parameters);
        auto deployment = insert_device_with_readback(
            track, effect_device, static_cast<std::uint32_t>(device_offset), 2, transport);
        if (!deployment) return std::unexpected(deployment.error());
        result.effects_inserted++;
        if (deployment->verified) {
            result.effects_verified++;
        } else if (deployment->observed_type) {
            if (!deployment->identity_verified)
                result.warnings.push_back(
                    "Inserted Timbre effect identity differs from requested '" + effect_device +
                    "'");
            if (!deployment->type_verified)
                result.warnings.push_back(
                    "Inserted Timbre effect is not reported as an audio effect");
            if (deployment->observed_active && !*deployment->observed_active)
                result.warnings.push_back("Inserted Timbre effect is inactive in Live");
            if (!deployment->flat_device_verified)
                result.warnings.push_back(
                    "Inserted Timbre effect is a Rack whose nested/parallel chains are outside "
                    "Sunny's flat device intent");
            if (!deployment->output_verified)
                result.warnings.push_back(
                    "The Timbre effect track did not retain instrument-generated audio output");
        }
        result.device_deployments.push_back(std::move(*deployment));

        // Native devices expose effect balance as a DeviceParameter, not as a
        // Device property. Avoid touching the parameter when the IR requests
        // the device's fully-wet/default value because some devices (for
        // example EQ Eight) intentionally have no Dry/Wet parameter.
        const auto effect_mix_path =
            "insert_chain.effects[" + std::to_string(effect_index) + "].mix";
        if (effect.mix < 1.0f && !rc.parameter_map.contains(effect_mix_path)) {
            auto effect_path = track.child("devices").child(static_cast<int>(device_offset));
            DeviceParameter wet_mapping;
            wet_mapping.device_index = static_cast<std::uint32_t>(device_offset);
            wet_mapping.parameter_name = "Dry/Wet";
            auto wet_result = deploy_parameter_mapping(effect_path,
                                                       effect_mix_path,
                                                       wet_mapping,
                                                       effect.mix,
                                                       effect.mix,
                                                       transport,
                                                       result,
                                                       unverified_offline_mapping);
            if (!wet_result) return std::unexpected(wet_result.error());
        }

        device_offset++;
    }

    // Apply the explicitly declared mappings only after the complete requested
    // device chain exists. Device indices are therefore stable for this pass.
    for (const auto& resolved : *resolved_mappings) {
        const auto& mapping = *resolved.mapping;
        AbletonParameterDeployment deployment;
        deployment.ir_path = resolved.path;
        deployment.device_index = mapping.device_index;
        deployment.parameter_name = mapping.parameter_name;
        deployment.value_property = mapping.value_property;
        deployment.source_value = resolved.source_value;
        deployment.requested_value = resolved.target_value;
        deployment.range_min = mapping.range_min;
        deployment.range_max = mapping.range_max;
        if (!source_materialized || mapping.device_index >= device_offset) {
            result.warnings.push_back(
                "Parameter mapping '" + resolved.path +
                "' was preserved but not applied because its target device was not materialised");
            result.parameter_deployments.push_back(std::move(deployment));
            continue;
        }

        const auto device_path =
            track.child("devices").child(static_cast<int>(mapping.device_index));
        auto mapping_result = deploy_parameter_mapping(device_path,
                                                       resolved.path,
                                                       mapping,
                                                       resolved.source_value,
                                                       resolved.target_value,
                                                       transport,
                                                       result,
                                                       unverified_offline_mapping);
        if (!mapping_result) return std::unexpected(mapping_result.error());
    }

    if (unverified_offline_mapping) {
        result.warnings.push_back(
            "Parameter writes were recorded by an offline transport and have no Live readback");
    }

    if (result.devices_created > 0) {
        result.warnings.push_back(
            rc.parameter_map.empty()
                ? "The native source device was inserted, but source-specific parameter values "
                  "were not applied"
                : "Only explicitly declared Timbre rendering mappings were applied; unspecified "
                  "source parameters remain target-dependent");
    }

    if (result.effects_inserted > 0) {
        const bool has_explicit_effect_mappings =
            std::any_of(rc.parameter_map.begin(), rc.parameter_map.end(), [](const auto& entry) {
                return entry.first.starts_with("insert_chain.effects[");
            });
        result.warnings.push_back(has_explicit_effect_mappings
                                      ? "Only explicitly mapped effect values and non-default wet "
                                        "balances were applied; unspecified effect parameters "
                                        "remain target-dependent"
                                      : "Native effects were inserted and non-default wet "
                                        "balances were applied, but other effect-specific "
                                        "parameter values were not applied");
    } else if (!profile.insert_chain.effects.empty() && !profile.insert_chain.bypass_all) {
        result.warnings.push_back(can_insert_native_devices
                                      ? "Native audio effects were not inserted because Sunny did "
                                        "not materialise an instrument source on the MIDI track"
                                      : "Native effects were not inserted: Track.insert_device "
                                        "requires Live 12.3+");
    }

    // The public Live Object Model cannot author modulation routings, macro
    // mappings or preset interpolation, so each is a target gap reported
    // individually (Timbre spec §0.5) rather than silently dropped.
    for (const auto& routing : profile.modulation.routings) {
        result.warnings.push_back("Modulation routing to '" + routing.target +
                                  "' was not applied: the public Live Object Model cannot "
                                  "author synthesis modulation routings");
    }
    for (const auto& macro : profile.modulation.macro_knobs) {
        result.warnings.push_back("Macro " + std::to_string(macro.index) + " ('" + macro.name +
                                  "') and its " + std::to_string(macro.mappings.size()) +
                                  " mapping(s) were not applied");
    }
    for (const auto& morph : profile.preset_morphs) {
        result.warnings.push_back(
            "Preset morph from preset " + std::to_string(morph.from_preset.value) + " to preset " +
            std::to_string(morph.to_preset.value) + " starting at bar " +
            std::to_string(morph.start.bar) + " was not applied: Live cannot interpolate presets");
    }

    if (!profile.parameter_automation.empty()) {
        static_assert(sizeof(std::size_t) <= sizeof(std::uint64_t));
        result.automation_lanes_requested =
            static_cast<std::uint64_t>(profile.parameter_automation.size());
        result.warnings.push_back(
            "Timbre automation was not written: the public Live Object Model can clear but cannot "
            "author clip automation envelopes");
    }

    return result;
}

} // namespace sunny::infrastructure::formats
