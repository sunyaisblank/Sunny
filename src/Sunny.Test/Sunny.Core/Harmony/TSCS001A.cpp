/**
 * @file TSCS001A.cpp
 * @brief Tests for HRCS001A (Chord-Scale Theory)
 *
 * Component: TSCS001A
 * Validates: Formal Spec §5.6
 */

#include <catch2/catch_test_macros.hpp>
#include "Harmony/HRCS001A.h"
#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

// =============================================================================
// Minor 9th avoidance rule
// =============================================================================

TEST_CASE("HRCS001A: is_avoid_note detects minor 9th", "[harmony][chord-scale][core]") {
    // C major chord: {0, 4, 7}
    std::vector<PitchClass> c_major = {0, 4, 7};

    // Db (1) is a minor 9th above C (0)
    REQUIRE(is_avoid_note(1, c_major));

    // F (5) is a minor 9th above E (4)
    REQUIRE(is_avoid_note(5, c_major));

    // Ab (8) is a minor 9th above G (7)
    REQUIRE(is_avoid_note(8, c_major));
}

TEST_CASE("HRCS001A: is_avoid_note allows non-minor-9th tensions", "[harmony][chord-scale][core]") {
    std::vector<PitchClass> c_major = {0, 4, 7};

    // D (2) = major 9th above C — available
    REQUIRE_FALSE(is_avoid_note(2, c_major));

    // A (9) = major 2nd above G — available
    REQUIRE_FALSE(is_avoid_note(9, c_major));

    // Bb (10) = minor 7th above C — available
    REQUIRE_FALSE(is_avoid_note(10, c_major));
}

// =============================================================================
// Chord-scale analysis
// =============================================================================

TEST_CASE("HRCS001A: analyze Imaj7 in C Ionian", "[harmony][chord-scale][core]") {
    // Cmaj7 = {0, 4, 7, 11} in C Ionian = {0, 2, 4, 5, 7, 9, 11}
    std::vector<PitchClass> cmaj7 = {0, 4, 7, 11};
    auto result = analyze_chord_scale(cmaj7, 0, SCALE_MAJOR);

    // Chord tones: {0, 4, 7, 11}
    REQUIRE(result.chord_tones.size() == 4);

    // F (5) is a minor 9th above E (4) — avoid
    bool f_is_avoid = std::find(result.avoid.begin(), result.avoid.end(), 5)
                      != result.avoid.end();
    REQUIRE(f_is_avoid);

    // D (2), A (9) are available tensions
    bool d_available = std::find(result.available.begin(), result.available.end(), 2)
                       != result.available.end();
    bool a_available = std::find(result.available.begin(), result.available.end(), 9)
                       != result.available.end();
    REQUIRE(d_available);
    REQUIRE(a_available);
}

TEST_CASE("HRCS001A: analyze ii7 in C Dorian", "[harmony][chord-scale][core]") {
    // Dm7 = {2, 5, 9, 0} in D Dorian = {2, 4, 5, 7, 9, 11, 0}
    std::vector<PitchClass> dm7 = {2, 5, 9, 0};
    auto result = analyze_chord_scale(dm7, 2, SCALE_DORIAN);

    REQUIRE(result.chord_tones.size() == 4);

    // All non-chord scale tones should be available (no avoid notes in Dorian ii7)
    // E(4), G(7), B(11) — none create m9 against {2,5,9,0}
    // E-D = 2 (no), E-F = 11 (no), E-A = 7 (no), E-C = 4 (no) — available
    // G-D = 5, G-F = 2, G-A = 10, G-C = 7 — available
    // B-D = 9, B-F = 6, B-A = 2, B-C = 11 — available
    REQUIRE(result.avoid.empty());
    REQUIRE(result.available.size() == 3);
}

TEST_CASE("HRCS001A: analyze V7 in C Mixolydian", "[harmony][chord-scale][core]") {
    // G7 = {7, 11, 2, 5} in G Mixolydian = {7, 9, 11, 0, 2, 4, 5}
    std::vector<PitchClass> g7 = {7, 11, 2, 5};
    auto result = analyze_chord_scale(g7, 7, SCALE_MIXOLYDIAN);

    REQUIRE(result.chord_tones.size() == 4);

    // C (0) is a minor 9th above B (11) — avoid
    bool c_is_avoid = std::find(result.avoid.begin(), result.avoid.end(), 0)
                      != result.avoid.end();
    REQUIRE(c_is_avoid);
}

TEST_CASE("HRCS001A: partition invariant (chord + available + avoid = scale)", "[harmony][chord-scale][core]") {
    std::vector<PitchClass> cmaj7 = {0, 4, 7, 11};
    auto result = analyze_chord_scale(cmaj7, 0, SCALE_MAJOR);

    std::size_t total = result.chord_tones.size()
                      + result.available.size()
                      + result.avoid.size();
    REQUIRE(total == SCALE_MAJOR.size());
}

