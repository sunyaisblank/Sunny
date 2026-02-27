/**
 * @file TUHT001A.h
 * @brief Historical temperaments
 *
 * Component: TUHT001A
 * Domain: TU (Tuning) | Category: HT (Historical Temperaments)
 *
 * Formal Spec §13.3: Tuning tables giving cent deviations from
 * 12-TET for historical temperaments.
 *
 * Invariants:
 * - tuning_table[i] for 12-TET is 0.0 for all i
 * - All tuning tables have exactly 12 entries (Z/12Z)
 * - tempered_frequency with all-zero table == edo_frequency
 */

#pragma once

#include "TUET001A.h"
#include "../Tensor/TNTP001A.h"

#include <array>
#include <cmath>
#include <span>
#include <string_view>

namespace Sunny::Core {

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
constexpr TuningTable TUNING_EQUAL = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

/// Pythagorean: pure fifths (3/2) except wolf between G#-Eb.
/// Starting from C, tuned by ascending fifths: C-G-D-A-E-B-F#-C#-G# and
/// descending fifths: C-F-Bb-Eb. The wolf falls on G#-Eb.
/// Deviations from 12-TET in cents (C = 0):
constexpr TuningTable TUNING_PYTHAGOREAN = {
    0.0,     // C
    -9.78,   // C#/Db
    3.91,    // D
    -5.87,   // Eb
    7.82,    // E
    -1.96,   // F
    11.73,   // F#
    1.96,    // G
    -7.82,   // Ab
    5.87,    // A
    -3.91,   // Bb
    9.78,    // B
};

/// Quarter-comma meantone: fifths narrowed by 1/4 syntonic comma (5.377 cents).
/// Pure major thirds (5/4). Wolf fifth on G#-Eb.
/// Deviations from 12-TET in cents (C = 0):
constexpr TuningTable TUNING_QUARTER_COMMA_MEANTONE = {
    0.0,     // C
    -24.04,  // C#
    6.84,    // D
    17.11,   // Eb
    -13.69,  // E
    3.42,    // F
    -20.53,  // F#
    -3.42,   // G
    13.69,   // Ab (≈ G#)
    10.26,   // A
    20.53,   // Bb
    -10.26,  // B
};

/// Werckmeister III (1691): 4 fifths narrowed by 1/4 Pythagorean comma.
/// Tempered fifths: C-G, G-D, D-A, B-F#. Rest pure.
/// Well-tempered: all keys usable, character varies.
/// Deviations from 12-TET in cents (C = 0):
constexpr TuningTable TUNING_WERCKMEISTER_III = {
    0.0,     // C
    -10.26,  // C#
    -7.82,   // D
    -5.87,   // Eb
    -13.69,  // E
    -1.96,   // F
    -11.73,  // F#
    -3.91,   // G
    -7.82,   // Ab
    -11.73,  // A
    -3.91,   // Bb
    -9.78,   // B
};

/// Vallotti (1754): 6 fifths tempered by 1/6 Pythagorean comma.
/// Tempered fifths: F-C-G-D-A-E-B. Rest pure.
/// Deviations from 12-TET in cents (C = 0):
constexpr TuningTable TUNING_VALLOTTI = {
    0.0,     // C
    -5.87,   // C#
    -1.96,   // D
    1.96,    // Eb
    -3.91,   // E
    3.91,    // F
    -1.96,   // F#
    -3.91,   // G
    -1.96,   // Ab
    -5.87,   // A
    1.96,    // Bb
    -7.82,   // B
};

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
 * f(p, o) = f_ref · 2^((12·o + p - 69 + τ(p)/1200))
 *
 * where τ(p) is the tuning table deviation in cents.
 *
 * @param pitch_class Pitch class (0–11)
 * @param octave MIDI octave
 * @param table Tuning table (cent deviations)
 * @param ref_freq Reference frequency (default A4=440)
 * @return Frequency in Hz
 */
[[nodiscard]] inline Result<double> tempered_frequency(
    int pitch_class,
    int octave,
    const TuningTable& table,
    double ref_freq = 440.0
) {
    if (pitch_class < 0 || pitch_class > 11)
        return std::unexpected(ErrorCode::InvalidPitchClass);
    int midi = 12 * octave + pitch_class + 12;  // MIDI note (C-1 = 0)
    return ref_freq * std::pow(2.0,
        (midi - 69.0) / 12.0 + table[pitch_class % 12] / 1200.0);
}

/**
 * @brief List all available temperament names
 */
[[nodiscard]] std::span<const std::string_view> list_temperament_names();

}  // namespace Sunny::Core
