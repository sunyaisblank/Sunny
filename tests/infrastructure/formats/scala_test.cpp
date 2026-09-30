/**
 * @file scala_test.cpp
 * @brief Scala tuning file reader/writer unit tests
 *
 *
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <sunny/infrastructure/formats/scala.hpp>

using namespace sunny::infrastructure::formats;
using Catch::Matchers::WithinAbs;

// =============================================================================
// ratio_to_cents
// =============================================================================

TEST_CASE("ratio_to_cents perfect fifth", "[scala][format]") {
    const auto cents = ratio_to_cents(3, 2);
    REQUIRE(cents.has_value());
    REQUIRE_THAT(*cents, WithinAbs(701.955, 0.001));
}

TEST_CASE("ratio_to_cents octave", "[scala][format]") {
    const auto cents = ratio_to_cents(2, 1);
    REQUIRE(cents.has_value());
    REQUIRE_THAT(*cents, WithinAbs(1200.0, 0.001));
}

TEST_CASE("ratio_to_cents major third", "[scala][format]") {
    const auto cents = ratio_to_cents(5, 4);
    REQUIRE(cents.has_value());
    REQUIRE_THAT(*cents, WithinAbs(386.314, 0.001));
}

TEST_CASE("ratio_to_cents rejects a non-positive ratio", "[scala][format][trust-boundary]") {
    CHECK_FALSE(ratio_to_cents(0, 1).has_value());
    CHECK_FALSE(ratio_to_cents(1, 0).has_value());
    CHECK_FALSE(ratio_to_cents(-3, 2).has_value());
}

// =============================================================================
// parse_scala: 12-TET
// =============================================================================

TEST_CASE("parse 12-TET", "[scala][format]") {
    std::string scl = "! 12-TET\n"
                      "!\n"
                      "12 equal temperament\n"
                      "12\n"
                      "!\n"
                      "100.000000\n"
                      "200.000000\n"
                      "300.000000\n"
                      "400.000000\n"
                      "500.000000\n"
                      "600.000000\n"
                      "700.000000\n"
                      "800.000000\n"
                      "900.000000\n"
                      "1000.000000\n"
                      "1100.000000\n"
                      "1200.000000\n";

    auto result = parse_scala(scl);
    REQUIRE(result.has_value());
    REQUIRE(result->intervals.size() == 12);
    REQUIRE_THAT(result->intervals[0].cents, WithinAbs(100.0, 0.001));
    REQUIRE_THAT(result->intervals[11].cents, WithinAbs(1200.0, 0.001));
    REQUIRE_FALSE(result->intervals[0].is_ratio);
}

// =============================================================================
// parse_scala: Just intonation
// =============================================================================

TEST_CASE("parse just intonation ratios", "[scala][format]") {
    std::string scl = "! Just intonation\n"
                      "Just intonation 7-note\n"
                      "7\n"
                      "!\n"
                      "9/8\n"
                      "5/4\n"
                      "4/3\n"
                      "3/2\n"
                      "5/3\n"
                      "15/8\n"
                      "2/1\n";

    auto result = parse_scala(scl);
    REQUIRE(result.has_value());
    REQUIRE(result->intervals.size() == 7);
    REQUIRE(result->intervals[0].is_ratio);
    REQUIRE(result->intervals[0].ratio_num == 9);
    REQUIRE(result->intervals[0].ratio_den == 8);
    REQUIRE_THAT(result->intervals[3].cents, WithinAbs(701.955, 0.001)); // 3/2
    REQUIRE_THAT(result->intervals[6].cents, WithinAbs(1200.0, 0.001));  // 2/1
}

TEST_CASE("general Scala scales expand over the complete Score pitch domain",
          "[scala][format][score-ir][tuning]") {
    ScalaTuning tritave;
    tritave.description = "Bohlen-Pierce fragment";
    tritave.intervals = {
        {146.3, false, 0, 0},
        {292.6, false, 0, 0},
        {1901.9550008653873, false, 0, 0},
    };
    auto expanded = scala_to_score_tuning(tritave, 60, 261.6255653005986);
    REQUIRE(expanded.has_value());
    CHECK(expanded->cents_from_reference[60] == 0.0);
    CHECK(expanded->cents_from_reference[61] == 146.3);
    CHECK(expanded->cents_from_reference[62] == 292.6);
    CHECK(expanded->cents_from_reference[63] == 1901.9550008653873);
    CHECK(expanded->cents_from_reference[59] == Catch::Approx(-1901.9550008653873 + 292.6));
    REQUIRE(sunny::core::validate_score_tuning(*expanded).has_value());
}

TEST_CASE("Scala to Score rejects an absent or non-positive recurrence period",
          "[scala][format][score-ir][tuning]") {
    CHECK_FALSE(scala_to_score_tuning(ScalaTuning{}).has_value());
    ScalaTuning invalid{"zero period", {{0.0, false, 0, 0}}};
    CHECK_FALSE(scala_to_score_tuning(invalid).has_value());
}

// =============================================================================
// Round-trip: write → parse
// =============================================================================

TEST_CASE("round-trip write then parse", "[scala][format]") {
    ScalaTuning original;
    original.description = "Test tuning";
    original.intervals = {
        {200.12345678901234, false, 0, 0},
        {400.0, false, 0, 0},
        {*ratio_to_cents(3, 2), true, 3, 2, "perfect fifth"},
        {1200.0, false, 0, 0},
    };

    const auto text = write_scala(original);
    REQUIRE(text.has_value());
    auto parsed = parse_scala(*text);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->intervals.size() == original.intervals.size());
    REQUIRE(parsed->description == original.description);

    // Ratio intervals preserve their ratio form
    REQUIRE(parsed->intervals[2].is_ratio);
    REQUIRE(parsed->intervals[2].ratio_num == 3);
    REQUIRE(parsed->intervals[2].ratio_den == 2);
    REQUIRE(parsed->intervals[2].trailing_text == "perfect fifth");

    // Cents intervals preserve values
    REQUIRE(parsed->intervals[0].cents == original.intervals[0].cents);
    REQUIRE(parsed->intervals[1].cents == original.intervals[1].cents);
    REQUIRE(parsed->intervals[3].cents == original.intervals[3].cents);
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("empty input", "[scala][format]") {
    auto result = parse_scala("");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidScalaFile);
}

TEST_CASE("missing note count", "[scala][format]") {
    auto result = parse_scala("Description only\n");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidScalaFile);
}

TEST_CASE("non-numeric interval", "[scala][format]") {
    std::string scl = "Bad tuning\n"
                      "1\n"
                      "not_a_number\n";

    auto result = parse_scala(scl);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == sunny::core::ErrorCode::InvalidScalaFile);
}

TEST_CASE("Scala parser preserves an explicitly empty description", "[scala][format]") {
    const auto result = parse_scala("\n2\n100.0\n2/1\n");
    REQUIRE(result.has_value());
    CHECK(result->description.empty());
    CHECK(result->intervals.size() == 2);
}

TEST_CASE("Scala parser accepts specified whitespace and trailing pitch text",
          "[scala][format][trust-boundary]") {
    const auto result = parse_scala("Labels\n3\n5 / 4 major-third\n100.0 cents\n2 octave\n");
    REQUIRE(result.has_value());
    REQUIRE(result->intervals.size() == 3);
    CHECK(result->intervals[0].is_ratio);
    CHECK(result->intervals[0].ratio_num == 5);
    CHECK(result->intervals[0].ratio_den == 4);
    CHECK(result->intervals[0].trailing_text == "major-third");
    CHECK_FALSE(result->intervals[1].is_ratio);
    CHECK(result->intervals[1].trailing_text == "cents");
    CHECK(result->intervals[2].ratio_num == 2);
    CHECK(result->intervals[2].ratio_den == 1);
    CHECK(result->intervals[2].trailing_text == "octave");
}

TEST_CASE("Scala parser rejects malformed numeric tokens and line counts",
          "[scala][format][trust-boundary]") {
    const std::vector<std::string> invalid = {
        "Bad count\n1junk\n100.0\n",
        "Negative numerator\n1\n-3/2\n",
        "Negative denominator\n1\n3/-2\n",
        "Junk numerator\n1\n3x/2\n",
        "Junk denominator\n1\n3/2x\n",
        "Two slashes\n1\n3/2/1\n",
        "Attached cents text\n1\n100.0x\n",
        "Non-finite cents\n1\n1.0e999\n",
        "Extra degree\n1\n100.0\n200.0\n",
    };
    for (const auto& text : invalid) {
        CAPTURE(text);
        const auto result = parse_scala(text);
        CHECK_FALSE(result.has_value());
        if (!result) CHECK(result.error() == sunny::core::ErrorCode::InvalidScalaFile);
    }
}

TEST_CASE("Scala writer rejects states that cannot form one valid scale file",
          "[scala][format][trust-boundary]") {
    ScalaTuning tuning;
    tuning.description = "Valid";
    tuning.intervals.push_back({100.0, false, 0, 0});

    SECTION("multiline description") {
        tuning.description = "line one\nline two";
        CHECK_FALSE(write_scala(tuning).has_value());
    }
    SECTION("comment-shaped description") {
        tuning.description = "! this would be parsed as a comment";
        CHECK_FALSE(write_scala(tuning).has_value());
    }
    SECTION("non-finite cents") {
        tuning.intervals[0].cents = std::numeric_limits<double>::infinity();
        CHECK_FALSE(write_scala(tuning).has_value());
    }
    SECTION("non-positive ratio") {
        tuning.intervals[0] = {0.0, true, -3, 2};
        CHECK_FALSE(write_scala(tuning).has_value());
    }
    SECTION("contradictory ratio cache") {
        tuning.intervals[0] = {100.0, true, 3, 2};
        CHECK_FALSE(write_scala(tuning).has_value());
    }
    SECTION("non-canonical or multiline trailing text") {
        tuning.intervals[0].trailing_text = " padded ";
        CHECK_FALSE(write_scala(tuning).has_value());
        tuning.intervals[0].trailing_text = "line one\nline two";
        CHECK_FALSE(write_scala(tuning).has_value());
    }
}

// =============================================================================
// scala_to_cent_table
// =============================================================================

TEST_CASE("scala_to_cent_table 12-TET all zeros", "[scala][format]") {
    ScalaTuning tet;
    tet.description = "12-TET";
    for (int i = 1; i <= 12; ++i) {
        tet.intervals.push_back({100.0 * i, false, 0, 0});
    }

    auto table = scala_to_cent_table(tet);
    REQUIRE(table.has_value());
    for (int i = 0; i < 12; ++i) {
        REQUIRE_THAT((*table)[static_cast<std::size_t>(i)], WithinAbs(0.0, 0.001));
    }
}

TEST_CASE("scala_to_cent_table wrong size", "[scala][format]") {
    ScalaTuning tuning;
    tuning.description = "7 note";
    tuning.intervals.resize(7);
    auto table = scala_to_cent_table(tuning);
    REQUIRE_FALSE(table.has_value());
    REQUIRE(table.error() == sunny::core::ErrorCode::FormatError);
}

TEST_CASE("scala_to_cent_table rejects a non-octave formal period", "[scala][format]") {
    ScalaTuning tuning;
    tuning.description = "12 divisions of a tritave";
    for (int i = 1; i <= 12; ++i) {
        tuning.intervals.push_back({1901.9550008653874 * i / 12.0, false, 0, 0});
    }

    auto table = scala_to_cent_table(tuning);
    REQUIRE_FALSE(table.has_value());
    REQUIRE(table.error() == sunny::core::ErrorCode::FormatError);

    tuning.intervals.back() = {*ratio_to_cents(2, 1), true, 2, 1};
    REQUIRE(scala_to_cent_table(tuning).has_value());
}
