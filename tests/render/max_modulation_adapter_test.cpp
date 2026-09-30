/**
 * @file max_modulation_adapter_test.cpp
 * @brief Adversarial tests for the bounded Max control/audio ownership boundary
 */

#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <sunny/max/modulation_adapter.hpp>
#include <thread>
#include <vector>

using sunny::core::ErrorCode;
using sunny::max::EnvelopeAdapter;
using sunny::max::LfoAdapter;
using sunny::max::MAX_CONTROL_QUEUE_CAPACITY;
using sunny::max::SampleAndHoldAdapter;
using sunny::render::LfoWaveform;

TEST_CASE("Max LFO adapter requires setup and applies controls at a block boundary",
          "[max-adapter][lfo]") {
    LfoAdapter adapter;
    double sample = 7.0;
    double* outputs[1]{&sample};

    const auto unconfigured = adapter.process(1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(unconfigured);
    CHECK(unconfigured.error() == ErrorCode::RenderNotConfigured);
    CHECK(sample == 7.0);

    REQUIRE(adapter.configure_dsp(4.0, 2));
    REQUIRE(adapter.publish_waveform(LfoWaveform::Saw));
    REQUIRE(adapter.publish_phase(0.25));
    CHECK(adapter.status().controls_pending == 2);
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == -0.5);

    const auto status = adapter.status();
    CHECK(status.configured);
    CHECK(status.sample_rate == 4.0);
    CHECK(status.maximum_frames == 2);
    CHECK(status.setup_failures == 0);
    CHECK(status.controls_enqueued == 2);
    CHECK(status.controls_applied == 2);
    CHECK(status.controls_pending == 0);
    CHECK(status.process_calls == 2);
    CHECK(status.last_frame_count == 1);
    CHECK(status.process_failures == 1);
    CHECK(status.last_error == ErrorCode::RenderNotConfigured);
    adapter.clear_error();
    CHECK(adapter.status().last_error == ErrorCode::Ok);
}

