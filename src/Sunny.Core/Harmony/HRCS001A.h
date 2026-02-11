/**
 * @file HRCS001A.h
 * @brief Chord-scale theory
 *
 * Component: HRCS001A
 * Domain: HR (Harmony) | Category: CS (Chord-Scale)
 *
 * Formal Spec §5.6: Determines available tensions and chord-scales
 * for chords in harmonic context.
 *
 * Invariants:
 * - All chord tones are members of the chord-scale
 * - An available tension t against chord tone c is avoided
 *   if (t - c) mod 12 == 1 (minor 9th interval)
 * - chord_scale_analysis.chord_tones ∪ available ∪ avoid == scale pitch classes
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Scale/SCDF001A.h"

#include <span>
#include <string_view>
#include <vector>

namespace Sunny::Core {

/**
 * @brief Result of chord-scale analysis
 *
 * Partitions scale tones into chord tones, available tensions,
 * and avoid notes based on the minor 9th avoidance rule.
 */
struct ChordScaleAnalysis {
    std::vector<PitchClass> chord_tones;  ///< Scale tones that are chord members
    std::vector<PitchClass> available;    ///< Tensions not creating minor 9th
    std::vector<PitchClass> avoid;        ///< Tensions creating minor 9th
};

/**
 * @brief Check if a tension creates a minor 9th against any chord tone
 *
 * Formal Spec §5.6: A tension t is avoided if (t - c) mod 12 == 1
 * for any chord tone c.
 *
 * @param tension Candidate tension pitch class
 * @param chord_tones Pitch classes of the chord
 * @return true if t creates a minor 9th against any chord tone
 */
[[nodiscard]] bool is_avoid_note(
    PitchClass tension,
    std::span<const PitchClass> chord_tones
);

/**
 * @brief Analyse a chord against a scale to find available tensions
 *
 * Partitions scale pitch classes into chord tones, available tensions,
 * and avoid notes per the minor 9th avoidance rule.
 *
 * @param chord_pcs Pitch classes of the chord
 * @param scale_root Root of the scale
 * @param scale_intervals Scale interval pattern from root
 * @return Analysis with chord tones, available tensions, and avoid notes
 */
[[nodiscard]] ChordScaleAnalysis analyze_chord_scale(
    std::span<const PitchClass> chord_pcs,
    PitchClass scale_root,
    std::span<const Interval> scale_intervals
);

/**
 * @brief Look up default chord-scale for a diatonic function
 *
 * Formal Spec §5.6 heuristic table. Returns the scale name for
 * common chord functions in jazz/common-practice harmony.
 *
 * Recognised function strings:
 *   "Imaj7", "ii7", "iii7", "IVmaj7", "V7", "vi7", "viiø7",
 *   "V7alt", "V7b9b13", "iiø7"
 *
 * @param chord_function Chord function string
 * @return Scale name (for use with find_scale) or nullopt
 */
[[nodiscard]] std::optional<std::string_view> chord_scale_for_function(
    std::string_view chord_function
);

/**
 * @brief Get default chord-scale name for a diatonic degree
 *
 * Returns the standard jazz chord-scale assignment for each
 * degree of the major or natural minor scale.
 *
 * Major: I→ionian, ii→dorian, iii→phrygian, IV→lydian,
 *        V→mixolydian, vi→aeolian, vii→locrian
 *
 * Minor: i→aeolian, ii°→locrian, III→major, iv→dorian,
 *        v→phrygian, VI→lydian, VII→mixolydian
 *
 * @param degree Scale degree (0-6)
 * @param is_major_key true for major, false for natural minor
 * @return Scale name or nullopt if degree is out of range
 */
[[nodiscard]] std::optional<std::string_view> default_chord_scale(
    int degree, bool is_major_key = true
);

}  // namespace Sunny::Core
