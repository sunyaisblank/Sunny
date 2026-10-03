/**
 * @file modulation.cpp
 * @brief Modulation source implementations
 *
 */

#include <algorithm>
#include <cmath>
#include <numbers>
#include <sunny/render/modulation.hpp>

namespace sunny::render {

namespace {

[[nodiscard]] bool finite_non_negative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

[[nodiscard]] double next_random(std::mt19937& rng) noexcept {
    const auto sample = static_cast<double>(rng());
    const auto maximum = static_cast<double>(std::mt19937::max());
    return 2.0 * (sample / maximum) - 1.0;
}

} // namespace

Lfo::Lfo() {
    reset_random_stream();
}

sunny::core::VoidResult Lfo::set_frequency(double hz) {
    if (!finite_non_negative(hz))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    frequency_ = hz;
    return {};
}

sunny::core::VoidResult Lfo::set_frequency(double hz, const SignalBlockContext& context) noexcept {
    if (!finite_non_negative(hz))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    if (hz > context.sample_rate())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    frequency_ = hz;
    return {};
}

sunny::core::VoidResult Lfo::set_waveform(LfoWaveform waveform) noexcept {
    if (!is_valid_waveform(waveform))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    waveform_ = waveform;
    return {};
}

sunny::core::VoidResult Lfo::set_phase(double phase) {
    if (!std::isfinite(phase) || phase < 0.0 || phase >= 1.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    phase_ = phase;
    return {};
}

void Lfo::set_seed(std::uint32_t seed) {
    seed_ = seed;
    reset();
}

void Lfo::reset_random_stream() {
    rng_.seed(seed_);
    last_random_ = next_random(rng_);
}

void Lfo::reset() {
    phase_ = 0.0;
    current_value_ = 0.0;
    reset_random_stream();
}

sunny::core::VoidResult Lfo::validate_context(const SignalBlockContext& context) const noexcept {
    if (frequency_ > context.sample_rate())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    return {};
}

sunny::core::Result<double> Lfo::process(double sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate <= 0.0 || frequency_ > sample_rate)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);

    return process_unchecked(sample_rate);
}

sunny::core::VoidResult Lfo::process_block(const SignalBlockContext& context,
                                           std::span<double> output) noexcept {
    if (output.size() > context.maximum_frames())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);
    if (auto compatible = validate_context(context); !compatible)
        return std::unexpected(compatible.error());

    for (double& sample : output)
        sample = process_unchecked(context.sample_rate());
    return {};
}

sunny::core::VoidResult Lfo::process_block(const SignalBlockContext& context,
                                           std::int64_t frame_count,
                                           double* output) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (output == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{output, *frames});
}

sunny::core::VoidResult Lfo::process_block(const SignalBlockContext& context,
                                           std::int64_t frame_count,
                                           std::int64_t output_count,
                                           double* const* outputs) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (output_count != signal_output_count)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{outputs[0], *frames});
}

sunny::core::VoidResult Lfo::process_block(const SignalBlockContext& context,
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
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{outputs[0], *frames});
}

double Lfo::process_unchecked(double sample_rate) noexcept {

    // Render the current phase. This makes phase zero observable on the first
    // sample and gives reset a single, deterministic meaning.
    switch (waveform_) {
    case LfoWaveform::Sine:
        current_value_ = std::sin(phase_ * 2.0 * std::numbers::pi);
        break;

    case LfoWaveform::Triangle:
        if (phase_ < 0.25) {
            current_value_ = phase_ * 4.0;
        } else if (phase_ < 0.75) {
            current_value_ = 1.0 - (phase_ - 0.25) * 4.0;
        } else {
            current_value_ = -1.0 + (phase_ - 0.75) * 4.0;
        }
        break;

    case LfoWaveform::Saw:
        current_value_ = 2.0 * phase_ - 1.0;
        break;

    case LfoWaveform::Square:
        current_value_ = phase_ < 0.5 ? 1.0 : -1.0;
        break;

    case LfoWaveform::Random:
        current_value_ = last_random_;
        break;
    }

    const double phase_increment = frequency_ / sample_rate;
    phase_ += phase_increment;
    if (phase_ >= 1.0) {
        phase_ -= 1.0;
        if (waveform_ == LfoWaveform::Random) last_random_ = next_random(rng_);
    }

    return current_value_;
}

sunny::core::VoidResult Envelope::set_attack(double seconds) {
    if (!finite_non_negative(seconds))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    attack_ = seconds;
    return {};
}

sunny::core::VoidResult Envelope::set_decay(double seconds) {
    if (!finite_non_negative(seconds))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    decay_ = seconds;
    return {};
}

