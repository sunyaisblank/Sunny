/**
 * @file modulation_adapter.hpp
 * @brief Bounded Max message-to-audio adapters for Sunny modulation sources
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <sunny/max/control_transfer.hpp>
#include <sunny/render/modulation.hpp>

namespace sunny::max {

namespace detail {

enum class LfoCommandKind : std::uint8_t { Frequency, Waveform, Phase, Seed, Reset };

struct LfoCommand {
    LfoCommandKind kind{};
    double scalar = 0.0;
    std::uint32_t integer = 0;
};

enum class EnvelopeCommandKind : std::uint8_t {
    Attack,
    Decay,
    Sustain,
    ReleaseTime,
    Trigger,
    Release,
    Reset
};

struct EnvelopeCommand {
    EnvelopeCommandKind kind{};
    double scalar = 0.0;
};

enum class SampleAndHoldCommandKind : std::uint8_t { Value, Reset };

struct SampleAndHoldCommand {
    SampleAndHoldCommandKind kind{};
    double scalar = 0.0;
};

} // namespace detail

/**
 * @brief Traditional zero-input/mono-output Max adapter for `sunny::render::Lfo`.
 *
 * Any number of Max control producers may call the publish methods; a producer-
 * side mutex serialises them into one bounded SPSC publication stream. The audio
 * owner never takes that mutex and applies at most MAX_CONTROL_QUEUE_CAPACITY
 * commands at the next block boundary. `configure_dsp` models `dsp64` and must
 * not overlap `process`, matching Max's DSP-chain rebuild lifecycle.
 */
class LfoAdapter {
  public:
    LfoAdapter() = default;
    LfoAdapter(const LfoAdapter&) = delete;
    LfoAdapter& operator=(const LfoAdapter&) = delete;
    LfoAdapter(LfoAdapter&&) = delete;
    LfoAdapter& operator=(LfoAdapter&&) = delete;

    [[nodiscard]] sunny::core::VoidResult configure_dsp(double sample_rate,
                                                        std::int64_t maximum_frames) noexcept;

    [[nodiscard]] sunny::core::VoidResult publish_frequency(double hz) noexcept;
    [[nodiscard]] sunny::core::VoidResult
    publish_waveform(sunny::render::LfoWaveform waveform) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_phase(double phase) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_seed(std::uint32_t seed) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_reset() noexcept;

    [[nodiscard]] sunny::core::VoidResult process(std::int64_t frame_count,
                                                  std::int64_t input_count,
                                                  double* const* inputs,
                                                  std::int64_t output_count,
                                                  double* const* outputs) noexcept;
    [[nodiscard]] sunny::core::VoidResult silence_output(std::int64_t frame_count,
                                                         std::int64_t output_count,
                                                         double* const* outputs) const noexcept;

    [[nodiscard]] ModulationAdapterStatus status() const noexcept;
    void clear_error() noexcept;

  private:
    [[nodiscard]] sunny::core::VoidResult publish(detail::LfoCommand command) noexcept;
    void apply_pending_controls() noexcept;

    sunny::render::Lfo lfo_;
    std::optional<sunny::render::SignalBlockContext> context_;
    detail::SerializedSpscQueue<detail::LfoCommand, MAX_CONTROL_QUEUE_CAPACITY> controls_;
    mutable std::mutex publisher_mutex_;
    detail::AdapterStatusState status_;
};

/** Traditional zero-input/mono-output Max adapter for `sunny::render::Envelope`. */
class EnvelopeAdapter {
  public:
    EnvelopeAdapter() = default;
    EnvelopeAdapter(const EnvelopeAdapter&) = delete;
    EnvelopeAdapter& operator=(const EnvelopeAdapter&) = delete;
    EnvelopeAdapter(EnvelopeAdapter&&) = delete;
    EnvelopeAdapter& operator=(EnvelopeAdapter&&) = delete;

    [[nodiscard]] sunny::core::VoidResult configure_dsp(double sample_rate,
                                                        std::int64_t maximum_frames) noexcept;

    [[nodiscard]] sunny::core::VoidResult publish_attack(double seconds) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_decay(double seconds) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_sustain(double level) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_release_time(double seconds) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_trigger() noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_release() noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_reset() noexcept;

    [[nodiscard]] sunny::core::VoidResult process(std::int64_t frame_count,
                                                  std::int64_t input_count,
                                                  double* const* inputs,
                                                  std::int64_t output_count,
                                                  double* const* outputs) noexcept;
    [[nodiscard]] sunny::core::VoidResult silence_output(std::int64_t frame_count,
                                                         std::int64_t output_count,
                                                         double* const* outputs) const noexcept;

    [[nodiscard]] ModulationAdapterStatus status() const noexcept;
    void clear_error() noexcept;

  private:
    [[nodiscard]] sunny::core::VoidResult publish(detail::EnvelopeCommand command) noexcept;
    void apply_pending_controls() noexcept;

    sunny::render::Envelope envelope_;
    std::optional<sunny::render::SignalBlockContext> context_;
    detail::SerializedSpscQueue<detail::EnvelopeCommand, MAX_CONTROL_QUEUE_CAPACITY> controls_;
    mutable std::mutex publisher_mutex_;
    detail::AdapterStatusState status_;
};

/**
 * Traditional zero-input/mono-output adapter for a message-triggered held value.
 *
 * This is deliberately not a signal-triggered sample-and-hold: accepted value
 * messages take effect in order at the next vector boundary and the complete
 * vector contains the resulting retained scalar.
 */
class SampleAndHoldAdapter {
  public:
    SampleAndHoldAdapter() = default;
    SampleAndHoldAdapter(const SampleAndHoldAdapter&) = delete;
    SampleAndHoldAdapter& operator=(const SampleAndHoldAdapter&) = delete;
    SampleAndHoldAdapter(SampleAndHoldAdapter&&) = delete;
    SampleAndHoldAdapter& operator=(SampleAndHoldAdapter&&) = delete;

    [[nodiscard]] sunny::core::VoidResult configure_dsp(double sample_rate,
                                                        std::int64_t maximum_frames) noexcept;

    [[nodiscard]] sunny::core::VoidResult publish_value(double value) noexcept;
    [[nodiscard]] sunny::core::VoidResult publish_reset() noexcept;

    [[nodiscard]] sunny::core::VoidResult process(std::int64_t frame_count,
                                                  std::int64_t input_count,
                                                  double* const* inputs,
                                                  std::int64_t output_count,
                                                  double* const* outputs) noexcept;
    [[nodiscard]] sunny::core::VoidResult silence_output(std::int64_t frame_count,
                                                         std::int64_t output_count,
                                                         double* const* outputs) const noexcept;

    [[nodiscard]] ModulationAdapterStatus status() const noexcept;
    void clear_error() noexcept;

  private:
    [[nodiscard]] sunny::core::VoidResult publish(detail::SampleAndHoldCommand command) noexcept;
    void apply_pending_controls() noexcept;

    sunny::render::SampleAndHold sample_and_hold_;
    std::optional<sunny::render::SignalBlockContext> context_;
    detail::SerializedSpscQueue<detail::SampleAndHoldCommand, MAX_CONTROL_QUEUE_CAPACITY> controls_;
    mutable std::mutex publisher_mutex_;
    detail::AdapterStatusState status_;
};

} // namespace sunny::max
