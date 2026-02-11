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
    // Ok
    values.insert(static_cast<int>(ErrorCode::Ok));
    // Validation errors (2xxx)
    values.insert(static_cast<int>(ErrorCode::InvalidMidiNote));
    values.insert(static_cast<int>(ErrorCode::InvalidVelocity));
    values.insert(static_cast<int>(ErrorCode::InvalidPitchClass));
    values.insert(static_cast<int>(ErrorCode::InvalidTempo));
    values.insert(static_cast<int>(ErrorCode::InvalidScaleName));
    values.insert(static_cast<int>(ErrorCode::InvalidChordQuality));
    values.insert(static_cast<int>(ErrorCode::InvalidRomanNumeral));
    values.insert(static_cast<int>(ErrorCode::InvalidNoteName));
    values.insert(static_cast<int>(ErrorCode::InvalidOctave));
    values.insert(static_cast<int>(ErrorCode::InvalidLetterName));
    values.insert(static_cast<int>(ErrorCode::InvalidSpelledPitch));
    values.insert(static_cast<int>(ErrorCode::InvalidIntervalQuality));
    values.insert(static_cast<int>(ErrorCode::InvalidTimeSignature));
    values.insert(static_cast<int>(ErrorCode::InvalidAppliedChord));
    values.insert(static_cast<int>(ErrorCode::InvalidToneRow));
    values.insert(static_cast<int>(ErrorCode::InvalidTriad));
    values.insert(static_cast<int>(ErrorCode::InvalidMelody));
    // Theory computation errors (3xxx)
    values.insert(static_cast<int>(ErrorCode::ScaleGenerationFailed));
    values.insert(static_cast<int>(ErrorCode::ChordGenerationFailed));
    values.insert(static_cast<int>(ErrorCode::ProgressionParseFailed));
    values.insert(static_cast<int>(ErrorCode::VoiceLeadingFailed));
    values.insert(static_cast<int>(ErrorCode::EuclideanInvalidParams));
    values.insert(static_cast<int>(ErrorCode::TupletInvalidRatio));
    values.insert(static_cast<int>(ErrorCode::HarmonyAnalysisFailed));
    values.insert(static_cast<int>(ErrorCode::NegativeHarmonyFailed));
    values.insert(static_cast<int>(ErrorCode::InvalidPitchClassOp));
    values.insert(static_cast<int>(ErrorCode::InvalidGeneratedScale));
    // Infrastructure errors (4xxx)
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
    // Format errors (45xx)
    values.insert(static_cast<int>(ErrorCode::FormatError));
    values.insert(static_cast<int>(ErrorCode::InvalidScalaFile));
    values.insert(static_cast<int>(ErrorCode::InvalidMidiFile));
    values.insert(static_cast<int>(ErrorCode::InvalidAbcFile));
    values.insert(static_cast<int>(ErrorCode::InvalidMusicXml));

    // 44 enum values, all distinct
    REQUIRE(values.size() == 44);
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
    REQUIRE(b.numerator == 1);
    REQUIRE(b.denominator == 4);
    REQUIRE(b.to_float() == 0.25);

    auto b2 = Beat::from_float(1.0);
    REQUIRE(b2.numerator == 1);
    REQUIRE(b2.denominator == 1);

    auto b3 = Beat::from_float(0.0);
    REQUIRE(b3.numerator == 0);
    REQUIRE(b3.denominator == 1);

    // Triplet eighth: 1/3
    auto b4 = Beat::from_float(1.0 / 3.0);
    REQUIRE(b4.numerator == 1);
    REQUIRE(b4.denominator == 3);

    // Negative value
    auto b5 = Beat::from_float(-0.5);
    REQUIRE(b5.numerator == -1);
    REQUIRE(b5.denominator == 2);
}

TEST_CASE("TSTN001A: Beat::from_float Stern-Brocot precision", "[tensor][beat]") {
    // 2/3 (triplet swing ratio per §9.9)
    auto swing = Beat::from_float(2.0 / 3.0);
    REQUIRE(swing.numerator == 2);
    REQUIRE(swing.denominator == 3);

    // 3/8 (dotted quarter)
    auto dotted = Beat::from_float(0.375);
    REQUIRE(dotted.numerator == 3);
    REQUIRE(dotted.denominator == 8);
}

TEST_CASE("TSTN001A: Beat normalise enforces denominator > 0", "[tensor][beat]") {
    // Negative denominator should be normalised
    auto b = Beat::normalise(3, -4);
    REQUIRE(b.numerator == -3);
    REQUIRE(b.denominator == 4);

    auto b2 = Beat::normalise(-5, -7);
    REQUIRE(b2.numerator == 5);
    REQUIRE(b2.denominator == 7);
}

