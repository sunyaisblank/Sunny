/**
 * @file TSSP001A.cpp
 * @brief Unit tests for PTSP001A (Spelled pitch operations)
 *
 * Component: TSSP001A
 * Domain: TS (Test) | Category: SP (Spelled Pitch)
 *
 * Tests: PTSP001A
 * Coverage: nat, pc, midi_value, midi, enharmonic_equivalent,
 *           line_of_fifths_position, from_line_of_fifths, to_spn, from_spn,
 *           default_spelling
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTSP001A.h"

using namespace Sunny::Core;

// =============================================================================
// nat() — natural pitch class lookup
// =============================================================================

TEST_CASE("PTSP001A: nat() maps letters to natural pitch classes", "[pitch][spelled]") {
    CHECK(nat(0) == 0);   // C
    CHECK(nat(1) == 2);   // D
    CHECK(nat(2) == 4);   // E
    CHECK(nat(3) == 5);   // F
    CHECK(nat(4) == 7);   // G
    CHECK(nat(5) == 9);   // A
    CHECK(nat(6) == 11);  // B
}

// =============================================================================
// pc() — pitch class from spelled pitch
// =============================================================================

TEST_CASE("PTSP001A: pc() computes pitch class correctly", "[pitch][spelled]") {
    SECTION("Natural notes") {
        CHECK(pc({0, 0, 4}) == 0);   // C4
        CHECK(pc({1, 0, 4}) == 2);   // D4
        CHECK(pc({2, 0, 4}) == 4);   // E4
        CHECK(pc({3, 0, 4}) == 5);   // F4
        CHECK(pc({4, 0, 4}) == 7);   // G4
        CHECK(pc({5, 0, 4}) == 9);   // A4
        CHECK(pc({6, 0, 4}) == 11);  // B4
    }

    SECTION("Sharps") {
        CHECK(pc({0, 1, 4}) == 1);   // C#4
        CHECK(pc({3, 1, 4}) == 6);   // F#4
        CHECK(pc({4, 1, 4}) == 8);   // G#4
    }

    SECTION("Flats") {
        CHECK(pc({1, -1, 4}) == 1);  // Db4
        CHECK(pc({2, -1, 4}) == 3);  // Eb4
        CHECK(pc({6, -1, 4}) == 10); // Bb4
    }

    SECTION("Double sharps and flats") {
        CHECK(pc({0, 2, 4}) == 2);   // C##4 = D
        CHECK(pc({1, -2, 4}) == 0);  // Dbb4 = C
    }

    SECTION("Wraparound") {
        CHECK(pc({6, 1, 4}) == 0);   // B#4 = C
        CHECK(pc({0, -1, 4}) == 11); // Cb4 = B
    }
}

// =============================================================================
// midi_value() and midi()
// =============================================================================

TEST_CASE("PTSP001A: midi_value() and midi() correctness", "[pitch][spelled]") {
    SECTION("Standard reference pitches") {
        CHECK(midi_value({0, 0, 4}) == 60);   // C4 = Middle C
        CHECK(midi_value({5, 0, 4}) == 69);   // A4 = 440 Hz
        CHECK(midi_value({0, 0, -1}) == 0);   // C-1 = lowest MIDI
    }

    SECTION("Accidentals shift value") {
        CHECK(midi_value({0, 1, 4}) == 61);   // C#4
        CHECK(midi_value({0, -1, 4}) == 59);  // Cb4
        CHECK(midi_value({0, 2, 4}) == 62);   // C##4
    }

    SECTION("Checked midi() bounds") {
        CHECK(midi({0, 0, 4}).has_value());
        CHECK(*midi({0, 0, 4}) == 60);

        CHECK(midi({0, 0, -1}).has_value());
        CHECK(*midi({0, 0, -1}) == 0);

        // Out of range
        CHECK(!midi({0, -1, -1}).has_value());  // Cb-1 = -1
    }
}

// =============================================================================
// enharmonic_equivalent()
// =============================================================================

TEST_CASE("PTSP001A: enharmonic_equivalent detects equivalent spellings", "[pitch][spelled]") {
    SECTION("Equivalent pairs") {
        CHECK(enharmonic_equivalent({0, 1, 4}, {1, -1, 4}));  // C#4 == Db4
        CHECK(enharmonic_equivalent({6, 1, 3}, {0, 0, 4}));   // B#3 == C4
        CHECK(enharmonic_equivalent({0, -1, 5}, {6, 0, 4}));  // Cb5 == B4
        CHECK(enharmonic_equivalent({2, 1, 4}, {3, 0, 4}));   // E#4 == F4
    }

    SECTION("Non-equivalent pairs") {
        CHECK_FALSE(enharmonic_equivalent({0, 0, 4}, {1, 0, 4}));  // C4 != D4
        CHECK_FALSE(enharmonic_equivalent({0, 1, 4}, {1, -1, 5})); // C#4 != Db5
    }
}

// =============================================================================
// line_of_fifths_position()
// =============================================================================

TEST_CASE("PTSP001A: line_of_fifths_position maps correctly", "[pitch][spelled]") {
    SECTION("Natural notes") {
        CHECK(line_of_fifths_position({3, 0, 4}) == -1);  // F
        CHECK(line_of_fifths_position({0, 0, 4}) == 0);   // C
        CHECK(line_of_fifths_position({4, 0, 4}) == 1);   // G
        CHECK(line_of_fifths_position({1, 0, 4}) == 2);   // D
        CHECK(line_of_fifths_position({5, 0, 4}) == 3);   // A
        CHECK(line_of_fifths_position({2, 0, 4}) == 4);   // E
        CHECK(line_of_fifths_position({6, 0, 4}) == 5);   // B
    }

    SECTION("Sharps") {
        CHECK(line_of_fifths_position({3, 1, 4}) == 6);   // F#
        CHECK(line_of_fifths_position({0, 1, 4}) == 7);   // C#
        CHECK(line_of_fifths_position({4, 1, 4}) == 8);   // G#
    }

    SECTION("Flats") {
        CHECK(line_of_fifths_position({6, -1, 4}) == -2);  // Bb
        CHECK(line_of_fifths_position({2, -1, 4}) == -3);  // Eb
        CHECK(line_of_fifths_position({5, -1, 4}) == -4);  // Ab
    }
}

// =============================================================================
// from_line_of_fifths() round-trip
// =============================================================================

TEST_CASE("PTSP001A: from_line_of_fifths round-trip", "[pitch][spelled]") {
    // For a range of LoF positions, verify:
    // line_of_fifths_position(from_line_of_fifths(q, oct)) == q
    for (int q = -14; q <= 14; ++q) {
        SpelledPitch sp = from_line_of_fifths(q, 4);
        INFO("LoF position q=" << q << " -> letter=" << (int)sp.letter
             << " acc=" << (int)sp.accidental);
        CHECK(line_of_fifths_position(sp) == q);
    }
}

TEST_CASE("PTSP001A: from_line_of_fifths specific values", "[pitch][spelled]") {
    // LoF 0 = C
    auto c = from_line_of_fifths(0, 4);
    CHECK(c.letter == 0);
    CHECK(c.accidental == 0);

    // LoF 7 = C#
    auto cs = from_line_of_fifths(7, 4);
    CHECK(cs.letter == 0);
    CHECK(cs.accidental == 1);

    // LoF -1 = F
    auto f = from_line_of_fifths(-1, 4);
    CHECK(f.letter == 3);
    CHECK(f.accidental == 0);

    // LoF -5 = Db
    auto db = from_line_of_fifths(-5, 4);
    CHECK(db.letter == 1);
    CHECK(db.accidental == -1);
}

// =============================================================================
// to_spn() formatting
// =============================================================================

TEST_CASE("PTSP001A: to_spn formatting", "[pitch][spelled]") {
    CHECK(to_spn({0, 0, 4}).value() == "C4");
    CHECK(to_spn({5, 0, 4}).value() == "A4");
    CHECK(to_spn({0, 1, 4}).value() == "C#4");
    CHECK(to_spn({6, -1, 3}).value() == "Bb3");
    CHECK(to_spn({1, 2, 5}).value() == "D##5");
    CHECK(to_spn({2, -2, 3}).value() == "Ebb3");
    CHECK(to_spn({0, 0, -1}).value() == "C-1");
    CHECK(to_spn({4, 0, 0}).value() == "G0");
}

// =============================================================================
// from_spn() parsing
// =============================================================================

TEST_CASE("PTSP001A: from_spn parsing", "[pitch][spelled]") {
    SECTION("Basic notes") {
        auto r = from_spn("C4");
        REQUIRE(r.has_value());
        CHECK(r->letter == 0);
        CHECK(r->accidental == 0);
        CHECK(r->octave == 4);
    }

    SECTION("Sharps and flats") {
        auto cs = from_spn("C#4");
        REQUIRE(cs.has_value());
        CHECK(cs->letter == 0);
        CHECK(cs->accidental == 1);

        auto bb = from_spn("Bb3");
        REQUIRE(bb.has_value());
        CHECK(bb->letter == 6);
        CHECK(bb->accidental == -1);
        CHECK(bb->octave == 3);
    }

    SECTION("Double accidentals") {
        auto dss = from_spn("D##5");
        REQUIRE(dss.has_value());
        CHECK(dss->letter == 1);
        CHECK(dss->accidental == 2);

        auto ebb = from_spn("Ebb3");
        REQUIRE(ebb.has_value());
        CHECK(ebb->letter == 2);
        CHECK(ebb->accidental == -2);
    }

    SECTION("Double sharp via x") {
        auto dx = from_spn("Dx4");
        REQUIRE(dx.has_value());
        CHECK(dx->letter == 1);
        CHECK(dx->accidental == 2);
    }

    SECTION("Negative octave") {
        auto cm1 = from_spn("C-1");
        REQUIRE(cm1.has_value());
        CHECK(cm1->letter == 0);
        CHECK(cm1->octave == -1);
    }

    SECTION("Case insensitive letter") {
        auto r = from_spn("c4");
        REQUIRE(r.has_value());
        CHECK(r->letter == 0);
        CHECK(r->octave == 4);

        auto g = from_spn("g#5");
        REQUIRE(g.has_value());
        CHECK(g->letter == 4);
        CHECK(g->accidental == 1);
    }

    SECTION("Error cases") {
        CHECK(!from_spn("").has_value());
        CHECK(!from_spn("X4").has_value());
        CHECK(!from_spn("C").has_value());   // no octave
        CHECK(!from_spn("H4").has_value());
    }

    SECTION("Round-trip: from_spn(to_spn(sp)) == sp") {
        SpelledPitch cases[] = {
            {0, 0, 4}, {0, 1, 4}, {6, -1, 3}, {1, 2, 5},
            {3, 0, 0}, {0, 0, -1}, {5, -2, 2}
        };
        for (auto sp : cases) {
            auto spn = to_spn(sp);
            REQUIRE(spn.has_value());
            auto rt = from_spn(*spn);
            REQUIRE(rt.has_value());
            CHECK(*rt == sp);
        }
    }
}

// =============================================================================
// default_spelling()
// =============================================================================

TEST_CASE("PTSP001A: default_spelling heuristic", "[pitch][spelled]") {
    SECTION("C major context (key_lof = 0)") {
        // All naturals should be preferred
        CHECK(pc(default_spelling(0, 0, 4)) == 0);   // C
        CHECK(default_spelling(0, 0, 4).accidental == 0);

        // Black keys: prefer sharps in C major context
        auto fs = default_spelling(6, 0, 4);  // F# or Gb
        CHECK(pc(fs) == 6);
        CHECK(fs.letter == 3);  // F#
        CHECK(fs.accidental == 1);
    }

    SECTION("F major context (key_lof = -1)") {
        // Bb should be preferred over A#
        auto bb = default_spelling(10, -1, 4);
        CHECK(pc(bb) == 10);
        CHECK(bb.letter == 6);  // Bb
        CHECK(bb.accidental == -1);
    }

    SECTION("G major context (key_lof = 1)") {
        // F# should be preferred over Gb
        auto fs = default_spelling(6, 1, 4);
        CHECK(pc(fs) == 6);
        CHECK(fs.letter == 3);  // F#
        CHECK(fs.accidental == 1);
    }

    SECTION("Pitch class round-trip") {
        // For every pitch class, default_spelling should produce a note
        // whose pc matches the input
        for (int p = 0; p < 12; ++p) {
            auto sp = default_spelling(static_cast<PitchClass>(p), 0, 4);
            CHECK(pc(sp) == p);
        }
    }
}

// =============================================================================
// is_valid_letter()
// =============================================================================

TEST_CASE("PTSP001A: is_valid_letter boundary check", "[pitch][spelled]") {
    CHECK(is_valid_letter(0));
    CHECK(is_valid_letter(6));
    CHECK_FALSE(is_valid_letter(-1));
    CHECK_FALSE(is_valid_letter(7));
}

// =============================================================================
// Remediation: to_spn error on invalid letter
// =============================================================================

TEST_CASE("PTSP001A: to_spn returns error for invalid letter", "[pitch][spelled]") {
    SpelledPitch invalid{7, 0, 4};
    auto result = to_spn(invalid);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidLetterName);
}

// =============================================================================
// Remediation: from_spn rejects excessive accidentals
// =============================================================================

TEST_CASE("PTSP001A: from_spn rejects excessive accidentals", "[pitch][spelled]") {
    // Build a string with 130 sharps, which overflows int8_t range
    std::string excessive = "C";
    for (int i = 0; i < 130; ++i) excessive += '#';
    excessive += "4";

    auto result = from_spn(excessive);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidSpelledPitch);
}

// =============================================================================
// Remediation: pc produces valid PitchClass for negative midi_value
// =============================================================================

TEST_CASE("PTSP001A: pc produces valid PitchClass for negative midi_value", "[pitch][spelled]") {
    // Cb0 has midi_value = 12*(0+1) + 0 + (-1) = 11, pc should be 11
    SpelledPitch cb0{0, -1, 0};
    CHECK(pc(cb0) == 11);

    // Cb-1 has midi_value = 12*(-1+1) + 0 + (-1) = -1, pc should still be 11
    SpelledPitch cbm1{0, -1, -1};
    CHECK(pc(cbm1) == 11);
}
