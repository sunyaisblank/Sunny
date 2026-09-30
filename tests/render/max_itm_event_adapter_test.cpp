/**
 * @file max_itm_event_adapter_test.cpp
 * @brief Max ITM producer transfer and permanent-event queue tests
 */

#include <array>
#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/max/itm_event_adapter.hpp>
#include <thread>
#include <vector>

using Catch::Approx;
using sunny::core::ErrorCode;
using sunny::max::ItmEventAdapter;
using sunny::max::ItmHostSnapshot;
using sunny::max::MAX_CONTROL_QUEUE_CAPACITY;
using sunny::max::MAX_ITM_EVENT_CAPACITY;
using sunny::render::ScheduledEvent;

namespace {

struct SlotActions {
    std::array<std::size_t, 1024> scheduled_slots{};
    std::array<double, 1024> scheduled_ticks{};
    std::array<std::size_t, 1024> cancelled_slots{};
    std::size_t scheduled_count = 0;
    std::size_t cancelled_count = 0;
};

struct WakeEvidence {
    std::atomic<int> active{0};
    std::atomic<int> maximum_active{0};
    std::atomic<int> calls{0};
};

void record_wake(void* context) noexcept {
    auto& evidence = *static_cast<WakeEvidence*>(context);
    const auto active = evidence.active.fetch_add(1, std::memory_order_acq_rel) + 1;
    auto maximum = evidence.maximum_active.load(std::memory_order_relaxed);
    while (maximum < active &&
           !evidence.maximum_active.compare_exchange_weak(
               maximum, active, std::memory_order_release, std::memory_order_relaxed)) {}
    std::this_thread::yield();
    evidence.calls.fetch_add(1, std::memory_order_relaxed);
    evidence.active.fetch_sub(1, std::memory_order_release);
}

void record_schedule(void* context, std::size_t slot, double host_tick) noexcept {
    auto& actions = *static_cast<SlotActions*>(context);
    actions.scheduled_slots[actions.scheduled_count] = slot;
    actions.scheduled_ticks[actions.scheduled_count] = host_tick;
    ++actions.scheduled_count;
}

void record_cancel(void* context, std::size_t slot) noexcept {
    auto& actions = *static_cast<SlotActions*>(context);
    actions.cancelled_slots[actions.cancelled_count++] = slot;
}

auto apply(ItmEventAdapter& adapter,
           SlotActions& actions,
           double current_tick = 0.0,
           double resolution = 480.0) {
    return adapter.apply_pending(
        ItmHostSnapshot{current_tick, resolution}, &actions, record_schedule, record_cancel);
}

} // namespace

TEST_CASE("Sunny ticks map injectively into the admitted Max ITM double region",
          "[max][itm][event][timing]") {
    const auto identity = sunny::max::map_tick_to_itm(960, 480, 480.0);
    REQUIRE(identity);
    CHECK(*identity == 960.0);

    const auto fractional = sunny::max::map_tick_to_itm(1, 960, 480.0);
    REQUIRE(fractional);
    CHECK(*fractional == Approx(0.5));

    CHECK_FALSE(sunny::max::map_tick_to_itm(-1, 480, 480.0));
    CHECK_FALSE(sunny::max::map_tick_to_itm(1, 0, 480.0));
    CHECK_FALSE(sunny::max::map_tick_to_itm(1, 480, 0.0));
    CHECK_FALSE(sunny::max::map_tick_to_itm(1, 480, std::numeric_limits<double>::quiet_NaN()));
    const auto collapsed =
        sunny::max::map_tick_to_itm(std::numeric_limits<std::int64_t>::max(), 480, 480.0);
    REQUIRE_FALSE(collapsed);
    CHECK(collapsed.error() == ErrorCode::RenderUnrepresentableTiming);
}

