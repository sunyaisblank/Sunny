/**
 * @file RDMD001A.cpp
 * @brief Modulation source implementations
 *
 * Component: RDMD001A
 */

#include "RDMD001A.h"

#include <numbers>

namespace Sunny::Render {

double Lfo::process(double sample_rate) {
    // Advance phase
    double phase_increment = frequency_ / sample_rate;
    phase_ += phase_increment;
    if (phase_ >= 1.0) {
        phase_ -= 1.0;
        // Update random value at cycle boundary
        if (waveform_ == LfoWaveform::Random) {
            std::uniform_real_distribution<double> dist(-1.0, 1.0);
            last_random_ = dist(rng_);
        }
    }

    // Calculate output based on waveform
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

    return current_value_;
}

void Envelope::trigger() {
    attack_start_value_ = current_value_;
    state_ = EnvelopeState::Attack;
}

void Envelope::release() {
    if (state_ != EnvelopeState::Idle) {
        state_ = EnvelopeState::Release;
    }
}

void Envelope::reset() {
    state_ = EnvelopeState::Idle;
    current_value_ = 0.0;
}

double Envelope::process(double sample_rate) {
    double sample_time = 1.0 / sample_rate;

    switch (state_) {
        case EnvelopeState::Idle:
            current_value_ = 0.0;
            break;

        case EnvelopeState::Attack: {
            if (attack_ <= 0.0) {
                current_value_ = 1.0;
                state_ = EnvelopeState::Decay;
            } else {
                // Increment by (distance / attack_time) per sample
                double increment = (1.0 - attack_start_value_) * sample_time / attack_;
                current_value_ += increment;
                if (current_value_ >= 1.0) {
                    current_value_ = 1.0;
                    state_ = EnvelopeState::Decay;
                }
            }
            break;
        }

        case EnvelopeState::Decay: {
            if (decay_ <= 0.0) {
                current_value_ = sustain_;
                state_ = EnvelopeState::Sustain;
            } else {
                double rate = sample_time / decay_;
                current_value_ -= (1.0 - sustain_) * rate;
                if (current_value_ <= sustain_) {
                    current_value_ = sustain_;
                    state_ = EnvelopeState::Sustain;
                }
            }
            break;
        }

        case EnvelopeState::Sustain:
            current_value_ = sustain_;
            break;

        case EnvelopeState::Release: {
            if (release_ <= 0.0) {
                current_value_ = 0.0;
                state_ = EnvelopeState::Idle;
            } else {
                double rate = sample_time / release_;
                current_value_ -= sustain_ * rate;
                if (current_value_ <= 0.0) {
                    current_value_ = 0.0;
                    state_ = EnvelopeState::Idle;
                }
            }
            break;
        }
    }

    return current_value_;
}

}  // namespace Sunny::Render
