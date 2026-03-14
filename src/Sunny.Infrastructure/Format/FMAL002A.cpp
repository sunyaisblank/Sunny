/**
 * @file FMAL002A.cpp
 * @brief Ableton Live Compiler — Timbre IR to device chains
 *
 * Component: FMAL002A
 *
 * Maps TimbreProfile source variants to Ableton devices:
 *   SubtractiveSynth → Analog
 *   FMSynth          → Operator
 *   WavetableSynth   → Wavetable
 *   SamplerSource    → Simpler
 *   Others           → Plugin (via TimbreRenderingConfig)
 *
 * EffectChain entries map to Ableton audio effects in order.
 * Parameter mappings from TimbreRenderingConfig are applied via
 * set_property on device parameter paths.
 */

#include "FMAL002A.h"

namespace Sunny::Infrastructure::Format {

using namespace Sunny::Core;

namespace {

/// Default Ableton device name for a sound source variant
std::string default_device_for_source(const SoundSourceData& source) {
    return std::visit([](const auto& s) -> std::string {
        using T = std::decay_t<decltype(s)>;
        if constexpr (std::is_same_v<T, SubtractiveSynth>)  return "Analog";
        else if constexpr (std::is_same_v<T, FMSynth>)      return "Operator";
        else if constexpr (std::is_same_v<T, WavetableSynth>) return "Wavetable";
        else if constexpr (std::is_same_v<T, SamplerSource>) return "Simpler";
        else if constexpr (std::is_same_v<T, GranularSynth>) return "Simpler";
        else if constexpr (std::is_same_v<T, AdditiveSynth>) return "Operator";
        else if constexpr (std::is_same_v<T, PhysicalModelSource>) return "Collision";
        else if constexpr (std::is_same_v<T, HybridSource>)  return "Instrument Rack";
        else return "Simpler";
    }, source.data);
}

/// Effect type name for Ableton device selection
std::string ableton_effect_name(const EffectParameters& params) {
    return std::visit([](const auto& p) -> std::string {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, DistortionEffect>)  return "Saturator";
        else if constexpr (std::is_same_v<T, DelayEffect>)  return "Delay";
        else if constexpr (std::is_same_v<T, ReverbEffect>)  return "Reverb";
        else if constexpr (std::is_same_v<T, ChorusEffect>)  return "Chorus-Ensemble";
        else if constexpr (std::is_same_v<T, PhaserEffect>)  return "Phaser";
        else if constexpr (std::is_same_v<T, FlangerEffect>) return "Flanger";
        else if constexpr (std::is_same_v<T, EQEffect>)      return "EQ Eight";
        else if constexpr (std::is_same_v<T, CompressorEffect>) return "Compressor";
        else return "Audio Effect Rack";
    }, params);
}

}  // anonymous namespace


Result<TimbreCompilationResult> compile_timbre_to_ableton(
    const TimbreProfile& profile,
    int track_index,
    LomTransport& transport
) {
    TimbreCompilationResult result;
    auto track = LomPaths::track(track_index);

    // Step 1: Load instrument device
    const auto& rc = profile.rendering;
    std::string device_name;

    if (rc.device_type.tag == DeviceTypeTag::NativeAbleton &&
        !rc.device_type.device_name.empty()) {
        device_name = rc.device_type.device_name;
    } else if (rc.device_type.tag == DeviceTypeTag::Plugin) {
        device_name = rc.device_type.plugin_identifier;
    } else {
        device_name = default_device_for_source(profile.source);
    }

    // Load device on track
    auto resp = transport.send(LomProtocol::call_method(
        track, "load_device", {device_name}));
    if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
    result.devices_created++;

    // Load preset if specified
    if (rc.preset_path) {
        resp = transport.send(LomProtocol::set_property(
            track.child("devices").child(0), "preset",
            *rc.preset_path));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
    }

    // Step 2: Apply parameter mappings
    for (const auto& [path, param] : rc.parameter_map) {
        auto device_path = track.child("devices").child(
            static_cast<int>(param.device_index));
        resp = transport.send(LomProtocol::set_property(
            device_path, param.parameter_name,
            static_cast<double>(param.range_min)));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        result.parameters_mapped++;
    }

    // Step 3: Insert effect chain
    int device_offset = 1;  // device 0 is the instrument
    for (const auto& effect : profile.insert_chain.effects) {
        if (!effect.enabled) continue;

        std::string effect_device = ableton_effect_name(effect.parameters);
        resp = transport.send(LomProtocol::call_method(
            track, "load_device", {effect_device}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        result.effects_inserted++;

        // Set dry/wet mix
        auto effect_path = track.child("devices").child(device_offset);
        resp = transport.send(LomProtocol::set_property(
            effect_path, "mix",
            static_cast<double>(effect.mix)));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);

        device_offset++;
    }

    // Step 4: Automation
    for (const auto& auto_lane : profile.parameter_automation) {
        resp = transport.send(LomProtocol::call_method(
            track, "create_automation_envelope",
            {auto_lane.parameter_path}));
        if (!resp.success) return std::unexpected(ErrorCode::SendFailed);
        result.automation_lanes++;
    }

    return result;
}

}  // namespace Sunny::Infrastructure::Format
