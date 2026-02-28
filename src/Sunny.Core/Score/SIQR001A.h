/**
 * @file SIQR001A.h
 * @brief Score IR agent interface queries — read-only score interrogation
 *
 * Component: SIQR001A
 * Domain: SI (Score IR) | Category: QR (Query)
 *
 * Provides ~20 query functions per spec §12.1 for agent-driven
 * composition workflows. All functions are read-only and return
 * copies or views of score data.
 *
 * Invariants:
 * - No query modifies the Score
 * - Queries return empty/nullopt for out-of-range positions
 */

#pragma once

#include "SIDC001A.h"
#include "SIVD001A.h"

#include <utility>

namespace Sunny::Core {

/**
 * @brief Collect all note events within a region
 *
 * Returns event pointers and their absolute positions.
 */
struct LocatedEvent {
    ScoreTime position;
    PartId part_id;
    const Event* event;
};

[[nodiscard]] std::vector<LocatedEvent> query_notes_in_range(
    const Score& score,
    const ScoreRegion& region
);

/**
 * @brief Find the harmonic annotation active at a given time
 */
[[nodiscard]] std::optional<HarmonicAnnotation> query_harmony_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Find the tempo event active at a given time
 *
 * Returns std::nullopt when the tempo map is empty.
 */
[[nodiscard]] std::optional<TempoEvent> query_tempo_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Find the key signature active at a given time
 *
 * Returns std::nullopt when the key map is empty.
 */
[[nodiscard]] std::optional<KeySignature> query_key_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Find the dynamic level active at a given time for a part
 */
[[nodiscard]] std::optional<DynamicLevel> query_dynamics_at(
    const Score& score,
    PartId part_id,
    ScoreTime time
);

/**
 * @brief List parts that have note content at a given time
 */
[[nodiscard]] std::vector<PartId> query_parts_playing_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Find the texture type at a given time (from orchestration layer)
 */
[[nodiscard]] std::optional<TextureType> query_texture_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Count voices in a specific bar and part
 */
[[nodiscard]] std::uint8_t query_voice_count(
    const Score& score,
    std::uint32_t bar,
    PartId part_id
);

/**
 * @brief Find pitch extremes for a part in a region
 *
 * Returns {lowest, highest} SpelledPitch pair.
 */
[[nodiscard]] std::optional<std::pair<SpelledPitch, SpelledPitch>> query_pitch_range(
    const Score& score,
    PartId part_id,
    const ScoreRegion& region
);

/**
 * @brief Total notated duration of events in a part within a region
 */
[[nodiscard]] Beat query_duration_total(
    const Score& score,
    PartId part_id,
    const ScoreRegion& region
);

/**
 * @brief Note density (notes per bar) in a region across all parts
 */
[[nodiscard]] double query_note_density(
    const Score& score,
    const ScoreRegion& region
);

/**
 * @brief Find the section active at a given time
 */
[[nodiscard]] std::optional<ScoreSection> query_section_at(
    const Score& score,
    ScoreTime time
);

/**
 * @brief Run full validation and return all diagnostics
 */
[[nodiscard]] std::vector<Diagnostic> query_diagnostics(const Score& score);

// =============================================================================
// Extended Queries (§12.1)
// =============================================================================

/**
 * @brief Return all harmonic annotations overlapping a time range
 */
[[nodiscard]] std::vector<HarmonicAnnotation> query_harmony_range(
    const Score& score,
    ScoreTime start,
    ScoreTime end
);

/**
 * @brief Pitched note with position context for melody extraction
 */
struct MelodyNote {
    SpelledPitch pitch;
    Beat duration;
    ScoreTime position;
};

/**
 * @brief Extract the melody line for a part within a region
 */
[[nodiscard]] std::vector<MelodyNote> query_melody_for(
    const Score& score,
    PartId part_id,
    const ScoreRegion& region
);

/**
 * @brief Chord with position context for progression extraction
 */
struct ChordEntry {
    ScoreTime position;
    Beat duration;
    ChordVoicing chord;
    std::string roman_numeral;
};

/**
 * @brief Extract the chord progression within a region
 */
[[nodiscard]] std::vector<ChordEntry> query_chord_progression(
    const Score& score,
    const ScoreRegion& region
);

/**
 * @brief Return orchestration annotations overlapping a region
 */
[[nodiscard]] std::vector<std::pair<PartId, TexturalRole>> query_orchestration(
    const Score& score,
    const ScoreRegion& region
);

/**
 * @brief Find all parts assigned a given textural role within a region
 */
[[nodiscard]] std::vector<PartId> query_parts_with_role(
    const Score& score,
    TexturalRole role,
    const ScoreRegion& region
);

/**
 * @brief Collect deduplicated, sorted pitch classes within a region
 */
[[nodiscard]] std::vector<PitchClass> query_pitch_content(
    const Score& score,
    const ScoreRegion& region
);

/**
 * @brief Return the time signature active at a given bar
 */
[[nodiscard]] TimeSignature query_time_signature_at(
    const Score& score,
    std::uint32_t bar
);

/**
 * @brief Return the full section hierarchy of the score
 */
[[nodiscard]] std::vector<ScoreSection> query_sections(const Score& score);

/**
 * @brief Condensed summary of each section's properties
 */
struct FormSummaryEntry {
    std::string label;
    ScoreTime start;
    ScoreTime end;
    KeySignature key;
    double tempo_bpm;
};

/**
 * @brief Produce a form summary for the score
 */
[[nodiscard]] std::vector<FormSummaryEntry> query_form_summary(const Score& score);

/**
 * @brief Occurrence of a pitch-class motif pattern
 */
struct MotifOccurrence {
    PartId part_id;
    ScoreTime position;
};

/**
 * @brief Find occurrences of a pitch-class motif pattern within a region
 */
[[nodiscard]] std::vector<MotifOccurrence> query_find_motif(
    const Score& score,
    const std::vector<PitchClass>& pattern,
    const ScoreRegion& region
);

// is_compilable already declared in SIVD001A.h

}  // namespace Sunny::Core
