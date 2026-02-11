/**
 * @file TSST001A.cpp
 * @brief Tests for HRST001A (Harmonic Progression State Transformation)
 *
 * Component: TSST001A
 * Validates: Formal Spec §6.5
 */

#include <catch2/catch_test_macros.hpp>
#include "Harmony/HRST001A.h"

using namespace Sunny::Core;

// =============================================================================
// Initial state construction
// =============================================================================

TEST_CASE("HRST001A: make_harmonic_state", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    REQUIRE(state.key.root == 0);
    REQUIRE_FALSE(state.key.is_minor);
    REQUIRE(state.chord_root == 0);
    REQUIRE(state.chord_quality == "major");
    REQUIRE(state.function == HarmonicFunction::Tonic);
}

TEST_CASE("HRST001A: function assignment for diatonic chords", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};

    // I = Tonic
    REQUIRE(make_harmonic_state(c_major, 0, "major").function == HarmonicFunction::Tonic);
    // ii = Subdominant
    REQUIRE(make_harmonic_state(c_major, 2, "minor").function == HarmonicFunction::Subdominant);
    // iii = Tonic
    REQUIRE(make_harmonic_state(c_major, 4, "minor").function == HarmonicFunction::Tonic);
    // IV = Subdominant
    REQUIRE(make_harmonic_state(c_major, 5, "major").function == HarmonicFunction::Subdominant);
    // V = Dominant
    REQUIRE(make_harmonic_state(c_major, 7, "major").function == HarmonicFunction::Dominant);
    // vi = Tonic
    REQUIRE(make_harmonic_state(c_major, 9, "minor").function == HarmonicFunction::Tonic);
    // vii = Dominant
    REQUIRE(make_harmonic_state(c_major, 11, "diminished").function == HarmonicFunction::Dominant);
}

// =============================================================================
// Diatonic transitions
// =============================================================================

TEST_CASE("HRST001A: diatonic transition I → IV", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // IV in C major: F major = {5, 9, 0} — all diatonic
    auto type = classify_transition(state, 5, "major");
    REQUIRE(type == TransitionType::Diatonic);
}

TEST_CASE("HRST001A: diatonic transition I → V", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    auto type = classify_transition(state, 7, "major");
    REQUIRE(type == TransitionType::Diatonic);
}

TEST_CASE("HRST001A: diatonic transition I → ii", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // ii = D minor = {2, 5, 9} — all diatonic to C major
    auto type = classify_transition(state, 2, "minor");
    REQUIRE(type == TransitionType::Diatonic);
}

// =============================================================================
// Applied / Secondary dominant transitions
// =============================================================================

TEST_CASE("HRST001A: applied transition V/V", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // V/V in C major = D major (root 2), which is non-diatonic (F#)
    auto type = classify_transition(state, 2, "major");
    REQUIRE(type == TransitionType::Applied);
}

TEST_CASE("HRST001A: applied transition V7/ii", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // V7/ii = A7 (root 9, dom7) — A is P5 above D (deg 1)
    // A7 = {9, 1, 4, 7} — C# (1) is non-diatonic
    auto type = classify_transition(state, 9, "7");
    REQUIRE(type == TransitionType::Applied);
}

TEST_CASE("HRST001A: applied transition V/vi", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // V/vi = E major (root 4) — G# non-diatonic
    auto type = classify_transition(state, 4, "major");
    REQUIRE(type == TransitionType::Applied);
}

// =============================================================================
// Modal interchange transitions
// =============================================================================

TEST_CASE("HRST001A: modal interchange iv in major key", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // iv (F minor) borrowed from C minor — Ab is non-diatonic
    auto type = classify_transition(state, 5, "minor");
    REQUIRE(type == TransitionType::ModalInterchange);
}

TEST_CASE("HRST001A: modal interchange bVII in major key", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // bVII = Bb major — borrowed from C natural minor
    auto type = classify_transition(state, 10, "major");
    REQUIRE(type == TransitionType::ModalInterchange);
}

// =============================================================================
// Modulatory transitions
// =============================================================================

TEST_CASE("HRST001A: modulatory transition with key change", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    HarmonicKey g_major = {7, false};
    auto type = classify_transition_with_key(state, 7, "major", g_major);
    REQUIRE(type == TransitionType::Modulatory);
}

TEST_CASE("HRST001A: same key returns non-modulatory", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    auto type = classify_transition_with_key(state, 7, "major", c_major);
    REQUIRE(type == TransitionType::Diatonic);
}

// =============================================================================
// Chromatic transitions
// =============================================================================

TEST_CASE("HRST001A: chromatic transition", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    // Db major in C major — not diatonic, not a secondary dominant,
    // not borrowed from C minor
    auto type = classify_transition(state, 1, "major");
    REQUIRE(type == TransitionType::Chromatic);
}

// =============================================================================
// State application
// =============================================================================

TEST_CASE("HRST001A: apply_transition preserves key", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    auto next = apply_transition(state, 7, "major");
    REQUIRE(next.key == c_major);
    REQUIRE(next.chord_root == 7);
    REQUIRE(next.chord_quality == "major");
    REQUIRE(next.function == HarmonicFunction::Dominant);
}

TEST_CASE("HRST001A: apply_transition with key change", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    HarmonicKey g_major = {7, false};
    auto state = make_harmonic_state(c_major, 0, "major");

    auto next = apply_transition(state, 7, "major", &g_major);
    REQUIRE(next.key == g_major);
    REQUIRE(next.chord_root == 7);
    REQUIRE(next.function == HarmonicFunction::Tonic);  // G is I in G major
}

// =============================================================================
// Progression analysis
// =============================================================================

TEST_CASE("HRST001A: analyze_progression diatonic I-IV-V-I", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    PitchClass roots[] = {0, 5, 7, 0};
    std::string_view quals[] = {"major", "major", "major", "major"};

    auto types = analyze_progression(c_major, roots, quals);
    REQUIRE(types.size() == 3);
    REQUIRE(types[0] == TransitionType::Diatonic);  // I → IV
    REQUIRE(types[1] == TransitionType::Diatonic);  // IV → V
    REQUIRE(types[2] == TransitionType::Diatonic);  // V → I
}

TEST_CASE("HRST001A: analyze_progression with secondary dominant", "[harmony][state][core]") {
    HarmonicKey c_major = {0, false};
    // I → V/V → V → I
    PitchClass roots[] = {0, 2, 7, 0};
    std::string_view quals[] = {"major", "major", "major", "major"};

    auto types = analyze_progression(c_major, roots, quals);
    REQUIRE(types.size() == 3);
    REQUIRE(types[0] == TransitionType::Applied);   // I → V/V (D major)
    REQUIRE(types[1] == TransitionType::Diatonic);  // V/V → V
    REQUIRE(types[2] == TransitionType::Diatonic);  // V → I
}

TEST_CASE("HRST001A: transition_type_to_string", "[harmony][state][core]") {
    REQUIRE(transition_type_to_string(TransitionType::Diatonic) == "Diatonic");
    REQUIRE(transition_type_to_string(TransitionType::Applied) == "Applied");
    REQUIRE(transition_type_to_string(TransitionType::ModalInterchange) == "Modal Interchange");
    REQUIRE(transition_type_to_string(TransitionType::Modulatory) == "Modulatory");
    REQUIRE(transition_type_to_string(TransitionType::Chromatic) == "Chromatic");
}
