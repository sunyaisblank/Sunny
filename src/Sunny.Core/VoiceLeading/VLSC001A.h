/**
 * @file VLSC001A.h
 * @brief Species Counterpoint Constraint Checker
 *
 * Component: VLSC001A
 * Domain: VL (VoiceLeading) | Category: SC (Species Counterpoint)
 *
 * Formal Spec §7.6: Species counterpoint (after Fux) defines five
 * progressive constraint sets for composing a counterpoint voice
 * against a given cantus firmus.
 *
 * This component validates counterpoint against species-specific rules.
 * It does not generate counterpoint (that is a heuristic/search problem);
 * it checks whether a given counterpoint satisfies the constraints.
 *
 * Species:
 *   1 — Note against note (all consonances, no parallels)
 *   2 — Two notes against one (strong-beat consonance, passing tones)
 *   3 — Four notes against one (consonant downbeat, neighbours allowed)
 *   4 — Syncopation (suspensions resolve down by step)
 *   5 — Florid (combines all species)
 *
 * Invariants:
 * - A cantus firmus note paired with itself at the unison is valid in species 1
 * - Parallel P5/P8 detection is consistent with VLCN001A
 * - Species 4 suspensions must resolve downward by step
 */

#pragma once

#include "../Tensor/TNTP001A.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Types
// =============================================================================

/**
 * @brief Species number (1–5)
 */
enum class Species : uint8_t {
    First  = 1,
    Second = 2,
    Third  = 3,
    Fourth = 4,
    Fifth  = 5
};

/**
 * @brief Which voice carries the counterpoint
 */
enum class CounterpointPosition {
    Above,  ///< Counterpoint is above the cantus firmus
    Below   ///< Counterpoint is below the cantus firmus
};

/**
 * @brief A single species counterpoint violation
 */
struct SpeciesViolation {
    int measure;           ///< Measure index (0-based)
    int beat;              ///< Beat within measure (0-based)
    std::string rule;      ///< Short rule identifier
    std::string message;   ///< Human-readable description
};

/**
 * @brief Result of species counterpoint validation
 */
struct SpeciesCheckResult {
    bool valid;                            ///< True if no violations
    std::vector<SpeciesViolation> violations;
};

// =============================================================================
// Interval Classification
// =============================================================================

/**
 * @brief Check if an interval (in semitones) is a perfect consonance
 *
 * Perfect consonances: P1 (0), P5 (7), P8 (12), and their octave compounds.
 */
[[nodiscard]] constexpr bool is_perfect_consonance(int semitones) noexcept {
    int reduced = semitones % 12;
    if (reduced < 0) reduced += 12;
    return reduced == 0 || reduced == 7;
}

/**
 * @brief Check if an interval (in semitones) is an imperfect consonance
 *
 * Imperfect consonances: m3 (3), M3 (4), m6 (8), M6 (9).
 */
[[nodiscard]] constexpr bool is_imperfect_consonance(int semitones) noexcept {
    int reduced = semitones % 12;
    if (reduced < 0) reduced += 12;
    return reduced == 3 || reduced == 4 || reduced == 8 || reduced == 9;
}

/**
 * @brief Check if an interval is consonant (perfect or imperfect)
 */
[[nodiscard]] constexpr bool is_consonant(int semitones) noexcept {
    return is_perfect_consonance(semitones) || is_imperfect_consonance(semitones);
}

/**
 * @brief Check if a melodic interval is a step (1 or 2 semitones)
 */
[[nodiscard]] constexpr bool is_step(int semitones) noexcept {
    int abs_val = semitones < 0 ? -semitones : semitones;
    return abs_val == 1 || abs_val == 2;
}

// =============================================================================
// Species Validation
// =============================================================================

/**
 * @brief Validate first species counterpoint
 *
 * One counterpoint note per cantus firmus note.
 * All vertical intervals must be consonant.
 *
 * @param cantus Cantus firmus as MIDI notes (one per measure)
 * @param counterpoint Counterpoint voice as MIDI notes (one per measure)
 * @param position Whether counterpoint is above or below
 * @return Validation result with any violations
 */
[[nodiscard]] SpeciesCheckResult check_first_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position = CounterpointPosition::Above
);

/**
 * @brief Validate second species counterpoint
 *
 * Two counterpoint notes per cantus firmus note.
 * Strong beats must be consonant; weak beats may pass through dissonance by step.
 *
 * @param cantus Cantus firmus (one note per measure)
 * @param counterpoint Counterpoint (two notes per measure, interleaved: [m0_beat1, m0_beat2, m1_beat1, ...])
 * @param position Whether counterpoint is above or below
 * @return Validation result
 */
[[nodiscard]] SpeciesCheckResult check_second_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position = CounterpointPosition::Above
);

/**
 * @brief Validate third species counterpoint
 *
 * Four counterpoint notes per cantus firmus note.
 * First beat consonant; beats 2–4 may be passing/neighbour tones.
 *
 * @param cantus Cantus firmus (one note per measure)
 * @param counterpoint Counterpoint (four notes per measure)
 * @param position Whether counterpoint is above or below
 * @return Validation result
 */
[[nodiscard]] SpeciesCheckResult check_third_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position = CounterpointPosition::Above
);

/**
 * @brief Validate fourth species counterpoint (syncopation)
 *
 * Counterpoint is displaced by half a measure.
 * Two notes per measure: [held_from_prev, preparation_for_next].
 * Dissonant suspensions on strong beats must resolve down by step.
 *
 * @param cantus Cantus firmus (one note per measure)
 * @param counterpoint Counterpoint (two notes per measure)
 * @param position Whether counterpoint is above or below
 * @return Validation result
 */
[[nodiscard]] SpeciesCheckResult check_fourth_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position = CounterpointPosition::Above
);

/**
 * @brief Validate fifth species (florid) counterpoint
 *
 * Combines elements of all four preceding species.
 * Uses a simplified rule set: consonance on strong beats,
 * stepwise dissonance on weak beats, proper suspension handling.
 *
 * @param cantus Cantus firmus (one note per measure)
 * @param counterpoint Counterpoint (variable notes per measure, 1–4 per measure)
 * @param notes_per_measure How many counterpoint notes in each measure
 * @param position Whether counterpoint is above or below
 * @return Validation result
 */
[[nodiscard]] SpeciesCheckResult check_fifth_species(
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    std::span<const int> notes_per_measure,
    CounterpointPosition position = CounterpointPosition::Above
);

/**
 * @brief Dispatch to the appropriate species checker
 *
 * For species 1–3, counterpoint length must be species_number * cantus length.
 * For species 4, counterpoint length must be 2 * cantus length.
 * For species 5, use check_fifth_species directly (needs notes_per_measure).
 *
 * @param species Which species to check (1–4; species 5 requires notes_per_measure)
 * @param cantus Cantus firmus
 * @param counterpoint Counterpoint voice
 * @param position Whether counterpoint is above or below
 * @return Validation result, or error if lengths mismatch
 */
[[nodiscard]] Result<SpeciesCheckResult> check_species(
    Species species,
    std::span<const MidiNote> cantus,
    std::span<const MidiNote> counterpoint,
    CounterpointPosition position = CounterpointPosition::Above
);

}  // namespace Sunny::Core
