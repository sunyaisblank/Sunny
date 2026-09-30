/**
 * @file beat.hpp
 * @brief Exact beat representation using rational numbers
 *
 * All timing calculations use rational arithmetic to avoid floating-point
 * precision issues. Conversion to audio samples happens at the boundary.
 *
 * Invariants:
 * - denominator > 0
 * - numerator and denominator are stored in lowest terms
 * - rational operations are exact or report arithmetic overflow
 *
 * Formal Spec §9.1: a beat is an irreducible fraction p/q where p is an
 * integer, q is positive, and gcd(|p|, q) = 1.
 */

#pragma once

#include <algorithm>
#include <cassert>
#include <cmath>
#include <compare>
#include <cstdint>
#include <exception>
#include <limits>
#include <numeric>
#include <sunny/core/types/music_types.hpp>

namespace sunny::core {

namespace detail {

struct CanonicalBeatComponents {
    std::int64_t numerator{0};
    std::int64_t denominator{1};
    bool representable{false};
};

inline constexpr auto sign_bit_magnitude = std::uint64_t{1} << 63;

[[nodiscard]] constexpr std::uint64_t magnitude(std::int64_t value) noexcept {
    if (value >= 0) return static_cast<std::uint64_t>(value);
    return static_cast<std::uint64_t>(-(value + 1)) + 1;
}

[[nodiscard]] constexpr bool signed_magnitude_fits(std::uint64_t value, bool negative) noexcept {
    const auto limit = static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    return value <= limit + static_cast<std::uint64_t>(negative);
}

[[nodiscard]] constexpr std::int64_t from_signed_magnitude(std::uint64_t value,
                                                           bool negative) noexcept {
    if (negative && value == sign_bit_magnitude) return std::numeric_limits<std::int64_t>::min();

    const auto signed_value = static_cast<std::int64_t>(value);
    return negative ? -signed_value : signed_value;
}

/** Canonicalise a rational without constructing a Beat or invoking assertions. */
[[nodiscard]] constexpr CanonicalBeatComponents
canonical_beat_components(std::int64_t numerator, std::int64_t denominator) noexcept {
    if (denominator == 0) return {};
    if (numerator == 0) return {0, 1, true};

    const bool negative = (numerator < 0) != (denominator < 0);
    auto numerator_magnitude = magnitude(numerator);
    auto denominator_magnitude = magnitude(denominator);
    const auto divisor = std::gcd(numerator_magnitude, denominator_magnitude);
    numerator_magnitude /= divisor;
    denominator_magnitude /= divisor;

    if (!signed_magnitude_fits(numerator_magnitude, negative) ||
        denominator_magnitude >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
        return {};

    return {
        from_signed_magnitude(numerator_magnitude, negative),
        static_cast<std::int64_t>(denominator_magnitude),
        true,
    };
}

[[nodiscard]] constexpr bool
try_add(std::int64_t lhs, std::int64_t rhs, std::int64_t& result) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((rhs > 0 && lhs > maximum - rhs) || (rhs < 0 && lhs < minimum - rhs)) return false;
    result = lhs + rhs;
    return true;
}

[[nodiscard]] constexpr bool
try_subtract(std::int64_t lhs, std::int64_t rhs, std::int64_t& result) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
    if ((rhs > 0 && lhs < minimum + rhs) || (rhs < 0 && lhs > maximum + rhs)) return false;
    result = lhs - rhs;
    return true;
}

[[nodiscard]] constexpr bool
try_multiply(std::int64_t lhs, std::int64_t rhs, std::int64_t& result) noexcept {
    constexpr auto minimum = std::numeric_limits<std::int64_t>::min();
    constexpr auto maximum = std::numeric_limits<std::int64_t>::max();

    if (lhs == 0 || rhs == 0) {
        result = 0;
        return true;
    }
    if ((lhs == -1 && rhs == minimum) || (rhs == -1 && lhs == minimum)) return false;

    if (lhs > 0) {
        if ((rhs > 0 && lhs > maximum / rhs) || (rhs < 0 && rhs < minimum / lhs)) return false;
    } else {
        if ((rhs > 0 && lhs < minimum / rhs) || (rhs < 0 && lhs < maximum / rhs)) return false;
    }

    result = lhs * rhs;
    return true;
}

[[nodiscard]] constexpr std::int64_t divide_exact(std::int64_t value,
                                                  std::uint64_t divisor) noexcept {
    assert(divisor != 0 && magnitude(value) % divisor == 0);
    const auto quotient = magnitude(value) / divisor;
    return from_signed_magnitude(quotient, value < 0);
}

/** Compare two non-negative fractions without cross-product overflow. */
[[nodiscard]] constexpr int compare_unsigned_fractions(std::uint64_t lhs_numerator,
                                                       std::uint64_t lhs_denominator,
                                                       std::uint64_t rhs_numerator,
                                                       std::uint64_t rhs_denominator) noexcept {
    bool reversed = false;

    while (true) {
        const auto lhs_quotient = lhs_numerator / lhs_denominator;
        const auto rhs_quotient = rhs_numerator / rhs_denominator;
        if (lhs_quotient != rhs_quotient) {
            const int comparison = lhs_quotient < rhs_quotient ? -1 : 1;
            return reversed ? -comparison : comparison;
        }

        const auto lhs_remainder = lhs_numerator % lhs_denominator;
        const auto rhs_remainder = rhs_numerator % rhs_denominator;
        if (lhs_remainder == 0 || rhs_remainder == 0) {
            if (lhs_remainder == rhs_remainder) return 0;
            const int comparison = lhs_remainder == 0 ? -1 : 1;
            return reversed ? -comparison : comparison;
        }

        lhs_numerator = lhs_denominator;
        lhs_denominator = lhs_remainder;
        rhs_numerator = rhs_denominator;
        rhs_denominator = rhs_remainder;
        reversed = !reversed;
    }
}

} // namespace detail