// =============================================================================
// Heuristic function lookup
// =============================================================================

TEST_CASE("HRCS001A: chord_scale_for_function lookup", "[harmony][chord-scale][core]") {
    REQUIRE(chord_scale_for_function("Imaj7") == "major");
    REQUIRE(chord_scale_for_function("ii7") == "dorian");
    REQUIRE(chord_scale_for_function("iii7") == "phrygian");
    REQUIRE(chord_scale_for_function("IVmaj7") == "lydian");
    REQUIRE(chord_scale_for_function("V7") == "mixolydian");
    REQUIRE(chord_scale_for_function("vi7") == "aeolian");
    REQUIRE(chord_scale_for_function("viiø7") == "locrian");
}

TEST_CASE("HRCS001A: chord_scale_for_function altered dominants", "[harmony][chord-scale][core]") {
    REQUIRE(chord_scale_for_function("V7alt") == "super_locrian");
    REQUIRE(chord_scale_for_function("V7b9b13") == "phrygian_dominant");
    REQUIRE(chord_scale_for_function("iiø7") == "locrian_natural2");
}

TEST_CASE("HRCS001A: chord_scale_for_function unknown returns nullopt", "[harmony][chord-scale][core]") {
    REQUIRE_FALSE(chord_scale_for_function("xyz").has_value());
}

TEST_CASE("HRCS001A: looked-up scales exist in catalogue", "[harmony][chord-scale][core]") {
    // Every scale returned by the heuristic lookup must exist in SCDF001A
    constexpr std::string_view functions[] = {
        "Imaj7", "ii7", "iii7", "IVmaj7", "V7", "vi7", "viiø7",
        "V7alt", "V7b9b13", "iiø7"
    };

    for (auto fn : functions) {
        auto scale_name = chord_scale_for_function(fn);
        REQUIRE(scale_name.has_value());
        auto scale = find_scale(*scale_name);
        REQUIRE(scale.has_value());
    }
}

// =============================================================================
// Default degree-based chord-scales
// =============================================================================

TEST_CASE("HRCS001A: default_chord_scale major key", "[harmony][chord-scale][core]") {
    REQUIRE(default_chord_scale(0, true) == "major");
    REQUIRE(default_chord_scale(1, true) == "dorian");
    REQUIRE(default_chord_scale(2, true) == "phrygian");
    REQUIRE(default_chord_scale(3, true) == "lydian");
    REQUIRE(default_chord_scale(4, true) == "mixolydian");
    REQUIRE(default_chord_scale(5, true) == "aeolian");
    REQUIRE(default_chord_scale(6, true) == "locrian");
}

TEST_CASE("HRCS001A: default_chord_scale minor key", "[harmony][chord-scale][core]") {
    REQUIRE(default_chord_scale(0, false) == "aeolian");
    REQUIRE(default_chord_scale(1, false) == "locrian");
    REQUIRE(default_chord_scale(2, false) == "major");
    REQUIRE(default_chord_scale(3, false) == "dorian");
    REQUIRE(default_chord_scale(4, false) == "phrygian");
    REQUIRE(default_chord_scale(5, false) == "lydian");
    REQUIRE(default_chord_scale(6, false) == "mixolydian");
}

TEST_CASE("HRCS001A: default_chord_scale out of range", "[harmony][chord-scale][core]") {
    REQUIRE_FALSE(default_chord_scale(-1).has_value());
    REQUIRE_FALSE(default_chord_scale(7).has_value());
}

// =============================================================================
// Remediation: negative interval without OOB in analyze_chord_scale
// =============================================================================

TEST_CASE("HRCS001A: analyze_chord_scale handles negative interval without OOB", "[harmony][chord-scale][core]") {
    // Scale root at Bb (10) with intervals that include a negative value.
    // The Interval type is int8_t; a negative value tests the mod12_positive path.
    std::vector<PitchClass> chord = {10, 2, 5};  // Bbmaj: Bb, D, F
    std::array<Interval, 3> neg_intervals = {0, -2, 4};
    auto result = analyze_chord_scale(chord, 10, neg_intervals);

    // Interval 0 from root 10: pc = mod12_positive(10 + 0) = 10 (chord tone)
    // Interval -2 from root 10: pc = mod12_positive(10 + (-2)) = 8 (Ab)
    // Interval 4 from root 10: pc = mod12_positive(10 + 4) = 2 (D, chord tone)
    std::size_t total = result.chord_tones.size()
                      + result.available.size()
                      + result.avoid.size();
    REQUIRE(total == neg_intervals.size());
}
