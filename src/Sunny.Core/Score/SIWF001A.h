/**
 * @file SIWF001A.h
 * @brief Score IR composition workflow functions
 *
 * Component: SIWF001A
 * Domain: SI (Score IR) | Category: WF (Workflow)
 *
 * Provides entry-point functions for agent-driven composition workflows
 * per spec §12.2. These functions create or structurally reshape scores
 * at the formal level, complementing SIQR001A (read-only queries) and
 * SIMT001A (atomic mutations on existing scores).
 *
 * Functions:
 * - create_score: build a valid Score from a compact specification
 * - set_formal_plan: assign section structure to a score
 * - set_section_harmony: write harmonic annotations into a region
 *
 * Invariants:
 * - create_score output passes validate_structural with no Error diagnostics
 * - set_formal_plan and set_section_harmony are undoable
 */

#pragma once

#include "SIDC001A.h"
#include "SIMT001A.h"

namespace Sunny::Core {

// =============================================================================
// Score Creation (§12.2.1)
// =============================================================================

/**
 * @brief Compact specification for creating a new score
 */
struct ScoreSpec {
    std::string title;
    std::uint32_t total_bars;
    double bpm;
    SpelledPitch key_root;
    bool minor = false;
    std::int8_t key_accidentals = 0;
    int time_sig_num = 4;
    int time_sig_den = 4;
    std::vector<PartDefinition> parts;
};

/**
 * @brief Build a valid Score from a compact specification
 *
 * Constructs tempo, key, and time signature maps from the spec fields,
 * creates parts with empty measures (whole-measure rests), assigns
 * sequential IDs, and validates the result.
 *
 * @param spec Score specification
 * @return Valid Score or error if validation fails
 */
[[nodiscard]] Result<Score> create_score(const ScoreSpec& spec);

// =============================================================================
// Formal Plan (§12.2.2)
// =============================================================================

/**
 * @brief Definition of a single formal section
 */
struct SectionDefinition {
    std::string label;
    std::uint32_t start_bar;
    std::uint32_t end_bar;
    std::optional<FormFunction> function;
};

/**
 * @brief Replace the section map with a formal plan
 *
 * Captures the old section map for undo, builds ScoreSection entries
 * from the definitions, and replaces score.section_map.
 *
 * @param score Target score (modified in place)
 * @param sections Section definitions
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_formal_plan(
    Score& score,
    std::vector<SectionDefinition> sections,
    UndoStack* undo = nullptr
);

// =============================================================================
// Section Harmony (§12.2.3)
// =============================================================================

/**
 * @brief A chord symbol at a specific position
 */
struct ChordSymbolEntry {
    ScoreTime position;
    SpelledPitch root;
    std::string quality;
    std::optional<SpelledPitch> bass;
};

/**
 * @brief Write harmonic annotations into a region
 *
 * For each entry, creates a HarmonicAnnotation with the chord voicing
 * derived from the root and quality. Captures old annotations in the
 * region for undo.
 *
 * @param score Target score (modified in place)
 * @param region Region to annotate
 * @param progression Chord progression entries
 * @param undo Optional undo stack
 * @return MutationResult or error
 */
[[nodiscard]] Result<MutationResult> set_section_harmony(
    Score& score,
    const ScoreRegion& region,
    std::vector<ChordSymbolEntry> progression,
    UndoStack* undo = nullptr
);

}  // namespace Sunny::Core