/**
 * @brief Exact beat representation using rational numbers.
 *
 * Every successfully constructed value is canonical. The two-argument
 * constructor is a programming-contract boundary: its denominator must be
 * non-zero and its canonical representation must fit in int64_t. Dynamic or
 * untrusted inputs must use a checked parser/factory before construction.
 */
class Beat {
  public:
    constexpr Beat() noexcept = default;

    constexpr Beat(std::int64_t numerator, std::int64_t denominator) noexcept {
        const auto canonical = detail::canonical_beat_components(numerator, denominator);
        assert(canonical.representable &&
               "Beat denominator must be non-zero and canonical representation must fit int64_t");
        if (!canonical.representable) std::terminate();
        numerator_ = canonical.numerator;
        denominator_ = canonical.denominator;
    }

    [[nodiscard]] constexpr std::int64_t numerator() const noexcept { return numerator_; }
    [[nodiscard]] constexpr std::int64_t denominator() const noexcept { return denominator_; }

    /** @brief Construct a ratio from dynamic input without violating the value invariant. */
    [[nodiscard]] static constexpr Result<Beat> from_ratio(std::int64_t numerator,
                                                           std::int64_t denominator) noexcept {
        if (denominator == 0) return std::unexpected(ErrorCode::InvalidBeat);
        const auto canonical = detail::canonical_beat_components(numerator, denominator);
        if (!canonical.representable) return std::unexpected(ErrorCode::ArithmeticOverflow);
        return Beat{canonical.numerator, canonical.denominator};
    }

    /** @brief Return the canonical representation of `num / den`. */
    [[nodiscard]] static constexpr Beat normalise(std::int64_t num, std::int64_t den) noexcept {
        return Beat{num, den};
    }

    /** @brief Convert to floating point at an output boundary. */
    [[nodiscard]] constexpr double to_float() const noexcept {
        return static_cast<double>(numerator_) / static_cast<double>(denominator_);
    }

