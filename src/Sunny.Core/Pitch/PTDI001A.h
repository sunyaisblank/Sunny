/**
 * @file PTDI001A.h
 * @brief Diatonic interval type and operations
 *
 * Component: PTDI001A
 * Domain: PT (Pitch) | Category: DI (Diatonic Interval)
 *
 * Represents intervals as (chromatic, diatonic) pairs, enabling
 * interval quality (major, minor, perfect, augmented, diminished)
 * and correct enharmonic-aware arithmetic.
 *
 * Invariants:
 * - interval_add is associative and has identity PERFECT_UNISON
 * - interval_invert(i) + i == PERFECT_OCTAVE for simple ascending intervals
 * - apply_interval(sp, interval_from_spelled_pitches(sp, sp2)) == sp2 (round-trip)
 */

#pragma once

#include "PTSP001A.h"

#include <array>
#include <cassert>
#include <string>
#include <string_view>

namespace Sunny::Core {

// =============================================================================
// DiatonicInterval Type
// =============================================================================

/**
 * @brief Interval with quality information preserved
 *
 * The (chromatic, diatonic) pair uniquely determines interval quality.
 * chromatic = semitone displacement (Z), diatonic = letter-name displacement (Z).
 */
struct DiatonicInterval {
    int chromatic;   ///< Semitone displacement (unbounded)
    int diatonic;    ///< Letter-name displacement (unbounded)

