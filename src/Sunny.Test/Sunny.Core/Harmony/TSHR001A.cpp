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
    }

    SECTION("Numeral with suffix parses base") {
        // "IIX" → parses "II" (degree 1), "X" is suffix
        auto iix = parse_roman_numeral("IIX");
        REQUIRE(iix.has_value());
        REQUIRE(iix->first == 1);
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

TEST_CASE("HRRN001A: Extended chord qualities (11th, 13th)", "[harmony][core]") {
    SECTION("Dominant 11th chord") {
        auto result = generate_chord(0, "11", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        // C, E, G, Bb, D, F = 60, 64, 67, 70, 74, 77
        REQUIRE(result->notes[0] == 60);
        REQUIRE(result->notes[1] == 64);
        REQUIRE(result->notes[2] == 67);
        REQUIRE(result->notes[3] == 70);
        REQUIRE(result->notes[4] == 74);
        REQUIRE(result->notes[5] == 77);
    }

    SECTION("Major 11th chord") {
        auto result = generate_chord(0, "maj11", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        REQUIRE(result->notes[3] == 71);  // B4 (major 7th)
    }

    SECTION("Minor 11th chord") {
        auto result = generate_chord(0, "m11", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        REQUIRE(result->notes[1] == 63);  // Eb4 (minor 3rd)
    }

    SECTION("Dominant 13th chord (11th omitted per §5.3)") {
        auto result = generate_chord(0, "13", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        // C, E, G, Bb, D, A = 60, 64, 67, 70, 74, 81
        REQUIRE(result->notes[0] == 60);
        REQUIRE(result->notes[5] == 81);
    }

    SECTION("Major 13th chord (11th omitted per §5.3)") {
        auto result = generate_chord(0, "maj13", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        REQUIRE(result->notes[3] == 71);  // B (major 7th)
        REQUIRE(result->notes[5] == 81);  // A (13th)
    }

    SECTION("Minor 13th chord (11th omitted per §5.3)") {
        auto result = generate_chord(0, "m13", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        REQUIRE(result->notes[1] == 63);  // Eb (minor 3rd)
    }
}

TEST_CASE("HRRN001A: Altered extension qualities", "[harmony][core]") {
    SECTION("dom7b9") {
        auto result = generate_chord(0, "dom7b9", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
        // C, E, G, Bb, Db = 60, 64, 67, 70, 73
        REQUIRE(result->notes[4] == 73);
    }

    SECTION("dom7s9") {
        auto result = generate_chord(0, "dom7s9", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
        // C, E, G, Bb, D# = 60, 64, 67, 70, 75
        REQUIRE(result->notes[4] == 75);
    }

    SECTION("dom7s11") {
        auto result = generate_chord(0, "dom7s11", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
        // C, E, G, Bb, F# = 60, 64, 67, 70, 78
        REQUIRE(result->notes[4] == 78);
    }

    SECTION("dom7b13") {
        auto result = generate_chord(0, "dom7b13", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
        // C, E, G, Bb, Ab = 60, 64, 67, 70, 80
        REQUIRE(result->notes[4] == 80);
    }

    SECTION("7alt chord (no P5 per spec)") {
        auto result = generate_chord(0, "7alt", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 7);
        // Altered: C, E, Bb, Db, D#, F#, Ab = 60, 64, 70, 73, 75, 78, 80
        REQUIRE(result->notes[0] == 60);
        REQUIRE(result->notes[1] == 64);
    }

    SECTION("alt alias matches 7alt") {
        auto alt = generate_chord(0, "alt", 4);
        auto alt7 = generate_chord(0, "7alt", 4);
        REQUIRE(alt.has_value());
        REQUIRE(alt7.has_value());
        REQUIRE(alt->notes == alt7->notes);
    }
}

TEST_CASE("HRRN001A: Extended chords via numeral", "[harmony][core]") {
    SECTION("V11 in C major") {
        auto result = generate_chord_from_numeral("V11", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
        // G dominant 11: G, B, D, F, A, C
        REQUIRE(result->root == 7);
    }

    SECTION("V13 in C major (11th omitted)") {
        auto result = generate_chord_from_numeral("V13", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 6);
    }

    SECTION("Vb9 in C major") {
        auto result = generate_chord_from_numeral("Vb9", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
    }

    SECTION("Valt in C major (no P5 per spec)") {
        auto result = generate_chord_from_numeral("Valt", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 7);
    }
}

TEST_CASE("HRRN001A: New chord qualities (Appendix B)", "[harmony][core]") {
    SECTION("Power chord") {
        auto result = generate_chord(0, "5", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 2);
        REQUIRE(result->notes[0] == 60);  // C4
        REQUIRE(result->notes[1] == 67);  // G4

        auto alias = generate_chord(0, "power", 4);
        REQUIRE(alias.has_value());
        REQUIRE(alias->notes == result->notes);
    }

    SECTION("Augmented 7th") {
        auto result = generate_chord(0, "aug7", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 4);
        // C, E, G#, Bb = 60, 64, 68, 70
        REQUIRE(result->notes[2] == 68);  // G# (augmented 5th)
        REQUIRE(result->notes[3] == 70);  // Bb (minor 7th)
    }

    SECTION("Augmented major 7th") {
        auto result = generate_chord(0, "augmaj7", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 4);
        // C, E, G#, B = 60, 64, 68, 71
        REQUIRE(result->notes[2] == 68);  // G# (augmented 5th)
        REQUIRE(result->notes[3] == 71);  // B (major 7th)
    }

    SECTION("6/9 chord") {
        auto result = generate_chord(0, "69", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 5);
        // C, E, G, A, D = 60, 64, 67, 69, 74
        REQUIRE(result->notes[3] == 69);  // A (6th)
        REQUIRE(result->notes[4] == 74);  // D (9th)

        auto alias = generate_chord(0, "6/9", 4);
        REQUIRE(alias.has_value());
        REQUIRE(alias->notes == result->notes);
    }

    SECTION("Altered extension aliases") {
        auto b9a = generate_chord(0, "7b9", 4);
        auto b9b = generate_chord(0, "dom7b9", 4);
        REQUIRE(b9a.has_value());
        REQUIRE(b9b.has_value());
        REQUIRE(b9a->notes == b9b->notes);

        auto s9a = generate_chord(0, "7#9", 4);
        auto s9b = generate_chord(0, "dom7s9", 4);
        REQUIRE(s9a.has_value());
        REQUIRE(s9b.has_value());
        REQUIRE(s9a->notes == s9b->notes);

        auto s11a = generate_chord(0, "7#11", 4);
        auto s11b = generate_chord(0, "dom7s11", 4);
        REQUIRE(s11a.has_value());
        REQUIRE(s11b.has_value());
        REQUIRE(s11a->notes == s11b->notes);

        auto b13a = generate_chord(0, "7b13", 4);
        auto b13b = generate_chord(0, "dom7b13", 4);
        REQUIRE(b13a.has_value());
        REQUIRE(b13b.has_value());
        REQUIRE(b13a->notes == b13b->notes);
    }

    SECTION("Add11 chord") {
        auto result = generate_chord(0, "add11", 4);
        REQUIRE(result.has_value());
        REQUIRE(result->notes.size() == 4);
        // C, E, G, F = 60, 64, 67, 77
        REQUIRE(result->notes[3] == 77);
    }
}

TEST_CASE("HRRN001A: parse_roman_numeral_full basic", "[harmony][core]") {
    SECTION("Simple degrees") {
        auto r = parse_roman_numeral_full("I");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 0);
        REQUIRE(r->is_upper == true);
        REQUIRE(r->accidental == 0);
        REQUIRE(r->is_neapolitan == false);
        REQUIRE(r->inversion == FiguredBass::Root);
    }

    SECTION("Lowercase degree") {
        auto r = parse_roman_numeral_full("vi");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 5);
        REQUIRE(r->is_upper == false);
    }

    SECTION("Quality modifier diminished") {
        auto r = parse_roman_numeral_full("vii°");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 6);
        REQUIRE(r->quality_mod == "°");
    }

    SECTION("Quality modifier augmented") {
        auto r = parse_roman_numeral_full("III+");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 2);
        REQUIRE(r->quality_mod == "+");
    }

    SECTION("Extension parsed") {
        auto r = parse_roman_numeral_full("V7");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 4);
        REQUIRE(r->extension == "7");
    }
}

TEST_CASE("HRRN001A: Chromatic alterations (§6.2)", "[harmony][core]") {
    SECTION("bVII parsed correctly") {
        auto r = parse_roman_numeral_full("bVII");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 6);
        REQUIRE(r->is_upper == true);
        REQUIRE(r->accidental == -1);
    }

    SECTION("#IV parsed correctly") {
        auto r = parse_roman_numeral_full("#IV");
        REQUIRE(r.has_value());
        REQUIRE(r->degree == 3);
        REQUIRE(r->is_upper == true);
        REQUIRE(r->accidental == 1);
    }

    SECTION("bVII chord in C major = Bb major") {
        auto result = generate_chord_from_numeral("bVII", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 10);  // Bb
        REQUIRE(result->quality == "major");
        // Bb, D, F = 70, 74, 77
        REQUIRE(result->notes[0] == 70);
        REQUIRE(result->notes[1] == 74);
        REQUIRE(result->notes[2] == 77);
    }

    SECTION("bVI chord in C major = Ab major") {
        auto result = generate_chord_from_numeral("bVI", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 8);  // Ab
    }

    SECTION("#IV in C major = F# (pitch class 6)") {
        auto result = generate_chord_from_numeral("#IV", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 6);  // F#
    }
}

TEST_CASE("HRRN001A: Neapolitan (§6.2)", "[harmony][core]") {
    SECTION("N parsed as bII") {
        auto r = parse_roman_numeral_full("N");
        REQUIRE(r.has_value());
        REQUIRE(r->is_neapolitan == true);
        REQUIRE(r->degree == 1);
        REQUIRE(r->accidental == -1);
        REQUIRE(r->is_upper == true);
    }

    SECTION("N chord in C major = Db major") {
        auto result = generate_chord_from_numeral("N", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 1);  // Db
        REQUIRE(result->quality == "major");
    }

    SECTION("N6 = Neapolitan sixth (first inversion)") {
        auto r = parse_roman_numeral_full("N6");
        REQUIRE(r.has_value());
        REQUIRE(r->is_neapolitan == true);
        REQUIRE(r->inversion == FiguredBass::First);
    }

    SECTION("N6 chord is first inversion Db major") {
        auto result = generate_chord_from_numeral("N6", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->inversion == 1);
        // First inversion: F is bass note
        REQUIRE(result->notes[0] % 12 == 5);  // F in bass
    }
}

TEST_CASE("HRRN001A: Figured bass inversions (§6.2)", "[harmony][core]") {
    SECTION("I6 = first inversion") {
        auto r = parse_roman_numeral_full("I6");
        REQUIRE(r.has_value());
        REQUIRE(r->inversion == FiguredBass::First);
    }

    SECTION("I64 = second inversion") {
        auto r = parse_roman_numeral_full("I64");
        REQUIRE(r.has_value());
        REQUIRE(r->inversion == FiguredBass::Second);
    }

    SECTION("V65 = first inversion seventh") {
        auto r = parse_roman_numeral_full("V65");
        REQUIRE(r.has_value());
        REQUIRE(r->inversion == FiguredBass::First);
        REQUIRE(r->extension == "7");
    }

    SECTION("V43 = second inversion seventh") {
        auto r = parse_roman_numeral_full("V43");
        REQUIRE(r.has_value());
        REQUIRE(r->inversion == FiguredBass::Second);
    }

    SECTION("V42 = third inversion seventh") {
        auto r = parse_roman_numeral_full("V42");
        REQUIRE(r.has_value());
        REQUIRE(r->inversion == FiguredBass::Third);
    }

    SECTION("I6 chord in C major: E in bass") {
        auto result = generate_chord_from_numeral("I6", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->inversion == 1);
        REQUIRE(result->notes[0] % 12 == 4);  // E in bass
    }

    SECTION("I64 chord in C major: G in bass") {
        auto result = generate_chord_from_numeral("I64", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->inversion == 2);
        REQUIRE(result->notes[0] % 12 == 7);  // G in bass
    }

    SECTION("V42 chord in C major: F in bass (third inv of G7)") {
        auto result = generate_chord_from_numeral("V42", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->inversion == 3);
        // G7 = G, B, D, F. Third inversion: F in bass.
        REQUIRE(result->notes[0] % 12 == 5);  // F in bass
    }
}

TEST_CASE("HRRN001A: Extended numerals with §6.2 grammar", "[harmony][core]") {
    SECTION("Vmaj7 in C major = G major 7") {
        auto result = generate_chord_from_numeral("Vmaj7", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 7);  // G
        REQUIRE(result->quality == "maj7");
    }

    SECTION("imaj7 in minor = minor-major 7") {
        auto r = parse_roman_numeral_full("imaj7");
        REQUIRE(r.has_value());
        REQUIRE(r->is_upper == false);
        REQUIRE(r->extension.find("maj7") != std::string::npos);
    }

    SECTION("iiø7 = half-diminished") {
        auto result = generate_chord_from_numeral("iiø7", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->quality == "m7b5");
    }

    SECTION("III+7 = augmented seventh") {
        auto result = generate_chord_from_numeral("III+7", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->quality == "aug7");
    }

    SECTION("viio as ASCII diminished") {
        auto result = generate_chord_from_numeral("viio", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->quality == "diminished");
    }

    SECTION("viio7 = fully diminished seventh") {
        auto result = generate_chord_from_numeral("viio7", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->quality == "dim7");
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

// =============================================================================
// §16.1.5 Chord Recognition Round-Trip
// =============================================================================

TEST_CASE("HRRN001A: recognize_chord (§16.1.5)", "[harmony][core]") {
    SECTION("Major triad: {0,4,7} → root=0, quality=major") {
        PitchClassSet pcs = {0, 4, 7};
        auto result = recognize_chord(pcs);
        REQUIRE(result.has_value());
        REQUIRE(result->first == 0);
        REQUIRE(result->second == "major");
    }

    SECTION("Minor triad: {0,3,7} → root=0, quality=minor") {
        PitchClassSet pcs = {0, 3, 7};
        auto result = recognize_chord(pcs);
        REQUIRE(result.has_value());
        REQUIRE(result->first == 0);
        REQUIRE(result->second == "minor");
    }

    SECTION("Transposed chord: G major {7,11,2}") {
        PitchClassSet pcs = {7, 11, 2};
        auto result = recognize_chord(pcs);
        REQUIRE(result.has_value());
        REQUIRE(result->first == 7);
        REQUIRE(result->second == "major");
    }

    SECTION("Dominant 7th: {0,4,7,10}") {
        PitchClassSet pcs = {0, 4, 7, 10};
        auto result = recognize_chord(pcs);
        REQUIRE(result.has_value());
        REQUIRE(result->first == 0);
        REQUIRE(result->second == "7");
    }

    SECTION("Diminished 7th: {0,3,6,9} — symmetric, any root valid") {
        PitchClassSet pcs = {0, 3, 6, 9};
        auto result = recognize_chord(pcs);
        REQUIRE(result.has_value());
        REQUIRE(result->second == "dim7");
        // Root can be any of {0, 3, 6, 9} — all are valid
        REQUIRE((result->first == 0 || result->first == 3 ||
                 result->first == 6 || result->first == 9));
    }

    SECTION("Round-trip: generate → extract PCS → recognize") {
        // Generate a chord, extract its PCS, recognize it
        auto chord = generate_chord(0, "major", 4);
        REQUIRE(chord.has_value());

        PitchClassSet pcs;
        for (auto note : chord->notes) {
            pcs.insert(note % 12);
        }

        auto recognized = recognize_chord(pcs);
        REQUIRE(recognized.has_value());
        REQUIRE(recognized->first == 0);
        REQUIRE(recognized->second == "major");
    }

    SECTION("Round-trip for multiple qualities") {
        // Note: augmented and dim7 excluded — symmetric PCS, root is ambiguous.
        struct Case { PitchClass root; std::string_view quality; };
        Case cases[] = {
            {0, "major"}, {7, "minor"}, {5, "diminished"},
            {2, "7"}, {9, "maj7"}, {11, "m7"},
        };
        for (const auto& c : cases) {
            auto chord = generate_chord(c.root, c.quality, 4);
            REQUIRE(chord.has_value());

            PitchClassSet pcs;
            for (auto note : chord->notes) {
                pcs.insert(note % 12);
            }

            auto recognized = recognize_chord(pcs);
            REQUIRE(recognized.has_value());
            REQUIRE(recognized->first == c.root);
            REQUIRE(recognized->second == c.quality);
        }
    }
}

// =============================================================================
// §16.1.6 Roman Numeral Round-Trip
// =============================================================================

TEST_CASE("HRRN001A: chord_to_numeral (§16.1.6)", "[harmony][core]") {
    SECTION("I in C major") {
        auto result = chord_to_numeral(0, "major", 0, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "I");
    }

    SECTION("V in C major") {
        auto result = chord_to_numeral(7, "major", 0, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "V");
    }

    SECTION("ii in C major") {
        auto result = chord_to_numeral(2, "minor", 0, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "ii");
    }

    SECTION("V7 in C major") {
        auto result = chord_to_numeral(7, "7", 0, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "V7");
    }

    SECTION("IV in G major") {
        // G major: key_root=7, IV = C (root=0)
        auto result = chord_to_numeral(0, "major", 7, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "IV");
    }

    SECTION("bVII in C major") {
        // Bb major in C: root=10, not diatonic, closest is VII (11) → b
        auto result = chord_to_numeral(10, "major", 0, SCALE_MAJOR);
        REQUIRE(result.has_value());
        REQUIRE(*result == "bVII");
    }
}

// =============================================================================
// §16.1.5 Comprehensive chord recognition round-trip (all 12 roots)
// =============================================================================

TEST_CASE("HRRN001A: chord recognition round-trip all roots (§16.1.5)", "[harmony][core]") {
    // Unambiguous qualities: exclude augmented (symmetric, root ambiguous),
    // dim7 (symmetric), 6/m6 (enharmonic with m7/mM7 of different root),
    // sus2/sus4 (Csus4 PCS == Fsus2 PCS — inversional ambiguity)
    std::string_view qualities[] = {
        "major", "minor", "diminished",
        "7", "maj7", "m7", "m7b5", "mM7",
    };

    for (auto quality : qualities) {
        for (PitchClass root = 0; root < 12; ++root) {
            auto chord = generate_chord(root, quality, 4);
            REQUIRE(chord.has_value());

            PitchClassSet pcs;
            for (auto note : chord->notes) {
                pcs.insert(note % 12);
            }

            auto recognized = recognize_chord(pcs);
            REQUIRE(recognized.has_value());
            REQUIRE(recognized->first == root);
            REQUIRE(recognized->second == quality);
        }
    }
}

// =============================================================================
// Audit remediation: is_minor casing and accidental spelling
// =============================================================================

TEST_CASE("HRRN001A: chord_to_numeral uses is_minor for casing",
          "[harmony][core]") {
    // In A minor (key_root=9), a G major chord (root=7) on degree V
    // should produce uppercase "V" because the chord quality is major.
    // The is_minor flag is now consumed rather than suppressed.
    auto result = chord_to_numeral(7, "major", 9, SCALE_MINOR, true);
    REQUIRE(result.has_value());
    // V chord in minor: major quality → uppercase
    CHECK(result->find("V") != std::string::npos);
    // Lowercase 'v' should not appear for a major chord
    CHECK(result->find("v") == std::string::npos);
}

TEST_CASE("HRRN001A: chord_to_numeral labels F# in C major as #IV not bV",
          "[harmony][core]") {
    // F# (pitch class 6) in C major: the tritone between IV and V.
    // Conventionally #IV is preferred over bV.
    auto result = chord_to_numeral(6, "major", 0, SCALE_MAJOR);
    REQUIRE(result.has_value());
    CHECK(*result == "#IV");
}
