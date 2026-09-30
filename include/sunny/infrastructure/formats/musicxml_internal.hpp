/**
 * @file musicxml_internal.hpp
 * @brief Shared MusicXML helper functions
 *
 * Internal header shared between  (low-level reader/writer) and
 *  (Score IR compiler). Not part of the public interface.
 *
 * Factored from.cpp's anonymous namespace to eliminate duplication
 * across the two MusicXML components.
 */

#pragma once

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/score/document.hpp>
#include <sunny/core/types/beat.hpp>

namespace sunny::infrastructure::formats::MxmlInternal {

inline const char* chord_numeral_mode_to_musicxml(sunny::core::ChordNumeralMode mode) {
    using Mode = sunny::core::ChordNumeralMode;
    switch (mode) {
    case Mode::Major:
        return "major";
    case Mode::Minor:
        return "minor";
    case Mode::NaturalMinor:
        return "natural minor";
    case Mode::MelodicMinor:
        return "melodic minor";
    case Mode::HarmonicMinor:
        return "harmonic minor";
    }
    return "";
}

inline std::optional<sunny::core::ChordNumeralMode>
musicxml_to_chord_numeral_mode(std::string_view mode) {
    using Mode = sunny::core::ChordNumeralMode;
    if (mode == "major") return Mode::Major;
    if (mode == "minor") return Mode::Minor;
    if (mode == "natural minor") return Mode::NaturalMinor;
    if (mode == "melodic minor") return Mode::MelodicMinor;
    if (mode == "harmonic minor") return Mode::HarmonicMinor;
    return std::nullopt;
}

inline const char* chord_degree_type_to_musicxml(sunny::core::ChordDegreeType type) {
    using Type = sunny::core::ChordDegreeType;
    switch (type) {
    case Type::Add:
        return "add";
    case Type::Alter:
        return "alter";
    case Type::Subtract:
        return "subtract";
    }
    return "";
}

inline std::optional<sunny::core::ChordDegreeType>
musicxml_to_chord_degree_type(std::string_view type) {
    using Type = sunny::core::ChordDegreeType;
    if (type == "add") return Type::Add;
    if (type == "alter") return Type::Alter;
    if (type == "subtract") return Type::Subtract;
    return std::nullopt;
}

inline bool valid_musicxml_harmony_kind(std::string_view kind) {
    constexpr std::array kinds{
        std::string_view{"major"},
        std::string_view{"minor"},
        std::string_view{"augmented"},
        std::string_view{"diminished"},
        std::string_view{"dominant"},
        std::string_view{"major-seventh"},
        std::string_view{"minor-seventh"},
        std::string_view{"diminished-seventh"},
        std::string_view{"augmented-seventh"},
        std::string_view{"half-diminished"},
        std::string_view{"major-minor"},
        std::string_view{"major-sixth"},
        std::string_view{"minor-sixth"},
        std::string_view{"dominant-ninth"},
        std::string_view{"major-ninth"},
        std::string_view{"minor-ninth"},
        std::string_view{"dominant-11th"},
        std::string_view{"major-11th"},
        std::string_view{"minor-11th"},
        std::string_view{"dominant-13th"},
        std::string_view{"major-13th"},
        std::string_view{"minor-13th"},
        std::string_view{"suspended-second"},
        std::string_view{"suspended-fourth"},
        std::string_view{"Neapolitan"},
        std::string_view{"Italian"},
        std::string_view{"French"},
        std::string_view{"German"},
        std::string_view{"pedal"},
        std::string_view{"power"},
        std::string_view{"Tristan"},
        std::string_view{"other"},
        std::string_view{"none"},
    };
    return std::find(kinds.begin(), kinds.end(), kind) != kinds.end();
}

/// Letter index to MusicXML step character
constexpr std::array<char, 7> STEP_CHARS = {'C', 'D', 'E', 'F', 'G', 'A', 'B'};

/// MusicXML step character to letter index; returns -1 for invalid input
inline int step_to_letter(char step) {
    switch (step) {
    case 'C':
        return 0;
    case 'D':
        return 1;
    case 'E':
        return 2;
    case 'F':
        return 3;
    case 'G':
        return 4;
    case 'A':
        return 5;
    case 'B':
        return 6;
    default:
        return -1;
    }
}

/// Convert line-of-fifths key centre to MusicXML <fifths> value.
/// For major keys the LoF position gives the fifths count directly;
/// minor keys add 3 (relative major is 3 LoF positions up).
inline int lof_to_fifths(sunny::core::SpelledPitch tonic, bool is_major) {
    int lof = sunny::core::line_of_fifths_position(tonic);
    if (!is_major) lof += 3;
    return lof;
}

/// Convert MusicXML <fifths> to a SpelledPitch tonic
inline sunny::core::SpelledPitch fifths_to_tonic(int fifths, bool is_major) {
    int lof = fifths;
    if (!is_major) lof += 3;
    return sunny::core::from_line_of_fifths(lof, 4);
}

/// Convert a non-negative Beat value to exact integer MusicXML duration units.
/// Returns nullopt if the Beat is negative, is not exactly representable at
/// the selected divisions value, or would exceed this implementation's
/// int-valued MusicXML timing domain.
inline std::optional<int> checked_beat_to_mxml_duration(sunny::core::Beat value, int divisions) {
    if (value.numerator() < 0 || divisions <= 0) return std::nullopt;
    if (value.numerator() == 0) return 0;

    const auto numerator_divisor = std::gcd(static_cast<std::uint64_t>(value.numerator()),
                                            static_cast<std::uint64_t>(value.denominator()));
    auto numerator = static_cast<std::uint64_t>(value.numerator()) / numerator_divisor;
    auto denominator = static_cast<std::uint64_t>(value.denominator()) / numerator_divisor;

    auto quarter_factor = std::uint64_t{4};
    const auto quarter_divisor = std::gcd(quarter_factor, denominator);
    quarter_factor /= quarter_divisor;
    denominator /= quarter_divisor;

    auto divisions_factor = static_cast<std::uint64_t>(divisions);
    const auto divisions_divisor = std::gcd(divisions_factor, denominator);
    divisions_factor /= divisions_divisor;
    denominator /= divisions_divisor;
    if (denominator != 1) return std::nullopt;

    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    if (numerator > maximum / quarter_factor) return std::nullopt;
    numerator *= quarter_factor;
    if (numerator > maximum / divisions_factor) return std::nullopt;
    return static_cast<int>(numerator * divisions_factor);
}

/// Convert a Beat after the containing compiler has preflighted every timing
/// value with checked_beat_to_mxml_duration.
inline int beat_to_mxml_duration(sunny::core::Beat value, int divisions) {
    const auto result = checked_beat_to_mxml_duration(value, divisions);
    assert(result.has_value() && "MusicXML timing value was not preflighted");
    return result.value_or(0);
}

/// Convert MusicXML duration units to Beat
inline sunny::core::Beat mxml_duration_to_beat(int units, int divisions) {
    return sunny::core::Beat{units, 4 * static_cast<int64_t>(divisions)};
}

struct MusicXmlGraphicDuration {
    std::string_view type;
    int dots;
};

/// Derive an exact MusicXML graphic note type and dot count where possible.
/// MusicXML's type vocabulary is finite, while duration itself is independent
/// and exact. Returning nullopt lets writers omit optional graphic data rather
/// than falsely labelling an arbitrary rational duration as a quarter note.
inline std::optional<MusicXmlGraphicDuration>
musicxml_graphic_duration(sunny::core::Beat duration) {
    if (duration.numerator() <= 0) return std::nullopt;
    const auto reduced = duration.reduce();
    struct BaseDuration {
        std::string_view type;
        int numerator;
        int denominator;
    };
    constexpr std::array<BaseDuration, 14> base_types = {{
        {"1024th", 1, 1024},
        {"512th", 1, 512},
        {"256th", 1, 256},
        {"128th", 1, 128},
        {"64th", 1, 64},
        {"32nd", 1, 32},
        {"16th", 1, 16},
        {"eighth", 1, 8},
        {"quarter", 1, 4},
        {"half", 1, 2},
        {"whole", 1, 1},
        {"breve", 2, 1},
        {"long", 4, 1},
        {"maxima", 8, 1},
    }};

    for (const auto& base : base_types) {
        const sunny::core::Beat base_duration{base.numerator, base.denominator};
        for (int dots = 0; dots <= 62; ++dots) {
            const auto denominator = std::uint64_t{1} << dots;
            const auto numerator = (denominator << 1) - 1;
            const auto candidate =
                sunny::core::checked_mul(base_duration,
                                         sunny::core::Beat{static_cast<std::int64_t>(numerator),
                                                           static_cast<std::int64_t>(denominator)});
            if (candidate && *candidate == reduced) return MusicXmlGraphicDuration{base.type, dots};
        }
    }
    return std::nullopt;
}

/// Compute divisions (LCM of denominators) needed for a set of Beat durations.
/// Accepts a callable that iterates all durations. Template to avoid header
/// dependency on MusicXmlScore.
template <typename DurationIterator>
std::optional<int> compute_divisions_from(DurationIterator begin, DurationIterator end) {
    std::uint64_t lcm_value = 1;
    constexpr auto maximum = static_cast<std::uint64_t>(std::numeric_limits<int>::max());
    for (auto it = begin; it != end; ++it) {
        if (it->numerator() < 0) return std::nullopt;
        if (it->numerator() == 0) continue;

        auto denominator = static_cast<std::uint64_t>(it->denominator());
        denominator /= std::gcd(static_cast<std::uint64_t>(it->numerator()), denominator);
        denominator /= std::gcd(std::uint64_t{4}, denominator);

        const auto common = std::gcd(lcm_value, denominator);
        const auto factor = denominator / common;
        if (lcm_value > maximum / factor) return std::nullopt;
        lcm_value *= factor;
    }
    return static_cast<int>(lcm_value);
}

} // namespace sunny::infrastructure::formats::MxmlInternal
