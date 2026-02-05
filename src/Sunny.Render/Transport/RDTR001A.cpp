/**
 * @file RDTR001A.cpp
 * @brief MIDI Transport implementation
 *
 * Component: RDTR001A
 */

#include "RDTR001A.h"

namespace Sunny::Render {

Transport::Transport(std::int64_t ppq)
    : ppq_(ppq) {}

void Transport::play() {
    state_ = TransportState::Playing;
}

void Transport::stop() {
    state_ = TransportState::Stopped;
    current_tick_ = 0;
}

void Transport::pause() {
    if (state_ == TransportState::Playing) {
        state_ = TransportState::Paused;
    }
}

void Transport::set_tempo(double bpm) {
    if (bpm >= 20.0 && bpm <= 999.0) {
        tempo_bpm_ = bpm;
    }
}

void Transport::set_position(std::int64_t ticks) {
    current_tick_ = ticks >= 0 ? ticks : 0;
}

TransportPosition Transport::position() const noexcept {
    return TransportPosition{current_tick_, ppq_, tempo_bpm_};
}

void Transport::schedule(const ScheduledEvent& event) {
    event_queue_.push(event);
}

void Transport::schedule_note(
    std::int64_t tick,
    Core::MidiNote pitch,
    Core::Beat duration,
    Core::Velocity velocity
) {
    Core::NoteEvent event;
    event.pitch = pitch;
    event.start_time = Core::Beat{tick, ppq_};
    event.duration = duration;
    event.velocity = velocity;

    schedule(ScheduledEvent{tick, event});

    // Schedule note-off
    std::int64_t duration_ticks = (duration.numerator * ppq_) / duration.denominator;
    Core::NoteEvent off_event = event;
    off_event.velocity = 0;  // Note-off
    schedule(ScheduledEvent{tick + duration_ticks, off_event});
}

void Transport::clear_scheduled() {
    while (!event_queue_.empty()) {
        event_queue_.pop();
    }
}

void Transport::advance(std::int64_t ticks) {
    if (state_ != TransportState::Playing) {
        return;
    }

    std::int64_t target_tick = current_tick_ + ticks;
    dispatch_events_until(target_tick);
    current_tick_ = target_tick;
}

void Transport::process_block(std::size_t sample_count, double sample_rate) {
    if (state_ != TransportState::Playing) {
        return;
    }

    // Calculate ticks for this block
    // ticks = samples * (tempo / 60) * ppq / sample_rate
    double seconds = static_cast<double>(sample_count) / sample_rate;
    double beats = seconds * tempo_bpm_ / 60.0;
    auto ticks = static_cast<std::int64_t>(beats * static_cast<double>(ppq_));

    advance(ticks);
}

void Transport::set_note_on_callback(MidiCallback callback) {
    note_on_callback_ = std::move(callback);
}

void Transport::set_note_off_callback(MidiCallback callback) {
    note_off_callback_ = std::move(callback);
}

void Transport::dispatch_events_until(std::int64_t tick) {
    while (!event_queue_.empty() && event_queue_.top().tick <= tick) {
        const auto& scheduled = event_queue_.top();

        if (scheduled.event.velocity > 0) {
            if (note_on_callback_) {
                note_on_callback_(scheduled.event);
            }
        } else {
            if (note_off_callback_) {
                note_off_callback_(scheduled.event);
            }
        }

        event_queue_.pop();
    }
}

}  // namespace Sunny::Render
