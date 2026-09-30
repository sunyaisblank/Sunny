/**
 * @file clock_adapter.cpp
 * @brief Bounded Max adapter for Sunny's local block clock
 */

#include <cmath>
#include <sunny/max/clock_adapter.hpp>

namespace sunny::max {

sunny::core::VoidResult ClockAdapter::configure_dsp(double sample_rate,
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

sunny::core::VoidResult ClockAdapter::publish(detail::ClockCommand command) noexcept {
    if (!controls_.try_push(command, [this]() noexcept { status_.record_enqueued(); })) {
        status_.record_rejection(sunny::core::ErrorCode::RenderControlQueueFull);
        return std::unexpected(sunny::core::ErrorCode::RenderControlQueueFull);
    }
    return {};
}

sunny::core::VoidResult ClockAdapter::publish_tempo(double bpm) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (!std::isfinite(bpm) || bpm < sunny::core::Constants::TEMPO_MIN_BPM ||
        bpm > sunny::core::Constants::TEMPO_MAX_BPM) {
        status_.record_rejection(sunny::core::ErrorCode::InvalidTempo);
        return std::unexpected(sunny::core::ErrorCode::InvalidTempo);
    }
    return publish({detail::ClockCommandKind::Tempo, bpm, 0});
}

sunny::core::VoidResult ClockAdapter::publish_position(std::int64_t ticks) noexcept {
    std::scoped_lock lock{publisher_mutex_};
    if (ticks < 0) {
        status_.record_rejection(sunny::core::ErrorCode::RenderInvalidPosition);
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidPosition);
    }
    return publish({detail::ClockCommandKind::Position, 0.0, ticks});
}

sunny::core::VoidResult ClockAdapter::publish_play() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::ClockCommandKind::Play, 0.0, 0});
}

sunny::core::VoidResult ClockAdapter::publish_record() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::ClockCommandKind::Record, 0.0, 0});
}

sunny::core::VoidResult ClockAdapter::publish_pause() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::ClockCommandKind::Pause, 0.0, 0});
}

sunny::core::VoidResult ClockAdapter::publish_stop() noexcept {
    std::scoped_lock lock{publisher_mutex_};
    return publish({detail::ClockCommandKind::Stop, 0.0, 0});
}

void ClockAdapter::apply_pending_controls() noexcept {
    detail::ClockCommand command;
    for (std::size_t applied = 0; applied < MAX_CONTROL_QUEUE_CAPACITY; ++applied) {
        if (!controls_.try_pop(command)) break;
        sunny::core::VoidResult result;
        switch (command.kind) {
        case detail::ClockCommandKind::Tempo:
            result = clock_.set_tempo(command.scalar);
            break;
        case detail::ClockCommandKind::Position:
            result = clock_.set_position(command.integer);
            break;
        case detail::ClockCommandKind::Play:
            clock_.play();
            break;
        case detail::ClockCommandKind::Record:
            clock_.record();
            break;
        case detail::ClockCommandKind::Pause:
            clock_.pause();
            break;
        case detail::ClockCommandKind::Stop:
            clock_.stop();
            break;
        }
        if (result)
            status_.record_applied();
        else
            status_.record_rejection(result.error());
    }
}

sunny::core::VoidResult ClockAdapter::process(std::int64_t frame_count,
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
    auto result = clock_.process_position_block(
        *context_, frame_count, input_count, inputs, output_count, outputs);
    if (!result) status_.record_process_failure(result.error());
    return result;
}

sunny::core::VoidResult ClockAdapter::silence_output(std::int64_t frame_count,
                                                     std::int64_t output_count,
                                                     double* const* outputs) const noexcept {
    if (!context_) return std::unexpected(sunny::core::ErrorCode::RenderNotConfigured);
    return detail::silence_generator_output(static_cast<std::int64_t>(context_->maximum_frames()),
                                            frame_count,
                                            0,
                                            output_count,
                                            outputs);
}

ControlAdapterStatus ClockAdapter::status() const noexcept {
    auto result = status_.snapshot();
    result.controls_pending = controls_.pending();
    return result;
}

void ClockAdapter::clear_error() noexcept {
    status_.clear_error();
}

} // namespace sunny::max
