/**
 * @file consonance.hpp
 * @brief Plomp-Levelt consonance/dissonance model
 *
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
 * - dissonance is symmetric and nonnegative for finite positive frequencies
 * - critical_bandwidth(f) > 0 for admitted finite f > 0
 */

#pragma once

#include <cmath>
#include <span>
#include <sunny/core/types/music_types.hpp>
#include <utility>
#include <vector>

namespace sunny::core {

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
    if (!std::isfinite(freq) || freq <= 0.0) return std::unexpected(ErrorCode::InvalidFrequency);
    double x = freq / 1000.0;
    const double bandwidth = 25.0 + 75.0 * std::pow(1.0 + 1.4 * x * x, 0.69);
    if (!std::isfinite(bandwidth)) return std::unexpected(ErrorCode::ArithmeticOverflow);
    return bandwidth;
}

// =============================================================================
// Plomp-Levelt Pure Tone Dissonance (§14.2)
// =============================================================================

/// Sethares's published fit to the Plomp-Levelt data (dissmeasure).
constexpr double PL_A = 3.51;
constexpr double PL_B = 5.75;

/**
 * @brief Dissonance between two pure tones (Plomp-Levelt)
 *
 * d(f1, f2) = 5 (exp(-3.51 x) - exp(-5.75 x))
 * x = 0.24 |f2-f1| / (0.0207 min(f1,f2) + 18.96).
 * See https://sethares.engr.wisc.edu/comprog.html. The separate Bark
 * bandwidth approximation above is not this fit's frequency scaling.
 *
 * @param f1 First frequency in Hz (> 0)
 * @param f2 Second frequency in Hz (> 0)
 * @return Dimensionless dissonance in [0, ~0.899], or an input/arithmetic error
 */
[[nodiscard]] inline Result<double> plomp_levelt_dissonance(double f1, double f2) {
    if (!std::isfinite(f1) || !std::isfinite(f2) || f1 <= 0.0 || f2 <= 0.0)
        return std::unexpected(ErrorCode::InvalidFrequency);
    double diff = std::abs(f2 - f1);
    const double x = 0.24 * diff / (0.0207 * std::min(f1, f2) + 18.96);
    return 5.0 * (std::exp(-PL_A * x) - std::exp(-PL_B * x));
}

namespace detail {
[[nodiscard]] inline Result<void>
validate_acoustic_partials(std::span<const std::pair<double, double>> partials) {
    for (const auto& [frequency, amplitude] : partials) {
        if (!std::isfinite(frequency) || frequency <= 0.0)
            return std::unexpected(ErrorCode::InvalidFrequency);
        if (!std::isfinite(amplitude) || amplitude < 0.0)
            return std::unexpected(ErrorCode::InvalidAcousticParameter);
    }
    return {};
}
} // namespace detail

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
 * All frequencies must be finite and positive; amplitudes finite and nonnegative.
 * This sums between-tone pairs, matching Sethares's published BASIC loop.
 * It omits each tone's within-spectrum terms from the MATLAB union-spectrum sum.
 *
 * @param partials_a Partials of first tone: (freq, amplitude) pairs
 * @param partials_b Partials of second tone: (freq, amplitude) pairs
 * @return Total weighted dissonance, or InvalidFrequency if any frequency <= 0
 */
[[nodiscard]] inline Result<double>
sethares_dissonance(std::span<const std::pair<double, double>> partials_a,
                    std::span<const std::pair<double, double>> partials_b) {
    if (auto valid = detail::validate_acoustic_partials(partials_a); !valid)
        return std::unexpected(valid.error());
    if (auto valid = detail::validate_acoustic_partials(partials_b); !valid)
        return std::unexpected(valid.error());
    double total = 0.0;
    for (const auto& [fa, aa] : partials_a) {
        for (const auto& [fb, ab] : partials_b) {
            double weight = std::min(aa, ab);
            auto d = plomp_levelt_dissonance(fa, fb);
            if (!d) return std::unexpected(d.error());
            total += weight * *d;
            if (!std::isfinite(total)) return std::unexpected(ErrorCode::ArithmeticOverflow);
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
[[nodiscard]] inline Result<std::vector<std::pair<double, double>>>
dissonance_curve(std::span<const std::pair<double, double>> partials,
                 double min_ratio = 1.0,
                 double max_ratio = 2.0,
                 int steps = 100) {
    if (!std::isfinite(min_ratio) || !std::isfinite(max_ratio) || min_ratio <= 0.0 ||
        max_ratio < min_ratio || steps <= 0)
        return std::unexpected(ErrorCode::InvalidAcousticParameter);
    if (auto valid = detail::validate_acoustic_partials(partials); !valid)
        return std::unexpected(valid.error());
    std::vector<std::pair<double, double>> curve;
    curve.reserve(static_cast<std::size_t>(steps) + 1);

    double step_size = (max_ratio - min_ratio) / steps;

    for (std::size_t i = 0; i <= static_cast<std::size_t>(steps); ++i) {
        double ratio = i == static_cast<std::size_t>(steps) ? max_ratio : min_ratio + i * step_size;

        // Transpose all partials by ratio
        std::vector<std::pair<double, double>> transposed;
        transposed.reserve(partials.size());
        for (const auto& [f, a] : partials) {
            const double frequency = f * ratio;
            if (!std::isfinite(frequency)) return std::unexpected(ErrorCode::ArithmeticOverflow);
            transposed.emplace_back(frequency, a);
        }

        auto d = sethares_dissonance(partials, transposed);
        if (!d) return std::unexpected(d.error());
        curve.emplace_back(ratio, *d);
    }

    return curve;
}

} // namespace sunny::core
