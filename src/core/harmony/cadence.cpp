/**
 * @file cadence.cpp
 * @brief Cadence detection implementation
 *
 */

#include <algorithm>
#include <array>
#include <sunny/core/harmony/cadence.hpp>
#include <sunny/core/harmony/harmonic_function.hpp>
#include <sunny/core/harmony/roman_numeral.hpp>
#include <sunny/core/pitch/pitch_class_set.hpp>
#include <sunny/core/scale/definitions.hpp>

namespace sunny::core {

namespace {

// Scale degree of a chord root with its alteration. Every interval has a
// spelling against the major or natural minor scale.
SpelledDegree find_scale_degree(PitchClass root, PitchClass key_root, bool is_minor) {
    const std::span<const Interval> scale =
        is_minor ? std::span<const Interval>(SCALE_MINOR) : std::span<const Interval>(SCALE_MAJOR);
    return spell_scale_degree(root - key_root, scale, is_minor).value_or(SpelledDegree{0, 0});
}

} // namespace

CadenceAnalysis detect_cadence(const ChordVoicing& penultimate,
                               const ChordVoicing& final_chord,
                               PitchClass key_root,
                               bool is_minor) {
    CadenceAnalysis result{};
    result.type = CadenceType::None;

    if (penultimate.empty() || final_chord.empty()) {
        return result;
    }

    // Determine scale degrees. Cadence patterns are defined on unaltered
    // degrees, so a chromatic root (bVI, bII) matches none of them.
    const auto pen = find_scale_degree(penultimate.root, key_root, is_minor);
    const auto fin = find_scale_degree(final_chord.root, key_root, is_minor);
    result.penultimate_degree = pen.degree;
    result.final_degree = fin.degree;
    const int pen_deg = pen.accidental == 0 ? pen.degree : -1;
    const int fin_deg = fin.accidental == 0 ? fin.degree : -1;

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
        if (penultimate.inversion == 1 || (penultimate.bass() % 12) != penultimate.root) {
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

std::vector<std::pair<std::size_t, CadenceAnalysis>> detect_cadences_in_progression(
    const std::vector<ChordVoicing>& progression, PitchClass key_root, bool is_minor) {
    std::vector<std::pair<std::size_t, CadenceAnalysis>> results;

    if (progression.size() < 2) return results;

    for (std::size_t i = 0; i + 1 < progression.size(); ++i) {
        auto analysis = detect_cadence(progression[i], progression[i + 1], key_root, is_minor);
        if (analysis.type != CadenceType::None) {
            results.emplace_back(i, analysis);
        }
    }

    return results;
}

} // namespace sunny::core
