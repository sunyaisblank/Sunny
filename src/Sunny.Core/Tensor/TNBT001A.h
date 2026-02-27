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
 * - Stored in lowest terms after every operation. Intermediate products
 *   use __int128 to prevent overflow during reduction.
 * - Rational operations are exact (no precision loss)
 * - Comparison uses cross-multiplication (no floating-point)
 *
 * Formal Spec §9.1: Beat value is element of Q (rationals), represented
 * as irreducible fraction p/q where p ∈ Z, q ∈ Z+, gcd(|p|, q) = 1.
 */

#pragma once

#include <cassert>
#include <cstdint>
#include <numeric>

#include "TNTP001A.h"

// __int128 is a GCC/Clang extension, not ISO C++. Suppress -Wpedantic for
// this translation unit; the type is used for overflow-safe intermediates
// in rational arithmetic and is supported on all target platforms.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

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
     * @brief Normalise from __int128 intermediates to canonical int64_t form
     *
     * Computes GCD and reduces in __int128 space, then asserts that the
     * reduced result fits within int64_t. This permits all intermediate
     * products to exceed 64 bits safely, while requiring only that the
     * final irreducible fraction fits in the native word size.
     *
     * @pre den != 0
     */
    [[nodiscard]] static constexpr Beat normalise_wide(
        __int128 num,
        __int128 den
    ) noexcept {
        if (num == 0) return Beat{0, 1};
        if (den < 0) { num = -num; den = -den; }
        // GCD on absolute values — std::gcd does not support __int128
        __int128 a = num < 0 ? -num : num;
        __int128 b = den;
        while (b != 0) { __int128 t = b; b = a % b; a = t; }
        num /= a;
        den /= a;
        // After reduction, result must fit in int64_t
        assert(num >= INT64_MIN && num <= INT64_MAX &&
               "Beat overflow: numerator exceeds int64_t after reduction");
        assert(den >= 1 && den <= INT64_MAX &&
               "Beat overflow: denominator exceeds int64_t after reduction");
        return Beat{static_cast<int64_t>(num), static_cast<int64_t>(den)};
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

    // =========================================================================
    // Comparison operators — cross-multiply in __int128 to prevent overflow
    // =========================================================================

    [[nodiscard]] constexpr bool operator==(const Beat& other) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * other.denominator;
        __int128 rhs = static_cast<__int128>(other.numerator) * denominator;
        return lhs == rhs;
    }

    [[nodiscard]] constexpr bool operator<(const Beat& other) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * other.denominator;
        __int128 rhs = static_cast<__int128>(other.numerator) * denominator;
        return lhs < rhs;
    }

    [[nodiscard]] constexpr bool operator<=(const Beat& other) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * other.denominator;
        __int128 rhs = static_cast<__int128>(other.numerator) * denominator;
        return lhs <= rhs;
    }

    [[nodiscard]] constexpr bool operator>(const Beat& other) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * other.denominator;
        __int128 rhs = static_cast<__int128>(other.numerator) * denominator;
        return lhs > rhs;
    }

    [[nodiscard]] constexpr bool operator>=(const Beat& other) const noexcept {
        __int128 lhs = static_cast<__int128>(numerator) * other.denominator;
        __int128 rhs = static_cast<__int128>(other.numerator) * denominator;
        return lhs >= rhs;
    }

    // =========================================================================
    // Arithmetic operators — compute in __int128, reduce, assert fits int64_t
    // =========================================================================

    [[nodiscard]] constexpr Beat operator+(const Beat& other) const noexcept {
        __int128 num = static_cast<__int128>(numerator) * other.denominator
                     + static_cast<__int128>(other.numerator) * denominator;
        __int128 den = static_cast<__int128>(denominator) * other.denominator;
        return normalise_wide(num, den);
    }

    [[nodiscard]] constexpr Beat operator-(const Beat& other) const noexcept {
        __int128 num = static_cast<__int128>(numerator) * other.denominator
                     - static_cast<__int128>(other.numerator) * denominator;
        __int128 den = static_cast<__int128>(denominator) * other.denominator;
        return normalise_wide(num, den);
    }

    /**
     * @brief Rational multiplication: (a/b) * (c/d) = ac/bd
     *
     * Formal Spec §9.1.
     */
    [[nodiscard]] constexpr Beat operator*(const Beat& other) const noexcept {
        __int128 num = static_cast<__int128>(numerator) * other.numerator;
        __int128 den = static_cast<__int128>(denominator) * other.denominator;
        return normalise_wide(num, den);
    }

    /**
     * @brief Scalar multiplication: (a/b) * n = an/b
     */
    [[nodiscard]] constexpr Beat operator*(std::int64_t scalar) const noexcept {
        __int128 num = static_cast<__int128>(numerator) * scalar;
        __int128 den = denominator;
        return normalise_wide(num, den);
    }

    /**
     * @brief Rational division: (a/b) / (c/d) = ad/bc
     *
     * @pre other != 0
     */
    [[nodiscard]] constexpr Beat operator/(const Beat& other) const noexcept {
        __int128 num = static_cast<__int128>(numerator) * other.denominator;
        __int128 den = static_cast<__int128>(denominator) * other.numerator;
        return normalise_wide(num, den);
    }

    /**
     * @brief Scalar division: (a/b) / n = a/bn
     *
     * @pre scalar != 0
     */
    [[nodiscard]] constexpr Beat operator/(std::int64_t scalar) const noexcept {
        __int128 num = numerator;
        __int128 den = static_cast<__int128>(denominator) * scalar;
        return normalise_wide(num, den);
    }

    /**
     * @brief Unary negation
     */
    [[nodiscard]] constexpr Beat operator-() const noexcept {
        assert(numerator != INT64_MIN &&
               "Beat: cannot negate INT64_MIN numerator");
        return Beat{-numerator, denominator};
    }
};

