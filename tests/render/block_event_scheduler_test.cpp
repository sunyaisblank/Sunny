/**
 * @file block_event_scheduler_test.cpp
 * @brief Bounded sample-offset event scheduling tests
 */

#include <algorithm>
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

namespace {

// Exact placement oracle. With tempo * ppq = ticks_per_minute / scale and
// sample_rate * 60 = samples_per_minute / scale, tick t lies at sample
// position t * samples_per_minute / ticks_per_minute, and the event belongs
// to the sample whose half-open interval contains that position. A position
// within the scheduler's documented tolerance below a boundary is snapped up
// to it; integral rates never need the tolerance.
struct ExactRate {
    std::int64_t ticks_per_minute;
    std::int64_t samples_per_minute;
};

std::int64_t exact_sample(std::int64_t tick, ExactRate rate) {
    const std::int64_t numerator = tick * rate.samples_per_minute;
    const std::int64_t whole = numerator / rate.ticks_per_minute;
    const std::int64_t remainder = numerator % rate.ticks_per_minute;
    const long double tolerance = sunny::render::BLOCK_SAMPLE_SNAP_TOLERANCE;
    if (remainder != 0 && static_cast<long double>(rate.ticks_per_minute - remainder) <=
                              tolerance * static_cast<long double>(rate.ticks_per_minute))
        return whole + 1;
    return whole;
}

struct PlacementRun {
    std::size_t events = 0;
    std::size_t mismatches = 0;
    std::int64_t first_tick = -1;
    std::int64_t first_expected = -1;
    std::int64_t first_actual = -1;
};

// Schedule every tick in [0, last_tick] (lazily, within queue capacity) and
// play blocks of block_frames until each has been emitted, comparing each
// emitted absolute sample index with the oracle.
PlacementRun run_placement(double sample_rate,
                           double tempo,
                           std::int64_t ppq,
                           std::int64_t block_frames,
                           std::int64_t last_tick,
                           ExactRate rate) {
    PlacementRun run;
    auto scheduler = BlockEventScheduler::create(ppq);
    REQUIRE(scheduler);
    REQUIRE(scheduler->set_tempo(tempo));
    const auto context = SignalBlockContext::create(sample_rate, block_frames);
    REQUIRE(context);
    scheduler->play();

    std::array<SampleOffsetEvent, BLOCK_EVENT_QUEUE_CAPACITY> output{};
    std::int64_t next_tick = 0;
    std::int64_t block_start = 0;
    while (run.events < static_cast<std::size_t>(last_tick + 1)) {
        while (next_tick <= last_tick &&
               scheduler->scheduled_count() < BLOCK_EVENT_QUEUE_CAPACITY) {
            REQUIRE(scheduler->schedule(event_at(next_tick, 60, 100)));
            ++next_tick;
        }
        const auto advanced = scheduler->process_block(*context, block_frames, output);
        REQUIRE(advanced);
        for (std::size_t index = 0; index < advanced->events_written; ++index) {
            const auto tick = output[index].scheduled.tick;
            const auto actual =
                block_start + static_cast<std::int64_t>(output[index].sample_offset);
            const auto expected = exact_sample(tick, rate);
            if (actual != expected && run.mismatches++ == 0) {
                run.first_tick = tick;
                run.first_expected = expected;
                run.first_actual = actual;
            }
        }
        run.events += advanced->events_written;
        block_start += block_frames;
        // Every tick the clock has passed must already have been queued.
        REQUIRE((next_tick > last_tick || next_tick > scheduler->position().ticks));
    }
    return run;
}

} // namespace

TEST_CASE("Block event on an exact sample boundary fires at that offset",
          "[render][clock][event][block]") {
    // 100 BPM at 480 PPQ is 48,000 ticks a minute; 48 kHz is 2,880,000
    // samples a minute, so each tick is exactly 60 samples.
    auto scheduler = BlockEventScheduler::create(480);
    REQUIRE(scheduler);
    REQUIRE(scheduler->set_tempo(100.0));
    REQUIRE(scheduler->schedule(event_at(1, 60, 100)));
    REQUIRE(scheduler->schedule(event_at(512, 61, 100)));
    scheduler->play();
    const auto context = SignalBlockContext::create(48000.0, 30721);
    REQUIRE(context);
    std::array<SampleOffsetEvent, 2> output{};

    const auto block = scheduler->process_block(*context, 30721, output);
    REQUIRE(block);
    REQUIRE(block->events_written == 2);
    CHECK(output[0].sample_offset == 60);
    CHECK(output[1].sample_offset == 30720);
}

TEST_CASE("Block event offsets equal exact rational placement across rates",
          "[render][clock][event][block][exhaustive]") {
    // Integral tempo * ppq and sample_rate * 60, so placement must be exact.
    for (const double sample_rate : {11025.0, 22050.0, 32000.0, 44100.0, 48000.0, 88200.0, 96000.0})
        for (const double tempo : {60.0, 90.0, 100.0, 120.0, 127.5, 140.0, 174.0})
            for (const std::int64_t ppq :
                 {std::int64_t{24}, std::int64_t{96}, std::int64_t{480}, std::int64_t{960}})
                for (const std::int64_t block :
                     {std::int64_t{1}, std::int64_t{64}, std::int64_t{441}, std::int64_t{512}}) {
                    const ExactRate rate{static_cast<std::int64_t>(tempo * 2.0) * ppq,
                                         static_cast<std::int64_t>(sample_rate) * 120};
                    // Keep each run near 20,000 samples.
                    const std::int64_t last_tick = std::min<std::int64_t>(
                        2000, 20000 * rate.ticks_per_minute / rate.samples_per_minute);
                    INFO("rate " << sample_rate << " tempo " << tempo << " ppq " << ppq << " block "
                                 << block);
                    const auto run = run_placement(sample_rate, tempo, ppq, block, last_tick, rate);
                    INFO("first mismatch: tick " << run.first_tick << " expected "
                                                 << run.first_expected << " actual "
                                                 << run.first_actual);
                    CHECK(run.events == static_cast<std::size_t>(last_tick + 1));
                    CHECK(run.mismatches == 0);
                }
}

