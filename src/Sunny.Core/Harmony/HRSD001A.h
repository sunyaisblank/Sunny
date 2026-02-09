/**
 * @file HRSD001A.h
 * @brief Secondary dominant and applied chord analysis
 *
 * Component: HRSD001A
 * Domain: HR (Harmony) | Category: SD (Secondary Dominant)
 *
 * Parse applied chord notation (V/V, V7/ii, vii°/V) and generate
 * the corresponding chord voicings.
 *
 * Invariants:
 * - V/V in C major = D major (root = 2)
 * - Applied function must be V-type or vii°-type
 * - Target degree must support a secondary dominant (not I or vii°)
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Pitch/PTPC001A.h"

#include <span>
#include <string>
#include <string_view>

namespace Sunny::Core {

/**
 * @brief Parsed applied chord notation
 */
struct AppliedChord {
    std::string applied_function;  ///< "V", "V7", "vii°", "viio7"
    int target_degree;             ///< 0-6 (scale degree of target)
    bool target_is_major;          ///< Whether target chord is major
};

/**
 * @brief Parse applied chord notation (e.g. "V/V", "V7/ii")
 *
 * Splits on '/', parses both halves.
 *
 * @param notation Applied chord string
 * @return AppliedChord or error
 */
[[nodiscard]] Result<AppliedChord> parse_applied_chord(std::string_view notation);

/**
 * @brief Generate a secondary dominant chord voicing
 *
 * @param notation Applied chord notation (e.g. "V/V", "V7/ii")
 * @param key_root Key root pitch class
 * @param scale_intervals Scale intervals for the key
 * @param octave Base octave for voicing
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> generate_secondary_dominant(
    std::string_view notation,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    int octave = 4
);

/**
 * @brief Check if a scale degree is a valid secondary dominant target
 *
 * V/I and V/vii° are invalid (I is tonic, vii° is diminished and
 * does not support a dominant relationship).
 *
 * @param target_degree Scale degree (0-6)
 * @param scale_intervals Scale interval array
 * @param key_root Key root pitch class
 * @return true if the degree can be a secondary dominant target
 */
[[nodiscard]] bool is_valid_secondary_target(
    int target_degree,
    std::span<const Interval> scale_intervals,
    PitchClass key_root
);

}  // namespace Sunny::Core
