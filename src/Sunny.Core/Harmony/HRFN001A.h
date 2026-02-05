/**
 * @file HRFN001A.h
 * @brief Functional harmony analysis
 *
 * Component: HRFN001A
 * Domain: HR (Harmony) | Category: FN (Function)
 *
 * Analyze chords for their harmonic function (Tonic, Subdominant, Dominant).
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Pitch/PTPS001A.h"

#include <string>

namespace Sunny::Core {

/**
 * @brief Harmonic function categories
 */
enum class HarmonicFunction {
    Tonic,       ///< T: I, vi, iii
    Subdominant, ///< S: IV, ii
    Dominant     ///< D: V, vii°
};

/**
 * @brief Result of chord analysis
 */
struct ChordAnalysis {
    PitchClass root;              ///< Chord root
    std::string quality;          ///< major, minor, diminished, etc.
    HarmonicFunction function;    ///< T, S, or D
    std::string numeral;          ///< Roman numeral
    int degree;                   ///< Scale degree (1-7)
};

/**
 * @brief Analyze chord function in a key
 *
 * @param chord_pcs Pitch class set of the chord
 * @param key_root Key root pitch class
 * @param is_minor Whether the key is minor
 * @return Analysis result
 */
[[nodiscard]] ChordAnalysis analyze_chord_function(
    const PitchClassSet& chord_pcs,
    PitchClass key_root,
    bool is_minor = false
);

/**
 * @brief Get Roman numeral for scale degree
 *
 * @param degree Scale degree (0-6)
 * @param is_major Whether the chord is major
 * @return Roman numeral string
 */
[[nodiscard]] std::string degree_to_numeral(int degree, bool is_major);

/**
 * @brief Convert harmonic function to string
 */
[[nodiscard]] constexpr std::string_view function_to_string(
    HarmonicFunction func
) noexcept {
    switch (func) {
        case HarmonicFunction::Tonic: return "T";
        case HarmonicFunction::Subdominant: return "S";
        case HarmonicFunction::Dominant: return "D";
    }
    return "T";
}

}  // namespace Sunny::Core
