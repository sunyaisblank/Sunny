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

// =============================================================================
// §9.5 Polyrhythm / Polymetre
// =============================================================================

TEST_CASE("RHEU001A: polyrhythm_onsets 3-against-2 (§9.5)", "[rhythm][core]") {
    auto result = polyrhythm_onsets(3, 2, Beat::one());
    REQUIRE(result.has_value());

    SECTION("m-stream has 3 onsets") {
        REQUIRE(result->stream_m.size() == 3);
        CHECK(result->stream_m[0] == Beat::zero());
        CHECK(result->stream_m[1] == Beat{1, 3});
        CHECK(result->stream_m[2] == Beat{2, 3});
    }

    SECTION("n-stream has 2 onsets") {
        REQUIRE(result->stream_n.size() == 2);
        CHECK(result->stream_n[0] == Beat::zero());
        CHECK(result->stream_n[1] == Beat{1, 2});
    }

    SECTION("Combined is sorted and deduplicated") {
        // Unique onsets: 0, 1/3, 1/2, 2/3
        REQUIRE(result->combined.size() == 4);
        CHECK(result->combined[0] == Beat::zero());
        CHECK(result->combined[1] == Beat{1, 3});
        CHECK(result->combined[2] == Beat{1, 2});
        CHECK(result->combined[3] == Beat{2, 3});
    }

    SECTION("Coincident count equals gcd(3,2) = 1") {
        CHECK(result->coincident_count == 1);
    }
}

TEST_CASE("RHEU001A: polyrhythm_onsets coincident (§9.5)", "[rhythm][core]") {
    SECTION("4 against 2: coincident at 0 and 1/2") {
        auto result = polyrhythm_onsets(4, 2, Beat::one());
        REQUIRE(result.has_value());
        CHECK(result->coincident_count == 2);
        // m: 0, 1/4, 1/2, 3/4  n: 0, 1/2
        // combined: 0, 1/4, 1/2, 3/4 (4 unique)
        CHECK(result->combined.size() == 4);
    }

    SECTION("6 against 4: gcd = 2") {
        auto result = polyrhythm_onsets(6, 4, Beat::one());
        REQUIRE(result.has_value());
        CHECK(result->coincident_count == 2);
        // 6 + 4 - 2 = 8 unique onsets
        CHECK(result->combined.size() == 8);
    }
}

TEST_CASE("RHEU001A: polyrhythm_onsets span scaling (§9.5)", "[rhythm][core]") {
    Beat span = Beat{2, 1};
    auto result = polyrhythm_onsets(3, 2, span);
    REQUIRE(result.has_value());

    // m-stream: 0, 2/3, 4/3
    CHECK(result->stream_m[1] == Beat{2, 3});
    CHECK(result->stream_m[2] == Beat{4, 3});

    // n-stream: 0, 1
    CHECK(result->stream_n[1] == Beat::one());
}

TEST_CASE("RHEU001A: polyrhythm_onsets validation (§9.5)", "[rhythm][core]") {
    CHECK(!polyrhythm_onsets(0, 3, Beat::one()).has_value());
    CHECK(!polyrhythm_onsets(3, 0, Beat::one()).has_value());
    CHECK(!polyrhythm_onsets(-1, 3, Beat::one()).has_value());
    CHECK(!polyrhythm_onsets(3, 2, Beat::zero()).has_value());
}

// =============================================================================
// §9.7 Rhythmic Transformations
// =============================================================================

TEST_CASE("RHEU001A: rhythm_scale augmentation/diminution (§9.7)", "[rhythm][core]") {
    std::vector<Beat> pattern = {Beat{1, 4}, Beat{1, 8}, Beat{1, 4}};

    SECTION("Augmentation (factor 2)") {
        auto result = rhythm_scale(pattern, Beat{2, 1});
        REQUIRE(result.size() == 3);
        CHECK(result[0] == Beat{1, 2});
        CHECK(result[1] == Beat{1, 4});
        CHECK(result[2] == Beat{1, 2});
    }

    SECTION("Diminution (factor 1/2)") {
        auto result = rhythm_scale(pattern, Beat{1, 2});
        REQUIRE(result.size() == 3);
        CHECK(result[0] == Beat{1, 8});
        CHECK(result[1] == Beat{1, 16});
        CHECK(result[2] == Beat{1, 8});
    }

    SECTION("Identity (factor 1)") {
        auto result = rhythm_scale(pattern, Beat::one());
        CHECK(result == pattern);
    }

    SECTION("Empty sequence") {
        std::span<const Beat> empty;
        auto result = rhythm_scale(empty, Beat{2, 1});
        CHECK(result.empty());
    }
}

TEST_CASE("RHEU001A: rhythm_retrograde (§9.7)", "[rhythm][core]") {
    std::vector<Beat> pattern = {Beat{1, 4}, Beat{1, 8}, Beat{1, 2}};

    SECTION("Reverses order") {
        auto result = rhythm_retrograde(pattern);
        REQUIRE(result.size() == 3);
        CHECK(result[0] == Beat{1, 2});
        CHECK(result[1] == Beat{1, 8});
        CHECK(result[2] == Beat{1, 4});
    }

    SECTION("Double retrograde is identity") {
        auto once = rhythm_retrograde(pattern);
        auto twice = rhythm_retrograde(once);
        CHECK(twice == pattern);
    }

    SECTION("Empty sequence") {
        std::span<const Beat> empty;
        CHECK(rhythm_retrograde(empty).empty());
    }
}

