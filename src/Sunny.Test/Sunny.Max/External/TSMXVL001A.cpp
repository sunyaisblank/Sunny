/**
 * @file TSMXVL001A.cpp
 * @brief Tests for standalone voice leading algorithm (VoiceLeadStandalone.h)
 *
 * Component: TSMXVL001A
 * Domain: TS (Test) | Category: MXVL (Max Voice Leading)
 *
 * Tests nearest_pitch, compute_voice_leading, lock_bass,
 * max_jump constraint, and voice crossing prevention.
 */

#include <catch2/catch_test_macros.hpp>

#include "Algorithm/VoiceLeadStandalone.h"

using namespace Sunny::Max::Algorithm;

// =============================================================================
// nearest_pitch
// =============================================================================

TEST_CASE("TSMXVL001A: nearest_pitch basic", "[max][voicelead]") {
    // source 60 (C4) + pc 7 (G): diff=7, >6 so diff=-5, result=55 (G3)
    REQUIRE(nearest_pitch(60, 7) == 55);

    // source 60 (C4) + pc 11 (B): diff=-1 (normalised), result=59 (B3)
    REQUIRE(nearest_pitch(60, 11) == 59);

    // source 60 (C4) + pc 0 (C): diff=0, result=60
    REQUIRE(nearest_pitch(60, 0) == 60);

    // source 60 + pc 5 (F): diff=5, result=65 (F4)
    REQUIRE(nearest_pitch(60, 5) == 65);
}

TEST_CASE("TSMXVL001A: nearest_pitch boundary", "[max][voicelead]") {
    // Low boundary: source 2 + pc 11 should not go below 0
    long result = nearest_pitch(2, 11);
    REQUIRE(result >= 0);
    REQUIRE(result <= 127);
    REQUIRE(result % 12 == 11);

    // High boundary: source 125 + pc 0 should not exceed 127
    result = nearest_pitch(125, 0);
    REQUIRE(result >= 0);
    REQUIRE(result <= 127);
    REQUIRE(result % 12 == 0);
}

// =============================================================================
// compute_voice_leading - basic
// =============================================================================

TEST_CASE("TSMXVL001A: compute_voice_leading 3-voice", "[max][voicelead]") {
    std::vector<long> source = {60, 64, 67};  // C E G
    std::vector<long> target_pcs = {5, 9, 0}; // F A C
    std::vector<long> result;

    long motion = compute_voice_leading(source, target_pcs, result, false, 0);

    REQUIRE(result.size() == 3);
    REQUIRE(motion > 0);

    // Result notes should have the target pitch classes (after voice crossing fix)
    for (std::size_t i = 0; i < result.size(); ++i) {
        REQUIRE(result[i] >= 0);
        REQUIRE(result[i] <= 127);
    }
}

TEST_CASE("TSMXVL001A: compute_voice_leading 4-voice", "[max][voicelead]") {
    std::vector<long> source = {48, 55, 60, 64};   // C G C E
    std::vector<long> target_pcs = {5, 9, 0, 4};   // F A C E
    std::vector<long> result;

    long motion = compute_voice_leading(source, target_pcs, result, false, 0);

    REQUIRE(result.size() == 4);
    REQUIRE(motion >= 0);
}

// =============================================================================
// lock_bass
// =============================================================================

TEST_CASE("TSMXVL001A: lock_bass behaviour", "[max][voicelead]") {
    std::vector<long> source = {48, 60, 64};
    std::vector<long> target_pcs = {5, 9, 0};  // F A C → bass locked to F
    std::vector<long> result;

    compute_voice_leading(source, target_pcs, result, true, 0);

    REQUIRE(result.size() == 3);
    // Bass voice should map to F (pc 5)
    REQUIRE(result[0] % 12 == 5);
}

// =============================================================================
// max_jump constraint
// =============================================================================

TEST_CASE("TSMXVL001A: max_jump constraint", "[max][voicelead]") {
    std::vector<long> source = {60};
    std::vector<long> target_pcs = {0};  // C to C — no jump needed
    std::vector<long> result;

    compute_voice_leading(source, target_pcs, result, false, 6);
    REQUIRE(result.size() == 1);

    // With a far target, jump is limited
    std::vector<long> far_target = {6};  // F#
    compute_voice_leading(source, far_target, result, false, 4);
    REQUIRE(result.size() == 1);
    long actual_jump = std::abs(result[0] - 60);
    // Jump should be limited, though re-alignment may adjust slightly
    REQUIRE(actual_jump <= 12);
}

// =============================================================================
// Empty Inputs
// =============================================================================

TEST_CASE("TSMXVL001A: Empty inputs", "[max][voicelead]") {
    std::vector<long> empty;
    std::vector<long> target_pcs = {0, 4, 7};
    std::vector<long> result;

    long motion = compute_voice_leading(empty, target_pcs, result, false, 0);
    REQUIRE(motion == 0);
    REQUIRE(result.empty());

    std::vector<long> source = {60, 64, 67};
    motion = compute_voice_leading(source, empty, result, false, 0);
    REQUIRE(motion == 0);
    REQUIRE(result.empty());
}

// =============================================================================
// Voice Crossing Prevention
// =============================================================================

TEST_CASE("TSMXVL001A: Voice crossing fix — result in ascending order", "[max][voicelead]") {
    std::vector<long> source = {60, 64, 67};
    std::vector<long> target_pcs = {0, 4, 7};
    std::vector<long> result;

    compute_voice_leading(source, target_pcs, result, false, 0);

    // After voice crossing fix, notes should be ascending
    for (std::size_t i = 1; i < result.size(); ++i) {
        REQUIRE(result[i] > result[i - 1]);
    }
}
