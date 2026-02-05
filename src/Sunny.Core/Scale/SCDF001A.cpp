/**
 * @file SCDF001A.cpp
 * @brief Scale definitions implementation
 *
 * Component: SCDF001A
 */

#include "SCDF001A.h"

#include <algorithm>
#include <array>
#include <cctype>

namespace Sunny::Core {

namespace {

// Helper to create ScaleDefinition with proper array initialization
template<std::size_t N>
constexpr ScaleDefinition make_scale(
    std::string_view name,
    const std::array<Interval, N>& intervals,
    std::string_view description
) {
    ScaleDefinition def{};
    def.name = name;
    def.note_count = static_cast<std::uint8_t>(N);
    def.description = description;
    for (std::size_t i = 0; i < N && i < 12; ++i) {
        def.intervals[i] = intervals[i];
    }
    return def;
}

// Built-in scale database
constexpr std::array<ScaleDefinition, 35> SCALE_DATABASE = {{
    // Diatonic modes
    make_scale("major", SCALE_MAJOR, "Ionian mode"),
    make_scale("ionian", SCALE_MAJOR, "Major scale"),
    make_scale("minor", SCALE_MINOR, "Natural minor / Aeolian"),
    make_scale("aeolian", SCALE_MINOR, "Natural minor"),
    make_scale("dorian", SCALE_DORIAN, "Minor with raised 6th"),
    make_scale("phrygian", SCALE_PHRYGIAN, "Minor with lowered 2nd"),
    make_scale("lydian", SCALE_LYDIAN, "Major with raised 4th"),
    make_scale("mixolydian", SCALE_MIXOLYDIAN, "Major with lowered 7th"),
    make_scale("locrian", SCALE_LOCRIAN, "Diminished mode"),

    // Harmonic and melodic minor variants
    make_scale("harmonic_minor", SCALE_HARMONIC_MINOR, "Minor with raised 7th"),
    make_scale("melodic_minor", SCALE_MELODIC_MINOR, "Jazz minor (ascending)"),

    // Pentatonic scales
    make_scale("pentatonic_major", SCALE_PENTATONIC_MAJOR, "Major without 4th and 7th"),
    make_scale("pentatonic_minor", SCALE_PENTATONIC_MINOR, "Minor pentatonic"),

    // Blues and jazz
    make_scale("blues", SCALE_BLUES, "Minor pentatonic with blue note"),
    make_scale("whole_tone", SCALE_WHOLE_TONE, "Symmetric whole tone"),
    make_scale("diminished_hw", SCALE_DIMINISHED_HW, "Octatonic half-whole"),
    make_scale("diminished_wh", SCALE_DIMINISHED_WH, "Octatonic whole-half"),
    make_scale("chromatic", SCALE_CHROMATIC, "All 12 semitones"),

    // Additional modes (from harmonic/melodic minor)
    make_scale("phrygian_dominant", std::array<Interval, 7>{0, 1, 4, 5, 7, 8, 10},
               "5th mode of harmonic minor"),
    make_scale("lydian_dominant", std::array<Interval, 7>{0, 2, 4, 6, 7, 9, 10},
               "4th mode of melodic minor"),
    make_scale("super_locrian", std::array<Interval, 7>{0, 1, 3, 4, 6, 8, 10},
               "Altered scale / 7th mode melodic minor"),
    make_scale("lydian_augmented", std::array<Interval, 7>{0, 2, 4, 6, 8, 9, 11},
               "3rd mode of melodic minor"),
    make_scale("locrian_natural2", std::array<Interval, 7>{0, 2, 3, 5, 6, 8, 10},
               "6th mode of melodic minor"),

    // World scales
    make_scale("hungarian_minor", std::array<Interval, 7>{0, 2, 3, 6, 7, 8, 11},
               "Gypsy minor"),
    make_scale("double_harmonic", std::array<Interval, 7>{0, 1, 4, 5, 7, 8, 11},
               "Byzantine / Arabic"),
    make_scale("hirajoshi", std::array<Interval, 5>{0, 2, 3, 7, 8},
               "Japanese pentatonic"),
    make_scale("in_sen", std::array<Interval, 5>{0, 1, 5, 7, 10},
               "Japanese In scale"),
    make_scale("kumoi", std::array<Interval, 5>{0, 2, 3, 7, 9},
               "Japanese Kumoi"),
    make_scale("pelog", std::array<Interval, 5>{0, 1, 3, 7, 8},
               "Balinese Pelog"),
    make_scale("iwato", std::array<Interval, 5>{0, 1, 5, 6, 10},
               "Japanese Iwato"),

    // Bebop scales
    make_scale("bebop_major", std::array<Interval, 8>{0, 2, 4, 5, 7, 8, 9, 11},
               "Major with added #5"),
    make_scale("bebop_dominant", std::array<Interval, 8>{0, 2, 4, 5, 7, 9, 10, 11},
               "Mixolydian with added 7"),
    make_scale("bebop_minor", std::array<Interval, 8>{0, 2, 3, 5, 7, 8, 9, 10},
               "Dorian with added 3"),

    // Synthetic
    make_scale("prometheus", std::array<Interval, 6>{0, 2, 4, 6, 9, 10},
               "Scriabin's mystic chord scale"),
    make_scale("augmented", std::array<Interval, 6>{0, 3, 4, 7, 8, 11},
               "Symmetric augmented"),
}};

// Scale name list for enumeration
constexpr std::array<std::string_view, 35> SCALE_NAMES = {{
    "major", "ionian", "minor", "aeolian", "dorian", "phrygian", "lydian",
    "mixolydian", "locrian", "harmonic_minor", "melodic_minor",
    "pentatonic_major", "pentatonic_minor", "blues", "whole_tone",
    "diminished_hw", "diminished_wh", "chromatic", "phrygian_dominant",
    "lydian_dominant", "super_locrian", "lydian_augmented", "locrian_natural2",
    "hungarian_minor", "double_harmonic", "hirajoshi", "in_sen", "kumoi",
    "pelog", "iwato", "bebop_major", "bebop_dominant", "bebop_minor",
    "prometheus", "augmented"
}};

// Case-insensitive string comparison
bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::optional<ScaleDefinition> find_scale(std::string_view name) {
    for (const auto& scale : SCALE_DATABASE) {
        if (iequals(scale.name, name)) {
            return scale;
        }
    }
    return std::nullopt;
}

std::span<const std::string_view> list_scale_names() {
    return SCALE_NAMES;
}

constexpr std::size_t scale_count() noexcept {
    return SCALE_DATABASE.size();
}

}  // namespace Sunny::Core
