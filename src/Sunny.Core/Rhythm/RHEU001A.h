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

// =============================================================================
// §9.5 Polyrhythm / Polymetre
// =============================================================================

/**
 * @brief Result of polyrhythm onset calculation
 */
struct PolyrhythmResult {
    std::vector<Beat> stream_m;     ///< Onsets for the m-stream
    std::vector<Beat> stream_n;     ///< Onsets for the n-stream
    std::vector<Beat> combined;     ///< Merged onsets (sorted, deduplicated)
    int coincident_count;           ///< Number of coincident onsets = gcd(m, n)
};

/**
 * @brief Compute onset times for an m-against-n polyrhythm
 *
 * Formal Spec §9.5.1: Places m equally-spaced onsets against n
 * equally-spaced onsets within the same time span.
 *
 * m-stream onsets: {k · span / m : k = 0, ..., m-1}
 * n-stream onsets: {k · span / n : k = 0, ..., n-1}
 *
 * Invariants:
 * - |stream_m| == m, |stream_n| == n
 * - |combined| == m + n - gcd(m, n)
 * - coincident_count == gcd(m, n)
 *
 * @param m First pulse count (> 0)
 * @param n Second pulse count (> 0)
 * @param span Total time span (must be positive)
 * @return Polyrhythm onset data or error
 */
[[nodiscard]] Result<PolyrhythmResult> polyrhythm_onsets(int m, int n, Beat span);

// =============================================================================
// §9.7 Rhythmic Transformations
// =============================================================================

/**
 * @brief Scale all durations by a rational factor
 *
 * Formal Spec §9.7.1: Augmentation (factor > 1) or diminution (0 < factor < 1).
 * All operations use exact rational arithmetic.
 *
 * Invariants:
 * - rhythm_scale(d, Beat::one()) == d
 * - |result| == |durations|
 *
 * @param durations Input duration sequence
 * @param factor Scaling factor (must be positive)
 * @return Scaled durations
 */
[[nodiscard]] std::vector<Beat> rhythm_scale(
    std::span<const Beat> durations, Beat factor);

/**
 * @brief Reverse a duration sequence
 *
 * Formal Spec §9.7.1: Rhythmic retrograde.
 *
 * Invariant: rhythm_retrograde(rhythm_retrograde(d)) == d
 */
[[nodiscard]] std::vector<Beat> rhythm_retrograde(
    std::span<const Beat> durations);

/**
 * @brief Cyclic rotation of a duration sequence
 *
 * Formal Spec §9.7.1: Cyclic permutation of onsets within a
 * repeating pattern. Positive k shifts left.
 *
 * Invariant: rhythm_rotate(d, |d|) == d
 *
 * @param durations Input duration sequence
 * @param k Rotation amount (positive = shift left)
 * @return Rotated sequence
 */
[[nodiscard]] std::vector<Beat> rhythm_rotate(
    std::span<const Beat> durations, int k);

// =============================================================================
// §9.8 Metric Modulation
// =============================================================================

/**
 * @brief Compute tempo ratio for a metric modulation
 *
 * Formal Spec §9.8.1: T₂ = T₁ · (p/q) · (b₁/b₂)
 * Returns the ratio T₂/T₁ as an exact rational.
 *
 * @param p Tuplet numerator (> 0)
 * @param q Tuplet denominator (> 0)
 * @param old_beat Old beat unit duration
 * @param new_beat New beat unit duration (must be positive)
 * @return Tempo ratio T₂/T₁
 */
[[nodiscard]] Beat metric_modulation_ratio(
    int p, int q, Beat old_beat, Beat new_beat);

// =============================================================================
// §9.9 Swing / Shuffle
// =============================================================================

/**
 * @brief Apply swing displacement to paired subdivisions
 *
 * Formal Spec §9.9.1: Displaces the second note of each pair by
 * swing ratio ρ. A straight pair divides the beat 50/50; swing places
 * the first at ρ·d and the second at (1−ρ)·d.
 *
 * Events on the "and" (half-beat position) are shifted; events at
 * other positions pass through unchanged.
 *
 * Common ratios: 1/2 = straight, 2/3 = triplet swing, 3/4 = hard swing.
 *
 * @param events Input note events (quantised to subdivisions)
 * @param swing_ratio ρ, e.g. Beat{2,3} for triplet swing
 * @param beat_duration Full beat duration d
 * @return Events with swing applied
 */
[[nodiscard]] std::vector<NoteEvent> apply_swing(
    std::span<const NoteEvent> events,
    Beat swing_ratio,
    Beat beat_duration);

}  // namespace Sunny::Core
