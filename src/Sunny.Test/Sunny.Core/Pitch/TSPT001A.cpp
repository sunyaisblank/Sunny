/**
 * @file TSPT001A.cpp
 * @brief Unit tests for PTPC001A (Pitch class operations)
 *
 * Component: TSPT001A
 * Domain: TS (Test) | Category: PT (Pitch)
 *
 * Tests: PTPC001A
 * Coverage: pitch_class, transpose, invert, interval_class, note_name
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTPC001A.h"

using namespace Sunny::Core;

TEST_CASE("PTPC001A: pitch_class extracts mod 12", "[pitch][core]") {
    SECTION("C notes across octaves") {
        CHECK(pitch_class(0) == 0);    // C-1
        CHECK(pitch_class(12) == 0);   // C0
        CHECK(pitch_class(24) == 0);   // C1
        CHECK(pitch_class(60) == 0);   // C4
        CHECK(pitch_class(120) == 0);  // C9
    }

    SECTION("All pitch classes at octave 4") {
        for (int pc = 0; pc < 12; ++pc) {
            CHECK(pitch_class(60 + pc) == pc);
        }
    }

    SECTION("Edge cases") {
        CHECK(pitch_class(0) == 0);
        CHECK(pitch_class(127) == 7);  // G9
    }
}

TEST_CASE("PTPC001A: transpose is T_n operation", "[pitch][core]") {
    SECTION("Identity: T_0(x) = x") {
        for (int pc = 0; pc < 12; ++pc) {
            CHECK(transpose(pc, 0) == pc);
        }
    }

    SECTION("Periodicity: T_12(x) = x") {
        for (int pc = 0; pc < 12; ++pc) {
            CHECK(transpose(pc, 12) == pc);
            CHECK(transpose(pc, 24) == pc);
            CHECK(transpose(pc, -12) == pc);
        }
    }

    SECTION("Associativity: T_a(T_b(x)) = T_{a+b}(x)") {
        CHECK(transpose(transpose(0, 3), 4) == transpose(0, 7));
        CHECK(transpose(transpose(5, 7), 2) == transpose(5, 9));
    }

    SECTION("Specific transpositions") {
        CHECK(transpose(0, 7) == 7);   // C -> G (P5)
        CHECK(transpose(0, 5) == 5);   // C -> F (P4)
        CHECK(transpose(7, 5) == 0);   // G -> C
        CHECK(transpose(11, 2) == 1);  // B -> C# (wrap)
    }

    SECTION("Negative intervals") {
        CHECK(transpose(0, -1) == 11);  // C -> B
        CHECK(transpose(0, -7) == 5);   // C -> F
        CHECK(transpose(5, -5) == 0);   // F -> C
    }
}

TEST_CASE("PTPC001A: invert is I_n operation", "[pitch][core]") {
    SECTION("Self-inverse: I_n(I_n(x)) = x") {
        for (int axis = 0; axis < 12; ++axis) {
            for (int pc = 0; pc < 12; ++pc) {
                CHECK(invert(invert(pc, axis), axis) == pc);
            }
        }
    }

    SECTION("I_0 is negation mod 12") {
        CHECK(invert(0, 0) == 0);
        CHECK(invert(1, 0) == 11);
        CHECK(invert(2, 0) == 10);
        CHECK(invert(6, 0) == 6);  // Tritone is fixed point
    }

    SECTION("Specific inversions") {
        // Inversion around E-Eb axis (common in negative harmony)
        CHECK(invert(0, 4) == 8);   // C -> Ab
        CHECK(invert(7, 4) == 1);   // G -> Db
    }
}

TEST_CASE("PTPC001A: interval_class reduces to [0,6]", "[pitch][core]") {
    SECTION("Symmetry: ic(i) = ic(-i)") {
        for (int i = -12; i <= 12; ++i) {
            CHECK(interval_class(i) == interval_class(-i));
        }
    }

    SECTION("Complementary: ic(i) = ic(12-i)") {
        CHECK(interval_class(1) == interval_class(11));   // m2 = M7
        CHECK(interval_class(2) == interval_class(10));   // M2 = m7
        CHECK(interval_class(3) == interval_class(9));    // m3 = M6
        CHECK(interval_class(4) == interval_class(8));    // M3 = m6
        CHECK(interval_class(5) == interval_class(7));    // P4 = P5
        CHECK(interval_class(6) == 6);                     // TT is unique
    }

    SECTION("Values") {
        CHECK(interval_class(0) == 0);
        CHECK(interval_class(1) == 1);
        CHECK(interval_class(5) == 5);
        CHECK(interval_class(6) == 6);
        CHECK(interval_class(7) == 5);
        CHECK(interval_class(11) == 1);
    }
}

TEST_CASE("PTPC001A: note_name conversions", "[pitch][core]") {
    SECTION("Sharp names") {
        CHECK(note_name(0) == "C");
        CHECK(note_name(1) == "C#");
        CHECK(note_name(4) == "E");
        CHECK(note_name(6) == "F#");
        CHECK(note_name(11) == "B");
    }

    SECTION("Flat names") {
        CHECK(note_name(1, true) == "Db");
        CHECK(note_name(3, true) == "Eb");
        CHECK(note_name(6, true) == "Gb");
        CHECK(note_name(10, true) == "Bb");
    }
}

TEST_CASE("PTPC001A: note_to_pitch_class parsing", "[pitch][core]") {
    SECTION("Natural notes") {
        CHECK(note_to_pitch_class("C") == 0);
        CHECK(note_to_pitch_class("D") == 2);
        CHECK(note_to_pitch_class("E") == 4);
        CHECK(note_to_pitch_class("F") == 5);
        CHECK(note_to_pitch_class("G") == 7);
        CHECK(note_to_pitch_class("A") == 9);
        CHECK(note_to_pitch_class("B") == 11);
    }

    SECTION("Sharps") {
        CHECK(note_to_pitch_class("C#") == 1);
        CHECK(note_to_pitch_class("F#") == 6);
        CHECK(note_to_pitch_class("G#") == 8);
    }

    SECTION("Flats") {
        CHECK(note_to_pitch_class("Db") == 1);
        CHECK(note_to_pitch_class("Eb") == 3);
        CHECK(note_to_pitch_class("Bb") == 10);
    }

    SECTION("Case insensitive") {
        CHECK(note_to_pitch_class("c") == 0);
        CHECK(note_to_pitch_class("g") == 7);
    }

    SECTION("Invalid input") {
        CHECK(!note_to_pitch_class("").has_value());
        CHECK(!note_to_pitch_class("X").has_value());
        CHECK(!note_to_pitch_class("H").has_value());
    }
}
