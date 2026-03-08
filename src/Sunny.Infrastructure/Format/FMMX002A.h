/**
 * @file FMMX002A.h
 * @brief MusicXML compilation from Score IR
 *
 * Component: FMMX002A
 * Domain: FM (Format) | Category: MX (MusicXML)
 *
 * Compiles a Score IR document to a complete MusicXML score-partwise
 * XML string. Walks the Score hierarchy (Part -> Measure -> Voice -> Event)
 * and emits MusicXML elements for notes, rests, chords, articulations,
 * dynamics, directions, hairpins, ties, slurs, tuplets, and transposition.
 *
 * Precondition: is_compilable(score) — no Error-level validation diagnostics.
 *
 * Invariants:
 * - Output is well-formed MusicXML parseable by parse_musicxml
 * - SpelledPitch enharmonic spelling is preserved
 * - Unsupported elements are recorded as diagnostics, not silently dropped
 */

#pragma once

#include "../../Sunny.Core/Score/SICM001A.h"

#include <string>

namespace Sunny::Infrastructure::Format {

struct MusicXmlCompilationResult {
    std::string xml;
    Sunny::Core::CompilationReport report;
};

[[nodiscard]] Sunny::Core::Result<MusicXmlCompilationResult>
compile_score_to_musicxml(const Sunny::Core::Score& score);

}  // namespace Sunny::Infrastructure::Format
