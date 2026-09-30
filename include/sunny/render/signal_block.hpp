/**
 * @file signal_block.hpp
 * @brief Validated host-neutral signal-vector setup
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <sunny/core/types/music_types.hpp>

namespace sunny::render {

/**
 * @brief Validated signal-vector setup shared by host-neutral DSP processors.
 *
 * A Max/MSP adapter can create this during its non-performing `dsp64` setup
 * from the host-provided sample rate and maximum vector size, then pass only
 * vectors no larger than `maximum_frames()` from `perform64`.
 */
class SignalBlockContext {
  public:
    [[nodiscard]] static sunny::core::Result<SignalBlockContext>
    create(double sample_rate, std::int64_t maximum_frames) noexcept;

    [[nodiscard]] sunny::core::Result<std::size_t>
    validate_frame_count(std::int64_t actual_frames) const noexcept;

    [[nodiscard]] double sample_rate() const noexcept { return sample_rate_; }
    [[nodiscard]] std::size_t maximum_frames() const noexcept { return maximum_frames_; }

  private:
    SignalBlockContext(double sample_rate, std::size_t maximum_frames) noexcept
        : sample_rate_(sample_rate), maximum_frames_(maximum_frames) {}

    double sample_rate_;
    std::size_t maximum_frames_;
};

} // namespace sunny::render
