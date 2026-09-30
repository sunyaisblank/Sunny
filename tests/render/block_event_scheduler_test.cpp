/**
 * @file block_event_scheduler_test.cpp
 * @brief Bounded sample-offset event scheduling tests
 */

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/render/block_event_scheduler.hpp>

using sunny::core::Beat;
using sunny::core::ErrorCode;
using sunny::core::MidiNote;
using sunny::core::NoteEvent;
using sunny::core::Velocity;
using sunny::render::BLOCK_EVENT_QUEUE_CAPACITY;
using sunny::render::BlockEventScheduler;
using sunny::render::SampleOffsetEvent;
using sunny::render::ScheduledEvent;
using sunny::render::SignalBlockContext;

namespace {

ScheduledEvent event_at(std::int64_t tick, MidiNote pitch, Velocity velocity) {
    NoteEvent event;
    event.pitch = pitch;
    event.start_time = Beat{99, 1}; // Scheduling replaces this with the canonical tick coordinate.
    event.duration = Beat{1, 4};
    event.velocity = velocity;
    return ScheduledEvent{tick, event};
}

} // namespace

TEST_CASE("Block event scheduler assigns half-open containing-sample offsets",
          "[render][clock][event][block]") {
    BlockEventScheduler scheduler;
    REQUIRE(scheduler.set_tempo(120.0));
    REQUIRE(scheduler.schedule(event_at(0, 60, 100)));
    REQUIRE(scheduler.schedule(event_at(3, 61, 100)));
    REQUIRE(scheduler.schedule(event_at(4, 62, 100)));
    REQUIRE(scheduler.schedule(event_at(4, 63, 0)));
    REQUIRE(scheduler.schedule(event_at(7, 64, 100)));
    REQUIRE(scheduler.schedule(event_at(8, 65, 100)));
    scheduler.play();

    const auto context = SignalBlockContext::create(480.0, 4);
    REQUIRE(context);
    std::array<SampleOffsetEvent, 6> output{};
    const auto first = scheduler.process_block(*context, 4, output);
    REQUIRE(first);
    CHECK(first->clock.tick_before == 0);
    CHECK(first->clock.tick_after == 8);
    CHECK(first->events_written == 5);
    CHECK(scheduler.scheduled_count() == 1);

    CHECK(output[0].sample_offset == 0);
    CHECK(output[0].scheduled.tick == 0);
    CHECK(output[0].scheduled.event.start_time == Beat::zero());
    CHECK(output[1].sample_offset == 1);
    CHECK(output[1].scheduled.tick == 3);
    CHECK(output[2].sample_offset == 2);
    CHECK(output[2].scheduled.event.velocity == 0); // Note-off precedes the equal-tick note-on.
    CHECK(output[3].sample_offset == 2);
    CHECK(output[3].scheduled.event.pitch == 62);
    CHECK(output[4].sample_offset == 3);
    CHECK(output[4].scheduled.tick == 7);

    std::array<SampleOffsetEvent, 1> boundary{};
    const auto second = scheduler.process_block(*context, 1, boundary);
    REQUIRE(second);
    CHECK(second->events_written == 1);
    CHECK(boundary[0].sample_offset == 0);
    CHECK(boundary[0].scheduled.tick == 8);
    CHECK(scheduler.scheduled_count() == 0);
}

TEST_CASE("Block event output saturation rejects before queue clock or output mutation",
          "[render][clock][event][block]") {
    BlockEventScheduler scheduler;
    REQUIRE(scheduler.schedule(event_at(0, 60, 100)));
    REQUIRE(scheduler.schedule(event_at(1, 61, 100)));
    scheduler.play();

    const auto context = SignalBlockContext::create(480.0, 1);
    REQUIRE(context);
    std::array<SampleOffsetEvent, 1> too_small{};
    too_small[0] = SampleOffsetEvent{99, event_at(99, 62, 100)};
    const auto rejected = scheduler.process_block(*context, 1, too_small);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == ErrorCode::RenderEventBufferFull);
    CHECK(too_small[0].sample_offset == 99);
    CHECK(too_small[0].scheduled.tick == 99);
    CHECK(scheduler.position().ticks == 0);
    CHECK(scheduler.scheduled_count() == 2);

    std::array<SampleOffsetEvent, 2> complete{};
    const auto admitted = scheduler.process_block(*context, 1, complete);
    REQUIRE(admitted);
    CHECK(admitted->events_written == 2);
    CHECK(scheduler.position().ticks == 2);
    CHECK(scheduler.scheduled_count() == 0);
}

