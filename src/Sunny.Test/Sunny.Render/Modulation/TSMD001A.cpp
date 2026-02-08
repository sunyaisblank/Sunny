/**
 * @file TSMD001A.cpp
 * @brief Unit tests for RDMD001A (Modulation)
 *
 * Component: TSMD001A
 * Tests: RDMD001A
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Modulation/RDMD001A.h"

#include <cmath>

using namespace Sunny::Render;
using Catch::Matchers::WithinAbs;

TEST_CASE("RDMD001A: LFO sine waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Sine);
    lfo.set_frequency(1.0);  // 1 Hz

    SECTION("Starts at zero") {
        double value = lfo.process(1000.0);  // First sample
        REQUIRE_THAT(value, WithinAbs(0.0, 0.01));
    }

    SECTION("Reaches peak at quarter cycle") {
        double sample_rate = 1000.0;
        // Advance to 1/4 cycle
        for (int i = 0; i < 250; ++i) {
            lfo.process(sample_rate);
        }
        REQUIRE_THAT(lfo.value(), WithinAbs(1.0, 0.05));
    }

    SECTION("Output range is [-1, 1]") {
        double sample_rate = 1000.0;
        double min_val = 0.0;
        double max_val = 0.0;

        for (int i = 0; i < 1000; ++i) {
            double val = lfo.process(sample_rate);
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        REQUIRE(min_val >= -1.0);
        REQUIRE(max_val <= 1.0);
        REQUIRE_THAT(min_val, WithinAbs(-1.0, 0.05));
        REQUIRE_THAT(max_val, WithinAbs(1.0, 0.05));
    }
}

TEST_CASE("RDMD001A: LFO square waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Square);
    lfo.set_frequency(1.0);

    SECTION("Only outputs +1 or -1") {
        double sample_rate = 1000.0;

        for (int i = 0; i < 1000; ++i) {
            double val = lfo.process(sample_rate);
            REQUIRE((val == 1.0 || val == -1.0));
        }
    }
}

TEST_CASE("RDMD001A: LFO triangle waveform", "[modulation][render]") {
    Lfo lfo;
    lfo.set_waveform(LfoWaveform::Triangle);
    lfo.set_frequency(1.0);

    SECTION("Output range is [-1, 1]") {
        double sample_rate = 1000.0;
        double min_val = 0.0;
        double max_val = 0.0;

        for (int i = 0; i < 1000; ++i) {
            double val = lfo.process(sample_rate);
            min_val = std::min(min_val, val);
            max_val = std::max(max_val, val);
        }

        REQUIRE(min_val >= -1.0);
        REQUIRE(max_val <= 1.0);
    }
}

TEST_CASE("RDMD001A: Envelope ADSR", "[modulation][render]") {
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
            env.process(sample_rate);
        }
        REQUIRE(env.value() > 0.9);
    }

    SECTION("Release returns to zero") {
        env.trigger();
        // Go through attack/decay
        for (int i = 0; i < 1000; ++i) {
            env.process(sample_rate);
        }

        env.release();
        // Process release
        for (int i = 0; i < 1000; ++i) {
            env.process(sample_rate);
        }

        REQUIRE(env.state() == EnvelopeState::Idle);
        REQUIRE(env.value() == 0.0);
    }

    SECTION("Reset immediately stops") {
        env.trigger();
        for (int i = 0; i < 100; ++i) {
            env.process(sample_rate);
        }

        env.reset();
        REQUIRE(env.state() == EnvelopeState::Idle);
        REQUIRE(env.value() == 0.0);
    }
}

TEST_CASE("RDMD001A: Sample and Hold", "[modulation][render]") {
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
