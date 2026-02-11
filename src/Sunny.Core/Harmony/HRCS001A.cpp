/**
 * @file HRCS001A.cpp
 * @brief Chord-scale theory
 *
 * Component: HRCS001A
 */

#include "HRCS001A.h"

#include <algorithm>

namespace Sunny::Core {

bool is_avoid_note(
    PitchClass tension,
    std::span<const PitchClass> chord_tones
) {
    for (PitchClass c : chord_tones) {
        // Minor 9th: (t - c) mod 12 == 1
        int diff = (static_cast<int>(tension) - static_cast<int>(c) + 12) % 12;
        if (diff == 1) {
            return true;
        }
    }
    return false;
}

ChordScaleAnalysis analyze_chord_scale(
    std::span<const PitchClass> chord_pcs,
    PitchClass scale_root,
    std::span<const Interval> scale_intervals
) {
    ChordScaleAnalysis result;

    // Build set of chord pitch classes for fast lookup
    bool is_chord[12] = {};
    for (PitchClass pc : chord_pcs) {
        is_chord[pc % 12] = true;
    }

    // Iterate over scale tones
    for (Interval iv : scale_intervals) {
        PitchClass pc = (static_cast<int>(scale_root) + iv) % 12;

        if (is_chord[pc]) {
            result.chord_tones.push_back(pc);
        } else if (is_avoid_note(pc, chord_pcs)) {
            result.avoid.push_back(pc);
        } else {
            result.available.push_back(pc);
        }
    }

    return result;
}

std::optional<std::string_view> chord_scale_for_function(
    std::string_view chord_function
) {
    // Formal Spec §5.6 heuristic table
    if (chord_function == "Imaj7")    return "major";        // Ionian
    if (chord_function == "ii7")      return "dorian";
    if (chord_function == "iii7")     return "phrygian";
    if (chord_function == "IVmaj7")   return "lydian";
    if (chord_function == "V7")       return "mixolydian";
    if (chord_function == "vi7")      return "aeolian";
    // Natural minor alias
    if (chord_function == "vi7")      return "minor";
    if (chord_function == "viiø7")    return "locrian";

    // Extended / altered dominants
    if (chord_function == "V7alt")    return "super_locrian"; // Altered scale
    if (chord_function == "V7b9b13")  return "phrygian_dominant";

    // Minor ii-V
    if (chord_function == "iiø7")     return "locrian_natural2";

    return std::nullopt;
}

std::optional<std::string_view> default_chord_scale(
    int degree, bool is_major_key
) {
    if (degree < 0 || degree > 6) return std::nullopt;

    // Major key: modes of the major scale in order
    constexpr std::string_view major_scales[] = {
        "major",        // I   → Ionian
        "dorian",       // ii  → Dorian
        "phrygian",     // iii → Phrygian
        "lydian",       // IV  → Lydian
        "mixolydian",   // V   → Mixolydian
        "aeolian",      // vi  → Aeolian
        "locrian",      // vii → Locrian
    };

    // Natural minor key: modes of the relative major
    // i=aeolian, ii°=locrian, III=major, iv=dorian,
    // v=phrygian, VI=lydian, VII=mixolydian
    constexpr std::string_view minor_scales[] = {
        "aeolian",      // i   → Aeolian
        "locrian",      // ii° → Locrian
        "major",        // III → Ionian
        "dorian",       // iv  → Dorian
        "phrygian",     // v   → Phrygian
        "lydian",       // VI  → Lydian (on relative major IV)
        "mixolydian",   // VII → Mixolydian
    };

    return is_major_key ? major_scales[degree] : minor_scales[degree];
}

}  // namespace Sunny::Core
