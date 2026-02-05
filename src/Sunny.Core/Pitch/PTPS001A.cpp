/**
 * @file PTPS001A.cpp
 * @brief Pitch class set operations implementation
 *
 * Component: PTPS001A
 */

#include "PTPS001A.h"

#include <algorithm>

namespace Sunny::Core {

PitchClassSet pcs_transpose(const PitchClassSet& pcs, int n) {
    PitchClassSet result;
    result.reserve(pcs.size());
    for (auto pc : pcs) {
        result.insert(transpose(pc, n));
    }
    return result;
}

PitchClassSet pcs_invert(const PitchClassSet& pcs, int axis) {
    PitchClassSet result;
    result.reserve(pcs.size());
    for (auto pc : pcs) {
        result.insert(invert(pc, axis));
    }
    return result;
}

std::array<int, 6> pcs_interval_vector(const PitchClassSet& pcs) {
    std::array<int, 6> counts = {0, 0, 0, 0, 0, 0};

    std::vector<PitchClass> sorted(pcs.begin(), pcs.end());
    std::sort(sorted.begin(), sorted.end());

    for (std::size_t i = 0; i < sorted.size(); ++i) {
        for (std::size_t j = i + 1; j < sorted.size(); ++j) {
            int ic = interval_class(sorted[j] - sorted[i]);
            if (ic >= 1 && ic <= 6) {
                counts[ic - 1]++;
            }
        }
    }

    return counts;
}

namespace {

// Helper: compute all rotations and find most compact
std::vector<PitchClass> find_normal_form_sorted(std::vector<PitchClass> sorted) {
    if (sorted.size() <= 1) return sorted;

    std::size_t n = sorted.size();
    std::vector<PitchClass> best = sorted;
    int best_span = 12;

    for (std::size_t r = 0; r < n; ++r) {
        // Rotate
        std::vector<PitchClass> rotated;
        rotated.reserve(n);
        for (std::size_t i = 0; i < n; ++i) {
            int pc = (sorted[(r + i) % n] - sorted[r] + 12) % 12;
            rotated.push_back(static_cast<PitchClass>(pc));
        }

        int span = rotated.back();  // Already transposed to start at 0
        if (span < best_span) {
            best_span = span;
            best = rotated;
        } else if (span == best_span) {
            // Compare lexicographically
            if (rotated < best) {
                best = rotated;
            }
        }
    }

    return best;
}

}  // namespace

std::vector<PitchClass> pcs_normal_form(const PitchClassSet& pcs) {
    if (pcs.empty()) return {};

    std::vector<PitchClass> sorted(pcs.begin(), pcs.end());
    std::sort(sorted.begin(), sorted.end());

    return find_normal_form_sorted(sorted);
}

std::vector<PitchClass> pcs_prime_form(const PitchClassSet& pcs) {
    if (pcs.empty()) return {};

    // Get normal form
    auto normal = pcs_normal_form(pcs);

    // Get normal form of inversion
    auto inverted = pcs_invert(pcs, 0);
    auto inv_normal = pcs_normal_form(inverted);

    // Return the lexicographically smaller one
    return (normal <= inv_normal) ? normal : inv_normal;
}

bool pcs_t_equivalent(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;
    auto prime_a = pcs_normal_form(a);
    auto prime_b = pcs_normal_form(b);
    return prime_a == prime_b;
}

bool pcs_ti_equivalent(const PitchClassSet& a, const PitchClassSet& b) {
    if (a.size() != b.size()) return false;
    auto prime_a = pcs_prime_form(a);
    auto prime_b = pcs_prime_form(b);
    return prime_a == prime_b;
}

}  // namespace Sunny::Core
