/**
 * @file negative_harmony.cpp
 * @brief Negative harmony implementation
 *
 */

#include <sunny/core/harmony/negative_harmony.hpp>
#include <sunny/core/pitch/pitch_class.hpp>

namespace sunny::core {

PitchClassSet negative_harmony(const PitchClassSet& chord_pcs, PitchClass key_root) {
    // The negative harmony axis is between scale degrees b3 and 3
    // (between Eb and E in C major, at pitch class 3.5)
    // Formula: I(x) = (7 + 2*key_root - x) mod 12
    // This correctly maps I <-> i, V <-> iv, IV <-> v

    PitchClassSet result;
    result.reserve(chord_pcs.size());

    int doubled_axis = 7 + 2 * static_cast<int>(key_root);

    for (auto pc : chord_pcs) {
        result.insert(PitchClass::wrapped(doubled_axis - static_cast<int>(pc)));
    }

    return result;
}

} // namespace sunny::core
