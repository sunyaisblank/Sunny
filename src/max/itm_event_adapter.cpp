/**
 * @file itm_event_adapter.cpp
 * @brief Bounded Max ITM event scheduling adapter
 */

#include <cmath>
#include <limits>
#include <sunny/max/itm_event_adapter.hpp>

namespace sunny::max {

namespace {

[[nodiscard]] sunny::core::Result<double>
unchecked_tick_to_itm(std::int64_t tick, std::int64_t ppq, double host_ticks_per_quarter) noexcept {
    const long double mapped = static_cast<long double>(tick) * host_ticks_per_quarter / ppq;
    if (!std::isfinite(mapped) || mapped > std::numeric_limits<double>::max())
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    const double result = static_cast<double>(mapped);
    if (!std::isfinite(result))
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    return result;
}

} // namespace

sunny::core::Result<double>
map_tick_to_itm(std::int64_t tick, std::int64_t ppq, double host_ticks_per_quarter) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    if (tick < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (!std::isfinite(host_ticks_per_quarter) || host_ticks_per_quarter <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);

    auto mapped = unchecked_tick_to_itm(tick, ppq, host_ticks_per_quarter);
    if (!mapped) return mapped;
    if (tick > 0) {
        auto previous = unchecked_tick_to_itm(tick - 1, ppq, host_ticks_per_quarter);
        if (!previous || *previous >= *mapped)
            return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    }
    if (tick < std::numeric_limits<std::int64_t>::max()) {
        auto next = unchecked_tick_to_itm(tick + 1, ppq, host_ticks_per_quarter);
        if (!next || *next <= *mapped)
            return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    }
    return mapped;
}

sunny::core::Result<std::int64_t>
tick_after_itm(ItmHostSnapshot host, std::int64_t ppq, std::int64_t delay_ticks) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    if (delay_ticks < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    if (!std::isfinite(host.current_tick) || host.current_tick < 0.0 ||
        !std::isfinite(host.ticks_per_quarter) || host.ticks_per_quarter <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);

    const long double sunny_position = static_cast<long double>(host.current_tick) * ppq /
                                       static_cast<long double>(host.ticks_per_quarter);
    if (!std::isfinite(sunny_position))
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);

    const long double sunny_floor = std::floor(sunny_position);
    const long double last_admissible_floor =
        static_cast<long double>(std::numeric_limits<std::int64_t>::max() - delay_ticks - 1);
    if (sunny_floor > last_admissible_floor)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    const auto first_future_tick = static_cast<std::int64_t>(sunny_floor) + 1;
    const auto target_tick = first_future_tick + delay_ticks;
    const auto mapped = map_tick_to_itm(target_tick, ppq, host.ticks_per_quarter);
    if (!mapped) return std::unexpected(mapped.error());
    if (*mapped <= host.current_tick)
        return std::unexpected(sunny::core::ErrorCode::RenderUnrepresentableTiming);
    return target_tick;
}

bool ItmEventAdapter::reserve(std::size_t count) noexcept {
    auto current = events_reserved_.load(std::memory_order_acquire);
    while (current <= MAX_ITM_EVENT_CAPACITY - count) {
        if (events_reserved_.compare_exchange_weak(
                current, current + count, std::memory_order_acq_rel, std::memory_order_acquire))
            return true;
    }
    return false;
}

void ItmEventAdapter::release(std::size_t count) noexcept {
    events_reserved_.fetch_sub(count, std::memory_order_acq_rel);
}

void ItmEventAdapter::record_rejection(sunny::core::ErrorCode error) noexcept {
    commands_rejected_.fetch_add(1, std::memory_order_release);
    last_error_.store(static_cast<int>(error), std::memory_order_release);
}

sunny::core::VoidResult ItmEventAdapter::publish(Command command,
                                                 void* wake_context,
                                                 ItmWakeConsumer wake_consumer) noexcept {
    const auto reservation = static_cast<std::size_t>(command.event_count);
    if (reservation != 0 && !reserve(reservation)) {
        record_rejection(sunny::core::ErrorCode::RenderEventQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderEventQueueFull);
    }
    if (!commands_.try_push(command, [this]() noexcept {
            commands_enqueued_.fetch_add(1, std::memory_order_release);
        })) {
        if (reservation != 0) release(reservation);
        record_rejection(sunny::core::ErrorCode::RenderControlQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderControlQueueFull);
    }
    if (wake_consumer != nullptr) wake_consumer(wake_context);
    return {};
}

