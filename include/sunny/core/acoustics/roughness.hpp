/**
 * @file roughness.hpp
 * @brief Roughness Calculation
 *
 *
 * Formal Spec §14.3: Roughness quantifies amplitude fluctuation
 * from close-frequency partials.
 *
 * R = Σ_n Σ_m g(a_n, b_m) · d(n·f1, m·f2)
 *
 * where g(a, b) is an amplitude weighting function.
 *
 * Invariants:
 * - equal-frequency pairs contribute zero; other pairs may contribute even
 *   when the two spectra are identical
 * - roughness increases then decreases as frequency difference grows
 */

#pragma once

#include <span>
#include <sunny/core/acoustics/consonance.hpp>
#include <utility>
#include <vector>

namespace sunny::core {

/**
 * @brief Compute roughness between two complex tones
 *
 * Uses Plomp-Levelt dissonance as the pairwise distance metric d,
 * with amplitude weighting g(a, b) = min(a, b).
 *
 * This is equivalent to sethares_dissonance for the min-weighting,
 * but provided separately for clarity of API intent.
 *
 * @param partials_a Partials of first tone: (freq, amplitude) pairs
 * @param partials_b Partials of second tone: (freq, amplitude) pairs
 * @return Roughness value (>= 0), or error on invalid frequencies
 */
[[nodiscard]] inline Result<double>
roughness(std::span<const std::pair<double, double>> partials_a,
          std::span<const std::pair<double, double>> partials_b) {
    return sethares_dissonance(partials_a, partials_b);
}

/**
 * @brief Compute roughness using product amplitude weighting
 *
 * Uses g(a, b) = a · b instead of min(a, b).
 *
 * @param partials_a Partials of first tone: (freq, amplitude)
 * @param partials_b Partials of second tone: (freq, amplitude)
 * @return Roughness value (>= 0), or error on invalid frequencies
 */
[[nodiscard]] inline Result<double>
roughness_product(std::span<const std::pair<double, double>> partials_a,
                  std::span<const std::pair<double, double>> partials_b) {
    if (auto valid = detail::validate_acoustic_partials(partials_a); !valid)
        return std::unexpected(valid.error());
    if (auto valid = detail::validate_acoustic_partials(partials_b); !valid)
        return std::unexpected(valid.error());
    double total = 0.0;
    for (const auto& [fa, aa] : partials_a) {
        for (const auto& [fb, ab] : partials_b) {
            auto d = plomp_levelt_dissonance(fa, fb);
            if (!d) return std::unexpected(d.error());
            if (*d == 0.0 || aa == 0.0 || ab == 0.0) continue;
            total += (aa * ab) * *d;
            if (!std::isfinite(total)) return std::unexpected(ErrorCode::ArithmeticOverflow);
        }
    }
    return total;
}

} // namespace sunny::core