TEST_CASE("Max ITM positions map to strict-future Sunny relative targets",
          "[max][itm][event][timing][relative]") {
    const auto exact = sunny::max::tick_after_itm(ItmHostSnapshot{480.0, 480.0}, 480, 0);
    REQUIRE(exact);
    CHECK(*exact == 481);

    const auto fractional = sunny::max::tick_after_itm(ItmHostSnapshot{50.25, 480.0}, 960, 0);
    REQUIRE(fractional);
    CHECK(*fractional == 101);
    REQUIRE(sunny::max::map_tick_to_itm(*fractional, 960, 480.0));
    CHECK(*sunny::max::map_tick_to_itm(*fractional, 960, 480.0) > 50.25);

    const auto delayed = sunny::max::tick_after_itm(ItmHostSnapshot{50.25, 480.0}, 960, 24);
    REQUIRE(delayed);
    CHECK(*delayed == 125);

    CHECK(sunny::max::tick_after_itm(ItmHostSnapshot{0.0, 480.0}, 0, 0).error() ==
          ErrorCode::RenderInvalidPPQ);
    CHECK(sunny::max::tick_after_itm(ItmHostSnapshot{0.0, 480.0}, 480, -1).error() ==
          ErrorCode::RenderInvalidPosition);
    CHECK(sunny::max::tick_after_itm(
              ItmHostSnapshot{std::numeric_limits<double>::quiet_NaN(), 480.0}, 480, 0)
              .error() == ErrorCode::RenderUnrepresentableTiming);
    CHECK(sunny::max::tick_after_itm(ItmHostSnapshot{0.0, 0.0}, 480, 0).error() ==
          ErrorCode::RenderUnrepresentableTiming);
    CHECK(sunny::max::tick_after_itm(
              ItmHostSnapshot{static_cast<double>(std::numeric_limits<std::int64_t>::max()), 480.0},
              480,
              0)
              .error() == ErrorCode::ArithmeticOverflow);

    ItmEventAdapter adapter;
    const auto sampled = sunny::max::tick_after_itm(ItmHostSnapshot{100.0, 480.0}, 480, 0);
    REQUIRE(sampled);
    REQUIRE(adapter.publish_event(*sampled, 60, 100));
    SlotActions actions;
    const auto stale = apply(adapter, actions, 101.0, 480.0);
    CHECK(stale.commands_applied == 0);
    CHECK(stale.commands_rejected == 1);
    CHECK(stale.last_error == ErrorCode::RenderInvalidPosition);
    CHECK(actions.scheduled_count == 0);
}

TEST_CASE("ITM publication validates every raw integer domain before reservation",
          "[max][itm][event][validation]") {
    ItmEventAdapter invalid_ppq{0};
    const auto bad_ppq = invalid_ppq.publish_event(1, 60, 100);
    REQUIRE_FALSE(bad_ppq);
    CHECK(bad_ppq.error() == ErrorCode::RenderInvalidPPQ);

    ItmEventAdapter adapter;
    CHECK(adapter.publish_event(-1, 60, 100).error() == ErrorCode::RenderInvalidPosition);
    CHECK(adapter.publish_event(1, -1, 100).error() == ErrorCode::InvalidMidiNote);
    CHECK(adapter.publish_event(1, 128, 100).error() == ErrorCode::InvalidMidiNote);
    CHECK(adapter.publish_event(1, 60, -1).error() == ErrorCode::InvalidVelocity);
    CHECK(adapter.publish_event(1, 60, 128).error() == ErrorCode::InvalidVelocity);
    CHECK(adapter.publish_event(1, 60, 100, 128, nullptr, nullptr).error() ==
          ErrorCode::InvalidVelocity);
    CHECK(adapter.publish_note(1, 60, 0, 100).error() == ErrorCode::RenderUnrepresentableTiming);
    CHECK(adapter.publish_note(1, 60, 1, 0).error() == ErrorCode::InvalidVelocity);
    CHECK(adapter.publish_note(std::numeric_limits<std::int64_t>::max(), 60, 1, 100).error() ==
          ErrorCode::ArithmeticOverflow);
    const auto status = adapter.status();
    CHECK(status.commands_enqueued == 0);
    CHECK(status.commands_pending == 0);
    CHECK(status.events_reserved == 0);
    CHECK(status.events_retained == 0);
    CHECK(status.last_error == ErrorCode::ArithmeticOverflow);
    adapter.clear_error();
    CHECK(adapter.status().last_error == ErrorCode::Ok);
    CHECK(adapter.status().commands_rejected == status.commands_rejected);
}

