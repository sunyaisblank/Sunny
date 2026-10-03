/**
 * @file modulation_test.cpp
 * @brief Modulation unit tests
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <sunny/render/modulation.hpp>
#include <vector>

using namespace sunny::render;
using Catch::Matchers::WithinAbs;

TEST_CASE("LFO sine waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Sine);
    lfo.set_frequency(1.0); // 1 Hz

    SECTION("Starts at zero") {
        double value = *lfo.process(1000.0); // First sample
        REQUIRE_THAT(value, WithinAbs(0.0, 0.01));
    }

    SECTION("Reaches peak at quarter cycle") {
        double sample_rate = 1000.0;
        // Advance to 1/4 cycle
        for (int i = 0; i < 250; ++i) {
            (void)lfo.process(sample_rate);
        }
        REQUIRE_THAT(lfo.value(), WithinAbs(1.0, 0.05));
    }

    SECTION("Output range is [-1, 1]") {
        double sample_rate = 1000.0;
        double min_val = 0.0;
        double max_val = 0.0;

        for (int i = 0; i < 1000; ++i) {
            double val = *lfo.process(sample_rate);
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        REQUIRE(min_val >= -1.0);
        REQUIRE(max_val <= 1.0);
        REQUIRE_THAT(min_val, WithinAbs(-1.0, 0.05));
        REQUIRE_THAT(max_val, WithinAbs(1.0, 0.05));
    }

    SECTION("Reset restores initial state: phase and value both zero") {
        for (int i = 0; i < 100; ++i) {
            (void)lfo.process(1000.0);
        }
        lfo.reset();
        REQUIRE(lfo.value() == 0.0);
    }
}

TEST_CASE("LFO square waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Square);
    lfo.set_frequency(1.0);

    SECTION("Only outputs +1 or -1") {
        double sample_rate = 1000.0;

        for (int i = 0; i < 1000; ++i) {
            double val = *lfo.process(sample_rate);
            REQUIRE((val == 1.0 || val == -1.0));
        }
    }
}

TEST_CASE("LFO triangle waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Triangle);
    lfo.set_frequency(1.0);

    SECTION("Output range is [-1, 1]") {
        double sample_rate = 1000.0;
        double min_val = 0.0;
        double max_val = 0.0;

        for (int i = 0; i < 1000; ++i) {
            double val = *lfo.process(sample_rate);
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        REQUIRE(min_val >= -1.0);
        REQUIRE(max_val <= 1.0);
    }
}

TEST_CASE("Envelope ADSR", "[modulation][render]") {
    Envelope env;
    env.set_attack(0.01);
    env.set_decay(0.01);
    env.set_sustain(0.5);
    env.set_release(0.01);

    double sample_rate = 44100.0;

    SECTION("Starts idle at zero") {
        REQUIRE(env.state() == EnvelopeState::Idle);
        REQUIRE(env.value() == 0.0);
    }

    SECTION("Trigger starts attack") {
        env.trigger();
        REQUIRE(env.state() == EnvelopeState::Attack);
    }

    SECTION("Attack reaches peak") {
        env.trigger();
        for (int i = 0; i < 500; ++i) {
            (void)env.process(sample_rate);
        }
        REQUIRE(env.value() > 0.9);
    }

    SECTION("Release returns to zero") {
        env.trigger();
        // Go through attack/decay
        for (int i = 0; i < 1000; ++i) {
            (void)env.process(sample_rate);
        }

        env.release();
        // Process release
        for (int i = 0; i < 1000; ++i) {
            (void)env.process(sample_rate);
        }

        REQUIRE(env.state() == EnvelopeState::Idle);
        REQUIRE(env.value() == 0.0);
    }

    SECTION("Reset immediately stops") {
        env.trigger();
        for (int i = 0; i < 100; ++i) {
            (void)env.process(sample_rate);
        }

        env.reset();
        REQUIRE(env.state() == EnvelopeState::Idle);
        REQUIRE(env.value() == 0.0);
    }
}

TEST_CASE("Sample and Hold", "[modulation][render]") {
    SampleAndHold sah;

    SECTION("Initial value is zero") {
        REQUIRE(sah.value() == 0.0);
    }

    SECTION("Trigger captures value") {
        sah.trigger(0.75);
        REQUIRE(sah.value() == 0.75);
    }

    SECTION("Value held until next trigger") {
        sah.trigger(0.5);
        REQUIRE(sah.value() == 0.5);

        // Value doesn't change without trigger
        REQUIRE(sah.value() == 0.5);

        sah.trigger(0.8);
        REQUIRE(sah.value() == 0.8);
    }

    SECTION("Reset clears value") {
        sah.trigger(1.0);
        sah.reset();
        REQUIRE(sah.value() == 0.0);
    }
}

TEST_CASE("Sample and Hold fills a validated generator vector without changing the hold",
          "[modulation][render][block]") {
    STATIC_CHECK(SampleAndHold::signal_input_count == 0);
    STATIC_CHECK(SampleAndHold::signal_output_count == 1);

    const auto context = SignalBlockContext::create(48'000.0, 3);
    REQUIRE(context);
    SampleAndHold sample_and_hold;
    REQUIRE(sample_and_hold.trigger(-0.25));

    double samples[3]{9.0, 9.0, 9.0};
    double* outputs[1]{samples};
    REQUIRE(sample_and_hold.process_block(*context, 3, 0, nullptr, 1, outputs));
    CHECK(samples[0] == -0.25);
    CHECK(samples[1] == -0.25);
    CHECK(samples[2] == -0.25);
    CHECK(sample_and_hold.value() == -0.25);

    const auto wrong_topology = sample_and_hold.process_block(*context, 3, 1, nullptr, 1, outputs);
    REQUIRE_FALSE(wrong_topology);
    CHECK(wrong_topology.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
    CHECK(samples[0] == -0.25);

    const auto oversized = sample_and_hold.process_block(*context, 4, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(sample_and_hold.value() == -0.25);

    REQUIRE(sample_and_hold.process_block(*context, 0, 0, nullptr, 1, nullptr));
}

TEST_CASE("Modulation rejects non-finite and out-of-domain input", "[modulation][render]") {
    Lfo lfo;
    REQUIRE(lfo.set_phase(0.25));
    const auto invalid_waveform = lfo.set_waveform(static_cast<LfoWaveform>(99));
    REQUIRE_FALSE(invalid_waveform);
    CHECK(invalid_waveform.error() == sunny::core::ErrorCode::RenderInvalidParameter);
    REQUIRE(lfo.process(4.0));
    CHECK_THAT(lfo.value(), WithinAbs(1.0, 1e-15));

    lfo.reset();
    REQUIRE_FALSE(lfo.set_frequency(-1.0));
    REQUIRE_FALSE(lfo.set_frequency(std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE(lfo.set_phase(1.0));
    REQUIRE_FALSE(lfo.process(0.0));

    Envelope envelope;
    REQUIRE_FALSE(envelope.set_attack(-0.1));
    REQUIRE_FALSE(envelope.set_sustain(1.1));
    REQUIRE_FALSE(envelope.process(std::numeric_limits<double>::infinity()));

    SampleAndHold sample_and_hold;
    REQUIRE_FALSE(sample_and_hold.trigger(1.01));
    REQUIRE(sample_and_hold.value() == 0.0);
}

TEST_CASE("Signal block context closes sample-rate and maximum-vector setup",
          "[modulation][render][block]") {
    const auto context = SignalBlockContext::create(48'000.0, 64);
    REQUIRE(context);
    CHECK(context->sample_rate() == 48'000.0);
    CHECK(context->maximum_frames() == 64);

    const auto zero_rate = SignalBlockContext::create(0.0, 64);
    REQUIRE_FALSE(zero_rate);
    CHECK(zero_rate.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);

    const auto infinite_rate =
        SignalBlockContext::create(std::numeric_limits<double>::infinity(), 64);
    REQUIRE_FALSE(infinite_rate);
    CHECK(infinite_rate.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);

    const auto empty_maximum = SignalBlockContext::create(48'000.0, 0);
    REQUIRE_FALSE(empty_maximum);
    CHECK(empty_maximum.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);

    const auto negative_maximum = SignalBlockContext::create(48'000.0, -1);
    REQUIRE_FALSE(negative_maximum);
    CHECK(negative_maximum.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);

    const auto negative_actual = context->validate_frame_count(-1);
    REQUIRE_FALSE(negative_actual);
    CHECK(negative_actual.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    REQUIRE(context->validate_frame_count(64) == 64);
    REQUIRE_FALSE(context->validate_frame_count(65));
}

TEST_CASE("Host-shaped modulation validates count and pointer before span construction",
          "[modulation][render][block]") {
    const auto context = SignalBlockContext::create(4.0, 2);
    REQUIRE(context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(1.0));

    const auto negative = lfo.process_block(*context, -1, nullptr);
    REQUIRE_FALSE(negative);
    CHECK(negative.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(lfo.value() == 0.0);

    const auto oversized = lfo.process_block(*context, 3, nullptr);
    REQUIRE_FALSE(oversized);
    CHECK(oversized.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(lfo.value() == 0.0);

    const auto missing = lfo.process_block(*context, 1, nullptr);
    REQUIRE_FALSE(missing);
    CHECK(missing.error() == sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    CHECK(lfo.value() == 0.0);

    REQUIRE(lfo.process_block(*context, 0, nullptr));
    CHECK(lfo.value() == 0.0);

    double lfo_output[2]{99.0, 99.0};
    REQUIRE(lfo.process_block(*context, 2, lfo_output));
    CHECK(lfo_output[0] == 0.0);
    CHECK(lfo_output[1] == 1.0);

    Envelope envelope;
    envelope.trigger();
    const auto missing_envelope = envelope.process_block(*context, 1, nullptr);
    REQUIRE_FALSE(missing_envelope);
    CHECK(missing_envelope.error() == sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    CHECK(envelope.value() == 0.0);
    CHECK(envelope.state() == EnvelopeState::Attack);

    double envelope_output[1]{-1.0};
    REQUIRE(envelope.process_block(*context, 1, envelope_output));
    CHECK(envelope_output[0] == 1.0);
}

TEST_CASE("Max-shaped mono output topology is validated before array dereference",
          "[modulation][render][block]") {
    const auto context = SignalBlockContext::create(4.0, 2);
    REQUIRE(context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(1.0));

    const auto negative_frames = lfo.process_block(*context, -1, -1, nullptr);
    REQUIRE_FALSE(negative_frames);
    CHECK(negative_frames.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(lfo.value() == 0.0);

    for (const std::int64_t invalid_count : {-1, 0, 2}) {
        const auto invalid = lfo.process_block(*context, 1, invalid_count, nullptr);
        REQUIRE_FALSE(invalid);
        CHECK(invalid.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
        CHECK(lfo.value() == 0.0);
    }

    const auto missing_array = lfo.process_block(*context, 1, 1, nullptr);
    REQUIRE_FALSE(missing_array);
    CHECK(missing_array.error() == sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    CHECK(lfo.value() == 0.0);

    double* missing_channels[1]{nullptr};
    const auto missing_channel = lfo.process_block(*context, 1, 1, missing_channels);
    REQUIRE_FALSE(missing_channel);
    CHECK(missing_channel.error() == sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    CHECK(lfo.value() == 0.0);

    REQUIRE(lfo.process_block(*context, 0, 1, nullptr));
    CHECK(lfo.value() == 0.0);

    double samples[2]{99.0, 99.0};
    double* outputs[1]{samples};
    REQUIRE(lfo.process_block(*context, 2, 1, outputs));
    CHECK(samples[0] == 0.0);
    CHECK(samples[1] == 1.0);

    Envelope envelope;
    envelope.trigger();
    const auto invalid_envelope = envelope.process_block(*context, 1, 0, nullptr);
    REQUIRE_FALSE(invalid_envelope);
    CHECK(invalid_envelope.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
    CHECK(envelope.value() == 0.0);
    CHECK(envelope.state() == EnvelopeState::Attack);

    double envelope_sample[1]{-1.0};
    double* envelope_outputs[1]{envelope_sample};
    REQUIRE(envelope.process_block(*context, 1, 1, envelope_outputs));
    CHECK(envelope_sample[0] == 1.0);
}

TEST_CASE("Max-shaped generator topology requires zero inputs and one output",
          "[modulation][render][block]") {
    STATIC_CHECK(Lfo::signal_input_count == 0);
    STATIC_CHECK(Lfo::signal_output_count == 1);
    STATIC_CHECK(Envelope::signal_input_count == 0);
    STATIC_CHECK(Envelope::signal_output_count == 1);
    STATIC_CHECK(SampleAndHold::signal_input_count == 0);
    STATIC_CHECK(SampleAndHold::signal_output_count == 1);

    const auto context = SignalBlockContext::create(4.0, 2);
    REQUIRE(context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(1.0));
    double unchanged[1]{41.0};
    double* outputs[1]{unchanged};

    const auto invalid_frames = lfo.process_block(*context, -1, 1, nullptr, 0, nullptr);
    REQUIRE_FALSE(invalid_frames);
    CHECK(invalid_frames.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(lfo.value() == 0.0);

    for (const std::int64_t invalid_inputs : {-1, 1, 2}) {
        const auto invalid = lfo.process_block(*context, 1, invalid_inputs, nullptr, 0, nullptr);
        REQUIRE_FALSE(invalid);
        CHECK(invalid.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
        CHECK(lfo.value() == 0.0);
        CHECK(unchanged[0] == 41.0);
    }

    const auto invalid_output = lfo.process_block(*context, 1, 0, nullptr, 0, nullptr);
    REQUIRE_FALSE(invalid_output);
    CHECK(invalid_output.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
    CHECK(lfo.value() == 0.0);

    const auto missing_output = lfo.process_block(*context, 1, 0, nullptr, 1, nullptr);
    REQUIRE_FALSE(missing_output);
    CHECK(missing_output.error() == sunny::core::ErrorCode::RenderInvalidSignalBuffer);
    CHECK(lfo.value() == 0.0);

    double* ignored_inputs[1]{nullptr};
    REQUIRE(lfo.process_block(*context, 0, 0, ignored_inputs, 1, nullptr));
    REQUIRE(lfo.process_block(*context, 1, 0, ignored_inputs, 1, outputs));
    CHECK(unchanged[0] == 0.0);

    Envelope envelope;
    envelope.trigger();
    const auto invalid_envelope = envelope.process_block(*context, 1, 1, nullptr, 1, outputs);
    REQUIRE_FALSE(invalid_envelope);
    CHECK(invalid_envelope.error() == sunny::core::ErrorCode::RenderInvalidChannelCount);
    CHECK(envelope.value() == 0.0);
    CHECK(envelope.state() == EnvelopeState::Attack);

    double envelope_sample[1]{-1.0};
    double* envelope_outputs[1]{envelope_sample};
    REQUIRE(envelope.process_block(*context, 1, 0, nullptr, 1, envelope_outputs));
    CHECK(envelope_sample[0] == 1.0);
}

TEST_CASE("A new signal context retains modulation state and applies the new rate",
          "[modulation][render][block]") {
    const auto first_context = SignalBlockContext::create(4.0, 1);
    const auto second_context = SignalBlockContext::create(8.0, 1);
    REQUIRE(first_context);
    REQUIRE(second_context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(1.0));
    double lfo_output[1]{};
    REQUIRE(lfo.process_block(*first_context, 1, lfo_output));
    CHECK(lfo_output[0] == 0.0);
    REQUIRE(lfo.process_block(*second_context, 1, lfo_output));
    CHECK(lfo_output[0] == 1.0);

    Envelope envelope;
    REQUIRE(envelope.set_attack(1.0));
    envelope.trigger();
    double envelope_output[1]{};
    REQUIRE(envelope.process_block(*first_context, 1, envelope_output));
    CHECK(envelope_output[0] == 0.25);
    REQUIRE(envelope.process_block(*second_context, 1, envelope_output));
    CHECK(envelope_output[0] == 0.375);
    CHECK(envelope.state() == EnvelopeState::Attack);
}

TEST_CASE("LFO context compatibility is decidable before a DSP rebuild is committed",
          "[modulation][render][block]") {
    const auto compatible_context = SignalBlockContext::create(8.0, 1);
    const auto incompatible_context = SignalBlockContext::create(2.0, 1);
    REQUIRE(compatible_context);
    REQUIRE(incompatible_context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(4.0));
    REQUIRE(lfo.validate_context(*compatible_context));

    const auto rejected = lfo.validate_context(*incompatible_context);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);
    CHECK(lfo.value() == 0.0);

    double unchanged[1]{73.0};
    double* outputs[1]{unchanged};
    const auto runtime_guard = lfo.process_block(*incompatible_context, 1, 0, nullptr, 1, outputs);
    REQUIRE_FALSE(runtime_guard);
    CHECK(runtime_guard.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);
    CHECK(lfo.value() == 0.0);
    CHECK(unchanged[0] == 73.0);
}

TEST_CASE("LFO frequency publication is atomic against the active DSP context",
          "[modulation][render][block][control]") {
    const auto active_context = SignalBlockContext::create(8.0, 1);
    const auto narrower_context = SignalBlockContext::create(2.0, 1);
    REQUIRE(active_context);
    REQUIRE(narrower_context);

    Lfo lfo;
    REQUIRE(lfo.set_frequency(2.0, *active_context));
    double first[1]{};
    REQUIRE(lfo.process_block(*active_context, first));
    CHECK(first[0] == 0.0);
    const Lfo before_rejections = lfo;

    const auto invalid_parameter =
        lfo.set_frequency(std::numeric_limits<double>::quiet_NaN(), *active_context);
    REQUIRE_FALSE(invalid_parameter);
    CHECK(invalid_parameter.error() == sunny::core::ErrorCode::RenderInvalidParameter);
    const auto incompatible = lfo.set_frequency(9.0, *active_context);
    REQUIRE_FALSE(incompatible);
    CHECK(incompatible.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);
    REQUIRE(lfo.validate_context(*narrower_context));

    Lfo baseline = before_rejections;
    double observed[1]{};
    double expected[1]{};
    REQUIRE(lfo.process_block(*active_context, observed));
    REQUIRE(baseline.process_block(*active_context, expected));
    CHECK(observed[0] == expected[0]);
    CHECK(lfo.value() == baseline.value());

    REQUIRE(lfo.set_frequency(8.0, *active_context));
    const auto now_incompatible = lfo.validate_context(*narrower_context);
    REQUIRE_FALSE(now_incompatible);
    CHECK(now_incompatible.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);
}

TEST_CASE("LFO block processing is scalar-equivalent and validates before mutation",
          "[modulation][render][block]") {
    Lfo block_lfo;
    Lfo scalar_lfo;
    for (auto* lfo : {&block_lfo, &scalar_lfo}) {
        lfo->set_waveform(LfoWaveform::Random);
        lfo->set_seed(42);
        REQUIRE(lfo->set_frequency(4.0));
    }

    const auto context = SignalBlockContext::create(16.0, 8);
    REQUIRE(context);
    std::vector<double> block(8, 99.0);
    REQUIRE(block_lfo.process_block(*context, block));
    for (double value : block)
        CHECK(value == *scalar_lfo.process(16.0));
    CHECK(block_lfo.value() == scalar_lfo.value());

    Lfo rejected;
    REQUIRE(rejected.set_frequency(101.0));
    const auto narrow_context = SignalBlockContext::create(100.0, 4);
    REQUIRE(narrow_context);
    std::vector<double> unchanged(4, 23.0);
    const auto invalid_rate = rejected.process_block(*narrow_context, unchanged);
    REQUIRE_FALSE(invalid_rate);
    CHECK(invalid_rate.error() == sunny::core::ErrorCode::RenderInvalidSampleRate);
    CHECK(unchanged == std::vector<double>(4, 23.0));
    CHECK(rejected.value() == 0.0);

    const auto short_context = SignalBlockContext::create(200.0, 3);
    REQUIRE(short_context);
    std::vector<double> oversized(4, 31.0);
    const auto invalid_size = rejected.process_block(*short_context, oversized);
    REQUIRE_FALSE(invalid_size);
    CHECK(invalid_size.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(oversized == std::vector<double>(4, 31.0));
    CHECK(rejected.value() == 0.0);

    std::span<double> empty;
    const auto empty_context = SignalBlockContext::create(200.0, 4);
    REQUIRE(empty_context);
    REQUIRE(rejected.process_block(*empty_context, empty));
    CHECK(rejected.value() == 0.0);
}

TEST_CASE("Envelope block processing is scalar-equivalent and all-or-none",
          "[modulation][render][block]") {
    Envelope block_envelope;
    Envelope scalar_envelope;
    for (auto* envelope : {&block_envelope, &scalar_envelope}) {
        REQUIRE(envelope->set_attack(0.25));
        REQUIRE(envelope->set_decay(0.25));
        REQUIRE(envelope->set_sustain(0.5));
        REQUIRE(envelope->set_release(0.5));
        envelope->trigger();
    }

    const auto context = SignalBlockContext::create(16.0, 8);
    REQUIRE(context);
    std::vector<double> block(8, -1.0);
    REQUIRE(block_envelope.process_block(*context, block));
    for (double value : block)
        CHECK(value == *scalar_envelope.process(16.0));
    CHECK(block_envelope.value() == scalar_envelope.value());
    CHECK(block_envelope.state() == scalar_envelope.state());

    const auto short_context = SignalBlockContext::create(16.0, 3);
    REQUIRE(short_context);
    const auto before_value = block_envelope.value();
    const auto before_state = block_envelope.state();
    std::vector<double> oversized(4, 17.0);
    const auto rejected = block_envelope.process_block(*short_context, oversized);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == sunny::core::ErrorCode::RenderInvalidBlockSize);
    CHECK(oversized == std::vector<double>(4, 17.0));
    CHECK(block_envelope.value() == before_value);
    CHECK(block_envelope.state() == before_state);
}

TEST_CASE("Random LFO is reproducible by seed and reset", "[modulation][render]") {
    Lfo first;
    Lfo second;
    first.set_waveform(LfoWaveform::Random);
    second.set_waveform(LfoWaveform::Random);
    first.set_seed(42);
    second.set_seed(42);
    REQUIRE(first.set_frequency(1.0));
    REQUIRE(second.set_frequency(1.0));

    std::vector<double> first_pass;
    for (int sample = 0; sample < 4; ++sample) {
        first_pass.push_back(*first.process(1.0));
        REQUIRE(first_pass.back() == *second.process(1.0));
    }
    REQUIRE_THAT(first_pass[0], WithinAbs(-0.2509197712063137, 1e-15));
    REQUIRE_THAT(first_pass[1], WithinAbs(0.5930859687722023, 1e-15));
    REQUIRE_THAT(first_pass[2], WithinAbs(0.9014286235676678, 1e-15));
    REQUIRE_THAT(first_pass[3], WithinAbs(-0.6331304245705554, 1e-15));

    first.reset();
    for (double expected : first_pass)
        REQUIRE(*first.process(1.0) == expected);
}

TEST_CASE("Envelope release duration is measured from release level", "[modulation][render]") {
    Envelope envelope;
    REQUIRE(envelope.set_attack(1.0));
    REQUIRE(envelope.set_release(0.5));
    envelope.trigger();
    for (int sample = 0; sample < 25; ++sample)
        (void)envelope.process(100.0);
    REQUIRE_THAT(envelope.value(), WithinAbs(0.25, 1e-12));

    envelope.release();
    for (int sample = 0; sample < 49; ++sample)
        (void)envelope.process(100.0);
    REQUIRE(envelope.value() > 0.0);
    (void)envelope.process(100.0);
    REQUIRE(envelope.state() == EnvelopeState::Idle);
    REQUIRE(envelope.value() == 0.0);
}

namespace {

// Process until the envelope leaves `stage`, returning the number of samples
// emitted in that stage, the last of which is included. Gives up after limit.
std::size_t samples_in_stage(Envelope& envelope,
                             EnvelopeState stage,
                             double sample_rate,
                             std::vector<double>& values,
                             std::size_t limit = 1'000'000) {
    values.clear();
    while (envelope.state() == stage && values.size() < limit)
        values.push_back(*envelope.process(sample_rate));
    return values.size();
}

} // namespace

TEST_CASE("Envelope stages last exactly their duration in samples", "[modulation][render]") {
    // Duration times sample rate, rounded up: 0.01 s is 480 samples at
    // 48 kHz and 441 at 44.1 kHz; 0.02 s at 22.05 kHz is 441; 0.01 s at
    // 22.05 kHz is 220.5, so the stage ends on sample 221.
    struct Case {
        double sample_rate;
        double seconds;
        std::size_t samples;
    };
    for (const auto& c : {Case{48000.0, 0.01, 480},
                          Case{44100.0, 0.01, 441},
                          Case{22050.0, 0.02, 441},
                          Case{22050.0, 0.01, 221},
                          Case{96000.0, 0.3, 28800}}) {
        INFO("rate " << c.sample_rate << " seconds " << c.seconds);
        Envelope envelope;
        REQUIRE(envelope.set_attack(c.seconds));
        REQUIRE(envelope.set_decay(c.seconds));
        REQUIRE(envelope.set_sustain(0.25));
        REQUIRE(envelope.set_release(c.seconds));
        std::vector<double> values;

        envelope.trigger();
        REQUIRE(samples_in_stage(envelope, EnvelopeState::Attack, c.sample_rate, values) ==
                c.samples);
        CHECK(values.back() == 1.0);
        CHECK(values[values.size() - 2] < 1.0);
        // Sample k of the attack lies k / (seconds * rate) of the way up.
        const double length = c.seconds * c.sample_rate;
        CHECK_THAT(values[0], WithinAbs(1.0 / length, 1e-12));
        CHECK_THAT(values[values.size() / 2],
                   WithinAbs(static_cast<double>(values.size() / 2 + 1) / length, 1e-12));

        REQUIRE(envelope.state() == EnvelopeState::Decay);
        REQUIRE(samples_in_stage(envelope, EnvelopeState::Decay, c.sample_rate, values) ==
                c.samples);
        CHECK(values.back() == 0.25);
        CHECK(values[values.size() - 2] > 0.25);
        CHECK(envelope.state() == EnvelopeState::Sustain);

        envelope.release();
        REQUIRE(samples_in_stage(envelope, EnvelopeState::Release, c.sample_rate, values) ==
                c.samples);
        CHECK(values.back() == 0.0);
        CHECK(values[values.size() - 2] > 0.0);
        CHECK(envelope.state() == EnvelopeState::Idle);
    }
}

TEST_CASE("Envelope with zero attack emits the peak", "[modulation][render]") {
    Envelope envelope;
    REQUIRE(envelope.set_attack(0.0));
    REQUIRE(envelope.set_decay(0.01));
    REQUIRE(envelope.set_sustain(0.5));
    envelope.trigger();
    CHECK(*envelope.process(48000.0) == 1.0);
    CHECK(envelope.state() == EnvelopeState::Decay);
    // The decay then starts from the peak: 0.5 over 480 samples.
    CHECK_THAT(*envelope.process(48000.0), WithinAbs(1.0 - 0.5 / 480.0, 1e-12));
}

TEST_CASE("Envelope retrigger restarts the attack from the current level", "[modulation][render]") {
    Envelope envelope;
    REQUIRE(envelope.set_attack(0.01));
    envelope.trigger();
    std::vector<double> values;
    for (int index = 0; index < 120; ++index)
        values.push_back(*envelope.process(48000.0));
    CHECK_THAT(values.back(), WithinAbs(0.25, 1e-12));
    envelope.trigger();
    std::size_t samples = samples_in_stage(envelope, EnvelopeState::Attack, 48000.0, values);
    CHECK(samples == 480);
    CHECK_THAT(values[0], WithinAbs(0.25 + 0.75 / 480.0, 1e-12));
    CHECK(values.back() == 1.0);
}
