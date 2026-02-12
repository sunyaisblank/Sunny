/**
 * @file TSSI001A.cpp
 * @brief Unit tests for Score IR foundation types (SITP001A)
 *
 * Component: TSSI001A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SITP001A
 * Coverage: Id<T>, PositiveRational, ScoreTime, enumerations,
 *           default_velocity, articulation_duration_factor, balance_factor
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Score/SITP001A.h"

using namespace Sunny::Core;

// =============================================================================
// Id<T>
// =============================================================================

TEST_CASE("SITP001A: Id<T> equality and ordering", "[score-ir][types]") {
    ScoreId a{1};
    ScoreId b{1};
    ScoreId c{2};
    PartId p{1};

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
    // Id<Score> and Id<Part> are distinct types — no cross-comparison
}

TEST_CASE("SITP001A: Id<T> hash", "[score-ir][types]") {
    std::hash<ScoreId> h;
    ScoreId a{42};
    ScoreId b{42};
    ScoreId c{99};

    CHECK(h(a) == h(b));
    // Different ids likely produce different hashes (not guaranteed but expected)
    CHECK(h(a) != h(c));
}

// =============================================================================
// PositiveRational
// =============================================================================

TEST_CASE("SITP001A: PositiveRational operations", "[score-ir][types]") {
    SECTION("make_bpm creates integer BPM") {
        auto bpm = make_bpm(120);
        CHECK(bpm.numerator == 120);
        CHECK(bpm.denominator == 1);
        CHECK(bpm.to_float() == 120.0);
    }

    SECTION("Fractional BPM") {
        PositiveRational bpm{241, 2};  // 120.5 BPM
        CHECK(bpm.to_float() == Catch::Approx(120.5));
    }

    SECTION("Ordering") {
        PositiveRational a{120, 1};
        PositiveRational b{121, 1};
        CHECK(a < b);
        CHECK(b > a);
        CHECK(a == PositiveRational{120, 1});
    }
}

// =============================================================================
// ScoreTime
// =============================================================================

TEST_CASE("SITP001A: ScoreTime ordering", "[score-ir][types]") {
    ScoreTime a{1, Beat::zero()};
    ScoreTime b{1, Beat{1, 4}};
    ScoreTime c{2, Beat::zero()};

    CHECK(a == SCORE_START);
    CHECK(a < b);
    CHECK(b < c);
    CHECK(a < c);
    CHECK(a <= a);
    CHECK(c > a);
    CHECK(c >= c);
}

// =============================================================================
// default_velocity
// =============================================================================

TEST_CASE("SITP001A: default_velocity ranges", "[score-ir][types]") {
    CHECK(default_velocity(DynamicLevel::pppp) == 12);
    CHECK(default_velocity(DynamicLevel::p) == 56);
    CHECK(default_velocity(DynamicLevel::mf) == 88);
    CHECK(default_velocity(DynamicLevel::f) == 104);
    CHECK(default_velocity(DynamicLevel::ffff) == 127);
    CHECK(default_velocity(DynamicLevel::sfz) == 120);
}

// =============================================================================
// articulation_duration_factor
// =============================================================================

TEST_CASE("SITP001A: articulation_duration_factor", "[score-ir][types]") {
    CHECK(articulation_duration_factor(ArticulationType::Staccato) == 0.50);
    CHECK(articulation_duration_factor(ArticulationType::Staccatissimo) == 0.25);
    CHECK(articulation_duration_factor(ArticulationType::Tenuto) == 1.00);
    CHECK(articulation_duration_factor(ArticulationType::Fermata) == 1.75);
    CHECK(articulation_duration_factor(ArticulationType::Accent) == 1.00);
}

// =============================================================================
// balance_factor
// =============================================================================

TEST_CASE("SITP001A: balance_factor", "[score-ir][types]") {
    CHECK(balance_factor(DynamicBalance::Foreground) == 1.00);
    CHECK(balance_factor(DynamicBalance::MiddleGround) == 0.85);
    CHECK(balance_factor(DynamicBalance::Background) == 0.70);
}
