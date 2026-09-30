/**
 * @file max_clock_adapter_test.cpp
 * @brief Bounded local-clock Max adapter tests
 */

#include <atomic>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/max/clock_adapter.hpp>
#include <thread>
#include <vector>

using Catch::Approx;
using sunny::core::ErrorCode;
using sunny::max::ClockAdapter;
using sunny::max::MAX_CONTROL_QUEUE_CAPACITY;

TEST_CASE("Clock adapter applies ordered controls at one valid vector edge",
          "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(1920.0, 4));
    REQUIRE(adapter.publish_position(480));
    REQUIRE(adapter.publish_tempo(120.0));
    REQUIRE(adapter.publish_play());

    double samples[4]{-1.0, -1.0, -1.0, -1.0};
    double* outputs[1]{samples};
    REQUIRE(adapter.process(4, 0, nullptr, 1, outputs));
    CHECK(samples[0] == Approx(1.0));
    CHECK(samples[1] == Approx(1.0 + 1.0 / 960.0));
    CHECK(samples[2] == Approx(1.0 + 1.0 / 480.0));
    CHECK(samples[3] == Approx(1.0 + 1.0 / 320.0));

    const auto status = adapter.status();
    CHECK(status.configured);
    CHECK(status.sample_rate == 1920.0);
    CHECK(status.maximum_frames == 4);
    CHECK(status.controls_enqueued == 3);
    CHECK(status.controls_applied == 3);
    CHECK(status.controls_rejected == 0);
    CHECK(status.process_calls == 1);
    CHECK(status.last_frame_count == 4);
    CHECK(status.process_failures == 0);
}

TEST_CASE("Clock adapter validates callback shape before draining controls",
          "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE(adapter.publish_play());

    double sample = 17.0;
    double* outputs[1]{&sample};
    const auto unconfigured = adapter.process(1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(unconfigured);
    CHECK(unconfigured.error() == ErrorCode::RenderNotConfigured);
    CHECK(sample == 17.0);
    CHECK(adapter.status().controls_applied == 0);
    CHECK(adapter.status().controls_pending == 1);

    REQUIRE(adapter.configure_dsp(48'000.0, 1));
    const auto wrong_input_count = adapter.process(1, 1, nullptr, 1, outputs);
    REQUIRE_FALSE(wrong_input_count);
    CHECK(wrong_input_count.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(sample == 17.0);
    CHECK(adapter.status().controls_applied == 0);

    const auto missing_output = adapter.process(1, 0, nullptr, 1, nullptr);
    REQUIRE_FALSE(missing_output);
    CHECK(missing_output.error() == ErrorCode::RenderInvalidSignalBuffer);
    CHECK(adapter.status().controls_applied == 0);

    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.0);
    CHECK(adapter.status().controls_applied == 1);
    CHECK(adapter.status().controls_pending == 0);
}

TEST_CASE("Clock adapter control domains and capacity are fail-closed", "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE_FALSE(adapter.configure_dsp(0.0, 64));
    REQUIRE_FALSE(adapter.publish_tempo(19.0));
    REQUIRE_FALSE(adapter.publish_tempo(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(adapter.publish_position(-1));

    for (std::size_t index = 0; index < MAX_CONTROL_QUEUE_CAPACITY; ++index)
        REQUIRE(adapter.publish_play());
    const auto saturated = adapter.publish_stop();
    REQUIRE_FALSE(saturated);
    CHECK(saturated.error() == ErrorCode::RenderControlQueueFull);

    const auto status = adapter.status();
    CHECK_FALSE(status.configured);
    CHECK(status.sample_rate == 0.0);
    CHECK(status.maximum_frames == 0);
    CHECK(status.setup_failures == 1);
    CHECK(status.controls_enqueued == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_applied == 0);
    CHECK(status.controls_pending == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_rejected == 4);
    CHECK(status.last_error == ErrorCode::RenderControlQueueFull);
}

TEST_CASE("Clock adapter preserves clock state across DSP reconfiguration",
          "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(1920.0, 2));
    REQUIRE(adapter.publish_play());

    double first[2]{};
    double* first_outputs[1]{first};
    REQUIRE(adapter.process(2, 0, nullptr, 1, first_outputs));
    CHECK(first[0] == Approx(0.0));
    CHECK(first[1] == Approx(1.0 / 960.0));

    REQUIRE(adapter.configure_dsp(960.0, 1));
    double next = -1.0;
    double* next_outputs[1]{&next};
    REQUIRE(adapter.process(1, 0, nullptr, 1, next_outputs));
    CHECK(next == Approx(1.0 / 480.0));
}

TEST_CASE("Clock adapter disables stale processing after a rejected DSP rebuild",
          "[max][clock][render][dsp-rebuild]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(1920.0, 1));
    REQUIRE(adapter.publish_play());

    const auto rejected = adapter.configure_dsp(0.0, 1);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == ErrorCode::RenderInvalidSampleRate);
    CHECK_FALSE(adapter.status().configured);
    CHECK(adapter.status().controls_pending == 1);

    double sample = 7.0;
    double* outputs[1]{&sample};
    const auto stale = adapter.process(1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(stale);
    CHECK(stale.error() == ErrorCode::RenderNotConfigured);
    CHECK(sample == 7.0);
    CHECK(adapter.status().controls_pending == 1);

    REQUIRE(adapter.configure_dsp(960.0, 1));
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.0);
    CHECK(adapter.status().controls_applied == 1);
}

TEST_CASE("Clock adapter pause record and stop preserve the declared local state algebra",
          "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(1920.0, 1));
    REQUIRE(adapter.publish_play());

    double sample{};
    double* outputs[1]{&sample};
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));

    REQUIRE(adapter.publish_pause());
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == Approx(1.0 / 960.0));
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == Approx(1.0 / 960.0));

    REQUIRE(adapter.publish_record());
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == Approx(1.0 / 960.0));
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == Approx(1.0 / 480.0));

    REQUIRE(adapter.publish_stop());
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.0);
}

