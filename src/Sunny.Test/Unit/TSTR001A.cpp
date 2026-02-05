/**
 * @file TSTR001A.cpp
 * @brief Unit tests for RDTR001A (Transport)
 *
 * Component: TSTR001A
 * Tests: RDTR001A
 */

#include <catch2/catch_test_macros.hpp>

#include "Transport/RDTR001A.h"

using namespace Sunny::Render;
using namespace Sunny::Core;

TEST_CASE("RDTR001A: Transport state management", "[transport][render]") {
    Transport transport;

    SECTION("Initial state is stopped") {
        REQUIRE(transport.state() == TransportState::Stopped);
        REQUIRE_FALSE(transport.is_playing());
    }

    SECTION("Play changes state to playing") {
        transport.play();
        REQUIRE(transport.state() == TransportState::Playing);
        REQUIRE(transport.is_playing());
    }

    SECTION("Stop resets to beginning") {
        transport.play();
        transport.advance(1000);
        transport.stop();

        REQUIRE(transport.state() == TransportState::Stopped);
        REQUIRE(transport.position().ticks == 0);
    }

    SECTION("Pause maintains position") {
        transport.play();
        transport.advance(1000);
        transport.pause();

        REQUIRE(transport.state() == TransportState::Paused);
        REQUIRE(transport.position().ticks == 1000);
    }
}

TEST_CASE("RDTR001A: Tempo control", "[transport][render]") {
    Transport transport;

    SECTION("Default tempo is 120 BPM") {
        REQUIRE(transport.tempo() == 120.0);
    }

    SECTION("Set valid tempo") {
        transport.set_tempo(140.0);
        REQUIRE(transport.tempo() == 140.0);
    }

    SECTION("Reject out-of-range tempo") {
        transport.set_tempo(10.0);   // Too slow
        REQUIRE(transport.tempo() == 120.0);  // Unchanged

        transport.set_tempo(1500.0);  // Too fast
        REQUIRE(transport.tempo() == 120.0);  // Unchanged
    }
}

TEST_CASE("RDTR001A: Position tracking", "[transport][render]") {
    Transport transport;

    SECTION("Advance increases position") {
        transport.play();
        transport.advance(480);

        auto pos = transport.position();
        REQUIRE(pos.ticks == 480);
    }

    SECTION("Advance only when playing") {
        transport.advance(480);  // Not playing
        REQUIRE(transport.position().ticks == 0);

        transport.play();
        transport.advance(480);
        REQUIRE(transport.position().ticks == 480);
    }

    SECTION("Position converts to beats") {
        transport.play();
        transport.advance(480);  // 1 beat at 480 PPQ

        auto pos = transport.position();
        auto beats = pos.to_beats();
        REQUIRE(beats.to_float() == 1.0);
    }
}

TEST_CASE("RDTR001A: Event scheduling", "[transport][render]") {
    Transport transport;

    int note_on_count = 0;
    int note_off_count = 0;

    transport.set_note_on_callback([&](const NoteEvent&) {
        note_on_count++;
    });

    transport.set_note_off_callback([&](const NoteEvent&) {
        note_off_count++;
    });

    SECTION("Scheduled events dispatch at correct tick") {
        transport.schedule_note(480, 60, Beat{1, 1}, 100);
        transport.play();

        transport.advance(240);  // Not yet
        REQUIRE(note_on_count == 0);

        transport.advance(240);  // Now at tick 480
        REQUIRE(note_on_count == 1);
    }

    SECTION("Clear removes scheduled events") {
        transport.schedule_note(480, 60, Beat{1, 1}, 100);
        transport.clear_scheduled();
        transport.play();
        transport.advance(960);

        REQUIRE(note_on_count == 0);
    }
}
