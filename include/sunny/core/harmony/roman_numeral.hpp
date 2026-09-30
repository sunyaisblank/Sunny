/**
 * @file roman_numeral.hpp
 * @brief Roman numeral parsing and chord generation
 *
 *
 * Parse Roman numeral notation and generate corresponding chords.
 */

#pragma once

#include <span>
#include <string>
#include <string_view>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/pitch/pitch_class_set.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <utility>

namespace sunny::core {

/**
 * @brief Inversion encoding from figured bass suffix
 *
 * Formal Spec §6.2: Inversion suffixes map to chord inversions.
 */
enum class FiguredBass : std::uint8_t {
    Root = 0,   ///< Root position (no suffix, or 5/3)
    First = 1,  ///< First inversion (6 for triads, 6/5 for 7ths)
    Second = 2, ///< Second inversion (6/4 for triads, 4/3 for 7ths)
    Third = 3   ///< Third inversion (4/2 for 7ths only)
};

/**
 * @brief Fully parsed Roman numeral per §6.2 BNF
 *
 * roman_numeral ::= [applied_prefix] degree [quality_modifier] [extension] [inversion]
 */
struct ParsedNumeral {
    int degree;              ///< Scale degree 0-6
    bool is_upper;           ///< Uppercase = major, lowercase = minor
    int accidental;          ///< Chromatic offset: -1 for ♭, +1 for ♯, 0 for natural
    std::string quality_mod; ///< "°", "+", "ø", or empty
    std::string extension;   ///< "7", "9", "11", "13", "maj7", "maj9", etc.
    FiguredBass inversion;   ///< Figured bass inversion
    bool is_neapolitan;      ///< True if "N" (= bII)
};

/**
 * @brief A scale degree with its chromatic alteration
 */
struct SpelledDegree {
    int degree;     ///< Scale degree 0-6
    int accidental; ///< -1 lowered (flat), +1 raised (sharp), 0 unaltered
};

/**
 * @brief Spell a root lying some semitones above the tonic as an altered degree
 *
 * The single authority for Roman-numeral degree spelling, after Kostka-Payne
 * and Aldwell-Schachter:
 * - a diatonic interval takes its degree unaltered;
 * - in a minor key (is_minor) the raised leading tone, 11 semitones above the
 *   tonic, is read as the seventh degree unaltered (vii°, and V contains it);
 * - intervals 1, 3, 8 and 10 are lowered degrees (bII, bIII, bVI, bVII: the
 *   Neapolitan and the chords of the parallel minor);
 * - every other chromatic interval is a raised degree (#IV, and in minor the
 *   raised third and sixth of the parallel major, #III and #VI).
 * If the preferred neighbour is not in the scale the other is used.
 *
 * @param semitones_above_tonic Interval from the tonic, any integer (reduced mod 12)
 * @param scale_intervals Heptatonic scale, ascending from 0
 * @param is_minor Whether the key is minor
 * @return The spelled degree, or InvalidRomanNumeral if neither neighbour of a
 *         chromatic interval is a scale degree below the seventh
 */
[[nodiscard]] Result<SpelledDegree> spell_scale_degree(int semitones_above_tonic,
                                                       std::span<const Interval> scale_intervals,
                                                       bool is_minor);

/**
 * @brief Parse Roman numeral to scale degree and quality (simple)
 *
 * Handles: I-VII, i-vii, with modifiers °, +, 7, maj7, etc.
 *
 * @param numeral Roman numeral string
 * @return Pair of (degree 0-6, is_major) or error
 */
[[nodiscard]] Result<std::pair<int, bool>> parse_roman_numeral(std::string_view numeral);

/**
 * @brief Parse Roman numeral with full §6.2 BNF grammar
 *
 * Handles chromatic alterations (♭VII, ♯IV, bVII, #IV, biii),
 * Neapolitan (N), quality modifiers (°, +, ø), extensions,
 * and figured bass inversion suffixes. The extension must be one of
 * 7, 9, 11, 13, maj7, maj9, maj11, maj13, b9, #9, #11, b13 and alt (each
 * altered form optionally preceded by 7); any other remainder, such as the
 * "t+" of "It+6", is InvalidRomanNumeral rather than being ignored. Applied
 * numerals ("V/V") are not single numerals and are refused here; see
 * generate_chord_from_numeral.
 *
 * @param numeral Roman numeral string
 * @return ParsedNumeral or error
 */
[[nodiscard]] Result<ParsedNumeral> parse_roman_numeral_full(std::string_view numeral);

/**
 * @brief Generate chord voicing from Roman numeral
 *
 * Supports the full §6.2 grammar including chromatic alterations,
 * Neapolitan, extensions, and inversions.
 *
 * An applied numeral X/Y (§6.2 applied_prefix) realises X in the key of Y:
 * Y is realised in the host key, and X is built on Y's root in the major
 * scale if Y is major or augmented, or the harmonic minor if Y is minor. A
 * diminished target is refused. Applied numerals nest: V/V/V is V of V/V.
 *
 * @param numeral Roman numeral (e.g., "I", "bVII", "V7", "ii65", "N", "V7/V")
 * @param key_root Key root pitch class
 * @param scale_intervals Scale intervals for chord construction
 * @param octave Base octave for voicing
 * @return ChordVoicing or error
 * @post Success contains every generated quality member and realizes the
 *       requested inversion; an out-of-range member rejects the whole voicing.
 */
[[nodiscard]] Result<ChordVoicing>
generate_chord_from_numeral(std::string_view numeral,
                            PitchClass key_root,
                            std::span<const Interval> scale_intervals,
                            int octave = 4);

/**
 * @brief Generate chord from root and quality
 *
 * @param root Root pitch class
 * @param quality Chord quality string
 * @param octave Base octave
 * @return ChordVoicing or error
 * @post Success contains every interval of the registered quality; an
 *       out-of-range member rejects the whole voicing.
 */
[[nodiscard]] Result<ChordVoicing>
generate_chord(PitchClass root, std::string_view quality, int octave = 4);

/**
 * @brief Get chord intervals for a quality
 *
 * @param quality Chord quality (major, minor, dim, aug, 7, maj7, etc.)
 * @return Intervals from root or UnknownChordQuality error
 */
[[nodiscard]] Result<std::vector<Interval>> chord_quality_intervals(std::string_view quality);

/**
 * @brief Recognize chord root and quality from a pitch class set (§16.1.5)
 *
 * Key-independent chord identification. For each candidate root,
 * computes intervals above root and matches against the chord
 * quality registry.
 *
 * @param pcs Pitch class set
 * @return (root, quality_name) or ChordNotRecognised error
 */
[[nodiscard]] Result<std::pair<PitchClass, std::string>> recognize_chord(const PitchClassSet& pcs);

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
[[nodiscard]] Result<std::string> chord_to_numeral(PitchClass chord_root,
                                                   std::string_view quality,
                                                   PitchClass key_root,
                                                   std::span<const Interval> scale_intervals,
                                                   bool is_minor = false);

} // namespace sunny::core
