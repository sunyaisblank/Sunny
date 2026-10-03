/**
 * @file historical_temperament.hpp
 * @brief Historical temperaments
 *
 *
 * Formal Spec §13.3: Tuning tables giving cent deviations from
 * 12-TET for historical temperaments, generated from their chains of fifths.
 *
 * Invariants:
 * - tuning_table[i] for 12-TET is 0.0 for all i
 * - All tuning tables have exactly 12 entries (Z/12Z)
 * - tempered_frequency with all-zero table == edo_frequency
 */

#pragma once

#include <array>
#include <cmath>
#include <span>
#include <string_view>
#include <sunny/core/tuning/equal_temperament.hpp>
#include <sunny/core/types/music_types.hpp>

namespace sunny::core {

/// A tuning table: cent deviations from 12-TET for each pitch class
using TuningTable = std::array<double, 12>;

/**
 * @brief Named temperament with tuning table
 */
struct Temperament {
    std::string_view name;
    TuningTable table;
    std::string_view description;
};

// =============================================================================
// Tuning Tables (Formal Spec §13.3)
// =============================================================================

/// 12-TET: all deviations are zero
inline constexpr TuningTable TUNING_EQUAL = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

// Each historical temperament is defined by the sizes of its fifths, so each
// table is generated from its chain of fifths rather than transcribed. The
// three generator intervals, in cents, are the exact logarithms
//   pure fifth         1200 log2(3/2)
//   syntonic comma     1200 log2(81/80)
//   Pythagorean comma  1200 log2(3^12 / 2^19)
// written as literals because std::log2 is not constexpr in C++23.
inline constexpr double PURE_FIFTH_CENTS = 701.9550008653874;
inline constexpr double SYNTONIC_COMMA_CENTS = 21.50628959671478;
inline constexpr double PYTHAGOREAN_COMMA_CENTS = 23.460010384649014;

/**
 * @brief Build a tuning table from a chain of eleven fifths
 *
 * The chain begins on @p first_pitch_class and ascends by fifths; element k
 * of @p fifths is the size in cents of the fifth from chain note k to chain
 * note k+1. Chain note k lies 700k cents above the first in 12-TET, so its
 * deviation is the running sum of (fifth - 700). The twelfth fifth, from
 * the last chain note back to the first, closes the circle at 8400 cents and
 * absorbs the remainder: the wolf of a regular temperament, or the last
 * fifth of a circulating one. The result is shifted so that C deviates by 0.
 *
 * @pre 0 <= first_pitch_class < 12
 */
[[nodiscard]] constexpr TuningTable table_from_fifth_chain(int first_pitch_class,
                                                           const std::array<double, 11>& fifths) {
    TuningTable table{};
    double deviation = 0.0;
    int pc = first_pitch_class;
    table[static_cast<std::size_t>(pc)] = 0.0;
    for (double fifth : fifths) {
        deviation += fifth - 700.0;
        pc = (pc + 7) % 12;
        table[static_cast<std::size_t>(pc)] = deviation;
    }
    const double c_deviation = table[0];
    for (double& entry : table) {
        entry -= c_deviation;
    }
    return table;
}

namespace detail {

[[nodiscard]] constexpr std::array<double, 11> uniform_fifths(double size) {
    std::array<double, 11> fifths{};
    for (double& fifth : fifths) {
        fifth = size;
    }
    return fifths;
}

} // namespace detail

/// Pythagorean: eleven pure fifths on the chain Eb-Bb-F-C-G-D-A-E-B-F#-C#-G#;
/// the wolf (about 678.49 cents) falls on G#-Eb.
inline constexpr TuningTable TUNING_PYTHAGOREAN =
    table_from_fifth_chain(3, detail::uniform_fifths(PURE_FIFTH_CENTS));

/// Quarter-comma meantone: eleven fifths narrowed by a quarter syntonic comma
/// (about 696.58 cents) on the chain Eb..G#, so every major third within the
/// chain is a pure 5/4; the wolf (about 737.64 cents) falls on G#-Eb.
inline constexpr TuningTable TUNING_QUARTER_COMMA_MEANTONE = table_from_fifth_chain(
    3, detail::uniform_fifths(PURE_FIFTH_CENTS - SYNTONIC_COMMA_CENTS / 4.0));

/// Werckmeister III (1691): C-G, G-D, D-A and B-F# narrowed by a quarter
/// Pythagorean comma; the other eight fifths pure. Well-tempered: all keys
/// usable, character varies. The chain starts on C.
inline constexpr TuningTable TUNING_WERCKMEISTER_III =
    table_from_fifth_chain(0,
                           {
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 4.0, // C-G
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 4.0, // G-D
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 4.0, // D-A
                               PURE_FIFTH_CENTS,                                 // A-E
                               PURE_FIFTH_CENTS,                                 // E-B
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 4.0, // B-F#
                               PURE_FIFTH_CENTS,                                 // F#-C#
                               PURE_FIFTH_CENTS,                                 // C#-G#
                               PURE_FIFTH_CENTS,                                 // G#-Eb
                               PURE_FIFTH_CENTS,                                 // Eb-Bb
                               PURE_FIFTH_CENTS, // Bb-F (F-C closes the circle)
                           });

/// Vallotti (1754): F-C-G-D-A-E-B narrowed by a sixth of a Pythagorean comma
/// (about 698.04 cents); the other six fifths pure. The chain starts on F.
inline constexpr TuningTable TUNING_VALLOTTI =
    table_from_fifth_chain(5,
                           {
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // F-C
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // C-G
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // G-D
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // D-A
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // A-E
                               PURE_FIFTH_CENTS - PYTHAGOREAN_COMMA_CENTS / 6.0, // E-B
                               PURE_FIFTH_CENTS,                                 // B-F#
                               PURE_FIFTH_CENTS,                                 // F#-C#
                               PURE_FIFTH_CENTS,                                 // C#-G#
                               PURE_FIFTH_CENTS,                                 // G#-Eb
                               PURE_FIFTH_CENTS, // Eb-Bb (Bb-F closes the circle)
                           });

// =============================================================================
// Lookup and frequency calculation
// =============================================================================

/**
 * @brief Find a temperament by name
 *
 * @param name Temperament name (case-sensitive)
 * @return Temperament or TemperamentNotFound error
 */
[[nodiscard]] Result<Temperament> find_temperament(std::string_view name);

/**
 * @brief Compute frequency under a tuning table
 *
 * f(p, o) = f_ref · 2^((12·(o+1) + p - 69)/12 + τ(p)/1200)
 *
 * where τ(p) is the tuning table deviation in cents.
 *
 * @param pitch_class Pitch class (0–11)
 * @param octave MIDI octave
 * @param table Tuning table (cent deviations)
 * @param ref_freq Reference frequency (default A4=440)
 * @return A finite positive frequency in Hz, or InvalidFrequency when the reference, table,
 *         exponent, or result is outside that tractable domain
 */
[[nodiscard]] inline Result<double>
tempered_frequency(int pitch_class, int octave, const TuningTable& table, double ref_freq = 440.0) {
    if (pitch_class < 0 || pitch_class > 11) return std::unexpected(ErrorCode::InvalidPitchClass);
    if (!std::isfinite(ref_freq) || ref_freq <= 0.0 ||
        !std::isfinite(table[static_cast<std::size_t>(pitch_class)]))
        return std::unexpected(ErrorCode::InvalidFrequency);

    // Compute in double so extreme octave inputs cannot overflow an intermediate integer.
    const double midi = 12.0 * static_cast<double>(octave) + static_cast<double>(pitch_class) +
                        12.0; // C-1 = MIDI 0
    const double exponent =
        (midi - 69.0) / 12.0 + table[static_cast<std::size_t>(pitch_class)] / 1200.0;
    const double frequency = ref_freq * std::exp2(exponent);
    if (!std::isfinite(exponent) || !std::isfinite(frequency) || frequency <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    return frequency;
}

/**
 * @brief List all available temperament names
 */
[[nodiscard]] std::span<const std::string_view> list_temperament_names();

} // namespace sunny::core
