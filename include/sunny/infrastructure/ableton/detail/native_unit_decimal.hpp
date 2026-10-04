/** Bounded decimal arithmetic for the Python native formatter evidence.
 * Decimal(str(double)) inputs use their shortest round-trip decimal value.
 * Native tokens contain at most 80 characters; arithmetic uses ROUND_HALF_EVEN
 * with the producer's precision (110 for search, 28 for setter readback).
 * Binary arithmetic and display increment never enlarge a physical tolerance.
 */
#pragma once
#include <algorithm>
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/infrastructure/ableton/detail/native_unit_display.hpp>

namespace sunny::infrastructure::native_unit_detail {
namespace decimal_detail {
struct Decimal {
    std::string digits = "0";
    int exponent = 0;
    bool negative = false;
};

inline void normalize(Decimal& value) {
    const auto first = value.digits.find_first_not_of('0');
    if (first == std::string::npos) {
        value = {};
        return;
    }
    value.digits.erase(0, first);
    while (value.digits.back() == '0') {
        value.digits.pop_back();
        ++value.exponent;
    }
}

inline std::optional<Decimal> parse(std::string_view token) {
    if (token.empty() || token.size() > 80) return std::nullopt;
    Decimal value;
    value.digits.clear();
    std::size_t position = 0;
    if (token[position] == '+' || token[position] == '-') value.negative = token[position++] == '-';
    bool fraction = false;
    while (position < token.size() && token[position] != 'e' && token[position] != 'E') {
        const char character = token[position++];
        if (character == '.' && !fraction) {
            fraction = true;
        } else if (character >= '0' && character <= '9') {
            value.digits += character;
            if (fraction) --value.exponent;
        } else {
            return std::nullopt;
        }
    }
    if (value.digits.empty()) return std::nullopt;
    if (position < token.size()) {
        ++position;
        auto suffix = token.substr(position);
        if (!suffix.empty() && suffix.front() == '+') suffix.remove_prefix(1);
        int exponent = 0;
        const auto read = std::from_chars(suffix.data(), suffix.data() + suffix.size(), exponent);
        if (read.ec != std::errc{} || read.ptr != suffix.data() + suffix.size() ||
            exponent < -400 || exponent > 400)
            return std::nullopt;
        value.exponent += exponent;
    }
    normalize(value);
    return value;
}

inline std::optional<Decimal> from_double(double value) {
    if (!std::isfinite(value)) return std::nullopt;
    char text[32];
    const auto write = std::to_chars(text, text + sizeof(text), value, std::chars_format::general);
    if (write.ec != std::errc{}) return std::nullopt;
    return parse(std::string_view{text, write.ptr});
}

inline int compare_digits(std::string_view first, std::string_view second) {
    if (first.size() != second.size()) return first.size() < second.size() ? -1 : 1;
    return first == second ? 0 : first < second ? -1 : 1;
}

inline int compare_magnitude(const Decimal& first, const Decimal& second) {
    if (first.digits == "0" || second.digits == "0")
        return first.digits == second.digits ? 0 : first.digits == "0" ? -1 : 1;
    const auto first_length = static_cast<int>(first.digits.size()) + first.exponent;
    const auto second_length = static_cast<int>(second.digits.size()) + second.exponent;
    if (first_length != second_length) return first_length < second_length ? -1 : 1;
    const auto count = std::max(first.digits.size(), second.digits.size());
    for (std::size_t i = 0; i < count; ++i) {
        const auto a = i < first.digits.size() ? first.digits[i] : '0';
        const auto b = i < second.digits.size() ? second.digits[i] : '0';
        if (a != b) return a < b ? -1 : 1;
    }
    return 0;
}

inline std::string add_digits(std::string first, std::string_view second) {
    first.insert(0, std::max(first.size(), second.size()) - first.size() + 1, '0');
    int carry = 0;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const auto index = first.size() - 1 - i;
        const int amount = first[index] - '0' + carry +
                           (i < second.size() ? second[second.size() - 1 - i] - '0' : 0);
        first[index] = static_cast<char>('0' + amount % 10);
        carry = amount / 10;
    }
    return first;
}

// Both operands are unsigned integers, first >= second.
inline std::string subtract_digits(std::string first, std::string_view second) {
    int borrow = 0;
    for (std::size_t i = 0; i < first.size(); ++i) {
        const auto index = first.size() - 1 - i;
        int amount = first[index] - '0' - borrow -
                     (i < second.size() ? second[second.size() - 1 - i] - '0' : 0);
        borrow = amount < 0 ? 1 : 0;
        if (borrow) amount += 10;
        first[index] = static_cast<char>('0' + amount);
    }
    const auto start = first.find_first_not_of('0');
    return start == std::string::npos ? "0" : first.substr(start);
}

inline void round(Decimal& value, std::size_t precision, bool sticky = false) {
    if (value.digits.size() <= precision) return;
    const auto discarded = value.digits.size() - precision;
    const char guard = value.digits[precision];
    const bool tail =
        sticky || value.digits.find_first_not_of('0', precision + 1) != std::string::npos;
    const bool increment =
        guard > '5' || (guard == '5' && (tail || (value.digits[precision - 1] - '0') % 2));
    value.digits.resize(precision);
    value.exponent += static_cast<int>(discarded);
    if (increment) value.digits = add_digits(value.digits, "1");
    normalize(value);
}

inline Decimal difference(Decimal first, Decimal second, std::size_t precision) {
    const auto exponent = std::min(first.exponent, second.exponent);
    // Finite double exponents plus bounded formatter tokens need < 900 digits.
    if (first.digits != "0")
        first.digits.append(static_cast<std::size_t>(first.exponent - exponent), '0');
    if (second.digits != "0")
        second.digits.append(static_cast<std::size_t>(second.exponent - exponent), '0');
    Decimal result;
    result.exponent = exponent;
    if (first.negative != second.negative) {
        result.digits = add_digits(first.digits, second.digits);
    } else if (compare_digits(first.digits, second.digits) >= 0) {
        result.digits = subtract_digits(first.digits, second.digits);
    } else {
        result.digits = subtract_digits(second.digits, first.digits);
    }
    normalize(result);
    round(result, precision);
    return result; // Absolute difference; the sign is immaterial.
}

inline Decimal divide(const Decimal& first, const Decimal& second, std::size_t precision) {
    if (first.digits == "0") return {};
    const auto shift = static_cast<int>(precision + 1 + second.digits.size()) -
                       static_cast<int>(first.digits.size());
    std::string dividend = first.digits, divisor = second.digits;
    if (shift >= 0)
        dividend.append(static_cast<std::size_t>(shift), '0');
    else
        divisor.append(static_cast<std::size_t>(-shift), '0');
    Decimal quotient;
    quotient.digits.clear();
    quotient.exponent = first.exponent - second.exponent - shift;
    quotient.negative = first.negative != second.negative;
    std::string remainder = "0";
    for (const char digit : dividend) {
        if (remainder == "0") remainder.clear();
        remainder += digit;
        unsigned amount = 0;
        while (compare_digits(remainder, divisor) >= 0) {
            remainder = subtract_digits(remainder, divisor);
            ++amount;
        }
        quotient.digits += static_cast<char>('0' + amount);
    }
    // Keep the remainder as sticky evidence when rounding the guard digits.
    const auto start = quotient.digits.find_first_not_of('0');
    quotient.digits.erase(0, start == std::string::npos ? quotient.digits.size() - 1 : start);
    round(quotient, precision, remainder != "0");
    normalize(quotient);
    return quotient;
}

inline std::optional<double> to_double(const Decimal& value) {
    const auto text =
        (value.negative ? "-" : "") + value.digits + "e" + std::to_string(value.exponent);
    double result = 0;
    const auto read = std::from_chars(text.data(), text.data() + text.size(), result);
    if (read.ec == std::errc::result_out_of_range &&
        static_cast<int>(value.digits.size()) + value.exponent < -307)
        return value.negative ? -0.0 : 0.0; // Python float(Decimal) underflow.
    if (read.ec != std::errc{} || read.ptr != text.data() + text.size() || !std::isfinite(result))
        return std::nullopt;
    return result;
}

inline std::optional<Decimal> physical(const DisplayReading& reading, std::size_t precision) {
    auto value = parse(reading.decimal_token);
    if (!value) return std::nullopt;
    value->exponent += reading.decimal_shift;
    if (reading.decimal_negate) value->negative = !value->negative;
    if (reading.decimal_shift || reading.decimal_negate) round(*value, precision);
    return value;
}
} // namespace decimal_detail

