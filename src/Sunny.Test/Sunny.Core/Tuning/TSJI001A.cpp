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
    REQUIRE(r.has_value());
    REQUIRE(r->numerator == 1);
    REQUIRE(r->denominator == 1);
    auto c = ji_cents(0);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(0.0, 1e-10));
}

TEST_CASE("TUJI001A: octave ratio is 2/1", "[tuning][ji][core]") {
    auto r = ji_ratio(12);
    REQUIRE(r.has_value());
    REQUIRE(r->numerator == 2);
    REQUIRE(r->denominator == 1);
    auto c = ji_cents(12);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(1200.0, 1e-10));
}

TEST_CASE("TUJI001A: perfect fifth is 3/2", "[tuning][ji][core]") {
    auto r = ji_ratio(7);
    REQUIRE(r.has_value());
    REQUIRE(*r == JIRatio{3, 2});
    auto c = ji_cents(7);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(701.955, 0.001));
}

TEST_CASE("TUJI001A: major third is 5/4", "[tuning][ji][core]") {
    auto r = ji_ratio(4);
    REQUIRE(r.has_value());
    REQUIRE(*r == JIRatio{5, 4});
    auto c = ji_cents(4);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(386.314, 0.001));
}

TEST_CASE("TUJI001A: perfect fourth is 4/3", "[tuning][ji][core]") {
    auto r = ji_ratio(5);
    REQUIRE(r.has_value());
    REQUIRE(*r == JIRatio{4, 3});
    auto c = ji_cents(5);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(498.045, 0.001));
}

TEST_CASE("TUJI001A: minor third is 6/5", "[tuning][ji][core]") {
    auto r = ji_ratio(3);
    REQUIRE(r.has_value());
    REQUIRE(*r == JIRatio{6, 5});
    auto c = ji_cents(3);
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(315.641, 0.001));
}

TEST_CASE("TUJI001A: out-of-range returns error", "[tuning][ji][core]") {
    CHECK_FALSE(ji_ratio(-1).has_value());
    CHECK_FALSE(ji_ratio(13).has_value());
}

// =============================================================================
// Frequency calculation
// =============================================================================

TEST_CASE("TUJI001A: ji_frequency with 5/4 ratio", "[tuning][ji][core]") {
    auto f = ji_frequency(440.0, *ji_ratio(4));
    REQUIRE(f.has_value());
    REQUIRE_THAT(*f, WithinAbs(550.0, 1e-10));  // 440 * 5/4
}

TEST_CASE("TUJI001A: ji_frequency octave doubles", "[tuning][ji][core]") {
    auto f = ji_frequency(261.63, *ji_ratio(12));
    REQUIRE(f.has_value());
    REQUIRE_THAT(*f, WithinAbs(523.26, 0.01));
}

// =============================================================================
// Commas
// =============================================================================

TEST_CASE("TUJI001A: syntonic comma ≈ 21.5 cents", "[tuning][ji][core]") {
    auto c = COMMA_SYNTONIC.to_cents();
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(21.506, 0.001));
}

TEST_CASE("TUJI001A: Pythagorean comma ≈ 23.5 cents", "[tuning][ji][core]") {
    auto c = COMMA_PYTHAGOREAN.to_cents();
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(23.460, 0.001));
}

TEST_CASE("TUJI001A: diesis ≈ 41.1 cents", "[tuning][ji][core]") {
    auto c = COMMA_DIESIS.to_cents();
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(41.059, 0.001));
}

TEST_CASE("TUJI001A: schisma ≈ 2.0 cents", "[tuning][ji][core]") {
    auto c = COMMA_SCHISMA.to_cents();
    REQUIRE(c.has_value());
    REQUIRE_THAT(*c, WithinAbs(1.954, 0.001));
}

// =============================================================================
// Ratio arithmetic
// =============================================================================

TEST_CASE("TUJI001A: ji_multiply reduces correctly", "[tuning][ji][core]") {
    // 3/2 * 4/3 = 12/6 = 2/1 (octave)
    auto result = ji_multiply({3, 2}, {4, 3});
    REQUIRE(result.has_value());
    REQUIRE(*result == JIRatio{2, 1});
}

TEST_CASE("TUJI001A: ji_divide ratio inversion", "[tuning][ji][core]") {
    // (3/2) / (3/2) = 1/1
    auto result = ji_divide({3, 2}, {3, 2});
    REQUIRE(result.has_value());
    REQUIRE(*result == JIRatio{1, 1});

    // (2/1) / (3/2) = 4/3 (perfect fourth)
    auto fourth = ji_divide({2, 1}, {3, 2});
    REQUIRE(fourth.has_value());
    REQUIRE(*fourth == JIRatio{4, 3});
}

TEST_CASE("TUJI001A: syntonic comma = 4 fifths - major third - 2 octaves", "[tuning][ji][core]") {
    // (3/2)^4 / (5/4) / (2/1)^2 = 81/16 / 5/4 / 4 = 81/80
    auto two_fifths_a = ji_multiply({3, 2}, {3, 2});
    REQUIRE(two_fifths_a.has_value());
    auto two_fifths_b = ji_multiply({3, 2}, {3, 2});
    REQUIRE(two_fifths_b.has_value());
    auto four_fifths = ji_multiply(*two_fifths_a, *two_fifths_b);
    REQUIRE(four_fifths.has_value());
    auto div_third = ji_divide(*four_fifths, {5, 4});
    REQUIRE(div_third.has_value());
    auto two_octaves = ji_multiply({2, 1}, {2, 1});
    REQUIRE(two_octaves.has_value());
    auto result = ji_divide(*div_third, *two_octaves);
    REQUIRE(result.has_value());
    REQUIRE(*result == COMMA_SYNTONIC);
}

// =============================================================================
// Precondition rejection tests
// =============================================================================

TEST_CASE("TUJI001A: JIRatio{0,0}.to_double returns error", "[tuning][ji][core]") {
    JIRatio r{0, 0};
    auto d = r.to_double();
    REQUIRE_FALSE(d.has_value());
    REQUIRE(d.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: JIRatio{1,0}.to_cents returns error", "[tuning][ji][core]") {
    JIRatio r{1, 0};
    auto c = r.to_cents();
    REQUIRE_FALSE(c.has_value());
    REQUIRE(c.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: ji_divide by zero numerator returns error", "[tuning][ji][core]") {
    auto r = ji_divide({3, 2}, {0, 1});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: ji_multiply with zero denominator returns error", "[tuning][ji][core]") {
    auto r = ji_multiply({3, 2}, {4, 0});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: ji_multiply detects overflow for large ratios", "[tuning][ji][core]") {
    // INT64_MAX is ~9.2e18; multiplying two large values should overflow
    JIRatio large_a{INT64_MAX / 2, 1};
    JIRatio large_b{3, 1};
    auto r = ji_multiply(large_a, large_b);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: ji_frequency with zero denominator returns error", "[tuning][ji][core]") {
    auto f = ji_frequency(440.0, {3, 0});
    REQUIRE_FALSE(f.has_value());
    REQUIRE(f.error() == ErrorCode::InvalidJIRatio);
}

TEST_CASE("TUJI001A: ji_cents out of range returns error", "[tuning][ji][core]") {
    auto c = ji_cents(-1);
    REQUIRE_FALSE(c.has_value());
    REQUIRE(c.error() == ErrorCode::InvalidJIRatio);
}