TEST_CASE("TSTN001A: Beat arithmetic auto-reduces", "[tensor][beat]") {
    Beat a{1, 2};
    Beat b{1, 2};

    // 1/2 + 1/2 = 1/1 (not 2/2)
    auto sum = a + b;
    REQUIRE(sum.numerator == 1);
    REQUIRE(sum.denominator == 1);

    // 3/4 - 1/4 = 1/2 (not 2/4)
    Beat c{3, 4};
    Beat d{1, 4};
    auto diff = c - d;
    REQUIRE(diff.numerator == 1);
    REQUIRE(diff.denominator == 2);
}

TEST_CASE("TSTN001A: Beat rational multiplication (§9.1)", "[tensor][beat]") {
    Beat a{2, 3};
    Beat b{3, 4};

    // (2/3) * (3/4) = 6/12 = 1/2
    auto prod = a * b;
    REQUIRE(prod.numerator == 1);
    REQUIRE(prod.denominator == 2);

    // Tuplet duration: triplet eighth = (2/3) * (1/2) = 1/3 (§9.4)
    Beat triplet_ratio{2, 3};
    Beat eighth{1, 2};
    auto triplet_eighth = triplet_ratio * eighth;
    REQUIRE(triplet_eighth.numerator == 1);
    REQUIRE(triplet_eighth.denominator == 3);

    // Nested tuplet: (2/3) * (4/5) * d = (8/15)*d (§9.4)
    Beat quintuplet_ratio{4, 5};
    auto nested = triplet_ratio * quintuplet_ratio;
    REQUIRE(nested.numerator == 8);
    REQUIRE(nested.denominator == 15);
}

TEST_CASE("TSTN001A: Beat rational division", "[tensor][beat]") {
    Beat a{1, 2};
    Beat b{1, 3};

    // (1/2) / (1/3) = 3/2
    auto quot = a / b;
    REQUIRE(quot.numerator == 3);
    REQUIRE(quot.denominator == 2);

    // Metric modulation: T2 = T1 * (p/q) — ratio via division
    Beat span_old{3, 1};  // 3 beats
    Beat span_new{2, 1};  // 2 beats
    auto tempo_ratio = span_old / span_new;
    REQUIRE(tempo_ratio.numerator == 3);
    REQUIRE(tempo_ratio.denominator == 2);
}

TEST_CASE("TSTN001A: Beat unary negation", "[tensor][beat]") {
    Beat a{3, 4};
    auto neg = -a;
    REQUIRE(neg.numerator == -3);
    REQUIRE(neg.denominator == 4);

    Beat b{-1, 2};
    auto pos = -b;
    REQUIRE(pos.numerator == 1);
    REQUIRE(pos.denominator == 2);
}

TEST_CASE("TSTN001A: beat_lcm", "[tensor][beat]") {
    Beat a{1, 4};
    Beat b{1, 3};
    auto lcm = beat_lcm(a, b);

    // LCM of 1/4 and 1/3: lcm(1,1)/gcd(4,3) = 1/1
    REQUIRE(lcm.numerator == 1);
    REQUIRE(lcm.denominator == 1);

    // LCM of 1/2 and 1/3 = 1/1
    auto lcm2 = beat_lcm(Beat{1, 2}, Beat{1, 3});
    REQUIRE(lcm2 == Beat{1, 1});

    // LCM of 3/4 and 2/3 = lcm(3,2)/gcd(4,3) = 6/1
    auto lcm3 = beat_lcm(Beat{3, 4}, Beat{2, 3});
    REQUIRE(lcm3 == Beat{6, 1});
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

TEST_CASE("TSTN001A: ScaleDefinition step_pattern (§4.1)", "[tensor][event]") {
    // Major scale: intervals {0, 2, 4, 5, 7, 9, 11}
    // Step pattern: (2, 2, 1, 2, 2, 2, 1) — sum = 12
    ScaleDefinition major;
    major.name = "major";
    major.intervals = {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0};
    major.note_count = 7;
    major.description = "";

    auto steps = major.step_pattern();
    REQUIRE(steps.size() == 7);
    REQUIRE(steps[0] == 2);
    REQUIRE(steps[1] == 2);
    REQUIRE(steps[2] == 1);
    REQUIRE(steps[3] == 2);
    REQUIRE(steps[4] == 2);
    REQUIRE(steps[5] == 2);
    REQUIRE(steps[6] == 1);

    // Verify conservation: sum of steps == 12
    int sum = 0;
    for (auto s : steps) sum += s;
    REQUIRE(sum == 12);

    // Whole-tone scale: intervals {0, 2, 4, 6, 8, 10}
    // Step pattern: (2, 2, 2, 2, 2, 2) — symmetric
    ScaleDefinition wt;
    wt.name = "whole-tone";
    wt.intervals = {0, 2, 4, 6, 8, 10, 0, 0, 0, 0, 0, 0};
    wt.note_count = 6;
    wt.description = "";

    auto wt_steps = wt.step_pattern();
    REQUIRE(wt_steps.size() == 6);
    for (auto s : wt_steps) REQUIRE(s == 2);
}