TEST_CASE("ITM adapter applies future events and atomic note pairs to fixed slots",
          "[max][itm][event]") {
    ItmEventAdapter adapter{960};
    REQUIRE(adapter.publish_event(200, 60, 90));
    REQUIRE(adapter.publish_note(240, 61, 20, 100, 23, nullptr, nullptr));
    const auto published = adapter.status();
    CHECK(published.commands_pending == 2);
    CHECK(published.events_reserved == 3);
    CHECK(published.events_retained == 0);

    SlotActions actions;
    const auto report = apply(adapter, actions, 50.0, 480.0);
    CHECK(report.commands_applied == 2);
    CHECK(report.commands_rejected == 0);
    REQUIRE(actions.scheduled_count == 3);
    CHECK(actions.scheduled_ticks[0] == Approx(100.0));
    CHECK(actions.scheduled_ticks[1] == Approx(120.0));
    CHECK(actions.scheduled_ticks[2] == Approx(130.0));

    std::array<ScheduledEvent, 2> output{};
    const auto note_on = adapter.fire(actions.scheduled_slots[1], output, &actions, record_cancel);
    REQUIRE(note_on);
    CHECK(*note_on == 1);
    CHECK(output[0].tick == 240);
    CHECK(output[0].event.pitch == 61);
    CHECK(output[0].event.velocity == 100);
    CHECK(output[0].event.release_velocity == 23);

    const auto status = adapter.status();
    CHECK(status.commands_enqueued == 2);
    CHECK(status.commands_applied == 2);
    CHECK(status.commands_pending == 0);
    CHECK(status.events_scheduled == 3);
    CHECK(status.events_fired == 1);
    CHECK(status.events_reserved == 2);
    CHECK(status.events_retained == 2);
}

TEST_CASE("ITM equal-tick dispatch is ordered and buffer saturation is transactional",
          "[max][itm][event][ordering]") {
    ItmEventAdapter adapter;
    REQUIRE(adapter.publish_event(100, 60, 100));
    REQUIRE(adapter.publish_event(100, 61, 0));
    SlotActions actions;
    REQUIRE(apply(adapter, actions).commands_applied == 2);
    REQUIRE(actions.scheduled_count == 2);

    std::array<ScheduledEvent, 1> too_small{};
    too_small[0].tick = 999;
    const auto saturated = adapter.fire(actions.scheduled_slots[0], too_small, nullptr, nullptr);
    REQUIRE_FALSE(saturated);
    CHECK(saturated.error() == ErrorCode::RenderEventBufferFull);
    CHECK(too_small[0].tick == 999);
    CHECK(adapter.status().events_reserved == 2);
    CHECK(adapter.status().events_retained == 2);
    CHECK(adapter.status().callback_failures == 1);
    CHECK(actions.cancelled_count == 0);

    std::array<ScheduledEvent, 2> complete{};
    complete[0].tick = 998;
    complete[1].tick = 999;
    const auto missing_stop = adapter.fire(actions.scheduled_slots[0], complete, nullptr, nullptr);
    REQUIRE_FALSE(missing_stop);
    CHECK(missing_stop.error() == ErrorCode::RenderInvalidParameter);
    CHECK(adapter.status().events_reserved == 2);
    CHECK(adapter.status().events_retained == 2);
    CHECK(adapter.status().callback_failures == 2);
    CHECK(actions.cancelled_count == 0);
    CHECK(complete[0].tick == 998);
    CHECK(complete[1].tick == 999);

    const auto fired = adapter.fire(actions.scheduled_slots[0], complete, &actions, record_cancel);
    REQUIRE(fired);
    CHECK(*fired == 2);
    CHECK(complete[0].event.velocity == 0);
    CHECK(complete[0].event.pitch == 61);
    CHECK(complete[1].event.velocity == 100);
    CHECK(complete[1].event.pitch == 60);
    REQUIRE(actions.cancelled_count == 2);
    CHECK(actions.cancelled_slots[0] == actions.scheduled_slots[1]);
    CHECK(actions.cancelled_slots[1] == actions.scheduled_slots[0]);
    CHECK(adapter.status().events_reserved == 0);
    CHECK(adapter.status().events_retained == 0);

    const auto duplicate_callback =
        adapter.fire(actions.scheduled_slots[1], complete, nullptr, nullptr);
    REQUIRE(duplicate_callback);
    CHECK(*duplicate_callback == 0);
}

