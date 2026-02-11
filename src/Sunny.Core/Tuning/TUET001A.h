/**
 * @file TUET001A.h
 * @brief Equal temperament generalisation
 *
 * Component: TUET001A
 * Domain: TU (Tuning) | Category: ET (Equal Temperament)
 *
 * Formal Spec §13.1: Generalises 12-TET to n-EDO (n equal divisions
 * of the octave). Pitch class arithmetic operates over Z/nZ.
 *
 * Invariants:
 * - edo_step_cents(12) == 100.0
 * - edo_transpose(pc, 0, n) == pc
 * - edo_transpose(pc, n, n) == pc
 * - edo_invert(edo_invert(pc, axis, n), axis, n) == pc
 * - edo_frequency(69, 12) == 440.0 (reference pitch)
 * - ratio_to_cents(2.0) == 1200.0
 */

#pragma once

#include <cmath>

namespace Sunny::Core {

/**
 * @brief Cents per step in n-EDO
 *
 * @param n Number of equal divisions (> 0)
 * @return 1200.0 / n
 */
[[nodiscard]] constexpr double edo_step_cents(int n) noexcept {
    return 1200.0 / n;
}

/**
 * @brief Frequency for a pitch in n-EDO
 *
 * f(p) = f_ref · 2^((p - ref_pitch) / n)
 *
 * Default reference: A4 = 440 Hz at MIDI pitch 69 in 12-EDO.
 * For non-12 EDOs, ref_pitch is the pitch number of the reference.
 *
 * @param pitch Pitch number (integer, can be negative)
 * @param n EDO division count
 * @param ref_freq Reference frequency in Hz (default 440.0)
 * @param ref_pitch Reference pitch number (default 69)
 * @return Frequency in Hz
 */
[[nodiscard]] inline double edo_frequency(
    int pitch,
    int n,
    double ref_freq = 440.0,
    int ref_pitch = 69
) noexcept {
    return ref_freq * std::pow(2.0, static_cast<double>(pitch - ref_pitch) / n);
}

/**
 * @brief Convert a frequency ratio to cents
 *
 * cents = 1200 · log2(ratio)
 *
 * @param ratio Frequency ratio (must be > 0)
 * @return Interval in cents
 */
[[nodiscard]] inline double ratio_to_cents(double ratio) noexcept {
    return 1200.0 * std::log2(ratio);
}

/**
 * @brief Convert cents to a frequency ratio
 *
 * ratio = 2^(cents / 1200)
 *
 * @param cents Interval in cents
 * @return Frequency ratio
 */
[[nodiscard]] inline double cents_to_ratio(double cents) noexcept {
    return std::pow(2.0, cents / 1200.0);
}

/**
 * @brief Transpose a pitch class in n-EDO
 *
 * T_i(pc) = (pc + interval) mod n
 *
 * @param pitch_class Pitch class in Z/nZ
 * @param interval Transposition amount
 * @param n EDO division count
 * @return Transposed pitch class in [0, n)
 */
[[nodiscard]] constexpr int edo_transpose(int pitch_class, int interval, int n) noexcept {
    return ((pitch_class + interval) % n + n) % n;
}

/**
 * @brief Invert a pitch class in n-EDO
 *
 * I_axis(pc) = (2 · axis - pc) mod n
 *
 * @param pitch_class Pitch class in Z/nZ
 * @param axis Inversion axis
 * @param n EDO division count
 * @return Inverted pitch class in [0, n)
 */
[[nodiscard]] constexpr int edo_invert(int pitch_class, int axis, int n) noexcept {
    return ((2 * axis - pitch_class) % n + n) % n;
}

/**
 * @brief Interval class in n-EDO
 *
 * ic(a, b) = min(d, n - d) where d = (b - a) mod n
 *
 * @param a First pitch class
 * @param b Second pitch class
 * @param n EDO division count
 * @return Interval class in [0, n/2]
 */
[[nodiscard]] constexpr int edo_interval_class(int a, int b, int n) noexcept {
    int d = ((b - a) % n + n) % n;
    return d <= n / 2 ? d : n - d;
}

/**
 * @brief Best approximation of a just ratio in n-EDO
 *
 * Finds the step count k in n-EDO that minimises
 * |k · (1200/n) - 1200 · log2(ratio)|.
 *
 * @param ratio Just frequency ratio (> 0)
 * @param n EDO division count
 * @return Number of steps (0 to n-1)
 */
[[nodiscard]] inline int edo_approximate_ratio(double ratio, int n) noexcept {
    double cents = ratio_to_cents(ratio);
    double step_cents = edo_step_cents(n);
    int k = static_cast<int>(std::round(cents / step_cents));
    return ((k % n) + n) % n;
}

/**
 * @brief Approximation error in cents for a ratio in n-EDO
 *
 * @param ratio Just frequency ratio (> 0)
 * @param n EDO division count
 * @return Absolute deviation in cents
 */
[[nodiscard]] inline double edo_approximation_error(double ratio, int n) noexcept {
    double target = ratio_to_cents(ratio);
    int k = edo_approximate_ratio(ratio, n);
    double approx = k * edo_step_cents(n);
    return std::abs(target - approx);
}

}  // namespace Sunny::Core
