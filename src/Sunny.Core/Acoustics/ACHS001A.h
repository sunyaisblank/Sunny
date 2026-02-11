/**
 * @file ACHS001A.h
 * @brief Harmonic Series reference table
 *
 * Component: ACHS001A
 * Domain: AC (Acoustics) | Category: HS (Harmonic Series)
 *
 * Formal Spec §14.1: The harmonic series f_n = n·f₀ for n = 1..16,
 * with 12-TET approximations and cent deviations.
 *
 * Invariants:
 * - partial_frequency(n, f0) == n * f0
 * - HARMONIC_SERIES[0].partial == 1 (fundamental)
 * - HARMONIC_SERIES[n].cents_deviation is relative to 12-TET
 */

#pragma once

#include <array>
#include <cmath>
#include <string_view>

namespace Sunny::Core {

/**
 * @brief Harmonic series entry
 */
struct HarmonicPartial {
    int partial;                  ///< Partial number (1 = fundamental)
    double ratio;                 ///< Frequency ratio to fundamental
    std::string_view interval;    ///< Approximate interval name
    double cents_deviation;       ///< Deviation from nearest 12-TET pitch (cents)
};

/// First 16 partials of the harmonic series (§14.1.1)
constexpr std::array<HarmonicPartial, 16> HARMONIC_SERIES = {{
    {1,  1.0,  "P1",          0.0},
    {2,  2.0,  "P8",          0.0},
    {3,  3.0,  "P8+P5",       2.0},
    {4,  4.0,  "2P8",         0.0},
    {5,  5.0,  "2P8+M3",    -13.7},
    {6,  6.0,  "2P8+P5",      2.0},
    {7,  7.0,  "2P8+m7",    -31.2},
    {8,  8.0,  "3P8",         0.0},
    {9,  9.0,  "3P8+M2",      3.9},
    {10, 10.0, "3P8+M3",    -13.7},
    {11, 11.0, "3P8+TT",    -48.7},
    {12, 12.0, "3P8+P5",      2.0},
    {13, 13.0, "3P8+m6",     40.5},
    {14, 14.0, "3P8+m7",    -31.2},
    {15, 15.0, "3P8+M7",    -11.7},
    {16, 16.0, "4P8",         0.0},
}};

/**
 * @brief Compute the frequency of the nth partial
 *
 * @param n Partial number (>= 1)
 * @param fundamental Fundamental frequency in Hz
 * @return Frequency of nth partial
 */
[[nodiscard]] constexpr double partial_frequency(int n, double fundamental) noexcept {
    return n * fundamental;
}

/**
 * @brief Generate a harmonic spectrum with amplitude rolloff
 *
 * Amplitude of nth partial = 1/n^rolloff.
 * Common: rolloff=1 (sawtooth-like), rolloff=2 (softer).
 *
 * @param fundamental Fundamental frequency
 * @param n_partials Number of partials to generate
 * @param rolloff Amplitude rolloff exponent (>= 0)
 * @return Pairs of (frequency, amplitude)
 */
[[nodiscard]] inline std::vector<std::pair<double, double>> harmonic_spectrum(
    double fundamental,
    int n_partials,
    double rolloff = 1.0
) {
    std::vector<std::pair<double, double>> result;
    result.reserve(n_partials);
    for (int n = 1; n <= n_partials; ++n) {
        double freq = n * fundamental;
        double amp = 1.0 / std::pow(static_cast<double>(n), rolloff);
        result.emplace_back(freq, amp);
    }
    return result;
}

}  // namespace Sunny::Core
