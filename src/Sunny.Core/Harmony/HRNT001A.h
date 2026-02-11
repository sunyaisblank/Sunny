/**
 * @file HRNT001A.h
 * @brief Non-tertian chord construction
 *
 * Component: HRNT001A
 * Domain: HR (Harmony) | Category: NT (Non-Tertian)
 *
 * Formal Spec §5.4: Quartal, quintal, secundal, and polychord construction.
 *
 * Invariants:
 * - chord_by_stacking(r, interval, n) produces exactly n notes
 * - Quartal stacking interval is 5 semitones (perfect fourth)
 * - Quintal stacking interval is 7 semitones (perfect fifth)
 * - Quintal voicings are pitch-class inversions of quartal voicings
 * - tone_cluster(r, w) produces w+1 pitch classes: {r, r+1, ..., r+w} mod 12
 * - polychord(A, B) pitch class set == union of A and B pitch class sets
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "../Pitch/PTPC001A.h"
#include "../Pitch/PTMN001A.h"

namespace Sunny::Core {

/**
 * @brief Build a chord by stacking a single interval
 *
 * Formal Spec §5.4.1–5.4.2: Generates notes by repeatedly adding
 * a fixed interval from the root. Covers quartal (5), quintal (7),
 * and any other uniform stacking.
 *
 * @param root Root pitch class
 * @param stack_interval Interval in semitones to stack
 * @param count Number of notes (>= 2)
 * @param octave Base octave for voicing
 * @return ChordVoicing or error
 */
[[nodiscard]] Result<ChordVoicing> chord_by_stacking(
    PitchClass root,
    Interval stack_interval,
    int count,
    int octave = 4
);

/**
 * @brief Build a quartal chord (stacked perfect fourths)
 *
 * Formal Spec §5.4.1:
 *   Quartal triad (count=3): {0, 5, 10}
 *   Quartal tetrachord (count=4): {0, 5, 10, 15}
 *
 * @param root Root pitch class
 * @param count Number of notes (default 3)
 * @param octave Base octave
 * @return ChordVoicing with quality "quartal"
 */
[[nodiscard]] Result<ChordVoicing> quartal_chord(
    PitchClass root, int count = 3, int octave = 4);

/**
 * @brief Build a quintal chord (stacked perfect fifths)
 *
 * Formal Spec §5.4.2: Quintal voicings are inversions of quartal
 * voicings (the fifth is the inversion of the fourth).
 *
 * @param root Root pitch class
 * @param count Number of notes (default 3)
 * @param octave Base octave
 * @return ChordVoicing with quality "quintal"
 */
[[nodiscard]] Result<ChordVoicing> quintal_chord(
    PitchClass root, int count = 3, int octave = 4);

/**
 * @brief Build a chromatic tone cluster
 *
 * Formal Spec §5.4.3: A tone cluster of width w at root r is
 * {r, r+1, r+2, ..., r+w} — consecutive chromatic pitches.
 * The cluster contains w+1 notes.
 *
 * @param root Root pitch class
 * @param width Cluster width in semitones (>= 1)
 * @param octave Base octave
 * @return ChordVoicing with quality "cluster"
 */
[[nodiscard]] Result<ChordVoicing> tone_cluster(
    PitchClass root, int width, int octave = 4);

/**
 * @brief Combine two chords into a polychord
 *
 * Formal Spec §5.4.4: A polychord is the superposition of two
 * (or more) triads. The pitch class set is the union of the
 * component sets. The root and quality are taken from the lower chord.
 *
 * Notes are merged and sorted in ascending order; duplicates removed.
 *
 * @param lower Lower chord (determines root)
 * @param upper Upper chord
 * @return Combined ChordVoicing
 */
[[nodiscard]] ChordVoicing polychord(
    const ChordVoicing& lower,
    const ChordVoicing& upper
);

}  // namespace Sunny::Core
