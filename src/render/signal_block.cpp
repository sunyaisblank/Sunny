/**
 * @file signal_block.cpp
 * @brief Validated host-neutral signal-vector setup
 */

#include <cmath>
#include <sunny/render/signal_block.hpp>
#include <utility>

namespace sunny::render {

sunny::core::Result<SignalBlockContext>
SignalBlockContext::create(double sample_rate, std::int64_t maximum_frames) noexcept {
    if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    if (maximum_frames <= 0 || !std::in_range<std::size_t>(maximum_frames))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);
    return SignalBlockContext{sample_rate, static_cast<std::size_t>(maximum_frames)};
}

sunny::core::Result<std::size_t>
SignalBlockContext::validate_frame_count(std::int64_t actual_frames) const noexcept {
    if (actual_frames < 0 || !std::in_range<std::size_t>(actual_frames))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);
    const auto frames = static_cast<std::size_t>(actual_frames);
    if (frames > maximum_frames_)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);
    return frames;
}

} // namespace sunny::render
