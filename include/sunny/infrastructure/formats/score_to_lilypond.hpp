/**
 * @file score_to_lilypond.hpp
 * @brief LilyPond compilation from Score IR
 *
 *
 * Compiles a Score IR document to a complete LilyPond (.ly) file string.
 * Uses  primitives (ly_pitch, ly_duration, ly_note, ly_rest,
 * ly_chord, ly_key, ly_time_signature) for element-level conversion.
 *
 * Precondition: is_compilable(score) — no Error-level validation diagnostics.
 *
 * Invariants:
 * - Output starts with \version
 * - Output contains syntactically valid LilyPond notation
 * - Every positive rational duration is retained exactly, using LilyPond
 *   duration multipliers when no conventional duration token exists
 * - Point metadata is positioned by a parallel exact spacer timeline
 * - Unsupported semantic distinctions remain visible and produce diagnostics
 */

#pragma once

#include <string>
#include <sunny/core/score/midi_compiler.hpp>

namespace sunny::infrastructure::formats {

struct LilyPondCompilationResult {
    std::string ly;
    sunny::core::CompilationReport report;
};

[[nodiscard]] sunny::core::Result<LilyPondCompilationResult>
compile_score_to_lilypond(const sunny::core::Score& score);

} // namespace sunny::infrastructure::formats
