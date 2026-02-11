/**
 * @file TSAC001A.cpp
 * @brief Unit tests for Acoustics components (§14.1–14.4)
 *
 * Component: TSAC001A
 * Tests: ACHS001A, ACPL001A, ACRG001A, ACVP001A
 * Validates: Formal Spec §14.1–14.4
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Acoustics/ACHS001A.h"
#include "Acoustics/ACPL001A.h"
#include "Acoustics/ACRG001A.h"
#include "Acoustics/ACVP001A.h"

using namespace Sunny::Core;
using Catch::Matchers::WithinAbs;
using Catch::Matchers::WithinRel;

// =============================================================================
// §14.1 Harmonic Series (ACHS001A)
// =============================================================================

TEST_CASE("ACHS001A: harmonic series table size", "[acoustics][core]") {
    REQUIRE(HARMONIC_SERIES.size() == 16);
}

TEST_CASE("ACHS001A: fundamental is partial 1 with zero deviation", "[acoustics][core]") {
    REQUIRE(HARMONIC_SERIES[0].partial == 1);
    REQUIRE(HARMONIC_SERIES[0].ratio == 1.0);
    REQUIRE(HARMONIC_SERIES[0].cents_deviation == 0.0);
}

TEST_CASE("ACHS001A: octaves have zero deviation", "[acoustics][core]") {
    // Partials 1, 2, 4, 8, 16 are exact octaves
    REQUIRE(HARMONIC_SERIES[0].cents_deviation == 0.0);   // 1
    REQUIRE(HARMONIC_SERIES[1].cents_deviation == 0.0);   // 2
    REQUIRE(HARMONIC_SERIES[3].cents_deviation == 0.0);   // 4
    REQUIRE(HARMONIC_SERIES[7].cents_deviation == 0.0);   // 8
    REQUIRE(HARMONIC_SERIES[15].cents_deviation == 0.0);  // 16
}

TEST_CASE("ACHS001A: partial_frequency basic", "[acoustics][core]") {
    REQUIRE(partial_frequency(1, 440.0) == 440.0);
    REQUIRE(partial_frequency(2, 440.0) == 880.0);
    REQUIRE(partial_frequency(3, 440.0) == 1320.0);
}

TEST_CASE("ACHS001A: harmonic_spectrum rolloff", "[acoustics][core]") {
    auto spectrum = harmonic_spectrum(100.0, 4, 1.0);
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
// §14.2 Consonance / Dissonance (ACPL001A)
// =============================================================================

TEST_CASE("ACPL001A: critical_bandwidth is positive", "[acoustics][core]") {
    REQUIRE(critical_bandwidth(100.0) > 0.0);
    REQUIRE(critical_bandwidth(1000.0) > 0.0);
    REQUIRE(critical_bandwidth(5000.0) > 0.0);
}

TEST_CASE("ACPL001A: critical_bandwidth increases with frequency", "[acoustics][core]") {
    REQUIRE(critical_bandwidth(2000.0) > critical_bandwidth(500.0));
}

TEST_CASE("ACPL001A: plomp_levelt unison is zero", "[acoustics][core]") {
    REQUIRE(plomp_levelt_dissonance(440.0, 440.0) == 0.0);
}

TEST_CASE("ACPL001A: plomp_levelt near-unison has dissonance", "[acoustics][core]") {
    // Slight detuning produces roughness
    double d = plomp_levelt_dissonance(440.0, 445.0);
    REQUIRE(d > 0.0);
}

TEST_CASE("ACPL001A: plomp_levelt wide interval has low dissonance", "[acoustics][core]") {
    // Octave apart — well beyond critical bandwidth
    double d_octave = plomp_levelt_dissonance(220.0, 440.0);
    double d_close = plomp_levelt_dissonance(440.0, 460.0);
    REQUIRE(d_close > d_octave);
}

TEST_CASE("ACPL001A: sethares_dissonance with harmonic tones", "[acoustics][core]") {
    auto tone_a = harmonic_spectrum(220.0, 6, 1.0);
    auto tone_b = harmonic_spectrum(330.0, 6, 1.0);  // Perfect fifth

    double d_fifth = sethares_dissonance(tone_a, tone_b);

    // Compare with minor second (dissonant)
    auto tone_c = harmonic_spectrum(233.08, 6, 1.0);  // ~m2 above
    double d_m2 = sethares_dissonance(tone_a, tone_c);

    // Fifth should be more consonant (less dissonance) than minor second
    REQUIRE(d_fifth < d_m2);
}

TEST_CASE("ACPL001A: dissonance_curve has unison minimum", "[acoustics][core]") {
    auto partials = harmonic_spectrum(220.0, 6, 1.0);
    auto curve = dissonance_curve(partials, 1.0, 2.0, 50);

    REQUIRE(curve.size() == 51);
    // Unison (ratio 1.0) should have low dissonance
    REQUIRE(curve.front().first == 1.0);
    REQUIRE_THAT(curve.front().second, WithinAbs(0.0, 0.01));
}

// =============================================================================
// §14.3 Roughness (ACRG001A)
// =============================================================================

TEST_CASE("ACRG001A: roughness identical tones is near-zero", "[acoustics][core]") {
    auto tone = harmonic_spectrum(440.0, 4, 1.0);
    // Self-roughness at unison — all partials match exactly
    double r = roughness(tone, tone);
    REQUIRE_THAT(r, WithinAbs(0.0, 1e-10));
}

TEST_CASE("ACRG001A: roughness_product positive for detuned tones", "[acoustics][core]") {
    auto tone_a = harmonic_spectrum(440.0, 4, 1.0);
    auto tone_b = harmonic_spectrum(445.0, 4, 1.0);
    double r = roughness_product(tone_a, tone_b);
    REQUIRE(r > 0.0);
}

// =============================================================================
// §14.4 Virtual Pitch (ACVP001A)
// =============================================================================

TEST_CASE("ACVP001A: virtual_pitch from complete harmonic series", "[acoustics][core]") {
    // Partials 1–5 of 100 Hz
    std::vector<double> freqs = {100.0, 200.0, 300.0, 400.0, 500.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(100.0, 0.01));
    REQUIRE(result->confidence >= 0.9);
}

TEST_CASE("ACVP001A: virtual_pitch missing fundamental", "[acoustics][core]") {
    // Partials 2–5 of 100 Hz (fundamental absent)
    std::vector<double> freqs = {200.0, 300.0, 400.0, 500.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(100.0, 0.01));
}

TEST_CASE("ACVP001A: virtual_pitch from high partials", "[acoustics][core]") {
    // Partials 3, 4, 5 of 110 Hz
    std::vector<double> freqs = {330.0, 440.0, 550.0};
    auto result = virtual_pitch(freqs);
    REQUIRE(result.has_value());
    REQUIRE_THAT(result->frequency, WithinRel(110.0, 0.02));
}

TEST_CASE("ACVP001A: virtual_pitch empty returns nullopt", "[acoustics][core]") {
    std::vector<double> empty;
    REQUIRE_FALSE(virtual_pitch(empty).has_value());
}

TEST_CASE("ACVP001A: virtual_pitch confidence reflects match quality", "[acoustics][core]") {
    // Perfect harmonic series → high confidence
    std::vector<double> perfect = {100.0, 200.0, 300.0, 400.0};
    auto result = virtual_pitch(perfect);
    REQUIRE(result.has_value());
    REQUIRE(result->confidence >= 0.9);
}
