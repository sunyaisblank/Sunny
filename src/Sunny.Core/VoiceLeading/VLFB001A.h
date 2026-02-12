/**
 * @file VLFB001A.h
 * @brief Figured Bass Realisation
 *
 * Component: VLFB001A
 * Domain: VL (VoiceLeading) | Category: FB (Figured Bass)
 *
 * Formal Spec §7.7: Given a bass note and figured bass symbols,
 * determine interval requirements above the bass.
 *
 * Figured bass symbol table:
 *   (none)/5/3  → 3rd, 5th          (root position)
 *   6/6/3       → 3rd, 6th          (first inversion)
 *   6/4         → 4th, 6th          (second inversion)
 *   7           → 3rd, 5th, 7th     (root position 7th)
 *   6/5         → 3rd, 5th, 6th     (first inversion 7th)
 *   4/3         → 3rd, 4th, 6th     (second inversion 7th)
 *   4/2 / 2     → 2nd, 4th, 6th     (third inversion 7th)
 *
 * Invariants:
 * - Realised voicing always includes the bass note
 * - Intervals are diatonic above bass within specified key
 * - Output is sorted ascending by pitch
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPC001A.h"
#include "../Scale/SCDF001A.h"

#include <string_view>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Figured Bass Types
// =============================================================================

/**
 * @brief Accidental modifier for a figured bass figure
 */
enum class FigureAccidental : std::uint8_t {
    Natural,  ///< No modification (diatonic)
    Sharp,    ///< Raise by semitone
    Flat,     ///< Lower by semitone
};

/**
 * @brief A single figure (number + optional accidental)
 */
struct Figure {
    int interval;                ///< Generic interval above bass (2, 3, 4, 5, 6, 7)
    FigureAccidental accidental; ///< Modification
};

/**
 * @brief Complete figured bass symbol for one bass note
 */
struct FiguredBassSymbol {
    std::vector<Figure> figures;
};

/**
 * @brief Result of figured bass realisation
 */
struct FiguredBassRealisation {
    MidiNote bass;                    ///< Bass note (given)
    std::vector<MidiNote> upper;      ///< Upper voices (ascending)
    std::vector<MidiNote> all_notes;  ///< All notes including bass (ascending)
};

// =============================================================================
// Figured Bass Parsing
// =============================================================================

/**
 * @brief Parse a figured bass string into a symbol
 *
 * Accepted formats: "", "5/3", "6", "6/3", "6/4", "7", "6/5",
 * "4/3", "4/2", "2". Accidentals: "#6", "b3", etc.
 *
 * @param text Figured bass string
 * @return Parsed symbol or error
 */
[[nodiscard]] Result<FiguredBassSymbol> parse_figured_bass(std::string_view text);

/**
 * @brief Get default intervals for a standard figured bass shorthand
 *
 * Shorthand mappings (§7.7.1):
 * - ""/"5/3"/"53"  → {3, 5}
 * - "6"/"6/3"/"63" → {3, 6}
 * - "6/4"/"64"     → {4, 6}
 * - "7"            → {3, 5, 7}
 * - "6/5"/"65"     → {3, 5, 6}
 * - "4/3"/"43"     → {3, 4, 6}
 * - "4/2"/"42"/"2" → {2, 4, 6}
 *
 * @param shorthand Figured bass shorthand string
 * @return Vector of generic intervals above bass
 */
[[nodiscard]] std::vector<int> figured_bass_intervals(std::string_view shorthand);

// =============================================================================
// Realisation
// =============================================================================

/**
 * @brief Realise a figured bass symbol above a given bass note
 *
 * Computes pitch classes for each figure by counting diatonic steps
 * above the bass within the given key, then places them in the
 * specified octave range.
 *
 * @param bass_note MIDI note for the bass
 * @param symbol Figured bass symbol
 * @param key_root Root pitch class of the key
 * @param key_scale Scale intervals (e.g., major scale)
 * @param upper_octave Octave for upper voices (default: same as bass or +1)
 * @return Realisation or error
 */
[[nodiscard]] Result<FiguredBassRealisation> realise_figured_bass(
    MidiNote bass_note,
    const FiguredBassSymbol& symbol,
    PitchClass key_root,
    std::span<const Interval> key_scale,
    int upper_octave = -1);

// =============================================================================
// Sequence Realisation (voice-led)
// =============================================================================

/**
 * @brief A bass note paired with its figured bass symbol
 */
struct FiguredBassEvent {
    MidiNote bass_note;
    FiguredBassSymbol symbol;
};

/**
 * @brief Result of figured bass sequence realisation
 */
struct FiguredBassSequenceResult {
    std::vector<FiguredBassRealisation> realisations;
};

/**
 * @brief Realise a sequence of figured bass events with voice-leading
 *
 * Each event is realised to produce the correct intervals above the bass.
 * Successive upper voices are connected via optimal voice leading (VLNT001A)
 * to minimise total voice motion across the progression.
 *
 * @param events Sequence of bass notes with figured bass symbols
 * @param key_root Root pitch class of the key
 * @param key_scale Scale intervals (e.g., SCALE_MAJOR)
 * @return Sequence of realisations or error
 */
[[nodiscard]] Result<FiguredBassSequenceResult> realise_figured_bass_sequence(
    std::span<const FiguredBassEvent> events,
    PitchClass key_root,
    std::span<const Interval> key_scale
);

}  // namespace Sunny::Core