    [[nodiscard]] constexpr bool operator==(const DiatonicInterval&) const noexcept = default;
};

// =============================================================================
// Constants
// =============================================================================

/// Default chromatic size for each diatonic interval within one octave
constexpr std::array<int, 7> DEFAULT_CHROMATIC_SIZE = {0, 2, 4, 5, 7, 9, 11};

/// Named interval constants
constexpr DiatonicInterval PERFECT_UNISON   = {0, 0};
constexpr DiatonicInterval MINOR_SECOND     = {1, 1};
constexpr DiatonicInterval MAJOR_SECOND     = {2, 1};
constexpr DiatonicInterval MINOR_THIRD      = {3, 2};
constexpr DiatonicInterval MAJOR_THIRD      = {4, 2};
constexpr DiatonicInterval PERFECT_FOURTH   = {5, 3};
constexpr DiatonicInterval AUGMENTED_FOURTH = {6, 3};
constexpr DiatonicInterval DIMINISHED_FIFTH = {6, 4};
constexpr DiatonicInterval PERFECT_FIFTH    = {7, 4};
constexpr DiatonicInterval MINOR_SIXTH      = {8, 5};
constexpr DiatonicInterval MAJOR_SIXTH      = {9, 5};
constexpr DiatonicInterval MINOR_SEVENTH    = {10, 6};
constexpr DiatonicInterval MAJOR_SEVENTH    = {11, 6};
constexpr DiatonicInterval PERFECT_OCTAVE   = {12, 7};

// =============================================================================
// Constexpr Operations
// =============================================================================

/**
 * @brief Interval number (1-based, absolute value)
 *
 * Unison = 1, second = 2, ..., octave = 8.
 *
 * @param di Diatonic interval
 * @return Interval number (always >= 1)
 */
[[nodiscard]] constexpr int interval_number(DiatonicInterval di) noexcept {
    int d = di.diatonic < 0 ? -di.diatonic : di.diatonic;
    return d + 1;
}

/**
 * @brief Test whether interval is simple (within one octave)
 */
[[nodiscard]] constexpr bool is_simple(DiatonicInterval di) noexcept {
    int d = di.diatonic < 0 ? -di.diatonic : di.diatonic;
    return d <= 6;
}

/**
 * @brief Test whether interval is compound (spans more than one octave)
 */
[[nodiscard]] constexpr bool is_compound(DiatonicInterval di) noexcept {
    int d = di.diatonic < 0 ? -di.diatonic : di.diatonic;
    return d >= 7;
}

/**
 * @brief Test whether a diatonic class belongs to the perfect family
 *
 * Perfect family: unison (0), fourth (3), fifth (4).
 * Imperfect family: second (1), third (2), sixth (5), seventh (6).
 *
 * @param d_mod7 Diatonic interval mod 7, in [0, 6]
 * @return true if the interval class uses P/A/d qualities
 */
[[nodiscard]] constexpr bool is_perfect_family(int d_mod7) noexcept {
    return d_mod7 == 0 || d_mod7 == 3 || d_mod7 == 4;
}

/**
 * @brief Default chromatic size for a given diatonic displacement
 *
 * Extends the one-octave table to arbitrary diatonic values using
 * octave periodicity: delta(d) = 12*(d/7) + delta_table(d%7).
 * Symmetric for negative values: delta(-d) = -delta(d).
 *
 * @param diatonic Diatonic displacement (Z)
 * @return Default chromatic size
 */
[[nodiscard]] constexpr int default_chromatic_size(int diatonic) noexcept {
    if (diatonic >= 0) {
        int octaves = diatonic / 7;
        int remainder = diatonic % 7;
        return 12 * octaves + DEFAULT_CHROMATIC_SIZE[remainder];
    } else {
        int pos = -diatonic;
        int octaves = pos / 7;
        int remainder = pos % 7;
        return -(12 * octaves + DEFAULT_CHROMATIC_SIZE[remainder]);
    }
}

/**
 * @brief Deviation from the default chromatic size
 *
 * For perfect-family intervals: 0 = perfect, +1 = augmented, -1 = diminished.
 * For imperfect-family intervals: 0 = major, -1 = minor, +1 = augmented, -2 = diminished.
 *
 * @param di Diatonic interval
 * @return Deviation in semitones
 */
[[nodiscard]] constexpr int deviation(DiatonicInterval di) noexcept {
    return di.chromatic - default_chromatic_size(di.diatonic);
}

/**
 * @brief Add two diatonic intervals
 *
 * Component-wise addition: (c1+c2, d1+d2).
 */
[[nodiscard]] constexpr DiatonicInterval interval_add(
    DiatonicInterval a,
    DiatonicInterval b
) noexcept {
    return {a.chromatic + b.chromatic, a.diatonic + b.diatonic};
}

/**
 * @brief Negate a diatonic interval
 */
[[nodiscard]] constexpr DiatonicInterval interval_negate(DiatonicInterval di) noexcept {
    return {-di.chromatic, -di.diatonic};
}

/**
 * @brief Invert a simple interval within the octave
 *
 * For a simple ascending interval i, invert(i) + i == PERFECT_OCTAVE.
 */
[[nodiscard]] constexpr DiatonicInterval interval_invert(DiatonicInterval di) noexcept {
    return {12 - di.chromatic, 7 - di.diatonic};
}

/**
 * @brief operator+ for interval addition
 */
[[nodiscard]] constexpr DiatonicInterval operator+(
    DiatonicInterval a,
    DiatonicInterval b
) noexcept {
    return interval_add(a, b);
}

/**
 * @brief Compute diatonic interval between two spelled pitches
 *
 * @param from Source pitch
 * @param to Destination pitch
 * @return DiatonicInterval from -> to
 */
[[nodiscard]] constexpr DiatonicInterval interval_from_spelled_pitches(
    SpelledPitch from,
    SpelledPitch to
) noexcept {
    int d = (static_cast<int>(to.letter) - static_cast<int>(from.letter))
            + 7 * (to.octave - from.octave);
    int c = midi_value(to) - midi_value(from);
    return {c, d};
}

/**
 * @brief Apply a diatonic interval to a spelled pitch
 *
 * Computes the target letter and octave from diatonic displacement,
 * then derives the accidental from the chromatic displacement.
 *
 * @param sp Source pitch
 * @param di Interval to apply
 * @return Resulting spelled pitch
 */
[[nodiscard]] constexpr SpelledPitch apply_interval(
    SpelledPitch sp,
    DiatonicInterval di
) noexcept {
    int letter_sum = static_cast<int>(sp.letter) + di.diatonic;

    // Floor division for letter -> octave mapping
    int octave_offset;
    int new_letter;
    if (letter_sum >= 0) {
        octave_offset = letter_sum / 7;
        new_letter = letter_sum % 7;
    } else {
        octave_offset = (letter_sum - 6) / 7;
        new_letter = ((letter_sum % 7) + 7) % 7;
    }

    assert(sp.octave + octave_offset >= -128 && sp.octave + octave_offset <= 127
        && "apply_interval: octave overflow");
    int8_t new_octave = static_cast<int8_t>(sp.octave + octave_offset);
    int target_midi = midi_value(sp) + di.chromatic;
    int natural_midi = 12 * (new_octave + 1) + static_cast<int>(nat(static_cast<uint8_t>(new_letter)));
    assert(target_midi - natural_midi >= -128 && target_midi - natural_midi <= 127
        && "apply_interval: accidental overflow");
    int8_t new_acc = static_cast<int8_t>(target_midi - natural_midi);

    return SpelledPitch{
        static_cast<uint8_t>(new_letter),
        new_acc,
        new_octave
    };
}

// =============================================================================
// Non-constexpr Functions (declared, defined in PTDI001A.cpp)
// =============================================================================

/**
 * @brief Quality string for an interval
 *
 * Returns: "P" (perfect), "M" (major), "m" (minor),
 * "A"/"AA"/... (augmented), "d"/"dd"/... (diminished).
 *
 * @param di Diatonic interval
 * @return Quality string
 */
[[nodiscard]] std::string quality(DiatonicInterval di);

/**
 * @brief Abbreviated quality + number string
 *
 * Examples: "P5", "m3", "M7", "A4", "d5"
 *
 * @param di Diatonic interval
 * @return Abbreviated string
 */
[[nodiscard]] std::string quality_abbreviation(DiatonicInterval di);

/**
 * @brief Parse quality-and-number string to DiatonicInterval
 *
 * Accepts: quality prefix + interval number (e.g. "P5", "m3", "M7", "A4", "d5")
 *
 * @param s Input string
 * @return DiatonicInterval or error
 */
[[nodiscard]] Result<DiatonicInterval> from_quality_and_number(std::string_view s);

}  // namespace Sunny::Core
