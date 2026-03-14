/**
 * @file TSTS001A.cpp
 * @brief Unit tests for RHTS001A (Time signature and metre)
 *
 * Component: TSTS001A
 * Tests: RHTS001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Rhythm/RHTS001A.h"

using namespace Sunny::Core;

TEST_CASE("RHTS001A: make_time_signature simple metres", "[rhythm][core]") {
    SECTION("4/4 creates 4 groups of 1") {
        auto ts = make_time_signature(4, 4);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 4);
        REQUIRE(ts->denominator == 4);
        REQUIRE(ts->groups.size() == 4);
        for (int g : ts->groups) REQUIRE(g == 1);
    }

    SECTION("3/4 creates 3 groups of 1") {
        auto ts = make_time_signature(3, 4);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 3);
        REQUIRE(ts->groups.size() == 3);
    }

    SECTION("2/4 creates 2 groups of 1") {
        auto ts = make_time_signature(2, 4);
        REQUIRE(ts.has_value());
        REQUIRE(ts->groups.size() == 2);
    }
}

TEST_CASE("RHTS001A: make_time_signature compound metres", "[rhythm][core]") {
    SECTION("6/8 creates 2 groups of 3") {
        auto ts = make_time_signature(6, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 6);
        REQUIRE(ts->denominator == 8);
        REQUIRE(ts->groups.size() == 2);
        REQUIRE(ts->groups[0] == 3);
        REQUIRE(ts->groups[1] == 3);
    }

    SECTION("9/8 creates 3 groups of 3") {
        auto ts = make_time_signature(9, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->groups.size() == 3);
        for (int g : ts->groups) REQUIRE(g == 3);
    }

    SECTION("12/8 creates 4 groups of 3") {
        auto ts = make_time_signature(12, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->groups.size() == 4);
    }
}

TEST_CASE("RHTS001A: make_time_signature validation", "[rhythm][core]") {
    SECTION("Zero numerator fails") {
        auto ts = make_time_signature(0, 4);
        REQUIRE_FALSE(ts.has_value());
        REQUIRE(ts.error() == ErrorCode::InvalidTimeSignature);
    }

    SECTION("Non-power-of-two denominator fails") {
        auto ts = make_time_signature(4, 3);
        REQUIRE_FALSE(ts.has_value());
    }

    SECTION("Negative values fail") {
        REQUIRE_FALSE(make_time_signature(-1, 4).has_value());
        REQUIRE_FALSE(make_time_signature(4, -1).has_value());
    }
}

TEST_CASE("RHTS001A: make_additive_time_signature", "[rhythm][core]") {
    SECTION("7/8 as 3+2+2") {
        auto ts = make_additive_time_signature({3, 2, 2}, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 7);
        REQUIRE(ts->denominator == 8);
        REQUIRE(ts->groups.size() == 3);
    }

    SECTION("5/8 as 3+2") {
        auto ts = make_additive_time_signature({3, 2}, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 5);
    }

    SECTION("8/8 as 3+3+2") {
        auto ts = make_additive_time_signature({3, 3, 2}, 8);
        REQUIRE(ts.has_value());
        REQUIRE(ts->numerator() == 8);
    }

    SECTION("Empty groups fails") {
        auto ts = make_additive_time_signature({}, 8);
        REQUIRE_FALSE(ts.has_value());
    }

    SECTION("Zero group size fails") {
        auto ts = make_additive_time_signature({3, 0, 2}, 8);
        REQUIRE_FALSE(ts.has_value());
    }
}

TEST_CASE("RHTS001A: measure_duration and beat_duration", "[rhythm][core]") {
    SECTION("4/4 measure = 1 whole note") {
        auto ts = make_time_signature(4, 4);
        REQUIRE(ts.has_value());
        auto dur = ts->measure_duration();
        // 4/4 = 1 whole note
        REQUIRE(dur == Beat{4, 4});
        REQUIRE(dur == Beat{1, 1});
    }

    SECTION("6/8 measure = 3/4") {
        auto ts = make_time_signature(6, 8);
        REQUIRE(ts.has_value());
        auto dur = ts->measure_duration();
        REQUIRE(dur == Beat{6, 8});
    }

    SECTION("Beat duration is 1/denominator") {
        auto ts = make_time_signature(3, 4);
        REQUIRE(ts.has_value());
        REQUIRE(ts->beat_duration() == Beat{1, 4});
    }
}

TEST_CASE("RHTS001A: classify_metre", "[rhythm][core]") {
    SECTION("4/4 is Simple") {
        auto ts = make_time_signature(4, 4);
        REQUIRE(classify_metre(*ts) == MetreType::Simple);
    }

    SECTION("3/4 is Simple") {
        auto ts = make_time_signature(3, 4);
        REQUIRE(classify_metre(*ts) == MetreType::Simple);
    }

    SECTION("6/8 is Compound") {
        auto ts = make_time_signature(6, 8);
        REQUIRE(classify_metre(*ts) == MetreType::Compound);
    }

    SECTION("9/8 is Compound") {
        auto ts = make_time_signature(9, 8);
        REQUIRE(classify_metre(*ts) == MetreType::Compound);
    }

    SECTION("7/8 (3+2+2) is Asymmetric") {
        auto ts = make_additive_time_signature({3, 2, 2}, 8);
        REQUIRE(classify_metre(*ts) == MetreType::Asymmetric);
    }

    SECTION("5/8 (3+2) is Asymmetric") {
        auto ts = make_additive_time_signature({3, 2}, 8);
        REQUIRE(classify_metre(*ts) == MetreType::Asymmetric);
    }

    SECTION("Groups of 5 are Complex") {
        auto ts = make_additive_time_signature({5, 5}, 8);
        REQUIRE(classify_metre(*ts) == MetreType::Complex);
    }
}

TEST_CASE("RHTS001A: metrical_weight", "[rhythm][core]") {
    SECTION("4/4 downbeat is strongest") {
        auto ts = make_time_signature(4, 4);
        REQUIRE(metrical_weight(*ts, 0) == 4);
    }

    SECTION("4/4 off-beats are weakest") {
        auto ts = make_time_signature(4, 4);
        int w0 = metrical_weight(*ts, 0);
        int w1 = metrical_weight(*ts, 1);
        int w2 = metrical_weight(*ts, 2);
        int w3 = metrical_weight(*ts, 3);
        REQUIRE(w0 > w1);
        REQUIRE(w0 > w2);
        REQUIRE(w0 > w3);
    }

    SECTION("6/8 group boundaries are strong") {
        auto ts = make_time_signature(6, 8);
        int w0 = metrical_weight(*ts, 0);  // Downbeat
        int w3 = metrical_weight(*ts, 3);  // Second group start
        int w1 = metrical_weight(*ts, 1);  // Weak subdivision
        REQUIRE(w0 > w3);
        REQUIRE(w3 > w1);
    }
}

TEST_CASE("RHTS001A: is_syncopated", "[rhythm][core]") {
    SECTION("Note on downbeat is not syncopated") {
        auto ts = make_time_signature(4, 4);
        REQUIRE_FALSE(is_syncopated(*ts, 0, 1));
    }

    SECTION("Note on weak beat sustaining through strong beat is syncopated") {
        auto ts = make_time_signature(4, 4);
        // Start on beat 3 (weak), sustain through beat 0 (downbeat)
        REQUIRE(is_syncopated(*ts, 3, 2));
    }

    SECTION("Short note on weak beat is not syncopated") {
        auto ts = make_time_signature(4, 4);
        REQUIRE_FALSE(is_syncopated(*ts, 1, 1));
    }

    SECTION("Zero duration is not syncopated") {
        auto ts = make_time_signature(4, 4);
        REQUIRE_FALSE(is_syncopated(*ts, 1, 0));
    }
}

TEST_CASE("RHTS001A: is_syncopated with equal weight positions", "[rhythm][core]") {
    // In 4/4, groups are {1,1,1,1}. Position 1 has metrical_weight 3
    // (group boundary) and position 2 also has weight 3 (group boundary).
    // A note starting at position 1, duration 2, sustains through position 2.
    // sustained_weight (3) > onset_weight (3) is false, so not syncopated.
    // A mutation of > to >= would incorrectly return true.
    auto ts = make_time_signature(4, 4);
    REQUIRE(ts.has_value());

    int w1 = metrical_weight(*ts, 1);
    int w2 = metrical_weight(*ts, 2);
    REQUIRE(w1 == w2);  // both group boundaries, weight 3

    CHECK_FALSE(is_syncopated(*ts, 1, 2));
}

TEST_CASE("RHTS001A: metrical_weight mid-group subdivision", "[rhythm][core]") {
    // In 6/8, groups are {3,3}, numerator is 6.
    // Position 0: downbeat → weight 4
    // Position 1: mid-group subdivision of first group (offset=1 in group of 3,
    //   g/2 = 1, so offset == g/2) → weight 1
    // Position 2: offset 2 in first group, not g/2 → weight 0
    // Position 3: group boundary → weight 3
    auto ts = make_time_signature(6, 8);
    REQUIRE(ts.has_value());

    CHECK(metrical_weight(*ts, 0) == 4);  // downbeat
    CHECK(metrical_weight(*ts, 1) == 1);  // mid-group subdivision
    CHECK(metrical_weight(*ts, 2) == 0);  // weak subdivision
    CHECK(metrical_weight(*ts, 3) == 3);  // group boundary

    // In 4/4, groups are {1,1,1,1}. Each group has size 1, so no mid-group
    // subdivision is possible. Position 2 is a group boundary (weight 3).
    // Position 1 is also a group boundary (weight 3).
    auto ts44 = make_time_signature(4, 4);
    REQUIRE(ts44.has_value());

    CHECK(metrical_weight(*ts44, 1) == 3);  // group boundary, not weight 1
    CHECK(metrical_weight(*ts44, 2) == 3);  // group boundary, not weight 1
}
