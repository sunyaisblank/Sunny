/**
 * @file HRST001A.cpp
 * @brief Harmonic progression as state transformation
 *
 * Component: HRST001A
 */

#include "HRST001A.h"
#include "HRRN001A.h"
#include "../Scale/SCDF001A.h"
#include "../Scale/SCRN001A.h"
#include "../Pitch/PTPC001A.h"

namespace Sunny::Core {

namespace {

// Get the scale intervals for a key
std::span<const Interval> key_scale(const HarmonicKey& key) {
    return key.is_minor
        ? std::span<const Interval>(SCALE_MINOR)
        : std::span<const Interval>(SCALE_MAJOR);
}

// Check if all pitch classes of a chord are diatonic to a key
bool is_chord_diatonic(
    PitchClass chord_root,
    std::string_view quality,
    const HarmonicKey& key
) {
    auto intervals_opt = chord_quality_intervals(quality);
    if (!intervals_opt) return false;

    auto scale = key_scale(key);
    bool in_scale[12] = {};
    for (Interval iv : scale) {
        in_scale[(key.root + iv) % 12] = true;
    }

    for (Interval iv : *intervals_opt) {
        PitchClass pc = (chord_root + (iv % 12)) % 12;
        if (!in_scale[pc]) return false;
    }
    return true;
}

// Check if a chord is a secondary dominant of some diatonic degree.
// Returns true if chord_root is a P5 above any non-tonic diatonic degree
// and the chord quality is dominant-type.
bool is_applied_chord(
    PitchClass chord_root,
    std::string_view quality,
    const HarmonicKey& key
) {
    // Applied chords have dominant quality
    bool is_dominant = (quality == "major" || quality == "7" ||
                        quality == "dom7" || quality == "dominant7");
    bool is_leading_tone = (quality == "diminished" || quality == "dim" ||
                            quality == "dim7" || quality == "diminished7" ||
                            quality == "m7b5" || quality == "half-diminished");
    if (!is_dominant && !is_leading_tone) return false;

    auto scale = key_scale(key);

    // Check each diatonic degree as potential target
    for (std::size_t deg = 0; deg < scale.size(); ++deg) {
        PitchClass target_root = (key.root + scale[deg]) % 12;

        if (is_dominant) {
            // V/X: chord root is P5 above target root
            PitchClass expected = (target_root + 7) % 12;
            if (chord_root == expected && target_root != key.root) {
                return true;
            }
        }
        if (is_leading_tone) {
            // vii°/X: chord root is a semitone below target root
            PitchClass expected = (target_root + 11) % 12;
            if (chord_root == expected && target_root != key.root) {
                return true;
            }
        }
    }
    return false;
}

// Check if a chord is borrowed from the parallel mode
bool is_modal_interchange(
    PitchClass chord_root,
    std::string_view quality,
    const HarmonicKey& key
) {
    // Parallel mode: major↔minor
    auto parallel = key.is_minor
        ? std::span<const Interval>(SCALE_MAJOR)
        : std::span<const Interval>(SCALE_MINOR);

    auto parallel_name = key.is_minor ? "major" : "minor";

    auto borrowed = find_borrowed_chords(
        key.root,
        key_scale(key),
        parallel,
        parallel_name
    );

    for (const auto& bc : borrowed) {
        if (bc.root == chord_root && bc.quality == quality) {
            return true;
        }
    }
    return false;
}

// Determine harmonic function of a chord in a key
HarmonicFunction determine_function(
    PitchClass chord_root,
    const HarmonicKey& key
) {
    auto scale = key_scale(key);

    // Find which degree the root falls on
    for (std::size_t deg = 0; deg < scale.size(); ++deg) {
        PitchClass degree_pc = (key.root + scale[deg]) % 12;
        if (degree_pc == chord_root) {
            // T: I(0), iii(2), vi(5)
            // S: ii(1), IV(3)
            // D: V(4), vii(6)
            switch (deg) {
                case 0: case 2: case 5:
                    return HarmonicFunction::Tonic;
                case 1: case 3:
                    return HarmonicFunction::Subdominant;
                case 4: case 6:
                    return HarmonicFunction::Dominant;
                default:
                    return HarmonicFunction::Tonic;
            }
        }
    }
    // Non-diatonic root — default to tonic (chromatic context)
    return HarmonicFunction::Tonic;
}

}  // namespace

TransitionType classify_transition(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality
) {
    // 1. Diatonic
    if (is_chord_diatonic(new_root, new_quality, current.key)) {
        return TransitionType::Diatonic;
    }

    // 2. Applied / secondary dominant
    if (is_applied_chord(new_root, new_quality, current.key)) {
        return TransitionType::Applied;
    }

    // 3. Modal interchange (borrowed chord)
    if (is_modal_interchange(new_root, new_quality, current.key)) {
        return TransitionType::ModalInterchange;
    }

    // 4. Chromatic fallback
    return TransitionType::Chromatic;
}

TransitionType classify_transition_with_key(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality,
    const HarmonicKey& new_key
) {
    if (!(new_key == current.key)) {
        return TransitionType::Modulatory;
    }
    return classify_transition(current, new_root, new_quality);
}

HarmonicState apply_transition(
    const HarmonicState& current,
    PitchClass new_root,
    std::string_view new_quality,
    const HarmonicKey* new_key
) {
    HarmonicState next;
    next.key = new_key ? *new_key : current.key;
    next.chord_root = new_root;
    next.chord_quality = std::string(new_quality);
    next.function = determine_function(new_root, next.key);
    return next;
}

HarmonicState make_harmonic_state(
    const HarmonicKey& key,
    PitchClass chord_root,
    std::string_view chord_quality
) {
    HarmonicState state;
    state.key = key;
    state.chord_root = chord_root;
    state.chord_quality = std::string(chord_quality);
    state.function = determine_function(chord_root, key);
    return state;
}

std::vector<TransitionType> analyze_progression(
    const HarmonicKey& key,
    std::span<const PitchClass> roots,
    std::span<const std::string_view> qualities
) {
    std::vector<TransitionType> transitions;
    if (roots.size() < 2 || roots.size() != qualities.size()) {
        return transitions;
    }

    transitions.reserve(roots.size() - 1);

    auto state = make_harmonic_state(key, roots[0], qualities[0]);
    for (std::size_t i = 1; i < roots.size(); ++i) {
        auto type = classify_transition(state, roots[i], qualities[i]);
        transitions.push_back(type);
        state = apply_transition(state, roots[i], qualities[i]);
    }

    return transitions;
}

}  // namespace Sunny::Core
