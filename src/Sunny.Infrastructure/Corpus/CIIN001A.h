/**
 * @file CIIN001A.h
 * @brief Corpus IR ingestion pipeline — MIDI and MusicXML to IngestedWork
 *
 * Component: CIIN001A
 * Domain: CI (Corpus IR) | Category: IN (Ingestion)
 *
 * Transforms raw file data (MIDI binary or MusicXML text) into a fully
 * populated IngestedWork with an embedded Score, analysis, and ingestion
 * confidence metrics.
 *
 * Pipeline stages:
 *   1. Parse — FMMI001A (MIDI) or FMMX001A (MusicXML)
 *   2. Key estimation — Krumhansl-Schmuckler on PC histogram
 *   3. Quantisation — snap ticks to nearest grid division
 *   4. Voice separation — register-based assignment
 *   5. Pitch spelling — context-aware enharmonic choice
 *   6. Score construction — build Score from processed data
 *   7. Analysis — CIAN001A decomposition
 *
 * Precondition:  Non-empty input data in valid MIDI or MusicXML format
 * Postcondition: IngestedWork with analysis_complete == true,
 *                populated IngestionConfidence, and embedded Score
 */

#pragma once

#include "Corpus/CIDC001A.h"

#include <cstdint>
#include <span>
#include <string_view>

namespace Sunny::Infrastructure::Corpus {

struct IngestionOptions {
    std::string title;
    std::string instrumentation = "Piano";
    int quantise_grid = 16;  // snap to 1/16 notes
};

[[nodiscard]] Sunny::Core::Result<Sunny::Core::IngestedWork> ingest_midi(
    std::span<const std::uint8_t> midi_data,
    Sunny::Core::IngestedWorkId work_id,
    const IngestionOptions& options);

[[nodiscard]] Sunny::Core::Result<Sunny::Core::IngestedWork> ingest_musicxml(
    std::string_view musicxml,
    Sunny::Core::IngestedWorkId work_id,
    const IngestionOptions& options);

}  // namespace Sunny::Infrastructure::Corpus
