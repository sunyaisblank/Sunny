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
 * @brief Collapse parts by instrument family into two-staff condensed score
 *
 * Groups parts by InstrumentFamily (Strings, Woodwinds, Brass, etc.)
 * and produces one two-voice Part per family. Notes at or above MIDI 60
 * go to voice 0 (treble); notes below go to voice 1 (bass). Families
 * containing only rests are omitted.
 *
 * @param score Source score
 * @return New score with one part per active instrument family
 */
[[nodiscard]] Score short_score(const Score& score);

/**
 * @brief Extract a single part as a standalone score
 *
 * Copies global maps, metadata, and the specified part. When rest_threshold
 * is non-zero, inserts cue notes from other parts after runs of consecutive
 * all-rest bars meeting or exceeding the threshold.
 *
 * @param score Source score
 * @param part_id Part to extract
 * @param rest_threshold Minimum consecutive rest bars before inserting a cue note (0 disables)
 * @return New score with one part, or empty score if part_id not found
 */
[[nodiscard]] Score part_extract(const Score& score, PartId part_id,
                                 std::uint32_t rest_threshold = 4);

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
