/**
 * @file ingestion.hpp
 * @brief Corpus IR ingestion pipeline — MIDI and MusicXML to IngestedWork
 *
 *
 * Transforms raw file data (MIDI binary or MusicXML text) into a fully
 * populated IngestedWork with an embedded Score, analysis, and ingestion
 * confidence metrics.
 *
 * Pipeline stages:
 *   1. Parse —  (MIDI) or  (MusicXML)
 *   2. Key estimation — Krumhansl-Schmuckler on PC histogram
 *   3. Quantisation — snap ticks to nearest grid division
 *   4. Voice separation — register-based assignment
 *   5. Pitch spelling — context-aware enharmonic choice
 *   6. Channel state — preserve one note channel and complete CC64/CC67 spans
 *   7. Loss accounting — retain unsupported event/topology facts as corrections
 *   8. Score construction — build Score from processed data
 *   9. Analysis — decomposition
 *
 * Precondition:  Non-empty input data in valid MIDI or MusicXML format;
 *                MIDI quantise_grid must be positive
 * Postcondition: IngestedWork with analysis_complete == true,
 *                populated IngestionConfidence, and embedded Score
 */

#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <sunny/core/corpus/document.hpp>

namespace sunny::infrastructure::corpus {

struct IngestionOptions {
    std::string title;
    std::string instrumentation = "Piano";
    int quantise_grid = 16; // divisions per whole note; 16 is a sixteenth-note grid
};

[[nodiscard]] sunny::core::Result<sunny::core::IngestedWork>
ingest_midi(std::span<const std::uint8_t> midi_data,
            sunny::core::IngestedWorkId work_id,
            const IngestionOptions& options);

[[nodiscard]] sunny::core::Result<sunny::core::IngestedWork>
ingest_musicxml(std::string_view musicxml,
                sunny::core::IngestedWorkId work_id,
                const IngestionOptions& options);

} // namespace sunny::infrastructure::corpus
