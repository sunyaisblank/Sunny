/**
 * @file TSSL001A.cpp
 * @brief Unit tests for FMSL001A (Scala tuning file reader/writer)
 *
 * Component: TSSL001A
 * Domain: TS (Test) | Category: SL (Scala)
 *
 * Tests: FMSL001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Format/FMSL001A.h"

using namespace Sunny::Infrastructure::Format;
using Catch::Matchers::WithinAbs;

// =============================================================================
// ratio_to_cents
// =============================================================================

TEST_CASE("FMSL001A: ratio_to_cents perfect fifth", "[scala][format]") {
    double cents = ratio_to_cents(3, 2);
    REQUIRE_THAT(cents, WithinAbs(701.955, 0.001));
}

TEST_CASE("FMSL001A: ratio_to_cents octave", "[scala][format]") {
    double cents = ratio_to_cents(2, 1);
    REQUIRE_THAT(cents, WithinAbs(1200.0, 0.001));
}

TEST_CASE("FMSL001A: ratio_to_cents major third", "[scala][format]") {
    double cents = ratio_to_cents(5, 4);
    REQUIRE_THAT(cents, WithinAbs(386.314, 0.001));
}

// =============================================================================
// parse_scala: 12-TET
// =============================================================================

TEST_CASE("FMSL001A: parse 12-TET", "[scala][format]") {
    std::string scl =
        "! 12-TET\n"
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

TEST_CASE("FMSL001A: parse just intonation ratios", "[scala][format]") {
    std::string scl =
        "! Just intonation\n"
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
    REQUIRE_THAT(result->intervals[3].cents, WithinAbs(701.955, 0.001));  // 3/2
    REQUIRE_THAT(result->intervals[6].cents, WithinAbs(1200.0, 0.001));  // 2/1
}

// =============================================================================
// Round-trip: write → parse
// =============================================================================

TEST_CASE("FMSL001A: round-trip write then parse", "[scala][format]") {
    ScalaTuning original;
    original.description = "Test tuning";
    original.intervals = {
        {200.0, false, 0, 0},
        {400.0, false, 0, 0},
        {ratio_to_cents(3, 2), true, 3, 2},
        {1200.0, false, 0, 0},
    };

    std::string text = write_scala(original);
    auto parsed = parse_scala(text);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->intervals.size() == original.intervals.size());
    REQUIRE(parsed->description == original.description);

    // Ratio intervals preserve their ratio form
    REQUIRE(parsed->intervals[2].is_ratio);
    REQUIRE(parsed->intervals[2].ratio_num == 3);
    REQUIRE(parsed->intervals[2].ratio_den == 2);

    // Cents intervals preserve values
    REQUIRE_THAT(parsed->intervals[0].cents, WithinAbs(200.0, 0.001));
    REQUIRE_THAT(parsed->intervals[3].cents, WithinAbs(1200.0, 0.001));
}

// =============================================================================
// Error cases
// =============================================================================

TEST_CASE("FMSL001A: empty input", "[scala][format]") {
    auto result = parse_scala("");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Sunny::Core::ErrorCode::InvalidScalaFile);
}

TEST_CASE("FMSL001A: missing note count", "[scala][format]") {
    auto result = parse_scala("Description only\n");
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Sunny::Core::ErrorCode::InvalidScalaFile);
}

TEST_CASE("FMSL001A: non-numeric interval", "[scala][format]") {
    std::string scl =
        "Bad tuning\n"
        "1\n"
        "not_a_number\n";

    auto result = parse_scala(scl);
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == Sunny::Core::ErrorCode::InvalidScalaFile);
}

// =============================================================================
// scala_to_cent_table
// =============================================================================

TEST_CASE("FMSL001A: scala_to_cent_table 12-TET all zeros", "[scala][format]") {
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

TEST_CASE("FMSL001A: scala_to_cent_table wrong size", "[scala][format]") {
    ScalaTuning tuning;
    tuning.description = "7 note";
    tuning.intervals.resize(7);
    auto table = scala_to_cent_table(tuning);
    REQUIRE_FALSE(table.has_value());
    REQUIRE(table.error() == Sunny::Core::ErrorCode::FormatError);
}
