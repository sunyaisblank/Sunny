/**
 * @file lilypond_test.cpp
 * @brief LilyPond notation writer unit tests
 *
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <sunny/infrastructure/formats/lilypond.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// ly_pitch
// =============================================================================

TEST_CASE("ly_pitch C4", "[lilypond][format]") {
    const auto pitch = ly_pitch({0, 0, 4});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "c'");
}

TEST_CASE("ly_pitch C#4", "[lilypond][format]") {
    const auto pitch = ly_pitch({0, 1, 4});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "cis'");
}

TEST_CASE("ly_pitch Bb3", "[lilypond][format]") {
    // B=6, flat=-1, octave 3
    const auto pitch = ly_pitch({6, -1, 3});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "bes");
}

TEST_CASE("ly_pitch D##5", "[lilypond][format]") {
    // D=1, double sharp=2, octave 5
    const auto pitch = ly_pitch({1, 2, 5});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "disis''");
}

TEST_CASE("ly_pitch Cb2", "[lilypond][format]") {
    // C=0, flat=-1, octave 2
    const auto pitch = ly_pitch({0, -1, 2});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "ces,");
}

TEST_CASE("ly_pitch Eb4", "[lilypond][format]") {
    // E=2, flat=-1, octave 4
    const auto pitch = ly_pitch({2, -1, 4});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "ees'");
}

TEST_CASE("ly_pitch Ab3", "[lilypond][format]") {
    // A=5, flat=-1, octave 3
    const auto pitch = ly_pitch({5, -1, 3});
    REQUIRE(pitch.has_value());
    REQUIRE(*pitch == "aes");
}

TEST_CASE("ly_pitch rejects an invalid letter instead of substituting C",
          "[lilypond][format][trust-boundary]") {
    REQUIRE_FALSE(ly_pitch({7, 0, 4}).has_value());
}

// =============================================================================
// ly_duration
// =============================================================================

TEST_CASE("ly_duration quarter", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "4");
}

TEST_CASE("ly_duration half", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 2});
    REQUIRE(r.has_value());
    REQUIRE(*r == "2");
}

TEST_CASE("ly_duration whole", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 1});
    REQUIRE(r.has_value());
    REQUIRE(*r == "1");
}

TEST_CASE("ly_duration dotted quarter", "[lilypond][format]") {
    auto r = ly_duration(Beat{3, 8});
    REQUIRE(r.has_value());
    REQUIRE(*r == "4.");
}

TEST_CASE("ly_duration eighth", "[lilypond][format]") {
    auto r = ly_duration(Beat{1, 8});
    REQUIRE(r.has_value());
    REQUIRE(*r == "8");
}

TEST_CASE("ly_duration preserves arbitrary and multiply-dotted rational values",
          "[lilypond][format][duration]") {
    auto arbitrary = ly_duration(Beat{5, 8});
    REQUIRE(arbitrary.has_value());
    REQUIRE(*arbitrary == "1*5/8");

    auto double_dotted = ly_duration(Beat{7, 16});
    REQUIRE(double_dotted.has_value());
    REQUIRE(*double_dotted == "4..");

    auto maxima = ly_duration(Beat{8, 1});
    REQUIRE(maxima.has_value());
    REQUIRE(*maxima == "\\maxima");
}

TEST_CASE("ly_duration rejects non-positive rational state", "[lilypond][format][trust-boundary]") {
    REQUIRE_FALSE(ly_duration(Beat::zero()).has_value());
    REQUIRE_FALSE(ly_duration(Beat{-1, 4}).has_value());
}

// =============================================================================
// ly_note
// =============================================================================

TEST_CASE("ly_note C4 quarter", "[lilypond][format]") {
    auto r = ly_note({0, 0, 4}, Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "c'4");
}

TEST_CASE("ly_note F#5 half", "[lilypond][format]") {
    auto r = ly_note({3, 1, 5}, Beat{1, 2});
    REQUIRE(r.has_value());
    REQUIRE(*r == "fis''2");
}

// =============================================================================
// ly_rest
// =============================================================================

TEST_CASE("ly_rest quarter", "[lilypond][format]") {
    auto r = ly_rest(Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "r4");
}

TEST_CASE("ly_rest whole", "[lilypond][format]") {
    auto r = ly_rest(Beat{1, 1});
    REQUIRE(r.has_value());
    REQUIRE(*r == "r1");
}

// =============================================================================
// ly_chord
// =============================================================================

TEST_CASE("ly_chord C-E-G quarter", "[lilypond][format]") {
    std::array<SpelledPitch, 3> pitches = {
        SpelledPitch{0, 0, 4}, // C4
        SpelledPitch{2, 0, 4}, // E4
        SpelledPitch{4, 0, 4}, // G4
    };
    auto r = ly_chord(pitches, Beat{1, 4});
    REQUIRE(r.has_value());
    REQUIRE(*r == "<c' e' g'>4");
}

TEST_CASE("ly_chord rejects an empty or invalidly spelled chord",
          "[lilypond][format][trust-boundary]") {
    const std::array<SpelledPitch, 0> empty{};
    CHECK_FALSE(ly_chord(empty, Beat{1, 4}).has_value());
    const std::array invalid = {SpelledPitch{7, 0, 4}};
    CHECK_FALSE(ly_chord(invalid, Beat{1, 4}).has_value());
}

// =============================================================================
// ly_key
// =============================================================================

TEST_CASE("ly_key C major", "[lilypond][format]") {
    const auto key = ly_key({0, 0, 4}, true);
    REQUIRE(key.has_value());
    REQUIRE(*key == "\\key c \\major");
}

TEST_CASE("ly_key A minor", "[lilypond][format]") {
    const auto key = ly_key({5, 0, 4}, false);
    REQUIRE(key.has_value());
    REQUIRE(*key == "\\key a \\minor");
}

TEST_CASE("ly_key Eb major", "[lilypond][format]") {
    const auto key = ly_key({2, -1, 4}, true);
    REQUIRE(key.has_value());
    REQUIRE(*key == "\\key ees \\major");
}

TEST_CASE("ly_key rejects an invalid tonic", "[lilypond][format][trust-boundary]") {
    REQUIRE_FALSE(ly_key({7, 0, 4}, true).has_value());
}

// =============================================================================
// ly_time_signature
// =============================================================================

TEST_CASE("ly_time_signature 4/4", "[lilypond][format]") {
    const auto time = ly_time_signature(4, 4);
    REQUIRE(time.has_value());
    REQUIRE(*time == "\\time 4/4");
}

TEST_CASE("ly_time_signature 6/8", "[lilypond][format]") {
    const auto time = ly_time_signature(6, 8);
    REQUIRE(time.has_value());
    REQUIRE(*time == "\\time 6/8");
}

TEST_CASE("ly_time_signature rejects non-positive fields", "[lilypond][format][trust-boundary]") {
    CHECK_FALSE(ly_time_signature(0, 4).has_value());
    CHECK_FALSE(ly_time_signature(4, 0).has_value());
}

// =============================================================================
// ly_fragment
// =============================================================================

TEST_CASE("ly_fragment ascending C major", "[lilypond][format]") {
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

TEST_CASE("ly_fragment preserves gaps, muted rests, and arbitrary duration",
          "[lilypond][format][timing]") {
    const std::array<NoteEvent, 3> events = {
        NoteEvent{60, Beat{1, 4}, Beat{1, 4}, 80},
        NoteEvent{0, Beat{1, 2}, Beat{1, 8}, 0, true},
        NoteEvent{62, Beat{3, 4}, Beat{5, 8}, 80},
    };
    const auto fragment = ly_fragment(events);
    REQUIRE(fragment.has_value());
    CHECK(*fragment == "r4 c'4 r8 r8 d'1*5/8");
}

TEST_CASE("ly_fragment rejects malformed, overlapping, or reordered event time",
          "[lilypond][format][timing][trust-boundary]") {
    CHECK_FALSE(ly_fragment(std::array{NoteEvent{60, Beat{-1, 4}, Beat{1, 4}, 80}}).has_value());
    CHECK_FALSE(ly_fragment(std::array{NoteEvent{60, Beat::zero(), Beat::zero(), 80}}).has_value());
    CHECK_FALSE(ly_fragment(std::array{
                                NoteEvent{60, Beat::zero(), Beat{1, 2}, 80},
                                NoteEvent{64, Beat{1, 4}, Beat{1, 4}, 80},
                            })
                    .has_value());
}

TEST_CASE("ly_fragment rejects non-default release velocity",
          "[lilypond][format][release-velocity][trust-boundary]") {
    auto event = NoteEvent{60, Beat::zero(), Beat{1, 4}, 80};
    event.release_velocity = 23;
    const auto fragment = ly_fragment(std::array{event});
    REQUIRE_FALSE(fragment.has_value());
    CHECK(fragment.error() == ErrorCode::TargetValueUnrepresentable);
}
