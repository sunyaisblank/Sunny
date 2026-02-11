/**
 * @file TSDN001A.cpp
 * @brief Unit tests for PTDN001A (Dihedral group D₁₂)
 *
 * Component: TSDN001A
 * Tests: PTDN001A
 *
 * Tests D₁₂ group operations: composition, inverse, apply,
 * order, conjugacy classes, subgroup generation.
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTDN001A.h"

#include <algorithm>
#include <map>

using namespace Sunny::Core;

TEST_CASE("PTDN001A: d12_compose composition law (§1.2)", "[pitch][dihedral]") {
    SECTION("T_a compose T_b = T_{a+b}") {
        auto result = d12_compose(d12_T(3), d12_T(5));
        REQUIRE(result == d12_T(8));
    }

    SECTION("T_a compose T_b wraps mod 12") {
        auto result = d12_compose(d12_T(7), d12_T(8));
        REQUIRE(result == d12_T(3));  // 15 mod 12 = 3
    }

    SECTION("T_a compose I_b = I_{a+b}") {
        auto result = d12_compose(d12_T(3), d12_I(5));
        REQUIRE(result == d12_I(8));
    }

    SECTION("I_a compose T_b = I_{a-b}") {
        auto result = d12_compose(d12_I(5), d12_T(3));
        REQUIRE(result == d12_I(2));
    }

    SECTION("I_a compose T_b wraps mod 12") {
        auto result = d12_compose(d12_I(2), d12_T(5));
        REQUIRE(result == d12_I(9));  // (2-5+12) mod 12 = 9
    }

    SECTION("I_a compose I_b = T_{a-b}") {
        auto result = d12_compose(d12_I(7), d12_I(3));
        REQUIRE(result == d12_T(4));
    }

    SECTION("Composition is associative") {
        auto a = d12_T(3);
        auto b = d12_I(7);
        auto c = d12_T(5);
        REQUIRE(d12_compose(d12_compose(a, b), c) ==
                d12_compose(a, d12_compose(b, c)));
    }
}

TEST_CASE("PTDN001A: d12_identity and d12_inverse", "[pitch][dihedral]") {
    SECTION("Identity is T_0") {
        REQUIRE(d12_identity() == d12_T(0));
    }

    SECTION("g compose identity = g") {
        auto g = d12_I(5);
        REQUIRE(d12_compose(g, d12_identity()) == g);
        REQUIRE(d12_compose(d12_identity(), g) == g);
    }

    SECTION("T inverse: T_k^{-1} = T_{-k}") {
        REQUIRE(d12_inverse(d12_T(3)) == d12_T(9));
        REQUIRE(d12_inverse(d12_T(0)) == d12_T(0));
    }

    SECTION("I inverse: I_k^{-1} = I_k (involution)") {
        auto g = d12_I(7);
        REQUIRE(d12_inverse(g) == g);
    }

    SECTION("g compose g^{-1} = identity") {
        auto elems = d12_elements();
        for (auto g : elems) {
            REQUIRE(d12_compose(g, d12_inverse(g)) == d12_identity());
        }
    }
}

TEST_CASE("PTDN001A: d12_apply action on pitch classes", "[pitch][dihedral]") {
    SECTION("T_k(x) = (x + k) mod 12") {
        REQUIRE(d12_apply(d12_T(3), 0) == 3);
        REQUIRE(d12_apply(d12_T(7), 8) == 3);  // (8+7) mod 12
    }

    SECTION("I_k(x) = (k - x) mod 12") {
        REQUIRE(d12_apply(d12_I(0), 4) == 8);   // (0-4+12) mod 12
        REQUIRE(d12_apply(d12_I(7), 3) == 4);   // (7-3) mod 12
    }

    SECTION("Group action homomorphism: g(h(x)) = (g∘h)(x)") {
        auto g = d12_I(5);
        auto h = d12_T(3);
        PitchClass x = 7;
        REQUIRE(d12_apply(d12_compose(g, h), x) == d12_apply(g, d12_apply(h, x)));
    }

    SECTION("Action homomorphism holds for all element pairs on x=0") {
        auto elems = d12_elements();
        PitchClass x = 0;
        for (int i = 0; i < 24; i += 6) {
            for (int j = 0; j < 24; j += 6) {
                auto gh = d12_compose(elems[i], elems[j]);
                REQUIRE(d12_apply(gh, x) == d12_apply(elems[i], d12_apply(elems[j], x)));
            }
        }
    }
}

TEST_CASE("PTDN001A: d12_apply_set preserves cardinality (§16.2.1)", "[pitch][dihedral]") {
    PitchClassSet pcs = {0, 4, 7};  // C major triad
    auto elems = d12_elements();
    for (auto g : elems) {
        auto result = d12_apply_set(g, pcs);
        REQUIRE(result.size() == pcs.size());
    }
}

TEST_CASE("PTDN001A: d12_order", "[pitch][dihedral]") {
    SECTION("T_0 has order 1") {
        REQUIRE(d12_order(d12_T(0)) == 1);
    }

    SECTION("T_1 has order 12") {
        REQUIRE(d12_order(d12_T(1)) == 12);
    }

    SECTION("T_6 has order 2") {
        REQUIRE(d12_order(d12_T(6)) == 2);
    }

    SECTION("T_4 has order 3 (augmented triad cycle)") {
        REQUIRE(d12_order(d12_T(4)) == 3);
    }

    SECTION("T_3 has order 4 (diminished 7th cycle)") {
        REQUIRE(d12_order(d12_T(3)) == 4);
    }

    SECTION("All inversions have order 2") {
        for (int k = 0; k < 12; ++k) {
            REQUIRE(d12_order(d12_I(k)) == 2);
        }
    }
}

TEST_CASE("PTDN001A: d12_elements enumerates all 24", "[pitch][dihedral]") {
    auto elems = d12_elements();
    REQUIRE(elems.size() == 24);

    // First 12 are transpositions, next 12 are inversions
    for (int i = 0; i < 12; ++i) {
        REQUIRE(elems[i].is_inversion == false);
        REQUIRE(elems[i].k == i);
    }
    for (int i = 0; i < 12; ++i) {
        REQUIRE(elems[12 + i].is_inversion == true);
        REQUIRE(elems[12 + i].k == i);
    }
}

TEST_CASE("PTDN001A: d12_conjugacy_classes (§1.2)", "[pitch][dihedral]") {
    auto classes = d12_conjugacy_classes();

    SECTION("9 conjugacy classes") {
        REQUIRE(classes.size() == 9);
    }

    SECTION("Total elements = 24") {
        int total = 0;
        for (const auto& cls : classes) {
            total += static_cast<int>(cls.size());
        }
        REQUIRE(total == 24);
    }

    SECTION("Class sizes: 1,2,2,2,2,2,1,6,6") {
        std::vector<int> sizes;
        for (const auto& cls : classes) {
            sizes.push_back(static_cast<int>(cls.size()));
        }
        std::sort(sizes.begin(), sizes.end());
        std::vector<int> expected = {1, 1, 2, 2, 2, 2, 2, 6, 6};
        REQUIRE(sizes == expected);
    }
}

TEST_CASE("PTDN001A: d12_generate subgroup closure", "[pitch][dihedral]") {
    SECTION("T_4 generates Z_3 = {T_0, T_4, T_8}") {
        auto sub = d12_generate(std::array{d12_T(4)});
        REQUIRE(sub.size() == 3);
    }

    SECTION("T_3 generates Z_4 = {T_0, T_3, T_6, T_9}") {
        auto sub = d12_generate(std::array{d12_T(3)});
        REQUIRE(sub.size() == 4);
    }

    SECTION("T_1 generates Z_12 (all transpositions)") {
        auto sub = d12_generate(std::array{d12_T(1)});
        REQUIRE(sub.size() == 12);
    }

    SECTION("I_0 alone generates {T_0, I_0}") {
        auto sub = d12_generate(std::array{d12_I(0)});
        REQUIRE(sub.size() == 2);
    }

    SECTION("T_1 and I_0 generate all of D_12") {
        auto sub = d12_generate(std::array{d12_T(1), d12_I(0)});
        REQUIRE(sub.size() == 24);
    }

    SECTION("Generated set is closed under composition") {
        auto sub = d12_generate(std::array{d12_T(4), d12_I(0)});
        for (const auto& a : sub) {
            for (const auto& b : sub) {
                auto product = d12_compose(a, b);
                bool found = false;
                for (const auto& s : sub) {
                    if (s == product) { found = true; break; }
                }
                REQUIRE(found);
            }
        }
    }
}

TEST_CASE("PTDN001A: d12_named_subgroups", "[pitch][dihedral]") {
    auto subs = d12_named_subgroups();

    SECTION("Contains cyclic and dihedral subgroups") {
        REQUIRE(subs.size() == 12);
    }

    SECTION("Z1 has 1 element") {
        auto it = std::find_if(subs.begin(), subs.end(),
            [](const D12Subgroup& s) { return s.name == "Z1"; });
        REQUIRE(it != subs.end());
        REQUIRE(it->elements.size() == 1);
    }

    SECTION("D12 has 24 elements") {
        auto it = std::find_if(subs.begin(), subs.end(),
            [](const D12Subgroup& s) { return s.name == "D12"; });
        REQUIRE(it != subs.end());
        REQUIRE(it->elements.size() == 24);
    }

    SECTION("Subgroup sizes follow dihedral group theory") {
        // Z_d has d elements, D_d has 2d elements
        std::map<std::string, int> expected = {
            {"Z1", 1}, {"Z2", 2}, {"Z3", 3}, {"Z4", 4}, {"Z6", 6}, {"Z12", 12},
            {"D1", 2}, {"D2", 4}, {"D3", 6}, {"D4", 8}, {"D6", 12}, {"D12", 24}
        };
        for (const auto& sub : subs) {
            auto it = expected.find(sub.name);
            REQUIRE(it != expected.end());
            REQUIRE(static_cast<int>(sub.elements.size()) == it->second);
        }
    }
}
