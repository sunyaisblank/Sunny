/**
 * @file VoiceLeadStandalone.h
 * @brief Standalone voice leading algorithm
 *
 * Extracted from MXVL001A for testability.
 * Pure C++ with zero Max SDK dependencies.
 *
 * Implements nearest-tone voice leading with optional bass locking,
 * max jump constraint, and voice crossing prevention.
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

namespace Sunny::Max::Algorithm {

/**
 * @brief Find the nearest MIDI pitch to source with the given pitch class
 *
 * @param source Source MIDI note [0, 127]
 * @param target_pc Target pitch class [0, 11]
 * @return Nearest MIDI note with the target pitch class
 */
inline long nearest_pitch(long source, long target_pc) {
    long source_pc = source % 12;
    long diff = target_pc - source_pc;

    // Normalise to [-6, 5] for minimal motion
    if (diff > 6) diff -= 12;
    if (diff < -6) diff += 12;

    long result = source + diff;

    // Clamp to valid MIDI range
    if (result < 0) result = target_pc;
    if (result > 127) result = 127 - (12 - target_pc);

    return result;
}

/**
 * @brief Compute optimal voice leading between source and target chords
 *
 * @param source Source MIDI notes
 * @param target_pcs Target pitch classes [0, 11]
 * @param result Output: result MIDI notes
 * @param lock_bass If true, bass voice is locked to root (first target PC)
 * @param max_jump Maximum voice jump in semitones (0 = unlimited)
 * @return Total voice motion (sum of absolute semitone movements)
 */
inline long compute_voice_leading(
    const std::vector<long>& source,
    const std::vector<long>& target_pcs,
    std::vector<long>& result,
    bool lock_bass,
    long max_jump
) {
    result.clear();
    if (source.empty() || target_pcs.empty()) return 0;

    // Extend target PCs if needed
    std::vector<long> targets = target_pcs;
    while (targets.size() < source.size()) {
        targets.push_back(target_pcs[targets.size() % target_pcs.size()]);
    }

    std::vector<bool> used(targets.size(), false);
    long total_motion = 0;

    for (std::size_t i = 0; i < source.size(); i++) {
        long src = source[i];
        long best_pitch = src;
        long best_distance = std::numeric_limits<long>::max();
        std::size_t best_idx = 0;

        if (lock_bass && i == 0) {
            best_pitch = nearest_pitch(src, targets[0]);
            used[0] = true;
        } else {
            for (std::size_t j = 0; j < targets.size(); j++) {
                if (used[j] && used.size() > source.size()) continue;

                long candidate = nearest_pitch(src, targets[j]);
                long distance = std::abs(candidate - src);

                if (!used[j] && distance < best_distance) {
                    best_distance = distance;
                    best_pitch = candidate;
                    best_idx = j;
                } else if (used[j] && distance < best_distance - 2) {
                    best_distance = distance;
                    best_pitch = candidate;
                    best_idx = j;
                }
            }
            used[best_idx] = true;
        }

        // Apply max jump constraint
        long motion = std::abs(best_pitch - src);
        if (max_jump > 0 && motion > max_jump) {
            if (best_pitch > src) {
                best_pitch = src + max_jump;
            } else {
                best_pitch = src - max_jump;
            }
            long target_pc = targets[best_idx];
            long d = target_pc - (best_pitch % 12);
            if (d > 6) d -= 12;
            if (d < -6) d += 12;
            best_pitch += d;
            best_pitch = std::clamp(best_pitch, 0L, 127L);
        }

        result.push_back(best_pitch);
        total_motion += std::abs(best_pitch - src);
    }

    // Fix voice crossings
    for (std::size_t i = 1; i < result.size(); i++) {
        if (result[i] <= result[i - 1]) {
            while (result[i] <= result[i - 1] && result[i] + 12 <= 127) {
                result[i] += 12;
            }
        }
    }

    return total_motion;
}

}  // namespace Sunny::Max::Algorithm
