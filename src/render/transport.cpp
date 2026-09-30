/**
 * @file transport.cpp
 * @brief MIDI Transport implementation
 */

#include <limits>
#include <sunny/render/transport.hpp>

namespace sunny::render {

sunny::core::Result<Transport> Transport::create(std::int64_t ppq) {
    auto clock = BlockClock::create(ppq);
    if (!clock) return std::unexpected(clock.error());
    return Transport{*clock};
}

bool Transport::EventLater::operator()(const QueuedEvent& lhs,
                                       const QueuedEvent& rhs) const noexcept {
    return scheduled_event_precedes(rhs.scheduled, rhs.sequence, lhs.scheduled, lhs.sequence);
}

void Transport::play() {
    clock_.play();
}

void Transport::record() {
    clock_.record();
}

void Transport::stop() {
    clock_.stop();
}

void Transport::pause() {
    clock_.pause();
}

sunny::core::VoidResult Transport::set_tempo(double bpm) {
    return clock_.set_tempo(bpm);
}

sunny::core::VoidResult Transport::set_position(std::int64_t ticks) {
    auto positioned = clock_.set_position(ticks);
    if (!positioned) return positioned;
    discard_events_before(ticks);
    return {};
}

sunny::core::VoidResult Transport::enqueue(const ScheduledEvent& event) {
    const auto position = clock_.position();
    auto normalized = normalise_scheduled_event(event, position.ticks, position.ppq);
    if (!normalized) return std::unexpected(normalized.error());
    if (next_sequence_ == std::numeric_limits<std::uint64_t>::max())
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    event_queue_.push(QueuedEvent{*normalized, next_sequence_++});
    return {};
}

sunny::core::VoidResult Transport::schedule(const ScheduledEvent& event) {
    return enqueue(event);
}

sunny::core::VoidResult Transport::schedule_note(std::int64_t tick,
                                                 sunny::core::MidiNote pitch,
                                                 sunny::core::Beat duration,
                                                 sunny::core::Velocity velocity,
                                                 std::uint8_t release_velocity) {
    const auto position = clock_.position();
    auto events = make_scheduled_note_pair(
        tick, position.ticks, position.ppq, pitch, duration, velocity, release_velocity);
    if (!events) return std::unexpected(events.error());
    if (next_sequence_ > std::numeric_limits<std::uint64_t>::max() - 2)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    event_queue_.push(QueuedEvent{events->note_on, next_sequence_++});
    event_queue_.push(QueuedEvent{events->note_off, next_sequence_++});
    return {};
}

void Transport::clear_scheduled() {
    while (!event_queue_.empty())
        event_queue_.pop();
    next_sequence_ = 0;
}

sunny::core::VoidResult Transport::advance(std::int64_t ticks) {
    const bool was_running = clock_.is_running();
    auto advanced = clock_.advance(ticks);
    if (!advanced) return std::unexpected(advanced.error());
    if (was_running) dispatch_events_until(advanced->tick_after);
    return {};
}

sunny::core::VoidResult Transport::process_block(std::size_t sample_count, double sample_rate) {
    const bool was_running = clock_.is_running();
    auto advanced = clock_.process_block(sample_count, sample_rate);
    if (!advanced) return std::unexpected(advanced.error());
    if (was_running) dispatch_events_until(advanced->tick_after);
    return {};
}

sunny::core::VoidResult Transport::process_block(const SignalBlockContext& context,
                                                 std::int64_t sample_count) {
    const bool was_running = clock_.is_running();
    auto advanced = clock_.process_block(context, sample_count);
    if (!advanced) return std::unexpected(advanced.error());
    if (was_running) dispatch_events_until(advanced->tick_after);
    return {};
}

void Transport::set_note_on_callback(MidiCallback callback) {
    note_on_callback_ = std::move(callback);
}

void Transport::set_note_off_callback(MidiCallback callback) {
    note_off_callback_ = std::move(callback);
}

void Transport::dispatch_events_until(std::int64_t tick) {
    while (!event_queue_.empty() && event_queue_.top().scheduled.tick <= tick) {
        const auto scheduled = event_queue_.top().scheduled;
        event_queue_.pop();

        if (scheduled.event.velocity > 0) {
            if (note_on_callback_) note_on_callback_(scheduled.event);
        } else {
            if (note_off_callback_) note_off_callback_(scheduled.event);
        }
    }
}

void Transport::discard_events_before(std::int64_t tick) {
    while (!event_queue_.empty() && event_queue_.top().scheduled.tick < tick)
        event_queue_.pop();
}

} // namespace sunny::render
