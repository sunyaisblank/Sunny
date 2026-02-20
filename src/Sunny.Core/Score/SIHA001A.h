/**
 * @file SIHA001A.h
 * @brief Score IR harmonic analysis — derivation from note content
 *
 * Component: SIHA001A
 * Domain: SI (Score IR) | Category: HA (Harmonic Analysis)
 *
 * Provides automatic harmonic annotation derivation from a Score's note
 * content. Consumes recognize_chord, chord_to_numeral, analyze_chord_function,
 * and detect_cadence to produce the HarmonicAnnotationLayer.
 *
 * The stale-region refresh function updates only invalidated regions,
 * preserving annotations outside the stale area.
 *
 * Precondition: score.time_map and score.key_map have entries at bar 1.
 */

#pragma once

#include "SIDC001A.h"

namespace Sunny::Core {

/**
 * @brief Derive harmonic annotations for the entire score
 *
 * Segments harmonic rhythm at the beat level, recognises chords,
 * generates Roman numerals relative to the active key, and detects
 * cadences at section boundaries and the score end.
 *
 * @param score Source score
 * @return Sorted HarmonicAnnotationLayer, or error if time/key maps are missing
 */
[[nodiscard]] Result<HarmonicAnnotationLayer> derive_harmonic_layer(
    const Score& score
);

/**
 * @brief Re-analyse only stale regions, updating annotations in place
 *
 * Removes annotations within each stale region, re-derives them,
 * and clears the stale region markers.
 *
 * @param score Score to update (modified in place)
 * @return Success or error
 */
[[nodiscard]] VoidResult refresh_stale_regions(Score& score);

/**
 * @brief Remove all stale harmonic region markers
 */
void clear_stale_harmonic_regions(Score& score);

}  // namespace Sunny::Core
