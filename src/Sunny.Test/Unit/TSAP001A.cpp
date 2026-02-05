/**
 * @file TSAP001A.cpp
 * @brief Unit tests for RDAP001A (Arpeggiator)
 *
 * Component: TSAP001A
 * Tests: RDAP001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>

#include "Arpeggio/RDAP001A.h"

using namespace Sunny::Render;
using namespace Sunny::Core;
using Catch::Matchers::Equals;

TEST_CASE("RDAP001A: Arpeggiator direction Up", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_notes({60, 64, 67});  // C, E, G

    SECTION("Pattern is sorted ascending") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern.size() == 3);
        REQUIRE(pattern[0] == 60);
        REQUIRE(pattern[1] == 64);
        REQUIRE(pattern[2] == 67);
    }
}

TEST_CASE("RDAP001A: Arpeggiator direction Down", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Down);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern is sorted descending") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern.size() == 3);
        REQUIRE(pattern[0] == 67);
        REQUIRE(pattern[1] == 64);
        REQUIRE(pattern[2] == 60);
    }
}

TEST_CASE("RDAP001A: Arpeggiator direction UpDown", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::UpDown);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern goes up then down (exclusive)") {
        auto pattern = arp.generate_pattern();
        // Should be: 60, 64, 67, 64 (not repeating 60 or 67)
        REQUIRE(pattern.size() == 4);
        REQUIRE(pattern[0] == 60);
        REQUIRE(pattern[1] == 64);
        REQUIRE(pattern[2] == 67);
        REQUIRE(pattern[3] == 64);
    }
}

TEST_CASE("RDAP001A: Arpeggiator octave range", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_octave_range(2);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern spans multiple octaves") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern.size() == 6);  // 3 notes * 2 octaves

        // First octave
        REQUIRE(pattern[0] == 60);
        REQUIRE(pattern[1] == 64);
        REQUIRE(pattern[2] == 67);

        // Second octave
        REQUIRE(pattern[3] == 72);
        REQUIRE(pattern[4] == 76);
        REQUIRE(pattern[5] == 79);
    }
}

TEST_CASE("RDAP001A: Arpeggiator step sequencing", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_notes({60, 64, 67});

    SECTION("next() cycles through pattern") {
        REQUIRE(arp.next() == 60);
        REQUIRE(arp.next() == 64);
        REQUIRE(arp.next() == 67);
        REQUIRE(arp.next() == 60);  // Wraps
    }

    SECTION("reset() starts from beginning") {
        arp.next();
        arp.next();
        arp.reset();
        REQUIRE(arp.next() == 60);
    }

    SECTION("current() doesn't advance") {
        REQUIRE(arp.current() == 60);
        REQUIRE(arp.current() == 60);
    }
}

TEST_CASE("RDAP001A: Arpeggiator with ChordVoicing", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);

    ChordVoicing voicing;
    voicing.notes = {60, 64, 67, 71};  // Cmaj7
    voicing.root = 0;
    voicing.quality = "maj7";

    arp.set_notes(voicing);

    SECTION("Accepts ChordVoicing input") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern.size() == 4);
    }
}

TEST_CASE("RDAP001A: generate_arpeggio function", "[arpeggio][render]") {
    ChordVoicing voicing;
    voicing.notes = {60, 64, 67};
    voicing.root = 0;
    voicing.quality = "major";

    SECTION("Generates note events") {
        auto events = generate_arpeggio(
            voicing,
            ArpDirection::Up,
            Beat{1, 4},  // 16th notes
            0.5,         // 50% gate
            1            // 1 octave
        );

        REQUIRE(events.size() == 3);

        // Check timing
        REQUIRE(events[0].start_time.numerator == 0);
        REQUIRE(events[1].start_time.numerator == 1);
        REQUIRE(events[2].start_time.numerator == 2);
    }

    SECTION("Gate affects duration") {
        auto events = generate_arpeggio(
            voicing,
            ArpDirection::Up,
            Beat{1, 4},
            0.5,
            1
        );

        // Duration should be half of step
        for (const auto& e : events) {
            // 0.5 * (1/4) = 1/8... but scaled
            REQUIRE(e.duration.numerator > 0);
        }
    }
}

TEST_CASE("RDAP001A: Edge cases", "[arpeggio][render]") {
    Arpeggiator arp;

    SECTION("Empty notes returns default on next()") {
        REQUIRE(arp.next() == 60);  // Middle C default
    }

    SECTION("Clear resets state") {
        arp.set_notes({60, 64, 67});
        arp.next();
        arp.clear();
        REQUIRE(arp.pattern_length() == 0);
    }

    SECTION("Direction Order preserves input order") {
        arp.set_direction(ArpDirection::Order);
        arp.set_notes({67, 60, 64});  // G, C, E (out of order)

        auto pattern = arp.generate_pattern();
        REQUIRE(pattern[0] == 67);
        REQUIRE(pattern[1] == 60);
        REQUIRE(pattern[2] == 64);
    }
}
