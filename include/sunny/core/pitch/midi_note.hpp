/**
 * @file midi_note.hpp
 * @brief MIDI note utilities
 *
 *
 * Conversion between MIDI notes, pitch classes, and octaves.
 *
 * Invariants:
 * - pitch_octave_to_midi(pitch_class(midi), octave(midi)) == midi
 * - midi_to_pitch_octave(pitch_octave_to_midi(pc, oct)) == (pc, oct)
 */

#pragma once

#include <optional>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/types/music_types.hpp>
#include <utility>

namespace sunny::core {

/**
 * @brief Decompose MIDI note into pitch class and octave
 *
 * @param midi MIDI note number
 * @return Pair of (pitch_class, octave) where C4 = MIDI 60
 */
[[nodiscard]] constexpr std::pair<PitchClass, int> midi_to_pitch_octave(MidiNote midi) noexcept {
    return {PitchClass::wrapped(midi), (midi / 12) - 1};
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
[[nodiscard]] constexpr std::optional<MidiNote> pitch_octave_to_midi(PitchClass pc,
                                                                     int octave) noexcept {
    auto midi = MidiNote::from_int((octave + 1) * 12 + pc);
    if (!midi) {
        return std::nullopt;
    }
    return *midi;
}

/**
 * @brief Find closest MIDI note with target pitch class
 *
 * @param reference Reference MIDI note
 * @param target_pc Target pitch class [0, 11]
 * @return Closest MIDI note with the target pitch class
 */
[[nodiscard]] constexpr MidiNote closest_pitch_class_midi(MidiNote reference,
                                                          PitchClass target_pc) noexcept {
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

    // A valid candidate always exists for reference ∈ [0, 127], so the
    // fallback to the reference note is unreachable; it preserves the
    // MidiNote invariant without an unchecked construction path.
    return MidiNote::from_int(best).value_or(reference);
}

/**
 * @brief Transpose MIDI note by interval
 *
 * @param midi MIDI note
 * @param interval Semitones (can be negative)
 * @return Transposed note, or nullopt if out of range
 */
[[nodiscard]] constexpr std::optional<MidiNote> transpose_midi(MidiNote midi,
                                                               int interval) noexcept {
    auto result = MidiNote::from_int(midi + interval);
    if (!result) {
        return std::nullopt;
    }
    return *result;
}

} // namespace sunny::core
