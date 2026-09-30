/**
 * @file block_clock.hpp
 * @brief Bounded callback-free signal-block clock
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <sunny/core/types/beat.hpp>
#include <sunny/core/types/music_types.hpp>
#include <sunny/render/signal_block.hpp>

namespace sunny::render {

class BlockEventScheduler;

/// Pulses per quarter note used by the default clock.
constexpr std::int64_t DEFAULT_PPQ = 480;

/// Transport/clock state.
enum class TransportState { Stopped, Playing, Paused, Recording };

/// Transport position in ticks.
struct TransportPosition {
    std::int64_t ticks; ///< Current position in ticks
    std::int64_t ppq;   ///< Pulses per quarter note
    double tempo_bpm;   ///< Current tempo

    /// Convert to Sunny's canonical whole-note Beat coordinate.
    [[nodiscard]] constexpr sunny::core::Beat to_beats() const noexcept {
        return sunny::core::Beat::normalise(ticks, 4 * ppq);
    }

    /// Convert to MIDI/Live quarter-note beat count.
    [[nodiscard]] constexpr double to_quarter_notes() const noexcept {
        return static_cast<double>(ticks) / static_cast<double>(ppq);
    }

    /// Convert to seconds at the current constant tempo.
    [[nodiscard]] constexpr double to_seconds() const noexcept {
        return to_quarter_notes() * 60.0 / tempo_bpm;
    }
};

/// Integer tick endpoints produced by one admitted advancement.
struct BlockClockAdvance {
    std::int64_t tick_before;
    std::int64_t tick_after;

    [[nodiscard]] constexpr std::int64_t ticks_advanced() const noexcept {
        return tick_after - tick_before;
    }
};

/**
 * @brief Exclusive-owner O(1) block-to-tick recurrence without event storage or callbacks.
 *
 * The context overload validates the signed host count before conversion. Admitted calls perform
 * no allocation, locking, I/O, callback dispatch, or unbounded iteration. The class has mutable
 * state and no internal synchronisation; every operation on one instance requires one owner.
 */
class BlockClock {
  public:
    static constexpr std::int64_t signal_input_count = 0;
    static constexpr std::int64_t signal_output_count = 1;

    BlockClock() = default;
    [[nodiscard]] static sunny::core::Result<BlockClock> create(std::int64_t ppq) noexcept;

    void play() noexcept;
    void record() noexcept;
    void stop() noexcept;
    void pause() noexcept;
    [[nodiscard]] sunny::core::VoidResult set_tempo(double bpm) noexcept;
    [[nodiscard]] sunny::core::VoidResult set_position(std::int64_t ticks) noexcept;

    [[nodiscard]] TransportState state() const noexcept { return state_; }
    [[nodiscard]] TransportPosition position() const noexcept;
    [[nodiscard]] double tempo() const noexcept { return tempo_bpm_; }
    [[nodiscard]] std::int64_t ppq() const noexcept { return ppq_; }
    [[nodiscard]] bool is_playing() const noexcept { return state_ == TransportState::Playing; }
    [[nodiscard]] bool is_running() const noexcept {
        return state_ == TransportState::Playing || state_ == TransportState::Recording;
    }

    [[nodiscard]] sunny::core::Result<BlockClockAdvance> advance(std::int64_t ticks) noexcept;
    [[nodiscard]] sunny::core::Result<BlockClockAdvance> process_block(std::size_t sample_count,
                                                                       double sample_rate) noexcept;
    [[nodiscard]] sunny::core::Result<BlockClockAdvance>
    process_block(const SignalBlockContext& context, std::int64_t sample_count) noexcept;

    /**
     * Render local quarter-note position at the start of every admitted sample, then commit the
     * same endpoint as `process_block`. Stopped/paused clocks emit a constant retained position.
     */
    [[nodiscard]] sunny::core::VoidResult
    process_position_block(const SignalBlockContext& context,
                           std::span<double> quarter_note_positions) noexcept;

    /**
     * Validate the complete traditional Max generator topology before selecting an output channel
     * or changing clock/output state. The input array is never dereferenced.
     */
    [[nodiscard]] sunny::core::VoidResult process_position_block(const SignalBlockContext& context,
                                                                 std::int64_t frame_count,
                                                                 std::int64_t input_count,
                                                                 double* const* inputs,
                                                                 std::int64_t output_count,
                                                                 double* const* outputs) noexcept;

  private:
    friend class BlockEventScheduler;

    struct PlannedBlockAdvance {
        std::int64_t tick_before;
        std::int64_t tick_after;
        long double fractional_after;
        long double ticks_per_sample;
        bool running;
    };

    TransportState state_{TransportState::Stopped};
    std::int64_t current_tick_{0};
    std::int64_t ppq_{DEFAULT_PPQ};
    double tempo_bpm_{120.0};
    long double fractional_tick_{0.0L};

    explicit BlockClock(std::int64_t ppq) noexcept : ppq_(ppq) {}
    [[nodiscard]] sunny::core::Result<PlannedBlockAdvance>
    plan_block(std::size_t sample_count, double sample_rate) const noexcept;
    void commit_block(const PlannedBlockAdvance& plan) noexcept;
};

} // namespace sunny::render