TEST_CASE("Block event offsets with a non-integral tick rate snap only within the tolerance",
          "[render][clock][event][block]") {
    // Tempos with a binary fraction give tempo * ppq exactly as an integer
    // over 1024, so the oracle stays exact although the scheduler cannot
    // use its integral path.
    for (const double sample_rate : {44100.0, 48000.0})
        for (const double tempo : {100.0 + 1.0 / 1024.0, 127.75 + 3.0 / 1024.0})
            for (const std::int64_t ppq : {std::int64_t{1}, std::int64_t{3}, std::int64_t{7}})
                for (const std::int64_t block : {std::int64_t{1}, std::int64_t{63}}) {
                    const ExactRate rate{static_cast<std::int64_t>(tempo * 1024.0) * ppq,
                                         static_cast<std::int64_t>(sample_rate) * 60 * 1024};
                    INFO("rate " << sample_rate << " tempo " << tempo << " ppq " << ppq << " block "
                                 << block);
                    const auto run = run_placement(sample_rate, tempo, ppq, block, 40, rate);
                    INFO("first mismatch: tick " << run.first_tick << " expected "
                                                 << run.first_expected << " actual "
                                                 << run.first_actual);
                    CHECK(run.events == 41);
                    CHECK(run.mismatches == 0);
                }
}

TEST_CASE("Block event boundary placement is exact at large tick positions",
          "[render][clock][event][block]") {
    // 48 kHz, 100 BPM, 480 PPQ: one tick every 60 samples from any origin.
    const auto context = SignalBlockContext::create(48000.0, 70);
    REQUIRE(context);
    for (const auto origin : {std::int64_t{0},
                              std::int64_t{1} << 53,
                              std::int64_t{1} << 62,
                              std::numeric_limits<std::int64_t>::max() - 4}) {
        INFO("origin = " << origin);
        auto scheduler = BlockEventScheduler::create(480);
        REQUIRE(scheduler);
        REQUIRE(scheduler->set_tempo(100.0));
        REQUIRE(scheduler->set_position(origin));
        REQUIRE(scheduler->schedule(event_at(origin + 1, 60, 100)));
        REQUIRE(scheduler->schedule(event_at(origin + 2, 61, 100)));
        scheduler->play();
        std::array<SampleOffsetEvent, 2> output{};

        const auto before = scheduler->process_block(*context, 60, output);
        REQUIRE(before);
        CHECK(before->events_written == 0); // Sample 60 is the exclusive right edge.
        CHECK(scheduler->position().ticks == origin + 1);

        const auto at = scheduler->process_block(*context, 61, output);
        REQUIRE(at);
        REQUIRE(at->events_written == 2);
        CHECK(output[0].scheduled.tick == origin + 1);
        CHECK(output[0].sample_offset == 0);
        CHECK(output[1].scheduled.tick == origin + 2);
        CHECK(output[1].sample_offset == 60);
    }
}

TEST_CASE("Block event placement carries the sub-tick position across rate changes",
          "[render][clock][event][block]") {
    // 30 samples at 48 kHz and 100 BPM (60 samples a tick) leave the clock
    // half-way through tick 0.
    SECTION("Tempo change") {
        auto scheduler = BlockEventScheduler::create(480);
        REQUIRE(scheduler);
        REQUIRE(scheduler->set_tempo(100.0));
        REQUIRE(scheduler->schedule(event_at(1, 60, 100)));
        scheduler->play();
        const auto context = SignalBlockContext::create(48000.0, 64);
        REQUIRE(context);
        std::array<SampleOffsetEvent, 1> output{};
        REQUIRE(scheduler->process_block(*context, 30, output)->events_written == 0);
        // At 120 BPM a tick is 50 samples, so the remaining half tick is 25.
        REQUIRE(scheduler->set_tempo(120.0));
        const auto next = scheduler->process_block(*context, 64, output);
        REQUIRE(next);
        REQUIRE(next->events_written == 1);
        CHECK(output[0].sample_offset == 25);
    }

    SECTION("Sample-rate change") {
        auto scheduler = BlockEventScheduler::create(480);
        REQUIRE(scheduler);
        REQUIRE(scheduler->set_tempo(100.0));
        REQUIRE(scheduler->schedule(event_at(1, 60, 100)));
        scheduler->play();
        const auto first = SignalBlockContext::create(48000.0, 64);
        const auto second = SignalBlockContext::create(44100.0, 64);
        REQUIRE(first);
        REQUIRE(second);
        std::array<SampleOffsetEvent, 1> output{};
        REQUIRE(scheduler->process_block(*first, 30, output)->events_written == 0);
        // At 44.1 kHz a tick is 55.125 samples; half of it is 27.5625, so
        // the tick falls in sample 27.
        const auto next = scheduler->process_block(*second, 64, output);
        REQUIRE(next);
        REQUIRE(next->events_written == 1);
        CHECK(output[0].sample_offset == 27);
    }
}