    /** @brief Approximate a finite floating-point beat with a bounded denominator. */
    [[nodiscard]] static Result<Beat> from_float(double beats,
                                                 std::int64_t max_denom = 10'000) noexcept {
        if (!std::isfinite(beats) || max_denom <= 0) return std::unexpected(ErrorCode::InvalidBeat);
        if (beats == 0.0) return Beat{};

        const bool negative = beats < 0.0;
        const long double target = std::fabs(static_cast<long double>(beats));
        constexpr auto maximum = std::numeric_limits<std::int64_t>::max();
        if (target > static_cast<long double>(maximum))
            return std::unexpected(ErrorCode::ArithmeticOverflow);

        std::int64_t previous_numerator = 0;
        std::int64_t previous_denominator = 1;
        std::int64_t current_numerator = 1;
        std::int64_t current_denominator = 0;
        long double remainder = target;

        const auto finish = [negative](std::int64_t numerator,
                                       std::int64_t denominator) -> Result<Beat> {
            return Beat::normalise(negative ? -numerator : numerator, denominator);
        };

        const auto closest_bounded = [&]() -> Result<Beat> {
            if (current_denominator == 0) return finish(previous_numerator, previous_denominator);

            const auto denominator_steps = (max_denom - previous_denominator) / current_denominator;
            const auto numerator_steps = current_numerator == 0
                                             ? denominator_steps
                                             : (maximum - previous_numerator) / current_numerator;
            const auto steps = std::min(denominator_steps, numerator_steps);

            std::int64_t scaled_numerator = 0;
            std::int64_t bounded_numerator = 0;
            std::int64_t scaled_denominator = 0;
            std::int64_t bounded_denominator = 0;
            if (!detail::try_multiply(current_numerator, steps, scaled_numerator) ||
                !detail::try_add(previous_numerator, scaled_numerator, bounded_numerator) ||
                !detail::try_multiply(current_denominator, steps, scaled_denominator) ||
                !detail::try_add(previous_denominator, scaled_denominator, bounded_denominator))
                return std::unexpected(ErrorCode::ArithmeticOverflow);

            const long double bounded_error =
                std::fabs(target - static_cast<long double>(bounded_numerator) /
                                       static_cast<long double>(bounded_denominator));
            const long double current_error =
                std::fabs(target - static_cast<long double>(current_numerator) /
                                       static_cast<long double>(current_denominator));
            if (current_error <= bounded_error)
                return finish(current_numerator, current_denominator);
            return finish(bounded_numerator, bounded_denominator);
        };

        for (int iteration = 0; iteration < 128; ++iteration) {
            const long double integral = std::floor(remainder);
            if (!std::isfinite(integral) || integral > static_cast<long double>(maximum))
                return closest_bounded();
            const auto coefficient = static_cast<std::int64_t>(integral);

            std::int64_t numerator_product = 0;
            std::int64_t next_numerator = 0;
            std::int64_t denominator_product = 0;
            std::int64_t next_denominator = 0;
            if (!detail::try_multiply(coefficient, current_numerator, numerator_product) ||
                !detail::try_add(previous_numerator, numerator_product, next_numerator) ||
                !detail::try_multiply(coefficient, current_denominator, denominator_product) ||
                !detail::try_add(previous_denominator, denominator_product, next_denominator) ||
                next_denominator > max_denom)
                return closest_bounded();

            previous_numerator = current_numerator;
            previous_denominator = current_denominator;
            current_numerator = next_numerator;
            current_denominator = next_denominator;

            const long double fractional = remainder - integral;
            if (fractional == 0.0L) return finish(current_numerator, current_denominator);
            remainder = 1.0L / fractional;
        }

        return closest_bounded();
    }

    [[nodiscard]] static constexpr Beat zero() noexcept { return {}; }
    [[nodiscard]] static constexpr Beat one() noexcept { return {1, 1}; }
    [[nodiscard]] constexpr Beat reduce() const noexcept { return *this; }

    [[nodiscard]] constexpr bool operator==(const Beat& other) const noexcept;
    [[nodiscard]] constexpr std::strong_ordering operator<=>(const Beat& other) const noexcept;

    [[nodiscard]] constexpr Beat operator+(const Beat& other) const noexcept;
    [[nodiscard]] constexpr Beat operator-(const Beat& other) const noexcept;
    [[nodiscard]] constexpr Beat operator*(const Beat& other) const noexcept;
    [[nodiscard]] constexpr Beat operator*(std::int64_t scalar) const noexcept;
    [[nodiscard]] constexpr Beat operator/(const Beat& other) const noexcept;
    [[nodiscard]] constexpr Beat operator/(std::int64_t scalar) const noexcept;

    [[nodiscard]] constexpr Beat operator-() const noexcept {
        const bool representable = numerator_ != std::numeric_limits<std::int64_t>::min();
        assert(representable && "Beat cannot negate INT64_MIN");
        return representable ? Beat{-numerator_, denominator_} : Beat{};
    }

  private:
    std::int64_t numerator_{0};
    std::int64_t denominator_{1};
};

