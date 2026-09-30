/**
 * @file modulation_adapter.cpp
 * @brief Bounded Max message-to-audio modulation adapters
 */

#include <cmath>
#include <sunny/max/modulation_adapter.hpp>

namespace sunny::max {

namespace {

[[nodiscard]] bool finite_non_negative(double value) noexcept {
    return std::isfinite(value) && value >= 0.0;
}

} // namespace

sunny::core::VoidResult LfoAdapter::configure_dsp(double sample_rate,
                                                  std::int64_t maximum_frames) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    auto candidate = sunny::render::SignalBlockContext::create(sample_rate, maximum_frames);
    if (!candidate) {
        context_.reset();
        status_.record_setup_failure(candidate.error());
        return std::unexpected(candidate.error());
    }
    bool pending_controls_compatible = true;
    controls_.for_each_pending([&](const detail::LfoCommand& command) noexcept {
        if (command.kind == detail::LfoCommandKind::Frequency &&
            command.scalar > candidate->sample_rate())
            pending_controls_compatible = false;
    });
    if (!pending_controls_compatible) {
        context_.reset();
        status_.record_setup_failure(sunny::core::ErrorCode::RenderInvalidSampleRate);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    }
    if (auto compatible = lfo_.validate_context(*candidate); !compatible) {
        context_.reset();
        status_.record_setup_failure(compatible.error());
        return std::unexpected(compatible.error());
    }
    context_ = *candidate;
    status_.record_configuration(sample_rate, maximum_frames);
    return {};
}

sunny::core::VoidResult LfoAdapter::publish(detail::LfoCommand command) noexcept {
    if (!controls_.try_push(command, [this]() noexcept { status_.record_enqueued(); })) {
        status_.record_rejection(sunny::core::ErrorCode::RenderControlQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderControlQueueFull);
    }
    return {};
}

sunny::core::VoidResult LfoAdapter::publish_frequency(double hz) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!finite_non_negative(hz)) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    if (context_ && hz > context_->sample_rate()) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidSampleRate);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSampleRate);
    }
    return publish({detail::LfoCommandKind::Frequency, hz, 0});
}

sunny::core::VoidResult LfoAdapter::publish_waveform(sunny::render::LfoWaveform waveform) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!sunny::render::Lfo::is_valid_waveform(waveform)) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::LfoCommandKind::Waveform, 0.0, static_cast<std::uint32_t>(waveform)});
}

sunny::core::VoidResult LfoAdapter::publish_phase(double phase) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!std::isfinite(phase) || phase < 0.0 || phase >= 1.0) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::LfoCommandKind::Phase, phase, 0});
}

sunny::core::VoidResult LfoAdapter::publish_seed(std::uint32_t seed) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::LfoCommandKind::Seed, 0.0, seed});
}

sunny::core::VoidResult LfoAdapter::publish_reset() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::LfoCommandKind::Reset, 0.0, 0});
}

void LfoAdapter::apply_pending_controls() noexcept {
    detail::LfoCommand command;
    for (std::size_t applied = 0; applied < MAX_CONTROL_QUEUE_CAPACITY; ++applied) {
        if (!controls_.try_pop(command)) break;
        sunny::core::VoidResult result;
        switch (command.kind) {
        case detail::LfoCommandKind::Frequency:
            result = lfo_.set_frequency(command.scalar, *context_);
            break;
        case detail::LfoCommandKind::Waveform:
            result = lfo_.set_waveform(static_cast<sunny::render::LfoWaveform>(command.integer));
            break;
        case detail::LfoCommandKind::Phase:
            result = lfo_.set_phase(command.scalar);
            break;
        case detail::LfoCommandKind::Seed:
            lfo_.set_seed(command.integer);
            break;
        case detail::LfoCommandKind::Reset:
            lfo_.reset();
            break;
        }
        if (result)
            status_.record_applied();
        else
            status_.record_rejection(result.error());
    }
}

sunny::core::VoidResult LfoAdapter::process(std::int64_t frame_count,
                                            std::int64_t input_count,
                                            double* const* inputs,
                                            std::int64_t output_count,
                                            double* const* outputs) noexcept {
    status_.record_process_call(frame_count);
    if (!context_) {
        status_.record_process_failure(sunny::core::ErrorCode::RenderNotConfigured);
        return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    }
    if (auto valid = detail::validate_generator_call(
            *context_, frame_count, input_count, output_count, outputs);
        !valid) {
        status_.record_process_failure(valid.error());
        return valid;
    }
    apply_pending_controls();
    auto result =
        lfo_.process_block(*context_, frame_count, input_count, inputs, output_count, outputs);
    if (!result) status_.record_process_failure(result.error());
    return result;
}

