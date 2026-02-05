/**
 * @file RHEU001A.h
 * @brief Euclidean rhythm generation
 *
 * Component: RHEU001A
 * Domain: RH (Rhythm) | Category: EU (Euclidean)
 *
 * Distributes k pulses across n steps as evenly as possible
 * using Bresenham's algorithm (exact integer arithmetic).
 *
 * Invariants:
 * - len(result) == steps
 * - sum(result) == pulses
 * - Pattern maximizes minimum inter-onset interval
 * - Algorithm is O(n) using integer arithmetic only
 */

#pragma once

#include "../Tensor/TNTP001A.h"
#include "../Tensor/TNBT001A.h"
#include "../Tensor/TNEV001A.h"

#include <vector>

namespace Sunny::Core {

/**
 * @brief Generate Euclidean rhythm pattern
 *
 * Uses Bresenham's algorithm for exact integer arithmetic.
 *
 * @param pulses Number of hits (k), where 0 <= k <= n
 * @param steps Total steps in pattern (n), where n >= 1
 * @param rotation Pattern rotation in steps
 * @return Boolean pattern or error
 */
[[nodiscard]] Result<std::vector<bool>> euclidean_rhythm(
    int pulses,
    int steps,
    int rotation = 0
);

/**
 * @brief Convert Euclidean pattern to note events
 *
 * @param pattern Boolean rhythm pattern
 * @param step_duration Duration of each step
 * @param pitch MIDI pitch for hits
 * @param velocity Velocity for hits
 * @return List of NoteEvent objects
 */
[[nodiscard]] std::vector<NoteEvent> euclidean_to_events(
    const std::vector<bool>& pattern,
    Beat step_duration,
    MidiNote pitch,
    Velocity velocity = 100
);

/**
 * @brief Get common Euclidean rhythm by name
 *
 * Known patterns:
 * - "tresillo": E(3,8)
 * - "cinquillo": E(5,8)
 * - "son_clave": E(5,16) rotated
 * - "rumba_clave": E(5,16) different rotation
 * - "bossa_nova": E(5,16)
 *
 * @param name Pattern name
 * @return Pattern or error
 */
[[nodiscard]] Result<std::vector<bool>> euclidean_preset(std::string_view name);

}  // namespace Sunny::Core
