/**
 * @file applied_chords.cpp
 * @brief Secondary dominant and applied chord implementation
 *
 */

#include <algorithm>
#include <string>
#include <sunny/core/harmony/applied_chords.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/midi_note.hpp>

namespace sunny::core {

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

} // namespace

Result<AppliedChord> parse_applied_chord(std::string_view notation) {
    auto slash_pos = notation.find('/');
    if (slash_pos == std::string_view::npos || slash_pos == 0 || slash_pos == notation.size() - 1) {
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

Result<ChordVoicing> generate_secondary_dominant(std::string_view notation,
                                                 PitchClass key_root,
                                                 std::span<const Interval> scale_intervals,
                                                 int octave) {
    auto parsed = parse_applied_chord(notation);
    if (!parsed.has_value()) {
        return std::unexpected(parsed.error());
    }

    const auto& ac = *parsed;

    // Validate target
    if (!is_valid_secondary_target_degree(ac.target_degree)) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    // Get target pitch class from scale
    if (ac.target_degree < 0 ||
        static_cast<std::size_t>(ac.target_degree) >= scale_intervals.size()) {
        return std::unexpected(ErrorCode::InvalidAppliedChord);
    }

    // Realisation belongs to the numeral grammar, which reads X/Y as X in the
    // key of Y. An applied leading-tone chord is diminished by definition, so
    // a bare "vii" is given its diminished mark before delegation.
    std::string numeral(notation);
    if (ac.applied_function.starts_with("vii")) {
        const std::string_view after = std::string_view(ac.applied_function).substr(3);
        const bool marked = after.starts_with("\xC2\xB0") || after.starts_with("\xC3\xB8") ||
                            after.starts_with("o");
        if (!marked) numeral.insert(3, "\xC2\xB0");
    }
    auto chord = generate_chord_from_numeral(numeral, key_root, scale_intervals, octave);
    if (!chord) return std::unexpected(ErrorCode::InvalidAppliedChord);
    return chord;
}

bool is_valid_secondary_target_degree(int target_degree) {
    if (target_degree < 0 || target_degree > 6) return false;

    // Degree 0 (I/i) is already tonic; V/I is the primary dominant, not secondary
    if (target_degree == 0) return false;

    // Degree 6 (vii°) is diminished; cannot be a valid secondary target
    // in common-practice harmony (no stable resolution point)
    if (target_degree == 6) return false;

    return true;
}

} // namespace sunny::core
