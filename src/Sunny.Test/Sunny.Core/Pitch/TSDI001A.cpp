/**
 * @file TSDI001A.cpp
 * @brief Unit tests for PTDI001A (Diatonic interval operations)
 *
 * Component: TSDI001A
 * Domain: TS (Test) | Category: DI (Diatonic Interval)
 *
 * Tests: PTDI001A
 * Coverage: interval_number, is_simple, is_compound, default_chromatic_size,
 *           deviation, quality, quality_abbreviation, interval arithmetic,
 *           interval_from_spelled_pitches, apply_interval, from_quality_and_number,
 *           round-trip invariant
 */

#include <catch2/catch_test_macros.hpp>

#include "Pitch/PTDI001A.h"

using namespace Sunny::Core;

// =============================================================================
// interval_number()
// =============================================================================

TEST_CASE("PTDI001A: interval_number computes 1-based number", "[interval][diatonic]") {
    CHECK(interval_number(PERFECT_UNISON) == 1);
    CHECK(interval_number(MINOR_SECOND) == 2);
    CHECK(interval_number(MAJOR_SECOND) == 2);
    CHECK(interval_number(MINOR_THIRD) == 3);
    CHECK(interval_number(MAJOR_THIRD) == 3);
    CHECK(interval_number(PERFECT_FOURTH) == 4);
    CHECK(interval_number(PERFECT_FIFTH) == 5);
    CHECK(interval_number(MINOR_SIXTH) == 6);
    CHECK(interval_number(MAJOR_SIXTH) == 6);
    CHECK(interval_number(MINOR_SEVENTH) == 7);
    CHECK(interval_number(MAJOR_SEVENTH) == 7);
    CHECK(interval_number(PERFECT_OCTAVE) == 8);

    SECTION("Compound intervals") {
        CHECK(interval_number({14, 8}) == 9);   // M9
        CHECK(interval_number({16, 9}) == 10);  // m10
        CHECK(interval_number({24, 14}) == 15); // P15 (double octave)
    }

    SECTION("Descending intervals") {
        CHECK(interval_number(interval_negate(PERFECT_FIFTH)) == 5);
        CHECK(interval_number(interval_negate(MAJOR_THIRD)) == 3);
    }
}

// =============================================================================
// is_simple() / is_compound()
// =============================================================================

TEST_CASE("PTDI001A: is_simple and is_compound classification", "[interval][diatonic]") {
    CHECK(is_simple(PERFECT_UNISON));
    CHECK(is_simple(MAJOR_SEVENTH));
    CHECK(is_simple({11, 6}));  // diatonic 6 = seventh
    CHECK_FALSE(is_simple(PERFECT_OCTAVE));
    CHECK_FALSE(is_simple({14, 8}));  // ninth

    CHECK_FALSE(is_compound(PERFECT_UNISON));
    CHECK_FALSE(is_compound(MAJOR_SEVENTH));
    CHECK(is_compound(PERFECT_OCTAVE));
    CHECK(is_compound({14, 8}));
}

// =============================================================================
// default_chromatic_size()
// =============================================================================

TEST_CASE("PTDI001A: default_chromatic_size for simple intervals", "[interval][diatonic]") {
    CHECK(default_chromatic_size(0) == 0);
    CHECK(default_chromatic_size(1) == 2);
    CHECK(default_chromatic_size(2) == 4);
    CHECK(default_chromatic_size(3) == 5);
    CHECK(default_chromatic_size(4) == 7);
    CHECK(default_chromatic_size(5) == 9);
    CHECK(default_chromatic_size(6) == 11);
}

TEST_CASE("PTDI001A: default_chromatic_size for compound intervals", "[interval][diatonic]") {
    CHECK(default_chromatic_size(7) == 12);   // octave
    CHECK(default_chromatic_size(8) == 14);   // ninth
    CHECK(default_chromatic_size(14) == 24);  // double octave
}

TEST_CASE("PTDI001A: default_chromatic_size for negative intervals", "[interval][diatonic]") {
    CHECK(default_chromatic_size(-1) == -2);
    CHECK(default_chromatic_size(-4) == -7);
    CHECK(default_chromatic_size(-7) == -12);
}

// =============================================================================
// deviation()
// =============================================================================

