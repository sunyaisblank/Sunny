/**
 * @file TSET001A.cpp
 * @brief Tests for TUET001A (Equal Temperament Generalisation)
 *
 * Component: TSET001A
 * Validates: Formal Spec §13.1
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Tuning/TUET001A.h"

using namespace Sunny::Core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// Step size
// =============================================================================

TEST_CASE("TUET001A: 12-TET step is 100 cents", "[tuning][core]") {
    REQUIRE_THAT(edo_step_cents(12), WithinAbs(100.0, 1e-10));
}

TEST_CASE("TUET001A: 24-TET step is 50 cents (quarter-tones)", "[tuning][core]") {
    REQUIRE_THAT(edo_step_cents(24), WithinAbs(50.0, 1e-10));
}

TEST_CASE("TUET001A: 19-TET step size", "[tuning][core]") {
    REQUIRE_THAT(edo_step_cents(19), WithinAbs(1200.0 / 19, 1e-10));
}

// =============================================================================
// Frequency calculation
// =============================================================================

TEST_CASE("TUET001A: A4 (pitch 69) is 440 Hz in 12-EDO", "[tuning][core]") {
    REQUIRE_THAT(edo_frequency(69, 12), WithinAbs(440.0, 1e-10));
}

TEST_CASE("TUET001A: A5 (pitch 81) is 880 Hz in 12-EDO", "[tuning][core]") {
    REQUIRE_THAT(edo_frequency(81, 12), WithinRel(880.0, 1e-10));
}

TEST_CASE("TUET001A: C4 (pitch 60) frequency in 12-EDO", "[tuning][core]") {
    double expected = 440.0 * std::pow(2.0, -9.0 / 12.0);  // ~261.63 Hz
    REQUIRE_THAT(edo_frequency(60, 12), WithinRel(expected, 1e-10));
}

TEST_CASE("TUET001A: Octave above reference doubles frequency", "[tuning][core]") {
    for (int n : {12, 19, 24, 31, 53}) {
        double f_ref = edo_frequency(69, n);
        double f_octave = edo_frequency(69 + n, n);
        REQUIRE_THAT(f_octave / f_ref, WithinRel(2.0, 1e-10));
    }
}

// =============================================================================
// Cents ↔ Ratio conversion
// =============================================================================

TEST_CASE("TUET001A: ratio_to_cents for octave", "[tuning][core]") {
    REQUIRE_THAT(ratio_to_cents(2.0), WithinAbs(1200.0, 1e-10));
}

TEST_CASE("TUET001A: ratio_to_cents for perfect fifth (3/2)", "[tuning][core]") {
    REQUIRE_THAT(ratio_to_cents(3.0 / 2.0), WithinAbs(701.955, 0.001));
}

TEST_CASE("TUET001A: ratio_to_cents for major third (5/4)", "[tuning][core]") {
    REQUIRE_THAT(ratio_to_cents(5.0 / 4.0), WithinAbs(386.314, 0.001));
}

TEST_CASE("TUET001A: cents_to_ratio roundtrip", "[tuning][core]") {
    for (double cents : {0.0, 100.0, 700.0, 1200.0}) {
        REQUIRE_THAT(ratio_to_cents(cents_to_ratio(cents)), WithinAbs(cents, 1e-10));
    }
}

TEST_CASE("TUET001A: ratio_to_cents for unison", "[tuning][core]") {
    REQUIRE_THAT(ratio_to_cents(1.0), WithinAbs(0.0, 1e-10));
}

// =============================================================================
// Pitch class arithmetic in n-EDO
// =============================================================================

TEST_CASE("TUET001A: edo_transpose identity", "[tuning][core]") {
    for (int n : {12, 19, 24}) {
        for (int pc = 0; pc < n; ++pc) {
            REQUIRE(edo_transpose(pc, 0, n) == pc);
        }
    }
}

TEST_CASE("TUET001A: edo_transpose modular wrap", "[tuning][core]") {
    REQUIRE(edo_transpose(10, 5, 12) == 3);   // (10+5) mod 12 = 3
    REQUIRE(edo_transpose(0, 19, 19) == 0);    // full cycle
    REQUIRE(edo_transpose(5, -3, 12) == 2);    // negative interval
}

TEST_CASE("TUET001A: edo_invert involution", "[tuning][core]") {
    for (int n : {12, 19, 24, 31}) {
        for (int pc = 0; pc < n; ++pc) {
            int inv = edo_invert(pc, 0, n);
            int inv2 = edo_invert(inv, 0, n);
            REQUIRE(inv2 == pc);
        }
    }
}

TEST_CASE("TUET001A: edo_invert around axis 0 in 12-EDO", "[tuning][core]") {
    // I_0(pc) = (0 - pc) mod 12 = (12 - pc) mod 12
    REQUIRE(edo_invert(0, 0, 12) == 0);
    REQUIRE(edo_invert(3, 0, 12) == 9);
    REQUIRE(edo_invert(7, 0, 12) == 5);
}

TEST_CASE("TUET001A: edo_interval_class", "[tuning][core]") {
    // In 12-TET: ic(0, 7) = min(7, 5) = 5
    REQUIRE(edo_interval_class(0, 7, 12) == 5);
    // ic(0, 6) = min(6, 6) = 6 (tritone)
    REQUIRE(edo_interval_class(0, 6, 12) == 6);
    // ic(0, 1) = 1
    REQUIRE(edo_interval_class(0, 1, 12) == 1);
    // Symmetric: ic(a,b) == ic(b,a)
    REQUIRE(edo_interval_class(3, 9, 12) == edo_interval_class(9, 3, 12));
}

// =============================================================================
// Just ratio approximation in n-EDO
// =============================================================================

TEST_CASE("TUET001A: perfect fifth approximation in various EDOs", "[tuning][core]") {
    // 3/2 ≈ 7 steps in 12-EDO
    REQUIRE(edo_approximate_ratio(3.0 / 2.0, 12) == 7);

    // 3/2 ≈ 11 steps in 19-EDO
    REQUIRE(edo_approximate_ratio(3.0 / 2.0, 19) == 11);

    // 3/2 ≈ 31 steps in 53-EDO
    REQUIRE(edo_approximate_ratio(3.0 / 2.0, 53) == 31);
}

TEST_CASE("TUET001A: major third approximation", "[tuning][core]") {
    // 5/4 ≈ 4 steps in 12-EDO (400 cents vs 386.3 cents, error ~13.7)
    REQUIRE(edo_approximate_ratio(5.0 / 4.0, 12) == 4);

    // 31-EDO gives better major third (closer to 386.3)
    REQUIRE(edo_approximate_ratio(5.0 / 4.0, 31) == 10);
}

TEST_CASE("TUET001A: 53-EDO has excellent fifth approximation", "[tuning][core]") {
    // 53-EDO fifth error should be < 1 cent
    double error = edo_approximation_error(3.0 / 2.0, 53);
    REQUIRE(error < 1.0);
}

TEST_CASE("TUET001A: 12-EDO major third error ~13.7 cents", "[tuning][core]") {
    double error = edo_approximation_error(5.0 / 4.0, 12);
    REQUIRE_THAT(error, WithinAbs(13.686, 0.01));
}
