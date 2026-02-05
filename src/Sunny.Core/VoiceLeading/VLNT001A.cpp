/**
 * @file VLNT001A.cpp
 * @brief Nearest-tone voice leading implementation
 *
 * Component: VLNT001A
 */

#include "VLNT001A.h"
#include "../Pitch/PTMN001A.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Sunny::Core {

namespace {

// Check interval between two notes
int get_interval(MidiNote a, MidiNote b) {
    return std::abs(static_cast<int>(a) - static_cast<int>(b)) % 12;
}

// Check for parallel motion at specific interval
bool check_parallel(MidiNote prev_a, MidiNote prev_b,
                    MidiNote next_a, MidiNote next_b,
                    int target_ic) {
    int prev_interval = get_interval(prev_a, prev_b);
    int next_interval = get_interval(next_a, next_b);

    if (prev_interval != target_ic || next_interval != target_ic) {
        return false;
    }

    // Check for parallel (not contrary) motion
    int motion_a = static_cast<int>(next_a) - static_cast<int>(prev_a);
    int motion_b = static_cast<int>(next_b) - static_cast<int>(prev_b);

    // Parallel motion: both move in the same direction
    return (motion_a > 0 && motion_b > 0) || (motion_a < 0 && motion_b < 0);
}

}  // namespace

Result<VoiceLeadingResult> voice_lead_nearest_tone(
    std::span<const MidiNote> source_pitches,
    std::span<const PitchClass> target_pitch_classes,
    bool lock_bass,
    bool allow_parallel_fifths,
    bool allow_parallel_octaves
) {
    if (source_pitches.empty()) {
        return VoiceLeadingResult{{}, 0, false, false};
    }

    if (target_pitch_classes.empty()) {
        return std::unexpected(ErrorCode::VoiceLeadingFailed);
    }

    std::size_t num_voices = source_pitches.size();

    // Extend target PCs if needed
    std::vector<PitchClass> targets(target_pitch_classes.begin(),
                                     target_pitch_classes.end());
    while (targets.size() < num_voices) {
        targets.push_back(targets[targets.size() % target_pitch_classes.size()]);
    }

    VoiceLeadingResult result;
    result.voiced_notes.reserve(num_voices);
    result.total_motion = 0;
    result.has_parallel_fifths = false;
    result.has_parallel_octaves = false;

    std::set<std::size_t> used_targets;

    for (std::size_t i = 0; i < num_voices; ++i) {
        MidiNote current = source_pitches[i];

        if (lock_bass && i == 0) {
            // Bass takes the root (first target PC)
            MidiNote new_pitch = closest_pitch_class_midi(current, targets[0]);
            result.voiced_notes.push_back(new_pitch);
            result.total_motion += std::abs(static_cast<int>(new_pitch) -
                                            static_cast<int>(current));
            used_targets.insert(0);
            continue;
        }

        // Find closest available target
        MidiNote best_pitch = current;
        int best_distance = std::numeric_limits<int>::max();
        std::size_t best_target_idx = 0;

        for (std::size_t j = 0; j < targets.size(); ++j) {
            // Skip used targets if we have enough
            if (used_targets.count(j) && used_targets.size() < targets.size()) {
                continue;
            }

            MidiNote candidate = closest_pitch_class_midi(current, targets[j]);
            int distance = std::abs(static_cast<int>(candidate) -
                                   static_cast<int>(current));

            if (distance < best_distance) {
                best_distance = distance;
                best_pitch = candidate;
                best_target_idx = j;
            }
        }

        result.voiced_notes.push_back(best_pitch);
        result.total_motion += best_distance;
        used_targets.insert(best_target_idx);
    }

    // Fix voice crossings (ensure ascending order)
    for (std::size_t i = 1; i < result.voiced_notes.size(); ++i) {
        while (result.voiced_notes[i] <= result.voiced_notes[i - 1]) {
            if (result.voiced_notes[i] + 12 <= 127) {
                result.voiced_notes[i] += 12;
            } else if (result.voiced_notes[i - 1] >= 12) {
                result.voiced_notes[i - 1] -= 12;
            } else {
                break;  // Can't fix
            }
        }
    }

    // Check for parallel fifths and octaves
    if (source_pitches.size() >= 2) {
        for (std::size_t i = 0; i < source_pitches.size(); ++i) {
            for (std::size_t j = i + 1; j < source_pitches.size(); ++j) {
                if (check_parallel(source_pitches[i], source_pitches[j],
                                   result.voiced_notes[i], result.voiced_notes[j], 7)) {
                    result.has_parallel_fifths = true;
                }
                if (check_parallel(source_pitches[i], source_pitches[j],
                                   result.voiced_notes[i], result.voiced_notes[j], 0)) {
                    result.has_parallel_octaves = true;
                }
            }
        }
    }

    return result;
}

std::vector<MidiNote> generate_close_voicing(
    std::span<const PitchClass> pitch_classes,
    int root_octave
) {
    if (pitch_classes.empty()) {
        return {};
    }

    std::vector<MidiNote> voicing;
    voicing.reserve(pitch_classes.size());

    auto base = pitch_octave_to_midi(pitch_classes[0], root_octave);
    if (!base) {
        return {};
    }

    voicing.push_back(*base);

    // Add remaining notes in ascending order within one octave
    for (std::size_t i = 1; i < pitch_classes.size(); ++i) {
        MidiNote last = voicing.back();
        PitchClass target_pc = pitch_classes[i];

        // Find next occurrence of this PC above the last note
        MidiNote candidate = closest_pitch_class_midi(last, target_pc);
        if (candidate <= last) {
            candidate += 12;
        }
        if (candidate > 127) {
            candidate -= 12;  // Wrap if needed
        }

        voicing.push_back(candidate);
    }

    return voicing;
}

std::vector<MidiNote> generate_drop2_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 4) {
        return std::vector<MidiNote>(close_voicing.begin(), close_voicing.end());
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop the second-from-top note down an octave
    std::size_t drop_idx = result.size() - 2;
    if (result[drop_idx] >= 12) {
        result[drop_idx] -= 12;
    }

    // Re-sort to maintain ascending order
    std::sort(result.begin(), result.end());

    return result;
}

std::vector<MidiNote> generate_drop3_voicing(std::span<const MidiNote> close_voicing) {
    if (close_voicing.size() < 4) {
        return std::vector<MidiNote>(close_voicing.begin(), close_voicing.end());
    }

    std::vector<MidiNote> result(close_voicing.begin(), close_voicing.end());

    // Drop the third-from-top note down an octave
    std::size_t drop_idx = result.size() - 3;
    if (result[drop_idx] >= 12) {
        result[drop_idx] -= 12;
    }

    std::sort(result.begin(), result.end());

    return result;
}

bool has_parallel_motion(
    MidiNote prev_bass,
    MidiNote prev_upper,
    MidiNote next_bass,
    MidiNote next_upper,
    int interval_class
) {
    return check_parallel(prev_bass, prev_upper, next_bass, next_upper, interval_class);
}

}  // namespace Sunny::Core
