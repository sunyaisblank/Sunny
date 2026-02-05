/**
 * @file HRNG001A.cpp
 * @brief Negative harmony implementation
 *
 * Component: HRNG001A
 */

#include "HRNG001A.h"
#include "../Pitch/PTPC001A.h"

namespace Sunny::Core {

PitchClassSet negative_harmony(
    const PitchClassSet& chord_pcs,
    PitchClass key_root
) {
    // The negative harmony axis is between scale degrees b3 and 3
    // (between Eb and E in C major, at pitch class 3.5)
    // Formula: I(x) = (7 + 2*key_root - x) mod 12
    // This correctly maps I <-> i, V <-> iv, IV <-> v

    PitchClassSet result;
    result.reserve(chord_pcs.size());

    int doubled_axis = 7 + 2 * static_cast<int>(key_root);

    for (auto pc : chord_pcs) {
        int transformed = (doubled_axis - static_cast<int>(pc) + 24) % 12;
        result.insert(static_cast<PitchClass>(transformed));
    }

    return result;
}

}  // namespace Sunny::Core
