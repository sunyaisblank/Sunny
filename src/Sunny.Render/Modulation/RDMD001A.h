/**
 * @file RDMD001A.h
 * @brief Modulation sources (LFO, Envelope, S&H)
 *
 * Component: RDMD001A
 * Domain: RD (Render) | Category: MD (Modulation)
 *
 * Provides sample-accurate modulation sources:
 * - LFO (sine, triangle, saw, square, random)
 * - ADSR Envelope
 * - Sample & Hold
 *
 * All modulators output normalized values [0.0, 1.0] or [-1.0, 1.0].
 */

#pragma once

#include <cstdint>
#include <cmath>
#include <random>

namespace Sunny::Render {

/// LFO waveform types
enum class LfoWaveform {
    Sine,
    Triangle,
    Saw,
    Square,
    Random
};

/**
 * @brief Low Frequency Oscillator
 *
 * Generates periodic modulation signals.
 * Output range: [-1.0, 1.0]
 */
class Lfo {
public:
    Lfo() = default;

    void set_frequency(double hz) { frequency_ = hz; }
    void set_waveform(LfoWaveform waveform) { waveform_ = waveform; }
    void set_phase(double phase) { phase_ = phase; }
    void reset() { phase_ = 0.0; }

    /// Process one sample, advance phase
    [[nodiscard]] double process(double sample_rate);

    /// Get current value without advancing
    [[nodiscard]] double value() const { return current_value_; }

private:
    LfoWaveform waveform_{LfoWaveform::Sine};
    double frequency_{1.0};
    double phase_{0.0};
    double current_value_{0.0};
    std::mt19937 rng_{std::random_device{}()};
    double last_random_{0.0};
};

/// ADSR envelope state
enum class EnvelopeState {
    Idle,
    Attack,
    Decay,
    Sustain,
    Release
};

/**
 * @brief ADSR Envelope Generator
 *
 * Standard Attack-Decay-Sustain-Release envelope.
 * Output range: [0.0, 1.0]
 */
class Envelope {
public:
    Envelope() = default;

    // Time parameters (in seconds)
    void set_attack(double seconds) { attack_ = seconds; }
    void set_decay(double seconds) { decay_ = seconds; }
    void set_sustain(double level) { sustain_ = level; }  // [0, 1]
    void set_release(double seconds) { release_ = seconds; }

    // Trigger control
    void trigger();  // Start attack
    void release();  // Start release
    void reset();    // Reset to idle

    /// Process one sample
    [[nodiscard]] double process(double sample_rate);

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
};

/**
 * @brief Sample and Hold
 *
 * Samples input at trigger points and holds value.
 */
class SampleAndHold {
public:
    SampleAndHold() = default;

    void trigger(double input) { held_value_ = input; }
    [[nodiscard]] double value() const { return held_value_; }
    void reset() { held_value_ = 0.0; }

private:
    double held_value_{0.0};
};

}  // namespace Sunny::Render
