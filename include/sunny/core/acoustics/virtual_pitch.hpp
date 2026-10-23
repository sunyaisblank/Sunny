/**
 * @file virtual_pitch.hpp
 * @brief Virtual Pitch and Missing Fundamental
 *
 *
 * Formal Spec §14.4: Virtual pitch is the perceived fundamental
 * frequency inferred from harmonic relationships among upper partials,
 * even when the fundamental is not physically present.
 *
 * Algorithm: For each candidate fundamental f₀, compute how well the
 * observed partials match the harmonic series n·f₀. The best-matching
 * f₀ is the virtual pitch.
 *
 * Invariants:
 * - harmonic-series consensus is bounded by the 20 Hz candidate floor,
 *   harmonic limit and cents tolerance
 * - confidence is the matched fraction for the returned candidate
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace sunny::core {

/**
 * @brief Result of virtual pitch estimation
 */
struct VirtualPitchResult {
    double frequency;  ///< Estimated fundamental frequency in Hz
    double confidence; ///< Match quality in [0, 1]
};

/**
 * @brief Estimate the virtual pitch (missing fundamental) from a set of partials
 *
 * Uses a simplified subharmonic coincidence algorithm:
 * 1. For each observed partial f_k, generate candidate fundamentals f_k/n
 *    for n = 1, 2, ..., max_harmonic.
 * 2. For each candidate, score how many observed partials match harmonics
 *    of that candidate (within tolerance).
 * 3. Return the candidate with the highest score.
 *
 * @param partial_freqs Observed partial frequencies (Hz)
 * @param tolerance_cents Maximum deviation to accept a match (default: 50 cents)
 * @param max_harmonic Maximum harmonic number to consider (default: 16)
 * @return Best candidate and matched fraction; nullopt on invalid controls,
 *         invalid frequencies, or no candidate at/above 20 Hz. No minimum
 *         matched fraction is imposed.
 */
[[nodiscard]] inline std::optional<VirtualPitchResult> virtual_pitch(
    std::span<const double> partial_freqs, double tolerance_cents = 50.0, int max_harmonic = 16) {
    if (partial_freqs.empty() || !std::isfinite(tolerance_cents) || tolerance_cents < 0.0 ||
        max_harmonic < 1)
        return std::nullopt;
    for (double frequency : partial_freqs) {
        if (!std::isfinite(frequency) || frequency <= 0.0) return std::nullopt;
    }

    double tolerance_ratio = std::pow(2.0, tolerance_cents / 1200.0);
    if (!std::isfinite(tolerance_ratio)) return std::nullopt;

    struct Candidate {
        double f0;
        std::size_t matches;
    };

    std::vector<Candidate> candidates;

    // Generate candidate fundamentals from each partial
    for (double fp : partial_freqs) {
        for (std::size_t n = 1; n <= static_cast<std::size_t>(max_harmonic); ++n) {
            double f0 = fp / n;
            if (f0 < 20.0) continue; // Below audible range

            // Score this candidate against all partials
            std::size_t matches = 0;
            for (double fq : partial_freqs) {
                // Check if fq ≈ m·f0 for some integer m
                double ratio = fq / f0;
                double nearest_harmonic = std::round(ratio);
                if (!std::isfinite(nearest_harmonic) || nearest_harmonic < 1.0 ||
                    nearest_harmonic > max_harmonic)
                    continue;

                double actual_ratio = ratio / nearest_harmonic;
                if (actual_ratio >= 1.0 / tolerance_ratio && actual_ratio <= tolerance_ratio) {
                    ++matches;
                }
            }

            candidates.push_back({f0, matches});
        }
    }

    if (candidates.empty()) return std::nullopt;

    // Find candidate with most matches; prefer highest f0 at equal matches
    auto best = std::max_element(
        candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.matches < b.matches || (a.matches == b.matches && a.f0 < b.f0);
        });

    double confidence =
        static_cast<double>(best->matches) / static_cast<double>(partial_freqs.size());

    // Averaging equal-scoring candidates can lose their distinct consensus
    // sets, making the reported match fraction false for the returned pitch.
    return VirtualPitchResult{best->f0, std::min(confidence, 1.0)};
}

} // namespace sunny::core
