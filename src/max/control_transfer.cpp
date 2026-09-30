/**
 * @file control_transfer.cpp
 * @brief Shared bounded Max adapter evidence state
 */

#include <algorithm>
#include <bit>
#include <limits>
#include <sunny/max/control_transfer.hpp>

namespace sunny::max::detail {

sunny::core::VoidResult validate_generator_call(const sunny::render::SignalBlockContext& context,
                                                std::int64_t frame_count,
                                                std::int64_t input_count,
                                                std::int64_t output_count,
                                                double* const* outputs) noexcept {
    const auto frames = context.validate_frame_count(frame_count);
    if (!frames) return std::unexpected(frames.error());
    if (input_count != 0 || output_count != 1)
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidChannelCount);
    if (*frames != 0 && (outputs == nullptr || outputs[0] == nullptr))
        return std::unexpected(sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    return {};
}

sunny::core::VoidResult silence_generator_output(std::int64_t maximum_frames,
                                                 std::int64_t frame_count,
                                                 std::int64_t input_count,
                                                 std::int64_t output_count,
                                                 double* const* outputs) noexcept {
    const auto context = sunny::render::SignalBlockContext::create(1.0, maximum_frames);
    if (!context) return std::unexpected(context.error());
    if (auto valid =
            validate_generator_call(*context, frame_count, input_count, output_count, outputs);
        !valid)
        return valid;
    if (frame_count != 0) std::fill_n(outputs[0], static_cast<std::size_t>(frame_count), 0.0);
    return {};
}

void AdapterStatusState::record_enqueued() noexcept {
    controls_enqueued_.fetch_add(1, std::memory_order_release);
}

void AdapterStatusState::record_applied() noexcept {
    controls_applied_.fetch_add(1, std::memory_order_release);
}

void AdapterStatusState::record_configuration(double sample_rate,
                                              std::int64_t maximum_frames) noexcept {
    configuration_generation_.fetch_add(1, std::memory_order_acq_rel);
    sample_rate_bits_.store(std::bit_cast<std::uint64_t>(sample_rate), std::memory_order_relaxed);
    maximum_frames_.store(maximum_frames, std::memory_order_relaxed);
    configured_.store(true, std::memory_order_relaxed);
    configuration_generation_.fetch_add(1, std::memory_order_release);
}

void AdapterStatusState::record_setup_failure(sunny::core::ErrorCode error) noexcept {
    configuration_generation_.fetch_add(1, std::memory_order_acq_rel);
    configured_.store(false, std::memory_order_relaxed);
    sample_rate_bits_.store(0, std::memory_order_relaxed);
    maximum_frames_.store(0, std::memory_order_relaxed);
    configuration_generation_.fetch_add(1, std::memory_order_release);
    setup_failures_.fetch_add(1, std::memory_order_release);
    last_error_.store(static_cast<int>(error), std::memory_order_release);
}

void AdapterStatusState::record_rejection(sunny::core::ErrorCode error) noexcept {
    controls_rejected_.fetch_add(1, std::memory_order_release);
    last_error_.store(static_cast<int>(error), std::memory_order_release);
}

void AdapterStatusState::record_process_call(std::int64_t frame_count) noexcept {
    last_frame_count_.store(frame_count, std::memory_order_relaxed);
    auto calls = process_calls_.load(std::memory_order_relaxed);
    while (calls != std::numeric_limits<std::int64_t>::max() &&
           !process_calls_.compare_exchange_weak(
               calls, calls + 1, std::memory_order_release, std::memory_order_relaxed)) {}
}

void AdapterStatusState::record_process_failure(sunny::core::ErrorCode error) noexcept {
    process_failures_.fetch_add(1, std::memory_order_release);
    last_error_.store(static_cast<int>(error), std::memory_order_release);
}

void AdapterStatusState::clear_error() noexcept {
    last_error_.store(static_cast<int>(sunny::core::ErrorCode::Ok), std::memory_order_release);
}

ControlAdapterStatus AdapterStatusState::snapshot() const noexcept {
    bool configured = false;
    std::uint64_t sample_rate_bits = 0;
    std::int64_t maximum_frames = 0;
    for (;;) {
        const auto generation_before = configuration_generation_.load(std::memory_order_acquire);
        if ((generation_before & 1U) != 0U) continue;
        configured = configured_.load(std::memory_order_relaxed);
        sample_rate_bits = sample_rate_bits_.load(std::memory_order_relaxed);
        maximum_frames = maximum_frames_.load(std::memory_order_relaxed);
        const auto generation_after = configuration_generation_.load(std::memory_order_acquire);
        if (generation_before == generation_after) break;
    }

    // Read the audio-side count before its causal producer-side count. An observed application
    // synchronizes through queue publication, so the following enqueued read cannot precede that
    // command's admission.
    const auto controls_applied = controls_applied_.load(std::memory_order_acquire);
    const auto controls_enqueued = controls_enqueued_.load(std::memory_order_acquire);
    return {
        .configured = configured,
        .sample_rate = std::bit_cast<double>(sample_rate_bits),
        .maximum_frames = maximum_frames,
        .setup_failures = setup_failures_.load(std::memory_order_acquire),
        .controls_enqueued = controls_enqueued,
        .controls_applied = controls_applied,
        .controls_rejected = controls_rejected_.load(std::memory_order_acquire),
        .process_calls = process_calls_.load(std::memory_order_acquire),
        .last_frame_count = last_frame_count_.load(std::memory_order_relaxed),
        .process_failures = process_failures_.load(std::memory_order_acquire),
        .last_error =
            static_cast<sunny::core::ErrorCode>(last_error_.load(std::memory_order_acquire)),
    };
}

} // namespace sunny::max::detail
