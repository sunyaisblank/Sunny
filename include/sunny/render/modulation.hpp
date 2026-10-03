/**
 * @file modulation.hpp
 * @brief Modulation sources (LFO, Envelope, S&H)
 *
 *
 * Provides validated scalar and bounded-vector modulation sources:
 * - LFO (sine, triangle, saw, square, random)
 * - ADSR Envelope
 * - Sample & Hold
 *
 * All modulators output normalized values [0.0, 1.0] or [-1.0, 1.0].
 */

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <sunny/core/types/music_types.hpp>
#include <sunny/render/signal_block.hpp>

namespace sunny::render {

/**
 * Thread ownership contract
 *
 * Modulator instances contain mutable recurrence and random-stream state and provide no internal
 * synchronisation. All control, trigger/reset, query, scalar, and block operations on one instance
 * require one exclusive owner. A host adapter must transfer validated controls to that owner and
 * apply them at a declared boundary; calling a setter concurrently with process_block is invalid.
 */

/// LFO waveform types
enum class LfoWaveform { Sine, Triangle, Saw, Square, Random };

/**
 * @brief Low Frequency Oscillator
 *
 * Generates periodic modulation signals.
 * Output range: [-1.0, 1.0]
 */
class Lfo {
  public:
    static constexpr std::int64_t signal_input_count = 0;
    static constexpr std::int64_t signal_output_count = 1;

    Lfo();

    sunny::core::VoidResult set_frequency(double hz);
    /**
     * Validate a candidate frequency against the active DSP context before
     * publishing it. Rejection preserves all oscillator state.
     */
    [[nodiscard]] sunny::core::VoidResult set_frequency(double hz,
                                                        const SignalBlockContext& context) noexcept;
    [[nodiscard]] static constexpr bool is_valid_waveform(LfoWaveform waveform) noexcept {
        switch (waveform) {
        case LfoWaveform::Sine:
        case LfoWaveform::Triangle:
        case LfoWaveform::Saw:
        case LfoWaveform::Square:
        case LfoWaveform::Random:
            return true;
        }
        return false;
    }
    sunny::core::VoidResult set_waveform(LfoWaveform waveform) noexcept;
    sunny::core::VoidResult set_phase(double phase);
    void set_seed(std::uint32_t seed);
    void reset();

    /**
     * Check retained LFO configuration against candidate DSP setup facts.
     * A host adapter uses this before publishing a rebuilt context.
     */
    [[nodiscard]] sunny::core::VoidResult
    validate_context(const SignalBlockContext& context) const noexcept;

    /// Render the current phase, then advance by one sample.
    [[nodiscard]] sunny::core::Result<double> process(double sample_rate);

    /**
     * Render one bounded signal vector after validating the complete call.
     * No allocation, locking, I/O, or partial output occurs in this method.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::span<double> output) noexcept;

    /**
     * Validate a signed host frame count and output pointer before constructing a span.
     * A null pointer is admitted only for an empty block.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        double* output) noexcept;

    /**
     * Validate a signed mono output count and pointer array before selecting
     * channel zero. The array is not dereferenced for a rejected count or an
     * empty vector.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        std::int64_t output_count,
                                                        double* const* outputs) noexcept;

    /**
     * Validate the complete signal topology supplied by a Max-shaped perform
     * callback. LFO is a traditional mono generator: exactly zero signal
     * inputs and one signal output. This is not an MC topology contract.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        std::int64_t input_count,
                                                        double* const* inputs,
                                                        std::int64_t output_count,
                                                        double* const* outputs) noexcept;

    /// Get current value without advancing
    [[nodiscard]] double value() const { return current_value_; }

  private:
    LfoWaveform waveform_{LfoWaveform::Sine};
    double frequency_{1.0};
    double phase_{0.0};
    double current_value_{0.0};
    std::uint32_t seed_{0};
    std::mt19937 rng_{seed_};
    double last_random_{0.0};

    void reset_random_stream();
    [[nodiscard]] double process_unchecked(double sample_rate) noexcept;
};

/// ADSR envelope state
enum class EnvelopeState { Idle, Attack, Decay, Sustain, Release };

/**
 * Relative tolerance of an envelope stage length. A stage of d seconds at sample rate r lasts
 * d x r samples; when that product lies within one part in 10^9 of an integer it is taken to be
 * the integer, absorbing the binary representation error of decimal durations such as 0.01 s.
 */
inline constexpr double ENVELOPE_STAGE_SNAP_TOLERANCE = 1e-9;

/**
 * @brief ADSR Envelope Generator
 *
 * Standard Attack-Decay-Sustain-Release envelope.
 * Output range: [0.0, 1.0]
 *
 * Each timed stage of d seconds at sample rate r spans L = d x r samples. The k-th sample emitted
 * in the stage (k >= 1) lies k / L of the way from the stage's start level to its end level, and
 * the stage ends on the first sample with k >= L, which emits the end level exactly: a 0.01 s
 * attack at 48 kHz emits 1/480, 2/480, ..., 1.0 over exactly 480 samples. A zero-length stage
 * therefore emits its end level for one sample, so a zero attack emits the peak. Position is
 * computed from the sample count, not by accumulating increments. If L changes within a stage
 * (a new sample rate or duration), the fraction already completed is kept and the remainder runs
 * at the new length.
 */
class Envelope {
  public:
    static constexpr std::int64_t signal_input_count = 0;
    static constexpr std::int64_t signal_output_count = 1;

