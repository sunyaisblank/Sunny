/**
 * @file TSHT001A.cpp
 * @brief Tests for TUHT001A (Historical Temperaments)
 *
 * Component: TSHT001A
 * Validates: Formal Spec §13.3
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "Tuning/TUHT001A.h"

using namespace Sunny::Core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// Tuning table invariants
// =============================================================================

TEST_CASE("TUHT001A: equal temperament table is all zeros", "[tuning][temperament][core]") {
    for (int i = 0; i < 12; ++i) {
        REQUIRE(TUNING_EQUAL[i] == 0.0);
    }
}

TEST_CASE("TUHT001A: Pythagorean C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_PYTHAGOREAN[0] == 0.0);
}

TEST_CASE("TUHT001A: Werckmeister III C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_WERCKMEISTER_III[0] == 0.0);
}

TEST_CASE("TUHT001A: Vallotti C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_VALLOTTI[0] == 0.0);
}

// =============================================================================
// Temperament lookup
// =============================================================================

TEST_CASE("TUHT001A: find_temperament known names", "[tuning][temperament][core]") {
    REQUIRE(find_temperament("equal").has_value());
    REQUIRE(find_temperament("pythagorean").has_value());
    REQUIRE(find_temperament("quarter_comma_meantone").has_value());
    REQUIRE(find_temperament("werckmeister_iii").has_value());
    REQUIRE(find_temperament("vallotti").has_value());
}

TEST_CASE("TUHT001A: find_temperament unknown returns nullopt", "[tuning][temperament][core]") {
    REQUIRE_FALSE(find_temperament("baroque_magic").has_value());
}

TEST_CASE("TUHT001A: list_temperament_names returns all", "[tuning][temperament][core]") {
    auto names = list_temperament_names();
    REQUIRE(names.size() == 5);
}

// =============================================================================
// Frequency calculation
// =============================================================================

TEST_CASE("TUHT001A: equal temperament frequency matches 12-EDO", "[tuning][temperament][core]") {
    // A4 (pc=9, octave=4) in equal temperament should be 440 Hz
    double f = tempered_frequency(9, 4, TUNING_EQUAL);
    REQUIRE_THAT(f, WithinRel(440.0, 1e-6));
}

TEST_CASE("TUHT001A: equal temperament C4", "[tuning][temperament][core]") {
    double f_equal = tempered_frequency(0, 4, TUNING_EQUAL);
    // C4 ≈ 261.63 Hz
    REQUIRE_THAT(f_equal, WithinRel(261.63, 0.001));
}

TEST_CASE("TUHT001A: tempered frequency differs from equal", "[tuning][temperament][core]") {
    // E4 in quarter-comma meantone should differ from 12-TET
    double f_equal = tempered_frequency(4, 4, TUNING_EQUAL);
    double f_meantone = tempered_frequency(4, 4, TUNING_QUARTER_COMMA_MEANTONE);

    // Meantone E is lower than 12-TET E (deviation is -13.69 cents)
    REQUIRE(f_meantone < f_equal);
}

TEST_CASE("TUHT001A: Pythagorean fifth is wider than equal", "[tuning][temperament][core]") {
    // G in Pythagorean has +1.96 cents deviation
    double f_g_pyth = tempered_frequency(7, 4, TUNING_PYTHAGOREAN);
    double f_g_equal = tempered_frequency(7, 4, TUNING_EQUAL);

    REQUIRE(f_g_pyth > f_g_equal);
}

TEST_CASE("TUHT001A: meantone major third is closer to just", "[tuning][temperament][core]") {
    // Just major third = 5/4 = 386.314 cents above root
    // 12-TET major third = 400 cents (error ~13.7)
    // Meantone E deviation = -13.69 cents → ~386.3 cents (near just)
    double f_c = tempered_frequency(0, 4, TUNING_QUARTER_COMMA_MEANTONE);
    double f_e = tempered_frequency(4, 4, TUNING_QUARTER_COMMA_MEANTONE);

    double meantone_third_cents = ratio_to_cents(f_e / f_c);
    double just_third_cents = ratio_to_cents(5.0 / 4.0);  // 386.314

    // Should be within 2 cents of just
    REQUIRE_THAT(meantone_third_cents, WithinAbs(just_third_cents, 2.0));
}