sunny::core::VoidResult ItmEventAdapter::publish_event(std::int64_t tick,
                                                       std::int64_t pitch,
                                                       std::int64_t velocity,
                                                       void* wake_context,
                                                       ItmWakeConsumer wake_consumer) noexcept {
    return publish_event(tick, pitch, velocity, 64, wake_context, wake_consumer);
}

sunny::core::VoidResult ItmEventAdapter::publish_event(std::int64_t tick,
                                                       std::int64_t pitch,
                                                       std::int64_t velocity,
                                                       std::int64_t release_velocity,
                                                       void* wake_context,
                                                       ItmWakeConsumer wake_consumer) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (pitch < 0 || pitch > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidMidiNote);
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiNote);
    }
    if (velocity < 0 || velocity > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidVelocity);
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    }
    if (release_velocity < 0 || release_velocity > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidVelocity);
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    }

    sunny::core::NoteEvent note;
    note.pitch = sunny::core::MidiNote::from_int(static_cast<int>(pitch)).value();
    note.velocity = static_cast<sunny::core::Velocity>(velocity);
    note.release_velocity = static_cast<std::uint8_t>(release_velocity);
    auto event = sunny::render::normalise_scheduled_event(
        sunny::render::ScheduledEvent{tick, note}, 0, ppq_);
    if (!event) {
        record_rejection(event.error());
        return std::unexpected(event.error());
    }
    return publish(Command{CommandKind::Events, 1, *event, {}}, wake_context, wake_consumer);
}

sunny::core::VoidResult ItmEventAdapter::publish_note(std::int64_t tick,
                                                      std::int64_t pitch,
                                                      std::int64_t duration_ticks,
                                                      std::int64_t velocity,
                                                      void* wake_context,
                                                      ItmWakeConsumer wake_consumer) noexcept {
    return publish_note(tick, pitch, duration_ticks, velocity, 64, wake_context, wake_consumer);
}

sunny::core::VoidResult ItmEventAdapter::publish_note(std::int64_t tick,
                                                      std::int64_t pitch,
                                                      std::int64_t duration_ticks,
                                                      std::int64_t velocity,
                                                      std::int64_t release_velocity,
                                                      void* wake_context,
                                                      ItmWakeConsumer wake_consumer) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (pitch < 0 || pitch > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidMidiNote);
        return std::unexpected(sunny::core::ErrorCode::InvalidMidiNote);
    }
    if (velocity < 1 || velocity > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidVelocity);
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    }
    if (release_velocity < 0 || release_velocity > 127) {
        record_rejection(sunny::core::ErrorCode::InvalidVelocity);
        return std::unexpected(sunny::core::ErrorCode::InvalidVelocity);
    }
    auto pair = sunny::render::make_scheduled_note_pair_ticks(
        tick,
        0,
        ppq_,
        sunny::core::MidiNote::from_int(static_cast<int>(pitch)).value(),
        duration_ticks,
        static_cast<sunny::core::Velocity>(velocity),
        static_cast<std::uint8_t>(release_velocity));
    if (!pair) {
        record_rejection(pair.error());
        return std::unexpected(pair.error());
    }
    return publish(Command{CommandKind::Events, 2, pair->note_on, pair->note_off},
                   wake_context,
                   wake_consumer);
}

sunny::core::VoidResult ItmEventAdapter::publish_clear(void* wake_context,
                                                       ItmWakeConsumer wake_consumer) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish(Command{CommandKind::Clear, 0, {}, {}}, wake_context, wake_consumer);
}

std::size_t ItmEventAdapter::allocate_slot() noexcept {
    for (std::size_t slot = 0; slot < slot_active_.size(); ++slot) {
        if (!slot_active_[slot]) {
            slot_active_[slot] = true;
            return slot;
        }
    }
    return MAX_ITM_EVENT_CAPACITY;
}

