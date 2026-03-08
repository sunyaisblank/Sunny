/**
 * @file FMLY002A.h
 * @brief LilyPond compilation from Score IR
 *
 * Component: FMLY002A
 * Domain: FM (Format) | Category: LY (LilyPond)
 *
 * Compiles a Score IR document to a complete LilyPond (.ly) file string.
 * Uses FMLY001A primitives (ly_pitch, ly_duration, ly_note, ly_rest,
 * ly_chord, ly_key, ly_time_signature) for element-level conversion.
 *
 * Precondition: is_compilable(score) — no Error-level validation diagnostics.
 *
 * Invariants:
 * - Output starts with \version
 * - Output contains syntactically valid LilyPond notation
 * - Non-representable durations produce diagnostics, not errors
 */

#pragma once

#include "../../Sunny.Core/Score/SICM001A.h"

#include <string>

namespace Sunny::Infrastructure::Format {

struct LilyPondCompilationResult {
    std::string ly;
    Sunny::Core::CompilationReport report;
};

[[nodiscard]] Sunny::Core::Result<LilyPondCompilationResult>
compile_score_to_lilypond(const Sunny::Core::Score& score);

}  // namespace Sunny::Infrastructure::Format
