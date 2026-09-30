/**
 * @file serialization_integer.hpp
 * @brief Checked integer conversion helpers for JSON-backed IR readers.
 */

#pragma once

#include <charconv>
#include <concepts>
#include <cstdint>
#include <limits>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <type_traits>

namespace sunny::core::detail {

template <std::integral Integer>
[[nodiscard]] Integer checked_integer(const nlohmann::json& value, std::string_view name) {
    static_assert(!std::same_as<Integer, bool>);

    bool in_range = false;
    if (value.is_number_unsigned()) {
        const auto raw = value.get<std::uint64_t>();
        in_range = raw <= static_cast<std::uint64_t>(std::numeric_limits<Integer>::max());
    } else if (value.is_number_integer()) {
        const auto raw = value.get<std::int64_t>();
        if constexpr (std::is_signed_v<Integer>) {
            in_range = raw >= static_cast<std::int64_t>(std::numeric_limits<Integer>::lowest()) &&
                       raw <= static_cast<std::int64_t>(std::numeric_limits<Integer>::max());
        } else {
            in_range =
                raw >= 0 && static_cast<std::uint64_t>(raw) <=
                                static_cast<std::uint64_t>(std::numeric_limits<Integer>::max());
        }
    }

    if (!in_range) {
        throw nlohmann::json::other_error::create(
            605, std::string(name) + " must be a representable integer", &value);
    }
    return value.get<Integer>();
}

template <std::integral Integer>
[[nodiscard]] Integer checked_integer_or(const nlohmann::json& object,
                                         const char* key,
                                         Integer default_value,
                                         std::string_view name) {
    if (!object.contains(key)) return default_value;
    return checked_integer<Integer>(object.at(key), name);
}

template <std::integral Integer>
[[nodiscard]] Integer checked_decimal_integer(std::string_view encoded,
                                              std::string_view name,
                                              const nlohmann::json& context) {
    static_assert(!std::same_as<Integer, bool>);

    Integer value{};
    const auto* first = encoded.data();
    const auto* last = first + encoded.size();
    const auto result = std::from_chars(first, last, value);
    if (encoded.empty() || result.ec != std::errc{} || result.ptr != last) {
        throw nlohmann::json::other_error::create(
            605, std::string(name) + " must be a representable base-10 integer", &context);
    }
    return value;
}

} // namespace sunny::core::detail
