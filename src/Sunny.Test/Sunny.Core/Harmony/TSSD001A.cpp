/**
 * @file TSSD001A.cpp
 * @brief Unit tests for HRSD001A (Secondary dominants)
 *
 * Component: TSSD001A
 * Tests: HRSD001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Harmony/HRSD001A.h"
#include "Scale/SCDF001A.h"

using namespace Sunny::Core;

TEST_CASE("HRSD001A: parse_applied_chord", "[harmony][core]") {
    SECTION("V/V parses correctly") {
        auto ac = parse_applied_chord("V/V");
        REQUIRE(ac.has_value());
        REQUIRE(ac->applied_function == "V");
        REQUIRE(ac->target_degree == 4);
        REQUIRE(ac->target_is_major == true);
    }

    SECTION("V7/ii parses correctly") {
        auto ac = parse_applied_chord("V7/ii");
        REQUIRE(ac.has_value());
        REQUIRE(ac->applied_function == "V7");
        REQUIRE(ac->target_degree == 1);
        REQUIRE(ac->target_is_major == false);
    }

    SECTION("vii°/V parses correctly") {
        auto ac = parse_applied_chord("vii°/V");
        REQUIRE(ac.has_value());
        REQUIRE(ac->applied_function == "vii°");
        REQUIRE(ac->target_degree == 4);
    }

    SECTION("No slash fails") {
        auto ac = parse_applied_chord("V7");
        REQUIRE_FALSE(ac.has_value());
    }

    SECTION("Empty parts fail") {
        REQUIRE_FALSE(parse_applied_chord("/V").has_value());
        REQUIRE_FALSE(parse_applied_chord("V/").has_value());
        REQUIRE_FALSE(parse_applied_chord("").has_value());
    }

    SECTION("Invalid function fails") {
        REQUIRE_FALSE(parse_applied_chord("IV/V").has_value());
        REQUIRE_FALSE(parse_applied_chord("ii/V").has_value());
    }

    SECTION("Invalid target fails") {
        REQUIRE_FALSE(parse_applied_chord("V/X").has_value());
    }
}

TEST_CASE("HRSD001A: generate_secondary_dominant in C major", "[harmony][core]") {
    SECTION("V/V = D major") {
        auto result = generate_secondary_dominant("V/V", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 2);  // D
        REQUIRE(result->quality == "major");
        // D, F#, A = 62, 66, 69
        REQUIRE(result->notes[0] == 62);
        REQUIRE(result->notes[1] == 66);
        REQUIRE(result->notes[2] == 69);
    }

    SECTION("V7/V = D7") {
        auto result = generate_secondary_dominant("V7/V", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 2);
        REQUIRE(result->quality == "7");
        REQUIRE(result->notes.size() == 4);
    }

    SECTION("V/ii = A major") {
        auto result = generate_secondary_dominant("V/ii", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 9);  // A
        REQUIRE(result->quality == "major");
    }

    SECTION("V7/ii = A7") {
        auto result = generate_secondary_dominant("V7/ii", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 9);
        REQUIRE(result->quality == "7");
        REQUIRE(result->notes.size() == 4);
    }

    SECTION("V/IV = C major (same as tonic)") {
        auto result = generate_secondary_dominant("V/IV", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 0);  // C
        REQUIRE(result->quality == "major");
    }

    SECTION("V/vi = E major") {
        auto result = generate_secondary_dominant("V/vi", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 4);  // E
    }

    SECTION("V/iii = B major") {
        auto result = generate_secondary_dominant("V/iii", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 11);  // B
    }
}

TEST_CASE("HRSD001A: generate_secondary_dominant vii° type", "[harmony][core]") {
    SECTION("vii°/V = F# diminished") {
        auto result = generate_secondary_dominant("vii°/V", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 6);  // F#
        REQUIRE(result->quality == "diminished");
    }

    SECTION("viio7/V = F# dim7") {
        auto result = generate_secondary_dominant("viio7/V", 0, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 6);  // F#
        REQUIRE(result->quality == "dim7");
        REQUIRE(result->notes.size() == 4);
    }
}

TEST_CASE("HRSD001A: generate_secondary_dominant in G major", "[harmony][core]") {
    SECTION("V/V in G = A major") {
        auto result = generate_secondary_dominant("V/V", 7, SCALE_MAJOR, 4);
        REQUIRE(result.has_value());
        REQUIRE(result->root == 9);  // A
    }
}

TEST_CASE("HRSD001A: is_valid_secondary_target", "[harmony][core]") {
    SECTION("Degree 0 (I) is invalid target") {
        REQUIRE_FALSE(is_valid_secondary_target(0, SCALE_MAJOR, 0));
    }

    SECTION("Degree 6 (vii) is invalid target") {
        REQUIRE_FALSE(is_valid_secondary_target(6, SCALE_MAJOR, 0));
    }

    SECTION("Degrees 1-5 are valid targets") {
        for (int d = 1; d <= 5; ++d) {
            REQUIRE(is_valid_secondary_target(d, SCALE_MAJOR, 0));
        }
    }

    SECTION("Out of range fails") {
        REQUIRE_FALSE(is_valid_secondary_target(-1, SCALE_MAJOR, 0));
        REQUIRE_FALSE(is_valid_secondary_target(7, SCALE_MAJOR, 0));
    }
}

TEST_CASE("HRSD001A: invalid secondary dominant targets", "[harmony][core]") {
    SECTION("V/I is rejected") {
        auto result = generate_secondary_dominant("V/I", 0, SCALE_MAJOR, 4);
        REQUIRE_FALSE(result.has_value());
        REQUIRE(result.error() == ErrorCode::InvalidAppliedChord);
    }

    SECTION("V/vii is rejected") {
        auto result = generate_secondary_dominant("V/vii", 0, SCALE_MAJOR, 4);
        REQUIRE_FALSE(result.has_value());
    }
}
