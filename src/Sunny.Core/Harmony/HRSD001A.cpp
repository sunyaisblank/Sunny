/**
 * @file HRSD001A.cpp
 * @brief Secondary dominant and applied chord implementation
 *
 * Component: HRSD001A
 */

#include "HRSD001A.h"
#include "HRRN001A.h"
#include "../Pitch/PTMN001A.h"

#include <algorithm>

namespace Sunny::Core {

namespace {

// Parse the applied function part (before the '/')
// Returns (is_vii_type, has_7)
bool parse_applied_function(std::string_view func, bool& is_vii_type, bool& has_seventh) {
    is_vii_type = false;
    has_seventh = false;

    if (func.empty()) return false;

    // Check for vii° / viio type
    if (func.starts_with("vii")) {
        is_vii_type = true;
        std::string_view suffix = func.substr(3);
        has_seventh = suffix.find('7') != std::string_view::npos;
        return true;
    }

    // Check for V type
    if (func[0] == 'V') {
        is_vii_type = false;
        has_seventh = func.find('7') != std::string_view::npos;
        return true;
    }

    return false;
}

// Parse target numeral to degree and is_major
bool parse_target(std::string_view target, int& degree, bool& is_major) {
    auto result = parse_roman_numeral(target);
    if (!result.has_value()) return false;
    degree = result->first;
    is_major = result->second;
    return true;
}

}  // namespace

Result<AppliedChord> parse_applied_chord(std::string_view notation) {
    auto slash_pos = notation.find('/');
    if (slash_pos == std::string_view::npos || slash_pos == 0 ||
        slash_pos == notation.size() - 1) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    std::string_view func_part = notation.substr(0, slash_pos);
    std::string_view target_part = notation.substr(slash_pos + 1);

    bool is_vii_type = false;
    bool has_seventh = false;
    if (!parse_applied_function(func_part, is_vii_type, has_seventh)) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    int target_degree = 0;
    bool target_is_major = false;
    if (!parse_target(target_part, target_degree, target_is_major)) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    AppliedChord ac;
    ac.applied_function = std::string(func_part);
    ac.target_degree = target_degree;
    ac.target_is_major = target_is_major;
    return ac;
}

Result<ChordVoicing> generate_secondary_dominant(
    std::string_view notation,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    int octave
) {
    auto parsed = parse_applied_chord(notation);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    const auto& ac = *parsed;

    // Validate target
    if (!is_valid_secondary_target(ac.target_degree, scale_intervals, key_root)) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    // Get target pitch class from scale
    if (ac.target_degree < 0 ||
        static_cast<std::size_t>(ac.target_degree) >= scale_intervals.size()) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    PitchClass target_pc = transpose(key_root, scale_intervals[ac.target_degree]);

    // Determine the applied function type
    bool is_vii_type = ac.applied_function.starts_with("vii");
    bool has_seventh = ac.applied_function.find('7') != std::string::npos;

    PitchClass chord_root;
    std::string_view quality;

    if (is_vii_type) {
        // vii° of target: root is a semitone below target
        chord_root = transpose(target_pc, 11);
        if (has_seventh) {
            quality = "dim7";
        } else {
            quality = "diminished";
        }
    } else {
        // V of target: root is a perfect fifth below target (= P4 above = 7 semitones below)
        chord_root = transpose(target_pc, 7);
        if (has_seventh) {
            quality = "7";
        } else {
            quality = "major";
        }
    }

    return generate_chord(chord_root, quality, octave);
}

bool is_valid_secondary_target(
    int target_degree,
    std::span<const Interval> scale_intervals,
    PitchClass /*key_root*/
) {
    if (target_degree < 0 || target_degree > 6) return false;

    // Degree 0 (I/i) is already tonic; V/I is the primary dominant, not secondary
    if (target_degree == 0) return false;

    // Degree 6 (vii°) is diminished; cannot be a valid secondary target
    // in common-practice harmony (no stable resolution point)
    if (target_degree == 6) return false;

    return true;
}

}  // namespace Sunny::Core