TEST_CASE("RHEU001A: rhythm_rotate (§9.7)", "[rhythm][core]") {
    std::vector<Beat> pattern = {Beat{1, 4}, Beat{1, 8}, Beat{1, 2}, Beat{1, 4}};

    SECTION("Rotate left by 1") {
        auto result = rhythm_rotate(pattern, 1);
        REQUIRE(result.size() == 4);
        CHECK(result[0] == Beat{1, 8});
        CHECK(result[1] == Beat{1, 2});
        CHECK(result[2] == Beat{1, 4});
        CHECK(result[3] == Beat{1, 4});
    }

    SECTION("Rotate by length is identity") {
        auto result = rhythm_rotate(pattern, 4);
        CHECK(result == pattern);
    }

    SECTION("Negative rotation (shift right)") {
        auto result = rhythm_rotate(pattern, -1);
        REQUIRE(result.size() == 4);
        CHECK(result[0] == Beat{1, 4});  // last element wraps to front
        CHECK(result[1] == Beat{1, 4});
        CHECK(result[2] == Beat{1, 8});
        CHECK(result[3] == Beat{1, 2});
    }

    SECTION("Empty sequence") {
        CHECK(rhythm_rotate({}, 3).empty());
    }
}

// =============================================================================
// §9.8 Metric Modulation
// =============================================================================

TEST_CASE("RHEU001A: metric_modulation_ratio (§9.8)", "[rhythm][core]") {
    SECTION("Triplet eighth becomes new quarter: ratio = 3/2") {
        // 3:2 tuplet, same beat unit
        auto ratio = metric_modulation_ratio(3, 2, Beat{1, 4}, Beat{1, 4});
        CHECK(ratio == Beat{3, 2});
    }

    SECTION("Dotted quarter becomes new quarter: ratio = 3/2") {
        // No tuplet (1:1), old beat = dotted quarter (3/8), new beat = quarter (1/4)
        auto ratio = metric_modulation_ratio(1, 1, Beat{3, 8}, Beat{1, 4});
        CHECK(ratio == Beat{3, 2});
    }

    SECTION("5:4 modulation") {
        auto ratio = metric_modulation_ratio(5, 4, Beat{1, 4}, Beat{1, 4});
        CHECK(ratio == Beat{5, 4});
    }

    SECTION("Beat unit change: half to quarter") {
        // 1:1 tuplet, old beat = half (1/2), new beat = quarter (1/4)
        auto ratio = metric_modulation_ratio(1, 1, Beat{1, 2}, Beat{1, 4});
        CHECK(ratio == Beat{2, 1});
    }
}

// =============================================================================
// §9.9 Swing / Shuffle
// =============================================================================

TEST_CASE("RHEU001A: apply_swing triplet (§9.9)", "[rhythm][core]") {
    Beat d = Beat{1, 4};  // quarter note beat

    // Two eighth notes on beat 0
    std::vector<NoteEvent> events = {
        {60, Beat::zero(), Beat{1, 8}, 100, false},
        {60, Beat{1, 8}, Beat{1, 8}, 100, false}
    };

    auto result = apply_swing(events, Beat{2, 3}, d);
    REQUIRE(result.size() == 2);

    SECTION("Downbeat duration adjusted to ρ·d") {
        CHECK(result[0].start_time == Beat::zero());
        // ρ·d = (2/3)·(1/4) = 1/6
        CHECK(result[0].duration == Beat{1, 6});
    }

    SECTION("And-beat shifted to ρ·d, duration (1-ρ)·d") {
        // onset = 0 + (2/3)·(1/4) = 1/6
        CHECK(result[1].start_time == Beat{1, 6});
        // duration = (1/3)·(1/4) = 1/12
        CHECK(result[1].duration == Beat{1, 12});
    }
}

TEST_CASE("RHEU001A: apply_swing straight is no-op (§9.9)", "[rhythm][core]") {
    Beat d = Beat{1, 4};
    std::vector<NoteEvent> events = {
        {60, Beat::zero(), Beat{1, 8}, 100, false},
        {60, Beat{1, 8}, Beat{1, 8}, 100, false}
    };

    auto result = apply_swing(events, Beat{1, 2}, d);
    CHECK(result[0].start_time == events[0].start_time);
    CHECK(result[0].duration == events[0].duration);
    CHECK(result[1].start_time == events[1].start_time);
    CHECK(result[1].duration == events[1].duration);
}

TEST_CASE("RHEU001A: apply_swing multiple beats (§9.9)", "[rhythm][core]") {
    Beat d = Beat{1, 4};
    std::vector<NoteEvent> events = {
        {60, Beat::zero(), Beat{1, 8}, 100, false},         // Beat 0, downbeat
        {62, Beat{1, 8}, Beat{1, 8}, 100, false},           // Beat 0, and
        {64, Beat{1, 4}, Beat{1, 8}, 100, false},           // Beat 1, downbeat
        {65, Beat{3, 8}, Beat{1, 8}, 100, false},           // Beat 1, and
    };

    auto result = apply_swing(events, Beat{2, 3}, d);
    REQUIRE(result.size() == 4);

    // Beat 0 "and" shifted to 1/6
    CHECK(result[1].start_time == Beat{1, 6});

    // Beat 1 "and" shifted: 1/4 + (2/3)·(1/4) = 1/4 + 1/6 = 5/12
    CHECK(result[3].start_time == Beat{5, 12});

    // Pitch and velocity preserved
    CHECK(result[1].pitch == 62);
    CHECK(result[3].pitch == 65);
}