TEST_CASE("Clock adapter serializes concurrent publishers into its bounded stream",
          "[max][clock][render][thread]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, MAX_CONTROL_QUEUE_CAPACITY));

    std::atomic<int> failures{0};
    std::vector<std::thread> publishers;
    publishers.reserve(8);
    for (int thread = 0; thread < 8; ++thread) {
        publishers.emplace_back([&adapter, &failures] {
            for (int command = 0; command < 8; ++command)
                if (!adapter.publish_play()) failures.fetch_add(1, std::memory_order_relaxed);
        });
    }
    for (auto& publisher : publishers)
        publisher.join();
    REQUIRE(failures.load(std::memory_order_relaxed) == 0);

    std::vector<double> samples(MAX_CONTROL_QUEUE_CAPACITY, -1.0);
    double* outputs[1]{samples.data()};
    REQUIRE(adapter.process(static_cast<std::int64_t>(samples.size()), 0, nullptr, 1, outputs));

    const auto status = adapter.status();
    CHECK(status.controls_enqueued == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_applied == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_rejected == 0);
}

TEST_CASE("Clock adapter reports an unrepresentable endpoint after applying valid edge controls",
          "[max][clock][render]") {
    ClockAdapter adapter;
    REQUIRE(adapter.configure_dsp(480.0, 1));
    REQUIRE(adapter.publish_position(std::numeric_limits<std::int64_t>::max()));
    REQUIRE(adapter.publish_play());

    double sample = 23.0;
    double* outputs[1]{&sample};
    const auto overflow = adapter.process(1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == ErrorCode::ArithmeticOverflow);
    CHECK(sample == 23.0);
    CHECK(adapter.status().controls_applied == 2);

    REQUIRE(adapter.silence_output(1, 1, outputs));
    CHECK(sample == 0.0);
    REQUIRE(adapter.publish_stop());
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.0);
}
