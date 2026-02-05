/**
 * @file TSSC001A.cpp
 * @brief Unit tests for SCDF001A (Scale definitions)
 *
 * Component: TSSC001A
 * Tests: SCDF001A
 *
 * Tests scale definition lookup and built-in scale registry.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

TEST_CASE("SCDF001A: Major scale definition", "[scale][core]") {
    SECTION("Major scale has correct intervals") {
        REQUIRE(SCALE_MAJOR.size() == 7);
        REQUIRE(SCALE_MAJOR[0] == 0);   // Root
        REQUIRE(SCALE_MAJOR[1] == 2);   // Major 2nd
        REQUIRE(SCALE_MAJOR[2] == 4);   // Major 3rd
        REQUIRE(SCALE_MAJOR[3] == 5);   // Perfect 4th
        REQUIRE(SCALE_MAJOR[4] == 7);   // Perfect 5th
        REQUIRE(SCALE_MAJOR[5] == 9);   // Major 6th
        REQUIRE(SCALE_MAJOR[6] == 11);  // Major 7th
    }

    SECTION("Major scale whole/half pattern: W-W-H-W-W-W-H") {
        // Verify step pattern
        REQUIRE(SCALE_MAJOR[1] - SCALE_MAJOR[0] == 2);  // W
        REQUIRE(SCALE_MAJOR[2] - SCALE_MAJOR[1] == 2);  // W
        REQUIRE(SCALE_MAJOR[3] - SCALE_MAJOR[2] == 1);  // H
        REQUIRE(SCALE_MAJOR[4] - SCALE_MAJOR[3] == 2);  // W
        REQUIRE(SCALE_MAJOR[5] - SCALE_MAJOR[4] == 2);  // W
        REQUIRE(SCALE_MAJOR[6] - SCALE_MAJOR[5] == 2);  // W
        // Final step to octave: 12 - 11 = 1 (H)
    }
}

TEST_CASE("SCDF001A: Minor scale definitions", "[scale][core]") {
    SECTION("Natural minor has correct intervals") {
        REQUIRE(SCALE_MINOR.size() == 7);
        REQUIRE(SCALE_MINOR[0] == 0);
        REQUIRE(SCALE_MINOR[1] == 2);
        REQUIRE(SCALE_MINOR[2] == 3);   // Minor 3rd
        REQUIRE(SCALE_MINOR[3] == 5);
        REQUIRE(SCALE_MINOR[4] == 7);
        REQUIRE(SCALE_MINOR[5] == 8);   // Minor 6th
        REQUIRE(SCALE_MINOR[6] == 10);  // Minor 7th
    }

    SECTION("Harmonic minor has raised 7th") {
        REQUIRE(SCALE_HARMONIC_MINOR[6] == 11);  // Major 7th
        // Rest same as natural minor
        REQUIRE(SCALE_HARMONIC_MINOR[2] == 3);
        REQUIRE(SCALE_HARMONIC_MINOR[5] == 8);
    }

    SECTION("Melodic minor has raised 6th and 7th") {
        REQUIRE(SCALE_MELODIC_MINOR[5] == 9);   // Major 6th
        REQUIRE(SCALE_MELODIC_MINOR[6] == 11);  // Major 7th
        REQUIRE(SCALE_MELODIC_MINOR[2] == 3);   // Minor 3rd
    }
}

TEST_CASE("SCDF001A: Mode definitions", "[scale][core]") {
    SECTION("Dorian is minor with raised 6th") {
        REQUIRE(SCALE_DORIAN[2] == 3);   // Minor 3rd
        REQUIRE(SCALE_DORIAN[5] == 9);   // Major 6th (raised)
        REQUIRE(SCALE_DORIAN[6] == 10);  // Minor 7th
    }

    SECTION("Phrygian is minor with lowered 2nd") {
        REQUIRE(SCALE_PHRYGIAN[1] == 1);  // Minor 2nd (lowered)
        REQUIRE(SCALE_PHRYGIAN[2] == 3);  // Minor 3rd
    }

    SECTION("Lydian is major with raised 4th") {
        REQUIRE(SCALE_LYDIAN[2] == 4);   // Major 3rd
        REQUIRE(SCALE_LYDIAN[3] == 6);   // Augmented 4th (raised)
    }

    SECTION("Mixolydian is major with lowered 7th") {
        REQUIRE(SCALE_MIXOLYDIAN[2] == 4);   // Major 3rd
        REQUIRE(SCALE_MIXOLYDIAN[6] == 10);  // Minor 7th (lowered)
    }

    SECTION("Locrian has lowered 2nd and 5th") {
        REQUIRE(SCALE_LOCRIAN[1] == 1);  // Minor 2nd
        REQUIRE(SCALE_LOCRIAN[4] == 6);  // Diminished 5th
    }
}

TEST_CASE("SCDF001A: Pentatonic scales", "[scale][core]") {
    SECTION("Pentatonic major has 5 notes") {
        REQUIRE(SCALE_PENTATONIC_MAJOR.size() == 5);
        REQUIRE(SCALE_PENTATONIC_MAJOR[0] == 0);
        REQUIRE(SCALE_PENTATONIC_MAJOR[1] == 2);
        REQUIRE(SCALE_PENTATONIC_MAJOR[2] == 4);
        REQUIRE(SCALE_PENTATONIC_MAJOR[3] == 7);
        REQUIRE(SCALE_PENTATONIC_MAJOR[4] == 9);
    }

    SECTION("Pentatonic minor has 5 notes") {
        REQUIRE(SCALE_PENTATONIC_MINOR.size() == 5);
        REQUIRE(SCALE_PENTATONIC_MINOR[0] == 0);
        REQUIRE(SCALE_PENTATONIC_MINOR[1] == 3);
        REQUIRE(SCALE_PENTATONIC_MINOR[2] == 5);
        REQUIRE(SCALE_PENTATONIC_MINOR[3] == 7);
        REQUIRE(SCALE_PENTATONIC_MINOR[4] == 10);
    }
}

TEST_CASE("SCDF001A: Blues scale", "[scale][core]") {
    SECTION("Blues scale has blue note (b5)") {
        REQUIRE(SCALE_BLUES.size() == 6);
        REQUIRE(SCALE_BLUES[0] == 0);
        REQUIRE(SCALE_BLUES[1] == 3);   // Minor 3rd
        REQUIRE(SCALE_BLUES[2] == 5);   // Perfect 4th
        REQUIRE(SCALE_BLUES[3] == 6);   // Blue note (dim 5th)
        REQUIRE(SCALE_BLUES[4] == 7);   // Perfect 5th
        REQUIRE(SCALE_BLUES[5] == 10);  // Minor 7th
    }
}

TEST_CASE("SCDF001A: Symmetric scales", "[scale][core]") {
    SECTION("Whole tone has 6 notes, all whole steps") {
        REQUIRE(SCALE_WHOLE_TONE.size() == 6);
        for (std::size_t i = 0; i < SCALE_WHOLE_TONE.size(); ++i) {
            REQUIRE(SCALE_WHOLE_TONE[i] == static_cast<Interval>(i * 2));
        }
    }

    SECTION("Diminished half-whole has 8 notes") {
        REQUIRE(SCALE_DIMINISHED_HW.size() == 8);
        // Alternating half-whole pattern
        REQUIRE(SCALE_DIMINISHED_HW[1] - SCALE_DIMINISHED_HW[0] == 1);  // H
        REQUIRE(SCALE_DIMINISHED_HW[2] - SCALE_DIMINISHED_HW[1] == 2);  // W
        REQUIRE(SCALE_DIMINISHED_HW[3] - SCALE_DIMINISHED_HW[2] == 1);  // H
        REQUIRE(SCALE_DIMINISHED_HW[4] - SCALE_DIMINISHED_HW[3] == 2);  // W
    }

    SECTION("Diminished whole-half has 8 notes") {
        REQUIRE(SCALE_DIMINISHED_WH.size() == 8);
        // Alternating whole-half pattern
        REQUIRE(SCALE_DIMINISHED_WH[1] - SCALE_DIMINISHED_WH[0] == 2);  // W
        REQUIRE(SCALE_DIMINISHED_WH[2] - SCALE_DIMINISHED_WH[1] == 1);  // H
        REQUIRE(SCALE_DIMINISHED_WH[3] - SCALE_DIMINISHED_WH[2] == 2);  // W
        REQUIRE(SCALE_DIMINISHED_WH[4] - SCALE_DIMINISHED_WH[3] == 1);  // H
    }

    SECTION("Chromatic has all 12 notes") {
        REQUIRE(SCALE_CHROMATIC.size() == 12);
        for (int i = 0; i < 12; ++i) {
            REQUIRE(SCALE_CHROMATIC[i] == static_cast<Interval>(i));
        }
    }
}

TEST_CASE("SCDF001A: Scale lookup by name", "[scale][core]") {
    SECTION("Find existing scales") {
        auto major = find_scale("major");
        REQUIRE(major.has_value());
        REQUIRE(major->name == "major");

        auto minor = find_scale("minor");
        REQUIRE(minor.has_value());

        auto dorian = find_scale("dorian");
        REQUIRE(dorian.has_value());
    }

    SECTION("Case-insensitive lookup") {
        auto major1 = find_scale("major");
        auto major2 = find_scale("Major");
        auto major3 = find_scale("MAJOR");

        REQUIRE(major1.has_value());
        REQUIRE(major2.has_value());
        REQUIRE(major3.has_value());
    }

    SECTION("Unknown scale returns nullopt") {
        auto unknown = find_scale("nonexistent");
        REQUIRE_FALSE(unknown.has_value());
    }
}

TEST_CASE("SCDF001A: Scale name listing", "[scale][core]") {
    SECTION("Returns non-empty list") {
        auto names = list_scale_names();
        REQUIRE(names.size() > 0);
    }

    SECTION("Contains common scales") {
        auto names = list_scale_names();
        bool has_major = false;
        bool has_minor = false;
        bool has_dorian = false;

        for (auto name : names) {
            if (name == "major") has_major = true;
            if (name == "minor") has_minor = true;
            if (name == "dorian") has_dorian = true;
        }

        REQUIRE(has_major);
        REQUIRE(has_minor);
        REQUIRE(has_dorian);
    }
}
