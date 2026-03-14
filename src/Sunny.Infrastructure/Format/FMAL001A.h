/**
 * @file FMAL001A.h
 * @brief Ableton Live Compiler — Score IR to LOM commands
 *
 * Component: FMAL001A
 * Domain: FM (Format) | Category: AL (Ableton Live)
 *
 * Compiles a validated Score IR document into a sequence of LOM bridge
 * commands that create an Ableton Live session: tracks, clips, notes,
 * tempo automation, and section markers.
 *
 * Uses CompiledMidi (SICM001A) for the musical content and translates
 * it to LOM operations via the abstract LomTransport (INTP001A).
 * Against CommandBuffer this is fully testable without Ableton;
 * against TcpTransport it drives a live session.
 *
 * Compilation steps (per Score IR Spec §9.2):
 *   1. Create one MIDI track per Part
 *   2. Create one clip per Part spanning the full score
 *   3. Inject notes from CompiledMidi into clips
 *   4. Write tempo automation to master
 *   5. Write time signature changes
 *   6. Create section markers from SectionMap
 *   7. Set pan and group assignments from RenderingConfig
 *
 * Precondition:  Score passes is_compilable() (no Error-level diagnostics)
 * Postcondition: All tracks, clips, and notes are created via transport
 */

#pragma once

#include "../Bridge/INTP001A.h"
#include "../../Sunny.Core/Score/SIDC001A.h"
#include "../../Sunny.Core/Score/SICM001A.h"

#include <string>
#include <vector>

namespace Sunny::Infrastructure::Format {

// =============================================================================
// Compilation result
// =============================================================================

/**
 * @brief Summary of an Ableton compilation
 */
struct AbletonCompilationResult {
    std::uint32_t tracks_created = 0;
    std::uint32_t clips_created = 0;
    std::uint32_t notes_written = 0;
    std::uint32_t tempo_events_written = 0;
    std::uint32_t markers_created = 0;
    Core::CompilationReport midi_report;
    std::vector<std::string> warnings;
};

// =============================================================================
// Compiler API
// =============================================================================

/**
 * @brief Compile a Score IR document to Ableton Live via LOM transport
 *
 * @param score     Validated Score IR document
 * @param transport LOM transport (CommandBuffer for testing, TcpTransport for live)
 * @param ppq       Pulses per quarter note (default 480)
 * @return Compilation result or error
 */
[[nodiscard]] Core::Result<AbletonCompilationResult> compile_to_ableton(
    const Core::Score& score,
    LomTransport& transport,
    int ppq = 480);

}  // namespace Sunny::Infrastructure::Format
