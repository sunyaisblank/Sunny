/** Shared strict finite native formatter grammar; physical evidence only. */
#pragma once
#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/core/timbre/live_capabilities.hpp>

namespace sunny::infrastructure::native_unit_detail {
using Unit = sunny::core::LiveNativePhysicalUnit;
struct DisplayReading {
    double value = 0.0;
    std::optional<double> increment;
    bool negative_infinity = false;
    std::string decimal_token = "0";
    int decimal_shift = 0;
    bool decimal_negate = false;
    int fractional_digits = 0;
    bool balance = false;
};

inline std::optional<DisplayReading>
display_reading(std::string_view raw, Unit unit, bool gain_infinity) {
    if (raw.empty() || raw.size() > 80) return std::nullopt;
    const auto first = raw.find_first_not_of(" \t");
    if (first == std::string_view::npos) return std::nullopt;
    raw = raw.substr(first, raw.find_last_not_of(" \t") - first + 1);
    if (gain_infinity && (raw == "-inf dB" || raw == "-∞ dB" || raw == "−inf dB" || raw == "−∞ dB"))
        return DisplayReading{0.0, std::nullopt, true};
    if (unit == Unit::StereoBalance && raw == "C") {
        DisplayReading result;
        result.balance = true;
        return result;
    }
    std::size_t position = 0;
    if (raw[position] == '+' || raw[position] == '-') ++position;
    const auto begin = position;
    while (position < raw.size() && raw[position] >= '0' && raw[position] <= '9')
        ++position;
    const auto integral_digits = position - begin;
    std::size_t fractional_digits = 0;
    if (position < raw.size() && raw[position] == '.') {
        const auto fractional_begin = ++position;
        while (position < raw.size() && raw[position] >= '0' && raw[position] <= '9')
            ++position;
        fractional_digits = position - fractional_begin;
        if (fractional_digits == 0) return std::nullopt;
    }
    if (integral_digits == 0 && fractional_digits == 0) return std::nullopt;
    auto token = raw.substr(0, position);
    auto suffix = raw.substr(position);
    if (const auto offset = suffix.find_first_not_of(" \t"); offset != std::string_view::npos)
        suffix.remove_prefix(offset);
    else
        suffix = {};
    double multiplier = 1.0;
    switch (unit) {
    case Unit::Decibels:
        if (suffix != "dB") return std::nullopt;
        break;
    case Unit::Hertz:
        if (suffix == "kHz")
            multiplier = 1000.0;
        else if (suffix != "Hz")
            return std::nullopt;
        break;
    case Unit::Milliseconds:
        if (suffix == "s")
            multiplier = 1000.0;
        else if (suffix != "ms")
            return std::nullopt;
        break;
    case Unit::QualityFactor:
        if (!suffix.empty() || position != raw.size()) return std::nullopt;
        break;
    case Unit::Percent:
        if (suffix != "%") return std::nullopt;
        break;
    case Unit::StereoBalance:
        if ((suffix != "L" && suffix != "R") || token.front() == '+' || token.front() == '-')
            return std::nullopt;
        if (suffix == "L") multiplier = -1.0;
        break;
    }
    if (token.front() == '+') token.remove_prefix(1);
    double amount;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), amount);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) return std::nullopt;
    const auto increment =
        std::pow(10.0, -static_cast<double>(fractional_digits)) * std::abs(multiplier);
    const auto physical = amount * multiplier;
    if (!std::isfinite(physical) || !std::isfinite(increment) || increment <= 0.0 ||
        ((unit == Unit::Hertz || unit == Unit::QualityFactor) && physical <= 0.0) ||
        (unit == Unit::Milliseconds && physical < 0.0) ||
        (unit == Unit::StereoBalance && amount <= 0.0))
        return std::nullopt;
    return DisplayReading{physical,
                          increment,
                          false,
                          std::string{token},
                          std::abs(multiplier) == 1000.0 ? 3 : 0,
                          multiplier < 0.0,
                          static_cast<int>(fractional_digits),
                          unit == Unit::StereoBalance};
}

} // namespace sunny::infrastructure::native_unit_detail
