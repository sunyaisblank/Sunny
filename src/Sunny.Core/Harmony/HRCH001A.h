/**
 * @file HRCH001A.h
 * @brief Chromatic harmony (Neapolitan, augmented sixth, tritone substitution)
 *
 * Component: HRCH001A
 * Domain: HR (Harmony) | Category: CH (Chromatic Harmony)
 *
 * Generates chromatic harmony chord voicings: Neapolitan sixth,
 * Italian/French/German augmented sixth, tritone substitutions,
 * and common-tone diminished seventh chords.
 *
 * Invariants:
 * - Neapolitan: bII6 (first inversion of bII major)
 * - Aug6 in C minor: Italian = {Ab, C, F#}
 * - Tritone sub: dom7 chord a tritone from the original root
 * - CT dim: dim7 built on the semitone above the target
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Pitch/PTPC001A.h"

namespace Sunny::Core {

/**
 * @brief Types of augmented sixth chord
 */
enum class AugSixthType {
    Italian,  ///< 3 notes: b6, 1, #4
    French,   ///< 4 notes: b6, 1, 2, #4
    German    ///< 4 notes: b6, 1, b3, #4
};

/**
 * @brief Generate Neapolitan sixth chord (bII6)
 *
 * First inversion of the bII major triad.
 *
 * @param key_root Key root pitch class
 * @param is_minor Whether the key is minor
 * @param octave Base octave
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> neapolitan_sixth(
    PitchClass key_root,
    bool is_minor,
    int octave = 4
);

/**
 * @brief Generate augmented sixth chord
 *
 * @param type Italian, French, or German
 * @param key_root Key root pitch class
 * @param is_minor Whether the key is minor
 * @param octave Base octave
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> augmented_sixth(
    AugSixthType type,
    PitchClass key_root,
    bool is_minor,
    int octave = 4
);

/**
 * @brief Generate tritone substitution of a dominant seventh chord
 *
 * Replaces a dom7 chord with the dom7 a tritone away.
 * The two chords share 3rd and 7th (enharmonically swapped).
 *
 * @param chord_root Root of the original dominant chord
 * @param octave Base octave
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> tritone_substitution(
    PitchClass chord_root,
    int octave = 4
);

/**
 * @brief Generate common-tone diminished seventh chord
 *
 * Built on the semitone above the target chord root.
 * Resolves to the target with the common tone (root) sustained.
 *
 * @param target_root Root of the target chord
 * @param octave Base octave
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> common_tone_diminished(
    PitchClass target_root,
    int octave = 4
);

}  // namespace Sunny::Core
