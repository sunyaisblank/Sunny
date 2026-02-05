/**
 * @file HRRN001A.cpp
 * @brief Roman numeral parsing and chord generation
 *
 * Component: HRRN001A
 */

#include "HRRN001A.h"
#include "../Pitch/PTMN001A.h"
#include "../Scale/SCDF001A.h"

#include <algorithm>
#include <cctype>
#include <unordered_map>

namespace Sunny::Core {

namespace {

// Chord quality intervals
const std::unordered_map<std::string_view, std::vector<Interval>> QUALITY_INTERVALS = {
    {"major", {0, 4, 7}},
    {"minor", {0, 3, 7}},
    {"diminished", {0, 3, 6}},
    {"dim", {0, 3, 6}},
    {"augmented", {0, 4, 8}},
    {"aug", {0, 4, 8}},
    {"sus2", {0, 2, 7}},
    {"sus4", {0, 5, 7}},
    {"sus", {0, 5, 7}},
    {"7", {0, 4, 7, 10}},
    {"dom7", {0, 4, 7, 10}},
    {"dominant7", {0, 4, 7, 10}},
    {"maj7", {0, 4, 7, 11}},
    {"major7", {0, 4, 7, 11}},
    {"m7", {0, 3, 7, 10}},
    {"min7", {0, 3, 7, 10}},
    {"minor7", {0, 3, 7, 10}},
    {"dim7", {0, 3, 6, 9}},
    {"diminished7", {0, 3, 6, 9}},
    {"m7b5", {0, 3, 6, 10}},
    {"half-diminished", {0, 3, 6, 10}},
    {"mM7", {0, 3, 7, 11}},
    {"minMaj7", {0, 3, 7, 11}},
    {"add9", {0, 4, 7, 14}},
    {"6", {0, 4, 7, 9}},
    {"m6", {0, 3, 7, 9}},
    {"9", {0, 4, 7, 10, 14}},
    {"maj9", {0, 4, 7, 11, 14}},
    {"m9", {0, 3, 7, 10, 14}},
};

// Parse Roman numeral base
bool parse_numeral_base(std::string_view s, int& degree, bool& is_upper) {
    if (s.empty()) return false;

    // Check case
    is_upper = std::isupper(static_cast<unsigned char>(s[0]));

    // Convert to lowercase for matching
    std::string lower;
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) {
            lower += std::tolower(static_cast<unsigned char>(c));
        } else {
            break;
        }
    }

    if (lower == "i") degree = 0;
    else if (lower == "ii") degree = 1;
    else if (lower == "iii") degree = 2;
    else if (lower == "iv") degree = 3;
    else if (lower == "v") degree = 4;
    else if (lower == "vi") degree = 5;
    else if (lower == "vii") degree = 6;
    else return false;

    return true;
}

}  // namespace

Result<std::pair<int, bool>> parse_roman_numeral(std::string_view numeral) {
    if (numeral.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    int degree;
    bool is_upper;

    if (!parse_numeral_base(numeral, degree, is_upper)) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    return std::make_pair(degree, is_upper);
}

Result<ChordVoicing> generate_chord_from_numeral(
    std::string_view numeral,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    int octave
) {
    if (numeral.empty()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    // Parse degree and case
    int degree;
    bool is_upper;
    if (!parse_numeral_base(numeral, degree, is_upper)) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    // Get root from scale degree
    if (degree < 0 || static_cast<std::size_t>(degree) >= scale_intervals.size()) {
        return std::unexpected(ErrorCode::InvalidRomanNumeral);
    }

    PitchClass chord_root = transpose(key_root, scale_intervals[degree]);

    // Determine chord quality from modifiers
    std::string quality;
    bool has_dim = numeral.find("°") != std::string_view::npos ||
                   numeral.find('o') != std::string_view::npos ||
                   numeral.find("dim") != std::string_view::npos;
    bool has_aug = numeral.find('+') != std::string_view::npos;
    bool has_7 = numeral.find('7') != std::string_view::npos;
    bool has_half_dim = numeral.find("ø") != std::string_view::npos ||
                        numeral.find("o7") != std::string_view::npos;

    if (has_half_dim) {
        quality = "m7b5";
    } else if (has_dim) {
        quality = has_7 ? "dim7" : "diminished";
    } else if (has_aug) {
        quality = "augmented";
    } else if (is_upper) {
        quality = has_7 ? "7" : "major";
    } else {
        quality = has_7 ? "m7" : "minor";
    }

    return generate_chord(chord_root, quality, octave);
}

Result<ChordVoicing> generate_chord(
    PitchClass root,
    std::string_view quality,
    int octave
) {
    auto intervals_opt = chord_quality_intervals(quality);
    if (!intervals_opt) {
        return std::unexpected(ErrorCode::InvalidChordQuality);
    }

    const auto& intervals = *intervals_opt;

    auto base_midi = pitch_octave_to_midi(root, octave);
    if (!base_midi) {
        return std::unexpected(ErrorCode::ChordGenerationFailed);
    }

    ChordVoicing voicing;
    voicing.root = root;
    voicing.quality = std::string(quality);
    voicing.inversion = 0;

    for (Interval interval : intervals) {
        int midi = *base_midi + interval;
        if (midi >= 0 && midi <= 127) {
            voicing.notes.push_back(static_cast<MidiNote>(midi));
        }
    }

    return voicing;
}

std::optional<std::vector<Interval>> chord_quality_intervals(std::string_view quality) {
    auto it = QUALITY_INTERVALS.find(quality);
    if (it != QUALITY_INTERVALS.end()) {
        return it->second;
    }

    // Try lowercase
    std::string lower;
    for (char c : quality) {
        lower += std::tolower(static_cast<unsigned char>(c));
    }

    it = QUALITY_INTERVALS.find(lower);
    if (it != QUALITY_INTERVALS.end()) {
        return it->second;
    }

    return std::nullopt;
}

}  // namespace Sunny::Core
