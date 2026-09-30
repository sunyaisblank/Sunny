/**
 * @file cadence.hpp
 * @brief Cadence detection and analysis
 *
 *
 * Detects cadence patterns in chord progressions by analysing
 * harmonic function of consecutive chord pairs.
 *
 * Invariants:
 * - PAC: V→I, root position, soprano on tonic
 * - IAC: V→I, but not satisfying PAC conditions
 * - Half: *→V
 * - Plagal: IV→I
 * - Deceptive: V→vi
 * - Phrygian half: iv6→V in minor
 */

#pragma once

#include <string>
#include <sunny/core/pitch/pitch_class.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <utility>
#include <vector>

namespace sunny::core {

/**
 * @brief Cadence type classification
 */
enum class CadenceType {
    PAC,          ///< Perfect Authentic Cadence: V→I, root pos, soprano on tonic
    IAC,          ///< Imperfect Authentic Cadence: V→I, not PAC conditions
    Half,         ///< Half Cadence: *→V
    Plagal,       ///< Plagal Cadence: IV→I
    Deceptive,    ///< Deceptive Cadence: V→vi
    PhrygianHalf, ///< Phrygian Half Cadence: iv6→V (minor)
    None          ///< No cadence detected
};

/**
 * @brief Analysis result for a cadence
 */
struct CadenceAnalysis {
    CadenceType type;
    int penultimate_degree; ///< Scale degree of penultimate chord (0-6), altered or not
    int final_degree;       ///< Scale degree of final chord (0-6), altered or not
    bool is_root_position;  ///< Whether final chord is in root position
    bool soprano_on_tonic;  ///< Whether soprano of final chord is the tonic
};

/**
 * @brief Detect cadence type from a pair of consecutive chords
 *
 * @param penultimate The penultimate chord voicing
 * @param final_chord The final chord voicing
 * @param key_root Key root pitch class
 * @param is_minor Whether the key is minor
 * @return CadenceAnalysis with detected type
 */
[[nodiscard]] CadenceAnalysis detect_cadence(const ChordVoicing& penultimate,
                                             const ChordVoicing& final_chord,
                                             PitchClass key_root,
                                             bool is_minor = false);

/**
 * @brief Scan a progression for all cadences
 *
 * @param progression Vector of chord voicings
 * @param key_root Key root pitch class
 * @param is_minor Whether the key is minor
 * @return Vector of (index, analysis) pairs; index is the position of the penultimate chord
 */
[[nodiscard]] std::vector<std::pair<std::size_t, CadenceAnalysis>> detect_cadences_in_progression(
    const std::vector<ChordVoicing>& progression, PitchClass key_root, bool is_minor = false);

} // namespace sunny::core
