/**
 * @file TSJI001A.cpp
 * @brief Tests for TUJI001A (Just Intonation)
 *
 * Component: TSJI001A
 * Validates: Formal Spec §13.2
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Tuning/TUJI001A.h"

using namespace Sunny::Core;
using Catch::Matchers::WithinAbs;

// =============================================================================
// 5-limit interval table
// =============================================================================

TEST_CASE("TUJI001A: unison ratio is 1/1", "[tuning][ji][core]") {
    auto r = ji_ratio(0);
    REQUIRE(r.numerator == 1);
    REQUIRE(r.denominator == 1);
    REQUIRE_THAT(ji_cents(0), WithinAbs(0.0, 1e-10));
}

TEST_CASE("TUJI001A: octave ratio is 2/1", "[tuning][ji][core]") {
    auto r = ji_ratio(12);
    REQUIRE(r.numerator == 2);
    REQUIRE(r.denominator == 1);
    REQUIRE_THAT(ji_cents(12), WithinAbs(1200.0, 1e-10));
}

TEST_CASE("TUJI001A: perfect fifth is 3/2", "[tuning][ji][core]") {
    auto r = ji_ratio(7);
    REQUIRE(r == JIRatio{3, 2});
    REQUIRE_THAT(ji_cents(7), WithinAbs(701.955, 0.001));
}

TEST_CASE("TUJI001A: major third is 5/4", "[tuning][ji][core]") {
    auto r = ji_ratio(4);
    REQUIRE(r == JIRatio{5, 4});
    REQUIRE_THAT(ji_cents(4), WithinAbs(386.314, 0.001));
}

TEST_CASE("TUJI001A: perfect fourth is 4/3", "[tuning][ji][core]") {
    auto r = ji_ratio(5);
    REQUIRE(r == JIRatio{4, 3});
    REQUIRE_THAT(ji_cents(5), WithinAbs(498.045, 0.001));
}

TEST_CASE("TUJI001A: minor third is 6/5", "[tuning][ji][core]") {
    auto r = ji_ratio(3);
    REQUIRE(r == JIRatio{6, 5});
    REQUIRE_THAT(ji_cents(3), WithinAbs(315.641, 0.001));
}

TEST_CASE("TUJI001A: out-of-range returns zero ratio", "[tuning][ji][core]") {
    REQUIRE(ji_ratio(-1) == JIRatio{0, 0});
    REQUIRE(ji_ratio(13) == JIRatio{0, 0});
}

// =============================================================================
// Frequency calculation
// =============================================================================

TEST_CASE("TUJI001A: ji_frequency with 5/4 ratio", "[tuning][ji][core]") {
    double f = ji_frequency(440.0, ji_ratio(4));
    REQUIRE_THAT(f, WithinAbs(550.0, 1e-10));  // 440 * 5/4
}

TEST_CASE("TUJI001A: ji_frequency octave doubles", "[tuning][ji][core]") {
    double f = ji_frequency(261.63, ji_ratio(12));
    REQUIRE_THAT(f, WithinAbs(523.26, 0.01));
}

// =============================================================================
// Commas
// =============================================================================

TEST_CASE("TUJI001A: syntonic comma ≈ 21.5 cents", "[tuning][ji][core]") {
    REQUIRE_THAT(COMMA_SYNTONIC.to_cents(), WithinAbs(21.506, 0.001));
}

TEST_CASE("TUJI001A: Pythagorean comma ≈ 23.5 cents", "[tuning][ji][core]") {
    REQUIRE_THAT(COMMA_PYTHAGOREAN.to_cents(), WithinAbs(23.460, 0.001));
}

TEST_CASE("TUJI001A: diesis ≈ 41.1 cents", "[tuning][ji][core]") {
    REQUIRE_THAT(COMMA_DIESIS.to_cents(), WithinAbs(41.059, 0.001));
}

TEST_CASE("TUJI001A: schisma ≈ 2.0 cents", "[tuning][ji][core]") {
    REQUIRE_THAT(COMMA_SCHISMA.to_cents(), WithinAbs(1.954, 0.001));
}

// =============================================================================
// Ratio arithmetic
// =============================================================================

TEST_CASE("TUJI001A: ji_multiply reduces correctly", "[tuning][ji][core]") {
    // 3/2 * 4/3 = 12/6 = 2/1 (octave)
    auto result = ji_multiply({3, 2}, {4, 3});
    REQUIRE(result == JIRatio{2, 1});
}

TEST_CASE("TUJI001A: ji_divide ratio inversion", "[tuning][ji][core]") {
    // (3/2) / (3/2) = 1/1
    auto result = ji_divide({3, 2}, {3, 2});
    REQUIRE(result == JIRatio{1, 1});

    // (2/1) / (3/2) = 4/3 (perfect fourth)
    auto fourth = ji_divide({2, 1}, {3, 2});
    REQUIRE(fourth == JIRatio{4, 3});
}

TEST_CASE("TUJI001A: syntonic comma = 4 fifths - major third - 2 octaves", "[tuning][ji][core]") {
    // (3/2)^4 / (5/4) / (2/1)^2 = 81/16 / 5/4 / 4 = 81/80
    auto four_fifths = ji_multiply(ji_multiply({3, 2}, {3, 2}), ji_multiply({3, 2}, {3, 2}));
    auto div_third = ji_divide(four_fifths, {5, 4});
    auto result = ji_divide(div_third, ji_multiply({2, 1}, {2, 1}));
    REQUIRE(result == COMMA_SYNTONIC);
}
