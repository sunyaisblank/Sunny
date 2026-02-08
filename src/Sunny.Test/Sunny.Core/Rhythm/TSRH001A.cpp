/**
 * @file TSRH001A.cpp
 * @brief Unit tests for RHEU001A (Euclidean rhythm)
 *
 * Component: TSRH001A
 * Domain: TS (Test) | Category: RH (Rhythm)
 *
 * Tests: RHEU001A
 * Coverage: euclidean_rhythm, euclidean_to_events, euclidean_preset
 */

#include <catch2/catch_test_macros.hpp>

#include "Rhythm/RHEU001A.h"

using namespace Sunny::Core;

TEST_CASE("RHEU001A: euclidean_rhythm basic invariants", "[rhythm][core]") {
    SECTION("Length equals steps") {
        auto result = euclidean_rhythm(3, 8, 0);
        REQUIRE(result.has_value());
        CHECK(result->size() == 8);
    }

    SECTION("Pulse count equals pulses parameter") {
        auto result = euclidean_rhythm(5, 12, 0);
        REQUIRE(result.has_value());
        int count = 0;
        for (bool b : *result) if (b) count++;
        CHECK(count == 5);
    }

    SECTION("Edge case: all rests") {
        auto result = euclidean_rhythm(0, 8, 0);
        REQUIRE(result.has_value());
        for (bool b : *result) CHECK(b == false);
    }

    SECTION("Edge case: all pulses") {
        auto result = euclidean_rhythm(8, 8, 0);
        REQUIRE(result.has_value());
        for (bool b : *result) CHECK(b == true);
    }
}

TEST_CASE("RHEU001A: euclidean_rhythm known patterns", "[rhythm][core]") {
    SECTION("E(3,8) - Tresillo") {
        auto result = euclidean_rhythm(3, 8, 0);
        REQUIRE(result.has_value());
        // Pattern: x..x..x. (1,0,0,1,0,0,1,0)
        std::vector<bool> expected = {true, false, false, true, false, false, true, false};
        CHECK(*result == expected);
    }

    SECTION("E(5,8) - Cinquillo") {
        auto result = euclidean_rhythm(5, 8, 0);
        REQUIRE(result.has_value());
        // Pattern: x.xx.xx. (1,0,1,1,0,1,1,0)
        std::vector<bool> expected = {true, false, true, true, false, true, true, false};
        CHECK(*result == expected);
    }

    SECTION("E(3,4) - Standard 4/4") {
        auto result = euclidean_rhythm(3, 4, 0);
        REQUIRE(result.has_value());
        // Pattern: x.x.x (but 4 steps, so x.xx)
        int count = 0;
        for (bool b : *result) if (b) count++;
        CHECK(count == 3);
    }
}

TEST_CASE("RHEU001A: euclidean_rhythm rotation", "[rhythm][core]") {
    SECTION("Rotation preserves pulse count") {
        for (int rot = 0; rot < 8; ++rot) {
            auto result = euclidean_rhythm(3, 8, rot);
            REQUIRE(result.has_value());
            int count = 0;
            for (bool b : *result) if (b) count++;
            CHECK(count == 3);
        }
    }

    SECTION("Rotation by steps returns original") {
        auto base = euclidean_rhythm(3, 8, 0);
        auto rotated = euclidean_rhythm(3, 8, 8);
        REQUIRE(base.has_value());
        REQUIRE(rotated.has_value());
        CHECK(*base == *rotated);
    }

    SECTION("Negative rotation") {
        auto pos = euclidean_rhythm(3, 8, 1);
        auto neg = euclidean_rhythm(3, 8, -7);  // -7 mod 8 = 1
        REQUIRE(pos.has_value());
        REQUIRE(neg.has_value());
        CHECK(*pos == *neg);
    }
}

TEST_CASE("RHEU001A: euclidean_rhythm validation", "[rhythm][core]") {
    SECTION("Invalid: steps < 1") {
        auto result = euclidean_rhythm(0, 0, 0);
        CHECK(!result.has_value());
        CHECK(result.error() == ErrorCode::EuclideanInvalidParams);
    }

    SECTION("Invalid: pulses > steps") {
        auto result = euclidean_rhythm(10, 8, 0);
        CHECK(!result.has_value());
        CHECK(result.error() == ErrorCode::EuclideanInvalidParams);
    }

    SECTION("Invalid: negative pulses") {
        auto result = euclidean_rhythm(-1, 8, 0);
        CHECK(!result.has_value());
    }

    SECTION("Invalid: steps too large") {
        auto result = euclidean_rhythm(10, 100, 0);  // > EUCLIDEAN_MAX_STEPS
        CHECK(!result.has_value());
    }
}

TEST_CASE("RHEU001A: euclidean_to_events", "[rhythm][core]") {
    SECTION("Creates correct number of events") {
        auto pattern = euclidean_rhythm(3, 8, 0);
        REQUIRE(pattern.has_value());

        auto events = euclidean_to_events(*pattern, Beat::one(), 60, 100);
        CHECK(events.size() == 3);
    }

    SECTION("Event timing is correct") {
        std::vector<bool> pattern = {true, false, true, false};
        Beat step = Beat{1, 4};  // Quarter note

        auto events = euclidean_to_events(pattern, step, 60, 100);
        REQUIRE(events.size() == 2);

        CHECK(events[0].start_time == Beat::zero());
        CHECK(events[1].start_time == Beat{2, 4});  // Step 2
    }

    SECTION("Event properties") {
        std::vector<bool> pattern = {true};
        auto events = euclidean_to_events(pattern, Beat::one(), 72, 80);

        REQUIRE(events.size() == 1);
        CHECK(events[0].pitch == 72);
        CHECK(events[0].velocity == 80);
        CHECK(events[0].duration == Beat::one());
    }
}

TEST_CASE("RHEU001A: euclidean_preset", "[rhythm][core]") {
    SECTION("Tresillo preset") {
        auto preset = euclidean_preset("tresillo");
        auto manual = euclidean_rhythm(3, 8, 0);
        REQUIRE(preset.has_value());
        REQUIRE(manual.has_value());
        CHECK(*preset == *manual);
    }

    SECTION("Cinquillo preset") {
        auto preset = euclidean_preset("cinquillo");
        REQUIRE(preset.has_value());
        int count = 0;
        for (bool b : *preset) if (b) count++;
        CHECK(count == 5);
        CHECK(preset->size() == 8);
    }

    SECTION("Unknown preset") {
        auto result = euclidean_preset("unknown_rhythm");
        CHECK(!result.has_value());
    }
}
