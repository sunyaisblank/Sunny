/**
 * @file HRRN001A.h
 * @brief Roman numeral parsing and chord generation
 *
 * Component: HRRN001A
 * Domain: HR (Harmony) | Category: RN (Roman Numeral)
 *
 * Parse Roman numeral notation and generate corresponding chords.
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Pitch/PTPC001A.h"

#include <span>
#include <string_view>

namespace Sunny::Core {

/**
 * @brief Parse Roman numeral to scale degree and quality
 *
 * Handles: I-VII, i-vii, with modifiers °, +, 7, maj7, etc.
 *
 * @param numeral Roman numeral string
 * @return Pair of (degree 0-6, is_major) or error
 */
[[nodiscard]] Result<std::pair<int, bool>> parse_roman_numeral(
    std::string_view numeral
);

/**
 * @brief Generate chord voicing from Roman numeral
 *
 * @param numeral Roman numeral (e.g., "I", "IV", "V7", "ii")
 * @param key_root Key root pitch class
 * @param scale_intervals Scale intervals for chord construction
 * @param octave Base octave for voicing
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> generate_chord_from_numeral(
    std::string_view numeral,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    int octave = 4
);

/**
 * @brief Generate chord from root and quality
 *
 * @param root Root pitch class
 * @param quality Chord quality string
 * @param octave Base octave
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> generate_chord(
    PitchClass root,
    std::string_view quality,
    int octave = 4
);

/**
 * @brief Get chord intervals for a quality
 *
 * @param quality Chord quality (major, minor, dim, aug, 7, maj7, etc.)
 * @return Intervals from root or nullopt
 */
[[nodiscard]] std::optional<std::vector<Interval>> chord_quality_intervals(
    std::string_view quality
);

}  // namespace Sunny::Core
