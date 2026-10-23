/**
 * @file harmonic_series.hpp
 * @brief Harmonic Series reference table
 *
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
#include <sunny/core/types/music_types.hpp>
#include <vector>

namespace sunny::core {

/**
 * @brief Harmonic series entry
 */
struct HarmonicPartial {
    int partial;               ///< Partial number (1 = fundamental)
    double ratio;              ///< Frequency ratio to fundamental
    std::string_view interval; ///< Approximate interval name
    double cents_deviation;    ///< Deviation from nearest 12-TET pitch (cents)
};

/// First 16 partials of the harmonic series (§14.1.1)
constexpr std::array<HarmonicPartial, 16> HARMONIC_SERIES = {{
    {1, 1.0, "P1", 0.0},
    {2, 2.0, "P8", 0.0},
    {3, 3.0, "P8+P5", 2.0},
    {4, 4.0, "2P8", 0.0},
    {5, 5.0, "2P8+M3", -13.7},
    {6, 6.0, "2P8+P5", 2.0},
    {7, 7.0, "2P8+m7", -31.2},
    {8, 8.0, "3P8", 0.0},
    {9, 9.0, "3P8+M2", 3.9},
    {10, 10.0, "3P8+M3", -13.7},
    {11, 11.0, "3P8+TT", -48.7},
    {12, 12.0, "3P8+P5", 2.0},
    {13, 13.0, "3P8+m6", 40.5},
    {14, 14.0, "3P8+m7", -31.2},
    {15, 15.0, "3P8+M7", -11.7},
    {16, 16.0, "4P8", 0.0},
}};

/**
 * @brief Compute the frequency of the nth partial
 *
 * @param n Partial number (>= 1)
 * @param fundamental Fundamental frequency in Hz (> 0)
 * @return Frequency of nth partial, or error if n < 1 or fundamental <= 0
 */
[[nodiscard]] inline Result<double> partial_frequency(int n, double fundamental) {
    if (n < 1) return std::unexpected(ErrorCode::InvalidPartialNumber);
    if (!std::isfinite(fundamental) || fundamental <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    const double frequency = n * fundamental;
    if (!std::isfinite(frequency)) return std::unexpected(ErrorCode::ArithmeticOverflow);
    return frequency;
}

/**
 * @brief Generate a harmonic spectrum with amplitude rolloff
 *
 * Amplitude of nth partial = 1/n^rolloff.
 * Common: rolloff=1 (sawtooth-like), rolloff=2 (softer).
 *
 * @param fundamental Fundamental frequency (> 0)
 * @param n_partials Number of partials to generate (>= 1)
 * @param rolloff Amplitude rolloff exponent (>= 0)
 * @return Pairs of (frequency, amplitude), or error if fundamental <= 0
 */
[[nodiscard]] inline Result<std::vector<std::pair<double, double>>>
harmonic_spectrum(double fundamental, int n_partials, double rolloff = 1.0) {
    if (!std::isfinite(fundamental) || fundamental <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    if (n_partials < 1) return std::unexpected(ErrorCode::InvalidPartialNumber);
    if (!std::isfinite(rolloff) || rolloff < 0.0)
        return std::unexpected(ErrorCode::InvalidAcousticParameter);
    if (!std::isfinite(fundamental * n_partials))
        return std::unexpected(ErrorCode::ArithmeticOverflow);
    std::vector<std::pair<double, double>> result;
    result.reserve(n_partials);
    for (std::size_t n = 1; n <= static_cast<std::size_t>(n_partials); ++n) {
        double freq = n * fundamental;
        // Negative exponent avoids an overflowing reciprocal denominator;
        // very small partial amplitudes may underflow to admitted zero.
        double amp = std::pow(static_cast<double>(n), -rolloff);
        result.emplace_back(freq, amp);
    }
    return result;
}

} // namespace sunny::core
