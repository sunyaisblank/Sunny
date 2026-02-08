/**
 * @file TSMXEU001A.cpp
 * @brief Tests for Bresenham Euclidean algorithm (EuclideanBresenham.h)
 *
 * Component: TSMXEU001A
 * Domain: TS (Test) | Category: MXEU (Max Euclidean)
 *
 * Tests the standalone Bresenham-based Euclidean rhythm algorithm
 * extracted from MXEU001A.
 */

#include <catch2/catch_test_macros.hpp>

#include "Algorithm/EuclideanBresenham.h"
#include "Rhythm/RHEU001A.h"

#include <algorithm>
#include <numeric>

using namespace Sunny::Max::Algorithm;

// =============================================================================
// Known Euclidean Patterns
// =============================================================================

TEST_CASE("TSMXEU001A: Known Euclidean patterns", "[max][euclidean]") {
    std::vector<bool> pattern;

    // E(3,8) — classic tresillo
    generate_euclidean_pattern(pattern, 3, 8, 0);
    REQUIRE(pattern.size() == 8);
    long pulse_count = std::count(pattern.begin(), pattern.end(), true);
    REQUIRE(pulse_count == 3);

    // E(5,8)
    generate_euclidean_pattern(pattern, 5, 8, 0);
    REQUIRE(pattern.size() == 8);
    pulse_count = std::count(pattern.begin(), pattern.end(), true);
    REQUIRE(pulse_count == 5);

    // E(1,4)
    generate_euclidean_pattern(pattern, 1, 4, 0);
    REQUIRE(pattern.size() == 4);
    pulse_count = std::count(pattern.begin(), pattern.end(), true);
    REQUIRE(pulse_count == 1);
}

// =============================================================================
// Edge Cases
// =============================================================================

TEST_CASE("TSMXEU001A: 0 pulses produces all false", "[max][euclidean]") {
    std::vector<bool> pattern;
    generate_euclidean_pattern(pattern, 0, 8, 0);

    REQUIRE(pattern.size() == 8);
    REQUIRE(std::none_of(pattern.begin(), pattern.end(), [](bool b) { return b; }));
}

TEST_CASE("TSMXEU001A: pulses == steps produces all true", "[max][euclidean]") {
    std::vector<bool> pattern;
    generate_euclidean_pattern(pattern, 8, 8, 0);

    REQUIRE(pattern.size() == 8);
    REQUIRE(std::all_of(pattern.begin(), pattern.end(), [](bool b) { return b; }));
}

TEST_CASE("TSMXEU001A: 1 step", "[max][euclidean]") {
    std::vector<bool> pattern;
    generate_euclidean_pattern(pattern, 1, 1, 0);

    REQUIRE(pattern.size() == 1);
    REQUIRE(pattern[0] == true);
}

// =============================================================================
// Rotation
// =============================================================================

TEST_CASE("TSMXEU001A: Rotation correctness", "[max][euclidean]") {
    std::vector<bool> base;
    generate_euclidean_pattern(base, 3, 8, 0);

    std::vector<bool> rotated;
    generate_euclidean_pattern(rotated, 3, 8, 1);

    // rotated[i] == base[(i+1)%8]
    for (std::size_t i = 0; i < 8; ++i) {
        REQUIRE(rotated[i] == base[(i + 1) % 8]);
    }
}

TEST_CASE("TSMXEU001A: Negative rotation normalisation", "[max][euclidean]") {
    std::vector<bool> pos;
    generate_euclidean_pattern(pos, 3, 8, 7);

    std::vector<bool> neg;
    generate_euclidean_pattern(neg, 3, 8, -1);

    // rotation 7 == rotation -1 (mod 8)
    REQUIRE(pos == neg);
}

// =============================================================================
// Pulse Count Invariant
// =============================================================================

TEST_CASE("TSMXEU001A: Pulse count invariant", "[max][euclidean]") {
    std::vector<bool> pattern;
    for (long steps = 1; steps <= 16; ++steps) {
        for (long pulses = 0; pulses <= steps; ++pulses) {
            generate_euclidean_pattern(pattern, pulses, steps, 0);
            long count = std::count(pattern.begin(), pattern.end(), true);
            REQUIRE(count == pulses);
        }
    }
}

// =============================================================================
// Equivalence with Sunny::Core
// =============================================================================

TEST_CASE("TSMXEU001A: Equivalence check against Sunny::Core euclidean_rhythm", "[max][euclidean]") {
    // Both algorithms should produce patterns with the same pulse count
    // and even distribution, though rotation semantics may differ
    for (int pulses = 1; pulses < 8; ++pulses) {
        std::vector<bool> bresenham_pattern;
        generate_euclidean_pattern(bresenham_pattern, pulses, 8, 0);

        auto core_result = Sunny::Core::euclidean_rhythm(pulses, 8);
        REQUIRE(core_result.has_value());

        // Same pulse count
        long b_count = std::count(bresenham_pattern.begin(), bresenham_pattern.end(), true);
        long c_count = std::count(core_result->begin(), core_result->end(), true);
        REQUIRE(b_count == c_count);
    }
}