sunny::core::VoidResult LfoAdapter::silence_output(std::int64_t frame_count,
                                                   std::int64_t output_count,
                                                   double* const* outputs) const noexcept {
    if (!context_) return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    return detail::silence_generator_output(static_cast<std::int64_t>(context_->maximum_frames()),
                                            frame_count,
                                            0,
                                            output_count,
                                            outputs);
}

ModulationAdapterStatus LfoAdapter::status() const noexcept {
    auto result = status_.snapshot();
    result.controls_pending = controls_.pending();
    return result;
}

void LfoAdapter::clear_error() noexcept {
    status_.clear_error();
}

sunny::core::VoidResult EnvelopeAdapter::configure_dsp(double sample_rate,
                                                       std::int64_t maximum_frames) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    auto candidate = sunny::render::SignalBlockContext::create(sample_rate, maximum_frames);
    if (!candidate) {
        context_.reset();
        status_.record_setup_failure(candidate.error());
        return std::unexpected(candidate.error());
    }
    context_ = *candidate;
    status_.record_configuration(sample_rate, maximum_frames);
    return {};
}

sunny::core::VoidResult EnvelopeAdapter::publish(detail::EnvelopeCommand command) noexcept {
    if (!controls_.try_push(command, [this]() noexcept { status_.record_enqueued(); })) {
        status_.record_rejection(sunny::core::ErrorCode::RenderControlQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderControlQueueFull);
    }
    return {};
}

sunny::core::VoidResult EnvelopeAdapter::publish_attack(double seconds) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!finite_non_negative(seconds)) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::EnvelopeCommandKind::Attack, seconds});
}

sunny::core::VoidResult EnvelopeAdapter::publish_decay(double seconds) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!finite_non_negative(seconds)) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::EnvelopeCommandKind::Decay, seconds});
}

sunny::core::VoidResult EnvelopeAdapter::publish_sustain(double level) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!std::isfinite(level) || level < 0.0 || level > 1.0) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::EnvelopeCommandKind::Sustain, level});
}

sunny::core::VoidResult EnvelopeAdapter::publish_release_time(double seconds) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!finite_non_negative(seconds)) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::EnvelopeCommandKind::ReleaseTime, seconds});
}

sunny::core::VoidResult EnvelopeAdapter::publish_trigger() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::EnvelopeCommandKind::Trigger, 0.0});
}

sunny::core::VoidResult EnvelopeAdapter::publish_release() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::EnvelopeCommandKind::Release, 0.0});
}

sunny::core::VoidResult EnvelopeAdapter::publish_reset() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::EnvelopeCommandKind::Reset, 0.0});
}

void EnvelopeAdapter::apply_pending_controls() noexcept {
    detail::EnvelopeCommand command;
    for (std::size_t applied = 0; applied < MAX_CONTROL_QUEUE_CAPACITY; ++applied) {
        if (!controls_.try_pop(command)) break;
        sunny::core::VoidResult result;
        switch (command.kind) {
        case detail::EnvelopeCommandKind::Attack:
            result = envelope_.set_attack(command.scalar);
            break;
        case detail::EnvelopeCommandKind::Decay:
            result = envelope_.set_decay(command.scalar);
            break;
        case detail::EnvelopeCommandKind::Sustain:
            result = envelope_.set_sustain(command.scalar);
            break;
        case detail::EnvelopeCommandKind::ReleaseTime:
            result = envelope_.set_release(command.scalar);
            break;
        case detail::EnvelopeCommandKind::Trigger:
            envelope_.trigger();
            break;
        case detail::EnvelopeCommandKind::Release:
            envelope_.release();
            break;
        case detail::EnvelopeCommandKind::Reset:
            envelope_.reset();
            break;
        }
        if (result)
            status_.record_applied();
        else
            status_.record_rejection(result.error());
    }
}

sunny::core::VoidResult EnvelopeAdapter::process(std::int64_t frame_count,
                                                 std::int64_t input_count,
                                                 double* const* inputs,
                                                 std::int64_t output_count,
                                                 double* const* outputs) noexcept {
    status_.record_process_call(frame_count);
    if (!context_) {
        status_.record_process_failure(sunny::core::ErrorCode::RenderNotConfigured);
        return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    }
    if (auto valid = detail::validate_generator_call(
            *context_, frame_count, input_count, output_count, outputs);
        !valid) {
        status_.record_process_failure(valid.error());
        return valid;
    }
    apply_pending_controls();
    auto result =
        envelope_.process_block(*context_, frame_count, input_count, inputs, output_count, outputs);
    if (!result) status_.record_process_failure(result.error());
    return result;
}