TEST_CASE("PTDI001A: deviation identifies quality offset", "[interval][diatonic]") {
    CHECK(deviation(PERFECT_UNISON) == 0);
    CHECK(deviation(PERFECT_FIFTH) == 0);
    CHECK(deviation(MAJOR_THIRD) == 0);
    CHECK(deviation(MINOR_THIRD) == -1);
    CHECK(deviation(AUGMENTED_FOURTH) == 1);
    CHECK(deviation(DIMINISHED_FIFTH) == -1);
    CHECK(deviation(MINOR_SECOND) == -1);
    CHECK(deviation(MAJOR_SEVENTH) == 0);
}

// =============================================================================
// quality() and quality_abbreviation()
// =============================================================================

TEST_CASE("PTDI001A: quality assigns correct quality strings", "[interval][diatonic]") {
    SECTION("Simple intervals") {
        CHECK(quality(PERFECT_UNISON) == "P");
        CHECK(quality(MINOR_SECOND) == "m");
        CHECK(quality(MAJOR_SECOND) == "M");
        CHECK(quality(MINOR_THIRD) == "m");
        CHECK(quality(MAJOR_THIRD) == "M");
        CHECK(quality(PERFECT_FOURTH) == "P");
        CHECK(quality(AUGMENTED_FOURTH) == "A");
        CHECK(quality(DIMINISHED_FIFTH) == "d");
        CHECK(quality(PERFECT_FIFTH) == "P");
        CHECK(quality(MINOR_SIXTH) == "m");
        CHECK(quality(MAJOR_SIXTH) == "M");
        CHECK(quality(MINOR_SEVENTH) == "m");
        CHECK(quality(MAJOR_SEVENTH) == "M");
        CHECK(quality(PERFECT_OCTAVE) == "P");
    }

    SECTION("Augmented and diminished") {
        CHECK(quality({8, 4}) == "A");   // Augmented fifth
        CHECK(quality({4, 3}) == "d");   // Diminished fourth
    }

    SECTION("Doubly augmented and diminished") {
        CHECK(quality({9, 4}) == "AA");  // Doubly augmented fifth
        CHECK(quality({3, 3}) == "dd");  // Doubly diminished fourth
        CHECK(quality({0, 2}) == "ddd");  // Triply diminished third (default=4, dev=-4)
    }
}

TEST_CASE("PTDI001A: quality_abbreviation formats correctly", "[interval][diatonic]") {
    CHECK(quality_abbreviation(PERFECT_FIFTH) == "P5");
    CHECK(quality_abbreviation(MINOR_THIRD) == "m3");
    CHECK(quality_abbreviation(MAJOR_SEVENTH) == "M7");
    CHECK(quality_abbreviation(AUGMENTED_FOURTH) == "A4");
    CHECK(quality_abbreviation(DIMINISHED_FIFTH) == "d5");
    CHECK(quality_abbreviation(PERFECT_UNISON) == "P1");
    CHECK(quality_abbreviation(PERFECT_OCTAVE) == "P8");
}

// =============================================================================
// interval_from_spelled_pitches()
// =============================================================================

TEST_CASE("PTDI001A: interval_from_spelled_pitches computes correctly", "[interval][diatonic]") {
    SECTION("Simple ascending intervals") {
        // C4 -> E4 = M3
        auto m3 = interval_from_spelled_pitches({0, 0, 4}, {2, 0, 4});
        CHECK(m3 == MAJOR_THIRD);

        // C4 -> G4 = P5
        auto p5 = interval_from_spelled_pitches({0, 0, 4}, {4, 0, 4});
        CHECK(p5 == PERFECT_FIFTH);

        // C4 -> C5 = P8
        auto p8 = interval_from_spelled_pitches({0, 0, 4}, {0, 0, 5});
        CHECK(p8 == PERFECT_OCTAVE);

        // C4 -> Eb4 = m3
        auto mi3 = interval_from_spelled_pitches({0, 0, 4}, {2, -1, 4});
        CHECK(mi3 == MINOR_THIRD);
    }

    SECTION("Descending intervals") {
        // G4 -> C4 = descending P5
        auto dp5 = interval_from_spelled_pitches({4, 0, 4}, {0, 0, 4});
        CHECK(dp5 == DiatonicInterval{-7, -4});
    }

    SECTION("Enharmonic distinction") {
        // C4 -> F#4 = A4 (augmented fourth)
        auto a4 = interval_from_spelled_pitches({0, 0, 4}, {3, 1, 4});
        CHECK(a4 == AUGMENTED_FOURTH);

        // C4 -> Gb4 = d5 (diminished fifth)
        auto d5 = interval_from_spelled_pitches({0, 0, 4}, {4, -1, 4});
        CHECK(d5 == DIMINISHED_FIFTH);
    }

    SECTION("Cross-octave intervals") {
        // B3 -> C4 = m2
        auto m2 = interval_from_spelled_pitches({6, 0, 3}, {0, 0, 4});
        CHECK(m2 == MINOR_SECOND);

        // F#4 -> C#5 = P5
        auto p5 = interval_from_spelled_pitches({3, 1, 4}, {0, 1, 5});
        CHECK(p5 == PERFECT_FIFTH);
    }
}

