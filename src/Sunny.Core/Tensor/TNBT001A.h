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
 * - denominator > 0 (always positive; enforced by normalise)
 * - Stored in lowest terms after every operation (prevents overflow)
 * - Rational operations are exact (no precision loss)
 * - Comparison uses cross-multiplication (no floating-point)
 *
 * Formal Spec §9.1: Beat value is element of Q (rationals), represented
 * as irreducible fraction p/q where p ∈ Z, q ∈ Z+, gcd(|p|, q) = 1.
 */

#pragma once

#include <cstdint>
#include <numeric>

namespace Sunny::Core {

/**
 * @brief Exact beat representation using rational numbers
 *
 * All constructors and operations maintain the canonical form:
 * denominator > 0, gcd(|numerator|, denominator) = 1.
 */
struct Beat {
    std::int64_t numerator;
    std::int64_t denominator;

    /**
     * @brief Normalise to canonical form: denominator > 0, irreducible
     *
     * @pre denominator != 0
     */
    [[nodiscard]] static constexpr Beat normalise(
        std::int64_t num,
        std::int64_t den
    ) noexcept {
        if (num == 0) return Beat{0, 1};
        // Ensure denominator is positive
        if (den < 0) { num = -num; den = -den; }
        std::int64_t g = std::gcd(num < 0 ? -num : num, den);
        return Beat{num / g, den / g};
    }

    /**
     * @brief Convert to floating-point (for final output only)
     */
    [[nodiscard]] constexpr double to_float() const noexcept {
        return static_cast<double>(numerator) / static_cast<double>(denominator);
    }

    /**
     * @brief Create from floating-point using Stern-Brocot / continued fraction
     *        approximation with bounded denominator
     *
     * Produces the closest rational p/q to the target where q <= max_denom.
     *
     * @param beats Target value
     * @param max_denom Maximum denominator (default 10000)
     * @return Best rational approximation
     */
    [[nodiscard]] static constexpr Beat from_float(
        double beats,
        std::int64_t max_denom = 10000
    ) noexcept {
        if (beats == 0.0) return Beat{0, 1};
        bool negative = beats < 0.0;
        double x = negative ? -beats : beats;

        // Stern-Brocot tree search for best rational approximation
        std::int64_t a_num = 0, a_den = 1;  // lower bound 0/1
        std::int64_t b_num = 1, b_den = 0;  // upper bound 1/0 (infinity)

        std::int64_t best_num = 0, best_den = 1;
        double best_err = x;

        for (int iter = 0; iter < 64; ++iter) {
            std::int64_t m_num = a_num + b_num;
            std::int64_t m_den = a_den + b_den;

            if (m_den > max_denom) break;

            double mediant = static_cast<double>(m_num) / static_cast<double>(m_den);
            double err = x - mediant;
            double abs_err = err < 0.0 ? -err : err;

            if (abs_err < best_err) {
                best_err = abs_err;
                best_num = m_num;
                best_den = m_den;
            }

            if (abs_err < 1e-15) break;

            if (err > 0.0) {
                a_num = m_num;
                a_den = m_den;
            } else {
                b_num = m_num;
                b_den = m_den;
            }
        }

        return Beat{negative ? -best_num : best_num, best_den};
    }

    [[nodiscard]] static constexpr Beat zero() noexcept { return Beat{0, 1}; }
    [[nodiscard]] static constexpr Beat one() noexcept { return Beat{1, 1}; }

    /**
     * @brief Reduce to lowest terms (explicit call; operations auto-reduce)
     */
    [[nodiscard]] constexpr Beat reduce() const noexcept {
        return normalise(numerator, denominator);
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

    // Arithmetic operators (all return normalised results)
    [[nodiscard]] constexpr Beat operator+(const Beat& other) const noexcept {
        return normalise(
            numerator * other.denominator + other.numerator * denominator,
            denominator * other.denominator
        );
    }

    [[nodiscard]] constexpr Beat operator-(const Beat& other) const noexcept {
        return normalise(
            numerator * other.denominator - other.numerator * denominator,
            denominator * other.denominator
        );
    }

    /**
     * @brief Rational multiplication: (a/b) * (c/d) = ac/bd
     *
     * Formal Spec §9.1.
     */
    [[nodiscard]] constexpr Beat operator*(const Beat& other) const noexcept {
        return normalise(
            numerator * other.numerator,
            denominator * other.denominator
        );
    }

    /**
     * @brief Scalar multiplication: (a/b) * n = an/b
     */
    [[nodiscard]] constexpr Beat operator*(std::int64_t scalar) const noexcept {
        return normalise(numerator * scalar, denominator);
    }

    /**
     * @brief Rational division: (a/b) / (c/d) = ad/bc
     *
     * @pre other != 0
     */
    [[nodiscard]] constexpr Beat operator/(const Beat& other) const noexcept {
        return normalise(
            numerator * other.denominator,
            denominator * other.numerator
        );
    }

    /**
     * @brief Scalar division: (a/b) / n = a/bn
     *
     * @pre scalar != 0
     */
    [[nodiscard]] constexpr Beat operator/(std::int64_t scalar) const noexcept {
        return normalise(numerator, denominator * scalar);
    }

    /**
     * @brief Unary negation
     */
    [[nodiscard]] constexpr Beat operator-() const noexcept {
        return Beat{-numerator, denominator};
    }
};

/**
 * @brief Compute LCM of two beat durations (exact)
 *
 * For positive rationals a/b and c/d: lcm = lcm(a,c) / gcd(b,d).
 *
 * @param a First duration (must be positive and reduced)
 * @param b Second duration (must be positive and reduced)
 * @return LCM as a Beat
 */
[[nodiscard]] constexpr Beat beat_lcm(Beat a, Beat b) noexcept {
    auto ar = a.reduce();
    auto br = b.reduce();
    std::int64_t num_gcd = std::gcd(ar.numerator, br.numerator);
    std::int64_t num_lcm = (ar.numerator / num_gcd) * br.numerator;
    std::int64_t den_gcd = std::gcd(ar.denominator, br.denominator);
    return Beat::normalise(num_lcm, den_gcd);
}

}  // namespace Sunny::Core
