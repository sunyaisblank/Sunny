/**
 * @file itm_event_adapter.hpp
 * @brief Bounded producer-to-Max-ITM event scheduling adapter
 */

#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <sunny/max/control_transfer.hpp>
#include <sunny/render/block_clock.hpp>
#include <sunny/render/scheduled_event.hpp>

namespace sunny::max {

inline constexpr std::size_t MAX_ITM_EVENT_CAPACITY = 256;

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "ITM evidence counters must not hide a library mutex");
static_assert(std::atomic<std::size_t>::is_always_lock_free,
              "ITM reservation admission must not hide a library mutex");
static_assert(std::atomic<int>::is_always_lock_free,
              "ITM error publication must not hide a library mutex");

/// Host timing facts sampled by the one Max scheduler consumer.
struct ItmHostSnapshot {
    double current_tick;
    double ticks_per_quarter;
};

/// Monotone counters plus distinct producer-queue, admission, and scheduler-retention gauges.
struct ItmEventAdapterStatus {
    std::uint64_t commands_enqueued = 0;
    std::uint64_t commands_applied = 0;
    std::uint64_t commands_rejected = 0;
    std::size_t commands_pending = 0;
    std::uint64_t events_scheduled = 0;
    std::uint64_t events_fired = 0;
    std::uint64_t events_cancelled = 0;
    std::uint64_t callback_failures = 0;
    std::size_t events_reserved = 0;
    std::size_t events_retained = 0;
    sunny::core::ErrorCode last_error = sunny::core::ErrorCode::Ok;
};

/// Result of one bounded command drain on the Max scheduler thread.
struct ItmApplyReport {
    std::size_t commands_applied = 0;
    std::size_t commands_rejected = 0;
    sunny::core::ErrorCode last_error = sunny::core::ErrorCode::Ok;
};

using ItmScheduleSlot = void (*)(void* context, std::size_t slot, double host_tick) noexcept;
using ItmCancelSlot = void (*)(void* context, std::size_t slot) noexcept;
using ItmWakeConsumer = void (*)(void* context) noexcept;

/**
 * Convert one nonnegative Sunny PPQ tick into Max ITM's double tick domain. Conversion is rejected
 * once adjacent Sunny ticks would collapse to the same host value.
 */
[[nodiscard]] sunny::core::Result<double>
map_tick_to_itm(std::int64_t tick, std::int64_t ppq, double host_ticks_per_quarter) noexcept;

/**
 * Derive a Sunny tick from a sampled Max transport position. A zero delay selects the first
 * representable Sunny tick whose mapped ITM value is strictly later than `host.current_tick`;
 * positive delays add whole Sunny ticks after that boundary.
 *
 * The result is a publication target, not a reservation of host time. `apply_pending()` still
 * rejects it if the transport reaches or passes the target before the command is consumed.
 */
[[nodiscard]] sunny::core::Result<std::int64_t>
tick_after_itm(ItmHostSnapshot host, std::int64_t ppq, std::int64_t delay_ticks) noexcept;

/**
 * @brief Fixed-capacity bridge between arbitrary Max message publishers and one ITM consumer.
 *
 * Publishers are serialized into a 64-command SPSC stream. A single Max scheduler callback owns
 * the 256-event sorted queue and fixed slot map. `apply_pending()` and `fire()` must therefore be
 * called by the same serial Max scheduler context; neither is an audio perform routine.
 */
class ItmEventAdapter {
  public:
    explicit ItmEventAdapter(std::int64_t ppq = sunny::render::DEFAULT_PPQ) noexcept : ppq_(ppq) {}
    ItmEventAdapter(const ItmEventAdapter&) = delete;
    ItmEventAdapter& operator=(const ItmEventAdapter&) = delete;
    ItmEventAdapter(ItmEventAdapter&&) = delete;
    ItmEventAdapter& operator=(ItmEventAdapter&&) = delete;

