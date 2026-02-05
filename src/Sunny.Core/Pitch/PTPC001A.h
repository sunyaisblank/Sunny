/**
 * @file PTPC001A.h
 * @brief Pitch class operations in Z/12Z
 *
 * Component: PTPC001A
 * Domain: PT (Pitch) | Category: PC (Pitch Class)
 *
 * Core operations on pitch classes using exact integer arithmetic.
 * The pitch class group is Z/12Z with operations T_n (transposition)
 * and I_n (inversion) generating the dihedral group D_12.
 *
 * Invariants:
 * - transpose(pc, 0) == pc (identity)
 * - transpose(transpose(pc, a), b) == transpose(pc, a + b) (associativity)
 * - transpose(pc, 12) == pc (periodicity)
 * - invert(invert(pc, axis), axis) == pc (self-inverse)
 */

#pragma once

#include "../Tensor/TNTP001A.h"

#include <array>
#include <string_view>

namespace Sunny::Core {

// =============================================================================
// Constants
// =============================================================================

/// Pitch class names (sharp preference)
constexpr std::array<std::string_view, 12> PITCH_CLASS_NAMES_SHARP = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

/// Pitch class names (flat preference)
constexpr std::array<std::string_view, 12> PITCH_CLASS_NAMES_FLAT = {
    "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
};

/// Interval names
constexpr std::array<std::string_view, 12> INTERVAL_NAMES = {
    "P1", "m2", "M2", "m3", "M3", "P4", "TT", "P5", "m6", "M6", "m7", "M7"
};

// =============================================================================
// Core Operations
// =============================================================================

/**
 * @brief Get pitch class from MIDI note
 *
 * Canonical projection from MIDI space to pitch class space.
 *
 * @param midi MIDI note number [0, 127]
 * @return Pitch class [0, 11]
 */
[[nodiscard]] constexpr PitchClass pitch_class(MidiNote midi) noexcept {
    return midi % 12;
}

/**
 * @brief Transpose pitch class by interval (T_n operation)
 *
 * Mathematical: T_n: Z/12Z -> Z/12Z, T_n(x) = (x + n) mod 12
 *
 * @param pc Pitch class [0, 11]
 * @param interval Transposition amount
 * @return Transposed pitch class [0, 11]
 */
[[nodiscard]] constexpr PitchClass transpose(PitchClass pc, int interval) noexcept {
    int result = (static_cast<int>(pc) + interval % 12 + 12) % 12;
    return static_cast<PitchClass>(result);
}

/**
 * @brief Invert pitch class around an axis (I_n operation)
 *
 * Mathematical: I_n: Z/12Z -> Z/12Z, I_n(x) = (2n - x) mod 12
 *
 * @param pc Pitch class [0, 11]
 * @param axis Inversion axis (default: 0)
 * @return Inverted pitch class [0, 11]
 */
[[nodiscard]] constexpr PitchClass invert(PitchClass pc, int axis = 0) noexcept {
    int result = (2 * axis - static_cast<int>(pc) % 12 + 24) % 12;
    return static_cast<PitchClass>(result);
}

/**
 * @brief Get interval class (reduce to [0, 6])
 *
 * @param semitones Raw interval in semitones
 * @return Interval class [0, 6]
 */
[[nodiscard]] constexpr int interval_class(int semitones) noexcept {
    int ic = (semitones % 12 + 12) % 12;
    return ic <= 6 ? ic : 12 - ic;
}

/**
 * @brief Get note name from pitch class
 *
 * @param pc Pitch class [0, 11]
 * @param prefer_flats Use flat names
 * @return Note name string view
 */
[[nodiscard]] constexpr std::string_view note_name(
    PitchClass pc,
    bool prefer_flats = false
) noexcept {
    return prefer_flats ? PITCH_CLASS_NAMES_FLAT[pc % 12]
                        : PITCH_CLASS_NAMES_SHARP[pc % 12];
}

/**
 * @brief Parse note name to pitch class
 *
 * @param name Note name (e.g., "C", "F#", "Bb")
 * @return Pitch class or error
 */
[[nodiscard]] Result<PitchClass> note_to_pitch_class(std::string_view name) noexcept;

}  // namespace Sunny::Core
