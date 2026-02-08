/**
 * @file ParameterSmoothing.h
 * @brief Parameter smoothing utilities
 *
 * Extracted from MXPR001A for testability.
 * Pure C++ with zero Max SDK dependencies.
 *
 * Provides one-pole exponential smoothing, linear ramping,
 * and clamping utilities for sample-accurate parameter control.
 */

#pragma once

#include <algorithm>
#include <cmath>

namespace Sunny::Max::Algorithm {

/**
 * @brief Calculate one-pole filter coefficient
 *
 * @param sample_rate Audio sample rate in Hz
 * @param smooth_ms Smoothing time in milliseconds
 * @return Filter coefficient in [0, 1). 0.0 means no smoothing (instant).
 */
inline double one_pole_coefficient(double sample_rate, double smooth_ms) {
    if (smooth_ms <= 0.0 || sample_rate <= 0.0) return 0.0;
    double smooth_samples = smooth_ms * sample_rate / 1000.0;
    return std::exp(-1.0 / smooth_samples);
}

/**
 * @brief Apply one-pole lowpass filter (single sample)
 *
 * @param current Current (smoothed) value
 * @param target Target value
 * @param coeff Filter coefficient from one_pole_coefficient()
 * @return Filtered output
 */
inline double one_pole_filter(double current, double target, double coeff) {
    if (coeff <= 0.0) return target;
    return current * coeff + target * (1.0 - coeff);
}

/**
 * @brief Calculate per-sample ramp increment for linear interpolation
 *
 * @param current Current value
 * @param target Target value
 * @param ramp_samples Number of samples over which to ramp
 * @return Per-sample increment
 */
inline double ramp_increment(double current, double target, long ramp_samples) {
    if (ramp_samples <= 0) return 0.0;
    return (target - current) / static_cast<double>(ramp_samples);
}

/**
 * @brief Clamp a value to the given range
 *
 * @param value Input value
 * @param min_val Minimum
 * @param max_val Maximum
 * @return Clamped value
 */
inline double clamp_value(double value, double min_val, double max_val) {
    return std::clamp(value, min_val, max_val);
}

/**
 * @brief Convert milliseconds to samples
 *
 * @param ms Duration in milliseconds
 * @param sample_rate Sample rate in Hz
 * @return Number of samples (minimum 1)
 */
inline long ms_to_samples(double ms, double sample_rate) {
    if (ms <= 0.0 || sample_rate <= 0.0) return 1;
    long samples = static_cast<long>(ms * sample_rate / 1000.0);
    return samples < 1 ? 1 : samples;
}

}  // namespace Sunny::Max::Algorithm
