/**
 * @file VLCN001A.cpp
 * @brief Voice-leading constraint checker implementation
 *
 * Component: VLCN001A
 */

#include "VLCN001A.h"
#include "VLNT001A.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace Sunny::Core {

VLConstraintConfig default_constraint_config() {
    VLConstraintConfig config;

    config.severities = {
        {VLConstraintRule::NoParallelFifths,       ConstraintSeverity::Error},
        {VLConstraintRule::NoParallelOctaves,      ConstraintSeverity::Error},
        {VLConstraintRule::NoDirectFifthsOctaves,  ConstraintSeverity::Warning},
        {VLConstraintRule::LeadingToneResolution,  ConstraintSeverity::Error},
        {VLConstraintRule::SeventhResolvesDown,    ConstraintSeverity::Warning},
        {VLConstraintRule::NoVoiceCrossing,        ConstraintSeverity::Error},
        {VLConstraintRule::NoLargeLeaps,           ConstraintSeverity::Warning},
        {VLConstraintRule::StepwiseAfterLeap,      ConstraintSeverity::Preference},
        {VLConstraintRule::CompleteChords,          ConstraintSeverity::Preference},
        {VLConstraintRule::DoubleTheRoot,           ConstraintSeverity::Preference},
        {VLConstraintRule::NoDoubleLeadingTone,    ConstraintSeverity::Error},
    };

    // SATB ranges
    config.voice_ranges = {
        {40, 60},   // Bass: E2-C4
        {48, 67},   // Tenor: C3-G4
        {55, 77},   // Alto: G3-F5
        {60, 84},   // Soprano: C4-C6
    };

    config.key_root = 0;
    config.is_minor = false;
    config.max_leap = 12;
    config.max_inner_leap = 7;

    return config;
}

namespace {

ConstraintSeverity get_severity(
    const VLConstraintConfig& config,
    VLConstraintRule rule
) {
    auto it = config.severities.find(rule);
    if (it != config.severities.end()) return it->second;
    return ConstraintSeverity::Warning;
}

void add_violation(
    std::vector<ConstraintViolation>& violations,
    VLConstraintRule rule,
    ConstraintSeverity severity,
    int v1, int v2,
    const std::string& desc
) {
    violations.push_back({rule, severity, v1, v2, desc});
}

// Leading tone in the key (degree 7)
PitchClass leading_tone(PitchClass key_root, bool is_minor) {
    // In major: key_root + 11. In minor (harmonic): same.
    return transpose(key_root, 11);
}

// Check if an interval (in semitones) is a perfect fifth (7) or octave/unison (0)
int interval_mod12(MidiNote a, MidiNote b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) % 12;
}

}  // namespace

