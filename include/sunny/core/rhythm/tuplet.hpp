/**
 * @file tuplet.hpp
 * @brief Tuplet representation and computation
 *
 *
 * Represents m-in-the-space-of-n tuplets using exact rational arithmetic.
 * All durations use the Beat type to avoid floating-point precision loss.
 *
 * Invariants:
 * - tuplet_total_span(t) == t.normal * t.base_duration
 * - tuplet_note_duration(t) * t.actual == tuplet_total_span(t)
 * - Nested tuplet ratios compose multiplicatively
 */

#pragma once

#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <vector>

namespace sunny::core {

/**
 * @brief Tuplet specification
 *
 * Represents m notes played in the time of n normal subdivisions.
 * E.g. triplet eighth = {actual=3, normal=2, base_duration=1/8}
 */
struct Tuplet {
    int actual{1};        ///< m notes played
    int normal{1};        ///< in the span of n
    Beat base_duration{}; ///< duration of one normal-subdivision note
};

/**
 * @brief Construct a validated tuplet
 *
 * @param actual Number of notes in the tuplet (m)
 * @param normal Number of normal subdivisions they replace (n)
 * @param base_duration Duration of one normal subdivision
 * @return Tuplet or error if ratio is invalid
 */
[[nodiscard]] Result<Tuplet> make_tuplet(int actual, int normal, Beat base_duration);

/**
 * @brief Duration of a single note in the tuplet
 *
 * Computed as (n/m) * base_duration, exact rational.
 *
 * @return Beat duration of one tuplet note
 */
[[nodiscard]] constexpr Beat tuplet_note_duration(const Tuplet& t) noexcept {
    return t.base_duration * Beat{t.normal, t.actual};
}

/**
 * @brief Total span of the tuplet group
 *
 * Equal to normal * base_duration.
 *
 * @return Total duration the tuplet occupies
 */
[[nodiscard]] constexpr Beat tuplet_total_span(const Tuplet& t) noexcept {
    return t.base_duration * t.normal;
}

/**
 * @brief Generate evenly-spaced note events for a tuplet
 *
 * @param tuplet Tuplet specification
 * @param start Start time for the first note
 * @param pitch MIDI pitch for all notes
 * @param velocity MIDI velocity
 * @return Vector of NoteEvent objects
 */
[[nodiscard]] std::vector<NoteEvent>
tuplet_to_events(const Tuplet& tuplet, Beat start, MidiNote pitch, Velocity velocity = 100);

/**
 * @brief Compute the note duration for a nested tuplet
 *
 * An inner tuplet inside an outer tuplet: the inner's base duration
 * is the outer's note duration. The compound ratio is
 * (outer_normal * inner_normal) / (outer_actual * inner_actual) * base_duration.
 *
 * @param outer The outer tuplet
 * @param inner_actual Number of notes in the inner tuplet
 * @param inner_normal Normal subdivisions for the inner tuplet
 * @return Duration of one inner tuplet note
 */
[[nodiscard]] constexpr Beat
nested_tuplet_duration(const Tuplet& outer, int inner_actual, int inner_normal) noexcept {
    return tuplet_note_duration(outer) * Beat{inner_normal, inner_actual};
}

} // namespace sunny::core