TEST_CASE("Max LFO adapter invalidates a rejected DSP rebuild without losing source state",
          "[max-adapter][lfo]") {
    LfoAdapter adapter;
    REQUIRE(adapter.configure_dsp(200.0, 2));
    REQUIRE(adapter.publish_frequency(100.0));
    REQUIRE(adapter.publish_frequency(10.0));

    // The lower latest request cannot hide an earlier incompatible command.
    const auto incompatible_rebuild = adapter.configure_dsp(50.0, 2);
    REQUIRE_FALSE(incompatible_rebuild);
    CHECK(incompatible_rebuild.error() == ErrorCode::RenderInvalidSampleRate);

    double sample = 9.0;
    double* outputs[1]{&sample};
    const auto stale_process = adapter.process(1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(stale_process);
    CHECK(stale_process.error() == ErrorCode::RenderNotConfigured);
    CHECK(sample == 9.0);
    CHECK_FALSE(adapter.status().configured);
    CHECK(adapter.status().sample_rate == 0.0);
    CHECK(adapter.status().maximum_frames == 0);
    CHECK(adapter.status().controls_pending == 2);

    // The wrapper can silence against the current dsp64 maximum without
    // reactivating the adapter's stale processing context.
    REQUIRE(sunny::max::detail::silence_generator_output(2, 1, 0, 1, outputs));
    CHECK(sample == 0.0);

    REQUIRE(adapter.configure_dsp(200.0, 2));
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(std::abs(sample) < 1.0e-12);

    const auto invalid_frequency = adapter.publish_frequency(201.0);
    REQUIRE_FALSE(invalid_frequency);
    CHECK(invalid_frequency.error() == ErrorCode::RenderInvalidSampleRate);
    const auto invalid_phase = adapter.publish_phase(1.0);
    REQUIRE_FALSE(invalid_phase);
    CHECK(invalid_phase.error() == ErrorCode::RenderInvalidParameter);

    const auto status = adapter.status();
    CHECK(status.configured);
    CHECK(status.sample_rate == 200.0);
    CHECK(status.maximum_frames == 2);
    CHECK(status.controls_enqueued == 2);
    CHECK(status.controls_applied == 2);
    CHECK(status.setup_failures == 1);
    CHECK(status.controls_rejected == 2);
    CHECK(status.process_calls == 2);
    CHECK(status.last_frame_count == 1);
    CHECK(status.process_failures == 1);
}

TEST_CASE("Max adapter rejects malformed callbacks before draining queued controls",
          "[max-adapter][buffers]") {
    LfoAdapter lfo;
    REQUIRE(lfo.configure_dsp(4.0, 1));
    REQUIRE(lfo.publish_waveform(LfoWaveform::Saw));
    REQUIRE(lfo.publish_phase(0.25));

    double lfo_sample = 9.0;
    double* lfo_outputs[1]{&lfo_sample};
    const auto malformed_lfo = lfo.process(1, 1, nullptr, 1, lfo_outputs);
    REQUIRE_FALSE(malformed_lfo);
    CHECK(malformed_lfo.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(lfo_sample == 9.0);
    CHECK(lfo.status().controls_applied == 0);
    CHECK(lfo.status().controls_pending == 2);

    REQUIRE(lfo.process(1, 0, nullptr, 1, lfo_outputs));
    CHECK(lfo_sample == -0.5);
    CHECK(lfo.status().controls_applied == 2);
    CHECK(lfo.status().controls_pending == 0);

    EnvelopeAdapter envelope;
    REQUIRE(envelope.configure_dsp(100.0, 1));
    REQUIRE(envelope.publish_attack(0.0));
    REQUIRE(envelope.publish_decay(0.0));
    REQUIRE(envelope.publish_sustain(0.4));
    REQUIRE(envelope.publish_trigger());

    double envelope_sample = 8.0;
    double* envelope_outputs[1]{&envelope_sample};
    const auto malformed_envelope = envelope.process(1, 0, nullptr, 1, nullptr);
    REQUIRE_FALSE(malformed_envelope);
    CHECK(malformed_envelope.error() == ErrorCode::RenderInvalidSignalBuffer);
    CHECK(envelope_sample == 8.0);
    CHECK(envelope.status().controls_applied == 0);
    CHECK(envelope.status().controls_pending == 4);

    REQUIRE(envelope.process(1, 0, nullptr, 1, envelope_outputs));
    CHECK(envelope_sample == 0.4);
    CHECK(envelope.status().controls_applied == 4);
    CHECK(envelope.status().controls_pending == 0);
}

TEST_CASE("Max control transfer is bounded and reports overflow without mutation",
          "[max-adapter][queue]") {
    LfoAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, 1));
    for (std::size_t command = 0; command < MAX_CONTROL_QUEUE_CAPACITY; ++command)
        REQUIRE(adapter.publish_reset());

    const auto overflow = adapter.publish_reset();
    REQUIRE_FALSE(overflow);
    CHECK(overflow.error() == ErrorCode::RenderControlQueueFull);
    CHECK(adapter.status().controls_pending == MAX_CONTROL_QUEUE_CAPACITY);
    REQUIRE(adapter.process(0, 0, nullptr, 1, nullptr));

    const auto status = adapter.status();
    CHECK(status.controls_enqueued == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_applied == MAX_CONTROL_QUEUE_CAPACITY);
    CHECK(status.controls_pending == 0);
    CHECK(status.controls_rejected == 1);
    CHECK(status.last_error == ErrorCode::RenderControlQueueFull);
}

TEST_CASE("Max producer serialization admits multiple control threads without audio locking",
          "[max-adapter][queue][threading]") {
    LfoAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, 1));
    std::atomic<bool> succeeded{true};
    std::vector<std::thread> producers;
    for (int producer = 0; producer < 4; ++producer) {
        producers.emplace_back([&] {
            for (int command = 0; command < 8; ++command)
                if (!adapter.publish_reset()) succeeded.store(false, std::memory_order_relaxed);
        });
    }
    for (auto& producer : producers)
        producer.join();

    REQUIRE(succeeded.load(std::memory_order_relaxed));
    REQUIRE(adapter.process(0, 0, nullptr, 1, nullptr));
    const auto status = adapter.status();
    CHECK(status.controls_enqueued == 32);
    CHECK(status.controls_applied == 32);
    CHECK(status.controls_rejected == 0);
}