// =============================================================================
// beat_lcm — LCM of two beat durations using __int128 intermediates
// =============================================================================

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
    __int128 a_num = ar.numerator < 0
        ? -static_cast<__int128>(ar.numerator)
        : static_cast<__int128>(ar.numerator);
    __int128 b_num = br.numerator < 0
        ? -static_cast<__int128>(br.numerator)
        : static_cast<__int128>(br.numerator);
    // GCD of numerators
    __int128 ga = a_num, gb = b_num;
    while (gb != 0) { __int128 t = gb; gb = ga % gb; ga = t; }
    __int128 num_lcm = (a_num / ga) * b_num;
    std::int64_t den_gcd = std::gcd(ar.denominator, br.denominator);
    assert(num_lcm >= 0 && num_lcm <= INT64_MAX &&
           "beat_lcm: numerator overflow after reduction");
    return Beat::normalise(static_cast<int64_t>(num_lcm), den_gcd);
}

// =============================================================================
// Checked arithmetic — return Result<Beat> instead of asserting on overflow
// =============================================================================

namespace detail {

/**
 * @brief Reduce __int128 intermediate and return Result<Beat>.
 *
 * Returns ArithmeticOverflow if the reduced result exceeds int64_t,
 * rather than asserting.
 */
[[nodiscard]] constexpr Result<Beat> checked_normalise_wide(
    __int128 num,
    __int128 den
) noexcept {
    if (num == 0) return Beat{0, 1};
    if (den < 0) { num = -num; den = -den; }
    __int128 a = num < 0 ? -num : num;
    __int128 b = den;
    while (b != 0) { __int128 t = b; b = a % b; a = t; }
    num /= a;
    den /= a;
    if (num < INT64_MIN || num > INT64_MAX || den < 1 || den > INT64_MAX)
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    return Beat{static_cast<int64_t>(num), static_cast<int64_t>(den)};
}

}  // namespace detail

/**
 * @brief Checked addition returning Result<Beat>.
 *
 * Suitable for Score IR boundaries where overflow must be reported
 * rather than treated as an invariant violation.
 */
[[nodiscard]] constexpr Result<Beat> checked_add(Beat a, Beat b) noexcept {
    __int128 num = static_cast<__int128>(a.numerator) * b.denominator
                 + static_cast<__int128>(b.numerator) * a.denominator;
    __int128 den = static_cast<__int128>(a.denominator) * b.denominator;
    return detail::checked_normalise_wide(num, den);
}

/**
 * @brief Checked subtraction returning Result<Beat>.
 */
[[nodiscard]] constexpr Result<Beat> checked_sub(Beat a, Beat b) noexcept {
    __int128 num = static_cast<__int128>(a.numerator) * b.denominator
                 - static_cast<__int128>(b.numerator) * a.denominator;
    __int128 den = static_cast<__int128>(a.denominator) * b.denominator;
    return detail::checked_normalise_wide(num, den);
}

/**
 * @brief Checked multiplication returning Result<Beat>.
 */
[[nodiscard]] constexpr Result<Beat> checked_mul(Beat a, Beat b) noexcept {
    __int128 num = static_cast<__int128>(a.numerator) * b.numerator;
    __int128 den = static_cast<__int128>(a.denominator) * b.denominator;
    return detail::checked_normalise_wide(num, den);
}

/**
 * @brief Checked division returning Result<Beat>.
 *
 * @pre b.numerator != 0
 */
[[nodiscard]] constexpr Result<Beat> checked_div(Beat a, Beat b) noexcept {
    __int128 num = static_cast<__int128>(a.numerator) * b.denominator;
    __int128 den = static_cast<__int128>(a.denominator) * b.numerator;
    return detail::checked_normalise_wide(num, den);
}

}  // namespace Sunny::Core

#pragma GCC diagnostic pop