struct DecimalDisplayComparison {
    double display_value = 0.0;
    std::optional<double> display_increment;
    double absolute_display_error = 0.0;
    bool within_tolerance = false;
};

inline std::optional<DecimalDisplayComparison>
decimal_display_comparison(const DisplayReading& reading,
                           double target,
                           double tolerance,
                           double full_scale = 1.0,
                           std::size_t precision = 110,
                           const DisplayReading* observed_full_scale = nullptr) {
    using namespace decimal_detail;
    if (reading.negative_infinity || !std::isfinite(tolerance) || tolerance < 0.0 ||
        !std::isfinite(full_scale) || full_scale <= 0.0 || (precision != 28 && precision != 110))
        return std::nullopt;
    auto amount = physical(reading, precision);
    auto desired = from_double(target), admitted = from_double(tolerance);
    auto scale =
        observed_full_scale ? physical(*observed_full_scale, precision) : from_double(full_scale);
    if (!amount || !desired || !admitted || !scale || scale->negative || scale->digits == "0")
        return std::nullopt;
    std::optional<Decimal> increment;
    if (reading.increment)
        increment = Decimal{"1", reading.decimal_shift - reading.fractional_digits, false};
    if (reading.balance || full_scale != 1.0 || observed_full_scale) {
        *amount = divide(*amount, *scale, precision);
        if (increment) *increment = divide(*increment, *scale, precision);
    }
    const auto error = difference(*amount, *desired, precision);
    const auto numeric = to_double(*amount), absolute_error = to_double(error);
    if (!numeric || !absolute_error) return std::nullopt;
    DecimalDisplayComparison result;
    result.display_value = *numeric;
    result.absolute_display_error = *absolute_error;
    result.within_tolerance = compare_magnitude(error, *admitted) <= 0;
    if (increment) {
        result.display_increment = to_double(*increment);
        if (!result.display_increment || *result.display_increment <= 0.0) return std::nullopt;
    }
    return result;
}
} // namespace sunny::infrastructure::native_unit_detail