TEST_CASE("Max producers and audio owner exchange controls concurrently within the fixed bound",
          "[max-adapter][queue][threading]") {
    LfoAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, 1));

    constexpr std::uint64_t producer_count = 4;
    constexpr std::uint64_t commands_per_producer = 250;
    constexpr std::uint64_t expected_commands = producer_count * commands_per_producer;
    std::atomic<bool> producers_done{false};
    std::atomic<bool> observer_done{false};
    std::atomic<bool> causal_counts{true};
    std::atomic<bool> pending_in_bounds{true};

    std::thread status_observer([&] {
        while (!observer_done.load(std::memory_order_acquire)) {
            if (adapter.status().controls_pending > MAX_CONTROL_QUEUE_CAPACITY)
                pending_in_bounds.store(false, std::memory_order_relaxed);
            std::this_thread::yield();
        }
    });

    std::thread audio_owner([&] {
        for (;;) {
            if (!adapter.process(0, 0, nullptr, 1, nullptr)) {
                causal_counts.store(false, std::memory_order_relaxed);
                return;
            }
            const auto status = adapter.status();
            if (status.controls_applied > status.controls_enqueued)
                causal_counts.store(false, std::memory_order_relaxed);
            if (producers_done.load(std::memory_order_acquire) &&
                status.controls_applied == expected_commands)
                return;
            std::this_thread::yield();
        }
    });

    std::vector<std::thread> producers;
    for (std::uint64_t producer = 0; producer < producer_count; ++producer) {
        producers.emplace_back([&] {
            for (std::uint64_t command = 0; command < commands_per_producer; ++command) {
                while (!adapter.publish_reset())
                    std::this_thread::yield();
            }
        });
    }
    for (auto& producer : producers)
        producer.join();
    producers_done.store(true, std::memory_order_release);
    audio_owner.join();
    observer_done.store(true, std::memory_order_release);
    status_observer.join();

    const auto status = adapter.status();
    CHECK(causal_counts.load(std::memory_order_relaxed));
    CHECK(pending_in_bounds.load(std::memory_order_relaxed));
    CHECK(status.controls_enqueued == expected_commands);
    CHECK(status.controls_applied == expected_commands);
    CHECK(status.controls_pending == 0);
    CHECK(status.process_failures == 0);
}

TEST_CASE("Max ADSR adapter preserves ordered block-edge gate semantics",
          "[max-adapter][envelope]") {
    EnvelopeAdapter adapter;
    REQUIRE(adapter.configure_dsp(100.0, 4));
    REQUIRE(adapter.publish_attack(0.0));
    REQUIRE(adapter.publish_decay(0.0));
    REQUIRE(adapter.publish_sustain(0.25));
    REQUIRE(adapter.publish_release_time(0.0));
    REQUIRE(adapter.publish_trigger());

    double sample = -1.0;
    double* outputs[1]{&sample};
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.25);

    REQUIRE(adapter.publish_release());
    REQUIRE(adapter.process(1, 0, nullptr, 1, outputs));
    CHECK(sample == 0.0);
    CHECK(adapter.status().controls_applied == 6);
}

TEST_CASE("Max held-value adapter applies complete ordered vectors at block boundaries",
          "[max-adapter][sample-and-hold]") {
    SampleAndHoldAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, 3));
    REQUIRE(adapter.publish_value(0.75));
    REQUIRE(adapter.publish_value(-0.25));

    double samples[3]{8.0, 8.0, 8.0};
    double* outputs[1]{samples};
    const auto malformed = adapter.process(3, 1, nullptr, 1, outputs);
    REQUIRE_FALSE(malformed);
    CHECK(malformed.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(samples[0] == 8.0);
    CHECK(adapter.status().controls_applied == 0);

    REQUIRE(adapter.process(3, 0, nullptr, 1, outputs));
    CHECK(samples[0] == -0.25);
    CHECK(samples[1] == -0.25);
    CHECK(samples[2] == -0.25);
    CHECK(adapter.status().controls_applied == 2);

    const auto invalid = adapter.publish_value(std::numeric_limits<double>::infinity());
    REQUIRE_FALSE(invalid);
    CHECK(invalid.error() == ErrorCode::RenderInvalidParameter);

    REQUIRE(adapter.publish_reset());
    REQUIRE(adapter.process(3, 0, nullptr, 1, outputs));
    CHECK(samples[0] == 0.0);
    CHECK(samples[1] == 0.0);
    CHECK(samples[2] == 0.0);
}

