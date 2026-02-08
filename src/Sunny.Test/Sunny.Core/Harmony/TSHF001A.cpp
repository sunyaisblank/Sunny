/**
 * @file TSHF001A.cpp
 * @brief Unit tests for HRFN001A (Functional harmony analysis)
 *
 * Component: TSHF001A
 * Tests: HRFN001A
 *
 * Tests chord function analysis and Roman numeral generation.
 */

#include <catch2/catch_test_macros.hpp>

#include "Harmony/HRFN001A.h"
#include "Pitch/PTPS001A.h"

using namespace Sunny::Core;

TEST_CASE("HRFN001A: Tonic function chords", "[harmony][core]") {
    SECTION("I chord is Tonic") {
        // C major chord: C, E, G
        PitchClassSet c_major = {0, 4, 7};
        auto analysis = analyze_chord_function(c_major, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Tonic);
        REQUIRE(analysis.degree == 1);
    }

    SECTION("vi chord is Tonic substitute") {
        // A minor chord: A, C, E (in C major)
        PitchClassSet a_minor = {9, 0, 4};
        auto analysis = analyze_chord_function(a_minor, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Tonic);
        REQUIRE(analysis.degree == 6);
    }

    SECTION("iii chord is Tonic substitute") {
        // E minor chord: E, G, B (in C major)
        PitchClassSet e_minor = {4, 7, 11};
        auto analysis = analyze_chord_function(e_minor, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Tonic);
        REQUIRE(analysis.degree == 3);
    }
}

TEST_CASE("HRFN001A: Subdominant function chords", "[harmony][core]") {
    SECTION("IV chord is Subdominant") {
        // F major chord: F, A, C (in C major)
        PitchClassSet f_major = {5, 9, 0};
        auto analysis = analyze_chord_function(f_major, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Subdominant);
        REQUIRE(analysis.degree == 4);
    }

    SECTION("ii chord is Subdominant") {
        // D minor chord: D, F, A (in C major)
        PitchClassSet d_minor = {2, 5, 9};
        auto analysis = analyze_chord_function(d_minor, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Subdominant);
        REQUIRE(analysis.degree == 2);
    }
}

TEST_CASE("HRFN001A: Dominant function chords", "[harmony][core]") {
    SECTION("V chord is Dominant") {
        // G major chord: G, B, D (in C major)
        PitchClassSet g_major = {7, 11, 2};
        auto analysis = analyze_chord_function(g_major, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Dominant);
        REQUIRE(analysis.degree == 5);
    }

    SECTION("vii° chord is Dominant substitute") {
        // B diminished: B, D, F (in C major)
        PitchClassSet b_dim = {11, 2, 5};
        auto analysis = analyze_chord_function(b_dim, 0, false);

        REQUIRE(analysis.function == HarmonicFunction::Dominant);
        REQUIRE(analysis.degree == 7);
    }
}

TEST_CASE("HRFN001A: Chord quality detection", "[harmony][core]") {
    SECTION("Major chord quality") {
        PitchClassSet c_major = {0, 4, 7};  // C, E, G
        auto analysis = analyze_chord_function(c_major, 0, false);
        REQUIRE(analysis.quality == "major");
    }

    SECTION("Minor chord quality") {
        PitchClassSet a_minor = {9, 0, 4};  // A, C, E
        auto analysis = analyze_chord_function(a_minor, 0, false);
        REQUIRE(analysis.quality == "minor");
    }

    SECTION("Diminished chord quality") {
        PitchClassSet b_dim = {11, 2, 5};  // B, D, F
        auto analysis = analyze_chord_function(b_dim, 0, false);
        REQUIRE(analysis.quality == "diminished");
    }

    SECTION("Dominant 7th chord quality") {
        PitchClassSet g7 = {7, 11, 2, 5};  // G, B, D, F
        auto analysis = analyze_chord_function(g7, 0, false);
        REQUIRE(analysis.quality == "dominant7");
    }

    SECTION("Major 7th chord quality") {
        PitchClassSet cmaj7 = {0, 4, 7, 11};  // C, E, G, B
        auto analysis = analyze_chord_function(cmaj7, 0, false);
        REQUIRE(analysis.quality == "major7");
    }

    SECTION("Minor 7th chord quality") {
        PitchClassSet dm7 = {2, 5, 9, 0};  // D, F, A, C
        auto analysis = analyze_chord_function(dm7, 0, false);
        REQUIRE(analysis.quality == "minor7");
    }
}