TEST_CASE("ITM command and event capacities fail closed", "[max][itm][event][capacity]") {
    ItmEventAdapter adapter;
    for (std::size_t index = 0; index < MAX_CONTROL_QUEUE_CAPACITY; ++index)
        REQUIRE(adapter.publish_event(1000, 60, 100));
    const auto command_full = adapter.publish_event(1000, 60, 100);
    REQUIRE_FALSE(command_full);
    CHECK(command_full.error() == ErrorCode::RenderControlQueueFull);
    const auto clear_full = adapter.publish_clear();
    REQUIRE_FALSE(clear_full);
    CHECK(clear_full.error() == ErrorCode::RenderControlQueueFull);
    CHECK(adapter.status().commands_pending == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(adapter.status().events_reserved == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(adapter.status().events_retained == 0);

    SlotActions actions;
    for (std::size_t batch = 0; batch < MAX_ITM_EVENT_CAPACITY / MAX_CONTROL_QUEUE_CAPACITY;
         ++batch) {
        if (batch != 0) {
            for (std::size_t index = 0; index < MAX_CONTROL_QUEUE_CAPACITY; ++index)
                REQUIRE(adapter.publish_event(1000, 60, 100));
        }
        const auto report = apply(adapter, actions);
        CHECK(report.commands_rejected == 0);
    }
    CHECK(adapter.status().events_reserved == MAX_ITM_EVENT_CAPACITY);
    CHECK(adapter.status().events_retained == MAX_ITM_EVENT_CAPACITY);
    const auto event_full = adapter.publish_event(1000, 60, 100);
    REQUIRE_FALSE(event_full);
    CHECK(event_full.error() == ErrorCode::RenderEventQueueFull);
    const auto pair_full = adapter.publish_note(1000, 61, 10, 100);
    REQUIRE_FALSE(pair_full);
    CHECK(pair_full.error() == ErrorCode::RenderEventQueueFull);

    std::array<ScheduledEvent, MAX_ITM_EVENT_CAPACITY> output{};
    const auto fired = adapter.fire(actions.scheduled_slots[0], output, &actions, record_cancel);
    REQUIRE(fired);
    CHECK(*fired == MAX_ITM_EVENT_CAPACITY);
    CHECK(adapter.status().events_reserved == 0);
    CHECK(adapter.status().events_retained == 0);
}

TEST_CASE("ITM strict-future and invalid-host failures release whole reservations",
          "[max][itm][event][rollback]") {
    ItmEventAdapter adapter;
    REQUIRE(adapter.publish_note(100, 60, 10, 100));
    SlotActions actions;
    const auto past = apply(adapter, actions, 100.0);
    CHECK(past.commands_applied == 0);
    CHECK(past.commands_rejected == 1);
    CHECK(past.last_error == ErrorCode::RenderInvalidPosition);
    CHECK(actions.scheduled_count == 0);
    CHECK(adapter.status().events_reserved == 0);

    REQUIRE(adapter.publish_event(200, 60, 100));
    const auto invalid_host = apply(adapter, actions, 0.0, std::numeric_limits<double>::infinity());
    CHECK(invalid_host.commands_rejected == 1);
    CHECK(invalid_host.last_error == ErrorCode::RenderUnrepresentableTiming);
    CHECK(adapter.status().events_reserved == 0);
}

TEST_CASE("ITM clear is ordered with later publications and suppresses old callbacks",
          "[max][itm][event][clear]") {
    ItmEventAdapter adapter;
    REQUIRE(adapter.publish_event(100, 60, 100));
    REQUIRE(adapter.publish_event(150, 62, 100));
    SlotActions first;
    REQUIRE(apply(adapter, first).commands_applied == 2);
    REQUIRE(first.scheduled_count == 2);
    const auto old_slot = first.scheduled_slots[1];

    REQUIRE(adapter.publish_clear());
    REQUIRE(adapter.publish_event(200, 61, 100));
    const auto before_second_apply = adapter.status();
    CHECK(before_second_apply.commands_pending == 2);
    CHECK(before_second_apply.events_reserved == 3);
    CHECK(before_second_apply.events_retained == 2);
    SlotActions second;
    const auto report = apply(adapter, second);
    CHECK(report.commands_applied == 2);
    REQUIRE(second.cancelled_count == 2);
    CHECK(second.cancelled_slots[1] == old_slot);
    REQUIRE(second.scheduled_count == 1);
    CHECK(adapter.status().events_reserved == 1);
    CHECK(adapter.status().events_retained == 1);
    CHECK(adapter.status().commands_pending == 0);

    std::array<ScheduledEvent, 1> output{};
    const auto old_callback = adapter.fire(old_slot, output, nullptr, nullptr);
    REQUIRE(old_callback);
    CHECK(*old_callback == 0);
    const auto current = adapter.fire(second.scheduled_slots[0], output, &second, record_cancel);
    REQUIRE(current);
    CHECK(*current == 1);
    CHECK(output[0].tick == 200);
    CHECK(adapter.status().events_cancelled == 2);
    CHECK(adapter.status().events_retained == 0);
}

TEST_CASE("ITM host-effect evidence requires the corresponding action callbacks",
          "[max][itm][event][callback-contract]") {
    ItmEventAdapter empty;
    REQUIRE(empty.publish_clear());
    const auto empty_clear =
        empty.apply_pending(ItmHostSnapshot{0.0, 480.0}, nullptr, nullptr, nullptr);
    CHECK(empty_clear.commands_applied == 1);
    CHECK(empty_clear.commands_rejected == 0);

    ItmEventAdapter adapter;
    REQUIRE(adapter.publish_event(100, 60, 100));
    SlotActions actions;

    const auto unscheduled =
        adapter.apply_pending(ItmHostSnapshot{0.0, 480.0}, &actions, nullptr, record_cancel);
    CHECK(unscheduled.commands_applied == 0);
    CHECK(unscheduled.commands_rejected == 1);
    CHECK(unscheduled.last_error == ErrorCode::RenderInvalidParameter);
    CHECK(actions.scheduled_count == 0);
    CHECK(adapter.status().events_scheduled == 0);
    CHECK(adapter.status().events_reserved == 0);
    CHECK(adapter.status().events_retained == 0);

    REQUIRE(adapter.publish_event(120, 61, 100));
    REQUIRE(apply(adapter, actions).commands_applied == 1);
    CHECK(adapter.status().events_scheduled == 1);
    CHECK(adapter.status().events_retained == 1);
    REQUIRE(adapter.publish_clear());

    const auto uncancelled =
        adapter.apply_pending(ItmHostSnapshot{0.0, 480.0}, &actions, record_schedule, nullptr);
    CHECK(uncancelled.commands_applied == 0);
    CHECK(uncancelled.commands_rejected == 1);
    CHECK(uncancelled.last_error == ErrorCode::RenderInvalidParameter);
    CHECK(actions.cancelled_count == 0);
    CHECK(adapter.status().events_cancelled == 0);
    CHECK(adapter.status().events_reserved == 1);
    CHECK(adapter.status().events_retained == 1);

    REQUIRE(adapter.publish_clear());
    const auto cancelled = apply(adapter, actions);
    CHECK(cancelled.commands_applied == 1);
    CHECK(cancelled.commands_rejected == 0);
    CHECK(actions.cancelled_count == 1);
    CHECK(adapter.status().events_cancelled == 1);
    CHECK(adapter.status().events_reserved == 0);
    CHECK(adapter.status().events_retained == 0);
}

TEST_CASE("ITM event publishers are serialized before the one scheduler consumer",
          "[max][itm][event][thread]") {
    ItmEventAdapter adapter;
    std::atomic<int> failures{0};
    WakeEvidence wake;
    std::vector<std::thread> publishers;
    publishers.reserve(8);
    for (int thread = 0; thread < 8; ++thread) {
        publishers.emplace_back([&adapter, &failures, &wake, thread] {
            for (int event = 0; event < 8; ++event) {
                if (!adapter.publish_event(100 + thread * 8 + event, 60, 100, &wake, record_wake))
                    failures.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto& publisher : publishers)
        publisher.join();
    REQUIRE(failures.load(std::memory_order_relaxed) == 0);
    CHECK(wake.calls.load(std::memory_order_relaxed) == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(wake.maximum_active.load(std::memory_order_relaxed) == 1);

    SlotActions actions;
    const auto report = apply(adapter, actions);
    CHECK(report.commands_applied == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(report.commands_rejected == 0);
    CHECK(actions.scheduled_count == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(adapter.status().events_reserved == MAX_CONTROL_QUEUE_CAPACITY);
}
