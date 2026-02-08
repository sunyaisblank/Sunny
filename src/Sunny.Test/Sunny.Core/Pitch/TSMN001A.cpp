/**
 * @file TSMN001A.cpp
 * @brief Tests for PTMN001A (MIDI note utilities)
 *
 * Component: TSMN001A
 * Domain: TS (Test) | Category: MN (MIDI Note)
 *
 * Tests midi_to_pitch_octave, midi_octave, pitch_octave_to_midi,
 * closest_pitch_class_midi, and transpose_midi.
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTMN001A.h"

using namespace Sunny::Core;

// =============================================================================
// midi_to_pitch_octave
// =============================================================================

TEST_CASE("TSMN001A: midi_to_pitch_octave known values", "[pitch][midi]") {
    // C4 = MIDI 60
    auto [pc60, oct60] = midi_to_pitch_octave(60);
    REQUIRE(pc60 == 0);
    REQUIRE(oct60 == 4);

    // A4 = MIDI 69
    auto [pc69, oct69] = midi_to_pitch_octave(69);
    REQUIRE(pc69 == 9);
    REQUIRE(oct69 == 4);

    // C-1 = MIDI 0
    auto [pc0, oct0] = midi_to_pitch_octave(0);
    REQUIRE(pc0 == 0);
    REQUIRE(oct0 == -1);

    // G9 = MIDI 127
    auto [pc127, oct127] = midi_to_pitch_octave(127);
    REQUIRE(pc127 == 7);
    REQUIRE(oct127 == 9);
}

// =============================================================================
// midi_octave
// =============================================================================

TEST_CASE("TSMN001A: midi_octave", "[pitch][midi]") {
    REQUIRE(midi_octave(60) == 4);
    REQUIRE(midi_octave(0) == -1);
    REQUIRE(midi_octave(127) == 9);
    REQUIRE(midi_octave(12) == 0);
    REQUIRE(midi_octave(24) == 1);
}

// =============================================================================
// pitch_octave_to_midi
// =============================================================================

TEST_CASE("TSMN001A: pitch_octave_to_midi known values", "[pitch][midi]") {
    REQUIRE(pitch_octave_to_midi(0, 4) == 60);   // C4
    REQUIRE(pitch_octave_to_midi(9, 4) == 69);   // A4
    REQUIRE(pitch_octave_to_midi(0, -1) == 0);   // C-1
    REQUIRE(pitch_octave_to_midi(7, 9) == 127);  // G9
}

TEST_CASE("TSMN001A: pitch_octave_to_midi out-of-range", "[pitch][midi]") {
    // Beyond MIDI 127
    REQUIRE(pitch_octave_to_midi(8, 9) == std::nullopt);
    // Below MIDI 0
    REQUIRE(pitch_octave_to_midi(0, -2) == std::nullopt);
}

// =============================================================================
// Round-trip invariant
// =============================================================================

TEST_CASE("TSMN001A: Round-trip invariant for all MIDI notes", "[pitch][midi]") {
    for (int n = 0; n <= 127; ++n) {
        auto [pc, oct] = midi_to_pitch_octave(static_cast<MidiNote>(n));
        auto result = pitch_octave_to_midi(pc, oct);
        REQUIRE(result.has_value());
        REQUIRE(*result == static_cast<MidiNote>(n));
    }
}

// =============================================================================
// closest_pitch_class_midi
// =============================================================================

TEST_CASE("TSMN001A: closest_pitch_class_midi", "[pitch][midi]") {
    // ref 60 (C4) + pc 7 (G) → 55 (G3, dist 5) not 67 (G4, dist 7)
    REQUIRE(closest_pitch_class_midi(60, 7) == 55);

    // ref 60 (C4) + pc 11 (B) → 59 (B3, dist 1, not 71)
    REQUIRE(closest_pitch_class_midi(60, 11) == 59);

    // ref 60 (C4) + pc 0 (C) → 60 (same note, dist 0)
    REQUIRE(closest_pitch_class_midi(60, 0) == 60);

    // ref 60 + pc 6 (F#): candidates [66, 54, 78], dist [6, 6, 18]
    // Equal distance — first candidate (66) wins
    REQUIRE(closest_pitch_class_midi(60, 6) == 66);
}

// =============================================================================
// transpose_midi
// =============================================================================

TEST_CASE("TSMN001A: transpose_midi", "[pitch][midi]") {
    REQUIRE(transpose_midi(60, 7) == 67);
    REQUIRE(transpose_midi(60, -12) == 48);
    REQUIRE(transpose_midi(60, 0) == 60);

    // Out of range
    REQUIRE(transpose_midi(0, -1) == std::nullopt);
    REQUIRE(transpose_midi(127, 1) == std::nullopt);
}
