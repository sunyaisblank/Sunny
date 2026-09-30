/**
 * @file transport_test.cpp
 * @brief Transport unit tests
 *
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>
#include <sunny/render/transport.hpp>

using namespace sunny::render;
using namespace sunny::core;
using Catch::Approx;

TEST_CASE("BlockClock is a bounded callback-free signal recurrence", "[clock][render][block]") {
    REQUIRE_FALSE(BlockClock::create(0));
    REQUIRE_FALSE(BlockClock::create(65'536));

    auto created = BlockClock::create(480);
    REQUIRE(created);
    auto clock = *created;
    const auto context = SignalBlockContext::create(1920.0, 1);
    REQUIRE(context);

    REQUIRE_FALSE(clock.process_block(1, 0.0));
    CHECK(clock.position().ticks == 0);

    clock.play();
    const auto negative = clock.process_block(*context, -1);
    REQUIRE_FALSE(negative);
    CHECK(negative.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(clock.position().ticks == 0);

    const auto oversized = clock.process_block(*context, 2);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(clock.position().ticks == 0);

    const auto first = clock.process_block(*context, 1);
    REQUIRE(first);
    CHECK(first->tick_before == 0);
    CHECK(first->tick_after == 0);
    CHECK(first->ticks_advanced() == 0);

    const auto second = clock.process_block(*context, 1);
    REQUIRE(second);
    CHECK(second->tick_before == 0);
    CHECK(second->tick_after == 1);
    CHECK(second->ticks_advanced() == 1);
    CHECK(clock.position().ticks == 1);

    auto reconfigured = *created;
    reconfigured.play();
    REQUIRE(reconfigured.process_block(*context, 1)); // Retains half a tick.
    const auto faster_context = SignalBlockContext::create(960.0, 1);
    REQUIRE(faster_context);
    REQUIRE(reconfigured.process_block(*faster_context, 1)); // Tick 1 plus retained half.
    REQUIRE(reconfigured.process_block(*context, 1));        // Retained halves complete tick 2.
    CHECK(reconfigured.position().ticks == 2);

    clock.pause();
    const auto paused = clock.process_block(*context, 1);
    REQUIRE(paused);
    CHECK(paused->tick_before == 1);
    CHECK(paused->tick_after == 1);

    REQUIRE(clock.set_position(std::numeric_limits<std::int64_t>::max()));
    clock.play();
    const auto overflow = clock.advance(1);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == ErrorCode::ArithmeticOverflow);
    CHECK(clock.position().ticks == std::numeric_limits<std::int64_t>::max());
}

TEST_CASE("BlockClock renders start-of-sample local quarter-note position",
          "[clock][render][block]") {
    STATIC_CHECK(BlockClock::signal_input_count == 0);
    STATIC_CHECK(BlockClock::signal_output_count == 1);

    auto clock = *BlockClock::create(480);
    auto endpoint_clock = clock;
    const auto context = SignalBlockContext::create(1920.0, 4);
    REQUIRE(context);
    clock.play();
    endpoint_clock.play();

    double samples[4]{-1.0, -1.0, -1.0, -1.0};
    double* outputs[1]{samples};
    REQUIRE(clock.process_position_block(*context, 4, 0, nullptr, 1, outputs));
    CHECK(samples[0] == Approx(0.0));
    CHECK(samples[1] == Approx(1.0 / 960.0));
    CHECK(samples[2] == Approx(1.0 / 480.0));
    CHECK(samples[3] == Approx(1.0 / 320.0));

    const auto endpoint = endpoint_clock.process_block(*context, 4);
    REQUIRE(endpoint);
    CHECK(clock.position().ticks == endpoint->tick_after);
    CHECK(clock.position().ticks == 2);

    clock.pause();
    double held[2]{-1.0, -1.0};
    const auto held_context = SignalBlockContext::create(1920.0, 2);
    REQUIRE(held_context);
    REQUIRE(clock.process_position_block(*held_context, held));
    CHECK(held[0] == Approx(1.0 / 240.0));
    CHECK(held[1] == Approx(1.0 / 240.0));
    CHECK(clock.position().ticks == 2);
}

TEST_CASE("BlockClock position vectors reject before output or clock mutation",
          "[clock][render][block]") {
    auto clock = *BlockClock::create(480);
    clock.play();
    const auto context = SignalBlockContext::create(480.0, 1);
    REQUIRE(context);

    double sample = 17.0;
    double* outputs[1]{&sample};
    const auto wrong_inputs = clock.process_position_block(*context, 1, 1, nullptr, 1, outputs);
    REQUIRE_FALSE(wrong_inputs);
    CHECK(wrong_inputs.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(sample == 17.0);
    CHECK(clock.position().ticks == 0);

    const auto missing_output = clock.process_position_block(*context, 1, 0, nullptr, 1, nullptr);
    REQUIRE_FALSE(missing_output);
    CHECK(missing_output.error() == ErrorCode::RenderInvalidSignalBuffer);
    CHECK(clock.position().ticks == 0);

    REQUIRE(clock.set_position(std::numeric_limits<std::int64_t>::max()));
    const auto overflow = clock.process_position_block(*context, 1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == ErrorCode::ArithmeticOverflow);
    CHECK(sample == 17.0);
    CHECK(clock.position().ticks == std::numeric_limits<std::int64_t>::max());

    REQUIRE(clock.process_position_block(*context, 0, 0, nullptr, 1, nullptr));
}

TEST_CASE("Transport state management", "[transport][render]") {
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

TEST_CASE("Tempo control", "[transport][render]") {
    Transport transport;

    SECTION("Default tempo is 120 BPM") {
        REQUIRE(transport.tempo() == 120.0);
    }

    SECTION("Set valid tempo") {
        transport.set_tempo(140.0);
        REQUIRE(transport.tempo() == 140.0);
    }

    SECTION("Reject out-of-range tempo") {
        transport.set_tempo(10.0);           // Too slow
        REQUIRE(transport.tempo() == 120.0); // Unchanged

        transport.set_tempo(1500.0);         // Too fast
        REQUIRE(transport.tempo() == 120.0); // Unchanged
    }
}

TEST_CASE("Position tracking", "[transport][render]") {
    Transport transport;

    SECTION("Advance increases position") {
        transport.play();
        transport.advance(480);

        auto pos = transport.position();
        REQUIRE(pos.ticks == 480);
    }

    SECTION("Advance only when playing") {
        transport.advance(480); // Not playing
        REQUIRE(transport.position().ticks == 0);

        transport.play();
        transport.advance(480);
        REQUIRE(transport.position().ticks == 480);
    }

    SECTION("Position distinguishes Sunny whole-note units from quarter-note host beats") {
        transport.play();
        transport.advance(480); // One quarter note at 480 PPQ.

        auto pos = transport.position();
        auto beats = pos.to_beats();
        REQUIRE(beats == Beat{1, 4});
        REQUIRE(pos.to_quarter_notes() == 1.0);
        REQUIRE(pos.to_seconds() == 0.5);
    }
}

TEST_CASE("Event scheduling", "[transport][render]") {
    Transport transport;

    int note_on_count = 0;
    int note_off_count = 0;

    transport.set_note_on_callback([&](const NoteEvent&) { note_on_count++; });

    transport.set_note_off_callback([&](const NoteEvent&) { note_off_count++; });

    SECTION("Scheduled events dispatch at correct tick") {
        transport.schedule_note(480, 60, Beat{1, 1}, 100);
        transport.play();

        transport.advance(240); // Not yet
        REQUIRE(note_on_count == 0);

        transport.advance(240); // Now at tick 480
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

TEST_CASE("Transport validates construction and timing input", "[transport][render]") {
    REQUIRE_FALSE(Transport::create(0));
    REQUIRE_FALSE(Transport::create(65'536));
    REQUIRE(Transport::create(960));

    Transport transport;
    REQUIRE_FALSE(transport.set_tempo(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(transport.set_position(-1));
    REQUIRE_FALSE(transport.advance(-1));
    REQUIRE_FALSE(transport.process_block(1, 0.0));
    REQUIRE_FALSE(transport.schedule_note(0, 60, Beat{1, 7}, 100));

    NoteEvent invalid_event;
    invalid_event.pitch = 60;
    invalid_event.duration = Beat{1, 4};
    invalid_event.velocity = 255;
    const auto invalid_velocity = transport.schedule(ScheduledEvent{0, invalid_event});
    REQUIRE_FALSE(invalid_velocity);
    CHECK(invalid_velocity.error() == ErrorCode::InvalidVelocity);
}

TEST_CASE("Transport accumulates sub-tick audio blocks", "[transport][render]") {
    Transport transport;
    transport.play();
    REQUIRE(transport.process_block(1, 1920.0));
    REQUIRE(transport.position().ticks == 0);
    REQUIRE(transport.process_block(1, 1920.0));
    REQUIRE(transport.position().ticks == 1);
}

TEST_CASE("Transport host blocks enforce the configured vector maximum before dispatch",
          "[transport][render][block]") {
    Transport transport;
    int callbacks = 0;
    transport.set_note_on_callback([&](const NoteEvent&) { ++callbacks; });
    REQUIRE(transport.schedule_note(0, 60, Beat{1, 4}, 100, 23));
    transport.play();

    const auto context = SignalBlockContext::create(1920.0, 1);
    REQUIRE(context);
    const auto oversized = transport.process_block(*context, 2);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(transport.position().ticks == 0);
    CHECK(callbacks == 0);

    const auto negative = transport.process_block(*context, -1);
    REQUIRE_FALSE(negative);
    CHECK(negative.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(transport.position().ticks == 0);
    CHECK(callbacks == 0);

    REQUIRE(transport.process_block(*context, 1));
    CHECK(transport.position().ticks == 0);
    CHECK(callbacks == 1);
    REQUIRE(transport.process_block(*context, 1));
    CHECK(transport.position().ticks == 1);
}

TEST_CASE("Recording is a running transport state", "[transport][render]") {
    Transport transport;
    transport.record();
    REQUIRE(transport.state() == TransportState::Recording);
    REQUIRE(transport.is_running());
    REQUIRE_FALSE(transport.is_playing());
    REQUIRE(transport.advance(10));
    REQUIRE(transport.position().ticks == 10);
    transport.pause();
    REQUIRE(transport.state() == TransportState::Paused);
}

TEST_CASE("Equal-tick note-offs precede note-ons", "[transport][render]") {
    Transport transport;
    std::vector<std::string> dispatched;
    transport.set_note_on_callback([&](const NoteEvent& event) {
        dispatched.push_back("on:" + std::to_string(static_cast<int>(event.pitch)));
    });
    transport.set_note_off_callback([&](const NoteEvent& event) {
        dispatched.push_back("off:" + std::to_string(static_cast<int>(event.pitch)));
    });

    REQUIRE(transport.schedule_note(0, 60, Beat{1, 4}, 100));
    REQUIRE(transport.schedule_note(480, 64, Beat{1, 4}, 100));
    transport.play();
    REQUIRE(transport.advance(480));
    REQUIRE(dispatched == std::vector<std::string>{"on:60", "off:60", "on:64"});
}

TEST_CASE("Transport callbacks carry canonical whole-note timing", "[transport][render]") {
    Transport transport;
    std::vector<NoteEvent> note_ons;
    std::vector<NoteEvent> note_offs;
    transport.set_note_on_callback([&](const NoteEvent& event) { note_ons.push_back(event); });
    transport.set_note_off_callback([&](const NoteEvent& event) { note_offs.push_back(event); });

    REQUIRE(transport.schedule_note(0, 60, Beat{1, 4}, 100, 23));
    transport.play();
    REQUIRE(transport.advance(480));

    REQUIRE(note_ons.size() == 1);
    CHECK(note_ons[0].start_time == Beat::zero());
    CHECK(note_ons[0].duration == Beat{1, 4});
    CHECK(note_ons[0].release_velocity == 23);
    REQUIRE(note_offs.size() == 1);
    CHECK(note_offs[0].start_time == Beat{1, 4});
    CHECK(note_offs[0].duration == Beat{1, 4});
    CHECK(note_offs[0].release_velocity == 23);
}

TEST_CASE("Transport callbacks observe the committed endpoint", "[transport][render]") {
    Transport transport;
    std::int64_t observed_tick = -1;
    transport.set_note_on_callback(
        [&](const NoteEvent&) { observed_tick = transport.position().ticks; });
    REQUIRE(transport.schedule_note(5, 60, Beat{1, 4}, 100));
    transport.play();

    REQUIRE(transport.advance(10));
    CHECK(observed_tick == 10);
    CHECK(transport.position().ticks == 10);
}

TEST_CASE("Seeking discards stale queued events without callbacks", "[transport][render]") {
    Transport transport;
    int callbacks = 0;
    transport.set_note_on_callback([&](const NoteEvent&) { ++callbacks; });
    REQUIRE(transport.schedule_note(100, 60, Beat{1, 1}, 100));
    REQUIRE(transport.set_position(200));
    transport.play();
    REQUIRE(transport.advance(0));
    REQUIRE(callbacks == 0);
}

TEST_CASE("BlockClock rejects the exclusive signed tick bound before conversion",
          "[clock][render][block]") {
    BlockClock clock;
    clock.play();
    REQUIRE(clock.process_block(1, 1920.0)); // Keep a fractional tick across rejection.
    // With one tick per sample, this exact power of two cannot be an int64 endpoint,
    // including on platforms where long double has the same precision as double.
    const auto rejected = clock.process_block(std::size_t{1} << 63, 960.0);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == ErrorCode::ArithmeticOverflow);
    CHECK(clock.position().ticks == 0);
    REQUIRE(clock.process_block(1, 1920.0));
    CHECK(clock.position().ticks == 1);
}
