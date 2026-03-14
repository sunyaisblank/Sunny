/**
 * @file FMAL003A.cpp
 * @brief Ableton Live Compiler — Mix IR to mixer infrastructure
 *
 * Component: FMAL003A
 *
 * Maps MixGraph to Ableton mixer:
 *   ChannelStrip     → Track mixer (volume, pan, mute, inserts)
 *   GroupBus         → Group Track
 *   AuxBus           → Return Track
 *   MasterBus        → Master Track effects
 *   MixAutomation    → Automation lanes
 *
 * Device mapping (defaults to Ableton native):
 *   MixEQ            → EQ Eight
 *   MixCompressor    → Compressor
 *   MixGate          → Gate
 *   MixLimiter       → Limiter
 *   MixMultibandDynamics → Multiband Dynamics
 *   MixSaturation    → Saturator
 *   MixStereoProcessor → Utility
 */

#include "FMAL003A.h"

namespace Sunny::Infrastructure::Format {

using namespace Sunny::Core;

namespace {

/// Map MixEffect variant to Ableton device name
std::string device_for_effect(const MixEffectParameters& params) {
    return std::visit([](const auto& p) -> std::string {
        using T = std::decay_t<decltype(p)>;
        if constexpr (std::is_same_v<T, MixEQ>)                 return "EQ Eight";
        else if constexpr (std::is_same_v<T, MixCompressor>)    return "Compressor";
        else if constexpr (std::is_same_v<T, MixGate>)          return "Gate";
        else if constexpr (std::is_same_v<T, MixLimiter>)       return "Limiter";
        else if constexpr (std::is_same_v<T, MixMultibandDynamics>) return "Multiband Dynamics";
        else if constexpr (std::is_same_v<T, MixSaturation>)    return "Saturator";
        else if constexpr (std::is_same_v<T, MixStereoProcessor>) return "Utility";
        else return "Audio Effect Rack";
    }, params);
}

/// Insert an effect chain on a track path, return count of effects inserted
std::uint32_t insert_effect_chain(
    const MixEffectChain& chain,
    const LomPath& track_path,
    LomTransport& transport
) {
    std::uint32_t count = 0;
    for (const auto& effect : chain.effects) {
        if (!effect.enabled) continue;
        std::string device = device_for_effect(effect.parameters);
        transport.send(LomProtocol::call_method(
            track_path, "load_device", {device}));
        count++;
    }
    return count;
}

}  // anonymous namespace


Result<MixCompilationResult> compile_mix_to_ableton(
    const MixGraph& graph,
    int base_track,
    LomTransport& transport
) {
    MixCompilationResult result;

    // Track index allocation:
    // base_track .. base_track + channels.size() - 1 : channel tracks (already created by Score compiler)
    // Group tracks and return tracks are created by this compiler.

    int next_group_track = base_track + static_cast<int>(graph.channels.size());
    int next_return_track = 0;

    // Step 1: Create group tracks
    for (const auto& group : graph.group_buses) {
        transport.send(LomProtocol::call_method(
            LomPaths::song(), "create_group_track",
            {next_group_track}));
        transport.send(LomProtocol::set_property(
            LomPaths::track(next_group_track), "name",
            group.name));
        result.group_tracks_created++;
        next_group_track++;
    }

    // Step 2: Create return tracks
    for (const auto& aux : graph.aux_buses) {
        transport.send(LomProtocol::call_method(
            LomPaths::song(), "create_return_track", {}));
        transport.send(LomProtocol::set_property(
            LomPath{{"song", "return_tracks", std::to_string(next_return_track)}},
            "name", aux.name));

        // Insert aux effect chain
        result.effects_inserted += insert_effect_chain(
            aux.effect_chain,
            LomPath{{"song", "return_tracks", std::to_string(next_return_track)}},
            transport);

        // Set return level
        transport.send(LomProtocol::set_property(
            LomPath{{"song", "return_tracks", std::to_string(next_return_track)}},
            "volume", static_cast<double>(aux.return_level)));

        result.return_tracks_created++;
        next_return_track++;
    }

    // Step 3: Configure channel strips
    for (std::size_t i = 0; i < graph.channels.size(); ++i) {
        const auto& ch = graph.channels[i];
        int track_idx = base_track + static_cast<int>(i);
        auto track_path = LomPaths::track(track_idx);

        // Insert effects
        result.effects_inserted += insert_effect_chain(
            ch.insert_chain, track_path, transport);

        // Set fader level
        transport.send(LomProtocol::set_property(
            track_path, "volume",
            static_cast<double>(ch.fader.level_db)));

        // Set pan
        transport.send(LomProtocol::set_property(
            track_path, "panning",
            static_cast<double>(ch.spatial.pan)));

        // Set mute
        if (ch.mute) {
            transport.send(LomProtocol::set_property(
                track_path, "mute", true));
        }

        // Set solo
        if (ch.solo) {
            transport.send(LomProtocol::set_property(
                track_path, "solo", true));
        }

        // Configure sends
        for (std::size_t si = 0; si < ch.sends.size(); ++si) {
            const auto& send = ch.sends[si];
            if (!send.enabled) continue;
            auto send_path = track_path.child("mixer").child("sends").child(
                static_cast<int>(si));
            transport.send(LomProtocol::set_property(
                send_path, "value",
                static_cast<double>(send.level_db)));
            result.sends_configured++;
        }

        result.channels_configured++;
    }

    // Step 4: Configure group bus effect chains
    int group_offset = base_track + static_cast<int>(graph.channels.size());
    for (std::size_t gi = 0; gi < graph.group_buses.size(); ++gi) {
        const auto& group = graph.group_buses[gi];
        int group_idx = group_offset + static_cast<int>(gi);
        auto group_path = LomPaths::track(group_idx);

        result.effects_inserted += insert_effect_chain(
            group.insert_chain, group_path, transport);

        // Set group fader
        transport.send(LomProtocol::set_property(
            group_path, "volume",
            static_cast<double>(group.fader.level_db)));
    }

    // Step 5: Master bus processing
    auto master_path = LomPath{{"song", "master_track"}};
    result.effects_inserted += insert_effect_chain(
        graph.master_bus.insert_chain, master_path, transport);

    // Set master fader
    transport.send(LomProtocol::set_property(
        master_path, "volume",
        static_cast<double>(graph.master_bus.fader.level_db)));

    // Step 6: Automation
    for (const auto& auto_entry : graph.automation) {
        transport.send(LomProtocol::call_method(
            LomPaths::song(), "create_automation_envelope",
            {auto_entry.target}));
        result.automation_lanes++;
    }

    return result;
}

}  // namespace Sunny::Infrastructure::Format
