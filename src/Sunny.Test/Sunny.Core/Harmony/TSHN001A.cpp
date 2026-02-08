/**
 * @file TSHN001A.cpp
 * @brief Unit tests for HRNG001A (Negative harmony)
 *
 * Component: TSHN001A
 * Tests: HRNG001A
 *
 * Tests negative harmony axis inversion transformation.
 *
 * Key invariant: negative_harmony(negative_harmony(pcs, key), key) == pcs
 */

#include <catch2/catch_test_macros.hpp>

#include "Harmony/HRNG001A.h"
#include "Pitch/PTPS001A.h"

using namespace Sunny::Core;

TEST_CASE("HRNG001A: negative_harmony_axis", "[harmony][core]") {
    SECTION("C major doubled axis is 7") {
        // Axis at 3.5 semitones, doubled = 7
        REQUIRE(negative_harmony_axis(0) == 7);
    }

    SECTION("G major doubled axis is 21") {
        REQUIRE(negative_harmony_axis(7) == 21);  // 7 + 2*7 = 21
    }

    SECTION("Axis is transposition-consistent") {
        for (int root = 0; root < 12; ++root) {
            int axis = negative_harmony_axis(static_cast<PitchClass>(root));
            REQUIRE(axis == 7 + 2 * root);
        }
    }
}

TEST_CASE("HRNG001A: Involution property", "[harmony][core]") {
    SECTION("Double application returns original (C major context)") {
        // This is the key invariant: f(f(x)) = x
        PitchClassSet c_major = {0, 4, 7};  // C, E, G

        auto neg1 = negative_harmony(c_major, 0);
        auto neg2 = negative_harmony(neg1, 0);

        REQUIRE(neg2 == c_major);
    }

    SECTION("Involution holds for any chord") {
        // Test with various chords
        std::vector<PitchClassSet> chords = {
            {0, 4, 7},      // C major
            {0, 3, 7},      // C minor
            {5, 9, 0},      // F major
            {7, 11, 2},     // G major
            {9, 0, 4},      // A minor
            {0, 4, 7, 11},  // Cmaj7
            {7, 11, 2, 5},  // G7
        };

        for (const auto& chord : chords) {
            auto neg1 = negative_harmony(chord, 0);
            auto neg2 = negative_harmony(neg1, 0);
            REQUIRE(neg2 == chord);
        }
    }

    SECTION("Involution holds in different keys") {
        PitchClassSet chord = {0, 4, 7};

        for (int key = 0; key < 12; ++key) {
            auto neg1 = negative_harmony(chord, static_cast<PitchClass>(key));
            auto neg2 = negative_harmony(neg1, static_cast<PitchClass>(key));
            REQUIRE(neg2 == chord);
        }
    }
}

TEST_CASE("HRNG001A: Cardinality preservation", "[harmony][core]") {
    SECTION("|result| == |input|") {
        PitchClassSet triad = {0, 4, 7};
        auto neg = negative_harmony(triad, 0);
        REQUIRE(neg.size() == triad.size());

        PitchClassSet seventh = {0, 4, 7, 11};
        auto neg7 = negative_harmony(seventh, 0);
        REQUIRE(neg7.size() == seventh.size());

        PitchClassSet dyad = {0, 7};
        auto neg2 = negative_harmony(dyad, 0);
        REQUIRE(neg2.size() == dyad.size());
    }
}

