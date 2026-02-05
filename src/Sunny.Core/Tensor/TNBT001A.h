/**
 * @file TNBT001A.h
 * @brief Exact beat representation using rational numbers
 *
 * Component: TNBT001A
 * Domain: TN (Tensor) | Category: BT (Beat)
 *
 * All timing calculations use rational arithmetic to avoid floating-point
 * precision issues. The final conversion to audio samples happens at the
 * last possible moment.
 *
 * Invariants:
 * - denominator > 0 (always positive)
 * - Rational operations are exact (no precision loss)
 * - Comparison uses cross-multiplication (no floating-point)
 */

#pragma once

#include <cstdint>
#include <numeric>

namespace Sunny::Core {

/**
 * @brief Exact beat representation using rational numbers
 */
struct Beat {
    std::int64_t numerator;
    std::int64_t denominator;

    /**
     * @brief Convert to floating-point (for final output only)
     */
    [[nodiscard]] constexpr double to_float() const noexcept {
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    }

    /**
     * @brief Create from floating-point with bounded denominator
     */
    [[nodiscard]] static constexpr Beat from_float(
        double beats,
        std::int64_t max_denom = 10000
    ) noexcept {
        std::int64_t num = static_cast<std::int64_t>(beats * max_denom);
        return Beat{num, max_denom};
    }

    [[nodiscard]] static constexpr Beat zero() noexcept { return Beat{0, 1}; }
    [[nodiscard]] static constexpr Beat one() noexcept { return Beat{1, 1}; }

    /**
     * @brief Reduce to lowest terms
     */
    [[nodiscard]] constexpr Beat reduce() const noexcept {
        if (numerator == 0) return Beat{0, 1};
        std::int64_t g = std::gcd(numerator, denominator);
        return Beat{numerator / g, denominator / g};
    }

    // Comparison operators (exact, no floating-point)
    [[nodiscard]] constexpr bool operator==(const Beat& other) const noexcept {
        return numerator * other.denominator == other.numerator * denominator;
    }

    [[nodiscard]] constexpr bool operator<(const Beat& other) const noexcept {
        return numerator * other.denominator < other.numerator * denominator;
    }

    [[nodiscard]] constexpr bool operator<=(const Beat& other) const noexcept {
        return numerator * other.denominator <= other.numerator * denominator;
    }

    [[nodiscard]] constexpr bool operator>(const Beat& other) const noexcept {
        return numerator * other.denominator > other.numerator * denominator;
    }

    [[nodiscard]] constexpr bool operator>=(const Beat& other) const noexcept {
        return numerator * other.denominator >= other.numerator * denominator;
    }

    // Arithmetic operators
    [[nodiscard]] constexpr Beat operator+(const Beat& other) const noexcept {
        return Beat{
            numerator * other.denominator + other.numerator * denominator,
            denominator * other.denominator
        };
    }

    [[nodiscard]] constexpr Beat operator-(const Beat& other) const noexcept {
        return Beat{
            numerator * other.denominator - other.numerator * denominator,
            denominator * other.denominator
        };
    }

    [[nodiscard]] constexpr Beat operator*(std::int64_t scalar) const noexcept {
        return Beat{numerator * scalar, denominator};
    }

    [[nodiscard]] constexpr Beat operator/(std::int64_t scalar) const noexcept {
        return Beat{numerator, denominator * scalar};
    }
};

/**
 * @brief Compute LCM of two beat durations (exact)
 *
 * @param a First duration
 * @param b Second duration
 * @return LCM as a Beat
 */
[[nodiscard]] constexpr Beat beat_lcm(Beat a, Beat b) noexcept {
    // LCM of fractions: lcm(a/b, c/d) = lcm(a,c) / gcd(b,d)
    std::int64_t num_gcd = std::gcd(a.numerator, b.numerator);
    std::int64_t num_lcm = (a.numerator / num_gcd) * b.numerator;
    std::int64_t den_gcd = std::gcd(a.denominator, b.denominator);
    return Beat{num_lcm, den_gcd};
}

}  // namespace Sunny::Core
