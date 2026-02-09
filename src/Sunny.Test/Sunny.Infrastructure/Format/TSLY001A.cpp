/**
 * @file TSLY001A.cpp
 * @brief Unit tests for FMLY001A (LilyPond notation writer)
 *
 * Component: TSLY001A
 * Domain: TS (Test) | Category: LY (LilyPond)
 *
 * Tests: FMLY001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMLY001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// ly_pitch
// =============================================================================

TEST_CASE("FMLY001A: ly_pitch C4", "[lilypond][format]") {
    REQUIRE(ly_pitch({0, 0, 4}) == "c'");
}

TEST_CASE("FMLY001A: ly_pitch C#4", "[lilypond][format]") {
    REQUIRE(ly_pitch({0, 1, 4}) == "cis'");
}

TEST_CASE("FMLY001A: ly_pitch Bb3", "[lilypond][format]") {
    // B=6, flat=-1, octave 3
    REQUIRE(ly_pitch({6, -1, 3}) == "bes");
}

TEST_CASE("FMLY001A: ly_pitch D##5", "[lilypond][format]") {
    // D=1, double sharp=2, octave 5
    REQUIRE(ly_pitch({1, 2, 5}) == "disis''");
}

TEST_CASE("FMLY001A: ly_pitch Cb2", "[lilypond][format]") {
    // C=0, flat=-1, octave 2
    REQUIRE(ly_pitch({0, -1, 2}) == "ces,");
}

TEST_CASE("FMLY001A: ly_pitch Eb4", "[lilypond][format]") {
    // E=2, flat=-1, octave 4
    REQUIRE(ly_pitch({2, -1, 4}) == "ees'");
}

TEST_CASE("FMLY001A: ly_pitch Ab3", "[lilypond][format]") {
    // A=5, flat=-1, octave 3
    REQUIRE(ly_pitch({5, -1, 3}) == "aes");
}

// =============================================================================
// ly_duration
// =============================================================================

TEST_CASE("FMLY001A: ly_duration quarter", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "4");
}

TEST_CASE("FMLY001A: ly_duration half", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 2});
    REQUIRE(r.has_value());
    REQUIRE(*r == "2");
}

TEST_CASE("FMLY001A: ly_duration whole", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 1});
    REQUIRE(r.has_value());
    REQUIRE(*r == "1");
}

TEST_CASE("FMLY001A: ly_duration dotted quarter", "[lilypond][format]") {
    auto r = ly_duration(Beat{3, 8});
    REQUIRE(r.has_value());
    REQUIRE(*r == "4.");
}

TEST_CASE("FMLY001A: ly_duration eighth", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 8});
    REQUIRE(r.has_value());
    REQUIRE(*r == "8");
}

TEST_CASE("FMLY001A: ly_duration unsupported", "[lilypond][format]") {
    // 5/8 is not a standard LilyPond duration
    auto r = ly_duration(Beat{5, 8});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::FormatError);
}

// =============================================================================
// ly_note
// =============================================================================

TEST_CASE("FMLY001A: ly_note C4 quarter", "[lilypond][format]") {
    auto r = ly_note({0, 0, 4}, Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "c'4");
}

TEST_CASE("FMLY001A: ly_note F#5 half", "[lilypond][format]") {
    auto r = ly_note({3, 1, 5}, Beat{1, 2});
    REQUIRE(r.has_value());
    REQUIRE(*r == "fis''2");
}

// =============================================================================
// ly_rest
// =============================================================================

TEST_CASE("FMLY001A: ly_rest quarter", "[lilypond][format]") {
    auto r = ly_rest(Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "r4");
}

TEST_CASE("FMLY001A: ly_rest whole", "[lilypond][format]") {
    auto r = ly_rest(Beat{1, 1});
    REQUIRE(r.has_value());
    REQUIRE(*r == "r1");
}

// =============================================================================
// ly_chord
// =============================================================================

TEST_CASE("FMLY001A: ly_chord C-E-G quarter", "[lilypond][format]") {
    std::array<SpelledPitch, 3> pitches = {
        SpelledPitch{0, 0, 4},  // C4
        SpelledPitch{2, 0, 4},  // E4
        SpelledPitch{4, 0, 4},  // G4
    };
    auto r = ly_chord(pitches, Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "<c' e' g'>4");
}

// =============================================================================
// ly_key
// =============================================================================

TEST_CASE("FMLY001A: ly_key C major", "[lilypond][format]") {
    REQUIRE(ly_key({0, 0, 4}, true) == "\\key c \\major");
}

TEST_CASE("FMLY001A: ly_key A minor", "[lilypond][format]") {
    REQUIRE(ly_key({5, 0, 4}, false) == "\\key a \\minor");
}

TEST_CASE("FMLY001A: ly_key Eb major", "[lilypond][format]") {
    REQUIRE(ly_key({2, -1, 4}, true) == "\\key ees \\major");
}

// =============================================================================
// ly_time_signature
// =============================================================================

TEST_CASE("FMLY001A: ly_time_signature 4/4", "[lilypond][format]") {
    REQUIRE(ly_time_signature(4, 4) == "\\time 4/4");
}

TEST_CASE("FMLY001A: ly_time_signature 6/8", "[lilypond][format]") {
    REQUIRE(ly_time_signature(6, 8) == "\\time 6/8");
}

// =============================================================================
// ly_fragment
// =============================================================================

TEST_CASE("FMLY001A: ly_fragment ascending C major", "[lilypond][format]") {
    // C4 D4 E4 F4, all quarter notes
    std::array<NoteEvent, 4> events = {
        NoteEvent{60, Beat{0, 1}, Beat{1, 4}, 80},
        NoteEvent{62, Beat{1, 4}, Beat{1, 4}, 80},
        NoteEvent{64, Beat{2, 4}, Beat{1, 4}, 80},
        NoteEvent{65, Beat{3, 4}, Beat{1, 4}, 80},
    };
    auto r = ly_fragment(events, 0);
    REQUIRE(r.has_value());
    REQUIRE(*r == "c'4 d'4 e'4 f'4");
}