// =============================================================================
// interval_add()
// =============================================================================

TEST_CASE("PTDI001A: interval_add arithmetic", "[interval][diatonic]") {
    // M3 + m3 = P5
    CHECK(interval_add(MAJOR_THIRD, MINOR_THIRD) == PERFECT_FIFTH);

    // P4 + P5 = P8
    CHECK(interval_add(PERFECT_FOURTH, PERFECT_FIFTH) == PERFECT_OCTAVE);

    // m2 + m2 = d3
    auto d3 = interval_add(MINOR_SECOND, MINOR_SECOND);
    CHECK(d3 == DiatonicInterval{2, 2});
    CHECK(quality(d3) == "d");

    // M3 + M3 = A5
    auto a5 = interval_add(MAJOR_THIRD, MAJOR_THIRD);
    CHECK(a5 == DiatonicInterval{8, 4});
    CHECK(quality(a5) == "A");

    // Identity: P1 + X = X
    CHECK(interval_add(PERFECT_UNISON, PERFECT_FIFTH) == PERFECT_FIFTH);
    CHECK(interval_add(PERFECT_UNISON, MINOR_THIRD) == MINOR_THIRD);

    // operator+ syntax
    CHECK((MAJOR_THIRD + MINOR_THIRD) == PERFECT_FIFTH);
}

// =============================================================================
// interval_negate()
// =============================================================================

TEST_CASE("PTDI001A: interval_negate reverses direction", "[interval][diatonic]") {
    CHECK(interval_negate(PERFECT_FIFTH) == DiatonicInterval{-7, -4});
    CHECK(interval_negate(MAJOR_THIRD) == DiatonicInterval{-4, -2});
    CHECK(interval_negate(PERFECT_UNISON) == PERFECT_UNISON);

    // Double negation identity
    CHECK(interval_negate(interval_negate(MINOR_SEVENTH)) == MINOR_SEVENTH);
}

// =============================================================================
// interval_invert()
// =============================================================================

TEST_CASE("PTDI001A: interval_invert within octave", "[interval][diatonic]") {
    // P5 <-> P4
    CHECK(interval_invert(PERFECT_FIFTH) == PERFECT_FOURTH);
    CHECK(interval_invert(PERFECT_FOURTH) == PERFECT_FIFTH);

    // M3 <-> m6
    CHECK(interval_invert(MAJOR_THIRD) == MINOR_SIXTH);
    CHECK(interval_invert(MINOR_SIXTH) == MAJOR_THIRD);

    // M7 <-> m2
    CHECK(interval_invert(MAJOR_SEVENTH) == MINOR_SECOND);

    // A4 <-> d5
    CHECK(interval_invert(AUGMENTED_FOURTH) == DIMINISHED_FIFTH);

    // Invariant: i + invert(i) == P8
    DiatonicInterval simple_intervals[] = {
        PERFECT_UNISON, MINOR_SECOND, MAJOR_SECOND, MINOR_THIRD, MAJOR_THIRD,
        PERFECT_FOURTH, AUGMENTED_FOURTH, DIMINISHED_FIFTH, PERFECT_FIFTH,
        MINOR_SIXTH, MAJOR_SIXTH, MINOR_SEVENTH, MAJOR_SEVENTH
    };
    for (auto i : simple_intervals) {
        auto sum = interval_add(i, interval_invert(i));
        CHECK(sum == PERFECT_OCTAVE);
    }
}

// =============================================================================
// apply_interval()
// =============================================================================