    [[nodiscard]] sunny::core::VoidResult
    publish_event(std::int64_t tick,
                  std::int64_t pitch,
                  std::int64_t velocity,
                  void* wake_context = nullptr,
                  ItmWakeConsumer wake_consumer = nullptr) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_event(std::int64_t tick,
                                                        std::int64_t pitch,
                                                        std::int64_t velocity,
                                                        std::int64_t release_velocity,
                                                        void* wake_context,
                                                        ItmWakeConsumer wake_consumer) noexcept;
    [[nodiscard]] sunny::core::VoidResult
    publish_note(std::int64_t tick,
                 std::int64_t pitch,
                 std::int64_t duration_ticks,
                 std::int64_t velocity,
                 void* wake_context = nullptr,
                 ItmWakeConsumer wake_consumer = nullptr) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_note(std::int64_t tick,
                                                       std::int64_t pitch,
                                                       std::int64_t duration_ticks,
                                                       std::int64_t velocity,
                                                       std::int64_t release_velocity,
                                                       void* wake_context,
                                                       ItmWakeConsumer wake_consumer) noexcept;
    [[nodiscard]] sunny::core::VoidResult
    publish_clear(void* wake_context = nullptr, ItmWakeConsumer wake_consumer = nullptr) noexcept;

    [[nodiscard]] ItmApplyReport apply_pending(ItmHostSnapshot host,
                                               void* action_context,
                                               ItmScheduleSlot schedule_slot,
                                               ItmCancelSlot cancel_slot) noexcept;

    /**
     * Consume the entire equal-source-tick group represented by `slot`. Insufficient caller
     * storage or a missing host-stop action preserves the event queue and every slot.
     */
    [[nodiscard]] sunny::core::Result<std::size_t>
    fire(std::size_t slot,
         std::span<sunny::render::ScheduledEvent> output,
         void* action_context,
         ItmCancelSlot stop_slot) noexcept;

    [[nodiscard]] ItmEventAdapterStatus status() const noexcept;
    void clear_error() noexcept;
    [[nodiscard]] std::int64_t ppq() const noexcept { return ppq_; }

  private:
    enum class CommandKind : std::uint8_t { Events, Clear };

    struct Command {
        CommandKind kind{};
        std::uint8_t event_count = 0;
        sunny::render::ScheduledEvent first{};
        sunny::render::ScheduledEvent second{};
    };

    struct QueuedEvent {
        sunny::render::ScheduledEvent scheduled{};
        std::uint64_t sequence = 0;
        std::size_t slot = 0;
    };

    [[nodiscard]] sunny::core::VoidResult
    publish(Command command, void* wake_context, ItmWakeConsumer wake_consumer) noexcept;
    [[nodiscard]] bool reserve(std::size_t count) noexcept;
    void release(std::size_t count) noexcept;
    [[nodiscard]] std::size_t allocate_slot() noexcept;
    void insert(QueuedEvent event) noexcept;
    void erase_range(std::size_t begin, std::size_t count) noexcept;
    void record_rejection(sunny::core::ErrorCode error) noexcept;

    std::int64_t ppq_;
    detail::SerializedSpscQueue<Command, MAX_CONTROL_QUEUE_CAPACITY> commands_;
    mutable std::mutex publisher_mutex_;

    // Scheduler-consumer-owned state.
    std::array<QueuedEvent, MAX_ITM_EVENT_CAPACITY> events_{};
    std::array<bool, MAX_ITM_EVENT_CAPACITY> slot_active_{};
    std::size_t event_count_{0};
    std::uint64_t next_sequence_{0};

    // Cross-context evidence and admission gauge.
    std::atomic<std::uint64_t> commands_enqueued_{0};
    std::atomic<std::uint64_t> commands_applied_{0};
    std::atomic<std::uint64_t> commands_rejected_{0};
    std::atomic<std::uint64_t> events_scheduled_{0};
    std::atomic<std::uint64_t> events_fired_{0};
    std::atomic<std::uint64_t> events_cancelled_{0};
    std::atomic<std::uint64_t> callback_failures_{0};
    std::atomic<std::size_t> events_reserved_{0};
    std::atomic<std::size_t> events_retained_{0};
    std::atomic<int> last_error_{static_cast<int>(sunny::core::ErrorCode::Ok)};
};

} // namespace sunny::max
