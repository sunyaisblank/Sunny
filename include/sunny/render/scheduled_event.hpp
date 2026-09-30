/**
 * @file scheduled_event.hpp
 * @brief Shared validated tick-scheduled MIDI event construction
 */

#pragma once

#include <cstdint>
#include <sunny/core/types/note_event.hpp>

namespace sunny::render {

/// One MIDI event assigned to an integer PPQ tick.
struct ScheduledEvent {
    std::int64_t tick;
    sunny::core::NoteEvent event;

    [[nodiscard]] constexpr bool operator>(const ScheduledEvent& other) const noexcept {
        return tick > other.tick;
    }
};

/// Atomic note-on/note-off construction result.
struct ScheduledNotePair {
    ScheduledEvent note_on;
    ScheduledEvent note_off;
};

/**
 * Total event order shared by every Sunny scheduler: tick, note-off before note-on, then insertion
 * sequence. The sequence arguments must be unique within one scheduler lifetime.
 */
[[nodiscard]] bool scheduled_event_precedes(const ScheduledEvent& lhs,
                                            std::uint64_t lhs_sequence,
                                            const ScheduledEvent& rhs,
                                            std::uint64_t rhs_sequence) noexcept;

/** Validate tick/velocity and replace caller-supplied start time with the canonical tick value. */
[[nodiscard]] sunny::core::Result<ScheduledEvent> normalise_scheduled_event(
    const ScheduledEvent& event, std::int64_t current_tick, std::int64_t ppq) noexcept;

/** Construct an integral-tick note pair without mutating a scheduler. */
[[nodiscard]] sunny::core::Result<ScheduledNotePair>
make_scheduled_note_pair(std::int64_t tick,
                         std::int64_t current_tick,
                         std::int64_t ppq,
                         sunny::core::MidiNote pitch,
                         sunny::core::Beat duration,
                         sunny::core::Velocity velocity,
                         std::uint8_t release_velocity = 64) noexcept;

/** Construct an integral-tick note pair without a rational intermediate multiplication. */
[[nodiscard]] sunny::core::Result<ScheduledNotePair>
make_scheduled_note_pair_ticks(std::int64_t tick,
                               std::int64_t current_tick,
                               std::int64_t ppq,
                               sunny::core::MidiNote pitch,
                               std::int64_t duration_ticks,
                               sunny::core::Velocity velocity,
                               std::uint8_t release_velocity = 64) noexcept;

} // namespace sunny::render