TEST_CASE("HRFN001A: degree_to_numeral", "[harmony][core]") {
    SECTION("Major numerals are uppercase") {
        REQUIRE(degree_to_numeral(0, true) == "I");
        REQUIRE(degree_to_numeral(1, true) == "II");
        REQUIRE(degree_to_numeral(2, true) == "III");
        REQUIRE(degree_to_numeral(3, true) == "IV");
        REQUIRE(degree_to_numeral(4, true) == "V");
        REQUIRE(degree_to_numeral(5, true) == "VI");
        REQUIRE(degree_to_numeral(6, true) == "VII");
    }

    SECTION("Minor numerals are lowercase") {
        REQUIRE(degree_to_numeral(0, false) == "i");
        REQUIRE(degree_to_numeral(1, false) == "ii");
        REQUIRE(degree_to_numeral(2, false) == "iii");
        REQUIRE(degree_to_numeral(3, false) == "iv");
        REQUIRE(degree_to_numeral(4, false) == "v");
        REQUIRE(degree_to_numeral(5, false) == "vi");
        REQUIRE(degree_to_numeral(6, false) == "vii");
    }

    SECTION("Invalid degree returns ?") {
        REQUIRE(degree_to_numeral(-1, true) == "?");
        REQUIRE(degree_to_numeral(7, true) == "?");
        REQUIRE(degree_to_numeral(100, true) == "?");
    }
}

TEST_CASE("HRFN001A: function_to_string", "[harmony][core]") {
    REQUIRE(function_to_string(HarmonicFunction::Tonic) == "T");
    REQUIRE(function_to_string(HarmonicFunction::Subdominant) == "S");
    REQUIRE(function_to_string(HarmonicFunction::Dominant) == "D");
}

TEST_CASE("HRFN001A: Minor key analysis", "[harmony][core]") {
    SECTION("i chord in minor key") {
        // A minor: A, C, E (in A minor key)
        PitchClassSet a_minor = {9, 0, 4};
        auto analysis = analyze_chord_function(a_minor, 9, true);

        REQUIRE(analysis.function == HarmonicFunction::Tonic);
        REQUIRE(analysis.degree == 1);
    }

    SECTION("iv chord in minor key") {
        // D minor: D, F, A (in A minor key)
        PitchClassSet d_minor = {2, 5, 9};
        auto analysis = analyze_chord_function(d_minor, 9, true);

        REQUIRE(analysis.function == HarmonicFunction::Subdominant);
    }

    SECTION("V chord in minor key") {
        // E major: E, G#, B (in A minor key - harmonic minor)
        PitchClassSet e_major = {4, 8, 11};
        auto analysis = analyze_chord_function(e_major, 9, true);

        REQUIRE(analysis.function == HarmonicFunction::Dominant);
    }
}

TEST_CASE("HRFN001A: Transposed keys", "[harmony][core]") {
    SECTION("G major key") {
        // G major chord in G major
        PitchClassSet g_major = {7, 11, 2};  // G, B, D
        auto analysis = analyze_chord_function(g_major, 7, false);

        REQUIRE(analysis.function == HarmonicFunction::Tonic);
        REQUIRE(analysis.degree == 1);
    }

    SECTION("D chord is V in G major") {
        // D major chord in G major
        PitchClassSet d_major = {2, 6, 9};  // D, F#, A
        auto analysis = analyze_chord_function(d_major, 7, false);

        REQUIRE(analysis.function == HarmonicFunction::Dominant);
        REQUIRE(analysis.degree == 5);
    }
}

TEST_CASE("HRFN001A: Edge cases", "[harmony][core]") {
    SECTION("Empty chord set") {
        PitchClassSet empty;
        auto analysis = analyze_chord_function(empty, 0, false);
        // Should return some default, not crash
        REQUIRE(analysis.quality == "unknown");
    }

    SECTION("Single note") {
        PitchClassSet single = {0};
        auto analysis = analyze_chord_function(single, 0, false);
        // Should handle gracefully
        REQUIRE(analysis.root == 0);
    }

    SECTION("Power chord (no third)") {
        PitchClassSet power = {0, 7};  // C, G
        auto analysis = analyze_chord_function(power, 0, false);
        // Quality depends on implementation
        REQUIRE((analysis.quality == "power" || analysis.quality == "sus" ||
                 analysis.quality == "unknown"));
    }
}
