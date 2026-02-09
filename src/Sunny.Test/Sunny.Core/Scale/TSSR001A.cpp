/**
 * @file TSSR001A.cpp
 * @brief Unit tests for SCRN001A (Scale relationships)
 *
 * Component: TSSR001A
 * Tests: SCRN001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Scale/SCRN001A.h"
#include "Scale/SCDF001A.h"
#include "Pitch/PTPC001A.h"

using namespace Sunny::Core;

// =============================================================================
// Degree Naming
// =============================================================================

TEST_CASE("SCRN001A: scale_degree_name major scale", "[scale][core]") {
    auto name0 = scale_degree_name(0, SCALE_MAJOR);
    REQUIRE(name0.has_value());
    REQUIRE(*name0 == ScaleDegreeName::Tonic);

    auto name4 = scale_degree_name(4, SCALE_MAJOR);
    REQUIRE(name4.has_value());
    REQUIRE(*name4 == ScaleDegreeName::Dominant);

    // Degree 6 in major: interval[6]=11 -> LeadingTone
    auto name6 = scale_degree_name(6, SCALE_MAJOR);
    REQUIRE(name6.has_value());
    REQUIRE(*name6 == ScaleDegreeName::LeadingTone);
}

TEST_CASE("SCRN001A: scale_degree_name minor scale degree 6", "[scale][core]") {
    // Natural minor: intervals[6]=10 -> Subtonic
    auto name6 = scale_degree_name(6, SCALE_MINOR);
    REQUIRE(name6.has_value());
    REQUIRE(*name6 == ScaleDegreeName::Subtonic);
}

TEST_CASE("SCRN001A: all 7 major degree names", "[scale][core]") {
    ScaleDegreeName expected[] = {
        ScaleDegreeName::Tonic, ScaleDegreeName::Supertonic,
        ScaleDegreeName::Mediant, ScaleDegreeName::Subdominant,
        ScaleDegreeName::Dominant, ScaleDegreeName::Submediant,
        ScaleDegreeName::LeadingTone
    };
    for (int d = 0; d < 7; ++d) {
        auto name = scale_degree_name(d, SCALE_MAJOR);
        REQUIRE(name.has_value());
        REQUIRE(*name == expected[d]);
    }
}

TEST_CASE("SCRN001A: scale_degree_name nullopt for non-7-note", "[scale][core]") {
    auto name = scale_degree_name(0, SCALE_PENTATONIC_MAJOR);
    REQUIRE_FALSE(name.has_value());
}

TEST_CASE("SCRN001A: degree_name_to_string", "[scale][core]") {
    REQUIRE(degree_name_to_string(ScaleDegreeName::Tonic) == "Tonic");
    REQUIRE(degree_name_to_string(ScaleDegreeName::LeadingTone) == "Leading Tone");
    REQUIRE(degree_name_to_string(ScaleDegreeName::Subtonic) == "Subtonic");
}

// =============================================================================
// Scale Relationships
// =============================================================================

TEST_CASE("SCRN001A: C major / A minor = Relative", "[scale][core]") {
    auto rel = classify_scale_relationship(0, SCALE_MAJOR, 9, SCALE_MINOR);
    REQUIRE(rel == ScaleRelationship::Relative);
}

TEST_CASE("SCRN001A: C major / C minor = Parallel", "[scale][core]") {
    auto rel = classify_scale_relationship(0, SCALE_MAJOR, 0, SCALE_MINOR);
    REQUIRE(rel == ScaleRelationship::Parallel);
}

TEST_CASE("SCRN001A: C major / D major = None", "[scale][core]") {
    auto rel = classify_scale_relationship(0, SCALE_MAJOR, 2, SCALE_MAJOR);
    REQUIRE(rel == ScaleRelationship::None);
}

TEST_CASE("SCRN001A: common tones C and G major share 6", "[scale][core]") {
    // C major: C D E F G A B ; G major: G A B C D E F#
    // Common: C D E G A B = 6
    auto count = scale_common_tone_count(0, SCALE_MAJOR, 7, SCALE_MAJOR);
    REQUIRE(count == 6);
}

TEST_CASE("SCRN001A: common tones same scale = cardinality", "[scale][core]") {
    auto count = scale_common_tone_count(0, SCALE_MAJOR, 0, SCALE_MAJOR);
    REQUIRE(count == 7);
}

TEST_CASE("SCRN001A: pentatonic subset of diatonic", "[scale][core]") {
    // C pentatonic major {C,D,E,G,A} ⊂ C major {C,D,E,F,G,A,B}
    REQUIRE(scale_is_subset(0, SCALE_PENTATONIC_MAJOR, 0, SCALE_MAJOR));
}

TEST_CASE("SCRN001A: diatonic not subset of pentatonic", "[scale][core]") {
    REQUIRE_FALSE(scale_is_subset(0, SCALE_MAJOR, 0, SCALE_PENTATONIC_MAJOR));
}

// =============================================================================
// Borrowed Chords
// =============================================================================

TEST_CASE("SCRN001A: borrowed chords from C minor", "[scale][core]") {
    auto borrowed = find_borrowed_chords(0, SCALE_MAJOR, SCALE_MINOR, "minor");
    REQUIRE_FALSE(borrowed.empty());

    // Should contain bIII (Eb major), bVI (Ab major), bVII (Bb major)
    bool found_bIII = false;
    bool found_bVI = false;
    bool found_bVII = false;

    for (auto& bc : borrowed) {
        if (bc.root == 3 && bc.quality == "major") found_bIII = true;
        if (bc.root == 8 && bc.quality == "major") found_bVI = true;
        if (bc.root == 10 && bc.quality == "major") found_bVII = true;
    }

    REQUIRE(found_bIII);
    REQUIRE(found_bVI);
    REQUIRE(found_bVII);
}

TEST_CASE("SCRN001A: borrowed chords source mode recorded", "[scale][core]") {
    auto borrowed = find_borrowed_chords(0, SCALE_MAJOR, SCALE_MINOR, "minor");
    for (auto& bc : borrowed) {
        REQUIRE(bc.source_mode == "minor");
    }
}

// =============================================================================
// Generated Scales
// =============================================================================

TEST_CASE("SCRN001A: generator 7, cardinality 7 = diatonic", "[scale][core]") {
    // Starting from C, generator=7 (perfect fifth): C G D A E B F# = {0,2,4,6,7,9,11}
    auto result = generate_scale_from_generator(0, 7, 7);
    REQUIRE(result.has_value());
    REQUIRE(result->pitch_classes.size() == 7);
    // Should contain 0,2,4,6,7,9,11
    REQUIRE(result->pitch_classes[0] == 0);
    REQUIRE(result->has_deep_scale_property);
}

TEST_CASE("SCRN001A: generator 7, cardinality 5 = pentatonic", "[scale][core]") {
    auto result = generate_scale_from_generator(0, 7, 5);
    REQUIRE(result.has_value());
    REQUIRE(result->pitch_classes.size() == 5);
    REQUIRE(result->has_deep_scale_property);
}

TEST_CASE("SCRN001A: generator 2, cardinality 6 = whole tone", "[scale][core]") {
    auto result = generate_scale_from_generator(0, 2, 6);
    REQUIRE(result.has_value());
    REQUIRE(result->pitch_classes.size() == 6);
    // {0,2,4,6,8,10}
    REQUIRE(result->pitch_classes[0] == 0);
    REQUIRE(result->pitch_classes[5] == 10);
}

TEST_CASE("SCRN001A: deep scale property g=7 true, g=2 false", "[scale][core]") {
    REQUIRE(has_deep_scale_property(7, 7));
    REQUIRE_FALSE(has_deep_scale_property(2, 6));
}

TEST_CASE("SCRN001A: invalid generated scale g=6 k=3", "[scale][core]") {
    // g=6: 0, 6, 0 — only 2 distinct PCs but k=3 requested
    auto result = generate_scale_from_generator(0, 6, 3);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ErrorCode::InvalidGeneratedScale);
}
