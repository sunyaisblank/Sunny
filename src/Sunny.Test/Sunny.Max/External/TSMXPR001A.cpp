/**
 * @file TSMXPR001A.cpp
 * @brief Tests for parameter smoothing utilities (ParameterSmoothing.h)
 *
 * Component: TSMXPR001A
 * Domain: TS (Test) | Category: MXPR (Max Parameter)
 *
 * Tests one_pole_coefficient, one_pole_filter, ramp_increment,
 * clamp_value, and ms_to_samples.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Algorithm/ParameterSmoothing.h"

#include <cmath>

using namespace Sunny::Max::Algorithm;

// =============================================================================
// one_pole_coefficient
// =============================================================================

TEST_CASE("TSMXPR001A: one_pole_coefficient standard values", "[max][parameter]") {
    // 44100 Hz, 10ms → exp(-1/441) ≈ 0.99773
    double coeff = one_pole_coefficient(44100.0, 10.0);
    double expected = std::exp(-1.0 / 441.0);
    REQUIRE_THAT(coeff, Catch::Matchers::WithinAbs(expected, 1e-6));
}

TEST_CASE("TSMXPR001A: one_pole_coefficient 0ms returns 0", "[max][parameter]") {
    REQUIRE(one_pole_coefficient(44100.0, 0.0) == 0.0);
}

TEST_CASE("TSMXPR001A: one_pole_coefficient 0 sample rate returns 0", "[max][parameter]") {
    REQUIRE(one_pole_coefficient(0.0, 10.0) == 0.0);
}

TEST_CASE("TSMXPR001A: one_pole_coefficient negative values return 0", "[max][parameter]") {
    REQUIRE(one_pole_coefficient(44100.0, -5.0) == 0.0);
    REQUIRE(one_pole_coefficient(-44100.0, 10.0) == 0.0);
}

// =============================================================================
// one_pole_filter
// =============================================================================

TEST_CASE("TSMXPR001A: one_pole_filter coeff 0 → instant", "[max][parameter]") {
    REQUIRE(one_pole_filter(0.0, 1.0, 0.0) == 1.0);
    REQUIRE(one_pole_filter(0.5, 0.0, 0.0) == 0.0);
}

TEST_CASE("TSMXPR001A: one_pole_filter coeff near 1 → barely moves", "[max][parameter]") {
    double result = one_pole_filter(0.0, 1.0, 0.999);
    // current * 0.999 + target * 0.001 = 0.0 + 0.001 = 0.001
    REQUIRE_THAT(result, Catch::Matchers::WithinAbs(0.001, 1e-9));
}

TEST_CASE("TSMXPR001A: one_pole_filter convergence", "[max][parameter]") {
    double current = 0.0;
    double target = 1.0;
    double coeff = one_pole_coefficient(44100.0, 10.0);

    // After many samples the filter should converge towards target
    for (int i = 0; i < 4410; ++i) {
        current = one_pole_filter(current, target, coeff);
    }
    REQUIRE_THAT(current, Catch::Matchers::WithinAbs(1.0, 0.001));
}

// =============================================================================
// ramp_increment
// =============================================================================

TEST_CASE("TSMXPR001A: ramp_increment basic", "[max][parameter]") {
    // 0 → 1 over 100 samples → 0.01
    double inc = ramp_increment(0.0, 1.0, 100);
    REQUIRE_THAT(inc, Catch::Matchers::WithinAbs(0.01, 1e-9));
}

TEST_CASE("TSMXPR001A: ramp_increment negative direction", "[max][parameter]") {
    double inc = ramp_increment(1.0, 0.0, 50);
    REQUIRE_THAT(inc, Catch::Matchers::WithinAbs(-0.02, 1e-9));
}

TEST_CASE("TSMXPR001A: ramp_increment 0 samples returns 0", "[max][parameter]") {
    REQUIRE(ramp_increment(0.0, 1.0, 0) == 0.0);
}

// =============================================================================
// clamp_value
// =============================================================================

TEST_CASE("TSMXPR001A: clamp_value boundary enforcement", "[max][parameter]") {
    REQUIRE(clamp_value(0.5, 0.0, 1.0) == 0.5);
    REQUIRE(clamp_value(-0.5, 0.0, 1.0) == 0.0);
    REQUIRE(clamp_value(1.5, 0.0, 1.0) == 1.0);
    REQUIRE(clamp_value(0.0, 0.0, 1.0) == 0.0);
    REQUIRE(clamp_value(1.0, 0.0, 1.0) == 1.0);
}

// =============================================================================
// ms_to_samples
// =============================================================================

TEST_CASE("TSMXPR001A: ms_to_samples", "[max][parameter]") {
    // 10ms @ 44100 Hz → 441
    REQUIRE(ms_to_samples(10.0, 44100.0) == 441);
}

TEST_CASE("TSMXPR001A: ms_to_samples 0ms returns 1", "[max][parameter]") {
    REQUIRE(ms_to_samples(0.0, 44100.0) == 1);
}

TEST_CASE("TSMXPR001A: ms_to_samples negative returns 1", "[max][parameter]") {
    REQUIRE(ms_to_samples(-5.0, 44100.0) == 1);
}

TEST_CASE("TSMXPR001A: ms_to_samples 1ms @ 44100", "[max][parameter]") {
    REQUIRE(ms_to_samples(1.0, 44100.0) == 44);
}
