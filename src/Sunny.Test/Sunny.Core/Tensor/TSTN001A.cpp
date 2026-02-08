/**
 * @file TSTN001A.cpp
 * @brief Tests for TNTP001A, TNBT001A, TNEV001A
 *
 * Component: TSTN001A
 * Domain: TS (Test) | Category: TN (Tensor)
 *
 * Tests core types, constants, validation, Beat arithmetic,
 * NoteEvent, ChordVoicing, and ScaleDefinition.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Tensor/TNTP001A.h"
#include "Tensor/TNBT001A.h"
#include "Tensor/TNEV001A.h"

#include <set>

using namespace Sunny::Core;

// =============================================================================
// TNTP001A - Validation Functions
// =============================================================================

TEST_CASE("TSTN001A: is_valid_midi_note boundary conditions", "[tensor][types]") {
    REQUIRE(is_valid_midi_note(0));
    REQUIRE(is_valid_midi_note(60));
    REQUIRE(is_valid_midi_note(127));
    REQUIRE_FALSE(is_valid_midi_note(-1));
    REQUIRE_FALSE(is_valid_midi_note(128));
    REQUIRE_FALSE(is_valid_midi_note(-100));
    REQUIRE_FALSE(is_valid_midi_note(256));
}

TEST_CASE("TSTN001A: is_valid_pitch_class boundary conditions", "[tensor][types]") {
    REQUIRE(is_valid_pitch_class(0));
    REQUIRE(is_valid_pitch_class(11));
    REQUIRE(is_valid_pitch_class(6));
    REQUIRE_FALSE(is_valid_pitch_class(-1));
    REQUIRE_FALSE(is_valid_pitch_class(12));
    REQUIRE_FALSE(is_valid_pitch_class(24));
}

TEST_CASE("TSTN001A: is_valid_velocity boundary conditions", "[tensor][types]") {
    REQUIRE(is_valid_velocity(1));
    REQUIRE(is_valid_velocity(64));
    REQUIRE(is_valid_velocity(127));
    REQUIRE_FALSE(is_valid_velocity(0));
    REQUIRE_FALSE(is_valid_velocity(-1));
    REQUIRE_FALSE(is_valid_velocity(128));
}

// =============================================================================
// TNTP001A - Constants
// =============================================================================

TEST_CASE("TSTN001A: Constants correctness", "[tensor][types]") {
    REQUIRE(Constants::MIDI_NOTE_MIN == 0);
    REQUIRE(Constants::MIDI_NOTE_MAX == 127);
    REQUIRE(Constants::VELOCITY_MIN == 1);
    REQUIRE(Constants::VELOCITY_MAX == 127);
    REQUIRE(Constants::PITCH_CLASS_COUNT == 12);
    REQUIRE(Constants::OCTAVE_MIN == -1);
    REQUIRE(Constants::OCTAVE_MAX == 9);
    REQUIRE(Constants::TEMPO_MIN_BPM == 20.0);
    REQUIRE(Constants::TEMPO_MAX_BPM == 999.0);
    REQUIRE(Constants::EUCLIDEAN_MAX_STEPS == 64);
}

// =============================================================================
// TNTP001A - ErrorCode
// =============================================================================

TEST_CASE("TSTN001A: ErrorCode enum value uniqueness", "[tensor][types]") {
    std::set<int> values;
    values.insert(static_cast<int>(ErrorCode::Ok));
    values.insert(static_cast<int>(ErrorCode::InvalidMidiNote));
    values.insert(static_cast<int>(ErrorCode::InvalidVelocity));
    values.insert(static_cast<int>(ErrorCode::InvalidPitchClass));
    values.insert(static_cast<int>(ErrorCode::InvalidTempo));
    values.insert(static_cast<int>(ErrorCode::InvalidScaleName));
    values.insert(static_cast<int>(ErrorCode::InvalidChordQuality));
    values.insert(static_cast<int>(ErrorCode::InvalidRomanNumeral));
    values.insert(static_cast<int>(ErrorCode::InvalidNoteName));
    values.insert(static_cast<int>(ErrorCode::InvalidOctave));
    values.insert(static_cast<int>(ErrorCode::ScaleGenerationFailed));
    values.insert(static_cast<int>(ErrorCode::ChordGenerationFailed));
    values.insert(static_cast<int>(ErrorCode::ProgressionParseFailed));
    values.insert(static_cast<int>(ErrorCode::VoiceLeadingFailed));
    values.insert(static_cast<int>(ErrorCode::EuclideanInvalidParams));
    values.insert(static_cast<int>(ErrorCode::TupletInvalidRatio));
    values.insert(static_cast<int>(ErrorCode::HarmonyAnalysisFailed));
    values.insert(static_cast<int>(ErrorCode::NegativeHarmonyFailed));
    values.insert(static_cast<int>(ErrorCode::InvalidPitchClassOp));
    values.insert(static_cast<int>(ErrorCode::ConnectionFailed));
    values.insert(static_cast<int>(ErrorCode::ConnectionLost));
    values.insert(static_cast<int>(ErrorCode::SendFailed));
    values.insert(static_cast<int>(ErrorCode::ReceiveFailed));
    values.insert(static_cast<int>(ErrorCode::ProtocolError));
    values.insert(static_cast<int>(ErrorCode::SessionNotReady));
    values.insert(static_cast<int>(ErrorCode::TransactionFailed));
    values.insert(static_cast<int>(ErrorCode::McpParseError));
    values.insert(static_cast<int>(ErrorCode::McpToolNotFound));
    values.insert(static_cast<int>(ErrorCode::OscEncodeError));
    values.insert(static_cast<int>(ErrorCode::OscDecodeError));

    // 30 enum values, all distinct
    REQUIRE(values.size() == 30);
}

// =============================================================================
// TNBT001A - Beat Type
// =============================================================================

TEST_CASE("TSTN001A: Beat::zero and Beat::one", "[tensor][beat]") {
    auto z = Beat::zero();
    REQUIRE(z.numerator == 0);
    REQUIRE(z.denominator == 1);
    REQUIRE(z.to_float() == 0.0);

    auto o = Beat::one();
    REQUIRE(o.numerator == 1);
    REQUIRE(o.denominator == 1);
    REQUIRE(o.to_float() == 1.0);
}

TEST_CASE("TSTN001A: Beat::reduce", "[tensor][beat]") {
    Beat b{6, 4};
    auto r = b.reduce();
    REQUIRE(r.numerator == 3);
    REQUIRE(r.denominator == 2);

    auto z = Beat::zero().reduce();
    REQUIRE(z.numerator == 0);
    REQUIRE(z.denominator == 1);

    Beat neg{-4, 6};
    auto nr = neg.reduce();
    REQUIRE(nr.numerator == -2);
    REQUIRE(nr.denominator == 3);
}

TEST_CASE("TSTN001A: Beat arithmetic", "[tensor][beat]") {
    Beat a{1, 4};
    Beat b{1, 3};

    // Addition: 1/4 + 1/3 = 7/12
    auto sum = a + b;
    REQUIRE(sum == Beat{7, 12});

    // Subtraction: 1/3 - 1/4 = 1/12
    auto diff = b - a;
    REQUIRE(diff == Beat{1, 12});

    // Multiplication by scalar: 1/4 * 3 = 3/4
    auto prod = a * 3;
    REQUIRE(prod == Beat{3, 4});

    // Division by scalar: 1/4 / 2 = 1/8
    auto quot = a / 2;
    REQUIRE(quot == Beat{1, 8});
}

TEST_CASE("TSTN001A: Beat comparison operators", "[tensor][beat]") {
    Beat a{1, 4};
    Beat b{1, 3};
    Beat c{2, 8};  // equals 1/4

    REQUIRE(a == c);
    REQUIRE(a < b);
    REQUIRE(a <= b);
    REQUIRE(a <= c);
    REQUIRE(b > a);
    REQUIRE(b >= a);
    REQUIRE(c >= a);
    REQUIRE_FALSE(a > b);
    REQUIRE_FALSE(b < a);
}

TEST_CASE("TSTN001A: Beat::from_float round-trip", "[tensor][beat]") {
    auto b = Beat::from_float(0.25);
    REQUIRE_THAT(b.to_float(), Catch::Matchers::WithinAbs(0.25, 0.001));

    auto b2 = Beat::from_float(1.0);
    REQUIRE_THAT(b2.to_float(), Catch::Matchers::WithinAbs(1.0, 0.001));

    auto b3 = Beat::from_float(0.0);
    REQUIRE_THAT(b3.to_float(), Catch::Matchers::WithinAbs(0.0, 0.001));
}

TEST_CASE("TSTN001A: beat_lcm", "[tensor][beat]") {
    Beat a{1, 4};
    Beat b{1, 3};
    auto lcm = beat_lcm(a, b);

    // LCM of 1/4 and 1/3: lcm(1,1)/gcd(4,3) = 1/1
    REQUIRE(lcm.numerator == 1);
    REQUIRE(lcm.denominator == 1);
}

// =============================================================================
// TNEV001A - NoteEvent
// =============================================================================

TEST_CASE("TSTN001A: NoteEvent construction and end_time", "[tensor][event]") {
    NoteEvent event;
    event.pitch = 60;
    event.start_time = Beat{1, 1};
    event.duration = Beat{1, 2};
    event.velocity = 100;
    event.muted = false;

    auto end = event.end_time();
    REQUIRE(end == Beat{3, 2});
}

TEST_CASE("TSTN001A: NoteEvent overlaps", "[tensor][event]") {
    NoteEvent a;
    a.pitch = 60;
    a.start_time = Beat{0, 1};
    a.duration = Beat{2, 1};
    a.velocity = 100;

    NoteEvent b;
    b.pitch = 64;
    b.start_time = Beat{1, 1};
    b.duration = Beat{2, 1};
    b.velocity = 100;

    // a [0, 2) overlaps b [1, 3)
    REQUIRE(a.overlaps(b));
    REQUIRE(b.overlaps(a));

    NoteEvent c;
    c.pitch = 67;
    c.start_time = Beat{2, 1};
    c.duration = Beat{1, 1};
    c.velocity = 100;

    // a [0, 2) does not overlap c [2, 3)
    REQUIRE_FALSE(a.overlaps(c));
}

// =============================================================================
// TNEV001A - ChordVoicing
// =============================================================================

TEST_CASE("TSTN001A: ChordVoicing construction", "[tensor][event]") {
    ChordVoicing cv;
    cv.notes = {60, 64, 67};
    cv.root = 0;
    cv.quality = "major";
    cv.inversion = 0;

    REQUIRE_FALSE(cv.empty());
    REQUIRE(cv.size() == 3);
    REQUIRE(cv.bass() == 60);
    REQUIRE(cv.soprano() == 67);
}

TEST_CASE("TSTN001A: ChordVoicing empty", "[tensor][event]") {
    ChordVoicing cv;
    REQUIRE(cv.empty());
    REQUIRE(cv.size() == 0);
    REQUIRE(cv.bass() == 0);
    REQUIRE(cv.soprano() == 0);
}

TEST_CASE("TSTN001A: ChordVoicing pitch_classes", "[tensor][event]") {
    ChordVoicing cv;
    cv.notes = {60, 64, 67};  // C4, E4, G4
    cv.root = 0;

    auto pcs = cv.pitch_classes();
    REQUIRE(pcs.size() == 3);
    REQUIRE(pcs[0] == 0);   // C
    REQUIRE(pcs[1] == 4);   // E
    REQUIRE(pcs[2] == 7);   // G
}

// =============================================================================
// TNEV001A - ScaleDefinition
// =============================================================================

TEST_CASE("TSTN001A: ScaleDefinition get_intervals span", "[tensor][event]") {
    ScaleDefinition sd;
    sd.name = "test";
    sd.intervals = {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0};
    sd.note_count = 7;
    sd.description = "test scale";

    auto span = sd.get_intervals();
    REQUIRE(span.size() == 7);
    REQUIRE(span[0] == 0);
    REQUIRE(span[6] == 11);
}
