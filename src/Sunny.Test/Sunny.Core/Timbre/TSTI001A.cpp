/**
 * @file TSTI001A.cpp
 * @brief Unit tests for Timbre IR foundation types (TITP001A)
 *
 * Component: TSTI001A
 * Domain: TS (Test) | Category: TI (Timbre IR)
 *
 * Tests: TITP001A
 * Coverage: Id types, enumerations, Waveform, Filter, Envelope, Oscillator,
 *           modulation types, effect parameter types, semantic descriptors
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Timbre/TITP001A.h"

using namespace Sunny::Core;

// =============================================================================
// Identifier types
// =============================================================================

TEST_CASE("TITP001A: TimbreProfileId equality and ordering", "[timbre-ir][types]") {
    TimbreProfileId a{1};
    TimbreProfileId b{1};
    TimbreProfileId c{2};

    CHECK(a == b);
    CHECK(a != c);
    CHECK(a < c);
}

TEST_CASE("TITP001A: EffectId and TimbrePresetId are distinct", "[timbre-ir][types]") {
    EffectId e{42};
    TimbrePresetId p{42};
    // These are different types — no cross-comparison possible at compile time
    CHECK(e.value == p.value);
}

// =============================================================================
// Waveform
// =============================================================================

TEST_CASE("TITP001A: Waveform default is Sine", "[timbre-ir][types]") {
    Waveform w;
    CHECK(w.type == WaveformType::Sine);
    CHECK(w.custom_harmonics.empty());
}

TEST_CASE("TITP001A: Waveform SuperSaw configuration", "[timbre-ir][types]") {
    Waveform w;
    w.type = WaveformType::SuperSaw;
    w.super_saw_detune = 15.0f;
    w.super_saw_voices = 7;

    CHECK(w.type == WaveformType::SuperSaw);
    CHECK(w.super_saw_detune == 15.0f);
    CHECK(w.super_saw_voices == 7);
}

TEST_CASE("TITP001A: Waveform Custom with harmonics", "[timbre-ir][types]") {
    Waveform w;
    w.type = WaveformType::Custom;
    w.custom_harmonics = {
        {1, 1.0f, 0.0f},
        {2, 0.5f, 0.0f},
        {3, 0.33f, 0.0f}
    };

    CHECK(w.custom_harmonics.size() == 3);
    CHECK(w.custom_harmonics[0].partial_number == 1);
    CHECK(w.custom_harmonics[1].amplitude == Catch::Approx(0.5f));
}

// =============================================================================
// Filter
// =============================================================================

TEST_CASE("TITP001A: Filter default values", "[timbre-ir][types]") {
    Filter f;
    CHECK(f.config.category == FilterCategory::LowPass);
    CHECK(f.config.slope == FilterSlope::Pole2);
    CHECK(f.cutoff == Catch::Approx(1000.0f));
    CHECK(f.resonance == Catch::Approx(0.0f));
    CHECK(f.drive == Catch::Approx(0.0f));
    CHECK(!f.envelope.has_value());
}

TEST_CASE("TITP001A: Filter with envelope modulation", "[timbre-ir][types]") {
    Filter f;
    f.config.category = FilterCategory::LadderLP;
    f.cutoff = 800.0f;
    f.resonance = 0.6f;
    f.envelope = Envelope{
        .stages = {
            {10.0f, 1.0f, EnvelopeCurve::Exponential, 2.0f},
            {200.0f, 0.4f, EnvelopeCurve::Linear},
            {500.0f, 0.0f, EnvelopeCurve::Exponential, 1.5f}
        },
        .velocity_sensitivity = 0.3f
    };
    f.envelope_depth = 0.7f;

    CHECK(f.envelope->stages.size() == 3);
    CHECK(f.envelope_depth == Catch::Approx(0.7f));
}

// =============================================================================
// Envelope
// =============================================================================

TEST_CASE("TITP001A: ADSR envelope construction", "[timbre-ir][types]") {
    Envelope adsr;
    adsr.stages = {
        {20.0f, 1.0f, EnvelopeCurve::Linear},       // Attack
        {100.0f, 0.7f, EnvelopeCurve::Exponential},  // Decay
        {300.0f, 0.0f, EnvelopeCurve::Exponential}   // Release
    };
    adsr.velocity_sensitivity = 0.5f;

    CHECK(adsr.stages.size() == 3);
    CHECK(adsr.stages[0].target_level == Catch::Approx(1.0f));
    CHECK(adsr.stages[1].target_level == Catch::Approx(0.7f));
    CHECK(adsr.stages[2].target_level == Catch::Approx(0.0f));
}

TEST_CASE("TITP001A: Envelope with loop", "[timbre-ir][types]") {
    Envelope env;
    env.stages = {
        {50.0f, 1.0f, EnvelopeCurve::Linear},
        {100.0f, 0.5f, EnvelopeCurve::SCurve},
        {100.0f, 1.0f, EnvelopeCurve::SCurve},
        {200.0f, 0.0f, EnvelopeCurve::Exponential}
    };
    env.loop = EnvelopeLoop{1, 2, 0};  // loop stages 1-2 indefinitely

    CHECK(env.loop.has_value());
    CHECK(env.loop->start_stage == 1);
    CHECK(env.loop->end_stage == 2);
    CHECK(env.loop->count == 0);
}

// =============================================================================
// Oscillator
// =============================================================================

TEST_CASE("TITP001A: Oscillator default state", "[timbre-ir][types]") {
    Oscillator osc;
    CHECK(osc.waveform.type == WaveformType::Sine);
    CHECK(osc.tune_semitones == 0);
    CHECK(osc.tune_cents == Catch::Approx(0.0f));
    CHECK(osc.phase == Catch::Approx(0.0f));
    CHECK(osc.pulse_width == Catch::Approx(0.5f));
    CHECK(osc.level == Catch::Approx(1.0f));
}

TEST_CASE("TITP001A: Oscillator detuned configuration", "[timbre-ir][types]") {
    Oscillator osc;
    osc.waveform.type = WaveformType::Saw;
    osc.tune_semitones = -12;
    osc.tune_cents = 5.0f;
    osc.level = 0.8f;

    CHECK(osc.tune_semitones == -12);
    CHECK(osc.tune_cents == Catch::Approx(5.0f));
}

// =============================================================================
// FM Synthesis types
// =============================================================================

TEST_CASE("TITP001A: FMAlgorithm preset vs custom", "[timbre-ir][types]") {
    SECTION("Preset algorithm") {
        FMAlgorithm algo;
        algo.use_preset = true;
        algo.preset_number = 5;

        CHECK(algo.use_preset);
        CHECK(algo.preset_number == 5);
        CHECK(algo.custom_routing.empty());
    }

    SECTION("Custom routing") {
        FMAlgorithm algo;
        algo.use_preset = false;
        algo.custom_routing = {
            {1, 0, 0.8f},  // op1 modulates op0
            {2, 0, 0.3f}   // op2 modulates op0
        };

        CHECK(!algo.use_preset);
        CHECK(algo.custom_routing.size() == 2);
    }
}

TEST_CASE("TITP001A: FMOperator with fixed frequency", "[timbre-ir][types]") {
    FMOperator op;
    op.fixed_frequency = 440.0f;
    op.level = 0.6f;
    op.envelope.stages = {{0.0f, 1.0f, EnvelopeCurve::Step}};

    CHECK(op.fixed_frequency.has_value());
    CHECK(*op.fixed_frequency == Catch::Approx(440.0f));
}

// =============================================================================
// Modulation types
// =============================================================================

TEST_CASE("TITP001A: ModulationSource construction", "[timbre-ir][types]") {
    SECTION("LFO source") {
        ModulationSource src;
        src.type = ModulationSourceType::LFO;
        src.index = 0;
        CHECK(src.type == ModulationSourceType::LFO);
    }

    SECTION("CC source") {
        ModulationSource src;
        src.type = ModulationSourceType::CC;
        src.cc_number = 74;
        CHECK(src.cc_number == 74);
    }

    SECTION("AudioFollower source") {
        ModulationSource src;
        src.type = ModulationSourceType::AudioFollower;
        src.sidechain_part = PartId{5};
        CHECK(src.sidechain_part.value == 5);
    }
}

TEST_CASE("TITP001A: ModulationRouting with via source", "[timbre-ir][types]") {
    ModulationRouting r;
    r.source = {ModulationSourceType::LFO, 0};
    r.target = "source.filter.cutoff";
    r.depth = 0.5f;
    r.via = ModulationSource{ModulationSourceType::ModWheel};

    CHECK(r.depth == Catch::Approx(0.5f));
    CHECK(r.via.has_value());
    CHECK(r.via->type == ModulationSourceType::ModWheel);
}

TEST_CASE("TITP001A: ModulationMatrix with LFOs and macros", "[timbre-ir][types]") {
    ModulationMatrix matrix;

    LFO lfo;
    lfo.waveform = LFOWaveformType::Triangle;
    lfo.rate.hz = 4.5f;
    lfo.retrigger = true;
    matrix.lfos.push_back(lfo);

    MacroKnob macro;
    macro.index = 0;
    macro.name = "Brightness";
    macro.mappings = {
        {"source.filter.cutoff", 200.0f, 8000.0f, {}},
        {"source.filter.resonance", 0.0f, 0.4f, {}}
    };
    matrix.macro_knobs.push_back(macro);

    matrix.routings.push_back({
        {ModulationSourceType::LFO, 0},
        "source.filter.cutoff",
        0.3f
    });

    CHECK(matrix.lfos.size() == 1);
    CHECK(matrix.macro_knobs.size() == 1);
    CHECK(matrix.routings.size() == 1);
    CHECK(matrix.macro_knobs[0].mappings.size() == 2);
}

// =============================================================================
// LFO
// =============================================================================

TEST_CASE("TITP001A: LFO free-running vs tempo-synced", "[timbre-ir][types]") {
    SECTION("Free-running") {
        LFO lfo;
        lfo.rate.synced = false;
        lfo.rate.hz = 6.0f;
        CHECK(!lfo.rate.synced);
        CHECK(lfo.rate.hz == Catch::Approx(6.0f));
    }

    SECTION("Tempo-synced") {
        LFO lfo;
        lfo.rate.synced = true;
        lfo.rate.division = Beat{1, 8};
        CHECK(lfo.rate.synced);
        CHECK(lfo.rate.division == Beat{1, 8});
    }
}

// =============================================================================
// Effect parameter types
// =============================================================================

TEST_CASE("TITP001A: DistortionEffect defaults", "[timbre-ir][types]") {
    DistortionEffect fx;
    CHECK(fx.algorithm.type == DistortionAlgorithmType::SoftClip);
    CHECK(fx.drive == Catch::Approx(0.0f));
    CHECK(fx.tone == Catch::Approx(0.5f));
}

TEST_CASE("TITP001A: DelayEffect tempo-synced", "[timbre-ir][types]") {
    DelayEffect fx;
    fx.delay_time.synced = true;
    fx.delay_time.division = Beat{1, 8};
    fx.feedback = 0.4f;
    fx.stereo_mode = StereoDelayMode::PingPong;

    CHECK(fx.delay_time.synced);
    CHECK(fx.feedback == Catch::Approx(0.4f));
    CHECK(fx.stereo_mode == StereoDelayMode::PingPong);
}

TEST_CASE("TITP001A: ReverbEffect convolution algorithm", "[timbre-ir][types]") {
    ReverbEffect fx;
    fx.algorithm.type = ReverbAlgorithmType::Convolution;
    fx.algorithm.impulse_path = "/samples/ir/hall.wav";
    fx.decay_time = 2.5f;
    fx.pre_delay = 30.0f;

    CHECK(fx.algorithm.type == ReverbAlgorithmType::Convolution);
    CHECK(fx.algorithm.impulse_path == "/samples/ir/hall.wav");
}

TEST_CASE("TITP001A: EQEffect multi-band", "[timbre-ir][types]") {
    EQEffect eq;
    eq.bands = {
        {80.0f, -3.0f, 0.7f, EQBandType::LowShelf},
        {2000.0f, 2.0f, 1.4f, EQBandType::Peak},
        {12000.0f, 1.5f, 0.7f, EQBandType::HighShelf}
    };

    CHECK(eq.bands.size() == 3);
    CHECK(eq.bands[0].band_type == EQBandType::LowShelf);
    CHECK(eq.bands[1].gain == Catch::Approx(2.0f));
}

TEST_CASE("TITP001A: CompressorEffect with sidechain", "[timbre-ir][types]") {
    CompressorEffect comp;
    comp.threshold = -24.0f;
    comp.ratio = 8.0f;
    comp.sidechain = SidechainConfig{
        PartId{3},
        Filter{
            .config = {FilterCategory::HighPass, FilterSlope::Pole2},
            .cutoff = 200.0f
        }
    };

    CHECK(comp.sidechain.has_value());
    CHECK(comp.sidechain->source.value == 3);
    CHECK(comp.sidechain->filter.has_value());
}

// =============================================================================
// Semantic Descriptors
// =============================================================================

TEST_CASE("TITP001A: SemanticTimbreDescriptor construction", "[timbre-ir][types]") {
    SemanticTimbreDescriptor desc;
    desc.brightness = 0.7f;
    desc.warmth = 0.8f;
    desc.roughness = 0.1f;
    desc.attack_character = AttackCharacter::Bowed;
    desc.sustain_character = SustainCharacter::Evolving;
    desc.width = 0.6f;
    desc.tags = {"warm", "evolving", "strings"};
    desc.derivation = DerivationMode::ParameterDerived;

    CHECK(desc.brightness == Catch::Approx(0.7f));
    CHECK(desc.tags.size() == 3);
    CHECK(desc.derivation == DerivationMode::ParameterDerived);
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("TITP001A: TimbreAutomation breakpoints", "[timbre-ir][types]") {
    TimbreAutomation auto_filter;
    auto_filter.parameter_path = "source.filter.cutoff";
    auto_filter.interpolation = AutomationInterpolation::Smooth;
    auto_filter.breakpoints = {
        {ScoreTime{1, Beat::zero()}, 500.0f},
        {ScoreTime{5, Beat::zero()}, 2000.0f},
        {ScoreTime{9, Beat::zero()}, 800.0f}
    };

    CHECK(auto_filter.breakpoints.size() == 3);
    CHECK(auto_filter.interpolation == AutomationInterpolation::Smooth);
}
