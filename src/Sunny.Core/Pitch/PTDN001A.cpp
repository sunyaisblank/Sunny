/**
 * @file PTDN001A.cpp
 * @brief Dihedral group D₁₂ implementation
 *
 * Component: PTDN001A
 */

#include "PTDN001A.h"

#include <algorithm>
#include <unordered_set>

namespace Sunny::Core {

PitchClassSet d12_apply_set(D12Element g, const PitchClassSet& s) {
    PitchClassSet result;
    for (auto pc : s) {
        result.insert(d12_apply(g, pc));
    }
    return result;
}

std::vector<std::vector<D12Element>> d12_conjugacy_classes() {
    // D₁₂ has 9 conjugacy classes (n=12, even):
    // 1. {T₀}
    // 2. {T₆}
    // 3-7. {Tₖ, T₁₂₋ₖ} for k = 1..5
    // 8. {I₀, I₂, I₄, I₆, I₈, I₁₀}
    // 9. {I₁, I₃, I₅, I₇, I₉, I₁₁}

    std::vector<std::vector<D12Element>> classes;

    // {T₀}
    classes.push_back({d12_T(0)});

    // {Tₖ, T₁₂₋ₖ} for k = 1..5
    for (int k = 1; k <= 5; ++k) {
        classes.push_back({d12_T(k), d12_T(12 - k)});
    }

    // {T₆}
    classes.push_back({d12_T(6)});

    // Even inversions
    std::vector<D12Element> even_inv;
    for (int k = 0; k < 12; k += 2) {
        even_inv.push_back(d12_I(k));
    }
    classes.push_back(even_inv);

    // Odd inversions
    std::vector<D12Element> odd_inv;
    for (int k = 1; k < 12; k += 2) {
        odd_inv.push_back(d12_I(k));
    }
    classes.push_back(odd_inv);

    return classes;
}

std::vector<D12Element> d12_generate(std::span<const D12Element> generators) {
    // Closure under composition: repeatedly apply generators until stable
    struct ElemHash {
        std::size_t operator()(D12Element e) const noexcept {
            return std::hash<int>{}(e.is_inversion * 12 + e.k);
        }
    };

    std::unordered_set<D12Element, ElemHash> seen;
    std::vector<D12Element> queue;

    // Start with identity
    seen.insert(d12_identity());
    queue.push_back(d12_identity());

    // Add generators
    for (auto g : generators) {
        if (seen.insert(g).second) {
            queue.push_back(g);
        }
    }

    // BFS closure
    for (std::size_t i = 0; i < queue.size(); ++i) {
        for (auto g : generators) {
            auto product = d12_compose(queue[i], g);
            if (seen.insert(product).second) {
                queue.push_back(product);
            }
            auto inv_product = d12_compose(g, queue[i]);
            if (seen.insert(inv_product).second) {
                queue.push_back(inv_product);
            }
        }
    }

    // Sort: transpositions first (by k), then inversions (by k)
    std::sort(queue.begin(), queue.end(), [](D12Element a, D12Element b) {
        if (a.is_inversion != b.is_inversion) return !a.is_inversion;
        return a.k < b.k;
    });

    return queue;
}

std::vector<D12Subgroup> d12_named_subgroups() {
    std::vector<D12Subgroup> subs;

    // Cyclic subgroups (transpositions only)
    subs.push_back({"Z1", {d12_T(0)}});
    subs.push_back({"Z2", d12_generate(std::array{d12_T(6)})});
    subs.push_back({"Z3", d12_generate(std::array{d12_T(4)})});
    subs.push_back({"Z4", d12_generate(std::array{d12_T(3)})});
    subs.push_back({"Z6", d12_generate(std::array{d12_T(2)})});
    subs.push_back({"Z12", d12_generate(std::array{d12_T(1)})});

    // Dihedral subgroups (transpositions + an inversion)
    subs.push_back({"D1", d12_generate(std::array{d12_I(0)})});
    subs.push_back({"D2", d12_generate(std::array{d12_T(6), d12_I(0)})});
    subs.push_back({"D3", d12_generate(std::array{d12_T(4), d12_I(0)})});
    subs.push_back({"D4", d12_generate(std::array{d12_T(3), d12_I(0)})});
    subs.push_back({"D6", d12_generate(std::array{d12_T(2), d12_I(0)})});
    subs.push_back({"D12", d12_generate(std::array{d12_T(1), d12_I(0)})});

    return subs;
}

}  // namespace Sunny::Core