TEST_CASE("PTDI001A: apply_interval computes correct pitches", "[interval][diatonic]") {
    SECTION("Basic intervals from C4") {
        // C4 + M3 = E4
        CHECK(apply_interval({0, 0, 4}, MAJOR_THIRD) == SpelledPitch{2, 0, 4});

        // C4 + m3 = Eb4
        CHECK(apply_interval({0, 0, 4}, MINOR_THIRD) == SpelledPitch{2, -1, 4});

        // C4 + P5 = G4
        CHECK(apply_interval({0, 0, 4}, PERFECT_FIFTH) == SpelledPitch{4, 0, 4});

        // C4 + P8 = C5
        CHECK(apply_interval({0, 0, 4}, PERFECT_OCTAVE) == SpelledPitch{0, 0, 5});
    }

    SECTION("Octave boundary crossing") {
        // B3 + m2 = C4
        CHECK(apply_interval({6, 0, 3}, MINOR_SECOND) == SpelledPitch{0, 0, 4});

        // A4 + M3 = C#5
        CHECK(apply_interval({5, 0, 4}, MAJOR_THIRD) == SpelledPitch{0, 1, 5});
    }

    SECTION("Sharp/flat results") {
        // F#4 + P5 = C#5
        CHECK(apply_interval({3, 1, 4}, PERFECT_FIFTH) == SpelledPitch{0, 1, 5});

        // Bb3 + M3 = D4
        CHECK(apply_interval({6, -1, 3}, MAJOR_THIRD) == SpelledPitch{1, 0, 4});

        // Eb4 + P5 = Bb4
        CHECK(apply_interval({2, -1, 4}, PERFECT_FIFTH) == SpelledPitch{6, -1, 4});
    }

    SECTION("Descending intervals") {
        // G4 - P5 = C4
        auto desc_p5 = interval_negate(PERFECT_FIFTH);
        CHECK(apply_interval({4, 0, 4}, desc_p5) == SpelledPitch{0, 0, 4});

        // C4 - m2 = B3
        auto desc_m2 = interval_negate(MINOR_SECOND);
        CHECK(apply_interval({0, 0, 4}, desc_m2) == SpelledPitch{6, 0, 3});
    }
}

// =============================================================================
// Round-trip: apply(sp1, interval(sp1, sp2)) == sp2
// =============================================================================

TEST_CASE("PTDI001A: round-trip apply/interval invariant", "[interval][diatonic]") {
    SpelledPitch pitches[] = {
        {0, 0, 4},   // C4
        {0, 1, 4},   // C#4
        {1, -1, 4},  // Db4
        {2, 0, 4},   // E4
        {3, 1, 4},   // F#4
        {4, -1, 4},  // Gb4
        {6, 0, 3},   // B3
        {5, 0, 5},   // A5
        {0, 0, -1},  // C-1
        {2, -1, 3},  // Eb3
        {6, -1, 2},  // Bb2
        {0, 2, 4},   // C##4
        {1, -2, 4},  // Dbb4
    };

    int n = sizeof(pitches) / sizeof(pitches[0]);
    for (int i = 0; i < n; ++i) {
        for (int j = 0; j < n; ++j) {
            auto di = interval_from_spelled_pitches(pitches[i], pitches[j]);
            auto result = apply_interval(pitches[i], di);
            INFO("from=" << to_spn(pitches[i]) << " to=" << to_spn(pitches[j])
                 << " interval=(" << di.chromatic << "," << di.diatonic << ")");
            CHECK(result == pitches[j]);
        }
    }
}

// =============================================================================
// from_quality_and_number()
// =============================================================================