std::vector<ConstraintViolation> check_voice_leading(
    const ChordVoicing& prev_chord,
    const ChordVoicing& next_chord,
    const VLConstraintConfig& config
) {
    std::vector<ConstraintViolation> violations;

    const auto& prev = prev_chord.notes;
    const auto& next = next_chord.notes;

    if (prev.empty() || next.empty()) return violations;

    std::size_t voices = std::min(prev.size(), next.size());

    // --- NoParallelFifths / NoParallelOctaves ---
    for (std::size_t i = 0; i < voices; ++i) {
        for (std::size_t j = i + 1; j < voices; ++j) {
            // Parallel fifths
            if (has_parallel_motion(prev[i], prev[j], next[i], next[j], 7)) {
                add_violation(violations, VLConstraintRule::NoParallelFifths,
                    get_severity(config, VLConstraintRule::NoParallelFifths),
                    static_cast<int>(i), static_cast<int>(j),
                    "Parallel fifths between voices");
            }
            // Parallel octaves
            if (has_parallel_motion(prev[i], prev[j], next[i], next[j], 0)) {
                add_violation(violations, VLConstraintRule::NoParallelOctaves,
                    get_severity(config, VLConstraintRule::NoParallelOctaves),
                    static_cast<int>(i), static_cast<int>(j),
                    "Parallel octaves between voices");
            }
        }
    }

    // --- NoDirectFifthsOctaves ---
    // Direct (hidden) fifths/octaves: outer voices move in same direction to P5 or P8
    if (voices >= 2) {
        std::size_t bass_i = 0;
        std::size_t sop_i = voices - 1;
        int bass_motion = static_cast<int>(next[bass_i]) - static_cast<int>(prev[bass_i]);
        int sop_motion = static_cast<int>(next[sop_i]) - static_cast<int>(prev[sop_i]);
        bool same_direction = (bass_motion > 0 && sop_motion > 0) ||
                              (bass_motion < 0 && sop_motion < 0);
        if (same_direction && bass_motion != 0) {
            int next_ic = interval_mod12(next[bass_i], next[sop_i]);
            if (next_ic == 7 || next_ic == 0) {
                // Direct fifth/octave: soprano should move by step
                int sop_step = std::abs(sop_motion);
                if (sop_step > 2) {
                    add_violation(violations, VLConstraintRule::NoDirectFifthsOctaves,
                        get_severity(config, VLConstraintRule::NoDirectFifthsOctaves),
                        static_cast<int>(bass_i), static_cast<int>(sop_i),
                        "Direct fifths/octaves in outer voices");
                }
            }
        }
    }

    // --- LeadingToneResolution ---
    PitchClass lt = leading_tone(config.key_root, config.is_minor);
    for (std::size_t i = 0; i < voices; ++i) {
        if (prev[i] % 12 == lt) {
            // Leading tone should resolve up by semitone to tonic
            PitchClass tonic = config.key_root;
            if (next[i] % 12 != tonic) {
                add_violation(violations, VLConstraintRule::LeadingToneResolution,
                    get_severity(config, VLConstraintRule::LeadingToneResolution),
                    static_cast<int>(i), -1,
                    "Leading tone does not resolve to tonic");
            }
        }
    }

    // --- SeventhResolvesDown ---
    // If the previous chord has a 7th (minor 7th = 10 or major 7th = 11 above root)
    PitchClass prev_root = prev_chord.root;
    PitchClass seventh_m = transpose(prev_root, 10);
    PitchClass seventh_M = transpose(prev_root, 11);
    for (std::size_t i = 0; i < voices; ++i) {
        PitchClass pc = prev[i] % 12;
        if (pc == seventh_m || pc == seventh_M) {
            // Should resolve down by step (1 or 2 semitones)
            int motion = static_cast<int>(next[i]) - static_cast<int>(prev[i]);
            if (motion >= 0) {
                add_violation(violations, VLConstraintRule::SeventhResolvesDown,
                    get_severity(config, VLConstraintRule::SeventhResolvesDown),
                    static_cast<int>(i), -1,
                    "Chord seventh does not resolve downward");
            }
        }
    }

    // --- NoVoiceCrossing ---
    for (std::size_t i = 1; i < voices; ++i) {
        if (next[i] < next[i - 1]) {
            add_violation(violations, VLConstraintRule::NoVoiceCrossing,
                get_severity(config, VLConstraintRule::NoVoiceCrossing),
                static_cast<int>(i - 1), static_cast<int>(i),
                "Voice crossing detected");
        }
    }

    // --- NoLargeLeaps ---
    for (std::size_t i = 0; i < voices; ++i) {
        int leap = std::abs(static_cast<int>(next[i]) - static_cast<int>(prev[i]));
        int max_allowed = (i == 0 || i == voices - 1)
            ? config.max_leap
            : config.max_inner_leap;
        if (leap > max_allowed) {
            add_violation(violations, VLConstraintRule::NoLargeLeaps,
                get_severity(config, VLConstraintRule::NoLargeLeaps),
                static_cast<int>(i), -1,
                "Large leap in voice");
        }
    }

    // --- NoDoubleLeadingTone ---
    int lt_count = 0;
    for (std::size_t i = 0; i < next.size(); ++i) {
        if (next[i] % 12 == lt) lt_count++;
    }
    if (lt_count > 1) {
        add_violation(violations, VLConstraintRule::NoDoubleLeadingTone,
            get_severity(config, VLConstraintRule::NoDoubleLeadingTone),
            -1, -1,
            "Leading tone is doubled");
    }

    // --- StepwiseAfterLeap ---
    // §7.4: After a leap (> 2 semitones), stepwise motion in the opposite
    // direction should follow. Flags leaps so that subsequent transitions
    // can verify compensation.
    for (std::size_t i = 0; i < voices; ++i) {
        int motion = std::abs(static_cast<int>(next[i]) - static_cast<int>(prev[i]));
        if (motion > 2) {
            add_violation(violations, VLConstraintRule::StepwiseAfterLeap,
                get_severity(config, VLConstraintRule::StepwiseAfterLeap),
                static_cast<int>(i), -1,
                "Leap should be followed by stepwise contrary motion");
        }
    }

    // --- CompleteChords ---
    // §7.4: Root, 3rd, 5th present; 5th may be omitted for 7th chords.
    {
        PitchClass root_pc = next_chord.root;
        bool has_third = false;
        bool has_fifth = false;

        for (auto note : next) {
            int above_root = (static_cast<int>(note % 12)
                              - static_cast<int>(root_pc) + 12) % 12;
            if (above_root == 3 || above_root == 4) has_third = true;
            if (above_root == 6 || above_root == 7 || above_root == 8) has_fifth = true;
        }

        // 5th omissible when chord quality implies a 7th or higher extension
        bool seventh_or_above =
            next_chord.quality.find('7') != std::string::npos ||
            next_chord.quality.find('9') != std::string::npos ||
            next_chord.quality.find("11") != std::string::npos ||
            next_chord.quality.find("13") != std::string::npos;

        if (!has_third) {
            add_violation(violations, VLConstraintRule::CompleteChords,
                get_severity(config, VLConstraintRule::CompleteChords),
                -1, -1,
                "Chord is missing the third");
        }
        if (!has_fifth && !seventh_or_above) {
            add_violation(violations, VLConstraintRule::CompleteChords,
                get_severity(config, VLConstraintRule::CompleteChords),
                -1, -1,
                "Chord is missing the fifth");
        }
    }

    // --- DoubleTheRoot ---
    // §7.4: In root position with 4+ voices, prefer doubling the root.
    if (next_chord.inversion == 0 && next.size() >= 4) {
        PitchClass root_pc = next_chord.root;
        int root_count = 0;
        for (auto note : next) {
            if (note % 12 == root_pc) root_count++;
        }
        if (root_count < 2) {
            add_violation(violations, VLConstraintRule::DoubleTheRoot,
                get_severity(config, VLConstraintRule::DoubleTheRoot),
                -1, -1,
                "Root position chord should double the root");
        }
    }

    return violations;
}

bool has_errors(const std::vector<ConstraintViolation>& violations) {
    return std::any_of(violations.begin(), violations.end(),
        [](const ConstraintViolation& v) {
            return v.severity == ConstraintSeverity::Error;
        });
}

std::vector<ConstraintViolation> filter_by_severity(
    const std::vector<ConstraintViolation>& violations,
    ConstraintSeverity severity
) {
    std::vector<ConstraintViolation> result;
    std::copy_if(violations.begin(), violations.end(), std::back_inserter(result),
        [severity](const ConstraintViolation& v) {
            return v.severity == severity;
        });
    return result;
}

}  // namespace Sunny::Core
