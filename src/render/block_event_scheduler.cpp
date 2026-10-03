/**
 * @file block_event_scheduler.cpp
 * @brief Fixed-capacity tick-to-sample-offset event scheduler
 */

#include <limits>
#include <sunny/render/block_event_scheduler.hpp>
#include <type_traits>

namespace sunny::render {

static_assert(std::is_trivially_copyable_v<ScheduledEvent>);
static_assert(std::is_trivially_copyable_v<SampleOffsetEvent>);

sunny::core::Result<BlockEventScheduler> BlockEventScheduler::create(std::int64_t ppq) noexcept {
    auto clock = BlockClock::create(ppq);
    if (!clock) return std::unexpected(clock.error());
    return BlockEventScheduler{*clock};
}

bool BlockEventScheduler::precedes(const QueuedEvent& lhs, const QueuedEvent& rhs) noexcept {
    return scheduled_event_precedes(lhs.scheduled, lhs.sequence, rhs.scheduled, rhs.sequence);
}

void BlockEventScheduler::insert(QueuedEvent event) noexcept {
    std::size_t insertion = event_count_;
    while (insertion > 0 && precedes(event, events_[insertion - 1])) {
        events_[insertion] = events_[insertion - 1];
        --insertion;
    }
    events_[insertion] = event;
    ++event_count_;
}

void BlockEventScheduler::erase_prefix(std::size_t count) noexcept {
    for (std::size_t index = count; index < event_count_; ++index)
        events_[index - count] = events_[index];
    event_count_ -= count;
}

sunny::core::VoidResult BlockEventScheduler::set_position(std::int64_t ticks) noexcept {
    auto positioned = clock_.set_position(ticks);
    if (!positioned) return positioned;
    std::size_t stale = 0;
    while (stale < event_count_ && events_[stale].scheduled.tick < ticks)
        ++stale;
    erase_prefix(stale);
    return {};
}

sunny::core::VoidResult BlockEventScheduler::schedule(const ScheduledEvent& event) noexcept {
    const auto position = clock_.position();
    auto normalized = normalise_scheduled_event(event, position.ticks, position.ppq);
    if (!normalized) return std::unexpected(normalized.error());
    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    if (event_count_ == BLOCK_EVENT_QUEUE_CAPACITY)
        return std::unexpected(sunny::core::ErrorCode::RenderEventQueueFull);
    insert(QueuedEvent{*normalized, next_sequence_++});
    return {};
}

sunny::core::VoidResult BlockEventScheduler::schedule_note(std::int64_t tick,
                                                           sunny::core::MidiNote pitch,
                                                           sunny::core::Beat duration,
                                                           sunny::core::Velocity velocity,
                                                           std::uint8_t release_velocity) noexcept {
    const auto position = clock_.position();
    auto pair = make_scheduled_note_pair(
        tick, position.ticks, position.ppq, pitch, duration, velocity, release_velocity);
    if (!pair) return std::unexpected(pair.error());
    if (next_sequence_ > std::numeric_limits<std::uint64_t>::max() - 2)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    if (event_count_ > BLOCK_EVENT_QUEUE_CAPACITY - 2)
        return std::unexpected(sunny::core::ErrorCode::RenderEventQueueFull);
    insert(QueuedEvent{pair->note_on, next_sequence_++});
    insert(QueuedEvent{pair->note_off, next_sequence_++});
    return {};
}

void BlockEventScheduler::clear_scheduled() noexcept {
    event_count_ = 0;
    next_sequence_ = 0;
}

sunny::core::Result<BlockEventAdvance>
BlockEventScheduler::process_block(const SignalBlockContext& context,
                                   std::int64_t frame_count,
                                   std::span<SampleOffsetEvent> output) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    auto plan = clock_.plan_block(*frames, context.sample_rate());
    if (!plan) return std::unexpected(plan.error());

    if (!plan->running || *frames == 0) {
        clock_.commit_block(*plan);
        return BlockEventAdvance{
            .clock = BlockClockAdvance{plan->tick_before, plan->tick_after},
            .events_written = 0,
        };
    }

    std::size_t due = 0;
    while (due < event_count_ && plan->contains(events_[due].scheduled.tick))
        ++due;
    if (due > output.size()) return std::unexpected(sunny::core::ErrorCode::RenderEventBufferFull);

    std::array<SampleOffsetEvent, BLOCK_EVENT_QUEUE_CAPACITY> planned{};
    for (std::size_t index = 0; index < due; ++index)
        planned[index] = SampleOffsetEvent{plan->sample_offset(events_[index].scheduled.tick),
                                           events_[index].scheduled};

    for (std::size_t index = 0; index < due; ++index)
        output[index] = planned[index];
    erase_prefix(due);
    clock_.commit_block(*plan);
    return BlockEventAdvance{
        .clock = BlockClockAdvance{plan->tick_before, plan->tick_after},
        .events_written = due,
    };
}

} // namespace sunny::render
