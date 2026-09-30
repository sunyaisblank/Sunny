/**
 * @file just_intonation.hpp
 * @brief Just intonation
 *
 *
 * Formal Spec §13.2: Just intonation intervals as integer frequency
 * ratios. Provides 5-limit interval table and comma definitions.
 *
 * Invariants:
 * - ji_ratio(0)->numerator == 1 && ji_ratio(0)->denominator == 1
 * - ji_cents(0) == 0.0
 * - All ratios have gcd(num, den) == 1
 * - comma_syntonic ≈ 21.5 cents
 */

#pragma once

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <sunny/core/tuning/equal_temperament.hpp>

namespace sunny::core {

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
    {1, 1},   // 0:  Unison
    {16, 15}, // 1:  Minor second
    {9, 8},   // 2:  Major second
    {6, 5},   // 3:  Minor third
    {5, 4},   // 4:  Major third
    {4, 3},   // 5:  Perfect fourth
    {45, 32}, // 6:  Tritone (augmented fourth)
    {3, 2},   // 7:  Perfect fifth
    {8, 5},   // 8:  Minor sixth
    {5, 3},   // 9:  Major sixth
    {9, 5},   // 10: Minor seventh
    {15, 8},  // 11: Major seventh
    {2, 1},   // 12: Octave
}};

/**
 * @brief Get the 5-limit JI ratio for a semitone index
 *
 * @param semitones Semitone offset (0–12)
 * @return JI ratio, or InvalidJIRatio if out of range
 */
[[nodiscard]] constexpr Result<JIRatio> ji_ratio(int semitones) noexcept {
    if (semitones < 0 || semitones > 12) return std::unexpected(ErrorCode::InvalidJIRatio);
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
    if (!r) return std::unexpected(r.error());
    return r->to_cents();
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
[[nodiscard]] inline Result<double> ji_frequency(double root_freq, JIRatio ratio) {
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
 * Cross-cancels before multiplication and uses portable checked arithmetic.
 *
 * @param a First ratio
 * @param b Second ratio
 * @return Reduced product, or InvalidJIRatio on overflow
 */
[[nodiscard]] inline Result<JIRatio> ji_multiply(JIRatio a, JIRatio b) {
    if (a.denominator == 0 || b.denominator == 0) {
        return std::unexpected(ErrorCode::InvalidJIRatio);
    }

    const auto magnitude = [](std::int64_t value) noexcept -> std::uint64_t {
        if (value >= 0) return static_cast<std::uint64_t>(value);
        return static_cast<std::uint64_t>(-(value + 1)) + 1;
    };
    const auto gcd = [](std::uint64_t lhs, std::uint64_t rhs) noexcept {
        while (rhs != 0) {
            const auto remainder = lhs % rhs;
            lhs = rhs;
            rhs = remainder;
        }
        return lhs;
    };
    const auto divide_exact = [](std::int64_t value, std::uint64_t divisor) noexcept {
        constexpr auto min_magnitude = std::uint64_t{1} << 63;
        if (divisor == min_magnitude) return std::int64_t{-1};
        return value / static_cast<std::int64_t>(divisor);
    };
    const auto checked_multiply = [](std::int64_t lhs,
                                     std::int64_t rhs) -> std::optional<std::int64_t> {
        constexpr auto min = std::numeric_limits<std::int64_t>::min();
        constexpr auto max = std::numeric_limits<std::int64_t>::max();
        if (lhs == 0 || rhs == 0) return 0;
        if ((lhs > 0 && rhs > 0 && lhs > max / rhs) || (lhs > 0 && rhs < 0 && rhs < min / lhs) ||
            (lhs < 0 && rhs > 0 && lhs < min / rhs) || (lhs < 0 && rhs < 0 && lhs < max / rhs)) {
            return std::nullopt;
        }
        return lhs * rhs;
    };

    // Cancel factors across the multiplication boundary before multiplying.
    const auto first_cancel = gcd(magnitude(a.numerator), magnitude(b.denominator));
    const auto second_cancel = gcd(magnitude(b.numerator), magnitude(a.denominator));
    a.numerator = divide_exact(a.numerator, first_cancel);
    b.denominator = divide_exact(b.denominator, first_cancel);
    b.numerator = divide_exact(b.numerator, second_cancel);
    a.denominator = divide_exact(a.denominator, second_cancel);

    auto numerator = checked_multiply(a.numerator, b.numerator);
    auto denominator = checked_multiply(a.denominator, b.denominator);
    if (!numerator || !denominator || *denominator == 0) {
        return std::unexpected(ErrorCode::InvalidJIRatio);
    }

    const auto final_gcd = gcd(magnitude(*numerator), magnitude(*denominator));
    return JIRatio{divide_exact(*numerator, final_gcd), divide_exact(*denominator, final_gcd)};
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

} // namespace sunny::core
