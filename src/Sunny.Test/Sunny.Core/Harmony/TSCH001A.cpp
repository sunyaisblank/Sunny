/**
 * @file TSCH001A.cpp
 * @brief Unit tests for HRCH001A (Chromatic harmony)
 *
 * Component: TSCH001A
 * Tests: HRCH001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Harmony/HRCH001A.h"
#include "Pitch/PTPC001A.h"

using namespace Sunny::Core;

TEST_CASE("HRCH001A: neapolitan_sixth in C minor", "[harmony][core]") {
    SECTION("bII6 root is Db") {
        auto nea = neapolitan_sixth(0, true, 4);
        REQUIRE(nea.has_value());
        REQUIRE(nea->root == 1);  // Db
        REQUIRE(nea->inversion == 1);
    }

    SECTION("First inversion: bass is not the root") {
        auto nea = neapolitan_sixth(0, true, 4);
        REQUIRE(nea.has_value());
        // In first inversion, bass should be the third (F)
        REQUIRE(nea->notes.front() % 12 == 5);  // F
    }

    SECTION("Contains three notes") {
        auto nea = neapolitan_sixth(0, true, 4);
        REQUIRE(nea.has_value());
        REQUIRE(nea->notes.size() == 3);
    }

    SECTION("Neapolitan in G minor") {
        auto nea = neapolitan_sixth(7, true, 4);
        REQUIRE(nea.has_value());
        REQUIRE(nea->root == 8);  // Ab
    }
}

TEST_CASE("HRCH001A: neapolitan_sixth in C major", "[harmony][core]") {
    SECTION("Works in major key (borrowed chord)") {
        auto nea = neapolitan_sixth(0, false, 4);
        REQUIRE(nea.has_value());
        REQUIRE(nea->root == 1);  // Db
    }
}

TEST_CASE("HRCH001A: augmented_sixth Italian", "[harmony][core]") {
    SECTION("It+6 in C minor has 3 notes") {
        auto it6 = augmented_sixth(AugSixthType::Italian, 0, true, 4);
        REQUIRE(it6.has_value());
        REQUIRE(it6->notes.size() == 3);
        REQUIRE(it6->quality == "It+6");
    }

    SECTION("It+6 pitch classes are Ab, C, F#") {
        auto it6 = augmented_sixth(AugSixthType::Italian, 0, true, 4);
        REQUIRE(it6.has_value());
        auto pcs = it6->pitch_classes();
        // Should contain 8 (Ab), 0 (C), 6 (F#)
        REQUIRE(std::find(pcs.begin(), pcs.end(), 8) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 0) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 6) != pcs.end());
    }
}

TEST_CASE("HRCH001A: augmented_sixth French", "[harmony][core]") {
    SECTION("Fr+6 in C minor has 4 notes") {
        auto fr6 = augmented_sixth(AugSixthType::French, 0, true, 4);
        REQUIRE(fr6.has_value());
        REQUIRE(fr6->notes.size() == 4);
        REQUIRE(fr6->quality == "Fr+6");
    }

    SECTION("Fr+6 pitch classes are Ab, C, D, F#") {
        auto fr6 = augmented_sixth(AugSixthType::French, 0, true, 4);
        REQUIRE(fr6.has_value());
        auto pcs = fr6->pitch_classes();
        REQUIRE(std::find(pcs.begin(), pcs.end(), 8) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 0) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 2) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 6) != pcs.end());
    }
}

TEST_CASE("HRCH001A: augmented_sixth German", "[harmony][core]") {
    SECTION("Ger+6 in C minor has 4 notes") {
        auto ger6 = augmented_sixth(AugSixthType::German, 0, true, 4);
        REQUIRE(ger6.has_value());
        REQUIRE(ger6->notes.size() == 4);
        REQUIRE(ger6->quality == "Ger+6");
    }

    SECTION("Ger+6 pitch classes are Ab, C, Eb, F#") {
        auto ger6 = augmented_sixth(AugSixthType::German, 0, true, 4);
        REQUIRE(ger6.has_value());
        auto pcs = ger6->pitch_classes();
        REQUIRE(std::find(pcs.begin(), pcs.end(), 8) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 0) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 3) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 6) != pcs.end());
    }

    SECTION("German aug6 is enharmonic to dom7") {
        // Ger+6 in C: Ab, C, Eb, F# ≅ Ab7 (Ab, C, Eb, Gb)
        auto ger6 = augmented_sixth(AugSixthType::German, 0, true, 4);
        REQUIRE(ger6.has_value());
        auto pcs = ger6->pitch_classes();
        // Same pitch classes as Ab dominant 7: {8, 0, 3, 6}
        std::sort(pcs.begin(), pcs.end());
        auto unique_end = std::unique(pcs.begin(), pcs.end());
        pcs.erase(unique_end, pcs.end());
        REQUIRE(pcs.size() == 4);
    }
}

TEST_CASE("HRCH001A: augmented_sixth in other keys", "[harmony][core]") {
    SECTION("It+6 in A minor") {
        auto it6 = augmented_sixth(AugSixthType::Italian, 9, true, 4);
        REQUIRE(it6.has_value());
        auto pcs = it6->pitch_classes();
        // A minor: b6=F(5), 1=A(9), #4=D#(3)
        REQUIRE(std::find(pcs.begin(), pcs.end(), 5) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 9) != pcs.end());
        REQUIRE(std::find(pcs.begin(), pcs.end(), 3) != pcs.end());
    }
}

TEST_CASE("HRCH001A: tritone_substitution", "[harmony][core]") {
    SECTION("Tritone sub of G7 is Db7") {
        auto sub = tritone_substitution(7, 4);  // G = pc 7
        REQUIRE(sub.has_value());
        REQUIRE(sub->root == 1);  // Db
        REQUIRE(sub->quality == "7");
        REQUIRE(sub->notes.size() == 4);
    }

    SECTION("Tritone sub of C7 is F#7/Gb7") {
        auto sub = tritone_substitution(0, 4);
        REQUIRE(sub.has_value());
        REQUIRE(sub->root == 6);  // F#/Gb
    }

    SECTION("Tritone sub preserves 3rd and 7th exchange") {
        // G7: G(7), B(11), D(2), F(5)  — 3rd=B(11), 7th=F(5)
        // Db7: Db(1), F(5), Ab(8), Cb(11) — 3rd=F(5), 7th=Cb=B(11)
        auto g7 = tritone_substitution(1, 4);  // sub of Db7 = G7
        REQUIRE(g7.has_value());
        REQUIRE(g7->root == 7);

        auto db7 = tritone_substitution(7, 4);  // sub of G7 = Db7
        REQUIRE(db7.has_value());
        REQUIRE(db7->root == 1);

        // Both share pitch classes 11 and 5
        auto g7_pcs = g7->pitch_classes();
        auto db7_pcs = db7->pitch_classes();
        bool g7_has_11 = std::find(g7_pcs.begin(), g7_pcs.end(), 11) != g7_pcs.end();
        bool g7_has_5 = std::find(g7_pcs.begin(), g7_pcs.end(), 5) != g7_pcs.end();
        bool db7_has_11 = std::find(db7_pcs.begin(), db7_pcs.end(), 11) != db7_pcs.end();
        bool db7_has_5 = std::find(db7_pcs.begin(), db7_pcs.end(), 5) != db7_pcs.end();
        REQUIRE(g7_has_11);
        REQUIRE(g7_has_5);
        REQUIRE(db7_has_11);
        REQUIRE(db7_has_5);
    }

    SECTION("Double tritone sub returns original root") {
        auto sub1 = tritone_substitution(0, 4);
        REQUIRE(sub1.has_value());
        auto sub2 = tritone_substitution(sub1->root, 4);
        REQUIRE(sub2.has_value());
        REQUIRE(sub2->root == 0);
    }
}

TEST_CASE("HRCH001A: common_tone_diminished", "[harmony][core]") {
    SECTION("CT dim of C = C# dim7") {
        auto ct = common_tone_diminished(0, 4);
        REQUIRE(ct.has_value());
        REQUIRE(ct->root == 1);  // C#
        REQUIRE(ct->quality == "dim7");
        REQUIRE(ct->notes.size() == 4);
    }

    SECTION("CT dim of G = Ab dim7") {
        auto ct = common_tone_diminished(7, 4);
        REQUIRE(ct.has_value());
        REQUIRE(ct->root == 8);  // Ab
    }

    SECTION("CT dim shares common tone with target") {
        // CT dim of C: C#, E, G, Bb
        // Target C major: C, E, G
        // Common tone: C is not in the dim chord, but E and G are
        auto ct = common_tone_diminished(0, 4);
        REQUIRE(ct.has_value());
        auto pcs = ct->pitch_classes();
        // The dim chord contains the root of the target enharmonically
        // C#dim7 = {1, 4, 7, 10} — shares 4(E) and 7(G) with C major {0, 4, 7}
        bool has_e = std::find(pcs.begin(), pcs.end(), 4) != pcs.end();
        bool has_g = std::find(pcs.begin(), pcs.end(), 7) != pcs.end();
        REQUIRE(has_e);
        REQUIRE(has_g);
    }
}
