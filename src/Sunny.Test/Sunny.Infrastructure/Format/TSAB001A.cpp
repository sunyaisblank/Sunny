/**
 * @file TSAB001A.cpp
 * @brief Unit tests for FMAB001A (ABC notation reader)
 *
 * Component: TSAB001A
 * Domain: TS (Test) | Category: AB (ABC)
 *
 * Tests: FMAB001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMAB001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// Header parsing
// =============================================================================

TEST_CASE("FMAB001A: parse header fields", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "T:Test Tune\n"
        "M:3/4\n"
        "L:1/4\n"
        "Q:140\n"
        "K:C\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->header.title == "Test Tune");
    REQUIRE(r->header.metre_num == 3);
    REQUIRE(r->header.metre_den == 4);
    REQUIRE(r->header.default_length == Beat{1, 4});
    REQUIRE(r->header.tempo_bpm == 140);
}

// =============================================================================
// Key parsing
// =============================================================================

TEST_CASE("FMAB001A: key C major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 0);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("FMAB001A: key Am", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Am\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 9);
    REQUIRE(r->is_minor);
}

TEST_CASE("FMAB001A: key D major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:D\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 2);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("FMAB001A: key Bb major", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Bb\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 10);
    REQUIRE_FALSE(r->is_minor);
}

TEST_CASE("FMAB001A: key F#m", "[abc][format]") {
    auto r = parse_abc("X:1\nK:F#m\n");
    REQUIRE(r.has_value());
    REQUIRE(r->key_root == 6);
    REQUIRE(r->is_minor);
}

// =============================================================================
// Simple melody
// =============================================================================

TEST_CASE("FMAB001A: ascending C-D-E-F-G-A-B-c", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "L:1/4\n"
        "K:C\n"
        "CDEF GABc\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 8);
    REQUIRE(r->notes[0].pitch == 60);  // C4
    REQUIRE(r->notes[1].pitch == 62);  // D4
    REQUIRE(r->notes[2].pitch == 64);  // E4
    REQUIRE(r->notes[3].pitch == 65);  // F4
    REQUIRE(r->notes[4].pitch == 67);  // G4
    REQUIRE(r->notes[5].pitch == 69);  // A4
    REQUIRE(r->notes[6].pitch == 71);  // B4
    REQUIRE(r->notes[7].pitch == 72);  // c5
}

// =============================================================================
// Accidentals
// =============================================================================

TEST_CASE("FMAB001A: explicit accidentals", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "L:1/4\n"
        "K:C\n"
        "^F _B =F\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 3);
    REQUIRE(r->notes[0].pitch == 66);  // F# = 65+1
    REQUIRE(r->notes[1].pitch == 70);  // Bb = 71-1
    REQUIRE(r->notes[2].pitch == 65);  // F natural
}

// =============================================================================
// Octave modifiers
// =============================================================================

TEST_CASE("FMAB001A: octave modifiers", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "L:1/4\n"
        "K:C\n"
        "c' C,\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 2);
    REQUIRE(r->notes[0].pitch == 84);  // c' = C6 (72+12)
    REQUIRE(r->notes[1].pitch == 48);  // C, = C3 (60-12)
}

// =============================================================================
// Duration
// =============================================================================

TEST_CASE("FMAB001A: duration multipliers", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "L:1/4\n"
        "K:C\n"
        "C2 C/2\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 2);
    REQUIRE(r->notes[0].duration == Beat{2, 4});   // double = half note
    REQUIRE(r->notes[1].duration == Beat{1, 8});    // half = eighth note
}

// =============================================================================
// Rests
// =============================================================================

TEST_CASE("FMAB001A: rest", "[abc][format]") {
    std::string abc =
        "X:1\n"
        "L:1/4\n"
        "K:C\n"
        "C z C\n";

    auto r = parse_abc(abc);
    REQUIRE(r.has_value());
    REQUIRE(r->notes.size() == 3);
    REQUIRE(r->notes[1].muted);  // rest is muted
}

// =============================================================================
// 3/4 time
// =============================================================================

TEST_CASE("FMAB001A: 3/4 metre parsing", "[abc][format]") {
    auto r = parse_abc("X:1\nM:3/4\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->header.metre_num == 3);
    REQUIRE(r->header.metre_den == 4);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("FMAB001A: empty body returns empty notes", "[abc][format]") {
    auto r = parse_abc("X:1\nK:C\n");
    REQUIRE(r.has_value());
    REQUIRE(r->notes.empty());
}

TEST_CASE("FMAB001A: invalid key", "[abc][format]") {
    auto r = parse_abc("X:1\nK:Z\n");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidAbcFile);
}

TEST_CASE("FMAB001A: missing K field", "[abc][format]") {
    auto r = parse_abc("X:1\nT:No Key\n");
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidAbcFile);
}

// =============================================================================
// Full tune with bar lines
// =============================================================================

TEST_CASE("FMAB001A: full tune with bar lines", "[abc][format]") {
    std::string abc =
        "X:1\n"
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
