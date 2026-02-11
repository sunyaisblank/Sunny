/**
 * @file TUHT001A.cpp
 * @brief Historical temperaments
 *
 * Component: TUHT001A
 */

#include "TUHT001A.h"

#include <span>

namespace Sunny::Core {

namespace {

constexpr Temperament TEMPERAMENTS[] = {
    {"equal", TUNING_EQUAL, "12-tone equal temperament"},
    {"pythagorean", TUNING_PYTHAGOREAN, "Pure fifths except wolf G#-Eb"},
    {"quarter_comma_meantone", TUNING_QUARTER_COMMA_MEANTONE,
     "Pure major thirds, fifths narrowed by 1/4 syntonic comma"},
    {"werckmeister_iii", TUNING_WERCKMEISTER_III,
     "4 fifths narrowed by 1/4 Pythagorean comma"},
    {"vallotti", TUNING_VALLOTTI,
     "6 fifths tempered by 1/6 Pythagorean comma"},
};

constexpr std::string_view TEMPERAMENT_NAMES[] = {
    "equal", "pythagorean", "quarter_comma_meantone",
    "werckmeister_iii", "vallotti",
};

}  // namespace

std::optional<Temperament> find_temperament(std::string_view name) {
    for (const auto& t : TEMPERAMENTS) {
        if (t.name == name) return t;
    }
    return std::nullopt;
}

std::span<const std::string_view> list_temperament_names() {
    return TEMPERAMENT_NAMES;
}

}  // namespace Sunny::Core
