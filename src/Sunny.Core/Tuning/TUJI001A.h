/**
 * @file TUJI001A.h
 * @brief Just intonation
 *
 * Component: TUJI001A
 * Domain: TU (Tuning) | Category: JI (Just Intonation)
 *
 * Formal Spec §13.2: Just intonation intervals as integer frequency
 * ratios. Provides 5-limit interval table and comma definitions.
 *
 * Invariants:
 * - ji_ratio(0).numerator == 1 && ji_ratio(0).denominator == 1
 * - ji_cents(0) == 0.0
 * - All ratios have gcd(num, den) == 1
 * - comma_syntonic ≈ 21.5 cents
 */

#pragma once

#include "TUET001A.h"

#include <array>
#include <climits>
#include <cstdint>
#include <cmath>
#include <numeric>

namespace Sunny::Core {

/**
 * @brief Rational frequency ratio (integer numerator/denominator)
 */
struct JIRatio {
    std::int64_t numerator;
    std::int64_t denominator;

    [[nodiscard]] Result<double> to_double() const {
        if (denominator == 0) return std::unexpected(ErrorCode::InvalidJIRatio);
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    }

    [[nodiscard]] Result<double> to_cents() const {
        auto d = to_double();
        if (!d) return std::unexpected(d.error());
        return ratio_to_cents(*d);
    }

    [[nodiscard]] constexpr bool operator==(const JIRatio&) const noexcept = default;
};

/**
 * @brief 5-limit just intonation interval table
 *
 * Formal Spec §13.2.1: 12 intervals plus unison, indexed by
 * semitone offset from root (0–12). All ratios are in 5-limit
 * (largest prime factor ≤ 5).
 */
constexpr std::array<JIRatio, 13> JI_5LIMIT_INTERVALS = {{
    {1, 1},      // 0:  Unison
    {16, 15},    // 1:  Minor second
    {9, 8},      // 2:  Major second
    {6, 5},      // 3:  Minor third
    {5, 4},      // 4:  Major third
    {4, 3},      // 5:  Perfect fourth
    {45, 32},    // 6:  Tritone (augmented fourth)
    {3, 2},      // 7:  Perfect fifth
    {8, 5},      // 8:  Minor sixth
    {5, 3},      // 9:  Major sixth
    {9, 5},      // 10: Minor seventh
    {15, 8},     // 11: Major seventh
    {2, 1},      // 12: Octave
}};

/**
 * @brief Get the 5-limit JI ratio for a semitone index
 *
 * @param semitones Semitone offset (0–12)
 * @return JI ratio, or {0,0} if out of range
 */
[[nodiscard]] constexpr JIRatio ji_ratio(int semitones) noexcept {
    if (semitones < 0 || semitones > 12) return {0, 0};
    return JI_5LIMIT_INTERVALS[semitones];
}

/**
 * @brief Get the cents value for a 5-limit JI interval
 *
 * @param semitones Semitone offset (0–12)
 * @return Cents value, or InvalidJIRatio if out of range
 */
[[nodiscard]] inline Result<double> ji_cents(int semitones) {
    auto r = ji_ratio(semitones);
    if (r.denominator == 0) return std::unexpected(ErrorCode::InvalidJIRatio);
    return r.to_cents();
}

/**
 * @brief Frequency for a JI pitch
 *
 * Computes the frequency by applying the JI ratio for the given
 * interval above a reference pitch.
 *
 * @param root_freq Root frequency in Hz
 * @param ratio JI ratio
 * @return Frequency in Hz, or InvalidJIRatio if denominator is zero
 */
[[nodiscard]] inline Result<double> ji_frequency(
    double root_freq,
    JIRatio ratio
) {
    auto d = ratio.to_double();
    if (!d) return std::unexpected(d.error());
    return root_freq * *d;
}

// =============================================================================
// Commas (§13.2.2)
// =============================================================================

/// Syntonic comma: 81/80 ≈ 21.5 cents (4 fifths vs 1 major third)
constexpr JIRatio COMMA_SYNTONIC = {81, 80};

/// Pythagorean comma: 531441/524288 ≈ 23.5 cents (12 fifths vs 7 octaves)
constexpr JIRatio COMMA_PYTHAGOREAN = {531441, 524288};

/// Diesis: 128/125 ≈ 41.1 cents (3 major thirds vs 1 octave)
constexpr JIRatio COMMA_DIESIS = {128, 125};

/// Schisma: 32805/32768 ≈ 2.0 cents (8 fifths + 1 major third vs 5 octaves)
constexpr JIRatio COMMA_SCHISMA = {32805, 32768};

/**
 * @brief Multiply two JI ratios and reduce
 *
 * Uses __int128 intermediate to detect overflow of int64_t.
 *
 * @param a First ratio
 * @param b Second ratio
 * @return Reduced product, or InvalidJIRatio on overflow
 */
[[nodiscard]] inline Result<JIRatio> ji_multiply(JIRatio a, JIRatio b) {
    __int128 num = static_cast<__int128>(a.numerator) * b.numerator;
    __int128 den = static_cast<__int128>(a.denominator) * b.denominator;
    if (den == 0) return std::unexpected(ErrorCode::InvalidJIRatio);
    // Reduce before range check to maximise representable products
    __int128 g = 1;
    {
        // GCD on __int128 via Euclidean algorithm
        __int128 x = num < 0 ? -num : num;
        __int128 y = den < 0 ? -den : den;
        while (y != 0) {
            __int128 t = y;
            y = x % y;
            x = t;
        }
        g = x;
    }
    num /= g;
    den /= g;
    if (num > INT64_MAX || num < INT64_MIN || den > INT64_MAX || den < INT64_MIN)
        return std::unexpected(ErrorCode::InvalidJIRatio);
    return JIRatio{static_cast<std::int64_t>(num), static_cast<std::int64_t>(den)};
}

/**
 * @brief Divide two JI ratios and reduce
 *
 * @param a Dividend
 * @param b Divisor
 * @return Reduced quotient, or InvalidJIRatio on overflow or zero divisor
 */
[[nodiscard]] inline Result<JIRatio> ji_divide(JIRatio a, JIRatio b) {
    if (b.numerator == 0) return std::unexpected(ErrorCode::InvalidJIRatio);
    return ji_multiply(a, {b.denominator, b.numerator});
}

}  // namespace Sunny::Core