void ItmEventAdapter::insert(QueuedEvent event) noexcept {
    std::size_t insertion = event_count_;
    while (insertion > 0 &&
           sunny::render::scheduled_event_precedes(event.scheduled,
                                                   event.sequence,
                                                   events_[insertion - 1].scheduled,
                                                   events_[insertion - 1].sequence)) {
        events_[insertion] = events_[insertion - 1];
        --insertion;
    }
    events_[insertion] = event;
    ++event_count_;
    events_retained_.store(event_count_, std::memory_order_release);
}

void ItmEventAdapter::erase_range(std::size_t begin, std::size_t count) noexcept {
    for (std::size_t index = begin + count; index < event_count_; ++index)
        events_[index - count] = events_[index];
    event_count_ -= count;
    events_retained_.store(event_count_, std::memory_order_release);
}

ItmApplyReport ItmEventAdapter::apply_pending(ItmHostSnapshot host,
                                              void* action_context,
                                              ItmScheduleSlot schedule_slot,
                                              ItmCancelSlot cancel_slot) noexcept {
    ItmApplyReport report;
    const bool valid_host = std::isfinite(host.current_tick) && host.current_tick >= 0.0 &&
                            std::isfinite(host.ticks_per_quarter) && host.ticks_per_quarter > 0.0;

    Command command;
    for (std::size_t drained = 0; drained < MAX_CONTROL_QUEUE_CAPACITY; ++drained) {
        if (!commands_.try_pop(command)) break;

        if (command.kind == CommandKind::Clear) {
            if (event_count_ != 0 && cancel_slot == nullptr) {
                const auto error = sunny::core::ErrorCode::RenderInvalidParameter;
                record_rejection(error);
                ++report.commands_rejected;
                report.last_error = error;
                continue;
            }
            const auto cancelled = event_count_;
            for (std::size_t index = 0; index < event_count_; ++index) {
                const auto slot = events_[index].slot;
                slot_active_[slot] = false;
                cancel_slot(action_context, slot);
            }
            event_count_ = 0;
            events_retained_.store(0, std::memory_order_release);
            next_sequence_ = 0;
            if (cancelled != 0) {
                release(cancelled);
                events_cancelled_.fetch_add(cancelled, std::memory_order_release);
            }
            commands_applied_.fetch_add(1, std::memory_order_release);
            ++report.commands_applied;
            continue;
        }

        const auto count = static_cast<std::size_t>(command.event_count);
        std::array<double, 2> targets{};
        sunny::core::ErrorCode error = sunny::core::ErrorCode::Ok;
        if (schedule_slot == nullptr) {
            error = sunny::core::ErrorCode::RenderInvalidParameter;
        } else if (!valid_host) {
            error = sunny::core::ErrorCode::RenderUnrepresentableTiming;
        } else {
            const std::array<const sunny::render::ScheduledEvent*, 2> source{&command.first,
                                                                             &command.second};
            for (std::size_t index = 0; index < count; ++index) {
                auto target = map_tick_to_itm(source[index]->tick, ppq_, host.ticks_per_quarter);
                if (!target) {
                    error = target.error();
                    break;
                }
                if (*target <= host.current_tick) {
                    error = sunny::core::ErrorCode::RenderInvalidPosition;
                    break;
                }
                targets[index] = *target;
            }
        }
        if (error == sunny::core::ErrorCode::Ok &&
            next_sequence_ > std::numeric_limits<std::uint64_t>::max() - count)
            error = sunny::core::ErrorCode::ArithmeticOverflow;

        if (error != sunny::core::ErrorCode::Ok) {
            release(count);
            record_rejection(error);
            ++report.commands_rejected;
            report.last_error = error;
            continue;
        }

        const std::array<const sunny::render::ScheduledEvent*, 2> source{&command.first,
                                                                         &command.second};
        std::array<std::size_t, 2> slots{MAX_ITM_EVENT_CAPACITY, MAX_ITM_EVENT_CAPACITY};
        for (std::size_t index = 0; index < count; ++index) {
            slots[index] = allocate_slot();
            // Reservation proves this cannot happen unless the consumer contract was violated.
            if (slots[index] == MAX_ITM_EVENT_CAPACITY) {
                error = sunny::core::ErrorCode::InvariantViolation;
                break;
            }
        }
        if (error != sunny::core::ErrorCode::Ok) {
            for (std::size_t index = 0; index < count; ++index)
                if (slots[index] < MAX_ITM_EVENT_CAPACITY) slot_active_[slots[index]] = false;
            release(count);
            record_rejection(error);
            ++report.commands_rejected;
            report.last_error = error;
            continue;
        }

        for (std::size_t index = 0; index < count; ++index) {
            insert(QueuedEvent{*source[index], next_sequence_++, slots[index]});
            schedule_slot(action_context, slots[index], targets[index]);
        }
        commands_applied_.fetch_add(1, std::memory_order_release);
        events_scheduled_.fetch_add(count, std::memory_order_release);
        ++report.commands_applied;
    }
    return report;
}

