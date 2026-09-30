/**
 * @file abc_test.cpp
 * @brief ABC notation reader unit tests
 *
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/infrastructure/formats/abc.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// Header parsing
// =============================================================================

TEST_CASE("parse header fields", "[abc][format]") {
    std::string abc = "X:1\n"
                      "T:Test Tune\n"
                      "M:3/4\n"
                      "L:1/4\n"
                      "Q:140\n"
                      "K:C\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->header.reference_number == 1);
    REQUIRE(r->header.title == "Test Tune");
    REQUIRE(r->header.metre_num == 3);
    REQUIRE(r->header.metre_den == 4);
    REQUIRE(r->header.default_length == Beat{1, 4});
    REQUIRE(r->header.tempo_bpm == 140);
    REQUIRE(r->header.tempo_unit == Beat{1, 4});
    REQUIRE(r->header.metre_explicit);
    REQUIRE_FALSE(r->header.free_metre);
    REQUIRE(r->header.default_length_explicit);
}

TEST_CASE("ABC compact header retains tune identity and descriptive fields",
          "[abc][format][header]") {
    const auto parsed = parse_abc("X:9223372036854775807\n"
                                  "C:First Composer\n"
                                  "N:First note\n"
                                  "C:Second Composer\n"
                                  "K:C\n");
    REQUIRE(parsed.has_value());
    CHECK(parsed->header.reference_number == std::numeric_limits<std::int64_t>::max());
    REQUIRE(parsed->header.descriptive_fields.size() == 3);
    CHECK(parsed->header.descriptive_fields[0].code == 'C');
    CHECK(parsed->header.descriptive_fields[0].value == "First Composer");
    CHECK(parsed->header.descriptive_fields[1].code == 'N');
    CHECK(parsed->header.descriptive_fields[1].value == "First note");
    CHECK(parsed->header.descriptive_fields[2].code == 'C');
    CHECK(parsed->header.descriptive_fields[2].value == "Second Composer");
}

TEST_CASE("ABC compact header preserves free metre, derived length, and tempo unit",
          "[abc][format][header]") {
    SECTION("absent metre is free and uses the specified ABC default") {
        const auto parsed = parse_abc("X:1\nK:C\n");
        REQUIRE(parsed.has_value());
        CHECK_FALSE(parsed->header.metre_explicit);
        CHECK(parsed->header.free_metre);
        CHECK(parsed->header.metre_num == 0);
        CHECK(parsed->header.metre_den == 0);
        CHECK(parsed->header.default_length == Beat{1, 8});
        CHECK_FALSE(parsed->header.tempo_bpm.has_value());
    }

    SECTION("meter below three quarters derives a sixteenth unit length") {
        const auto parsed = parse_abc("X:1\nM:2/4\nK:C\nC\n");
        REQUIRE(parsed.has_value());
        CHECK(parsed->header.default_length == Beat{1, 16});
        REQUIRE(parsed->notes.size() == 1);
        CHECK(parsed->notes[0].duration == Beat{1, 16});
    }

    SECTION("explicit free metre and explicit tempo beat unit survive") {
        const auto parsed = parse_abc("X:1\nM:none\nQ:1/2=120\nK:C\n");
        REQUIRE(parsed.has_value());
        CHECK(parsed->header.metre_explicit);
        CHECK(parsed->header.free_metre);
        CHECK(parsed->header.tempo_bpm == 120);
        CHECK(parsed->header.tempo_unit == Beat{1, 2});
    }

    SECTION("deprecated bare tempo is explicitly relative to the unit note length") {
        const auto parsed = parse_abc("X:1\nL:1/8\nQ:90\nK:C\n");
        REQUIRE(parsed.has_value());
        CHECK(parsed->header.tempo_bpm == 90);
        CHECK(parsed->header.tempo_unit == Beat{1, 8});
    }
}

TEST_CASE("ABC recognized header fields fail closed on malformed or richer values",
          "[abc][format][header][trust-boundary]") {
    for (const auto* abc : {
             "X:1\nM:3/x\nK:C\n",
             "X:1\nM:3/4junk\nK:C\n",
             "X:1\nM:0/4\nK:C\n",
             "X:1\nM:(2+3)/8\nK:C\n",
             "X:1\nL:0\nK:C\n",
             "X:1\nL:1/0\nK:C\n",
             "X:1\nL:999999999999999999999999\nK:C\n",
             "X:1\nQ:fast\nK:C\n",
             "X:1\nQ:1/4=0\nK:C\n",
             "X:1\nQ:1/4 3/8=40\nK:C\n",
             "X:1\nM:4/4\nM:3/4\nK:C\n",
             "X:1\nL:1/8\nL:1/4\nK:C\n",
             "X:1\nQ:90\nQ:100\nK:C\n",
         }) {
        CAPTURE(abc);
        CHECK_FALSE(parse_abc(abc).has_value());
    }
}

// =============================================================================
// Key parsing
// =============================================================================

TEST_CASE("key C major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 0);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("key Am", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Am\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 9);
    REQUIRE(r->is_minor);
}

TEST_CASE("key D major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:D\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 2);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("key Bb major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Bb\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 10);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("key F#m", "[abc][format]") {
    auto r = parse_abc("X:1\nK:F#m\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 6);
    REQUIRE(r->is_minor);
}

TEST_CASE("ABC church modes derive their exact traditional signatures", "[abc][format][key]") {
    const auto dorian = parse_abc("X:1\nK:DDor\nF B\n");
    REQUIRE(dorian.has_value());
    CHECK(dorian->key_tonic == SpelledPitch{1, 0, 4});
    CHECK(dorian->key_mode == "dorian");
    CHECK(dorian->key_fifths == 0);
    CHECK_FALSE(dorian->is_minor);
    REQUIRE(dorian->notes.size() == 2);
    CHECK(dorian->notes[0].pitch == 65); // F natural, not D-major F-sharp
    CHECK(dorian->notes[1].pitch == 71); // B natural

    const auto mixolydian = parse_abc("X:1\nK:G mixolydian\nF\n");
    REQUIRE(mixolydian.has_value());
    CHECK(mixolydian->key_mode == "mixolydian");
    CHECK(mixolydian->key_fifths == 0);
    REQUIRE(mixolydian->notes.size() == 1);
    CHECK(mixolydian->notes[0].pitch == 65);
}

TEST_CASE("ABC unsupported key modifiers are rejected instead of read as major",
          "[abc][format][key][trust-boundary]") {
    CHECK_FALSE(parse_abc("X:1\nK:D unknown\nF\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:D Phr ^f\nF\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:D clef=bass\nF\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:none\nF\n").has_value());
}

// =============================================================================
// Simple melody
// =============================================================================

TEST_CASE("ascending C-D-E-F-G-A-B-c", "[abc][format]") {
    std::string abc = "X:1\n"
                      "L:1/4\n"
                      "K:C\n"
                      "CDEF GABc\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 8);
    REQUIRE(r->notes[0].pitch == 60); // C4
    REQUIRE(r->notes[1].pitch == 62); // D4
    REQUIRE(r->notes[2].pitch == 64); // E4
    REQUIRE(r->notes[3].pitch == 65); // F4
    REQUIRE(r->notes[4].pitch == 67); // G4
    REQUIRE(r->notes[5].pitch == 69); // A4
    REQUIRE(r->notes[6].pitch == 71); // B4
    REQUIRE(r->notes[7].pitch == 72); // c5
}

// =============================================================================
// Accidentals
// =============================================================================

TEST_CASE("explicit accidentals", "[abc][format]") {
    std::string abc = "X:1\n"
                      "L:1/4\n"
                      "K:C\n"
                      "^F _B =F\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 3);
    REQUIRE(r->notes[0].pitch == 66); // F# = 65+1
    REQUIRE(r->notes[1].pitch == 70); // Bb = 71-1
    REQUIRE(r->notes[2].pitch == 65); // F natural
}

// =============================================================================
// Octave modifiers
// =============================================================================

TEST_CASE("octave modifiers", "[abc][format]") {
    std::string abc = "X:1\n"
                      "L:1/4\n"
                      "K:C\n"
                      "c' C,\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 2);
    REQUIRE(r->notes[0].pitch == 84); // c' = C6 (72+12)
    REQUIRE(r->notes[1].pitch == 48); // C, = C3 (60-12)
}

// =============================================================================
// Duration
// =============================================================================

TEST_CASE("duration multipliers", "[abc][format]") {
    std::string abc = "X:1\n"
                      "L:1/4\n"
                      "K:C\n"
                      "C2 C/2\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 2);
    REQUIRE(r->notes[0].duration == Beat{2, 4}); // double = half note
    REQUIRE(r->notes[1].duration == Beat{1, 8}); // half = eighth note
}

TEST_CASE("ABC note-length grammar is exact and checked", "[abc][format][duration]") {
    const auto parsed = parse_abc("X:1\nL:1/8\nK:C\nC// C3/2 z///\n");
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->notes.size() == 3);
    CHECK(parsed->notes[0].duration == Beat{1, 32});
    CHECK(parsed->notes[1].duration == Beat{3, 16});
    CHECK(parsed->notes[2].duration == Beat{1, 64});

    for (const auto* abc : {
             "X:1\nK:C\nC0\n",
             "X:1\nK:C\nC/0\n",
             "X:1\nK:C\nC//2\n",
             "X:1\nK:C\nC999999999999999999999999\n",
             "X:1\nL:1\nK:C\nC9223372036854775807 C\n",
         }) {
        CAPTURE(abc);
        CHECK_FALSE(parse_abc(abc).has_value());
    }
}

// =============================================================================
// Rests
// =============================================================================

TEST_CASE("rest", "[abc][format]") {
    std::string abc = "X:1\n"
                      "L:1/4\n"
                      "K:C\n"
                      "C z C\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 3);
    REQUIRE(r->notes[1].muted); // rest is muted
}

// =============================================================================
// 3/4 time
// =============================================================================

TEST_CASE("3/4 metre parsing", "[abc][format]") {
    auto r = parse_abc("X:1\nM:3/4\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->header.metre_num == 3);
    REQUIRE(r->header.metre_den == 4);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("empty body returns empty notes", "[abc][format]") {
    auto r = parse_abc("X:1\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->notes.empty());
}

TEST_CASE("invalid key", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Z\n");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidAbcFile);
}

TEST_CASE("ABC pitch target range and accidental grammar fail closed",
          "[abc][format][pitch][trust-boundary]") {
    CHECK_FALSE(parse_abc("X:1\nK:C\nC,,,,,,\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:C\nc'''''\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:C\n^_C\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nK:C\n^^^C\n").has_value());

    const auto exact = parse_abc("X:1\nK:C\n^^F __B C,'\n");
    REQUIRE(exact.has_value());
    REQUIRE(exact->notes.size() == 3);
    CHECK(exact->notes[0].pitch == 67);
    CHECK(exact->notes[1].pitch == 69);
    CHECK(exact->notes[2].pitch == 60);
}

TEST_CASE("ABC comments are non-sounding and unsupported body algebras are rejected",
          "[abc][format][trust-boundary]") {
    const auto commented = parse_abc("X:1\n% CDE is a header comment\nK:C\nC % DEF\n");
    REQUIRE(commented.has_value());
    REQUIRE(commented->notes.size() == 1);
    CHECK(commented->notes[0].pitch == 60);

    for (const auto* abc : {
             "X:1\nK:C\n[CEG]\n",
             "X:1\nK:C\n\"Am\"C\n",
             "X:1\nK:C\nC>D\n",
             "X:1\nK:C\n(3ABC\n",
             "X:1\nK:C\n|:C:|\n",
             "X:1\nK:C\nQ:120\nC\n",
             "X:1\nK:C\nZ2\n",
             "X:1\nK:C\nC-\n",
         }) {
        CAPTURE(abc);
        CHECK_FALSE(parse_abc(abc).has_value());
    }
}

TEST_CASE("missing K field", "[abc][format]") {
    auto r = parse_abc("X:1\nT:No Key\n");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidAbcFile);
}

TEST_CASE("ABC complete-tune envelope requires one positive reference field",
          "[abc][format][trust-boundary]") {
    CHECK_FALSE(parse_abc("K:C\n").has_value());
    CHECK_FALSE(parse_abc("X:0\nK:C\n").has_value());
    CHECK_FALSE(parse_abc("X:not-a-number\nK:C\n").has_value());
    CHECK_FALSE(parse_abc("X:1\nX:2\nK:C\n").has_value());
}

// =============================================================================
// Full tune with bar lines
// =============================================================================

TEST_CASE("full tune with bar lines", "[abc][format]") {
    std::string abc = "X:1\n"
                      "T:Simple\n"
                      "M:4/4\n"
                      "L:1/4\n"
                      "K:C\n"
                      "CDEF|GABc|\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 8);
    REQUIRE(r->header.title == "Simple");
}
