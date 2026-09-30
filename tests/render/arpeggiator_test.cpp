/**
 * @file arpeggiator_test.cpp
 * @brief Arpeggiator unit tests
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_vector.hpp>
#include <limits>
#include <sunny/render/arpeggiator.hpp>

using namespace sunny::render;
using namespace sunny::core;
using Catch::Matchers::Equals;

TEST_CASE("Arpeggiator direction Up", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_notes({60, 64, 67}); // C, E, G

    SECTION("Pattern is sorted ascending") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern->size() == 3);
        REQUIRE((*pattern)[0] == 60);
        REQUIRE((*pattern)[1] == 64);
        REQUIRE((*pattern)[2] == 67);
    }
}

TEST_CASE("Arpeggiator direction Down", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Down);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern is sorted descending") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern->size() == 3);
        REQUIRE((*pattern)[0] == 67);
        REQUIRE((*pattern)[1] == 64);
        REQUIRE((*pattern)[2] == 60);
    }
}

TEST_CASE("Arpeggiator direction UpDown", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::UpDown);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern goes up then down (exclusive)") {
        auto pattern = arp.generate_pattern();
        // Should be: 60, 64, 67, 64 (not repeating 60 or 67)
        REQUIRE(pattern->size() == 4);
        REQUIRE((*pattern)[0] == 60);
        REQUIRE((*pattern)[1] == 64);
        REQUIRE((*pattern)[2] == 67);
        REQUIRE((*pattern)[3] == 64);
    }
}

TEST_CASE("Arpeggiator octave range", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_octave_range(2);
    arp.set_notes({60, 64, 67});

    SECTION("Pattern spans multiple octaves") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern->size() == 6); // 3 notes * 2 octaves

        // First octave
        REQUIRE((*pattern)[0] == 60);
        REQUIRE((*pattern)[1] == 64);
        REQUIRE((*pattern)[2] == 67);

        // Second octave
        REQUIRE((*pattern)[3] == 72);
        REQUIRE((*pattern)[4] == 76);
        REQUIRE((*pattern)[5] == 79);
    }
}

TEST_CASE("Arpeggiator step sequencing", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);
    arp.set_notes({60, 64, 67});

    SECTION("next() cycles through pattern") {
        REQUIRE(*arp.next() == 60);
        REQUIRE(*arp.next() == 64);
        REQUIRE(*arp.next() == 67);
        REQUIRE(*arp.next() == 60); // Wraps
    }

    SECTION("reset() starts from beginning") {
        (void)arp.next();
        (void)arp.next();
        arp.reset();
        REQUIRE(*arp.next() == 60);
    }

    SECTION("current() doesn't advance") {
        REQUIRE(*arp.current() == 60);
        REQUIRE(*arp.current() == 60);
    }
}

TEST_CASE("Arpeggiator with ChordVoicing", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_direction(ArpDirection::Up);

    ChordVoicing voicing;
    voicing.notes = {60, 64, 67, 71}; // Cmaj7
    voicing.root = 0;
    voicing.quality = "maj7";

    arp.set_notes(voicing);

    SECTION("Accepts ChordVoicing input") {
        auto pattern = arp.generate_pattern();
        REQUIRE(pattern->size() == 4);
    }
}

TEST_CASE("generate_arpeggio function", "[arpeggio][render]") {
    ChordVoicing voicing;
    voicing.notes = {60, 64, 67};
    voicing.root = 0;
    voicing.quality = "major";

    SECTION("Generates note events") {
        auto events = generate_arpeggio(voicing,
                                        ArpDirection::Up,
                                        Beat{1, 4}, // 16th notes
                                        0.5,        // 50% gate
                                        1           // 1 octave
        );

        REQUIRE(events->size() == 3);

        // Check timing
        REQUIRE((*events)[0].start_time == Beat{0, 1});
        REQUIRE((*events)[1].start_time == Beat{1, 4});
        REQUIRE((*events)[2].start_time == Beat{1, 2});
    }

    SECTION("Gate affects duration") {
        auto events = generate_arpeggio(voicing, ArpDirection::Up, Beat{1, 4}, 0.5, 1);

        // Duration should be half of step
        for (const auto& e : *events) {
            // 0.5 * (1/4) = 1/8... but scaled
            REQUIRE(e.duration.numerator() > 0);
        }
    }
}

TEST_CASE("Arpeggiator edge cases", "[arpeggio][render]") {
    Arpeggiator arp;

    SECTION("Empty notes are declined instead of fabricating a note") {
        auto note = arp.next();
        REQUIRE_FALSE(note);
        REQUIRE(note.error() == ErrorCode::RenderEmptyPattern);
    }

    SECTION("Clear resets state") {
        arp.set_notes({60, 64, 67});
        (void)arp.next();
        arp.clear();
        auto length = arp.pattern_length();
        REQUIRE_FALSE(length);
        REQUIRE(length.error() == ErrorCode::RenderEmptyPattern);
    }

    SECTION("Direction Order preserves input order") {
        arp.set_direction(ArpDirection::Order);
        arp.set_notes({67, 60, 64}); // G, C, E (out of order)

        auto pattern = arp.generate_pattern();
        REQUIRE((*pattern)[0] == 67);
        REQUIRE((*pattern)[1] == 60);
        REQUIRE((*pattern)[2] == 64);
    }
}

TEST_CASE("Arpeggiator configuration invalidates cached patterns", "[arpeggio][render]") {
    Arpeggiator arp;
    arp.set_notes({60, 64, 67});
    REQUIRE((*arp.generate_pattern())[0] == 60);

    REQUIRE(arp.set_direction(ArpDirection::Down));
    auto down = arp.generate_pattern();
    REQUIRE((*down)[0] == 67);

    REQUIRE(arp.set_octave_range(2));
    auto expanded = arp.generate_pattern();
    REQUIRE(expanded->size() == 6);
}

TEST_CASE("Arpeggiator rejects invalid configuration and MIDI overflow", "[arpeggio][render]") {
    Arpeggiator arp;
    REQUIRE_FALSE(arp.set_gate(0.0));
    REQUIRE_FALSE(arp.set_gate(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(arp.set_octave_range(0));
    REQUIRE(arp.gate() == 0.5);
    REQUIRE(arp.octave_range() == 1);

    arp.set_notes({120});
    REQUIRE(arp.set_octave_range(2));
    auto pattern = arp.generate_pattern();
    REQUIRE_FALSE(pattern);
    REQUIRE(pattern.error() == ErrorCode::InvalidMidiNote);
}

TEST_CASE("Random arpeggiator patterns are seed reproducible", "[arpeggio][render]") {
    Arpeggiator first;
    Arpeggiator second;
    const std::vector<MidiNote> notes{48, 50, 52, 53, 55, 57, 59, 60};
    first.set_notes(notes);
    second.set_notes(notes);
    REQUIRE(first.set_direction(ArpDirection::Random));
    REQUIRE(second.set_direction(ArpDirection::Random));
    first.set_seed(1234);
    second.set_seed(1234);
    auto pattern = first.generate_pattern();
    REQUIRE(*pattern == *second.generate_pattern());
    REQUIRE(*pattern == std::vector<MidiNote>{55, 59, 60, 50, 52, 48, 57, 53});
}

TEST_CASE("Arpeggio event timing is exact or declined", "[arpeggio][render]") {
    ChordVoicing voicing;
    voicing.notes = {60, 64};

    auto events = generate_arpeggio(voicing, ArpDirection::Up, Beat{1, 4}, 0.5, 1);
    REQUIRE(events);
    REQUIRE((*events)[0].duration == Beat{1, 8});
    REQUIRE((*events)[1].start_time == Beat{1, 4});

    REQUIRE_FALSE(generate_arpeggio(voicing, ArpDirection::Up, Beat{0, 1}, 0.5, 1));
    REQUIRE_FALSE(generate_arpeggio({}, ArpDirection::Up, Beat{1, 4}, 0.5, 1));
}
