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

}  // namespace Sunny::Core
