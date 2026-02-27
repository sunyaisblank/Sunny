/**
 * @file ACRG001A.h
 * @brief Roughness Calculation
 *
 * Component: ACRG001A
 * Domain: AC (Acoustics) | Category: RG (Roughness)
 *
 * Formal Spec §14.3: Roughness quantifies amplitude fluctuation
 * from close-frequency partials.
 *
 * R = Σ_n Σ_m g(a_n, b_m) · d(n·f1, m·f2)
 *
 * where g(a, b) is an amplitude weighting function.
 *
 * Invariants:
 * - roughness(f, f, ...) == 0 (identical tones have no beating)
 * - roughness increases then decreases as frequency difference grows
 */

#pragma once

#include "ACPL001A.h"

#include <span>
#include <utility>
#include <vector>

namespace Sunny::Core {

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
[[nodiscard]] inline Result<double> roughness(
    std::span<const std::pair<double, double>> partials_a,
    std::span<const std::pair<double, double>> partials_b
) {
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
[[nodiscard]] inline Result<double> roughness_product(
    std::span<const std::pair<double, double>> partials_a,
    std::span<const std::pair<double, double>> partials_b
) {
    double total = 0.0;
    for (const auto& [fa, aa] : partials_a) {
        for (const auto& [fb, ab] : partials_b) {
            auto d = plomp_levelt_dissonance(fa, fb);
            if (!d) return std::unexpected(d.error());
            total += (aa * ab) * *d;
        }
    }
    return total;
}

}  // namespace Sunny::Core
