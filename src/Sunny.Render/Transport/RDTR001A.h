/**
 * @file RDTR001A.h
 * @brief MIDI Transport and Scheduling
 *
 * Component: RDTR001A
 * Domain: RD (Render) | Category: TR (Transport)
 *
 * Provides sample-accurate MIDI event scheduling with:
 * - Tick-based timing (PPQ resolution)
 * - Transport state management (play, stop, pause)
 * - Tempo-synced scheduling
 *
 * Invariants:
 * - All timing uses exact integer arithmetic (no floating-point drift)
 * - Events are processed in tick order
 * - Transport state transitions are atomic
 */

#pragma once

#include "Tensor/TNTP001A.h"
#include "Tensor/TNBT001A.h"
#include "Tensor/TNEV001A.h"

#include <cstdint>
#include <functional>
#include <queue>
#include <vector>

namespace Sunny::Render {

/// Pulses per quarter note (standard MIDI resolution)
constexpr std::int64_t DEFAULT_PPQ = 480;

/// Transport state
enum class TransportState {
    Stopped,
    Playing,
    Paused,
    Recording
};

/// Transport position in ticks
struct TransportPosition {
    std::int64_t ticks;          ///< Current position in ticks
    std::int64_t ppq;            ///< Pulses per quarter note
    double tempo_bpm;            ///< Current tempo

    /// Convert to beats
    [[nodiscard]] constexpr Core::Beat to_beats() const noexcept {
        return Core::Beat{ticks, ppq};
    }

    /// Convert to seconds at current tempo
    [[nodiscard]] constexpr double to_seconds() const noexcept {
        double beats = static_cast<double>(ticks) / static_cast<double>(ppq);
        return beats * 60.0 / tempo_bpm;
    }
};

/// Scheduled MIDI event
struct ScheduledEvent {
    std::int64_t tick;           ///< Scheduled tick
    Core::NoteEvent event;       ///< The note event

    /// Comparison for priority queue (earlier events first)
    [[nodiscard]] constexpr bool operator>(const ScheduledEvent& other) const noexcept {
        return tick > other.tick;
    }
};

/// MIDI event callback type
using MidiCallback = std::function<void(const Core::NoteEvent&)>;

/**
 * @brief Transport scheduler for sample-accurate MIDI timing
 *
 * Manages a queue of scheduled events and dispatches them
 * at the correct tick positions.
 */
class Transport {
public:
    explicit Transport(std::int64_t ppq = DEFAULT_PPQ);

    // Transport control
    void play();
    void stop();
    void pause();
    void set_tempo(double bpm);
    void set_position(std::int64_t ticks);

    // State queries
    [[nodiscard]] TransportState state() const noexcept { return state_; }
    [[nodiscard]] TransportPosition position() const noexcept;
    [[nodiscard]] double tempo() const noexcept { return tempo_bpm_; }
    [[nodiscard]] bool is_playing() const noexcept {
        return state_ == TransportState::Playing;
    }

    // Event scheduling
    void schedule(const ScheduledEvent& event);
    void schedule_note(std::int64_t tick, Core::MidiNote pitch,
                       Core::Beat duration, Core::Velocity velocity = 100);
    void clear_scheduled();

    // Processing
    void advance(std::int64_t ticks);
    void process_block(std::size_t sample_count, double sample_rate);

    // Callbacks
    void set_note_on_callback(MidiCallback callback);
    void set_note_off_callback(MidiCallback callback);

private:
    TransportState state_{TransportState::Stopped};
    std::int64_t current_tick_{0};
    std::int64_t ppq_;
    double tempo_bpm_{120.0};

    std::priority_queue<ScheduledEvent,
                        std::vector<ScheduledEvent>,
                        std::greater<ScheduledEvent>> event_queue_;

    MidiCallback note_on_callback_;
    MidiCallback note_off_callback_;

    void dispatch_events_until(std::int64_t tick);
};

}  // namespace Sunny::Render
