/**
 * @file TSTI002A.cpp
 * @brief Unit tests for Timbre IR document model (TIDC001A)
 *
 * Component: TSTI002A
 * Domain: TS (Test) | Category: TI (Timbre IR)
 *
 * Tests: TIDC001A
 * Coverage: SoundSource variants, SoundSourceData, HybridSource,
 *           EffectChain, TimbreProfile construction
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Timbre/TIDC001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helper: build a minimal ADSR envelope
// =============================================================================

static Envelope make_adsr(float a_ms, float d_ms, float sustain, float r_ms) {
    return Envelope{
        .stages = {
            {a_ms, 1.0f, EnvelopeCurve::Linear},
            {d_ms, sustain, EnvelopeCurve::Exponential},
            {r_ms, 0.0f, EnvelopeCurve::Exponential}
        }
    };
}

// =============================================================================
// SubtractiveSynth
// =============================================================================

TEST_CASE("TIDC001A: SubtractiveSynth with two oscillators", "[timbre-ir][document]") {
    SubtractiveSynth sub;

    Oscillator osc1;
    osc1.waveform.type = WaveformType::Saw;
    osc1.level = 0.8f;

    Oscillator osc2;
    osc2.waveform.type = WaveformType::Square;
    osc2.tune_semitones = -12;
    osc2.level = 0.5f;

    sub.oscillators = {osc1, osc2};
    sub.oscillator_mix = {0.8f, 0.5f};
    sub.filter.config.category = FilterCategory::LadderLP;
    sub.filter.cutoff = 2000.0f;
    sub.filter.resonance = 0.4f;
    sub.amplifier = make_adsr(5.0f, 300.0f, 0.7f, 200.0f);
    sub.unison = UnisonConfig{4, 10.0f, 0.5f, 0.7f};

    CHECK(sub.oscillators.size() == 2);
    CHECK(sub.oscillator_mix.size() == 2);
    CHECK(sub.filter.config.category == FilterCategory::LadderLP);
    CHECK(sub.unison.has_value());
    CHECK(sub.unison->voice_count == 4);
}

// =============================================================================
// FMSynth
// =============================================================================

TEST_CASE("TIDC001A: FMSynth four-operator", "[timbre-ir][document]") {
    FMSynth fm;
    fm.algorithm.use_preset = true;
    fm.algorithm.preset_number = 1;
    fm.feedback = 0.3f;

    for (int i = 0; i < 4; ++i) {
        FMOperator op;
        op.ratio = static_cast<float>(i + 1);
        op.level = 1.0f / static_cast<float>(i + 1);
        op.envelope = make_adsr(0.0f, 500.0f, 0.8f, 100.0f);
        fm.operators.push_back(op);
    }

    CHECK(fm.operators.size() == 4);
    CHECK(fm.operators[0].ratio == Catch::Approx(1.0f));
    CHECK(fm.operators[3].ratio == Catch::Approx(4.0f));
}

// =============================================================================
// WavetableSynth
// =============================================================================

TEST_CASE("TIDC001A: WavetableSynth construction", "[timbre-ir][document]") {
    WavetableSynth wt;
    wt.wavetable = "Serum/Analog";
    wt.position = 0.3f;
    wt.frame_count = 256;
    wt.interpolation = WavetableSynth::Interpolation::Spectral;
    wt.amplifier = make_adsr(10.0f, 200.0f, 0.8f, 300.0f);

    CHECK(wt.wavetable == "Serum/Analog");
    CHECK(wt.position == Catch::Approx(0.3f));
}

// =============================================================================
// GranularSynth
// =============================================================================

TEST_CASE("TIDC001A: GranularSynth construction", "[timbre-ir][document]") {
    GranularSynth gran;
    gran.source = "/samples/texture.wav";
    gran.grain_size = 80.0f;
    gran.grain_density = 30.0f;
    gran.position = 0.5f;
    gran.position_random = 0.1f;
    gran.reverse_probability = 0.2f;
    gran.grain_envelope.shape = GrainEnvelopeShape::Gaussian;

    CHECK(gran.grain_size == Catch::Approx(80.0f));
    CHECK(gran.grain_envelope.shape == GrainEnvelopeShape::Gaussian);
}

// =============================================================================
// AdditiveSynth
// =============================================================================

TEST_CASE("TIDC001A: AdditiveSynth harmonic series", "[timbre-ir][document]") {
    AdditiveSynth add;
    add.partial_count = 8;
    for (std::uint16_t i = 1; i <= 8; ++i) {
        PartialDefinition p;
        p.ratio = static_cast<float>(i);
        p.amplitude = 1.0f / static_cast<float>(i);
        add.partials.push_back(p);
    }
    add.global_envelope = make_adsr(50.0f, 200.0f, 0.6f, 500.0f);

    CHECK(add.partials.size() == 8);
    CHECK(add.partials[0].amplitude == Catch::Approx(1.0f));
    CHECK(add.partials[7].amplitude == Catch::Approx(1.0f / 8.0f));
}

// =============================================================================
// PhysicalModelSource
// =============================================================================

TEST_CASE("TIDC001A: PhysicalModelSource bowed string", "[timbre-ir][document]") {
    PhysicalModelSource phys;
    phys.model.category = PhysicalModelCategory::Bowed;
    phys.model.bow_pressure = 0.6f;
    phys.model.bow_speed = 0.4f;
    phys.exciter.type = ExciterType::Bow;
    phys.coupling = 0.7f;
    phys.damping = 0.3f;
    phys.brightness = 0.6f;

    CHECK(phys.model.category == PhysicalModelCategory::Bowed);
    CHECK(phys.exciter.type == ExciterType::Bow);
}

// =============================================================================
// SamplerSource
// =============================================================================

TEST_CASE("TIDC001A: SamplerSource with mic positions", "[timbre-ir][document]") {
    SamplerSource sampler;
    sampler.library = "Spitfire BBC Symphony Orchestra";
    sampler.preset = "Violins I - Legato";
    sampler.microphone_positions = {
        {"Close", 0.8f, true},
        {"Tree", 1.0f, true},
        {"Ambient", 0.4f, true},
        {"Outrigger", 0.0f, false}
    };
    sampler.release_samples = true;
    sampler.round_robin_mode = RoundRobinMode::RoundRobin;

    CHECK(sampler.microphone_positions.size() == 4);
    CHECK(!sampler.microphone_positions[3].enabled);
}

// =============================================================================
// SoundSourceData and variant dispatch
// =============================================================================

TEST_CASE("TIDC001A: SoundSourceData variant holds SubtractiveSynth", "[timbre-ir][document]") {
    SoundSourceData src;
    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    src.data = std::move(sub);

    CHECK(std::holds_alternative<SubtractiveSynth>(src.data));
}

TEST_CASE("TIDC001A: SoundSourceData variant holds SamplerSource", "[timbre-ir][document]") {
    SoundSourceData src;
    SamplerSource sampler;
    sampler.library = "Kontakt Factory";
    sampler.preset = "Grand Piano";
    src.data = std::move(sampler);

    CHECK(std::holds_alternative<SamplerSource>(src.data));
    CHECK(std::get<SamplerSource>(src.data).library == "Kontakt Factory");
}

// =============================================================================
// HybridSource (recursive)
// =============================================================================

TEST_CASE("TIDC001A: HybridSource with two layers", "[timbre-ir][document]") {
    auto layer1 = std::make_unique<SoundSourceData>();
    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    layer1->data = std::move(sub);

    auto layer2 = std::make_unique<SoundSourceData>();
    SamplerSource sampler;
    sampler.library = "Kontakt";
    sampler.preset = "Bass";
    layer2->data = std::move(sampler);

    HybridSource hybrid;
    hybrid.layers.push_back(std::move(layer1));
    hybrid.layers.push_back(std::move(layer2));
    hybrid.routing.type = LayerRoutingType::Mix;
    hybrid.routing.mix_levels = {0.7f, 0.3f};

    SoundSourceData src;
    src.data = std::move(hybrid);

    CHECK(std::holds_alternative<HybridSource>(src.data));

    auto& h = std::get<HybridSource>(src.data);
    CHECK(h.layers.size() == 2);
    CHECK(std::holds_alternative<SubtractiveSynth>(h.layers[0]->data));
    CHECK(std::holds_alternative<SamplerSource>(h.layers[1]->data));
}

// =============================================================================
// EffectChain
// =============================================================================

TEST_CASE("TIDC001A: EffectChain with multiple effects", "[timbre-ir][document]") {
    EffectChain chain;

    Effect dist;
    dist.id = EffectId{1};
    dist.parameters = DistortionEffect{
        .algorithm = {DistortionAlgorithmType::TubeEmulation},
        .drive = 0.4f,
        .tone = 0.6f,
        .output_level = 0.9f
    };
    dist.mix = 1.0f;

    Effect delay;
    delay.id = EffectId{2};
    delay.parameters = DelayEffect{
        .delay_time = {true, 0.0f, Beat{1, 8}},
        .feedback = 0.35f,
        .stereo_mode = StereoDelayMode::PingPong
    };
    delay.mix = 0.3f;

    Effect reverb;
    reverb.id = EffectId{3};
    reverb.parameters = ReverbEffect{
        .algorithm = {ReverbAlgorithmType::Hall},
        .decay_time = 2.0f,
        .pre_delay = 25.0f
    };
    reverb.mix = 0.25f;

    chain.effects = {dist, delay, reverb};

    CHECK(chain.effects.size() == 3);
    CHECK(std::holds_alternative<DistortionEffect>(chain.effects[0].parameters));
    CHECK(std::holds_alternative<DelayEffect>(chain.effects[1].parameters));
    CHECK(std::holds_alternative<ReverbEffect>(chain.effects[2].parameters));
    CHECK(chain.effects[1].mix == Catch::Approx(0.3f));
}

// =============================================================================
// TimbrePreset
// =============================================================================

TEST_CASE("TIDC001A: TimbrePreset with parameter state", "[timbre-ir][document]") {
    TimbrePreset preset;
    preset.id = TimbrePresetId{1};
    preset.name = "Dark Pad";
    preset.description = "Warm, dark evolving pad";
    preset.semantic_descriptors.brightness = 0.2f;
    preset.semantic_descriptors.warmth = 0.9f;
    preset.semantic_descriptors.movement = 0.7f;
    preset.parameter_state = {
        {"source.filter.cutoff", 800.0f},
        {"source.filter.resonance", 0.3f},
        {"source.oscillators[0].level", 1.0f}
    };
    preset.tags = {"pad", "dark", "evolving"};

    CHECK(preset.parameter_state.size() == 3);
    CHECK(preset.tags.size() == 3);
}

// =============================================================================
// PresetMorph
// =============================================================================

TEST_CASE("TIDC001A: PresetMorph between two presets", "[timbre-ir][document]") {
    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{1, Beat::zero()};
    morph.end = ScoreTime{9, Beat::zero()};
    morph.curve.type = MappingCurveType::SCurve;

    CHECK(morph.from_preset.value == 1);
    CHECK(morph.to_preset.value == 2);
    CHECK(morph.start.bar == 1);
    CHECK(morph.end.bar == 9);
}

// =============================================================================
// TimbreRenderingConfig
// =============================================================================

TEST_CASE("TIDC001A: TimbreRenderingConfig native Ableton device", "[timbre-ir][document]") {
    TimbreRenderingConfig cfg;
    cfg.device_type.tag = DeviceTypeTag::NativeAbleton;
    cfg.device_type.device_name = "Analog";
    cfg.parameter_map = {
        {"source.oscillators[0].waveform", DeviceParameter{0, "Osc1 Shape", 0.0f, 1.0f}},
        {"source.filter.cutoff", DeviceParameter{0, "Filter Freq", 20.0f, 20000.0f}}
    };

    CHECK(cfg.device_type.tag == DeviceTypeTag::NativeAbleton);
    CHECK(cfg.parameter_map.size() == 2);
}

// =============================================================================
// TimbreProfile (full construction)
// =============================================================================

TEST_CASE("TIDC001A: TimbreProfile complete construction", "[timbre-ir][document]") {
    TimbreProfile profile;
    profile.id = TimbreProfileId{1};
    profile.part_id = PartId{1};
    profile.name = "Warm Analog Pad";

    // Sound source: subtractive
    SubtractiveSynth sub;
    Oscillator osc;
    osc.waveform.type = WaveformType::Saw;
    sub.oscillators = {osc};
    sub.filter.config.category = FilterCategory::LowPass;
    sub.filter.cutoff = 2000.0f;
    sub.amplifier = make_adsr(200.0f, 500.0f, 0.7f, 800.0f);
    profile.source.data = std::move(sub);

    // Modulation
    LFO lfo;
    lfo.waveform = LFOWaveformType::Sine;
    lfo.rate.hz = 0.5f;
    profile.modulation.lfos = {lfo};
    profile.modulation.routings = {
        {{ModulationSourceType::LFO, 0}, "source.filter.cutoff", 0.3f}
    };

    // Effect chain
    Effect reverb;
    reverb.id = EffectId{1};
    reverb.parameters = ReverbEffect{
        .algorithm = {ReverbAlgorithmType::Hall},
        .decay_time = 3.0f
    };
    reverb.mix = 0.3f;
    profile.insert_chain.effects = {reverb};

    // Semantic descriptors
    profile.semantic_descriptors.brightness = 0.3f;
    profile.semantic_descriptors.warmth = 0.8f;
    profile.semantic_descriptors.attack_character = AttackCharacter::Gradual;
    profile.semantic_descriptors.sustain_character = SustainCharacter::Evolving;
    profile.semantic_descriptors.tags = {"pad", "warm", "analog"};

    // Automation
    profile.parameter_automation = {{
        "source.filter.cutoff",
        {{ScoreTime{1, Beat::zero()}, 2000.0f},
         {ScoreTime{17, Beat::zero()}, 5000.0f}},
        AutomationInterpolation::Smooth
    }};

    CHECK(profile.id.value == 1);
    CHECK(profile.part_id.value == 1);
    CHECK(std::holds_alternative<SubtractiveSynth>(profile.source.data));
    CHECK(profile.modulation.lfos.size() == 1);
    CHECK(profile.modulation.routings.size() == 1);
    CHECK(profile.insert_chain.effects.size() == 1);
    CHECK(profile.semantic_descriptors.tags.size() == 3);
    CHECK(profile.parameter_automation.size() == 1);
}
