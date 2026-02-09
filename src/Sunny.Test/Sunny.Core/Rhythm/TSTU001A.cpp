/**
 * @file TSTU001A.cpp
 * @brief Unit tests for RHTU001A (Tuplets)
 *
 * Component: TSTU001A
 * Tests: RHTU001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Rhythm/RHTU001A.h"

using namespace Sunny::Core;

TEST_CASE("RHTU001A: make_tuplet validation", "[rhythm][core]") {
    SECTION("Valid triplet") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        REQUIRE(t->actual == 3);
        REQUIRE(t->normal == 2);
    }

    SECTION("Zero actual fails") {
        auto t = make_tuplet(0, 2, Beat{1, 8});
        REQUIRE_FALSE(t.has_value());
        REQUIRE(t.error() == ErrorCode::TupletInvalidRatio);
    }

    SECTION("Negative normal fails") {
        auto t = make_tuplet(3, -1, Beat{1, 8});
        REQUIRE_FALSE(t.has_value());
    }

    SECTION("Zero base duration fails") {
        auto t = make_tuplet(3, 2, Beat{0, 1});
        REQUIRE_FALSE(t.has_value());
    }
}

TEST_CASE("RHTU001A: tuplet_note_duration", "[rhythm][core]") {
    SECTION("Triplet eighth = 1/12 (2/3 of 1/8)") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        Beat dur = tuplet_note_duration(*t);
        // (2/3) * (1/8) = 2/24 = 1/12
        REQUIRE(dur.reduce() == Beat{1, 12});
    }

    SECTION("Quintuplet = 1/5 of the span") {
        auto t = make_tuplet(5, 4, Beat{1, 16});
        REQUIRE(t.has_value());
        Beat dur = tuplet_note_duration(*t);
        // (4/5) * (1/16) = 4/80 = 1/20
        REQUIRE(dur.reduce() == Beat{1, 20});
    }

    SECTION("Duplet in compound time = 3/2 of base") {
        auto t = make_tuplet(2, 3, Beat{1, 8});
        REQUIRE(t.has_value());
        Beat dur = tuplet_note_duration(*t);
        // (3/2) * (1/8) = 3/16
        REQUIRE(dur.reduce() == Beat{3, 16});
    }
}

TEST_CASE("RHTU001A: tuplet_total_span", "[rhythm][core]") {
    SECTION("Triplet eighth spans 2 eighths") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        Beat span = tuplet_total_span(*t);
        // 2 * (1/8) = 2/8 = 1/4
        REQUIRE(span.reduce() == Beat{1, 4});
    }

    SECTION("Sum of note durations equals total span") {
        auto t = make_tuplet(5, 4, Beat{1, 16});
        REQUIRE(t.has_value());
        Beat note = tuplet_note_duration(*t);
        Beat sum = note * t->actual;
        Beat span = tuplet_total_span(*t);
        REQUIRE(sum.reduce() == span.reduce());
    }
}

TEST_CASE("RHTU001A: tuplet_to_events", "[rhythm][core]") {
    SECTION("Triplet generates 3 events") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        auto events = tuplet_to_events(*t, Beat::zero(), 60, 100);
        REQUIRE(events.size() == 3);
    }

    SECTION("Events are evenly spaced") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        auto events = tuplet_to_events(*t, Beat::zero(), 60, 100);
        Beat dur = tuplet_note_duration(*t);
        for (std::size_t i = 0; i < events.size(); ++i) {
            Beat expected_start = dur * static_cast<std::int64_t>(i);
            REQUIRE(events[i].start_time == expected_start);
            REQUIRE(events[i].duration == dur);
            REQUIRE(events[i].pitch == 60);
            REQUIRE(events[i].velocity == 100);
        }
    }

    SECTION("Events start at specified offset") {
        auto t = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(t.has_value());
        Beat offset = Beat{1, 4};
        auto events = tuplet_to_events(*t, offset, 72, 80);
        REQUIRE(events[0].start_time == offset);
    }
}

TEST_CASE("RHTU001A: nested_tuplet_duration", "[rhythm][core]") {
    SECTION("Triplet of quintuplets") {
        // Outer: 5-in-4 sixteenth, inner: 3-in-2
        // Outer note = (4/5) * (1/16) = 4/80 = 1/20
        // Inner note = (2/3) * (1/20) = 2/60 = 1/30
        auto outer = make_tuplet(5, 4, Beat{1, 16});
        REQUIRE(outer.has_value());
        Beat inner_dur = nested_tuplet_duration(*outer, 3, 2);
        // (4 * 2) / (5 * 3) * (1/16) = 8/(15*16) = 8/240 = 1/30
        REQUIRE(inner_dur.reduce() == Beat{1, 30});
    }

    SECTION("Nested triplet-of-triplets") {
        // Outer: 3-in-2 eighth, inner: 3-in-2
        // Outer note = (2/3) * (1/8) = 1/12
        // Inner note = (2/3) * (1/12) = 1/18
        auto outer = make_tuplet(3, 2, Beat{1, 8});
        REQUIRE(outer.has_value());
        Beat inner_dur = nested_tuplet_duration(*outer, 3, 2);
        // (2*2)/(3*3) * (1/8) = 4/72 = 1/18
        REQUIRE(inner_dur.reduce() == Beat{1, 18});
    }
}
