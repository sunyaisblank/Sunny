/**
 * @file historical_temperament_test.cpp
 * @brief Historical Temperaments tests
 *
 * Validates: Formal Spec §13.3
 */

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
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
// Fifth-chain invariant
//
// Each temperament is defined by the size of its twelve fifths. The fifth
// above pitch class p spans 700 + table[p+7] - table[p] cents, because the
// table holds deviations from 12-TET. Expected sizes are computed here from
// the defining ratios with std::log2, independently of the header constants:
//   pure fifth         P  = 1200 log2(3/2)          ≈ 701.955
//   syntonic comma     SC = 1200 log2(81/80)        ≈  21.506
//   Pythagorean comma  PC = 1200 log2(3^12 / 2^19)  ≈  23.460
// =============================================================================

namespace {

constexpr int C = 0, Cs = 1, D = 2, Eb = 3, E = 4, F = 5, Fs = 6, G = 7, Gs = 8, A = 9, Bb = 10,
              B = 11;

double pure_fifth() {
    return 1200.0 * std::log2(3.0 / 2.0);
}
double syntonic_comma() {
    return 1200.0 * std::log2(81.0 / 80.0);
}
double pythagorean_comma() {
    return 1200.0 * std::log2(531441.0 / 524288.0);
}

double fifth_above(const TuningTable& table, int pc) {
    return 700.0 + table[static_cast<std::size_t>((pc + 7) % 12)] -
           table[static_cast<std::size_t>(pc)];
}

void check_fifths(const TuningTable& table, const std::array<double, 12>& expected) {
    for (int pc = 0; pc < 12; ++pc) {
        INFO("fifth above pitch class " << pc);
        CHECK_THAT(fifth_above(table, pc), WithinAbs(expected[static_cast<std::size_t>(pc)], 0.01));
    }
}

std::array<double, 12> fifths_all(double size) {
    std::array<double, 12> fifths{};
    fifths.fill(size);
    return fifths;
}

} // namespace

TEST_CASE("Pythagorean: eleven pure fifths and the wolf on G#-Eb",
          "[tuning][temperament][core]") {
    auto expected = fifths_all(pure_fifth());
    expected[Gs] = 8400.0 - 11.0 * pure_fifth(); // ≈ 678.49
    check_fifths(TUNING_PYTHAGOREAN, expected);
}

TEST_CASE("Quarter-comma meantone: eleven fifths of P - SC/4 and the wolf on G#-Eb",
          "[tuning][temperament][core]") {
    const double meantone_fifth = pure_fifth() - syntonic_comma() / 4.0; // ≈ 696.58
    auto expected = fifths_all(meantone_fifth);
    expected[Gs] = 8400.0 - 11.0 * meantone_fifth; // ≈ 737.64
    check_fifths(TUNING_QUARTER_COMMA_MEANTONE, expected);

    // Four tempered fifths less two octaves is a pure 5/4 for every third
    // that does not cross the wolf: Eb-G, Bb-D, F-A, C-E, G-B, D-F#, A-C#,
    // E-G#.
    const double pure_third = 1200.0 * std::log2(5.0 / 4.0);
    for (int root : {Eb, Bb, F, C, G, D, A, E}) {
        INFO("major third above pitch class " << root);
        const double third = 400.0 +
                             TUNING_QUARTER_COMMA_MEANTONE[static_cast<std::size_t>((root + 4) % 12)] -
                             TUNING_QUARTER_COMMA_MEANTONE[static_cast<std::size_t>(root)];
        CHECK_THAT(third, WithinAbs(pure_third, 0.01));
    }
}

TEST_CASE("Werckmeister III: C-G, G-D, D-A and B-F# narrowed by PC/4, the rest pure",
          "[tuning][temperament][core]") {
    auto expected = fifths_all(pure_fifth());
    for (int pc : {C, G, D, B}) {
        expected[static_cast<std::size_t>(pc)] = pure_fifth() - pythagorean_comma() / 4.0;
    }
    check_fifths(TUNING_WERCKMEISTER_III, expected);

    // Published deviations from 12-TET (Werckmeister 1691, C = 0).
    CHECK_THAT(TUNING_WERCKMEISTER_III[Cs], WithinAbs(-9.78, 0.01));
    CHECK_THAT(TUNING_WERCKMEISTER_III[E], WithinAbs(-9.78, 0.01));
    CHECK_THAT(TUNING_WERCKMEISTER_III[B], WithinAbs(-7.82, 0.01));
}

TEST_CASE("Vallotti: F-C-G-D-A-E-B narrowed by PC/6, the rest pure",
          "[tuning][temperament][core]") {
    auto expected = fifths_all(pure_fifth());
    for (int pc : {F, C, G, D, A, E}) {
        expected[static_cast<std::size_t>(pc)] = pure_fifth() - pythagorean_comma() / 6.0;
    }
    check_fifths(TUNING_VALLOTTI, expected);
}

TEST_CASE("Every table is anchored at C and closes the circle of fifths",
          "[tuning][temperament][core]") {
    for (const auto& table : {TUNING_EQUAL,
                              TUNING_PYTHAGOREAN,
                              TUNING_QUARTER_COMMA_MEANTONE,
                              TUNING_WERCKMEISTER_III,
                              TUNING_VALLOTTI}) {
        CHECK(table[C] == 0.0);
        double circle = 0.0;
        for (int pc = 0; pc < 12; ++pc)
            circle += fifth_above(table, pc);
        CHECK_THAT(circle, WithinAbs(8400.0, 1e-9));
    }
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

    // Meantone E is four narrowed fifths above C: -SC = -13.69 cents
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
