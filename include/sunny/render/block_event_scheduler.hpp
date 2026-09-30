/**
 * @file block_event_scheduler.hpp
 * @brief Fixed-capacity tick-to-sample-offset event scheduler
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <sunny/render/block_clock.hpp>
#include <sunny/render/scheduled_event.hpp>

namespace sunny::render {

inline constexpr std::size_t BLOCK_EVENT_QUEUE_CAPACITY = 256;

/// One scheduled event assigned to a zero-based offset in an admitted signal vector.
struct SampleOffsetEvent {
    std::size_t sample_offset;
    ScheduledEvent scheduled;
};

/// Clock endpoint and event cardinality committed by one admitted vector.
struct BlockEventAdvance {
    BlockClockAdvance clock;
    std::size_t events_written;
};

/**
 * @brief Exclusive-owner bounded event planner without callbacks or dynamic storage.
 *
 * Scheduled events are retained in tick/note-off/insertion order. A running block assigns every
 * event in its half-open exact local tick interval to the containing sample. Events at or behind
 * the retained start map to offset zero; an event exactly at the post-vector endpoint remains for
 * the next non-empty block. All operations on one instance require one exclusive owner.
 */
class BlockEventScheduler {
  public:
    BlockEventScheduler() = default;
    [[nodiscard]] static sunny::core::Result<BlockEventScheduler> create(std::int64_t ppq) noexcept;

    void play() noexcept { clock_.play(); }
    void record() noexcept { clock_.record(); }
    void stop() noexcept { clock_.stop(); }
    void pause() noexcept { clock_.pause(); }
    [[nodiscard]] sunny::core::VoidResult set_tempo(double bpm) noexcept {
        return clock_.set_tempo(bpm);
    }
    [[nodiscard]] sunny::core::VoidResult set_position(std::int64_t ticks) noexcept;

    [[nodiscard]] TransportState state() const noexcept { return clock_.state(); }
    [[nodiscard]] TransportPosition position() const noexcept { return clock_.position(); }
    [[nodiscard]] double tempo() const noexcept { return clock_.tempo(); }
    [[nodiscard]] bool is_running() const noexcept { return clock_.is_running(); }
    [[nodiscard]] std::size_t scheduled_count() const noexcept { return event_count_; }

    [[nodiscard]] sunny::core::VoidResult schedule(const ScheduledEvent& event) noexcept;
    [[nodiscard]] sunny::core::VoidResult
    schedule_note(std::int64_t tick,
                  sunny::core::MidiNote pitch,
                  sunny::core::Beat duration,
                  sunny::core::Velocity velocity = 100,
                  std::uint8_t release_velocity = 64) noexcept;
    void clear_scheduled() noexcept;

    /**
     * Validate and plan the entire vector before output, queue, or clock mutation. The event span
     * must hold every due event; insufficient storage is fail-closed.
     */
    [[nodiscard]] sunny::core::Result<BlockEventAdvance>
    process_block(const SignalBlockContext& context,
                  std::int64_t frame_count,
                  std::span<SampleOffsetEvent> events) noexcept;

  private:
    struct QueuedEvent {
        ScheduledEvent scheduled;
        std::uint64_t sequence;
    };

    BlockClock clock_;
    std::array<QueuedEvent, BLOCK_EVENT_QUEUE_CAPACITY> events_{};
    std::size_t event_count_{0};
    std::uint64_t next_sequence_{0};

    explicit BlockEventScheduler(BlockClock clock) noexcept : clock_(clock) {}
    [[nodiscard]] static bool precedes(const QueuedEvent& lhs, const QueuedEvent& rhs) noexcept;
    void insert(QueuedEvent event) noexcept;
    void erase_prefix(std::size_t count) noexcept;
};

} // namespace sunny::render
