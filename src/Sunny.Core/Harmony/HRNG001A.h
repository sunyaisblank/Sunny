/**
 * @file HRNG001A.h
 * @brief Negative harmony transformation
 *
 * Component: HRNG001A
 * Domain: HR (Harmony) | Category: NG (Negative)
 *
 * Implements negative harmony via axis inversion.
 * The axis is located between the 3rd and 4th scale degrees.
 *
 * Invariants:
 * - negative_harmony(negative_harmony(pcs, key), key) == pcs
 * - |result| == |input|
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPS001A.h"

namespace Sunny::Core {

/**
 * @brief Transform chord using negative harmony
 *
 * The axis is between scale degrees 3 and 4 (Eb-E in C major).
 * This inverts I <-> i, IV <-> v, V <-> iv, etc.
 *
 * @param chord_pcs Original chord pitch classes
 * @param key_root Key root for axis calculation
 * @return Transformed pitch class set
 */
[[nodiscard]] PitchClassSet negative_harmony(
    const PitchClassSet& chord_pcs,
    PitchClass key_root
);

/**
 * @brief Get the doubled negative harmony axis for a key
 *
 * The axis is at 3.5 semitones above the root (between Eb and E in C).
 * Since we can't use fractional pitch classes, this returns 2*axis = 7 + 2*key_root.
 * Use with formula: I(x) = (doubled_axis - x) mod 12
 *
 * @param key_root Key root pitch class
 * @return Doubled axis value (7 for C major, meaning axis at 3.5)
 */
[[nodiscard]] constexpr int negative_harmony_axis(PitchClass key_root) noexcept {
    // Axis is between scale degrees b3 and 3 (3.5 semitones from root)
    // Return doubled value for integer arithmetic: 2 * (key_root + 3.5) = 2*key_root + 7
    return 7 + 2 * key_root;
}

}  // namespace Sunny::Core
