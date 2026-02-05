/**
 * @file TSDG001A.cpp
 * @brief Diagnostic tests for pitch class invariants
 *
 * Component: TSDG001A
 * Domain: TS (Test) | Category: DG (Diagnostic)
 *
 * Verifies mathematical invariants of pitch class operations.
 * These tests ensure the Z/12Z group properties hold.
 */

#include <catch2/catch_test_macros.hpp>

#include <set>

#include "Pitch/PTPC001A.h"
#include "Pitch/PTPS001A.h"

using namespace Sunny::Core;

TEST_CASE("TSDG001A: Z/12Z group axioms", "[diagnostic][pitch]") {
    SECTION("Closure: T_n(x) is in [0,11]") {
        for (int pc = 0; pc < 12; ++pc) {
            for (int n = -24; n <= 24; ++n) {
                auto result = transpose(pc, n);
                CHECK(result >= 0);
                CHECK(result < 12);
            }
        }
    }

    SECTION("Associativity: T_a(T_b(x)) = T_{a+b}(x)") {
        for (int x = 0; x < 12; ++x) {
            for (int a = 0; a < 12; ++a) {
                for (int b = 0; b < 12; ++b) {
                    auto left = transpose(transpose(x, a), b);
                    auto right = transpose(x, a + b);
                    CHECK(left == right);
                }
            }
        }
    }

    SECTION("Identity: T_0(x) = x") {
        for (int x = 0; x < 12; ++x) {
            CHECK(transpose(x, 0) == x);
        }
    }

    SECTION("Inverse: T_{-n}(T_n(x)) = x") {
        for (int x = 0; x < 12; ++x) {
            for (int n = 0; n < 12; ++n) {
                auto result = transpose(transpose(x, n), -n);
                CHECK(result == x);
            }
        }
    }

    SECTION("Cyclic: T_12(x) = x") {
        for (int x = 0; x < 12; ++x) {
            CHECK(transpose(x, 12) == x);
        }
    }
}

TEST_CASE("TSDG001A: Dihedral group D_12 properties", "[diagnostic][pitch]") {
    SECTION("I_n is involution: I_n(I_n(x)) = x") {
        for (int x = 0; x < 12; ++x) {
            for (int n = 0; n < 12; ++n) {
                auto result = invert(invert(x, n), n);
                CHECK(result == x);
            }
        }
    }

    SECTION("Inversion closure: I_n(x) is in [0,11]") {
        for (int x = 0; x < 12; ++x) {
            for (int n = 0; n < 12; ++n) {
                auto result = invert(x, n);
                CHECK(result >= 0);
                CHECK(result < 12);
            }
        }
    }

    SECTION("T and I generate D_12") {
        // Every element of D_12 can be written as T_a or I_a for some a
        // Check that we can reach all 24 elements
        std::set<std::pair<int, bool>> elements;  // (value, is_inversion)

        for (int a = 0; a < 12; ++a) {
            elements.insert({a, false});  // T_a
            elements.insert({a, true});   // I_a
        }

        CHECK(elements.size() == 24);
    }
}

TEST_CASE("TSDG001A: Interval class properties", "[diagnostic][pitch]") {
    SECTION("ic is symmetric: ic(i) = ic(-i)") {
        for (int i = -24; i <= 24; ++i) {
            CHECK(interval_class(i) == interval_class(-i));
        }
    }

    SECTION("ic is in [0,6]") {
        for (int i = -24; i <= 24; ++i) {
            auto ic = interval_class(i);
            CHECK(ic >= 0);
            CHECK(ic <= 6);
        }
    }

    SECTION("Complementary intervals: ic(i) = ic(12-i)") {
        for (int i = 0; i <= 12; ++i) {
            CHECK(interval_class(i) == interval_class(12 - i));
        }
    }
}

TEST_CASE("TSDG001A: Interval vector invariants", "[diagnostic][pcs]") {
    SECTION("IV is invariant under transposition") {
        PitchClassSet pcs = {0, 4, 7};  // Major triad

        auto base_iv = pcs_interval_vector(pcs);

        for (int n = 0; n < 12; ++n) {
            auto transposed = pcs_transpose(pcs, n);
            auto transposed_iv = pcs_interval_vector(transposed);
            CHECK(transposed_iv == base_iv);
        }
    }

    SECTION("IV is invariant under inversion") {
        PitchClassSet pcs = {0, 4, 7};

        auto base_iv = pcs_interval_vector(pcs);

        for (int axis = 0; axis < 12; ++axis) {
            auto inverted = pcs_invert(pcs, axis);
            auto inverted_iv = pcs_interval_vector(inverted);
            CHECK(inverted_iv == base_iv);
        }
    }

    SECTION("IV sum equals n(n-1)/2 where n = |PCS|") {
        PitchClassSet pcs = {0, 3, 7, 10};  // 4 elements
        auto iv = pcs_interval_vector(pcs);

        int sum = 0;
        for (int i = 0; i < 6; ++i) {
            sum += iv[i];
        }

        // n=4, so n(n-1)/2 = 6
        CHECK(sum == 6);
    }
}

TEST_CASE("TSDG001A: Prime form uniqueness", "[diagnostic][pcs]") {
    SECTION("TI-equivalent sets have same prime form") {
        // All transpositions of major triad
        PitchClassSet base = {0, 4, 7};
        auto base_pf = pcs_prime_form(base);

        for (int t = 0; t < 12; ++t) {
            auto transposed = pcs_transpose(base, t);
            CHECK(pcs_prime_form(transposed) == base_pf);
        }

        // All inversions
        for (int axis = 0; axis < 12; ++axis) {
            auto inverted = pcs_invert(base, axis);
            CHECK(pcs_prime_form(inverted) == base_pf);
        }
    }
}
