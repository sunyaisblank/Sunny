/**
 * @file HRST001A.h
 * @brief Harmonic progression as state transformation
 *
 * Component: HRST001A
 * Domain: HR (Harmony) | Category: ST (State Transformation)
 *
 * Formal Spec §6.5: Models harmonic progressions as transitions
 * between harmonic states, classifying each chord change.
 *
 * Invariants:
 * - classify_transition returns exactly one TransitionType
 * - Diatonic transition implies all chord tones ∈ current key
 * - apply_transition preserves key unless TransitionType is Modulatory
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNEV001A.h"
#include "HRFN001A.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace Sunny::Core {

/**
 * @brief Key representation (root + mode)
 */
struct HarmonicKey {
    PitchClass root;
    bool is_minor;

    [[nodiscard]] constexpr bool operator==(const HarmonicKey&) const noexcept = default;
};

/**
 * @brief Harmonic state tuple (K, C, F) per §6.5.1
 *
 * The history H is maintained externally as a vector of states.
 */
struct HarmonicState {
    HarmonicKey key;              ///< Current key
    PitchClass chord_root;        ///< Current chord root
    std::string chord_quality;    ///< Current chord quality
    HarmonicFunction function;    ///< Harmonic function of chord in key
};

/**
 * @brief Classification of harmonic transitions per §6.5.2
 */
enum class TransitionType {
    Diatonic,         ///< New chord is diatonic to current key
    Applied,          ///< Secondary dominant or leading-tone chord
    ModalInterchange, ///< Borrowed from parallel mode
    Modulatory,       ///< Key change
    Chromatic         ///< Non-diatonic, non-functional
};

/**
 * @brief Convert transition type to string
 */
[[nodiscard]] constexpr std::string_view transition_type_to_string(
    TransitionType type
) noexcept {
    switch (type) {
        case TransitionType::Diatonic:         return "Diatonic";
        case TransitionType::Applied:          return "Applied";
        case TransitionType::ModalInterchange: return "Modal Interchange";
        case TransitionType::Modulatory:       return "Modulatory";
        case TransitionType::Chromatic:        return "Chromatic";
    }
    return "Unknown";
}

/**
 * @brief Classify a harmonic transition
 *
 * Determines the type of transition from the current state to a new
 * chord. Classification order:
 *   1. Diatonic (all chord tones in current key)
 *   2. Applied (secondary dominant of a diatonic degree)
 *   3. Modal interchange (borrowed from parallel mode)
 *   4. Chromatic (fallback)
 *
 * Modulatory transitions are not detected here; use
 * classify_transition_with_key for explicit key changes.
 *
 * @param current Current harmonic state
 * @param new_root Root of the new chord
 * @param new_quality Quality of the new chord
 * @return Transition classification
 */
[[nodiscard]] TransitionType classify_transition(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality
);

/**
 * @brief Classify a transition with an explicit new key
 *
 * If new_key differs from current key, returns Modulatory.
 * Otherwise delegates to classify_transition.
 *
 * @param current Current harmonic state
 * @param new_root Root of the new chord
 * @param new_quality Quality of the new chord
 * @param new_key Explicit key for the new chord
 * @return Transition classification
 */
[[nodiscard]] TransitionType classify_transition_with_key(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality,
    const HarmonicKey& new_key
);

/**
 * @brief Apply a transition and produce a new state
 *
 * Creates a new HarmonicState from the current state and transition.
 * If new_key is provided, the key changes (modulatory). Otherwise
 * the key is preserved.
 *
 * @param current Current state
 * @param new_root Root of the new chord
 * @param new_quality Quality of the new chord
 * @param new_key Optional new key (for modulation)
 * @return New harmonic state
 */
[[nodiscard]] HarmonicState apply_transition(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality,
    const HarmonicKey* new_key = nullptr
);

/**
 * @brief Create an initial harmonic state from a key and chord
 *
 * @param key Tonal key
 * @param chord_root Root of the initial chord
 * @param chord_quality Quality of the initial chord
 * @return Initial harmonic state
 */
[[nodiscard]] HarmonicState make_harmonic_state(
    const HarmonicKey& key,
    PitchClass chord_root,
    std::string_view chord_quality
);

/**
 * @brief Analyse a chord progression and classify each transition
 *
 * @param key Initial key
 * @param roots Sequence of chord roots
 * @param qualities Sequence of chord quality strings
 * @return Sequence of transition types (length = n-1 for n chords)
 */
[[nodiscard]] std::vector<TransitionType> analyze_progression(
    const HarmonicKey& key,
    std::span<const PitchClass> roots,
    std::span<const std::string_view> qualities
);

}  // namespace Sunny::Core
