/**
 * @file clock_adapter.hpp
 * @brief Bounded Max message-to-audio adapter for Sunny's local block clock
 */

#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <sunny/max/control_transfer.hpp>
#include <sunny/render/block_clock.hpp>
#include <utility>

namespace sunny::max {

namespace detail {

enum class ClockCommandKind : std::uint8_t { Tempo, Position, Play, Record, Pause, Stop };

struct ClockCommand {
    ClockCommandKind kind{};
    double scalar = 0.0;
    std::int64_t integer = 0;
};

} // namespace detail

/**
 * @brief Traditional zero-input/mono-output adapter for local quarter-note position.
 *
 * This adapter is deliberately not a Live or Max transport observer. Message producers are
 * serialized into one fixed-capacity queue; the exclusive audio owner applies controls in order at
 * the next valid vector boundary, then renders Sunny's local position at each sample.
 */
class ClockAdapter {
  public:
    ClockAdapter() = default;
    explicit ClockAdapter(sunny::render::BlockClock clock) noexcept : clock_(std::move(clock)) {}
    ClockAdapter(const ClockAdapter&) = delete;
    ClockAdapter& operator=(const ClockAdapter&) = delete;
    ClockAdapter(ClockAdapter&&) = delete;
    ClockAdapter& operator=(ClockAdapter&&) = delete;

    [[nodiscard]] sunny::core::VoidResult configure_dsp(double sample_rate,
                                                        std::int64_t maximum_frames) noexcept;

    [[nodiscard]] sunny::core::VoidResult publish_tempo(double bpm) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_position(std::int64_t ticks) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_play() noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_record() noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_pause() noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_stop() noexcept;

    [[nodiscard]] sunny::core::VoidResult process(std::int64_t frame_count,
                                                  std::int64_t input_count,
                                                  double* const* inputs,
                                                  std::int64_t output_count,
                                                  double* const* outputs) noexcept;
    [[nodiscard]] sunny::core::VoidResult silence_output(std::int64_t frame_count,
                                                         std::int64_t output_count,
                                                         double* const* outputs) const noexcept;

    [[nodiscard]] ControlAdapterStatus status() const noexcept;
    void clear_error() noexcept;

  private:
    [[nodiscard]] sunny::core::VoidResult publish(detail::ClockCommand command) noexcept;
    void apply_pending_controls() noexcept;

    sunny::render::BlockClock clock_;
    std::optional<sunny::render::SignalBlockContext> context_;
    detail::SerializedSpscQueue<detail::ClockCommand, MAX_CONTROL_QUEUE_CAPACITY> controls_;
    mutable std::mutex publisher_mutex_;
    detail::AdapterStatusState status_;
};

} // namespace sunny::max
