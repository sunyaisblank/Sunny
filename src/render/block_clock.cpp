/**
 * @file block_clock.cpp
 * @brief Bounded callback-free signal-block clock
 */

#include <cmath>
#include <limits>
#include <sunny/render/block_clock.hpp>

namespace sunny::render {

sunny::core::Result<BlockClock> BlockClock::create(std::int64_t ppq) noexcept {
    if (ppq < 1 || ppq > 65'535) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPPQ);
    return BlockClock{ppq};
}

void BlockClock::play() noexcept {
    state_ = TransportState::Playing;
}

void BlockClock::record() noexcept {
    state_ = TransportState::Recording;
}

void BlockClock::stop() noexcept {
    state_ = TransportState::Stopped;
    current_tick_ = 0;
    fractional_tick_ = 0.0L;
}

void BlockClock::pause() noexcept {
    if (is_running()) state_ = TransportState::Paused;
}

sunny::core::VoidResult BlockClock::set_tempo(double bpm) noexcept {
    if (!std::isfinite(bpm) || bpm < sunny::core::Constants::TEMPO_MIN_BPM ||
        bpm > sunny::core::Constants::TEMPO_MAX_BPM)
        return std::unexpected(sunny::core::ErrorCode::InvalidTempo);
    tempo_bpm_ = bpm;
    return {};
}

sunny::core::VoidResult BlockClock::set_position(std::int64_t ticks) noexcept {
    if (ticks < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    current_tick_ = ticks;
    fractional_tick_ = 0.0L;
    return {};
}

TransportPosition BlockClock::position() const noexcept {
    return TransportPosition{current_tick_, ppq_, tempo_bpm_};
}

sunny::core::Result<BlockClockAdvance> BlockClock::advance(std::int64_t ticks) noexcept {
    if (ticks < 0) return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    const auto before = current_tick_;
    if (!is_running()) return BlockClockAdvance{before, before};
    if (current_tick_ > std::numeric_limits<std::int64_t>::max() - ticks)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    current_tick_ += ticks;
    return BlockClockAdvance{before, current_tick_};
}

sunny::core::Result<BlockClockAdvance> BlockClock::process_block(std::size_t sample_count,
                                                                 double sample_rate) noexcept {
    auto plan = plan_block(sample_count, sample_rate);
    if (!plan) return std::unexpected(plan.error());
    commit_block(*plan);
    return BlockClockAdvance{plan->tick_before, plan->tick_after};
}

sunny::core::Result<BlockClock::PlannedBlockAdvance>
BlockClock::plan_block(std::size_t sample_count, double sample_rate) const noexcept {
    if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    const auto before = current_tick_;
    if (!is_running()) return PlannedBlockAdvance{before, before, fractional_tick_, 0.0L, false};

    const long double ticks_per_sample = static_cast<long double>(tempo_bpm_) *
                                         static_cast<long double>(ppq_) /
                                         (60.0L * static_cast<long double>(sample_rate));
    const long double tick_delta = static_cast<long double>(sample_count) * ticks_per_sample;
    const long double accumulated = fractional_tick_ + tick_delta;
    // 2^63 is exact even when long double is double-width; INT64_MAX may round up to it.
    if (!std::isfinite(accumulated) || accumulated >= 0x1p63L)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);

    const auto whole_ticks = static_cast<std::int64_t>(std::floor(accumulated));
    if (current_tick_ > std::numeric_limits<std::int64_t>::max() - whole_ticks)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    return PlannedBlockAdvance{
        before,
        current_tick_ + whole_ticks,
        accumulated - static_cast<long double>(whole_ticks),
        ticks_per_sample,
        true,
    };
}

void BlockClock::commit_block(const PlannedBlockAdvance& plan) noexcept {
    current_tick_ = plan.tick_after;
    fractional_tick_ = plan.fractional_after;
}

sunny::core::Result<BlockClockAdvance>
BlockClock::process_block(const SignalBlockContext& context, std::int64_t sample_count) noexcept {
    const auto frames = context.validate_frame_count(sample_count);
    if (!frames) return std::unexpected(frames.error());
    return process_block(*frames, context.sample_rate());
}

sunny::core::VoidResult
BlockClock::process_position_block(const SignalBlockContext& context,
                                   std::span<double> quarter_note_positions) noexcept {
    if (quarter_note_positions.size() > context.maximum_frames())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);

    auto plan = plan_block(quarter_note_positions.size(), context.sample_rate());
    if (!plan) return std::unexpected(plan.error());

    const long double start = static_cast<long double>(current_tick_) + fractional_tick_;
    const long double ppq = static_cast<long double>(ppq_);
    if (plan->running) {
        for (std::size_t index = 0; index < quarter_note_positions.size(); ++index) {
            const long double tick_position =
                start + static_cast<long double>(index) * plan->ticks_per_sample;
            quarter_note_positions[index] = static_cast<double>(tick_position / ppq);
        }
    } else {
        const double position = static_cast<double>(start / ppq);
        for (double& sample : quarter_note_positions)
            sample = position;
    }

    commit_block(*plan);
    return {};
}

sunny::core::VoidResult BlockClock::process_position_block(const SignalBlockContext& context,
                                                           std::int64_t frame_count,
                                                           std::int64_t input_count,
                                                           double* const* inputs,
                                                           std::int64_t output_count,
                                                           double* const* outputs) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (input_count != signal_input_count)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    if (output_count != signal_output_count)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    (void)inputs;
    if (*frames == 0) return process_position_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_position_block(context, std::span<double>{outputs[0], *frames});
}

} // namespace sunny::render
