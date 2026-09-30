/**
 * @file types_test.cpp
 * @brief Unit tests for Score IR foundation types
 *
 *
 * Coverage: Id<T>, PositiveRational, ScoreTime, enumerations,
 *           default_velocity, articulation_duration_factor, balance_factor
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <concepts>
#include <sunny/core/score/types.hpp>
#include <sunny/core/score/written_duration.hpp>

using namespace sunny::core;

// =============================================================================
// Id<T>
// =============================================================================

TEST_CASE("Id<T> equality and ordering", "[score-ir][types]") {
    ScoreId a{1};
    ScoreId b{1};
    ScoreId c{2};
    static_assert(!std::same_as<ScoreId, PartId>);

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
    // Id<Score> and Id<Part> are distinct types — no cross-comparison
}

TEST_CASE("Id<T> hash", "[score-ir][types]") {
    std::hash<ScoreId> h;
    ScoreId a{42};
    ScoreId b{42};
    ScoreId c{99};

    CHECK(h(a) == h(b));
    // Different ids likely produce different hashes (not guaranteed but expected)
    CHECK(h(a) != h(c));
}

// =============================================================================
// PositiveRational
// =============================================================================

TEST_CASE("PositiveRational operations", "[score-ir][types]") {
    SECTION("make_bpm creates integer BPM") {
        auto bpm = make_bpm(120);
        CHECK(bpm.numerator() == 120);
        CHECK(bpm.denominator() == 1);
        CHECK(bpm.to_float() == 120.0);
    }

    SECTION("Fractional BPM") {
        PositiveRational bpm{241, 2}; // 120.5 BPM
        CHECK(bpm.to_float() == Catch::Approx(120.5));
    }

    SECTION("Construction canonicalises equivalent rates") {
        PositiveRational bpm{240, 2};
        CHECK(bpm.numerator() == 120);
        CHECK(bpm.denominator() == 1);
        CHECK(bpm == PositiveRational{120, 1});
    }

    SECTION("Dynamic construction rejects non-positive components") {
        const auto valid = PositiveRational::from_ratio(363, 3);
        REQUIRE(valid.has_value());
        CHECK(*valid == PositiveRational{121, 1});

        const auto zero = PositiveRational::from_ratio(0, 1);
        REQUIRE_FALSE(zero.has_value());
        CHECK(zero.error() == ErrorCode::InvalidTempo);

        const auto negative_denominator = PositiveRational::from_ratio(120, -1);
        REQUIRE_FALSE(negative_denominator.has_value());
        CHECK(negative_denominator.error() == ErrorCode::InvalidTempo);
    }

    SECTION("Ordering") {
        PositiveRational a{120, 1};
        PositiveRational b{121, 1};
        CHECK(a < b);
        CHECK(b > a);
        CHECK(a == PositiveRational{120, 1});
    }
}

// =============================================================================
// ScoreTime
// =============================================================================

TEST_CASE("ScoreTime ordering", "[score-ir][types]") {
    ScoreTime a{1, Beat::zero()};
    ScoreTime b{1, Beat{1, 4}};
    ScoreTime c{2, Beat::zero()};

    CHECK(a == SCORE_START);
    CHECK(a < b);
    CHECK(b < c);
    CHECK(a < c);
    CHECK(a <= a);
    CHECK(c > a);
    CHECK(c >= c);
}

// =============================================================================
// default_velocity
// =============================================================================

TEST_CASE("default_velocity ranges", "[score-ir][types]") {
    CHECK(default_velocity(DynamicLevel::pppp) == 12);
    CHECK(default_velocity(DynamicLevel::p) == 56);
    CHECK(default_velocity(DynamicLevel::mf) == 88);
    CHECK(default_velocity(DynamicLevel::f) == 104);
    CHECK(default_velocity(DynamicLevel::ffff) == 127);
    CHECK(default_velocity(DynamicLevel::sfz) == 120);
}

// =============================================================================
// articulation_duration_factor
// =============================================================================

TEST_CASE("articulation_duration_factor", "[score-ir][types]") {
    CHECK(articulation_duration_factor(ArticulationType::Staccato) == 0.50);
    CHECK(articulation_duration_factor(ArticulationType::Staccatissimo) == 0.25);
    CHECK(articulation_duration_factor(ArticulationType::Tenuto) == 1.00);
    CHECK(articulation_duration_factor(ArticulationType::Fermata) == 1.75);
    CHECK(articulation_duration_factor(ArticulationType::Accent) == 1.00);
}

// =============================================================================
// balance_factor
// =============================================================================

TEST_CASE("balance_factor", "[score-ir][types]") {
    CHECK(balance_factor(DynamicBalance::Foreground) == 1.00);
    CHECK(balance_factor(DynamicBalance::MiddleGround) == 0.85);
    CHECK(balance_factor(DynamicBalance::Background) == 0.70);
}

// =============================================================================
// Written duration projection (issue #10)
// =============================================================================

TEST_CASE("written duration splits unwritable dyadic values into tied glyphs",
          "[score-ir][types][notation][regression]") {
    // 5/16 = 1/4 + 1/16 and 5/8 = 1/2 + 1/8: no single glyph, two tied ones.
    // 7/16 is a double-dotted quarter.
    const auto five_sixteenths = project_written_duration(Beat{5, 16});
    REQUIRE(five_sixteenths.has_value());
    REQUIRE(five_sixteenths->pieces.size() == 2);
    CHECK(five_sixteenths->pieces[0].value.exponent == -2);
    CHECK(five_sixteenths->pieces[0].value.dots == 0);
    CHECK(five_sixteenths->pieces[0].sounding == Beat{1, 4});
    CHECK(five_sixteenths->pieces[1].value.exponent == -4);
    CHECK(five_sixteenths->pieces[1].sounding == Beat{1, 16});

    const auto five_eighths = project_written_duration(Beat{5, 8});
    REQUIRE(five_eighths.has_value());
    REQUIRE(five_eighths->pieces.size() == 2);
    CHECK(five_eighths->pieces[0].sounding == Beat{1, 2});
    CHECK(five_eighths->pieces[1].sounding == Beat{1, 8});

    const auto double_dotted = project_written_duration(Beat{7, 16});
    REQUIRE(double_dotted.has_value());
    REQUIRE(double_dotted->pieces.size() == 1);
    CHECK(double_dotted->pieces[0].value.exponent == -2);
    CHECK(double_dotted->pieces[0].value.dots == 2);
}

TEST_CASE("written duration writes tuplet members at their written value",
          "[score-ir][types][notation][regression]") {
    // Inside a 3:2 tuplet a 1/12 note is written 1/12 x 3/2 = 1/8, an eighth.
    const auto member = project_written_duration(Beat{1, 12}, Beat{3, 2});
    REQUIRE(member.has_value());
    REQUIRE(member->pieces.size() == 1);
    CHECK(member->pieces[0].value.exponent == -3);
    CHECK(member->inferred_actual == 1);

    // Outside any tuplet the duration implies its own ratio: 1/12 is an
    // eighth under 3:2, 1/20 a sixteenth under 5:4, 1/28 a sixteenth under 7:4.
    const auto triplet = project_written_duration(Beat{1, 12});
    REQUIRE(triplet.has_value());
    CHECK(triplet->inferred_actual == 3);
    CHECK(triplet->inferred_normal == 2);
    CHECK(triplet->pieces[0].value.exponent == -3);
    const auto quintuplet = project_written_duration(Beat{1, 20});
    REQUIRE(quintuplet.has_value());
    CHECK(quintuplet->inferred_actual == 5);
    CHECK(quintuplet->inferred_normal == 4);
    CHECK(quintuplet->pieces[0].value.exponent == -4);
    const auto septuplet = project_written_duration(Beat{1, 28});
    REQUIRE(septuplet.has_value());
    CHECK(septuplet->inferred_actual == 7);
    CHECK(septuplet->inferred_normal == 4);
    CHECK(septuplet->pieces[0].value.exponent == -4);
}

TEST_CASE("written duration reports durations with no written form",
          "[score-ir][types][notation][regression]") {
    // Shorter than the shortest admitted glyph, or non-dyadic inside a tuplet.
    CHECK_FALSE(project_written_duration(Beat{1, 2048}).has_value());
    CHECK_FALSE(project_written_duration(Beat{1, 256}, Beat::one(), -7).has_value());
    CHECK_FALSE(project_written_duration(Beat{1, 10}, Beat{3, 2}).has_value());
}