TEST_CASE("HRNG001A: Known transformations in C major", "[harmony][core]") {
    // Negative harmony with axis at 3.5 semitones (between Eb and E)
    // Formula: I(x) = (7 - x) mod 12

    SECTION("C major -> C minor (I -> i)") {
        // C, E, G -> G, Eb, C = C minor
        PitchClassSet c_major = {0, 4, 7};
        auto result = negative_harmony(c_major, 0);

        PitchClassSet c_minor = {0, 3, 7};  // C, Eb, G
        REQUIRE(result == c_minor);
    }

    SECTION("G major -> F minor (V -> iv)") {
        // G, B, D -> C, Ab, F = F minor
        PitchClassSet g_major = {7, 11, 2};
        auto result = negative_harmony(g_major, 0);

        PitchClassSet f_minor = {0, 5, 8};  // F, Ab, C
        REQUIRE(result == f_minor);
    }

    SECTION("F major -> G minor (IV -> v)") {
        // F, A, C -> D, Bb, G = G minor
        PitchClassSet f_major = {5, 9, 0};
        auto result = negative_harmony(f_major, 0);

        PitchClassSet g_minor = {2, 7, 10};  // G, Bb, D
        REQUIRE(result == g_minor);
    }

    SECTION("A minor -> Eb major (vi -> bIII)") {
        // A, C, E -> Bb, G, Eb = Eb major
        PitchClassSet a_minor = {9, 0, 4};
        auto result = negative_harmony(a_minor, 0);

        PitchClassSet eb_major = {3, 7, 10};  // Eb, G, Bb
        REQUIRE(result == eb_major);
    }
}

TEST_CASE("HRNG001A: Functional duality", "[harmony][core]") {
    // Negative harmony swaps functional roles:
    // I (C major) -> i (C minor)
    // IV (F major) -> v (G minor)
    // V (G major) -> iv (F minor)

    SECTION("Tonic and subdominant swap roles") {
        // I -> i (same root, quality flip)
        PitchClassSet I = {0, 4, 7};
        auto neg_I = negative_harmony(I, 0);

        // IV -> v (G minor)
        PitchClassSet IV = {5, 9, 0};
        auto neg_IV = negative_harmony(IV, 0);

        // I (C major) becomes C minor {0, 3, 7}
        REQUIRE(neg_I.count(3) > 0);  // Contains Eb (minor 3rd of C)

        // IV (F major) becomes G minor {2, 7, 10}
        REQUIRE(neg_IV.count(10) > 0); // Contains Bb (minor 3rd of G)
    }
}

TEST_CASE("HRNG001A: Empty and single note", "[harmony][core]") {
    SECTION("Empty set returns empty") {
        PitchClassSet empty;
        auto result = negative_harmony(empty, 0);
        REQUIRE(result.empty());
    }

    SECTION("Single note inverts correctly") {
        PitchClassSet single = {0};  // C
        auto result = negative_harmony(single, 0);

        // I(0) = (7 - 0) % 12 = 7 (G)
        REQUIRE(result.size() == 1);
        REQUIRE(result.count(7) == 1);
    }
}

TEST_CASE("HRNG001A: All 12 pitch classes", "[harmony][core]") {
    SECTION("Chromatic set inverts to chromatic set") {
        PitchClassSet chromatic;
        for (int i = 0; i < 12; ++i) {
            chromatic.insert(static_cast<PitchClass>(i));
        }

        auto result = negative_harmony(chromatic, 0);
        REQUIRE(result.size() == 12);

        // All pitch classes should still be present
        for (int i = 0; i < 12; ++i) {
            REQUIRE(result.count(static_cast<PitchClass>(i)) == 1);
        }
    }
}

TEST_CASE("HRNG001A: Transposed keys", "[harmony][core]") {
    SECTION("G major context") {
        // G major chord in G major context
        PitchClassSet g_major = {7, 11, 2};  // G, B, D
        auto result = negative_harmony(g_major, 7);

        // Should transform to C minor in G context
        // Axis = (7 + 4) % 12 = 11
        // The result should be inverted around axis 11
    }

    SECTION("Different keys produce different results") {
        PitchClassSet chord = {0, 4, 7};

        auto c_context = negative_harmony(chord, 0);
        auto g_context = negative_harmony(chord, 7);

        // Same chord in different key contexts should produce different results
        REQUIRE(c_context != g_context);
    }
}
