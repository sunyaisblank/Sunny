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

#include "../Pitch/PTPS001A.h"

#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace Sunny::Core {

/**
 * @brief Inversion encoding from figured bass suffix
 *
 * Formal Spec §6.2: Inversion suffixes map to chord inversions.
 */
enum class FiguredBass : std::uint8_t {
    Root = 0,       ///< Root position (no suffix, or 5/3)
    First = 1,      ///< First inversion (6 for triads, 6/5 for 7ths)
    Second = 2,     ///< Second inversion (6/4 for triads, 4/3 for 7ths)
    Third = 3       ///< Third inversion (4/2 for 7ths only)
};

/**
 * @brief Fully parsed Roman numeral per §6.2 BNF
 *
 * roman_numeral ::= [applied_prefix] degree [quality_modifier] [extension] [inversion]
 */
struct ParsedNumeral {
    int degree;                 ///< Scale degree 0-6
    bool is_upper;              ///< Uppercase = major, lowercase = minor
    int accidental;             ///< Chromatic offset: -1 for ♭, +1 for ♯, 0 for natural
    std::string quality_mod;    ///< "°", "+", "ø", or empty
    std::string extension;      ///< "7", "9", "11", "13", "maj7", "maj9", etc.
    FiguredBass inversion;      ///< Figured bass inversion
    bool is_neapolitan;         ///< True if "N" (= bII)
};

/**
 * @brief Parse Roman numeral to scale degree and quality (simple)
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
 * @brief Parse Roman numeral with full §6.2 BNF grammar
 *
 * Handles chromatic alterations (♭VII, ♯IV, bVII, #IV),
 * Neapolitan (N), quality modifiers (°, +, ø), extensions,
 * and figured bass inversion suffixes.
 *
 * @param numeral Roman numeral string
 * @return ParsedNumeral or error
 */
[[nodiscard]] Result<ParsedNumeral> parse_roman_numeral_full(
    std::string_view numeral
);

/**
 * @brief Generate chord voicing from Roman numeral
 *
 * Supports the full §6.2 grammar including chromatic alterations,
 * Neapolitan, extensions, and inversions.
 *
 * @param numeral Roman numeral (e.g., "I", "bVII", "V7", "ii65", "N")
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

/**
 * @brief Recognize chord root and quality from a pitch class set (§16.1.5)
 *
 * Key-independent chord identification. For each candidate root,
 * computes intervals above root and matches against the chord
 * quality registry.
 *
 * @param pcs Pitch class set
 * @return (root, quality_name) or nullopt if unrecognised
 */
[[nodiscard]] std::optional<std::pair<PitchClass, std::string>> recognize_chord(
    const PitchClassSet& pcs
);

/**
 * @brief Generate roman numeral string from chord in a key (§16.1.6)
 *
 * Reverse of parse_roman_numeral: given a chord root, quality,
 * and key context, produces a canonical roman numeral string.
 *
 * @param chord_root Chord root pitch class
 * @param quality Chord quality string
 * @param key_root Key root pitch class
 * @param scale_intervals Scale intervals
 * @param is_minor Whether the key is minor
 * @return Roman numeral string or error
 */
[[nodiscard]] Result<std::string> chord_to_numeral(
    PitchClass chord_root,
    std::string_view quality,
    PitchClass key_root,
    std::span<const Interval> scale_intervals,
    bool is_minor = false
);

}  // namespace Sunny::Core
