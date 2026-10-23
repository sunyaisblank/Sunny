/**
 * @file figured_bass.hpp
 * @brief Figured Bass Realisation
 *
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

#include <string_view>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/pitch/spelled_pitch.hpp>
#include <sunny/core/scale/definitions.hpp>
#include <sunny/core/types/music_types.hpp>
#include <vector>

namespace sunny::core {

// =============================================================================
// Figured Bass Types
// =============================================================================

/**
 * @brief Accidental modifier for a figured bass figure
 */
enum class FigureAccidental : std::uint8_t {
    Natural, ///< No modification (diatonic)
    Sharp,   ///< Raise by semitone
    Flat,    ///< Lower by semitone
};

/**
 * @brief A single figure (number + optional accidental)
 */
struct Figure {
    int interval;                ///< Generic interval above bass [1,9]
    FigureAccidental accidental; ///< Modification
};

inline constexpr std::size_t MAX_FIGURED_BASS_VOICES = 12;

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
    MidiNote bass;                   ///< Bass note (given)
    std::vector<MidiNote> upper;     ///< Upper voices (ascending)
    std::vector<MidiNote> all_notes; ///< All notes including bass (ascending)
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
 * Counts generic intervals in a seven-note key. The MIDI-only overload
 * requires a bass in the key scale; chromatic bass spelling is ambiguous
 * and returns InvalidSpelledPitch. Use the spelled overload for chromatic
 * basses or enharmonic distinctions. Empty/invalid figures, more than 12
 * upper voices, or malformed scales return an error.
 * Every upper voice is strictly above the bass and satisfies the indicated
 * compound interval's minimum register. Output remains MIDI bounded.
 *
 * @param bass_note MIDI note for the bass
 * @param symbol Figured bass symbol
 * @param key_root Root pitch class of the key
 * @param key_scale Scale intervals (e.g., major scale)
 * @param upper_octave Preferred MIDI octave [0,9]; -1 chooses bass octave+1,
 * clamped to [0,9]. Notes may shift upward to satisfy their interval.
 * @return Realisation or error
 */
[[nodiscard]] Result<FiguredBassRealisation>
realise_figured_bass(MidiNote bass_note,
                     const FiguredBassSymbol& symbol,
                     PitchClass key_root,
                     std::span<const Interval> key_scale,
                     int upper_octave = -1);

/** Count letter intervals above the spelled bass, using key-relative tones.
 * The tonic's octave is ignored; its letter and accidental define the key.
 * Figure Sharp/Flat raise/lower the key-relative target by one semitone;
 * Figure Natural retains its historical meaning of no alteration.
 */
[[nodiscard]] Result<FiguredBassRealisation>
realise_figured_bass(SpelledPitch bass_note,
                     const FiguredBassSymbol& symbol,
                     SpelledPitch key_root,
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

struct SpelledFiguredBassEvent {
    SpelledPitch bass_note;
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
 * Equal-cardinality successive upper voices minimise summed absolute
 * semitone motion between sorted voices, subject to the target figures,
 * their compound-interval register, MIDI bounds, and all upper voices
 * remaining above the current bass. Other contrapuntal constraints are
 * not applied. A cardinality change uses direct realisation.
 *
 * @param events Sequence of bass notes with figured bass symbols
 * @param key_root Root pitch class of the key
 * @param key_scale Scale intervals (e.g., SCALE_MAJOR)
 * @return Sequence of realisations or error
 */
[[nodiscard]] Result<FiguredBassSequenceResult>
realise_figured_bass_sequence(std::span<const FiguredBassEvent> events,
                              PitchClass key_root,
                              std::span<const Interval> key_scale);

[[nodiscard]] Result<FiguredBassSequenceResult>
realise_figured_bass_sequence(std::span<const SpelledFiguredBassEvent> events,
                              SpelledPitch key_root,
                              std::span<const Interval> key_scale);

} // namespace sunny::core