TEST_CASE("PTDI001A: from_quality_and_number parsing", "[interval][diatonic]") {
    SECTION("Perfect family") {
        auto p1 = from_quality_and_number("P1");
        REQUIRE(p1.has_value());
        CHECK(*p1 == PERFECT_UNISON);

        auto p5 = from_quality_and_number("P5");
        REQUIRE(p5.has_value());
        CHECK(*p5 == PERFECT_FIFTH);

        auto p8 = from_quality_and_number("P8");
        REQUIRE(p8.has_value());
        CHECK(*p8 == PERFECT_OCTAVE);
    }

    SECTION("Major and minor") {
        auto m3 = from_quality_and_number("m3");
        REQUIRE(m3.has_value());
        CHECK(*m3 == MINOR_THIRD);

        auto M7 = from_quality_and_number("M7");
        REQUIRE(M7.has_value());
        CHECK(*M7 == MAJOR_SEVENTH);

        auto M2 = from_quality_and_number("M2");
        REQUIRE(M2.has_value());
        CHECK(*M2 == MAJOR_SECOND);
    }

    SECTION("Augmented and diminished") {
        auto a4 = from_quality_and_number("A4");
        REQUIRE(a4.has_value());
        CHECK(*a4 == AUGMENTED_FOURTH);

        auto d5 = from_quality_and_number("d5");
        REQUIRE(d5.has_value());
        CHECK(*d5 == DIMINISHED_FIFTH);
    }

    SECTION("Doubly augmented/diminished") {
        auto aa5 = from_quality_and_number("AA5");
        REQUIRE(aa5.has_value());
        CHECK(aa5->chromatic == 9);
        CHECK(aa5->diatonic == 4);

        auto dd4 = from_quality_and_number("dd4");
        REQUIRE(dd4.has_value());
        CHECK(dd4->chromatic == 3);
        CHECK(dd4->diatonic == 3);
    }

    SECTION("Error cases") {
        // P3 is invalid (3rd is imperfect family)
        CHECK(!from_quality_and_number("P3").has_value());

        // m5 is invalid (5th is perfect family)
        CHECK(!from_quality_and_number("m5").has_value());

        // M4 is invalid (4th is perfect family)
        CHECK(!from_quality_and_number("M4").has_value());

        // Empty string
        CHECK(!from_quality_and_number("").has_value());

        // No number
        CHECK(!from_quality_and_number("P").has_value());
    }

    SECTION("Round-trip with quality_abbreviation") {
        DiatonicInterval intervals[] = {
            PERFECT_UNISON, MINOR_SECOND, MAJOR_SECOND,
            MINOR_THIRD, MAJOR_THIRD, PERFECT_FOURTH,
            AUGMENTED_FOURTH, DIMINISHED_FIFTH, PERFECT_FIFTH,
            MINOR_SIXTH, MAJOR_SIXTH, MINOR_SEVENTH,
            MAJOR_SEVENTH, PERFECT_OCTAVE
        };
        for (auto di : intervals) {
            auto abbr = quality_abbreviation(di);
            auto parsed = from_quality_and_number(abbr);
            INFO("abbreviation: " << abbr);
            REQUIRE(parsed.has_value());
            CHECK(*parsed == di);
        }
    }
}

// =============================================================================
// is_perfect_family()
// =============================================================================

TEST_CASE("PTDI001A: is_perfect_family classification", "[interval][diatonic]") {
    CHECK(is_perfect_family(0));   // unison
    CHECK_FALSE(is_perfect_family(1));  // second
    CHECK_FALSE(is_perfect_family(2));  // third
    CHECK(is_perfect_family(3));   // fourth
    CHECK(is_perfect_family(4));   // fifth
    CHECK_FALSE(is_perfect_family(5));  // sixth
    CHECK_FALSE(is_perfect_family(6));  // seventh
}

// =============================================================================
// Named constants verify against computed values
// =============================================================================

TEST_CASE("PTDI001A: named constants match computed intervals", "[interval][diatonic]") {
    // Verify each named constant matches what interval_from_spelled_pitches gives
    SpelledPitch c4 = {0, 0, 4};

    CHECK(interval_from_spelled_pitches(c4, {0, 0, 4}) == PERFECT_UNISON);
    CHECK(interval_from_spelled_pitches(c4, {1, -1, 4}) == MINOR_SECOND);
    CHECK(interval_from_spelled_pitches(c4, {1, 0, 4}) == MAJOR_SECOND);
    CHECK(interval_from_spelled_pitches(c4, {2, -1, 4}) == MINOR_THIRD);
    CHECK(interval_from_spelled_pitches(c4, {2, 0, 4}) == MAJOR_THIRD);
    CHECK(interval_from_spelled_pitches(c4, {3, 0, 4}) == PERFECT_FOURTH);
    CHECK(interval_from_spelled_pitches(c4, {3, 1, 4}) == AUGMENTED_FOURTH);
    CHECK(interval_from_spelled_pitches(c4, {4, -1, 4}) == DIMINISHED_FIFTH);
    CHECK(interval_from_spelled_pitches(c4, {4, 0, 4}) == PERFECT_FIFTH);
    CHECK(interval_from_spelled_pitches(c4, {5, -1, 4}) == MINOR_SIXTH);
    CHECK(interval_from_spelled_pitches(c4, {5, 0, 4}) == MAJOR_SIXTH);
    CHECK(interval_from_spelled_pitches(c4, {6, -1, 4}) == MINOR_SEVENTH);
    CHECK(interval_from_spelled_pitches(c4, {6, 0, 4}) == MAJOR_SEVENTH);
    CHECK(interval_from_spelled_pitches(c4, {0, 0, 5}) == PERFECT_OCTAVE);
}