sunny::core::VoidResult Envelope::set_sustain(double level) {
    if (!std::isfinite(level) || level < 0.0 || level > 1.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    sustain_ = level;
    return {};
}

sunny::core::VoidResult Envelope::set_release(double seconds) {
    if (!finite_non_negative(seconds))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    release_ = seconds;
    return {};
}

void Envelope::enter_stage(EnvelopeState state) noexcept {
    state_ = state;
    stage_samples_ = 0;
    stage_length_ = 0.0;
    stage_completed_ = 0.0;
}

void Envelope::trigger() {
    attack_start_value_ = current_value_;
    enter_stage(EnvelopeState::Attack);
}

void Envelope::release() {
    if (state_ != EnvelopeState::Idle) {
        release_start_value_ = current_value_;
        enter_stage(EnvelopeState::Release);
    }
}

void Envelope::reset() {
    enter_stage(EnvelopeState::Idle);
    current_value_ = 0.0;
    attack_start_value_ = 0.0;
    release_start_value_ = 0.0;
}

sunny::core::Result<double> Envelope::process(double sample_rate) {
    if (!std::isfinite(sample_rate) || sample_rate <= 0.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);

    return process_unchecked(sample_rate);
}

sunny::core::VoidResult Envelope::process_block(const SignalBlockContext& context,
                                                std::span<double> output) noexcept {
    if (output.size() > context.maximum_frames())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);

    for (double& sample : output)
        sample = process_unchecked(context.sample_rate());
    return {};
}

sunny::core::VoidResult Envelope::process_block(const SignalBlockContext& context,
                                                std::int64_t frame_count,
                                                double* output) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (output == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{output, *frames});
}

sunny::core::VoidResult Envelope::process_block(const SignalBlockContext& context,
                                                std::int64_t frame_count,
                                                std::int64_t output_count,
                                                double* const* outputs) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (output_count != signal_output_count)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{outputs[0], *frames});
}

sunny::core::VoidResult Envelope::process_block(const SignalBlockContext& context,
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
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{outputs[0], *frames});
}

namespace {

// Snap a stage length in samples to the nearest integer within ENVELOPE_STAGE_SNAP_TOLERANCE.
[[nodiscard]] double snap_stage_length(double samples) noexcept {
    const double nearest = std::round(samples);
    return std::fabs(samples - nearest) <=
                   ENVELOPE_STAGE_SNAP_TOLERANCE * std::max(1.0, std::fabs(nearest))
               ? nearest
               : samples;
}

} // namespace

double Envelope::advance_stage(double seconds, double sample_rate) noexcept {
    const double length = snap_stage_length(seconds * sample_rate);
    if (stage_samples_ == 0) {
        stage_length_ = length;
    } else if (length != stage_length_) {
        // Keep the completed fraction; the rest of the stage runs at the new length.
        stage_completed_ += static_cast<double>(stage_samples_) / stage_length_;
        stage_samples_ = 0;
        stage_length_ = length;
    }
    ++stage_samples_;
    const double remaining = snap_stage_length((1.0 - stage_completed_) * length);
    if (static_cast<double>(stage_samples_) >= remaining) return 1.0;
    return stage_completed_ + static_cast<double>(stage_samples_) / length;
}

double Envelope::process_unchecked(double sample_rate) noexcept {
    switch (state_) {
    case EnvelopeState::Idle:
        current_value_ = 0.0;
        break;

    case EnvelopeState::Attack: {
        const double position = advance_stage(attack_, sample_rate);
        if (position >= 1.0) {
            current_value_ = 1.0;
            enter_stage(EnvelopeState::Decay);
        } else {
            current_value_ = attack_start_value_ + (1.0 - attack_start_value_) * position;
        }
        break;
    }

    case EnvelopeState::Decay: {
        const double position = advance_stage(decay_, sample_rate);
        if (position >= 1.0) {
            current_value_ = sustain_;
            enter_stage(EnvelopeState::Sustain);
        } else {
            current_value_ = 1.0 - (1.0 - sustain_) * position;
        }
        break;
    }

    case EnvelopeState::Sustain:
        current_value_ = sustain_;
        break;

    case EnvelopeState::Release: {
        const double position = advance_stage(release_, sample_rate);
        if (position >= 1.0) {
            current_value_ = 0.0;
            enter_stage(EnvelopeState::Idle);
        } else {
            current_value_ = release_start_value_ * (1.0 - position);
        }
        break;
    }
    }
    return current_value_;
}

sunny::core::VoidResult SampleAndHold::trigger(double input) {
    if (!std::isfinite(input) || input < -1.0 || input > 1.0)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    held_value_ = input;
    return {};
}

sunny::core::VoidResult SampleAndHold::process_block(const SignalBlockContext& context,
                                                     std::span<double> output) const noexcept {
    if (output.size() > context.maximum_frames())
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidBlockSize);
    std::fill(output.begin(), output.end(), held_value_);
    return {};
}

sunny::core::VoidResult SampleAndHold::process_block(const SignalBlockContext& context,
                                                     std::int64_t frame_count,
                                                     double* output) const noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (output == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{output, *frames});
}

sunny::core::VoidResult SampleAndHold::process_block(const SignalBlockContext& context,
                                                     std::int64_t frame_count,
                                                     std::int64_t input_count,
                                                     double* const* inputs,
                                                     std::int64_t output_count,
                                                     double* const* outputs) const noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (input_count != signal_input_count || output_count != signal_output_count)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    (void)inputs;
    if (*frames == 0) return process_block(context, std::span<double>{});
    if (outputs == nullptr || outputs[0] == nullptr)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return process_block(context, std::span<double>{outputs[0], *frames});
}

} // namespace sunny::render
