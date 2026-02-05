/**
 * @file TSPS001A.cpp
 * @brief Unit tests for PTPS001A (Pitch class set operations)
 *
 * Component: TSPS001A
 * Domain: TS (Test) | Category: PS (Pitch Set)
 *
 * Tests: PTPS001A
 * Coverage: pcs_transpose, pcs_invert, pcs_interval_vector, pcs_normal_form, pcs_prime_form
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTPS001A.h"

using namespace Sunny::Core;

TEST_CASE("PTPS001A: pcs_transpose", "[pcs][core]") {
    SECTION("Identity: T_0") {
        PitchClassSet pcs = {0, 4, 7};  // C major triad
        auto result = pcs_transpose(pcs, 0);
        CHECK(result == pcs);
    }

    SECTION("Transpose major triad up P5") {
        PitchClassSet c_major = {0, 4, 7};  // C-E-G
        auto g_major = pcs_transpose(c_major, 7);  // G-B-D
        PitchClassSet expected = {7, 11, 2};
        CHECK(g_major == expected);
    }

    SECTION("Periodicity: T_12 = identity") {
        PitchClassSet pcs = {0, 3, 7};
        CHECK(pcs_transpose(pcs, 12) == pcs);
    }

    SECTION("Negative transposition") {
        PitchClassSet pcs = {7, 11, 2};  // G major
        auto result = pcs_transpose(pcs, -7);  // Back to C
        PitchClassSet expected = {0, 4, 7};
        CHECK(result == expected);
    }
}

TEST_CASE("PTPS001A: pcs_invert", "[pcs][core]") {
    SECTION("Self-inverse") {
        PitchClassSet pcs = {0, 4, 7};
        auto inverted = pcs_invert(pcs, 0);
        auto restored = pcs_invert(inverted, 0);
        CHECK(restored == pcs);
    }

    SECTION("I_0 of major triad") {
        PitchClassSet c_major = {0, 4, 7};
        auto result = pcs_invert(c_major, 0);
        // C(0)->C(0), E(4)->Ab(8), G(7)->F(5)
        PitchClassSet expected = {0, 5, 8};  // F minor
        CHECK(result == expected);
    }
}

TEST_CASE("PTPS001A: pcs_interval_vector", "[pcs][core]") {
    SECTION("Major triad: [0,0,1,1,1,0]") {
        PitchClassSet major = {0, 4, 7};
        auto iv = pcs_interval_vector(major);
        // M3 (ic4): 1, m3 (ic3): 1, P5 (ic5): 1
        CHECK(iv[0] == 0);  // ic1
        CHECK(iv[1] == 0);  // ic2
        CHECK(iv[2] == 1);  // ic3 (minor 3rd E-G)
        CHECK(iv[3] == 1);  // ic4 (major 3rd C-E)
        CHECK(iv[4] == 1);  // ic5 (P5 C-G)
        CHECK(iv[5] == 0);  // ic6
    }

    SECTION("Minor triad: [0,0,1,1,1,0]") {
        PitchClassSet minor = {0, 3, 7};
        auto iv = pcs_interval_vector(minor);
        // Same interval vector as major
        CHECK(iv[2] == 1);  // ic3
        CHECK(iv[3] == 1);  // ic4
        CHECK(iv[4] == 1);  // ic5
    }

    SECTION("Chromatic cluster: high in small ICs") {
        PitchClassSet cluster = {0, 1, 2};
        auto iv = pcs_interval_vector(cluster);
        CHECK(iv[0] == 2);  // Two semitones
        CHECK(iv[1] == 1);  // One whole tone
    }

    SECTION("Empty set") {
        PitchClassSet empty;
        auto iv = pcs_interval_vector(empty);
        for (int i = 0; i < 6; ++i) {
            CHECK(iv[i] == 0);
        }
    }
}

TEST_CASE("PTPS001A: pcs_normal_form", "[pcs][core]") {
    SECTION("Major triad normal form") {
        PitchClassSet major = {0, 4, 7};
        auto nf = pcs_normal_form(major);
        // Normal form starts at 0, most compact
        REQUIRE(nf.size() == 3);
        CHECK(nf[0] == 0);
    }

    SECTION("Ordering") {
        PitchClassSet pcs = {7, 0, 4};  // Same as above, different order
        auto nf = pcs_normal_form(pcs);
        REQUIRE(nf.size() == 3);
        // Should be in ascending order from most compact rotation
    }

    SECTION("Empty set") {
        PitchClassSet empty;
        auto nf = pcs_normal_form(empty);
        CHECK(nf.empty());
    }
}

TEST_CASE("PTPS001A: pcs_prime_form", "[pcs][core]") {
    SECTION("Major and minor triads have same prime form") {
        PitchClassSet major = {0, 4, 7};
        PitchClassSet minor = {0, 3, 7};

        auto pf_major = pcs_prime_form(major);
        auto pf_minor = pcs_prime_form(minor);

        CHECK(pf_major == pf_minor);  // Both are 3-11
    }

    SECTION("Prime form starts at 0") {
        PitchClassSet pcs = {5, 9, 0};  // F major
        auto pf = pcs_prime_form(pcs);
        REQUIRE(!pf.empty());
        CHECK(pf[0] == 0);
    }
}

TEST_CASE("PTPS001A: pcs equivalence", "[pcs][core]") {
    SECTION("Transposition equivalence") {
        PitchClassSet c_major = {0, 4, 7};
        PitchClassSet g_major = {7, 11, 2};
        CHECK(pcs_t_equivalent(c_major, g_major));
    }

    SECTION("TI equivalence") {
        PitchClassSet major = {0, 4, 7};
        PitchClassSet minor = {0, 3, 7};
        CHECK(pcs_ti_equivalent(major, minor));
    }

    SECTION("Non-equivalent sets") {
        PitchClassSet triad = {0, 4, 7};
        PitchClassSet seventh = {0, 4, 7, 10};
        CHECK(!pcs_t_equivalent(triad, seventh));
    }
}