namespace detail {

[[nodiscard]] constexpr Result<Beat> checked_normalise(std::int64_t numerator,
                                                       std::int64_t denominator) noexcept {
    const auto result = Beat::from_ratio(numerator, denominator);
    if (!result) return std::unexpected(ErrorCode::ArithmeticOverflow);
    return result;
}

[[nodiscard]] constexpr int compare(Beat lhs, Beat rhs) noexcept {
    if (lhs.numerator() < 0 && rhs.numerator() >= 0) return -1;
    if (lhs.numerator() >= 0 && rhs.numerator() < 0) return 1;

    const int comparison =
        compare_unsigned_fractions(magnitude(lhs.numerator()),
                                   static_cast<std::uint64_t>(lhs.denominator()),
                                   magnitude(rhs.numerator()),
                                   static_cast<std::uint64_t>(rhs.denominator()));
    return lhs.numerator() < 0 ? -comparison : comparison;
}

[[nodiscard]] constexpr Beat require_value(Result<Beat> result) noexcept {
    assert(result.has_value() && "Beat arithmetic overflow");
    return result ? *result : Beat{};
}

} // namespace detail

/** @brief Checked addition that reports an unrepresentable result. */
[[nodiscard]] constexpr Result<Beat> checked_add(Beat lhs, Beat rhs) noexcept {
    lhs = lhs.reduce();
    rhs = rhs.reduce();

    const auto denominator_gcd = std::gcd(lhs.denominator(), rhs.denominator());
    const auto lhs_factor = rhs.denominator() / denominator_gcd;
    const auto rhs_factor = lhs.denominator() / denominator_gcd;

    std::int64_t lhs_term = 0;
    std::int64_t rhs_term = 0;
    std::int64_t numerator = 0;
    if (!detail::try_multiply(lhs.numerator(), lhs_factor, lhs_term) ||
        !detail::try_multiply(rhs.numerator(), rhs_factor, rhs_term) ||
        !detail::try_add(lhs_term, rhs_term, numerator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    const auto remaining_gcd =
        std::gcd(detail::magnitude(numerator), static_cast<std::uint64_t>(denominator_gcd));
    numerator = detail::divide_exact(numerator, remaining_gcd);

    std::int64_t denominator = 0;
    if (!detail::try_multiply(
            rhs_factor, rhs.denominator() / static_cast<std::int64_t>(remaining_gcd), denominator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    return Beat{numerator, denominator};
}

/** @brief Checked subtraction that reports an unrepresentable result. */
[[nodiscard]] constexpr Result<Beat> checked_sub(Beat lhs, Beat rhs) noexcept {
    lhs = lhs.reduce();
    rhs = rhs.reduce();

    const auto denominator_gcd = std::gcd(lhs.denominator(), rhs.denominator());
    const auto lhs_factor = rhs.denominator() / denominator_gcd;
    const auto rhs_factor = lhs.denominator() / denominator_gcd;

    std::int64_t lhs_term = 0;
    std::int64_t rhs_term = 0;
    std::int64_t numerator = 0;
    if (!detail::try_multiply(lhs.numerator(), lhs_factor, lhs_term) ||
        !detail::try_multiply(rhs.numerator(), rhs_factor, rhs_term) ||
        !detail::try_subtract(lhs_term, rhs_term, numerator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    const auto remaining_gcd =
        std::gcd(detail::magnitude(numerator), static_cast<std::uint64_t>(denominator_gcd));
    numerator = detail::divide_exact(numerator, remaining_gcd);

    std::int64_t denominator = 0;
    if (!detail::try_multiply(
            rhs_factor, rhs.denominator() / static_cast<std::int64_t>(remaining_gcd), denominator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    return Beat{numerator, denominator};
}

/** @brief Checked multiplication with cross-cancellation before products. */
[[nodiscard]] constexpr Result<Beat> checked_mul(Beat lhs, Beat rhs) noexcept {
    lhs = lhs.reduce();
    rhs = rhs.reduce();

    const auto lhs_divisor =
        std::gcd(detail::magnitude(lhs.numerator()), static_cast<std::uint64_t>(rhs.denominator()));
    const auto rhs_divisor =
        std::gcd(detail::magnitude(rhs.numerator()), static_cast<std::uint64_t>(lhs.denominator()));

    const auto lhs_numerator = detail::divide_exact(lhs.numerator(), lhs_divisor);
    const auto rhs_numerator = detail::divide_exact(rhs.numerator(), rhs_divisor);
    const auto lhs_denominator = lhs.denominator() / static_cast<std::int64_t>(rhs_divisor);
    const auto rhs_denominator = rhs.denominator() / static_cast<std::int64_t>(lhs_divisor);

    std::int64_t numerator = 0;
    std::int64_t denominator = 0;
    if (!detail::try_multiply(lhs_numerator, rhs_numerator, numerator) ||
        !detail::try_multiply(lhs_denominator, rhs_denominator, denominator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    return Beat{numerator, denominator};
}

/** @brief Checked division with cross-cancellation before products. */
[[nodiscard]] constexpr Result<Beat> checked_div(Beat lhs, Beat rhs) noexcept {
    lhs = lhs.reduce();
    rhs = rhs.reduce();
    if (rhs.numerator() == 0) return std::unexpected(ErrorCode::ArithmeticOverflow);

    const auto numerator_divisor =
        std::gcd(detail::magnitude(lhs.numerator()), detail::magnitude(rhs.numerator()));
    const auto denominator_divisor = std::gcd(static_cast<std::uint64_t>(lhs.denominator()),
                                              static_cast<std::uint64_t>(rhs.denominator()));

    const auto lhs_numerator = detail::divide_exact(lhs.numerator(), numerator_divisor);
    const auto rhs_numerator = detail::divide_exact(rhs.numerator(), numerator_divisor);
    const auto lhs_denominator = lhs.denominator() / static_cast<std::int64_t>(denominator_divisor);
    const auto rhs_denominator = rhs.denominator() / static_cast<std::int64_t>(denominator_divisor);

    std::int64_t numerator = 0;
    std::int64_t denominator = 0;
    if (!detail::try_multiply(lhs_numerator, rhs_denominator, numerator) ||
        !detail::try_multiply(lhs_denominator, rhs_numerator, denominator))
        return std::unexpected(ErrorCode::ArithmeticOverflow);

    return detail::checked_normalise(numerator, denominator);
}

constexpr bool Beat::operator==(const Beat& other) const noexcept {
    return detail::compare(*this, other) == 0;
}

constexpr std::strong_ordering Beat::operator<=>(const Beat& other) const noexcept {
    const int comparison = detail::compare(*this, other);
    if (comparison < 0) return std::strong_ordering::less;
    if (comparison > 0) return std::strong_ordering::greater;
    return std::strong_ordering::equal;
}

constexpr Beat Beat::operator+(const Beat& other) const noexcept {
    return detail::require_value(checked_add(*this, other));
}

constexpr Beat Beat::operator-(const Beat& other) const noexcept {
    return detail::require_value(checked_sub(*this, other));
}

constexpr Beat Beat::operator*(const Beat& other) const noexcept {
    return detail::require_value(checked_mul(*this, other));
}

constexpr Beat Beat::operator*(std::int64_t scalar) const noexcept {
    return *this * Beat{scalar, 1};
}

constexpr Beat Beat::operator/(const Beat& other) const noexcept {
    return detail::require_value(checked_div(*this, other));
}

constexpr Beat Beat::operator/(std::int64_t scalar) const noexcept {
    return *this / Beat{scalar, 1};
}

/**
 * @brief Compute the exact LCM of two positive beat durations.
 *
 * For positive rationals a/b and c/d: lcm = lcm(a,c) / gcd(b,d).
 */
[[nodiscard]] constexpr Beat beat_lcm(Beat lhs, Beat rhs) noexcept {
    lhs = lhs.reduce();
    rhs = rhs.reduce();
    const bool valid = lhs.numerator() > 0 && rhs.numerator() > 0;
    assert(valid && "beat_lcm requires positive beat durations");
    if (!valid) return {};

    const auto numerator_gcd = std::gcd(lhs.numerator(), rhs.numerator());
    std::int64_t numerator = 0;
    const bool representable =
        detail::try_multiply(lhs.numerator() / numerator_gcd, rhs.numerator(), numerator);
    assert(representable && "beat_lcm result exceeds int64_t");
    if (!representable) return {};

    return Beat::normalise(numerator, std::gcd(lhs.denominator(), rhs.denominator()));
}

} // namespace sunny::core
