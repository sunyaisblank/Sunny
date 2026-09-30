/**
 * @file historical_temperament_test.cpp
 * @brief Historical Temperaments tests
 *
 * Validates: Formal Spec §13.3
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sunny/core/tuning/historical_temperament.hpp>

using namespace sunny::core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// Tuning table invariants
// =============================================================================

TEST_CASE("equal temperament table is all zeros", "[tuning][temperament][core]") {
    for (int i = 0; i < 12; ++i) {
        REQUIRE(TUNING_EQUAL[i] == 0.0);
    }
}

TEST_CASE("Pythagorean C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_PYTHAGOREAN[0] == 0.0);
}

TEST_CASE("Werckmeister III C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_WERCKMEISTER_III[0] == 0.0);
}

TEST_CASE("Vallotti C deviation is zero", "[tuning][temperament][core]") {
    REQUIRE(TUNING_VALLOTTI[0] == 0.0);
}

// =============================================================================
// Temperament lookup
// =============================================================================

TEST_CASE("find_temperament known names", "[tuning][temperament][core]") {
    REQUIRE(find_temperament("equal").has_value());
    REQUIRE(find_temperament("pythagorean").has_value());
    REQUIRE(find_temperament("quarter_comma_meantone").has_value());
    REQUIRE(find_temperament("werckmeister_iii").has_value());
    REQUIRE(find_temperament("vallotti").has_value());
}

TEST_CASE("find_temperament unknown returns nullopt", "[tuning][temperament][core]") {
    REQUIRE_FALSE(find_temperament("baroque_magic").has_value());
}

TEST_CASE("list_temperament_names returns all", "[tuning][temperament][core]") {
    auto names = list_temperament_names();
    REQUIRE(names.size() == 5);
}

// =============================================================================
// Frequency calculation
// =============================================================================

TEST_CASE("equal temperament frequency matches 12-EDO", "[tuning][temperament][core]") {
    // A4 (pc=9, octave=4) in equal temperament should be 440 Hz
    auto f = tempered_frequency(9, 4, TUNING_EQUAL);
    REQUIRE(f.has_value());
    REQUIRE_THAT(*f, WithinRel(440.0, 1e-6));
}

TEST_CASE("equal temperament C4", "[tuning][temperament][core]") {
    auto f_equal = tempered_frequency(0, 4, TUNING_EQUAL);
    REQUIRE(f_equal.has_value());
    // C4 ≈ 261.63 Hz
    REQUIRE_THAT(*f_equal, WithinRel(261.63, 0.001));
}

TEST_CASE("tempered frequency differs from equal", "[tuning][temperament][core]") {
    // E4 in quarter-comma meantone should differ from 12-TET
    auto f_equal = tempered_frequency(4, 4, TUNING_EQUAL);
    auto f_meantone = tempered_frequency(4, 4, TUNING_QUARTER_COMMA_MEANTONE);
    REQUIRE(f_equal.has_value());
    REQUIRE(f_meantone.has_value());

    // Meantone E is lower than 12-TET E (deviation is -13.69 cents)
    REQUIRE(*f_meantone < *f_equal);
}

TEST_CASE("Pythagorean fifth is wider than equal", "[tuning][temperament][core]") {
    // G in Pythagorean has +1.96 cents deviation
    auto f_g_pyth = tempered_frequency(7, 4, TUNING_PYTHAGOREAN);
    auto f_g_equal = tempered_frequency(7, 4, TUNING_EQUAL);
    REQUIRE(f_g_pyth.has_value());
    REQUIRE(f_g_equal.has_value());

    REQUIRE(*f_g_pyth > *f_g_equal);
}

TEST_CASE("meantone major third is closer to just", "[tuning][temperament][core]") {
    // Just major third = 5/4 = 386.314 cents above root
    // 12-TET major third = 400 cents (error ~13.7)
    // Meantone E deviation = -13.69 cents → ~386.3 cents (near just)
    auto f_c = tempered_frequency(0, 4, TUNING_QUARTER_COMMA_MEANTONE);
    auto f_e = tempered_frequency(4, 4, TUNING_QUARTER_COMMA_MEANTONE);
    REQUIRE(f_c.has_value());
    REQUIRE(f_e.has_value());

    auto meantone_third_cents = ratio_to_cents(*f_e / *f_c);
    auto just_third_cents = ratio_to_cents(5.0 / 4.0); // 386.314
    REQUIRE(meantone_third_cents.has_value());
    REQUIRE(just_third_cents.has_value());

    // Should be within 2 cents of just
    REQUIRE_THAT(*meantone_third_cents, WithinAbs(*just_third_cents, 2.0));
}

// =============================================================================
// Precondition rejection tests
// =============================================================================

TEST_CASE("tempered_frequency rejects negative pitch_class", "[tuning][temperament][core]") {
    auto r = tempered_frequency(-1, 4, TUNING_EQUAL);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidPitchClass);
}

TEST_CASE("tempered_frequency rejects pitch_class > 11", "[tuning][temperament][core]") {
    auto r = tempered_frequency(12, 4, TUNING_EQUAL);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidPitchClass);
}

TEST_CASE("tempered_frequency rejects non-finite or non-positive frequency state",
          "[tuning][temperament][core][trust-boundary]") {
    auto non_finite_table = TUNING_EQUAL;
    non_finite_table[9] = std::numeric_limits<double>::quiet_NaN();

    for (const auto& result : {
             tempered_frequency(9, 4, TUNING_EQUAL, 0.0),
             tempered_frequency(9, 4, TUNING_EQUAL, -440.0),
             tempered_frequency(9, 4, TUNING_EQUAL, std::numeric_limits<double>::infinity()),
             tempered_frequency(9, 4, non_finite_table),
             tempered_frequency(9, std::numeric_limits<int>::max(), TUNING_EQUAL),
         }) {
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::InvalidFrequency);
    }
}
