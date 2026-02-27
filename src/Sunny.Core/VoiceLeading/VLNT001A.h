/**
 * @file VLNT001A.h
 * @brief Nearest-tone voice leading algorithm
 *
 * Component: VLNT001A
 * Domain: VL (VoiceLeading) | Category: NT (Nearest Tone)
 *
 * Computes optimal voice leading by minimizing total voice motion.
 *
 * Invariants:
 * - len(result) == len(source_pitches)
 * - {p % 12 | p in result} == set(target_pitch_classes)
 * - Total motion is minimized subject to constraints
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"

#include <span>
#include <vector>

namespace Sunny::Core {

/**
 * @brief Result of voice leading computation
 */
struct VoiceLeadingResult {
    std::vector<MidiNote> voiced_notes;    ///< Output voicing
    int total_motion;                       ///< Sum of |source[i] - result[i]|
    bool has_parallel_fifths;               ///< Warning: parallel P5
    bool has_parallel_octaves;              ///< Warning: parallel P8
};

/**
 * @brief Compute optimal voice leading using nearest-tone algorithm
 *
 * For each voice, finds the closest pitch with the target pitch class,
 * minimizing total voice movement.
 *
 * @pre source_pitches.size() == target_pitch_classes.size()
 * @param source_pitches Current voicing as MIDI notes
 * @param target_pitch_classes Target chord as pitch classes
 * @param lock_bass If true, bass takes chord root
 * @param allow_parallel_fifths If true, don't avoid parallel P5
 * @param allow_parallel_octaves If true, don't avoid parallel P8
 * @return VoiceLeadingResult or error (VoiceLeadingFailed if sizes differ)
 */
[[nodiscard]] Result<VoiceLeadingResult> voice_lead_nearest_tone(
    std::span<const MidiNote> source_pitches,
    std::span<const PitchClass> target_pitch_classes,
    bool lock_bass = false,
    bool allow_parallel_fifths = false,
    bool allow_parallel_octaves = false
);

/**
 * @brief Generate close voicing (all notes within one octave)
 *
 * @param pitch_classes Pitch classes to voice
 * @param root_octave Octave for the root
 * @return MIDI notes in close position
 */
[[nodiscard]] std::vector<MidiNote> generate_close_voicing(
    std::span<const PitchClass> pitch_classes,
    int root_octave = 4
);

/**
 * @brief Generate drop-2 voicing
 *
 * Drops the second voice from the top down an octave.
 *
 * @pre close_voicing.size() >= 4
 * @param close_voicing Close position voicing (ascending)
 * @return Drop-2 voicing or error (VoiceLeadingFailed if fewer than 4 notes)
 */
[[nodiscard]] Result<std::vector<MidiNote>> generate_drop2_voicing(
    std::span<const MidiNote> close_voicing
);

/**
 * @brief Generate drop-3 voicing
 *
 * Drops the third voice from the top down an octave.
 *
 * @pre close_voicing.size() >= 4
 * @param close_voicing Close position voicing (ascending)
 * @return Drop-3 voicing or error (VoiceLeadingFailed if fewer than 4 notes)
 */
[[nodiscard]] Result<std::vector<MidiNote>> generate_drop3_voicing(
    std::span<const MidiNote> close_voicing
);

/**
 * @brief Generate open voicing (tones span > one octave)
 *
 * Formal Spec §7.5: Alternates voices between lower and upper registers
 * by dropping every other voice down an octave from close position.
 *
 * @param close_voicing Close position voicing (ascending)
 * @return Open voicing
 */
[[nodiscard]] std::vector<MidiNote> generate_open_voicing(
    std::span<const MidiNote> close_voicing
);

/**
 * @brief Generate drop-2+4 voicing
 *
 * Formal Spec §7.5: Drops both 2nd and 4th voices from the top
 * down an octave. Requires at least 4 notes.
 *
 * @pre close_voicing.size() >= 4
 * @param close_voicing Close position voicing (ascending)
 * @return Drop 2+4 voicing or error (VoiceLeadingFailed if fewer than 4 notes)
 */
[[nodiscard]] Result<std::vector<MidiNote>> generate_drop24_voicing(
    std::span<const MidiNote> close_voicing
);

/**
 * @brief Generate spread voicing
 *
 * Formal Spec §7.5: Bass note separated by at least an octave
 * from the upper structure. Upper voices remain in close position.
 *
 * @pre close_voicing.size() >= 3
 * @param close_voicing Close position voicing (ascending)
 * @return Spread voicing or error (VoiceLeadingFailed if fewer than 3 notes)
 */
[[nodiscard]] Result<std::vector<MidiNote>> generate_spread_voicing(
    std::span<const MidiNote> close_voicing
);

/**
 * @brief Voice motion type between two voices (§7.3)
 */
enum class VoiceMotionType {
    Parallel,   ///< Same direction, same displacement
    Similar,    ///< Same direction, different displacement
    Contrary,   ///< Opposite directions
    Oblique,    ///< One voice stationary, the other moves
    Static      ///< Both voices stationary
};

/**
 * @brief Classify motion type between two voice pairs (§7.3)
 *
 * @param a1 First voice, previous pitch
 * @param a2 First voice, next pitch
 * @param b1 Second voice, previous pitch
 * @param b2 Second voice, next pitch
 * @return VoiceMotionType classification
 */
[[nodiscard]] VoiceMotionType classify_voice_motion(
    MidiNote a1, MidiNote a2,
    MidiNote b1, MidiNote b2
);

/**
 * @brief Compute optimal voice leading using Hungarian algorithm (§7.2)
 *
 * Finds the minimum-cost assignment (bijection) between source pitches
 * and target pitch classes. Produces provably optimal total motion,
 * unlike the greedy nearest-tone approximation.
 *
 * @pre source_pitches.size() == target_pitch_classes.size()
 * @param source_pitches Current voicing as MIDI notes
 * @param target_pitch_classes Target chord as pitch classes
 * @param lock_bass If true, bass takes chord root
 * @return VoiceLeadingResult or error (VoiceLeadingFailed if sizes differ)
 */
[[nodiscard]] Result<VoiceLeadingResult> voice_lead_optimal(
    std::span<const MidiNote> source_pitches,
    std::span<const PitchClass> target_pitch_classes,
    bool lock_bass = false
);

/**
 * @brief Check for parallel motion between two voice pairs
 *
 * @param prev_bass Previous bass note
 * @param prev_upper Previous upper note
 * @param next_bass Next bass note
 * @param next_upper Next upper note
 * @param interval_class Interval class to check (7=P5, 0=P8)
 * @return true if parallel motion at given interval
 */
[[nodiscard]] bool has_parallel_motion(
    MidiNote prev_bass,
    MidiNote prev_upper,
    MidiNote next_bass,
    MidiNote next_upper,
    int interval_class
);

}  // namespace Sunny::Core
