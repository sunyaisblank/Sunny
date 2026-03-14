/**
 * @file FMAL002A.h
 * @brief Ableton Live Compiler — Timbre IR to device chains
 *
 * Component: FMAL002A
 * Domain: FM (Format) | Category: AL (Ableton Live)
 *
 * Compiles a validated TimbreProfile into LOM commands that create
 * an Ableton instrument device chain on the specified track:
 *   1. Load instrument device (native Ableton or plugin)
 *   2. Map parameters from TimbreRenderingConfig
 *   3. Insert EffectChain as audio effects
 *   4. Write automation lanes from TimbreAutomation
 *
 * Precondition:  TimbreProfile passes validation (no Error diagnostics)
 * Postcondition: Track has instrument + effects configured via transport
 */

#pragma once

#include "../Bridge/INTP001A.h"
#include "../../Sunny.Core/Timbre/TIDC001A.h"

#include <string>
#include <vector>

namespace Sunny::Infrastructure::Format {

struct TimbreCompilationResult {
    std::uint32_t devices_created = 0;
    std::uint32_t effects_inserted = 0;
    std::uint32_t parameters_mapped = 0;
    std::uint32_t automation_lanes = 0;
    std::vector<std::string> warnings;
};

/**
 * @brief Compile a TimbreProfile to an Ableton track via LOM transport
 *
 * @param profile   Validated TimbreProfile
 * @param track_index  Ableton track index to configure
 * @param transport LOM transport (CommandBuffer for testing)
 * @return Compilation result or error
 */
[[nodiscard]] Core::Result<TimbreCompilationResult> compile_timbre_to_ableton(
    const Core::TimbreProfile& profile,
    int track_index,
    LomTransport& transport);

}  // namespace Sunny::Infrastructure::Format
