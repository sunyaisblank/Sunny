/**
 * @file block_clock.cpp
 * @brief Bounded callback-free signal-block clock
 */

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
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

namespace {

// Rates up to 2^40 keep the recovery of a remainder from its long double quotient exact (see
// plan_block); the products with the block length are checked for overflow there.
constexpr long double EXACT_RATE_LIMIT = 0x1p40L;

// The value as an integer when it is integral, positive and at most EXACT_RATE_LIMIT.
std::optional<std::int64_t> exact_rate(double value) noexcept {
    const auto wide = static_cast<long double>(value);
    if (!(wide > 0.0L) || wide > EXACT_RATE_LIMIT || std::floor(wide) != wide) return std::nullopt;
    return static_cast<std::int64_t>(wide);
}

} // namespace

std::size_t BlockClock::PlannedBlockAdvance::sample_offset(std::int64_t tick) const noexcept {
    if (sample_count == 0 || tick <= tick_before) return 0;
    if (exact) {
        // Sample k starts at tick_before + (start_remainder + k * P) / Q, so the tick lies in
        // sample floor((d * Q - start_remainder) / P). `contains` bounds d * Q by the block's
        // total numerator, which plan_block checked against overflow.
        const std::int64_t distance = tick - tick_before;
        const std::int64_t numerator =
            distance * exact->samples_per_minute - exact->start_remainder;
        return static_cast<std::size_t>(numerator / exact->ticks_per_minute);
    }
    const long double position =
        (static_cast<long double>(tick - tick_before) - fractional_before) / ticks_per_sample;
    if (!(position > 0.0L)) return 0;
    const long double nearest = std::round(position);
    const long double sample = std::fabs(position - nearest) <= BLOCK_SAMPLE_SNAP_TOLERANCE
                                   ? nearest
                                   : std::floor(position);
    // A contained tick lies before sample_count; only a rate above 2^20 ticks per sample could
    // snap one onto the right edge, and it stays in the last sample.
    const auto last = static_cast<long double>(sample_count - 1);
    return static_cast<std::size_t>(sample < last ? sample : last);
}

sunny::core::Result<BlockClock::PlannedBlockAdvance>
BlockClock::plan_block(std::size_t sample_count, double sample_rate) const noexcept {
    if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    const auto before = current_tick_;
    if (!is_running())
        return PlannedBlockAdvance{before,
                                   before,
                                   fractional_tick_,
                                   fractional_tick_,
                                   0.0L,
                                   sample_count,
                                   std::nullopt,
                                   false};

    const long double ticks_per_sample = static_cast<long double>(tempo_bpm_) *
                                         static_cast<long double>(ppq_) /
                                         (60.0L * static_cast<long double>(sample_rate));

    // The products are formed in double precision, so a decimal tempo whose product with the PPQ
    // rounds to an integer (100.1 x 480) takes the exact path.
    const auto ticks_per_minute = exact_rate(tempo_bpm_ * static_cast<double>(ppq_));
    const auto samples_per_minute = exact_rate(sample_rate * 60.0);
    if (ticks_per_minute && samples_per_minute) {
        const std::int64_t per_tick = *samples_per_minute;
        const std::int64_t per_sample = *ticks_per_minute;
        // The retained fraction is remainder / per_tick rounded to long double, whose relative
        // error is at most 2^-52; with per_tick <= 2^40 the product below lies within 2^-11 of
        // the remainder, so rounding recovers it exactly. After a rate change it is the nearest
        // multiple of 1 / per_tick ticks.
        auto remainder = static_cast<std::int64_t>(
            std::llround(fractional_tick_ * static_cast<long double>(per_tick)));
        remainder = std::clamp<std::int64_t>(remainder, 0, per_tick - 1);
        if (sample_count > static_cast<std::uint64_t>(
                               (std::numeric_limits<std::int64_t>::max() - remainder) / per_sample))
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        const std::int64_t total = remainder + static_cast<std::int64_t>(sample_count) * per_sample;
        const std::int64_t whole_ticks = total / per_tick;
        if (current_tick_ > std::numeric_limits<std::int64_t>::max() - whole_ticks)
            return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
        return PlannedBlockAdvance{
            before,
            current_tick_ + whole_ticks,
            fractional_tick_,
            static_cast<long double>(total % per_tick) / static_cast<long double>(per_tick),
            ticks_per_sample,
            sample_count,
            ExactTickRate{per_sample, per_tick, remainder},
            true,
        };
    }

    const long double tick_delta = static_cast<long double>(sample_count) * ticks_per_sample;
    long double accumulated = fractional_tick_ + tick_delta;
    // 2^63 is exact even when long double is double-width; INT64_MAX may round up to it.
    if (!std::isfinite(accumulated) || accumulated >= 0x1p63L)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    // An endpoint within the tolerance of a tick boundary is that boundary, so a tick there is
    // left for the next block, as sample_offset would place it at the right edge.
    const long double nearest = std::round(accumulated);
    if (std::fabs(accumulated - nearest) <= BLOCK_SAMPLE_SNAP_TOLERANCE * ticks_per_sample)
        accumulated = nearest;

    const auto whole_ticks = static_cast<std::int64_t>(std::floor(accumulated));
    if (current_tick_ > std::numeric_limits<std::int64_t>::max() - whole_ticks)
        return std::unexpected(sunny::core::ErrorCode::ArithmeticOverflow);
    return PlannedBlockAdvance{
        before,
        current_tick_ + whole_ticks,
        fractional_tick_,
        accumulated - static_cast<long double>(whole_ticks),
        ticks_per_sample,
        sample_count,
        std::nullopt,
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
