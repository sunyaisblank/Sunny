/**
 * @file ACPL001A.h
 * @brief Plomp-Levelt consonance/dissonance model
 *
 * Component: ACPL001A
 * Domain: AC (Acoustics) | Category: PL (Plomp-Levelt)
 *
 * Formal Spec §14.2: Sensory consonance model based on critical bandwidth.
 *
 * The Plomp-Levelt model computes dissonance between two pure tones.
 * The Sethares extension sums pairwise dissonance across all partials
 * of complex tones, yielding a dissonance curve whose minima correspond
 * to "consonant" intervals for a given timbre.
 *
 * Invariants:
 * - dissonance(f, f) == 0 (unison is consonant)
 * - dissonance(f, 2f) ≈ 0 (octave is near-consonant for harmonic timbres)
 * - critical_bandwidth(f) > 0 for all f > 0
 */

#pragma once

#include "../Tensor/TNTP001A.h"

#include <cmath>
#include <span>
#include <utility>
#include <vector>

namespace Sunny::Core {

// =============================================================================
// Critical Bandwidth (§14.2)
// =============================================================================

/**
 * @brief Compute critical bandwidth using Bark scale approximation
 *
 * CB(f) ≈ 25 + 75 · (1 + 1.4 · (f/1000)²)^0.69
 *
 * @param freq Frequency in Hz (> 0)
 * @return Critical bandwidth in Hz, or InvalidFrequency if freq <= 0
 */
[[nodiscard]] inline Result<double> critical_bandwidth(double freq) {
    if (freq <= 0.0) return std::unexpected(ErrorCode::InvalidFrequency);
    double x = freq / 1000.0;
    return 25.0 + 75.0 * std::pow(1.0 + 1.4 * x * x, 0.69);
}

// =============================================================================
// Plomp-Levelt Pure Tone Dissonance (§14.2)
// =============================================================================

/// Empirical constants for the Plomp-Levelt model
constexpr double PL_A = 3.5;
constexpr double PL_B = 5.75;

/**
 * @brief Dissonance between two pure tones (Plomp-Levelt)
 *
 * d(f1, f2) = e^(-a·s) - e^(-b·s)
 * where s = |f2 - f1| / (0.24 · CB(mean(f1, f2)))
 *
 * @param f1 First frequency in Hz (> 0)
 * @param f2 Second frequency in Hz (> 0)
 * @return Dissonance value in [0, ~0.37], or InvalidFrequency if either <= 0
 */
[[nodiscard]] inline Result<double> plomp_levelt_dissonance(
    double f1, double f2
) {
    if (f1 <= 0.0 || f2 <= 0.0) return std::unexpected(ErrorCode::InvalidFrequency);
    double diff = std::abs(f2 - f1);
    if (diff < 1e-10) return 0.0;

    double mean_f = (f1 + f2) / 2.0;
    auto cb = critical_bandwidth(mean_f);
    if (!cb) return std::unexpected(cb.error());
    double s = diff / (0.24 * *cb);

    return std::exp(-PL_A * s) - std::exp(-PL_B * s);
}

// =============================================================================
// Sethares Model: Complex Tone Dissonance (§14.2)
// =============================================================================

/**
 * @brief Total dissonance between two complex tones (Sethares model)
 *
 * Sums pairwise Plomp-Levelt dissonance across all partial pairs,
 * weighted by partial amplitudes.
 *
 * Each tone is represented as a vector of (frequency, amplitude) pairs.
 * All frequencies must be positive.
 *
 * @param partials_a Partials of first tone: (freq, amplitude) pairs
 * @param partials_b Partials of second tone: (freq, amplitude) pairs
 * @return Total weighted dissonance, or InvalidFrequency if any frequency <= 0
 */
[[nodiscard]] inline Result<double> sethares_dissonance(
    std::span<const std::pair<double, double>> partials_a,
    std::span<const std::pair<double, double>> partials_b
) {
    double total = 0.0;
    for (const auto& [fa, aa] : partials_a) {
        for (const auto& [fb, ab] : partials_b) {
            double weight = std::min(aa, ab);
            auto d = plomp_levelt_dissonance(fa, fb);
            if (!d) return std::unexpected(d.error());
            total += weight * *d;
        }
    }
    return total;
}

/**
 * @brief Compute dissonance curve for a timbre over a range of intervals
 *
 * Sweeps the interval ratio from min_ratio to max_ratio and computes
 * dissonance at each step. Minima correspond to consonant intervals
 * for the given timbre.
 *
 * @param partials Reference tone partials: (freq, amplitude) pairs
 * @param min_ratio Minimum interval ratio (e.g., 1.0 for unison)
 * @param max_ratio Maximum interval ratio (e.g., 2.0 for octave)
 * @param steps Number of steps in the sweep
 * @return Vector of (ratio, dissonance) pairs, or error on invalid frequencies
 */
[[nodiscard]] inline Result<std::vector<std::pair<double, double>>> dissonance_curve(
    std::span<const std::pair<double, double>> partials,
    double min_ratio = 1.0,
    double max_ratio = 2.0,
    int steps = 100
) {
    std::vector<std::pair<double, double>> curve;
    curve.reserve(steps + 1);

    double step_size = (max_ratio - min_ratio) / steps;

    for (int i = 0; i <= steps; ++i) {
        double ratio = min_ratio + i * step_size;

        // Transpose all partials by ratio
        std::vector<std::pair<double, double>> transposed;
        transposed.reserve(partials.size());
        for (const auto& [f, a] : partials) {
            transposed.emplace_back(f * ratio, a);
        }

        auto d = sethares_dissonance(partials, transposed);
        if (!d) return std::unexpected(d.error());
        curve.emplace_back(ratio, *d);
    }

    return curve;
}

}  // namespace Sunny::Core
