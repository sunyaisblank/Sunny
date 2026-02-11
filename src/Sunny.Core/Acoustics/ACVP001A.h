/**
 * @file ACVP001A.h
 * @brief Virtual Pitch and Missing Fundamental
 *
 * Component: ACVP001A
 * Domain: AC (Acoustics) | Category: VP (Virtual Pitch)
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
 * - virtual_pitch({f, 2f, 3f}) == f (harmonic series yields fundamental)
 * - virtual_pitch({2f, 3f, 4f}) ≈ f (missing fundamental inferred)
 */

#pragma once

#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace Sunny::Core {

/**
 * @brief Result of virtual pitch estimation
 */
struct VirtualPitchResult {
    double frequency;    ///< Estimated fundamental frequency in Hz
    double confidence;   ///< Match quality in [0, 1]
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
 * @return Estimated fundamental and confidence, or nullopt if no consensus
 */
[[nodiscard]] inline std::optional<VirtualPitchResult> virtual_pitch(
    std::span<const double> partial_freqs,
    double tolerance_cents = 50.0,
    int max_harmonic = 16
) {
    if (partial_freqs.empty()) return std::nullopt;

    double tolerance_ratio = std::pow(2.0, tolerance_cents / 1200.0);

    struct Candidate {
        double f0;
        int matches;
    };

    std::vector<Candidate> candidates;

    // Generate candidate fundamentals from each partial
    for (double fp : partial_freqs) {
        for (int n = 1; n <= max_harmonic; ++n) {
            double f0 = fp / n;
            if (f0 < 20.0) continue;  // Below audible range

            // Score this candidate against all partials
            int matches = 0;
            for (double fq : partial_freqs) {
                // Check if fq ≈ m·f0 for some integer m
                double ratio = fq / f0;
                double nearest_harmonic = std::round(ratio);
                if (nearest_harmonic < 1.0) continue;

                double actual_ratio = fq / (nearest_harmonic * f0);
                if (actual_ratio >= 1.0 / tolerance_ratio
                    && actual_ratio <= tolerance_ratio) {
                    ++matches;
                }
            }

            candidates.push_back({f0, matches});
        }
    }

    if (candidates.empty()) return std::nullopt;

    // Find candidate with most matches; prefer highest f0 at equal matches
    auto best = std::max_element(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.matches < b.matches
                || (a.matches == b.matches && a.f0 < b.f0);
        });

    // Merge candidates near the best f0 for stability
    double best_f0 = best->f0;
    double sum_f0 = 0.0;
    int count = 0;
    for (const auto& c : candidates) {
        if (c.matches == best->matches) {
            double r = c.f0 / best_f0;
            if (r >= 1.0 / tolerance_ratio && r <= tolerance_ratio) {
                sum_f0 += c.f0;
                ++count;
            }
        }
    }

    double final_f0 = sum_f0 / count;
    double confidence = static_cast<double>(best->matches)
                      / static_cast<double>(partial_freqs.size());

    return VirtualPitchResult{final_f0, std::min(confidence, 1.0)};
}

}  // namespace Sunny::Core
