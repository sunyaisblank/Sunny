/**
 * @file TSHR001A.cpp
 * @brief Unit tests for HRRN001A (Roman numeral parsing and chord generation)
 *
 * Component: TSHR001A
 * Tests: HRRN001A
 *
 * Tests Roman numeral parsing and chord generation from numerals.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Harmony/HRRN001A.h"
#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

TEST_CASE("HRRN001A: parse_roman_numeral basic", "[harmony][core]") {
    SECTION("Major numerals (uppercase)") {
        auto I = parse_roman_numeral("I");
        REQUIRE(I.has_value());
        REQUIRE(I->first == 0);   // Degree
        REQUIRE(I->second == true);  // Is major

        auto IV = parse_roman_numeral("IV");
        REQUIRE(IV.has_value());
        REQUIRE(IV->first == 3);
        REQUIRE(IV->second == true);

        auto V = parse_roman_numeral("V");
        REQUIRE(V.has_value());
        REQUIRE(V->first == 4);
        REQUIRE(V->second == true);
    }

    SECTION("Minor numerals (lowercase)") {
        auto i = parse_roman_numeral("i");
        REQUIRE(i.has_value());
        REQUIRE(i->first == 0);
        REQUIRE(i->second == false);

        auto ii = parse_roman_numeral("ii");
        REQUIRE(ii.has_value());
        REQUIRE(ii->first == 1);
        REQUIRE(ii->second == false);

        auto vi = parse_roman_numeral("vi");
        REQUIRE(vi.has_value());
        REQUIRE(vi->first == 5);
        REQUIRE(vi->second == false);
    }

    SECTION("All seven degrees") {
        std::array<std::string_view, 7> numerals = {
            "I", "II", "III", "IV", "V", "VI", "VII"
        };
        for (int i = 0; i < 7; ++i) {
            auto result = parse_roman_numeral(numerals[i]);
            REQUIRE(result.has_value());
            REQUIRE(result->first == i);
        }
    }
}

TEST_CASE("HRRN001A: parse_roman_numeral with modifiers", "[harmony][core]") {
    SECTION("Seventh chords") {
        auto V7 = parse_roman_numeral("V7");
        REQUIRE(V7.has_value());
        REQUIRE(V7->first == 4);

        auto ii7 = parse_roman_numeral("ii7");
        REQUIRE(ii7.has_value());
        REQUIRE(ii7->first == 1);
    }

    SECTION("Invalid numerals") {
        auto empty = parse_roman_numeral("");
        REQUIRE_FALSE(empty.has_value());

        auto invalid = parse_roman_numeral("X");
        REQUIRE_FALSE(invalid.has_value());

        auto invalid2 = parse_roman_numeral("IIX");
        REQUIRE_FALSE(invalid2.has_value());
    }
}

TEST_CASE("HRRN001A: generate_chord_from_numeral in C major", "[harmony][core]") {
    SECTION("I chord is C major") {
        auto result = generate_chord_from_numeral("I", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 0);  // C
        REQUIRE(chord.notes.size() >= 3);
        // C, E, G = 60, 64, 67
        REQUIRE(chord.notes[0] == 60);
        REQUIRE(chord.notes[1] == 64);
        REQUIRE(chord.notes[2] == 67);
    }

    SECTION("IV chord is F major") {
        auto result = generate_chord_from_numeral("IV", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 5);  // F
        // F, A, C = 65, 69, 72
        REQUIRE(chord.notes[0] == 65);
        REQUIRE(chord.notes[1] == 69);
        REQUIRE(chord.notes[2] == 72);
    }

    SECTION("V chord is G major") {
        auto result = generate_chord_from_numeral("V", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 7);  // G
        // G, B, D = 67, 71, 74
        REQUIRE(chord.notes[0] == 67);
        REQUIRE(chord.notes[1] == 71);
        REQUIRE(chord.notes[2] == 74);
    }

    SECTION("ii chord is D minor") {
        auto result = generate_chord_from_numeral("ii", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 2);  // D
        REQUIRE(chord.quality == "minor");
        // D, F, A = 62, 65, 69
        REQUIRE(chord.notes[0] == 62);
        REQUIRE(chord.notes[1] == 65);
        REQUIRE(chord.notes[2] == 69);
    }
}

TEST_CASE("HRRN001A: generate_chord_from_numeral with modifiers", "[harmony][core]") {
    SECTION("V7 is dominant seventh") {
        auto result = generate_chord_from_numeral("V7", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() >= 4);
        // G, B, D, F = 67, 71, 74, 77
        REQUIRE(chord.notes.size() == 4);
    }

    SECTION("vii° is diminished") {
        auto result = generate_chord_from_numeral("vii°", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.quality == "diminished");
        // B, D, F = 71, 74, 77
    }
}

TEST_CASE("HRRN001A: generate_chord basic", "[harmony][core]") {
    SECTION("C major chord") {
        auto result = generate_chord(0, "major", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 0);
        REQUIRE(chord.quality == "major");
        REQUIRE(chord.notes.size() == 3);
        REQUIRE(chord.notes[0] == 60);  // C4
        REQUIRE(chord.notes[1] == 64);  // E4
        REQUIRE(chord.notes[2] == 67);  // G4
    }

    SECTION("C minor chord") {
        auto result = generate_chord(0, "minor", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.quality == "minor");
        REQUIRE(chord.notes[1] == 63);  // Eb4 (minor 3rd)
    }

    SECTION("C diminished chord") {
        auto result = generate_chord(0, "diminished", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes[1] == 63);  // Eb4 (minor 3rd)
        REQUIRE(chord.notes[2] == 66);  // Gb4 (diminished 5th)
    }

    SECTION("C augmented chord") {
        auto result = generate_chord(0, "augmented", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes[1] == 64);  // E4 (major 3rd)
        REQUIRE(chord.notes[2] == 68);  // G#4 (augmented 5th)
    }
}

TEST_CASE("HRRN001A: generate_chord seventh chords", "[harmony][core]") {
    SECTION("C major 7") {
        auto result = generate_chord(0, "maj7", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() == 4);
        REQUIRE(chord.notes[3] == 71);  // B4 (major 7th)
    }

    SECTION("C dominant 7") {
        auto result = generate_chord(0, "7", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() == 4);
        REQUIRE(chord.notes[3] == 70);  // Bb4 (minor 7th)
    }

    SECTION("C minor 7") {
        auto result = generate_chord(0, "m7", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() == 4);
        REQUIRE(chord.notes[1] == 63);  // Eb4 (minor 3rd)
        REQUIRE(chord.notes[3] == 70);  // Bb4 (minor 7th)
    }

    SECTION("C diminished 7") {
        auto result = generate_chord(0, "dim7", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() == 4);
        REQUIRE(chord.notes[3] == 69);  // A4 (diminished 7th = major 6th)
    }

    SECTION("C half-diminished") {
        auto result = generate_chord(0, "m7b5", 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.notes.size() == 4);
        REQUIRE(chord.notes[2] == 66);  // Gb4 (diminished 5th)
        REQUIRE(chord.notes[3] == 70);  // Bb4 (minor 7th)
    }
}

TEST_CASE("HRRN001A: chord_quality_intervals", "[harmony][core]") {
    SECTION("Known qualities return intervals") {
        auto major = chord_quality_intervals("major");
        REQUIRE(major.has_value());
        REQUIRE(major->size() == 3);
        REQUIRE((*major)[0] == 0);
        REQUIRE((*major)[1] == 4);
        REQUIRE((*major)[2] == 7);

        auto minor = chord_quality_intervals("minor");
        REQUIRE(minor.has_value());
        REQUIRE((*minor)[1] == 3);

        auto dom7 = chord_quality_intervals("7");
        REQUIRE(dom7.has_value());
        REQUIRE(dom7->size() == 4);
    }

    SECTION("Unknown quality returns nullopt") {
        auto unknown = chord_quality_intervals("nonexistent");
        REQUIRE_FALSE(unknown.has_value());
    }

    SECTION("Case variations work") {
        auto maj1 = chord_quality_intervals("major");
        auto maj2 = chord_quality_intervals("Major");

        REQUIRE(maj1.has_value());
        REQUIRE(maj2.has_value());
    }
}

TEST_CASE("HRRN001A: Transposed keys", "[harmony][core]") {
    SECTION("I chord in G major") {
        auto result = generate_chord_from_numeral("I", 7, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 7);  // G
        // G4, B4, D5 = 67, 71, 74
        REQUIRE(chord.notes[0] == 67);
    }

    SECTION("V chord in D major") {
        auto result = generate_chord_from_numeral("V", 2, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());

        auto& chord = *result;
        REQUIRE(chord.root == 9);  // A
    }
}

TEST_CASE("HRRN001A: Octave parameter", "[harmony][core]") {
    SECTION("Different octaves produce correct MIDI notes") {
        auto oct3 = generate_chord(0, "major", 3);
        auto oct4 = generate_chord(0, "major", 4);
        auto oct5 = generate_chord(0, "major", 5);

        REQUIRE(oct3.has_value());
        REQUIRE(oct4.has_value());
        REQUIRE(oct5.has_value());

        REQUIRE(oct3->notes[0] == 48);  // C3
        REQUIRE(oct4->notes[0] == 60);  // C4
        REQUIRE(oct5->notes[0] == 72);  // C5
    }
}

TEST_CASE("HRRN001A: Error handling", "[harmony][core]") {
    SECTION("Invalid numeral returns error") {
        auto result = generate_chord_from_numeral("X", 0, SCALE_MAJOR, 4);
        REQUIRE_FALSE(result.has_value());
    }

    SECTION("Empty numeral returns error") {
        auto result = generate_chord_from_numeral("", 0, SCALE_MAJOR, 4);
        REQUIRE_FALSE(result.has_value());
    }
}
