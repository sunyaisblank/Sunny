/**
 * @file acoustics_test.cpp
 * @brief Unit tests for Acoustics components (§14.1–14.4)
 *
 * Validates: Formal Spec §14.1–14.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <sunny/core/acoustics/consonance.hpp>
#include <sunny/core/acoustics/harmonic_series.hpp>
#include <sunny/core/acoustics/roughness.hpp>
#include <sunny/core/acoustics/virtual_pitch.hpp>

using namespace sunny::core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// §14.1 Harmonic Series//
// =============================================================================

TEST_CASE("harmonic series table size", "[acoustics][core]") {
    REQUIRE(HARMONIC_SERIES.size() == 16);
}

TEST_CASE("fundamental is partial 1 with zero deviation", "[acoustics][core]") {
    REQUIRE(HARMONIC_SERIES[0].partial == 1);
    REQUIRE(HARMONIC_SERIES[0].ratio == 1.0);
    REQUIRE(HARMONIC_SERIES[0].cents_deviation == 0.0);
}

TEST_CASE("octaves have zero deviation", "[acoustics][core]") {
    // Partials 1, 2, 4, 8, 16 are exact octaves
    REQUIRE(HARMONIC_SERIES[0].cents_deviation == 0.0);  // 1
    REQUIRE(HARMONIC_SERIES[1].cents_deviation == 0.0);  // 2
    REQUIRE(HARMONIC_SERIES[3].cents_deviation == 0.0);  // 4
    REQUIRE(HARMONIC_SERIES[7].cents_deviation == 0.0);  // 8
    REQUIRE(HARMONIC_SERIES[15].cents_deviation == 0.0); // 16
}

TEST_CASE("partial_frequency basic", "[acoustics][core]") {
    REQUIRE(*partial_frequency(1, 440.0) == 440.0);
    REQUIRE(*partial_frequency(2, 440.0) == 880.0);
    REQUIRE(*partial_frequency(3, 440.0) == 1320.0);
}

TEST_CASE("harmonic_spectrum rolloff", "[acoustics][core]") {
    auto spectrum_r = harmonic_spectrum(100.0, 4, 1.0);
    REQUIRE(spectrum_r.has_value());
    auto& spectrum = *spectrum_r;
    REQUIRE(spectrum.size() == 4);

    REQUIRE(spectrum[0].first == 100.0);
    REQUIRE(spectrum[0].second == 1.0);

    REQUIRE(spectrum[1].first == 200.0);
    REQUIRE_THAT(spectrum[1].second, WithinRel(0.5, 1e-10));

    REQUIRE(spectrum[2].first == 300.0);
    REQUIRE_THAT(spectrum[2].second, WithinRel(1.0 / 3.0, 1e-10));

    REQUIRE(spectrum[3].first == 400.0);
    REQUIRE_THAT(spectrum[3].second, WithinRel(0.25, 1e-10));
}

// =============================================================================
// §14.2 Consonance / Dissonance//
// =============================================================================

TEST_CASE("critical_bandwidth is positive", "[acoustics][core]") {
    REQUIRE(*critical_bandwidth(100.0) > 0.0);
    REQUIRE(*critical_bandwidth(1000.0) > 0.0);
    REQUIRE(*critical_bandwidth(5000.0) > 0.0);
}

TEST_CASE("critical_bandwidth increases with frequency", "[acoustics][core]") {
    REQUIRE(*critical_bandwidth(2000.0) > *critical_bandwidth(500.0));
}

TEST_CASE("plomp_levelt unison is zero", "[acoustics][core]") {
    REQUIRE(*plomp_levelt_dissonance(440.0, 440.0) == 0.0);
}

TEST_CASE("plomp_levelt near-unison has dissonance", "[acoustics][core]") {
    // Slight detuning produces roughness
    auto d = plomp_levelt_dissonance(440.0, 445.0);
    REQUIRE(d.has_value());
    REQUIRE(*d > 0.0);
}

TEST_CASE("plomp_levelt wide interval has low dissonance", "[acoustics][core]") {
    // Octave apart — well beyond critical bandwidth
    auto d_octave = plomp_levelt_dissonance(220.0, 440.0);
    auto d_close = plomp_levelt_dissonance(440.0, 460.0);
    REQUIRE(d_octave.has_value());
    REQUIRE(d_close.has_value());
    REQUIRE(*d_close > *d_octave);
}

TEST_CASE("sethares_dissonance with harmonic tones", "[acoustics][core]") {
    auto tone_a = *harmonic_spectrum(220.0, 6, 1.0);
    auto tone_b = *harmonic_spectrum(330.0, 6, 1.0); // Perfect fifth

    auto d_fifth = sethares_dissonance(tone_a, tone_b);
    REQUIRE(d_fifth.has_value());

    // Compare with minor second (dissonant)
    auto tone_c = *harmonic_spectrum(233.08, 6, 1.0); // ~m2 above
    auto d_m2 = sethares_dissonance(tone_a, tone_c);
    REQUIRE(d_m2.has_value());

    // Fifth should be more consonant (less dissonance) than minor second
    REQUIRE(*d_fifth < *d_m2);
}

TEST_CASE("dissonance_curve retains complex-spectrum cross-pair roughness", "[acoustics][core]") {
    auto partials = *harmonic_spectrum(220.0, 6, 1.0);
    auto curve_r = dissonance_curve(partials, 1.0, 2.0, 50);
    REQUIRE(curve_r.has_value());
    auto& curve = *curve_r;

    REQUIRE(curve.size() == 51);
    // Decimal evaluation of the published cross-pair loop, i,j=1..6,
    // frequencies220*i/220*j, amplitudes1/i and1/j. Equal indices
    // contribute zero, while other partial pairs retain a nonzero tail.
    REQUIRE(curve.front().first == 1.0);
    REQUIRE_THAT(curve.front().second, WithinAbs(0.04632791693759504, 1e-13));
    CHECK(curve[1].first == 1.02);
    CHECK(curve[1].second > curve.front().second);
    CHECK(curve.back().first == 2.0);
}

// =============================================================================
// §14.3 Roughness// =============================================================================

TEST_CASE("roughness equal pure tones is zero", "[acoustics][core]") {
    const std::vector<std::pair<double, double>> tone{{440.0, 1.0}};
    auto r = roughness(tone, tone);
    REQUIRE(r.has_value());
    REQUIRE(*r == 0.0);
}

TEST_CASE("Sethares pair values match the published frequency fit", "[acoustics][core][evidence]") {
    // Decimal hand evaluation of Sethares's constants, independent of this API:
    // s=.24/(.0207*440+18.96), d=5*(exp(-3.51*s*20)-exp(-5.75*s*20)).
    constexpr double expected = 0.873030315214684;
    REQUIRE_THAT(*plomp_levelt_dissonance(440.0, 460.0), WithinAbs(expected, 1e-13));
    REQUIRE_THAT(*plomp_levelt_dissonance(460.0, 440.0), WithinAbs(expected, 1e-13));
    // Analytic derivative vanishes at x=ln(5.75/3.51)/(5.75-3.51).
    REQUIRE_THAT(*plomp_levelt_dissonance(440.0, 465.76992296290943),
                 WithinAbs(0.898782427191725, 1e-13));
    CHECK(*plomp_levelt_dissonance(440.0, 464.0) < 0.898782427191725);
    CHECK(*plomp_levelt_dissonance(440.0, 467.0) < 0.898782427191725);
    const std::vector<std::pair<double, double>> a{{440.0, 0.5}}, b{{460.0, 0.25}};
    REQUIRE_THAT(*sethares_dissonance(a, b), WithinAbs(expected / 4.0, 1e-13));
    REQUIRE_THAT(*roughness_product(a, b), WithinAbs(expected / 8.0, 1e-13));
}

TEST_CASE("identical complex spectra retain unequal-frequency contributions",
          "[acoustics][core][evidence]") {
    const std::vector<std::pair<double, double>> spectrum{{440.0, 1.0}, {460.0, 1.0}};
    // Two equal-frequency pairs contribute zero; two cross pairs each .873030315214684.
    REQUIRE_THAT(*roughness(spectrum, spectrum), WithinAbs(1.746060630429368, 1e-13));
}

TEST_CASE("roughness_product positive for detuned tones", "[acoustics][core]") {
    auto tone_a = *harmonic_spectrum(440.0, 4, 1.0);
    auto tone_b = *harmonic_spectrum(445.0, 4, 1.0);
    auto r = roughness_product(tone_a, tone_b);
    REQUIRE(r.has_value());
    REQUIRE(*r > 0.0);
}

// =============================================================================
// §14.4 Virtual Pitch//
// =============================================================================

TEST_CASE("virtual_pitch from complete harmonic series", "[acoustics][core]") {
    // Partials 1–5 of 100 Hz
    std::vector<double> freqs = {100.0, 200.0, 300.0, 400.0, 500.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(100.0, 0.01));
    REQUIRE(result->confidence >= 0.9);
}

TEST_CASE("virtual_pitch missing fundamental", "[acoustics][core]") {
    // Partials 2–5 of 100 Hz (fundamental absent)
    std::vector<double> freqs = {200.0, 300.0, 400.0, 500.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(100.0, 0.01));
}

TEST_CASE("virtual_pitch from high partials", "[acoustics][core]") {
    // Partials 3, 4, 5 of 110 Hz
    std::vector<double> freqs = {330.0, 440.0, 550.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(110.0, 0.02));
}

TEST_CASE("virtual_pitch empty returns nullopt", "[acoustics][core]") {
    std::vector<double> empty;
    REQUIRE_FALSE(virtual_pitch(empty).has_value());
}

TEST_CASE("virtual_pitch confidence reflects match quality", "[acoustics][core]") {
    // Perfect harmonic series → high confidence
    std::vector<double> perfect = {100.0, 200.0, 300.0, 400.0};
    auto result = virtual_pitch(perfect);
    REQUIRE(result.has_value());
    REQUIRE(result->confidence >= 0.9);
}

// =============================================================================
// Precondition rejection tests
// =============================================================================

TEST_CASE("partial_frequency rejects n < 1", "[acoustics][core]") {
    auto r = partial_frequency(0, 440.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidPartialNumber);
}

TEST_CASE("partial_frequency rejects f <= 0", "[acoustics][core]") {
    auto r = partial_frequency(1, 0.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("partial_frequency rejects negative f", "[acoustics][core]") {
    auto r = partial_frequency(1, -100.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("harmonic_spectrum rejects f <= 0", "[acoustics][core]") {
    auto r = harmonic_spectrum(0.0, 4, 1.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("harmonic_spectrum rejects n_partials < 1", "[acoustics][core]") {
    auto r = harmonic_spectrum(440.0, 0, 1.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidPartialNumber);
}

TEST_CASE("critical_bandwidth rejects f <= 0", "[acoustics][core]") {
    auto r = critical_bandwidth(0.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("critical_bandwidth rejects negative f", "[acoustics][core]") {
    auto r = critical_bandwidth(-100.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("plomp_levelt_dissonance rejects f1 <= 0", "[acoustics][core]") {
    auto r = plomp_levelt_dissonance(0.0, 440.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("plomp_levelt_dissonance rejects f2 <= 0", "[acoustics][core]") {
    auto r = plomp_levelt_dissonance(440.0, -1.0);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("acoustic APIs reject nonfinite input and invalid sweep controls",
          "[acoustics][core][evidence]") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double inf = std::numeric_limits<double>::infinity();
    for (double frequency : {nan, inf, -inf, 0.0, -1.0}) {
        CHECK_FALSE(partial_frequency(1, frequency));
        CHECK_FALSE(harmonic_spectrum(frequency, 2));
        CHECK_FALSE(critical_bandwidth(frequency));
        CHECK_FALSE(plomp_levelt_dissonance(frequency, 440.0));
        CHECK_FALSE(plomp_levelt_dissonance(440.0, frequency));
        const std::vector<std::pair<double, double>> invalid{{frequency, 1.0}};
        CHECK_FALSE(sethares_dissonance(invalid, {}));
        CHECK_FALSE(roughness_product({}, invalid));
        CHECK_FALSE(dissonance_curve(invalid));
        CHECK_FALSE(virtual_pitch(std::vector<double>{frequency, 440.0}));
    }
    const std::vector<std::pair<double, double>> tone{{440.0, 1.0}};
    for (double amplitude : {nan, inf, -1.0}) {
        const std::vector<std::pair<double, double>> invalid{{440.0, amplitude}};
        CHECK_FALSE(sethares_dissonance(invalid, tone));
        CHECK_FALSE(roughness_product(tone, invalid));
    }
    CHECK(*sethares_dissonance(std::vector<std::pair<double, double>>{{460.0, 0.0}}, tone) == 0.0);
    CHECK_FALSE(harmonic_spectrum(440.0, 2, nan));
    CHECK_FALSE(harmonic_spectrum(440.0, 2, -1.0));
    CHECK_FALSE(dissonance_curve(tone, 1.0, 2.0, 0));
    CHECK_FALSE(dissonance_curve(tone, 1.0, 2.0, -1));
    CHECK_FALSE(dissonance_curve(tone, 2.0, 1.0));
    CHECK_FALSE(dissonance_curve(tone, 0.0, 2.0));
    CHECK_FALSE(dissonance_curve(tone, nan, 2.0));
    CHECK_FALSE(dissonance_curve(tone, 1.0, inf));
    REQUIRE(dissonance_curve(tone, 1.0, 2.0, 2)->size() == 3);
    CHECK_FALSE(virtual_pitch(std::vector<double>{440.0}, nan));
    CHECK_FALSE(virtual_pitch(std::vector<double>{440.0}, -1.0));
    CHECK_FALSE(virtual_pitch(std::vector<double>{440.0}, 50.0, 0));
}

TEST_CASE("acoustic arithmetic overflow is explicit", "[acoustics][core][evidence]") {
    const auto check_overflow = [](const auto& result) {
        REQUIRE_FALSE(result);
        CHECK(result.error() == ErrorCode::ArithmeticOverflow);
    };
    const double largest = std::numeric_limits<double>::max();
    check_overflow(partial_frequency(2, largest));
    check_overflow(harmonic_spectrum(largest, 2));
    check_overflow(critical_bandwidth(largest));
    const std::vector<std::pair<double, double>> low{{440.0, largest}, {440.0, largest}};
    const std::vector<std::pair<double, double>> high{{460.0, largest}};
    check_overflow(sethares_dissonance(low, high));
    check_overflow(roughness_product(low, high));
    CHECK(*roughness_product(low, low) == 0.0);
    check_overflow(
        dissonance_curve(std::vector<std::pair<double, double>>{{largest, 1.0}}, 1.0, 2.0));
    auto estimate = virtual_pitch(std::vector<double>{largest, largest}, 0.0, 1);
    REQUIRE(estimate);
    CHECK(estimate->frequency == largest);
    CHECK(estimate->confidence == 1.0);
}

TEST_CASE("virtual pitch reports support for its returned candidate",
          "[acoustics][core][evidence]") {
    // At 50 cents, 100 Hz matches 97.5/100/102, while 102 Hz matches
    // 100/102/104.5. Their average 101 Hz matches only 100/102.
    auto estimate = virtual_pitch(std::vector<double>{97.5, 100.0, 102.0, 104.5}, 50.0, 16);
    REQUIRE(estimate);
    CHECK(estimate->frequency == 102.0);
    CHECK(estimate->confidence == 0.75);
    // 1700/100=17 exceeds the requested harmonic bound, so 100 Hz cannot
    // claim both observations. Highest candidate wins the one-match tie.
    estimate = virtual_pitch(std::vector<double>{100.0, 1700.0}, 0.0, 16);
    REQUIRE(estimate);
    CHECK(estimate->frequency == 1700.0);
    CHECK(estimate->confidence == 0.5);
}