    Envelope() = default;

    // Time parameters (in seconds)
    sunny::core::VoidResult set_attack(double seconds);
    sunny::core::VoidResult set_decay(double seconds);
    sunny::core::VoidResult set_sustain(double level);
    sunny::core::VoidResult set_release(double seconds);

    // Trigger control
    void trigger(); // Start attack
    void release(); // Start release
    void reset();   // Reset to idle

    /// Process one sample
    [[nodiscard]] sunny::core::Result<double> process(double sample_rate);

    /**
     * Render one bounded signal vector after validating the complete call.
     * No allocation, locking, I/O, or partial output occurs in this method.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::span<double> output) noexcept;

    /**
     * Validate a signed host frame count and output pointer before constructing a span.
     * A null pointer is admitted only for an empty block.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        double* output) noexcept;

    /**
     * Validate a signed mono output count and pointer array before selecting
     * channel zero. The array is not dereferenced for a rejected count or an
     * empty vector.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        std::int64_t output_count,
                                                        double* const* outputs) noexcept;

    /**
     * Validate the complete signal topology supplied by a Max-shaped perform
     * callback. Envelope is a traditional mono generator: exactly zero signal
     * inputs and one signal output. This is not an MC topology contract.
     */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        std::int64_t input_count,
                                                        double* const* inputs,
                                                        std::int64_t output_count,
                                                        double* const* outputs) noexcept;

    /// Get current value
    [[nodiscard]] double value() const { return current_value_; }

    /// Get current state
    [[nodiscard]] EnvelopeState state() const { return state_; }

    /// Check if envelope is active
    [[nodiscard]] bool is_active() const { return state_ != EnvelopeState::Idle; }

  private:
    EnvelopeState state_{EnvelopeState::Idle};
    double attack_{0.01};
    double decay_{0.1};
    double sustain_{0.7};
    double release_{0.3};
    double current_value_{0.0};
    double attack_start_value_{0.0};
    double release_start_value_{0.0};
    std::uint64_t stage_samples_{0}; ///< Samples emitted in the stage at stage_length_.
    double stage_length_{0.0};       ///< Stage length in samples when stage_samples_ began.
    double stage_completed_{0.0};    ///< Fraction completed before stage_length_ took effect.

    void enter_stage(EnvelopeState state) noexcept;
    /// Advance one sample in the current stage and return its position in [0, 1].
    [[nodiscard]] double advance_stage(double seconds, double sample_rate) noexcept;
    [[nodiscard]] double process_unchecked(double sample_rate) noexcept;
};

/**
 * @brief Sample and Hold
 *
 * Samples input at trigger points and holds value.
 */
class SampleAndHold {
  public:
    static constexpr std::int64_t signal_input_count = 0;
    static constexpr std::int64_t signal_output_count = 1;

    SampleAndHold() = default;

    sunny::core::VoidResult trigger(double input);
    [[nodiscard]] double value() const { return held_value_; }
    void reset() { held_value_ = 0.0; }

    /** Fill one bounded vector with the retained value without advancing state. */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::span<double> output) const noexcept;

    /** Validate a signed host frame count before constructing the output span. */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        double* output) const noexcept;

    /** Validate the traditional zero-input/mono-output generator topology. */
    [[nodiscard]] sunny::core::VoidResult process_block(const SignalBlockContext& context,
                                                        std::int64_t frame_count,
                                                        std::int64_t input_count,
                                                        double* const* inputs,
                                                        std::int64_t output_count,
                                                        double* const* outputs) const noexcept;

  private:
    double held_value_{0.0};
};

} // namespace sunny::render
