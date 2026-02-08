/**
 * @file EuclideanBresenham.h
 * @brief Bresenham-based Euclidean rhythm generation
 *
 * Standalone algorithm extracted from MXEU001A for testability.
 * Pure C++ with zero Max SDK dependencies.
 *
 * Uses integer Bresenham's algorithm for exact Euclidean distribution.
 */

#pragma once

#include <cstddef>
#include <vector>

namespace Sunny::Max::Algorithm {

/**
 * @brief Generate Euclidean rhythm using Bresenham's algorithm
 *
 * Distributes `pulses` hits as evenly as possible across `steps` positions,
 * then rotates by `rotation` positions.
 *
 * @param pattern Output vector, resized to `steps`
 * @param pulses Number of active steps [0, steps]
 * @param steps Total number of steps (must be > 0)
 * @param rotation Pattern rotation offset (any integer, normalised mod steps)
 */
inline void generate_euclidean_pattern(
    std::vector<bool>& pattern,
    long pulses,
    long steps,
    long rotation
) {
    pattern.resize(static_cast<std::size_t>(steps));

    if (steps <= 0) return;

    if (pulses < 0) pulses = 0;
    if (pulses > steps) pulses = steps;

    if (pulses == 0) {
        for (long i = 0; i < steps; i++) pattern[static_cast<std::size_t>(i)] = false;
        return;
    }
    if (pulses == steps) {
        for (long i = 0; i < steps; i++) pattern[static_cast<std::size_t>(i)] = true;
        return;
    }

    // Bresenham's algorithm
    for (long i = 0; i < steps; i++) {
        long threshold = (i + 1) * pulses;
        long prev_threshold = i * pulses;
        pattern[static_cast<std::size_t>(i)] = (threshold / steps) > (prev_threshold / steps);
    }

    // Apply rotation
    if (rotation != 0) {
        rotation = ((rotation % steps) + steps) % steps;
        std::vector<bool> rotated(static_cast<std::size_t>(steps));
        for (long i = 0; i < steps; i++) {
            rotated[static_cast<std::size_t>(i)] =
                pattern[static_cast<std::size_t>((i + rotation) % steps)];
        }
        pattern = rotated;
    }
}

}  // namespace Sunny::Max::Algorithm
