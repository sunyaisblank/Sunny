/**
 * @file TSNT001A.cpp
 * @brief Tests for HRNT001A (Non-Tertian Chord Construction)
 *
 * Component: TSNT001A
 * Validates: Formal Spec §5.4
 */

#include <catch2/catch_test_macros.hpp>
#include "Harmony/HRNT001A.h"
#include "Harmony/HRRN001A.h"

using namespace Sunny::Core;

// =============================================================================
// §5.4.1 Quartal Chords
// =============================================================================

TEST_CASE("HRNT001A: Quartal triad", "[harmony][non-tertian][core]") {
    // Quartal triad on C4: C4, F4, Bb4 = {0, 5, 10} from root
    auto chord = quartal_chord(0, 3, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes.size() == 3);
    REQUIRE(chord->quality == "quartal");
    REQUIRE(chord->root == 0);

    // Intervals: 0, +5, +10 from C4 (MIDI 60)
    REQUIRE(chord->notes[0] == 60);
    REQUIRE(chord->notes[1] == 65);
    REQUIRE(chord->notes[2] == 70);

    // Pitch classes: {0, 5, 10}
    auto pcs = chord->pitch_classes();
    REQUIRE(pcs[0] == 0);
    REQUIRE(pcs[1] == 5);
    REQUIRE(pcs[2] == 10);
}

TEST_CASE("HRNT001A: Quartal tetrachord", "[harmony][non-tertian][core]") {
    // Quartal tetrachord on C4: C4, F4, Bb4, Eb5 = {0, 5, 10, 15}
    auto chord = quartal_chord(0, 4, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes.size() == 4);
    REQUIRE(chord->notes[0] == 60);
    REQUIRE(chord->notes[1] == 65);
    REQUIRE(chord->notes[2] == 70);
    REQUIRE(chord->notes[3] == 75);

    // Pitch class 75 % 12 = 3
    auto pcs = chord->pitch_classes();
    REQUIRE(pcs[3] == 3);
}

TEST_CASE("HRNT001A: Quartal chord on different roots", "[harmony][non-tertian][core]") {
    // Quartal triad on D4: D4, G4, C5
    auto chord = quartal_chord(2, 3, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes[0] == 62);  // D4
    REQUIRE(chord->notes[1] == 67);  // G4
    REQUIRE(chord->notes[2] == 72);  // C5
}

// =============================================================================
// §5.4.2 Quintal Chords
// =============================================================================

TEST_CASE("HRNT001A: Quintal triad", "[harmony][non-tertian][core]") {
    // Quintal triad on C4: C4, G4, D5 = {0, 7, 14}
    auto chord = quintal_chord(0, 3, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes.size() == 3);
    REQUIRE(chord->quality == "quintal");

    REQUIRE(chord->notes[0] == 60);
    REQUIRE(chord->notes[1] == 67);
    REQUIRE(chord->notes[2] == 74);
}

TEST_CASE("HRNT001A: Quintal-quartal pitch class relationship", "[harmony][non-tertian][core]") {
    // Quintal voicings are inversions of quartal voicings:
    // Quartal triad on C: PCS = {0, 5, 10}
    // Quintal triad on Bb: PCS = {10, 5, 0} = {0, 5, 10}
    auto quartal = quartal_chord(0, 3, 4);
    auto quintal = quintal_chord(10, 3, 4);  // Root Bb
    REQUIRE(quartal.has_value());
    REQUIRE(quintal.has_value());

    // Extract pitch class sets (unordered)
    auto q4_pcs = quartal->pitch_classes();
    auto q5_pcs = quintal->pitch_classes();
    std::sort(q4_pcs.begin(), q4_pcs.end());
    std::sort(q5_pcs.begin(), q5_pcs.end());
    REQUIRE(q4_pcs == q5_pcs);
}

// =============================================================================
// §5.4.3 Secundal Clusters
// =============================================================================

TEST_CASE("HRNT001A: Chromatic tone cluster", "[harmony][non-tertian][core]") {
    // Cluster of width 3 on C4: C4, Db4, D4, Eb4
    auto chord = tone_cluster(0, 3, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes.size() == 4);  // width + 1
    REQUIRE(chord->quality == "cluster");
    REQUIRE(chord->root == 0);

    REQUIRE(chord->notes[0] == 60);
    REQUIRE(chord->notes[1] == 61);
    REQUIRE(chord->notes[2] == 62);
    REQUIRE(chord->notes[3] == 63);
}

