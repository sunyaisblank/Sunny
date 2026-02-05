/**
 * @file PTMN001A.h
 * @brief MIDI note utilities
 *
 * Component: PTMN001A
 * Domain: PT (Pitch) | Category: MN (MIDI Note)
 *
 * Conversion between MIDI notes, pitch classes, and octaves.
 *
 * Invariants:
 * - pitch_octave_to_midi(pitch_class(midi), octave(midi)) == midi
 * - midi_to_pitch_octave(pitch_octave_to_midi(pc, oct)) == (pc, oct)
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "PTPC001A.h"

#include <optional>
#include <utility>

namespace Sunny::Core {

/**
 * @brief Decompose MIDI note into pitch class and octave
 *
 * @param midi MIDI note number
 * @return Pair of (pitch_class, octave) where C4 = MIDI 60
 */
[[nodiscard]] constexpr std::pair<PitchClass, int> midi_to_pitch_octave(
    MidiNote midi
) noexcept {
    return {midi % 12, (midi / 12) - 1};
}

/**
 * @brief Get octave from MIDI note
 *
 * @param midi MIDI note number
 * @return Octave number (C4 = 4)
 */
[[nodiscard]] constexpr int midi_octave(MidiNote midi) noexcept {
    return (midi / 12) - 1;
}

/**
 * @brief Construct MIDI note from pitch class and octave
 *
 * @param pc Pitch class [0, 11]
 * @param octave Octave number (C4 = 4)
 * @return MIDI note number, or nullopt if out of range
 */
[[nodiscard]] constexpr std::optional<MidiNote> pitch_octave_to_midi(
    PitchClass pc,
    int octave
) noexcept {
    int midi = (octave + 1) * 12 + pc;
    if (midi < 0 || midi > 127) {
        return std::nullopt;
    }
    return static_cast<MidiNote>(midi);
}

/**
 * @brief Construct MIDI note (unchecked version)
 *
 * @pre octave in [-1, 9] and result in [0, 127]
 */
[[nodiscard]] constexpr MidiNote pitch_octave_to_midi_unchecked(
    PitchClass pc,
    int octave
) noexcept {
    return static_cast<MidiNote>((octave + 1) * 12 + pc);
}

/**
 * @brief Find closest MIDI note with target pitch class
 *
 * @param reference Reference MIDI note
 * @param target_pc Target pitch class [0, 11]
 * @return Closest MIDI note with the target pitch class
 */
[[nodiscard]] constexpr MidiNote closest_pitch_class_midi(
    MidiNote reference,
    PitchClass target_pc
) noexcept {
    int ref_octave = reference / 12;

    int candidates[3] = {
        ref_octave * 12 + target_pc,
        (ref_octave - 1) * 12 + target_pc,
        (ref_octave + 1) * 12 + target_pc,
    };

    int best = candidates[0];
    int best_dist = 127;

    for (int c : candidates) {
        if (c >= 0 && c <= 127) {
            int dist = c > reference ? c - reference : reference - c;
            if (dist < best_dist) {
                best_dist = dist;
                best = c;
            }
        }
    }

    return static_cast<MidiNote>(best);
}

/**
 * @brief Transpose MIDI note by interval
 *
 * @param midi MIDI note
 * @param interval Semitones (can be negative)
 * @return Transposed note, or nullopt if out of range
 */
[[nodiscard]] constexpr std::optional<MidiNote> transpose_midi(
    MidiNote midi,
    int interval
) noexcept {
    int result = midi + interval;
    if (result < 0 || result > 127) {
        return std::nullopt;
    }
    return static_cast<MidiNote>(result);
}

}  // namespace Sunny::Core
