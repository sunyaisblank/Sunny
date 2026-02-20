/**
 * @file SIVW001A.h
 * @brief Score IR view system — derived read-only projections
 *
 * Component: SIVW001A
 * Domain: SI (Score IR) | Category: VW (View)
 *
 * Provides functions that produce derived Score objects from an existing
 * Score. Views are non-destructive: the source Score is const and the
 * result is a new, independent Score.
 *
 * Functions:
 * - piano_reduction: collapse all parts to 2-staff piano
 * - short_score: reduce to 4 SATB voices
 * - part_extract: single-part score
 * - harmonic_skeleton: strip ornamentation, keep chord tones
 * - region_view: extract a region as standalone score
 *
 * Invariants:
 * - Source Score is never modified
 * - Output Score passes structural validation
 */

#pragma once

#include "SIDC001A.h"

namespace Sunny::Core {

/**
 * @brief Collapse all parts into a two-staff piano reduction
 *
 * Treble staff receives notes above middle C (MIDI 60);
 * bass staff receives notes at or below.
 *
 * @param score Source score
 * @return New score with one Piano part and two voices per measure
 */
[[nodiscard]] Score piano_reduction(const Score& score);

/**
 * @brief Reduce to four SATB voices
 *
 * Assigns notes to Soprano, Alto, Tenor, Bass voices by register.
 * Soprano/Alto share treble staff; Tenor/Bass share bass staff.
 *
 * @param score Source score
 * @return New score with one part and up to four voices per measure
 */
[[nodiscard]] Score short_score(const Score& score);

/**
 * @brief Extract a single part as a standalone score
 *
 * Copies global maps, metadata, and the specified part.
 *
 * @param score Source score
 * @param part_id Part to extract
 * @return New score with one part, or empty score if part_id not found
 */
[[nodiscard]] Score part_extract(const Score& score, PartId part_id);

/**
 * @brief Strip ornamentation, keep chord tones
 *
 * Removes grace notes, ornaments, and notes marked as non-chord tones
 * in the harmonic annotation layer. Preserves rhythmic structure by
 * converting removed notes to rests where needed.
 *
 * @param score Source score
 * @return New score with simplified melodic content
 */
[[nodiscard]] Score harmonic_skeleton(const Score& score);

/**
 * @brief Extract a rectangular region as a standalone score
 *
 * Creates a new score containing only the events within the given
 * region. Bar numbers are renumbered starting from 1.
 *
 * @param score Source score
 * @param region Region to extract
 * @return New score spanning the region
 */
[[nodiscard]] Score region_view(const Score& score, const ScoreRegion& region);

}  // namespace Sunny::Core
