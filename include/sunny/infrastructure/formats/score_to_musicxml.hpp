/**
 * @file score_to_musicxml.hpp
 * @brief MusicXML compilation from Score IR
 *
 *
 * Compiles a Score IR document to a complete MusicXML score-partwise
 * XML string. Walks the Score hierarchy (Part -> Measure -> Voice -> Event)
 * and emits MusicXML elements for notes, rests, chords, articulations,
 * dynamics, directions, hairpins, ties, slurs, tuplets, and transposition.
 *
 * Precondition: is_compilable(score) — no Error-level validation diagnostics.
 *
 * Invariants:
 * - Output is well-formed MusicXML 4.0 from the richer Score-export profile
 * - parse_musicxml accepts only the sequential intersection of this profile;
 *   multi-voice/staff output legitimately uses cursor elements outside that reader
 * - SpelledPitch enharmonic spelling is preserved
 * - Unsupported elements are recorded as diagnostics, not silently dropped
 */

#pragma once

#include <string>
#include <sunny/core/score/midi_compiler.hpp>

namespace sunny::infrastructure::formats {

struct MusicXmlCompilationResult {
    std::string xml;
    sunny::core::CompilationReport report;
};

[[nodiscard]] sunny::core::Result<MusicXmlCompilationResult>
compile_score_to_musicxml(const sunny::core::Score& score);

} // namespace sunny::infrastructure::formats
