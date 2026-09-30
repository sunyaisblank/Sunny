/**
 * @file transport.hpp
 * @brief MIDI Transport and Scheduling
 *
 *
 * Provides deterministic, tick-based MIDI event scheduling with:
 * - Tick-based timing (PPQ resolution)
 * - Transport state management (play, stop, pause)
 * - Tempo-synced scheduling
 *
 * Invariants:
 * - Explicit tick advances use checked integer arithmetic
 * - Audio-block conversion retains fractional ticks between calls
 * - Events are processed in tick order
 * - Transport state transitions follow the documented state machine
 *
 * Thread/host boundary:
 * - A Transport instance is exclusively owned and has no internal synchronisation.
 * - Its block overload is vector-bounded but not an audio-thread/Max perform routine: dispatch can
 *   drain an unbounded event backlog and invoke arbitrary callbacks.
 * - Callbacks observe the committed endpoint, must not throw, and must not invoke a mutating
 *   operation on the same Transport reentrantly.
 */

#pragma once

#include <cstdint>
#include <functional>
#include <queue>
#include <sunny/core/types/music_types.hpp>
#include <sunny/core/types/note_event.hpp>
#include <sunny/render/block_clock.hpp>
#include <sunny/render/scheduled_event.hpp>
#include <vector>

namespace sunny::render {

/// MIDI event callback type
using MidiCallback = std::function<void(const sunny::core::NoteEvent&)>;

/**
 * @brief Transport scheduler for deterministic tick-boundary MIDI timing
 *
 * Manages a queue of scheduled events and dispatches them
 * at the correct tick positions.
 */
class Transport {
  public:
    Transport() = default;
    [[nodiscard]] static sunny::core::Result<Transport> create(std::int64_t ppq);

    // Transport control
    void play();
    void record();
    void stop();
    void pause();
    sunny::core::VoidResult set_tempo(double bpm);
    sunny::core::VoidResult set_position(std::int64_t ticks);

    // State queries
    [[nodiscard]] TransportState state() const noexcept { return clock_.state(); }
    [[nodiscard]] TransportPosition position() const noexcept { return clock_.position(); }
    [[nodiscard]] double tempo() const noexcept { return clock_.tempo(); }
    [[nodiscard]] bool is_playing() const noexcept { return clock_.is_playing(); }
    [[nodiscard]] bool is_running() const noexcept { return clock_.is_running(); }

    // Event scheduling
    sunny::core::VoidResult schedule(const ScheduledEvent& event);
    sunny::core::VoidResult schedule_note(std::int64_t tick,
                                          sunny::core::MidiNote pitch,
                                          sunny::core::Beat duration,
                                          sunny::core::Velocity velocity = 100,
                                          std::uint8_t release_velocity = 64);
    void clear_scheduled();

    // Processing. These methods are not real-time safe because event dispatch is unbounded and
    // callbacks are unconstrained; SignalBlockContext validates cardinality only.
    sunny::core::VoidResult advance(std::int64_t ticks);
    sunny::core::VoidResult process_block(std::size_t sample_count, double sample_rate);
    sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                          std::int64_t sample_count);

    // Callbacks. Handlers may query the committed endpoint, must not throw, and must not mutate
    // this instance reentrantly.
    void set_note_on_callback(MidiCallback callback);
    void set_note_off_callback(MidiCallback callback);

  private:
    BlockClock clock_;
    std::uint64_t next_sequence_{0};

    struct QueuedEvent {
        ScheduledEvent scheduled;
        std::uint64_t sequence;
    };

    struct EventLater {
        [[nodiscard]] bool operator()(const QueuedEvent& lhs,
                                      const QueuedEvent& rhs) const noexcept;
    };

    std::priority_queue<QueuedEvent, std::vector<QueuedEvent>, EventLater> event_queue_;

    MidiCallback note_on_callback_;
    MidiCallback note_off_callback_;

    explicit Transport(BlockClock clock) noexcept : clock_(clock) {}
    [[nodiscard]] sunny::core::VoidResult enqueue(const ScheduledEvent& event);
    void dispatch_events_until(std::int64_t tick);
    void discard_events_before(std::int64_t tick);
};

} // namespace sunny::render
