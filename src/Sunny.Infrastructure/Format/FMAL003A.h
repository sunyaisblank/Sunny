/**
 * @file FMAL003A.h
 * @brief Ableton Live Compiler — Mix IR to mixer infrastructure
 *
 * Component: FMAL003A
 * Domain: FM (Format) | Category: AL (Ableton Live)
 *
 * Compiles a validated MixGraph into LOM commands that configure
 * Ableton's mixer: group tracks, return tracks, effect racks,
 * send levels, fader/pan/mute states, and automation.
 *
 * Compilation steps (Mix IR Spec §10):
 *   1. Create group tracks for GroupBuses
 *   2. Create return tracks for AuxBuses
 *   3. Insert effect chains on channels, groups, aux, and master
 *   4. Configure routing (sends, group assignments, sidechains)
 *   5. Set levels, pan, mute/solo
 *   6. Insert master processing chain
 *   7. Write automation lanes
 *
 * Precondition:  MixGraph passes validation (no Error diagnostics)
 * Postcondition: Mixer infrastructure created via transport
 */

#pragma once

#include "../Bridge/INTP001A.h"
#include "../../Sunny.Core/Mix/MIDC001A.h"

#include <string>
#include <vector>

namespace Sunny::Infrastructure::Format {

struct MixCompilationResult {
    std::uint32_t group_tracks_created = 0;
    std::uint32_t return_tracks_created = 0;
    std::uint32_t effects_inserted = 0;
    std::uint32_t sends_configured = 0;
    std::uint32_t automation_lanes = 0;
    std::uint32_t channels_configured = 0;
    std::vector<std::string> warnings;
};

/**
 * @brief Compile a MixGraph to Ableton mixer via LOM transport
 *
 * @param graph       Validated MixGraph
 * @param base_track  First track index (channels start here)
 * @param transport   LOM transport (CommandBuffer for testing)
 * @return Compilation result or error
 */
[[nodiscard]] Core::Result<MixCompilationResult> compile_mix_to_ableton(
    const Core::MixGraph& graph,
    int base_track,
    LomTransport& transport);

}  // namespace Sunny::Infrastructure::Format