sunny::core::VoidResult EnvelopeAdapter::silence_output(std::int64_t frame_count,
                                                        std::int64_t output_count,
                                                        double* const* outputs) const noexcept {
    if (!context_) return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    return detail::silence_generator_output(static_cast<std::int64_t>(context_->maximum_frames()),
                                            frame_count,
                                            0,
                                            output_count,
                                            outputs);
}

ModulationAdapterStatus EnvelopeAdapter::status() const noexcept {
    auto result = status_.snapshot();
    result.controls_pending = controls_.pending();
    return result;
}

void EnvelopeAdapter::clear_error() noexcept {
    status_.clear_error();
}

sunny::core::VoidResult SampleAndHoldAdapter::configure_dsp(double sample_rate,
                                                            std::int64_t maximum_frames) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    auto candidate = sunny::render::SignalBlockContext::create(sample_rate, maximum_frames);
    if (!candidate) {
        context_.reset();
        status_.record_setup_failure(candidate.error());
        return std::unexpected(candidate.error());
    }
    context_ = *candidate;
    status_.record_configuration(sample_rate, maximum_frames);
    return {};
}

sunny::core::VoidResult
SampleAndHoldAdapter::publish(detail::SampleAndHoldCommand command) noexcept {
    if (!controls_.try_push(command, [this]() noexcept { status_.record_enqueued(); })) {
        status_.record_rejection(sunny::core::ErrorCode::RenderControlQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderControlQueueFull);
    }
    return {};
}

sunny::core::VoidResult SampleAndHoldAdapter::publish_value(double value) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!std::isfinite(value) || value < -1.0 || value > 1.0) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidParameter);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidParameter);
    }
    return publish({detail::SampleAndHoldCommandKind::Value, value});
}

sunny::core::VoidResult SampleAndHoldAdapter::publish_reset() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::SampleAndHoldCommandKind::Reset, 0.0});
}

void SampleAndHoldAdapter::apply_pending_controls() noexcept {
    detail::SampleAndHoldCommand command;
    for (std::size_t applied = 0; applied < MAX_CONTROL_QUEUE_CAPACITY; ++applied) {
        if (!controls_.try_pop(command)) break;
        sunny::core::VoidResult result;
        switch (command.kind) {
        case detail::SampleAndHoldCommandKind::Value:
            result = sample_and_hold_.trigger(command.scalar);
            break;
        case detail::SampleAndHoldCommandKind::Reset:
            sample_and_hold_.reset();
            break;
        }
        if (result)
            status_.record_applied();
        else
            status_.record_rejection(result.error());
    }
}

sunny::core::VoidResult SampleAndHoldAdapter::process(std::int64_t frame_count,
                                                      std::int64_t input_count,
                                                      double* const* inputs,
                                                      std::int64_t output_count,
                                                      double* const* outputs) noexcept {
    status_.record_process_call(frame_count);
    if (!context_) {
        status_.record_process_failure(sunny::core::ErrorCode::RenderNotConfigured);
        return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    }
    if (auto valid = detail::validate_generator_call(
            *context_, frame_count, input_count, output_count, outputs);
        !valid) {
        status_.record_process_failure(valid.error());
        return valid;
    }
    apply_pending_controls();
    auto result = sample_and_hold_.process_block(
        *context_, frame_count, input_count, inputs, output_count, outputs);
    if (!result) status_.record_process_failure(result.error());
    return result;
}

sunny::core::VoidResult SampleAndHoldAdapter::silence_output(
    std::int64_t frame_count, std::int64_t output_count, double* const* outputs) const noexcept {
    if (!context_) return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    return detail::silence_generator_output(static_cast<std::int64_t>(context_->maximum_frames()),
                                            frame_count,
                                            0,
                                            output_count,
                                            outputs);
}

ModulationAdapterStatus SampleAndHoldAdapter::status() const noexcept {
    auto result = status_.snapshot();
    result.controls_pending = controls_.pending();
    return result;
}

void SampleAndHoldAdapter::clear_error() noexcept {
    status_.clear_error();
}

} // namespace sunny::max