sunny::core::Result<std::size_t>
ItmEventAdapter::fire(std::size_t slot,
                      std::span<sunny::render::ScheduledEvent> output,
                      void* action_context,
                      ItmCancelSlot stop_slot) noexcept {
    if (slot >= MAX_ITM_EVENT_CAPACITY) {
        callback_failures_.fetch_add(1, std::memory_order_release);
        last_error_.store(static_cast<int>(sunny::core::ErrorCode::RenderInvalidParameter),
                          std::memory_order_release);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    if (!slot_active_[slot]) return std::size_t{0};

    std::size_t found = event_count_;
    for (std::size_t index = 0; index < event_count_; ++index) {
        if (events_[index].slot == slot) {
            found = index;
            break;
        }
    }
    if (found == event_count_) {
        callback_failures_.fetch_add(1, std::memory_order_release);
        last_error_.store(static_cast<int>(sunny::core::ErrorCode::InvariantViolation),
                          std::memory_order_release);
        return std::unexpected(sunny::core::ErrorCode::InvariantViolation);
    }

    const auto tick = events_[found].scheduled.tick;
    std::size_t begin = found;
    while (begin > 0 && events_[begin - 1].scheduled.tick == tick)
        --begin;
    std::size_t end = found + 1;
    while (end < event_count_ && events_[end].scheduled.tick == tick)
        ++end;
    const auto count = end - begin;
    if (output.size() < count) {
        callback_failures_.fetch_add(1, std::memory_order_release);
        last_error_.store(static_cast<int>(sunny::core::ErrorCode::RenderEventBufferFull),
                          std::memory_order_release);
        return std::unexpected(sunny::core::ErrorCode::RenderEventBufferFull);
    }
    if (stop_slot == nullptr) {
        callback_failures_.fetch_add(1, std::memory_order_release);
        last_error_.store(static_cast<int>(sunny::core::ErrorCode::RenderInvalidParameter),
                          std::memory_order_release);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }

    for (std::size_t index = 0; index < count; ++index) {
        stop_slot(action_context, events_[begin + index].slot);
        output[index] = events_[begin + index].scheduled;
        slot_active_[events_[begin + index].slot] = false;
    }
    erase_range(begin, count);
    release(count);
    events_fired_.fetch_add(count, std::memory_order_release);
    return count;
}

ItmEventAdapterStatus ItmEventAdapter::status() const noexcept {
    return {
        .commands_enqueued = commands_enqueued_.load(std::memory_order_acquire),
        .commands_applied = commands_applied_.load(std::memory_order_acquire),
        .commands_rejected = commands_rejected_.load(std::memory_order_acquire),
        .commands_pending = commands_.pending(),
        .events_scheduled = events_scheduled_.load(std::memory_order_acquire),
        .events_fired = events_fired_.load(std::memory_order_acquire),
        .events_cancelled = events_cancelled_.load(std::memory_order_acquire),
        .callback_failures = callback_failures_.load(std::memory_order_acquire),
        .events_reserved = events_reserved_.load(std::memory_order_acquire),
        .events_retained = events_retained_.load(std::memory_order_acquire),
        .last_error =
            static_cast<sunny::core::ErrorCode>(last_error_.load(std::memory_order_acquire)),
    };
}

void ItmEventAdapter::clear_error() noexcept {
    last_error_.store(static_cast<int>(sunny::core::ErrorCode::Ok), std::memory_order_release);
}

} // namespace sunny::max