TEST_CASE("Block event queue capacity and note pairs are fail-closed",
          "[render][clock][event][capacity]") {
    BlockEventScheduler scheduler;
    for (std::size_t index = 0; index < BLOCK_EVENT_QUEUE_CAPACITY - 1; ++index)
        REQUIRE(scheduler.schedule(event_at(1000, 60, 100)));
    CHECK(scheduler.scheduled_count() == BLOCK_EVENT_QUEUE_CAPACITY - 1);

    const auto pair = scheduler.schedule_note(1000, 61, Beat{1, 4}, 100);
    REQUIRE_FALSE(pair);
    CHECK(pair.error() == ErrorCode::RenderEventQueueFull);
    CHECK(scheduler.scheduled_count() == BLOCK_EVENT_QUEUE_CAPACITY - 1);

    REQUIRE(scheduler.schedule(event_at(1000, 62, 100)));
    const auto invalid = scheduler.schedule(event_at(1000, 63, 255));
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == ErrorCode::InvalidVelocity);
    const auto saturated = scheduler.schedule(event_at(1000, 63, 100));
    REQUIRE_FALSE(saturated);
    CHECK(saturated.error() == ErrorCode::RenderEventQueueFull);
    CHECK(scheduler.scheduled_count() == BLOCK_EVENT_QUEUE_CAPACITY);

    scheduler.clear_scheduled();
    CHECK(scheduler.scheduled_count() == 0);
}

TEST_CASE("Block event scheduler validates before state and retains events while inactive",
          "[render][clock][event][block]") {
    REQUIRE_FALSE(BlockEventScheduler::create(0));
    REQUIRE_FALSE(sunny::render::normalise_scheduled_event(event_at(0, 60, 100), 0, 0));
    REQUIRE_FALSE(sunny::render::make_scheduled_note_pair(0, 0, 0, 60, Beat{1, 4}, 100));
    BlockEventScheduler scheduler;
    REQUIRE_FALSE(scheduler.set_position(-1));
    REQUIRE_FALSE(scheduler.schedule_note(0, 60, Beat{1, 7}, 100));
    REQUIRE(scheduler.schedule(event_at(0, 60, 100)));

    const auto context = SignalBlockContext::create(480.0, 4);
    REQUIRE(context);
    std::array<SampleOffsetEvent, 1> output{};
    const auto stopped = scheduler.process_block(*context, 4, output);
    REQUIRE(stopped);
    CHECK(stopped->events_written == 0);
    CHECK(scheduler.scheduled_count() == 1);

    scheduler.play();
    const auto empty = scheduler.process_block(*context, 0, output);
    REQUIRE(empty);
    CHECK(empty->events_written == 0);
    CHECK(scheduler.scheduled_count() == 1);

    const auto oversized = scheduler.process_block(*context, 5, output);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(scheduler.scheduled_count() == 1);

    const auto admitted = scheduler.process_block(*context, 1, output);
    REQUIRE(admitted);
    CHECK(admitted->events_written == 1);
    CHECK(scheduler.scheduled_count() == 0);
}

TEST_CASE("Block event arithmetic overflow is wholly state preserving",
          "[render][clock][event][block]") {
    BlockEventScheduler scheduler;
    REQUIRE(scheduler.set_position(std::numeric_limits<std::int64_t>::max()));
    REQUIRE(scheduler.schedule(event_at(std::numeric_limits<std::int64_t>::max(), 60, 100)));
    scheduler.play();

    const auto context = SignalBlockContext::create(480.0, 1);
    REQUIRE(context);
    std::array<SampleOffsetEvent, 1> output{};
    output[0] = SampleOffsetEvent{7, event_at(7, 61, 100)};
    const auto result = scheduler.process_block(*context, 1, output);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::ArithmeticOverflow);
    CHECK(output[0].sample_offset == 7);
    CHECK(output[0].scheduled.tick == 7);
    CHECK(scheduler.position().ticks == std::numeric_limits<std::int64_t>::max());
    CHECK(scheduler.scheduled_count() == 1);
}

TEST_CASE("Block event offsets preserve fractions at large tick positions",
          "[render][clock][event][block]") {
    // At 120 BPM and 480 PPQ this is exactly one sixteenth of a tick per sample.
    const auto context = SignalBlockContext::create(15360.0, 17);
    REQUIRE(context);
    for (const auto origin : {std::int64_t{0},
                              std::int64_t{1} << 53,
                              std::int64_t{1} << 62,
                              std::numeric_limits<std::int64_t>::max() - 4}) {
        INFO("origin = " << origin);
        BlockEventScheduler scheduler;
        REQUIRE(scheduler.set_position(origin));
        REQUIRE(scheduler.schedule(event_at(origin, 60, 100)));
        REQUIRE(scheduler.schedule(event_at(origin + 1, 61, 100)));
        REQUIRE(scheduler.schedule(event_at(origin + 2, 62, 100)));
        scheduler.play();
        std::array<SampleOffsetEvent, 3> output{};

        const auto first = scheduler.process_block(*context, 1, output);
        REQUIRE(first);
        REQUIRE(first->events_written == 1);
        CHECK(output[0].scheduled.tick == origin);
        CHECK(output[0].sample_offset == 0);
        CHECK(scheduler.position().ticks == origin);

        const auto boundary = scheduler.process_block(*context, 15, output);
        REQUIRE(boundary);
        CHECK(boundary->events_written == 0); // Exact right edge belongs to the next block.
        CHECK(scheduler.position().ticks == origin + 1);

        const auto next = scheduler.process_block(*context, 17, output);
        REQUIRE(next);
        REQUIRE(next->events_written == 2);
        CHECK(output[0].scheduled.tick == origin + 1);
        CHECK(output[0].sample_offset == 0);
        CHECK(output[1].scheduled.tick == origin + 2);
        CHECK(output[1].sample_offset == 16);
        CHECK(scheduler.scheduled_count() == 0);
    }
}