TEST_CASE("Max ADSR and held-value adapters disable stale contexts after rejected rebuilds",
          "[max-adapter][dsp-rebuild]") {
    EnvelopeAdapter envelope;
    REQUIRE(envelope.configure_dsp(100.0, 1));
    REQUIRE(envelope.publish_trigger());
    REQUIRE_FALSE(envelope.configure_dsp(100.0, 0));
    CHECK_FALSE(envelope.status().configured);
    CHECK(envelope.status().sample_rate == 0.0);
    CHECK(envelope.status().maximum_frames == 0);

    double envelope_sample = 5.0;
    double* envelope_outputs[1]{&envelope_sample};
    const auto stale_envelope = envelope.process(1, 0, nullptr, 1, envelope_outputs);
    REQUIRE_FALSE(stale_envelope);
    CHECK(stale_envelope.error() == ErrorCode::RenderNotConfigured);
    CHECK(envelope_sample == 5.0);
    CHECK(envelope.status().controls_pending == 1);
    REQUIRE(envelope.configure_dsp(100.0, 1));
    REQUIRE(envelope.process(1, 0, nullptr, 1, envelope_outputs));
    CHECK(envelope.status().controls_applied == 1);

    SampleAndHoldAdapter held;
    REQUIRE(held.configure_dsp(48'000.0, 1));
    REQUIRE(held.publish_value(0.5));
    REQUIRE_FALSE(held.configure_dsp(std::numeric_limits<double>::quiet_NaN(), 1));
    CHECK_FALSE(held.status().configured);
    CHECK(held.status().sample_rate == 0.0);
    CHECK(held.status().maximum_frames == 0);

    double held_sample = 6.0;
    double* held_outputs[1]{&held_sample};
    const auto stale_held = held.process(1, 0, nullptr, 1, held_outputs);
    REQUIRE_FALSE(stale_held);
    CHECK(stale_held.error() == ErrorCode::RenderNotConfigured);
    CHECK(held_sample == 6.0);
    CHECK(held.status().controls_pending == 1);
    REQUIRE(held.configure_dsp(48'000.0, 1));
    REQUIRE(held.process(1, 0, nullptr, 1, held_outputs));
    CHECK(held_sample == 0.5);
    CHECK(held.status().controls_applied == 1);
}

TEST_CASE("Max adapter silence policy validates extent before clearing host output",
          "[max-adapter][buffers]") {
    EnvelopeAdapter adapter;
    REQUIRE(adapter.configure_dsp(48'000.0, 2));
    double output[2]{0.5, -0.5};
    double* outputs[1]{output};

    const auto wrong_count = adapter.silence_output(2, 0, outputs);
    REQUIRE_FALSE(wrong_count);
    CHECK(wrong_count.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(output[0] == 0.5);
    CHECK(output[1] == -0.5);

    const auto oversized = adapter.silence_output(3, 1, outputs);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(output[0] == 0.5);
    CHECK(output[1] == -0.5);

    REQUIRE(adapter.silence_output(2, 1, outputs));
    CHECK(output[0] == 0.0);
    CHECK(output[1] == 0.0);

    output[0] = 0.25;
    output[1] = -0.25;
    const auto invalid_current_maximum =
        sunny::max::detail::silence_generator_output(0, 2, 0, 1, outputs);
    REQUIRE_FALSE(invalid_current_maximum);
    CHECK(invalid_current_maximum.error() == ErrorCode::RenderInvalidBlockSize);
    CHECK(output[0] == 0.25);
    CHECK(output[1] == -0.25);

    const auto wrong_current_topology =
        sunny::max::detail::silence_generator_output(2, 2, 1, 1, outputs);
    REQUIRE_FALSE(wrong_current_topology);
    CHECK(wrong_current_topology.error() == ErrorCode::RenderInvalidChannelCount);
    CHECK(output[0] == 0.25);
    CHECK(output[1] == -0.25);

    REQUIRE(sunny::max::detail::silence_generator_output(2, 2, 0, 1, outputs));
    CHECK(output[0] == 0.0);
    CHECK(output[1] == 0.0);
}