TEST_CASE("HRNT001A: Cluster width invariant", "[harmony][non-tertian][core]") {
    // Width w produces w+1 notes
    for (int w = 1; w <= 11; ++w) {
        auto chord = tone_cluster(0, w, 4);
        REQUIRE(chord.has_value());
        REQUIRE(chord->notes.size() == static_cast<std::size_t>(w + 1));
    }
}

TEST_CASE("HRNT001A: Cluster pitch classes", "[harmony][non-tertian][core]") {
    // Cluster of width 5 on C: {0,1,2,3,4,5}
    auto chord = tone_cluster(0, 5, 4);
    REQUIRE(chord.has_value());
    auto pcs = chord->pitch_classes();
    for (int i = 0; i <= 5; ++i) {
        REQUIRE(pcs[i] == i);
    }
}

TEST_CASE("HRNT001A: Cluster error on invalid width", "[harmony][non-tertian][core]") {
    auto chord = tone_cluster(0, 0, 4);
    REQUIRE_FALSE(chord.has_value());
}

// =============================================================================
// §5.4.4 Polychords
// =============================================================================

TEST_CASE("HRNT001A: Polychord construction", "[harmony][non-tertian][core]") {
    // D major over C7 → classic upper-structure polychord
    auto lower = generate_chord(static_cast<PitchClass>(0), "7", 4);
    auto upper = generate_chord(static_cast<PitchClass>(2), "major", 5);
    REQUIRE(lower.has_value());
    REQUIRE(upper.has_value());

    auto poly = polychord(*lower, *upper);
    REQUIRE(poly.quality == "polychord");
    REQUIRE(poly.root == 0);  // Root from lower chord

    // Notes merged and sorted, no duplicates
    for (std::size_t i = 1; i < poly.notes.size(); ++i) {
        REQUIRE(poly.notes[i] > poly.notes[i - 1]);
    }
}

TEST_CASE("HRNT001A: Polychord PCS is union", "[harmony][non-tertian][core]") {
    // C major (C E G) + Eb major (Eb G Bb) in same octave
    auto lower = generate_chord(static_cast<PitchClass>(0), "major", 4);
    auto upper = generate_chord(static_cast<PitchClass>(3), "major", 4);
    REQUIRE(lower.has_value());
    REQUIRE(upper.has_value());

    auto poly = polychord(*lower, *upper);
    auto pcs = poly.pitch_classes();
    std::sort(pcs.begin(), pcs.end());
    pcs.erase(std::unique(pcs.begin(), pcs.end()), pcs.end());

    // C=0, Eb=3, E=4, G=7, Bb=10
    REQUIRE(pcs.size() == 5);
    REQUIRE(pcs[0] == 0);
    REQUIRE(pcs[1] == 3);
    REQUIRE(pcs[2] == 4);
    REQUIRE(pcs[3] == 7);
    REQUIRE(pcs[4] == 10);
}

TEST_CASE("HRNT001A: Polychord duplicate removal", "[harmony][non-tertian][core]") {
    // C major + C major in same octave → same 3 notes, no duplicates
    auto c = generate_chord(static_cast<PitchClass>(0), "major", 4);
    REQUIRE(c.has_value());

    auto poly = polychord(*c, *c);
    REQUIRE(poly.notes.size() == 3);
}

// =============================================================================
// General chord_by_stacking
// =============================================================================

TEST_CASE("HRNT001A: chord_by_stacking with tritone", "[harmony][non-tertian][core]") {
    // Stacking tritones (6 semitones): C4, F#4, C5
    auto chord = chord_by_stacking(0, 6, 3, 4);
    REQUIRE(chord.has_value());
    REQUIRE(chord->notes.size() == 3);
    REQUIRE(chord->notes[0] == 60);
    REQUIRE(chord->notes[1] == 66);
    REQUIRE(chord->notes[2] == 72);
}

TEST_CASE("HRNT001A: chord_by_stacking error on count < 2", "[harmony][non-tertian][core]") {
    auto chord = chord_by_stacking(0, 5, 1, 4);
    REQUIRE_FALSE(chord.has_value());
    REQUIRE(chord.error() == ErrorCode::ChordGenerationFailed);
}

TEST_CASE("HRNT001A: chord_by_stacking error on invalid interval", "[harmony][non-tertian][core]") {
    auto chord = chord_by_stacking(0, 0, 3, 4);
    REQUIRE_FALSE(chord.has_value());

    auto chord2 = chord_by_stacking(0, 12, 3, 4);
    REQUIRE_FALSE(chord2.has_value());
}
