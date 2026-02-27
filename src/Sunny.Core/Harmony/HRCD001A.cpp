/**
 * @file HRCD001A.cpp
 * @brief Cadence detection implementation
 *
 * Component: HRCD001A
 */

#include "HRCD001A.h"
#include "HRFN001A.h"
#include "../Pitch/PTPS001A.h"

#include <algorithm>
#include <array>

namespace Sunny::Core {

namespace {

// Major scale degrees (semitones from root)
constexpr std::array<int, 7> MAJOR_SCALE = {0, 2, 4, 5, 7, 9, 11};
constexpr std::array<int, 7> MINOR_SCALE = {0, 2, 3, 5, 7, 8, 10};

int find_scale_degree(PitchClass root, PitchClass key_root, bool is_minor) {
    const auto& scale = is_minor ? MINOR_SCALE : MAJOR_SCALE;
    int interval = (root - key_root + 12) % 12;
    for (int i = 0; i < 7; ++i) {
        if (scale[i] == interval) return i;
    }
    // Chromatic: find nearest degree and prefer conventional spelling.
    // When equidistant, prefer sharp for degrees 0,1,3,4 and flat for 2,5,6.
    int sharp_deg = -1;
    int flat_deg = -1;
    int best = 0;
    int best_dist = 12;
    for (int i = 0; i < 7; ++i) {
        int d = std::abs(scale[i] - interval);
        d = std::min(d, 12 - d);
        if (d < best_dist) { best_dist = d; best = i; }
        if (scale[i] == interval - 1) sharp_deg = i;
        if (scale[i] == interval + 1) flat_deg = i;
    }
    if (best_dist == 1 && sharp_deg >= 0 && flat_deg >= 0) {
        static constexpr bool PREFER_SHARP[7] = {
            true, true, false, true, true, false, false
        };
        return PREFER_SHARP[sharp_deg] ? sharp_deg : flat_deg;
    }
    return best;
}

}  // namespace

CadenceAnalysis detect_cadence(
    const ChordVoicing& penultimate,
    const ChordVoicing& final_chord,
    PitchClass key_root,
    bool is_minor
) {
    CadenceAnalysis result{};
    result.type = CadenceType::None;

    if (penultimate.empty() || final_chord.empty()) {
        return result;
    }

    // Determine scale degrees
    int pen_deg = find_scale_degree(penultimate.root, key_root, is_minor);
    int fin_deg = find_scale_degree(final_chord.root, key_root, is_minor);

    result.penultimate_degree = pen_deg;
    result.final_degree = fin_deg;

    // Root position check: bass note pitch class == chord root
    result.is_root_position = (final_chord.bass() % 12) == final_chord.root;

    // Soprano on tonic check
    result.soprano_on_tonic = (final_chord.soprano() % 12) == key_root;

    // V→I: Authentic cadence
    if (pen_deg == 4 && fin_deg == 0) {
        if (result.is_root_position && result.soprano_on_tonic) {
            result.type = CadenceType::PAC;
        } else {
            result.type = CadenceType::IAC;
        }
        return result;
    }

    // V→vi: Deceptive cadence
    if (pen_deg == 4 && fin_deg == 5) {
        result.type = CadenceType::Deceptive;
        return result;
    }

    // IV→I: Plagal cadence
    if (pen_deg == 3 && fin_deg == 0) {
        result.type = CadenceType::Plagal;
        return result;
    }

    // iv6→V in minor: Phrygian half cadence
    if (is_minor && pen_deg == 3 && fin_deg == 4) {
        // Check if penultimate is in first inversion
        if (penultimate.inversion == 1 ||
            (penultimate.bass() % 12) != penultimate.root) {
            result.type = CadenceType::PhrygianHalf;
            return result;
        }
    }

    // *→V: Half cadence
    if (fin_deg == 4) {
        result.type = CadenceType::Half;
        return result;
    }

    return result;
}

std::vector<std::pair<std::size_t, CadenceAnalysis>>
detect_cadences_in_progression(
    const std::vector<ChordVoicing>& progression,
    PitchClass key_root,
    bool is_minor
) {
    std::vector<std::pair<std::size_t, CadenceAnalysis>> results;

    if (progression.size() < 2) return results;

    for (std::size_t i = 0; i + 1 < progression.size(); ++i) {
        auto analysis = detect_cadence(progression[i], progression[i + 1],
                                       key_root, is_minor);
        if (analysis.type != CadenceType::None) {
            results.emplace_back(i, analysis);
        }
    }

    return results;
}

}  // namespace Sunny::Core
