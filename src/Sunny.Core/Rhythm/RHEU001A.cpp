/**
 * @file RHEU001A.cpp
 * @brief Euclidean rhythm implementation
 *
 * Component: RHEU001A
 */

#include "RHEU001A.h"

namespace Sunny::Core {

Result<std::vector<bool>> euclidean_rhythm(int pulses, int steps, int rotation) {
    // Validate parameters
    if (steps < 1) {
        return std::unexpected(ErrorCode::EuclideanInvalidParams);
    }
    if (pulses < 0 || pulses > steps) {
        return std::unexpected(ErrorCode::EuclideanInvalidParams);
    }
    if (steps > Constants::EUCLIDEAN_MAX_STEPS) {
        return std::unexpected(ErrorCode::EuclideanInvalidParams);
    }

    // Edge cases
    if (pulses == 0) {
        return std::vector<bool>(steps, false);
    }
    if (pulses == steps) {
        return std::vector<bool>(steps, true);
    }

    // Bjorklund's algorithm for Euclidean rhythms
    // This produces maximally even distribution starting with a pulse
    std::vector<std::vector<bool>> groups;

    // Initialize: pulses groups of {true}, (steps-pulses) groups of {false}
    for (int i = 0; i < pulses; ++i) {
        groups.push_back({true});
    }
    for (int i = 0; i < steps - pulses; ++i) {
        groups.push_back({false});
    }

    // Recursively interleave until convergence
    while (true) {
        std::size_t count_first = 0;
        if (!groups.empty()) {
            auto first_pattern = groups[0];
            for (const auto& g : groups) {
                if (g == first_pattern) count_first++;
                else break;
            }
        }

        std::size_t remainder = groups.size() - count_first;
        if (remainder <= 1) break;

        std::vector<std::vector<bool>> new_groups;
        std::size_t pairs = std::min(count_first, remainder);

        for (std::size_t i = 0; i < pairs; ++i) {
            std::vector<bool> merged = groups[i];
            merged.insert(merged.end(),
                          groups[count_first + i].begin(),
                          groups[count_first + i].end());
            new_groups.push_back(merged);
        }

        // Append remaining unmatched groups
        for (std::size_t i = pairs; i < count_first; ++i) {
            new_groups.push_back(groups[i]);
        }
        for (std::size_t i = pairs; i < remainder; ++i) {
            new_groups.push_back(groups[count_first + i]);
        }

        groups = std::move(new_groups);
    }

    // Flatten groups into pattern
    std::vector<bool> pattern;
    pattern.reserve(steps);
    for (const auto& g : groups) {
        for (bool b : g) {
            pattern.push_back(b);
        }
    }

    // Apply rotation
    if (rotation != 0) {
        rotation = ((rotation % steps) + steps) % steps;
        std::vector<bool> rotated(steps);
        for (int i = 0; i < steps; ++i) {
            rotated[i] = pattern[(i + rotation) % steps];
        }
        return rotated;
    }

    return pattern;
}

std::vector<NoteEvent> euclidean_to_events(
    const std::vector<bool>& pattern,
    Beat step_duration,
    MidiNote pitch,
    Velocity velocity
) {
    std::vector<NoteEvent> events;
    events.reserve(pattern.size());

    Beat current_time = Beat::zero();

    for (std::size_t i = 0; i < pattern.size(); ++i) {
        if (pattern[i]) {
            NoteEvent event;
            event.pitch = pitch;
            event.start_time = current_time;
            event.duration = step_duration;
            event.velocity = velocity;
            event.muted = false;
            events.push_back(event);
        }
        current_time = current_time + step_duration;
    }

    return events;
}

Result<std::vector<bool>> euclidean_preset(std::string_view name) {
    if (name == "tresillo") {
        return euclidean_rhythm(3, 8, 0);
    }
    if (name == "cinquillo") {
        return euclidean_rhythm(5, 8, 0);
    }
    if (name == "son_clave") {
        return euclidean_rhythm(5, 16, 3);
    }
    if (name == "rumba_clave") {
        return euclidean_rhythm(5, 16, 4);
    }
    if (name == "bossa_nova") {
        return euclidean_rhythm(5, 16, 0);
    }
    if (name == "gahu") {
        return euclidean_rhythm(4, 12, 0);
    }

    return std::unexpected(ErrorCode::EuclideanInvalidParams);
}

// =============================================================================
// §9.5 Polyrhythm / Polymetre
// =============================================================================

Result<PolyrhythmResult> polyrhythm_onsets(int m, int n, Beat span) {
    if (m < 1 || n < 1) {
        return std::unexpected(ErrorCode::EuclideanInvalidParams);
    }
    if (span <= Beat::zero()) {
        return std::unexpected(ErrorCode::EuclideanInvalidParams);
    }

    PolyrhythmResult result;
    result.coincident_count = std::gcd(m, n);

    // m-stream onsets: k · span / m
    result.stream_m.reserve(m);
    for (int k = 0; k < m; ++k) {
        result.stream_m.push_back(
            span * Beat{static_cast<std::int64_t>(k),
                        static_cast<std::int64_t>(m)});
    }

    // n-stream onsets: k · span / n
    result.stream_n.reserve(n);
    for (int k = 0; k < n; ++k) {
        result.stream_n.push_back(
            span * Beat{static_cast<std::int64_t>(k),
                        static_cast<std::int64_t>(n)});
    }

    // Merge two sorted streams, deduplicating coincident onsets
    result.combined.reserve(m + n);
    std::size_t i = 0, j = 0;
    while (i < result.stream_m.size() && j < result.stream_n.size()) {
        if (result.stream_m[i] < result.stream_n[j]) {
            result.combined.push_back(result.stream_m[i++]);
        } else if (result.stream_n[j] < result.stream_m[i]) {
            result.combined.push_back(result.stream_n[j++]);
        } else {
            result.combined.push_back(result.stream_m[i]);
            ++i;
            ++j;
        }
    }
    while (i < result.stream_m.size()) {
        result.combined.push_back(result.stream_m[i++]);
    }
    while (j < result.stream_n.size()) {
        result.combined.push_back(result.stream_n[j++]);
    }

    return result;
}

// =============================================================================
// §9.7 Rhythmic Transformations
// =============================================================================

std::vector<Beat> rhythm_scale(std::span<const Beat> durations, Beat factor) {
    std::vector<Beat> result;
    result.reserve(durations.size());
    for (const auto& d : durations) {
        result.push_back(d * factor);
    }
    return result;
}

std::vector<Beat> rhythm_retrograde(std::span<const Beat> durations) {
    return std::vector<Beat>(durations.rbegin(), durations.rend());
}

std::vector<Beat> rhythm_rotate(std::span<const Beat> durations, int k) {
    if (durations.empty()) return {};
    int n = static_cast<int>(durations.size());
    k = ((k % n) + n) % n;
    std::vector<Beat> result;
    result.reserve(n);
    for (int i = 0; i < n; ++i) {
        result.push_back(durations[(i + k) % n]);
    }
    return result;
}

// =============================================================================
// §9.8 Metric Modulation
// =============================================================================

Beat metric_modulation_ratio(int p, int q, Beat old_beat, Beat new_beat) {
    Beat p_over_q = Beat{static_cast<std::int64_t>(p),
                         static_cast<std::int64_t>(q)};
    return p_over_q * (old_beat / new_beat);
}

// =============================================================================
// §9.9 Swing / Shuffle
// =============================================================================

std::vector<NoteEvent> apply_swing(
    std::span<const NoteEvent> events,
    Beat swing_ratio,
    Beat beat_duration
) {
    std::vector<NoteEvent> result(events.begin(), events.end());
    Beat half_beat = beat_duration / 2;

    for (auto& event : result) {
        // Compute beat index: floor(start_time / beat_duration)
        Beat ratio = event.start_time / beat_duration;
        std::int64_t beat_idx = ratio.numerator / ratio.denominator;

        Beat beat_start = beat_duration * beat_idx;
        Beat pos_in_beat = event.start_time - beat_start;

        if (pos_in_beat == half_beat) {
            // Event on the "and" — shift to swung position
            event.start_time = beat_start + beat_duration * swing_ratio;
            event.duration = beat_duration * (Beat::one() - swing_ratio);
        } else if (pos_in_beat == Beat::zero() && event.duration == half_beat) {
            // Downbeat event paired with an "and" — adjust duration
            event.duration = beat_duration * swing_ratio;
        }
    }

    return result;
}

}  // namespace Sunny::Core
