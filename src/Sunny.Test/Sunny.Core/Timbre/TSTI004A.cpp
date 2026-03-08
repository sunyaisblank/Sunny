/**
 * @file TSTI004A.cpp
 * @brief Unit tests for Timbre IR serialisation (TISZ001A)
 *
 * Component: TSTI004A
 * Domain: TS (Test) | Category: TI (Timbre IR)
 *
 * Tests: TISZ001A
 * Coverage: Round-trip fidelity for all SoundSource variants, effects,
 *           modulation, presets, automation, rendering config; schema
 *           version check; malformed JSON rejection.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Timbre/TISZ001A.h"

using namespace Sunny::Core;
using Catch::Approx;

// =============================================================================
// Helpers
// =============================================================================

static Envelope make_adsr(float a, float d, float sus, float r) {
    return Envelope{
        .stages = {
            {a, 1.0f, EnvelopeCurve::Linear},
            {d, sus, EnvelopeCurve::Exponential},
            {r, 0.0f, EnvelopeCurve::Exponential}
        }
    };
}

/// Minimal valid SubtractiveSynth profile for round-trip baseline
static TimbreProfile make_subtractive_profile() {
    TimbreProfile p;
    p.id = TimbreProfileId{42};
    p.part_id = PartId{7};
    p.name = "SubSynth Lead";

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{{WaveformType::Saw}, 0, 0.0f, 0.0f, 0.5f, 1.0f},
                       Oscillator{{WaveformType::Square}, -12, 5.0f, 0.25f, 0.3f, 0.8f}};
    sub.noise = NoiseGenerator{NoiseColour::Pink, 0.15f};
    sub.oscillator_mix = {0.7f, 0.3f};
    sub.filter.config.category = FilterCategory::LadderLP;
    sub.filter.config.slope = FilterSlope::Pole4;
    sub.filter.cutoff = 2400.0f;
    sub.filter.resonance = 0.6f;
    sub.filter.drive = 0.2f;
    sub.filter.key_tracking = 0.5f;
    sub.filter.envelope = make_adsr(10.0f, 200.0f, 0.3f, 300.0f);
    sub.filter.envelope_depth = 0.7f;
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    sub.unison = UnisonConfig{4, 15.0f, 0.8f, 0.6f};
    sub.portamento = PortamentoConfig{80.0f, PortamentoMode::Legato, GlideCurve::Exponential};
    p.source.data = std::move(sub);

    return p;
}

/// Round-trip a profile through JSON and return the deserialised result
static TimbreProfile round_trip(const TimbreProfile& original) {
    auto j = timbre_to_json(original);
    auto result = timbre_from_json(j);
    REQUIRE(result.has_value());
    return std::move(*result);
}

/// Round-trip through string form
static TimbreProfile round_trip_string(const TimbreProfile& original) {
    auto s = timbre_to_json_string(original);
    auto result = timbre_from_json_string(s);
    REQUIRE(result.has_value());
    return std::move(*result);
}

// =============================================================================
// Schema version
// =============================================================================

TEST_CASE("TISZ001A: schema version check rejects wrong version", "[timbre-ir][serialisation]") {
    auto p = make_subtractive_profile();
    auto j = timbre_to_json(p);
    j["schema_version"] = 999;
    auto result = timbre_from_json(j);
    CHECK(!result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("TISZ001A: schema version is written correctly", "[timbre-ir][serialisation]") {
    auto p = make_subtractive_profile();
    auto j = timbre_to_json(p);
    CHECK(j.at("schema_version").get<int>() == TIMBRE_IR_SCHEMA_VERSION);
}

// =============================================================================
// SubtractiveSynth round-trip
// =============================================================================

TEST_CASE("TISZ001A: SubtractiveSynth round-trip preserves all fields", "[timbre-ir][serialisation]") {
    auto original = make_subtractive_profile();
    auto rt = round_trip(original);

    CHECK(rt.id.value == 42);
    CHECK(rt.part_id.value == 7);
    CHECK(rt.name == "SubSynth Lead");

    auto& sub = std::get<SubtractiveSynth>(rt.source.data);
    REQUIRE(sub.oscillators.size() == 2);
    CHECK(sub.oscillators[0].waveform.type == WaveformType::Saw);
    CHECK(sub.oscillators[1].tune_semitones == -12);
    CHECK(sub.oscillators[1].tune_cents == Approx(5.0f));
    CHECK(sub.oscillators[1].pulse_width == Approx(0.3f));

    REQUIRE(sub.noise.has_value());
    CHECK(sub.noise->colour == NoiseColour::Pink);
    CHECK(sub.noise->level == Approx(0.15f));

    CHECK(sub.oscillator_mix.size() == 2);
    CHECK(sub.oscillator_mix[0] == Approx(0.7f));

    CHECK(sub.filter.config.category == FilterCategory::LadderLP);
    CHECK(sub.filter.config.slope == FilterSlope::Pole4);
    CHECK(sub.filter.cutoff == Approx(2400.0f));
    CHECK(sub.filter.resonance == Approx(0.6f));
    CHECK(sub.filter.envelope_depth == Approx(0.7f));
    REQUIRE(sub.filter.envelope.has_value());
    CHECK(sub.filter.envelope->stages.size() == 3);

    CHECK(sub.amplifier.stages.size() == 3);

    REQUIRE(sub.unison.has_value());
    CHECK(sub.unison->voice_count == 4);
    CHECK(sub.unison->detune == Approx(15.0f));

    REQUIRE(sub.portamento.has_value());
    CHECK(sub.portamento->mode == PortamentoMode::Legato);
    CHECK(sub.portamento->curve == GlideCurve::Exponential);
}

// =============================================================================
// FMSynth round-trip
// =============================================================================

TEST_CASE("TISZ001A: FMSynth round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{1};
    p.part_id = PartId{1};
    p.name = "FM Bass";

    FMSynth fm;
    FMOperator op1;
    op1.ratio = 1.0f;
    op1.level = 1.0f;
    op1.envelope = make_adsr(1.0f, 50.0f, 0.6f, 100.0f);
    op1.detune = 3.0f;
    FMOperator op2;
    op2.ratio = 2.0f;
    op2.fixed_frequency = 440.0f;
    op2.level = 0.8f;
    op2.envelope = make_adsr(2.0f, 80.0f, 0.4f, 150.0f);
    fm.operators = {op1, op2};
    fm.algorithm.use_preset = false;
    fm.algorithm.preset_number = 5;
    fm.algorithm.custom_routing = {{0, 1, 0.75f}};
    fm.feedback = 0.3f;
    p.source.data = std::move(fm);

    auto rt = round_trip(p);
    auto& fm_rt = std::get<FMSynth>(rt.source.data);
    REQUIRE(fm_rt.operators.size() == 2);
    CHECK(fm_rt.operators[0].ratio == Approx(1.0f));
    CHECK(fm_rt.operators[1].fixed_frequency.has_value());
    CHECK(*fm_rt.operators[1].fixed_frequency == Approx(440.0f));
    CHECK(!fm_rt.algorithm.use_preset);
    REQUIRE(fm_rt.algorithm.custom_routing.size() == 1);
    CHECK(fm_rt.algorithm.custom_routing[0].depth == Approx(0.75f));
    CHECK(fm_rt.feedback == Approx(0.3f));
}

// =============================================================================
// WavetableSynth round-trip
// =============================================================================

TEST_CASE("TISZ001A: WavetableSynth round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{2};
    p.part_id = PartId{1};
    p.name = "Wavetable Pad";

    WavetableSynth wt;
    wt.wavetable = "basic_shapes";
    wt.position = 0.35f;
    wt.frame_count = 512;
    wt.interpolation = WavetableSynth::Interpolation::Spectral;
    wt.filter = Filter{};
    wt.filter->cutoff = 5000.0f;
    wt.amplifier = make_adsr(50.0f, 200.0f, 0.9f, 500.0f);
    p.source.data = std::move(wt);

    auto rt = round_trip(p);
    auto& wt_rt = std::get<WavetableSynth>(rt.source.data);
    CHECK(wt_rt.wavetable == "basic_shapes");
    CHECK(wt_rt.position == Approx(0.35f));
    CHECK(wt_rt.frame_count == 512);
    CHECK(wt_rt.interpolation == WavetableSynth::Interpolation::Spectral);
    REQUIRE(wt_rt.filter.has_value());
    CHECK(wt_rt.filter->cutoff == Approx(5000.0f));
}

// =============================================================================
// GranularSynth round-trip
// =============================================================================

TEST_CASE("TISZ001A: GranularSynth round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{3};
    p.part_id = PartId{1};
    p.name = "Granular Texture";

    GranularSynth gr;
    gr.source = "field_recording.wav";
    gr.grain_size = 80.0f;
    gr.grain_density = 30.0f;
    gr.position = 0.5f;
    gr.position_random = 0.1f;
    gr.pitch_random = 25.0f;
    gr.grain_envelope.shape = GrainEnvelopeShape::Trapezoid;
    gr.grain_envelope.trapezoid_attack_ratio = 0.3f;
    gr.spray = 0.2f;
    gr.stereo_spread = 0.6f;
    gr.reverse_probability = 0.15f;
    p.source.data = std::move(gr);

    auto rt = round_trip(p);
    auto& gr_rt = std::get<GranularSynth>(rt.source.data);
    CHECK(gr_rt.source == "field_recording.wav");
    CHECK(gr_rt.grain_size == Approx(80.0f));
    CHECK(gr_rt.grain_density == Approx(30.0f));
    CHECK(gr_rt.grain_envelope.shape == GrainEnvelopeShape::Trapezoid);
    CHECK(gr_rt.grain_envelope.trapezoid_attack_ratio == Approx(0.3f));
    CHECK(gr_rt.reverse_probability == Approx(0.15f));
}

// =============================================================================
// AdditiveSynth round-trip
// =============================================================================

TEST_CASE("TISZ001A: AdditiveSynth round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{4};
    p.part_id = PartId{1};
    p.name = "Additive Organ";

    AdditiveSynth add;
    add.partial_count = 8;
    add.partials = {
        {1.0f, 1.0f, 0.0f, std::nullopt, 0.0f},
        {2.0f, 0.5f, 0.0f, make_adsr(10.0f, 100.0f, 0.8f, 200.0f), 1.5f},
        {3.0f, 0.33f, 0.1f, std::nullopt, 0.0f}
    };
    add.global_envelope = make_adsr(20.0f, 150.0f, 0.7f, 300.0f);
    p.source.data = std::move(add);

    auto rt = round_trip(p);
    auto& add_rt = std::get<AdditiveSynth>(rt.source.data);
    CHECK(add_rt.partial_count == 8);
    REQUIRE(add_rt.partials.size() == 3);
    CHECK(add_rt.partials[0].ratio == Approx(1.0f));
    CHECK(add_rt.partials[1].ratio == Approx(2.0f));
    REQUIRE(add_rt.partials[1].envelope.has_value());
    CHECK(add_rt.partials[1].detune == Approx(1.5f));
    CHECK(!add_rt.partials[0].envelope.has_value());
}

// =============================================================================
// PhysicalModelSource round-trip
// =============================================================================

TEST_CASE("TISZ001A: PhysicalModelSource round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{5};
    p.part_id = PartId{1};
    p.name = "Bowed String";

    PhysicalModelSource phys;
    phys.model.category = PhysicalModelCategory::Bowed;
    phys.model.string_material = StringMaterial::Gut;
    phys.model.bow_pressure = 0.7f;
    phys.model.bow_speed = 0.4f;
    phys.exciter.type = ExciterType::Bow;
    phys.exciter.amplitude = 0.9f;
    phys.exciter.brightness = 0.6f;
    phys.resonator.frequency_ratio = 1.0f;
    phys.resonator.decay = 0.8f;
    phys.resonator.mode_count = 12;
    phys.coupling = 0.3f;
    phys.damping = 0.4f;
    phys.brightness = 0.65f;
    p.source.data = std::move(phys);

    auto rt = round_trip(p);
    auto& phys_rt = std::get<PhysicalModelSource>(rt.source.data);
    CHECK(phys_rt.model.category == PhysicalModelCategory::Bowed);
    CHECK(phys_rt.model.string_material == StringMaterial::Gut);
    CHECK(phys_rt.exciter.type == ExciterType::Bow);
    CHECK(phys_rt.resonator.mode_count == 12);
    CHECK(phys_rt.coupling == Approx(0.3f));
}

// =============================================================================
// SamplerSource round-trip
// =============================================================================

TEST_CASE("TISZ001A: SamplerSource round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{6};
    p.part_id = PartId{1};
    p.name = "Orchestral Strings";

    SamplerSource sampler;
    sampler.library = "Orchestral Tools";
    sampler.preset = "Violins Sustain";
    sampler.sample_map.zones = {
        {36, 59, 0, 63, "sustain", "/samples/vln_sus_lo.wav"},
        {60, 84, 64, 127, "sustain", "/samples/vln_sus_hi.wav"}
    };
    sampler.microphone_positions = {
        {"Close", 1.0f, true},
        {"Room", 0.6f, true}
    };
    sampler.release_samples = true;
    sampler.round_robin_mode = RoundRobinMode::Random;
    sampler.envelope_override = make_adsr(10.0f, 50.0f, 1.0f, 300.0f);
    sampler.filter = Filter{};
    sampler.filter->cutoff = 8000.0f;
    sampler.tuning_offset = -5.0f;
    p.source.data = std::move(sampler);

    auto rt = round_trip(p);
    auto& s_rt = std::get<SamplerSource>(rt.source.data);
    CHECK(s_rt.library == "Orchestral Tools");
    CHECK(s_rt.preset == "Violins Sustain");
    REQUIRE(s_rt.sample_map.zones.size() == 2);
    CHECK(s_rt.sample_map.zones[0].low_key == 36);
    CHECK(s_rt.sample_map.zones[1].articulation == "sustain");
    REQUIRE(s_rt.microphone_positions.size() == 2);
    CHECK(s_rt.microphone_positions[1].name == "Room");
    CHECK(s_rt.release_samples);
    CHECK(s_rt.round_robin_mode == RoundRobinMode::Random);
    REQUIRE(s_rt.envelope_override.has_value());
    REQUIRE(s_rt.filter.has_value());
    CHECK(s_rt.tuning_offset == Approx(-5.0f));
}

// =============================================================================
// HybridSource round-trip (recursive)
// =============================================================================

TEST_CASE("TISZ001A: HybridSource round-trip with nested layers", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{7};
    p.part_id = PartId{1};
    p.name = "Hybrid Pad";

    auto layer1 = std::make_unique<SoundSourceData>();
    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier = make_adsr(50.0f, 200.0f, 0.9f, 500.0f);
    layer1->data = std::move(sub);

    auto layer2 = std::make_unique<SoundSourceData>();
    AdditiveSynth add;
    add.partial_count = 4;
    add.partials = {{1.0f, 1.0f, 0.0f, std::nullopt, 0.0f}};
    add.global_envelope = make_adsr(100.0f, 300.0f, 0.7f, 600.0f);
    layer2->data = std::move(add);

    HybridSource hybrid;
    hybrid.layers.push_back(std::move(layer1));
    hybrid.layers.push_back(std::move(layer2));
    hybrid.routing.type = LayerRoutingType::Mix;
    hybrid.routing.mix_levels = {0.6f, 0.4f};
    p.source.data = std::move(hybrid);

    auto rt = round_trip(p);
    auto& h_rt = std::get<HybridSource>(rt.source.data);
    REQUIRE(h_rt.layers.size() == 2);
    REQUIRE(h_rt.layers[0] != nullptr);
    REQUIRE(h_rt.layers[1] != nullptr);

    CHECK(std::holds_alternative<SubtractiveSynth>(h_rt.layers[0]->data));
    CHECK(std::holds_alternative<AdditiveSynth>(h_rt.layers[1]->data));

    CHECK(h_rt.routing.type == LayerRoutingType::Mix);
    CHECK(h_rt.routing.mix_levels.size() == 2);
    CHECK(h_rt.routing.mix_levels[0] == Approx(0.6f));
}

// =============================================================================
// Effect chain round-trip
// =============================================================================

TEST_CASE("TISZ001A: EffectChain round-trip with multiple effect types", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    DistortionEffect dist;
    dist.algorithm.type = DistortionAlgorithmType::TubeEmulation;
    dist.drive = 0.6f;
    dist.tone = 0.7f;

    DelayEffect delay;
    delay.delay_time.synced = true;
    delay.delay_time.division = Beat{1, 8};
    delay.feedback = 0.4f;
    delay.stereo_mode = StereoDelayMode::PingPong;

    ReverbEffect reverb;
    reverb.algorithm.type = ReverbAlgorithmType::Hall;
    reverb.decay_time = 2.5f;
    reverb.pre_delay = 30.0f;

    ChorusEffect chorus;
    chorus.rate = 1.5f;
    chorus.depth = 0.6f;
    chorus.voices = 3;

    CompressorEffect comp;
    comp.threshold = -18.0f;
    comp.ratio = 3.0f;
    comp.sidechain = SidechainConfig{PartId{2}, std::nullopt};

    p.insert_chain.effects = {
        {EffectId{1}, dist, true, 1.0f},
        {EffectId{2}, delay, true, 0.5f},
        {EffectId{3}, reverb, false, 0.3f},
        {EffectId{4}, chorus, true, 0.7f},
        {EffectId{5}, comp, true, 1.0f}
    };
    p.insert_chain.bypass_all = false;

    auto rt = round_trip(p);
    REQUIRE(rt.insert_chain.effects.size() == 5);
    CHECK(!rt.insert_chain.bypass_all);

    CHECK(std::holds_alternative<DistortionEffect>(rt.insert_chain.effects[0].parameters));
    CHECK(std::holds_alternative<DelayEffect>(rt.insert_chain.effects[1].parameters));
    CHECK(std::holds_alternative<ReverbEffect>(rt.insert_chain.effects[2].parameters));
    CHECK(std::holds_alternative<ChorusEffect>(rt.insert_chain.effects[3].parameters));
    CHECK(std::holds_alternative<CompressorEffect>(rt.insert_chain.effects[4].parameters));

    auto& dist_rt = std::get<DistortionEffect>(rt.insert_chain.effects[0].parameters);
    CHECK(dist_rt.algorithm.type == DistortionAlgorithmType::TubeEmulation);
    CHECK(dist_rt.drive == Approx(0.6f));

    auto& delay_rt = std::get<DelayEffect>(rt.insert_chain.effects[1].parameters);
    CHECK(delay_rt.delay_time.synced);
    CHECK(delay_rt.stereo_mode == StereoDelayMode::PingPong);

    auto& reverb_rt = std::get<ReverbEffect>(rt.insert_chain.effects[2].parameters);
    CHECK(reverb_rt.algorithm.type == ReverbAlgorithmType::Hall);
    CHECK(reverb_rt.decay_time == Approx(2.5f));
    CHECK(!rt.insert_chain.effects[2].enabled);

    auto& comp_rt = std::get<CompressorEffect>(rt.insert_chain.effects[4].parameters);
    CHECK(comp_rt.threshold == Approx(-18.0f));
    REQUIRE(comp_rt.sidechain.has_value());
    CHECK(comp_rt.sidechain->source.value == 2);
}

// =============================================================================
// Phaser, Flanger, EQ effect round-trip
// =============================================================================

TEST_CASE("TISZ001A: Phaser, Flanger, EQ round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    PhaserEffect phaser;
    phaser.rate = 0.8f;
    phaser.stages = 6;
    phaser.feedback = 0.5f;
    phaser.center_frequency = 1200.0f;

    FlangerEffect flanger;
    flanger.rate = 0.4f;
    flanger.feedback = -0.3f;
    flanger.manual = 3.0f;

    EQEffect eq;
    eq.bands = {
        {100.0f, -3.0f, 0.7f, EQBandType::LowShelf},
        {3000.0f, 2.0f, 1.5f, EQBandType::Peak},
        {10000.0f, -6.0f, 0.5f, EQBandType::HighCut}
    };

    p.insert_chain.effects = {
        {EffectId{10}, phaser, true, 0.5f},
        {EffectId{11}, flanger, true, 0.4f},
        {EffectId{12}, eq, true, 1.0f}
    };

    auto rt = round_trip(p);
    REQUIRE(rt.insert_chain.effects.size() == 3);

    auto& ph_rt = std::get<PhaserEffect>(rt.insert_chain.effects[0].parameters);
    CHECK(ph_rt.stages == 6);
    CHECK(ph_rt.center_frequency == Approx(1200.0f));

    auto& fl_rt = std::get<FlangerEffect>(rt.insert_chain.effects[1].parameters);
    CHECK(fl_rt.feedback == Approx(-0.3f));

    auto& eq_rt = std::get<EQEffect>(rt.insert_chain.effects[2].parameters);
    REQUIRE(eq_rt.bands.size() == 3);
    CHECK(eq_rt.bands[0].band_type == EQBandType::LowShelf);
    CHECK(eq_rt.bands[2].band_type == EQBandType::HighCut);
}

// =============================================================================
// Modulation matrix round-trip
// =============================================================================

TEST_CASE("TISZ001A: ModulationMatrix round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    LFO lfo;
    lfo.waveform = LFOWaveformType::Triangle;
    lfo.rate.synced = true;
    lfo.rate.division = Beat{1, 4};
    lfo.phase = 0.25f;
    lfo.retrigger = false;
    lfo.fade_in = 500.0f;
    lfo.custom_points = {{0.0f, 0.0f}, {0.5f, 1.0f}, {1.0f, 0.0f}};

    StepSequencer seq;
    seq.steps = {0.0f, 0.5f, 1.0f, 0.75f, 0.25f, 0.5f, 0.8f, 0.1f};
    seq.step_duration = Beat{1, 16};
    seq.smooth = 0.3f;
    seq.loop_mode = LoopMode::PingPong;

    MacroKnob macro;
    macro.index = 0;
    macro.name = "Brightness";
    macro.value = 0.5f;
    macro.mappings = {
        {"source.filter.cutoff", 200.0f, 8000.0f, {MappingCurveType::Exponential, {}}},
        {"source.filter.resonance", 0.0f, 0.8f, {MappingCurveType::Linear, {}}}
    };

    ModulationRouting route;
    route.source = {ModulationSourceType::LFO, 0};
    route.target = "source.filter.cutoff";
    route.depth = 0.6f;
    route.via = ModulationSource{ModulationSourceType::ModWheel, 0};

    p.modulation.lfos = {lfo};
    p.modulation.step_sequencers = {seq};
    p.modulation.macro_knobs = {macro};
    p.modulation.routings = {route};

    auto rt = round_trip(p);

    REQUIRE(rt.modulation.lfos.size() == 1);
    CHECK(rt.modulation.lfos[0].waveform == LFOWaveformType::Triangle);
    CHECK(rt.modulation.lfos[0].rate.synced);
    CHECK(!rt.modulation.lfos[0].retrigger);
    CHECK(rt.modulation.lfos[0].fade_in == Approx(500.0f));
    CHECK(rt.modulation.lfos[0].custom_points.size() == 3);

    REQUIRE(rt.modulation.step_sequencers.size() == 1);
    CHECK(rt.modulation.step_sequencers[0].steps.size() == 8);
    CHECK(rt.modulation.step_sequencers[0].loop_mode == LoopMode::PingPong);

    REQUIRE(rt.modulation.macro_knobs.size() == 1);
    CHECK(rt.modulation.macro_knobs[0].name == "Brightness");
    REQUIRE(rt.modulation.macro_knobs[0].mappings.size() == 2);
    CHECK(rt.modulation.macro_knobs[0].mappings[0].min == Approx(200.0f));
    CHECK(rt.modulation.macro_knobs[0].mappings[0].curve.type == MappingCurveType::Exponential);

    REQUIRE(rt.modulation.routings.size() == 1);
    CHECK(rt.modulation.routings[0].source.type == ModulationSourceType::LFO);
    CHECK(rt.modulation.routings[0].target == "source.filter.cutoff");
    CHECK(rt.modulation.routings[0].depth == Approx(0.6f));
    REQUIRE(rt.modulation.routings[0].via.has_value());
    CHECK(rt.modulation.routings[0].via->type == ModulationSourceType::ModWheel);
}

// =============================================================================
// Semantic descriptors round-trip
// =============================================================================

TEST_CASE("TISZ001A: SemanticTimbreDescriptor round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();
    p.semantic_descriptors.brightness = 0.8f;
    p.semantic_descriptors.warmth = 0.3f;
    p.semantic_descriptors.roughness = 0.2f;
    p.semantic_descriptors.attack_character = AttackCharacter::Plucked;
    p.semantic_descriptors.sustain_character = SustainCharacter::Evolving;
    p.semantic_descriptors.width = 0.6f;
    p.semantic_descriptors.density = 0.4f;
    p.semantic_descriptors.movement = 0.7f;
    p.semantic_descriptors.weight = 0.3f;
    p.semantic_descriptors.tags = {"bright", "metallic", "evolving"};
    p.semantic_descriptors.derivation = DerivationMode::ParameterDerived;

    auto rt = round_trip(p);
    CHECK(rt.semantic_descriptors.brightness == Approx(0.8f));
    CHECK(rt.semantic_descriptors.attack_character == AttackCharacter::Plucked);
    CHECK(rt.semantic_descriptors.sustain_character == SustainCharacter::Evolving);
    CHECK(rt.semantic_descriptors.tags.size() == 3);
    CHECK(rt.semantic_descriptors.tags[1] == "metallic");
    CHECK(rt.semantic_descriptors.derivation == DerivationMode::ParameterDerived);
}

// =============================================================================
// Automation round-trip
// =============================================================================

TEST_CASE("TISZ001A: TimbreAutomation round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    TimbreAutomation auto1;
    auto1.parameter_path = "source.filter.cutoff";
    auto1.breakpoints = {
        {ScoreTime{1, Beat{0, 1}}, 2000.0f},
        {ScoreTime{2, Beat{2, 4}}, 8000.0f},
        {ScoreTime{4, Beat{0, 1}}, 2000.0f}
    };
    auto1.interpolation = AutomationInterpolation::Smooth;

    p.parameter_automation = {auto1};

    auto rt = round_trip(p);
    REQUIRE(rt.parameter_automation.size() == 1);
    CHECK(rt.parameter_automation[0].parameter_path == "source.filter.cutoff");
    REQUIRE(rt.parameter_automation[0].breakpoints.size() == 3);
    CHECK(rt.parameter_automation[0].breakpoints[1].value == Approx(8000.0f));
    CHECK(rt.parameter_automation[0].interpolation == AutomationInterpolation::Smooth);
}

// =============================================================================
// Preset and PresetMorph round-trip
// =============================================================================

TEST_CASE("TISZ001A: TimbrePreset round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    TimbrePreset preset;
    preset.id = TimbrePresetId{100};
    preset.name = "Warm Pad";
    preset.description = "A warm evolving pad sound";
    preset.semantic_descriptors.brightness = 0.3f;
    preset.semantic_descriptors.warmth = 0.9f;
    preset.parameter_state = {{"source.filter.cutoff", 3000.0f}, {"source.filter.resonance", 0.4f}};
    preset.tags = {"pad", "warm"};
    p.presets = {preset};

    auto rt = round_trip(p);
    REQUIRE(rt.presets.size() == 1);
    CHECK(rt.presets[0].id.value == 100);
    CHECK(rt.presets[0].name == "Warm Pad");
    REQUIRE(rt.presets[0].description.has_value());
    CHECK(*rt.presets[0].description == "A warm evolving pad sound");
    CHECK(rt.presets[0].parameter_state.size() == 2);
    CHECK(rt.presets[0].parameter_state.at("source.filter.cutoff") == Approx(3000.0f));
    CHECK(rt.presets[0].tags.size() == 2);
}

TEST_CASE("TISZ001A: PresetMorph round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{100};
    morph.to_preset = TimbrePresetId{101};
    morph.start = ScoreTime{1, Beat{0, 1}};
    morph.end = ScoreTime{4, Beat{0, 1}};
    morph.curve.type = MappingCurveType::SCurve;
    p.preset_morphs = {morph};

    auto rt = round_trip(p);
    REQUIRE(rt.preset_morphs.size() == 1);
    CHECK(rt.preset_morphs[0].from_preset.value == 100);
    CHECK(rt.preset_morphs[0].to_preset.value == 101);
    CHECK(rt.preset_morphs[0].curve.type == MappingCurveType::SCurve);
}

// =============================================================================
// Rendering config round-trip
// =============================================================================

TEST_CASE("TISZ001A: TimbreRenderingConfig round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    p.rendering.device_type.tag = DeviceTypeTag::Plugin;
    p.rendering.device_type.device_name = "Serum";
    p.rendering.device_type.plugin_format = PluginFormat::VST;
    p.rendering.device_type.plugin_identifier = "com.xfer.serum";
    p.rendering.parameter_map = {
        {"source.filter.cutoff", {0, "Filter Cutoff", 20.0f, 20000.0f, {MappingCurveType::Logarithmic, {}}}},
        {"source.filter.resonance", {1, "Filter Resonance", 0.0f, 1.0f, {MappingCurveType::Linear, {}}}}
    };
    p.rendering.preset_path = "/presets/lead/bright.fxp";

    auto rt = round_trip(p);
    CHECK(rt.rendering.device_type.tag == DeviceTypeTag::Plugin);
    CHECK(rt.rendering.device_type.device_name == "Serum");
    CHECK(rt.rendering.device_type.plugin_identifier == "com.xfer.serum");
    CHECK(rt.rendering.parameter_map.size() == 2);
    CHECK(rt.rendering.parameter_map.at("source.filter.cutoff").parameter_name == "Filter Cutoff");
    CHECK(rt.rendering.parameter_map.at("source.filter.cutoff").range_max == Approx(20000.0f));
    REQUIRE(rt.rendering.preset_path.has_value());
    CHECK(*rt.rendering.preset_path == "/presets/lead/bright.fxp");
}

// =============================================================================
// String-form round-trip
// =============================================================================

TEST_CASE("TISZ001A: string serialisation round-trip", "[timbre-ir][serialisation]") {
    auto original = make_subtractive_profile();
    auto rt = round_trip_string(original);
    CHECK(rt.id.value == original.id.value);
    CHECK(rt.name == original.name);
    auto& sub = std::get<SubtractiveSynth>(rt.source.data);
    CHECK(sub.oscillators.size() == 2);
}

// =============================================================================
// Malformed JSON rejection
// =============================================================================

TEST_CASE("TISZ001A: malformed JSON string returns error", "[timbre-ir][serialisation]") {
    auto result = timbre_from_json_string("{not valid json");
    CHECK(!result.has_value());
    CHECK(result.error() == ErrorCode::FormatError);
}

TEST_CASE("TISZ001A: missing required field returns error", "[timbre-ir][serialisation]") {
    auto p = make_subtractive_profile();
    auto j = timbre_to_json(p);
    j.erase("name");
    auto result = timbre_from_json(j);
    CHECK(!result.has_value());
}

// =============================================================================
// Custom waveform harmonics round-trip
// =============================================================================

TEST_CASE("TISZ001A: custom waveform harmonics round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{8};
    p.part_id = PartId{1};
    p.name = "Custom Wave";

    SubtractiveSynth sub;
    Oscillator osc;
    osc.waveform.type = WaveformType::Custom;
    osc.waveform.custom_harmonics = {
        {1, 1.0f, 0.0f},
        {3, 0.33f, 0.0f},
        {5, 0.2f, 0.5f}
    };
    sub.oscillators = {osc};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    p.source.data = std::move(sub);

    auto rt = round_trip(p);
    auto& sub_rt = std::get<SubtractiveSynth>(rt.source.data);
    auto& wf = sub_rt.oscillators[0].waveform;
    CHECK(wf.type == WaveformType::Custom);
    REQUIRE(wf.custom_harmonics.size() == 3);
    CHECK(wf.custom_harmonics[0].partial_number == 1);
    CHECK(wf.custom_harmonics[2].phase == Approx(0.5f));
}

// =============================================================================
// Envelope loop round-trip
// =============================================================================

TEST_CASE("TISZ001A: Envelope with loop round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{9};
    p.part_id = PartId{1};
    p.name = "Looped Env";

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier.stages = {
        {10.0f, 1.0f, EnvelopeCurve::Linear},
        {100.0f, 0.7f, EnvelopeCurve::Exponential},
        {50.0f, 0.9f, EnvelopeCurve::Linear},
        {200.0f, 0.0f, EnvelopeCurve::Exponential}
    };
    sub.amplifier.loop = EnvelopeLoop{1, 2, 3};
    sub.amplifier.velocity_sensitivity = 0.8f;
    sub.amplifier.key_tracking = 0.2f;
    p.source.data = std::move(sub);

    auto rt = round_trip(p);
    auto& env = std::get<SubtractiveSynth>(rt.source.data).amplifier;
    REQUIRE(env.loop.has_value());
    CHECK(env.loop->start_stage == 1);
    CHECK(env.loop->end_stage == 2);
    CHECK(env.loop->count == 3);
    CHECK(env.velocity_sensitivity == Approx(0.8f));
    CHECK(env.key_tracking == Approx(0.2f));
}

// =============================================================================
// SuperSaw waveform round-trip
// =============================================================================

TEST_CASE("TISZ001A: SuperSaw waveform parameters round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{10};
    p.part_id = PartId{1};
    p.name = "SuperSaw";

    SubtractiveSynth sub;
    Oscillator osc;
    osc.waveform.type = WaveformType::SuperSaw;
    osc.waveform.super_saw_detune = 0.6f;
    osc.waveform.super_saw_voices = 9;
    sub.oscillators = {osc};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    p.source.data = std::move(sub);

    auto rt = round_trip(p);
    auto& wf = std::get<SubtractiveSynth>(rt.source.data).oscillators[0].waveform;
    CHECK(wf.type == WaveformType::SuperSaw);
    CHECK(wf.super_saw_detune == Approx(0.6f));
    CHECK(wf.super_saw_voices == 9);
}

// =============================================================================
// Dual filter with parallel routing round-trip
// =============================================================================

TEST_CASE("TISZ001A: dual filter parallel routing round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p;
    p.id = TimbreProfileId{11};
    p.part_id = PartId{1};
    p.name = "Dual Filter";

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.filter.cutoff = 3000.0f;
    sub.filter_2 = Filter{};
    sub.filter_2->config.category = FilterCategory::HighPass;
    sub.filter_2->cutoff = 500.0f;
    sub.filter_routing = SubtractiveSynth::FilterRouting::Parallel;
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    p.source.data = std::move(sub);

    auto rt = round_trip(p);
    auto& sub_rt = std::get<SubtractiveSynth>(rt.source.data);
    REQUIRE(sub_rt.filter_2.has_value());
    CHECK(sub_rt.filter_2->config.category == FilterCategory::HighPass);
    CHECK(sub_rt.filter_2->cutoff == Approx(500.0f));
    CHECK(sub_rt.filter_routing == SubtractiveSynth::FilterRouting::Parallel);
}

// =============================================================================
// Distortion waveshaper curve round-trip
// =============================================================================

TEST_CASE("TISZ001A: Distortion waveshaper curve round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    DistortionEffect dist;
    dist.algorithm.type = DistortionAlgorithmType::WaveShaper;
    dist.algorithm.waveshaper_curve = {{-1.0f, -0.8f}, {0.0f, 0.0f}, {1.0f, 0.8f}};
    dist.drive = 0.5f;
    p.insert_chain.effects = {{EffectId{1}, dist, true, 1.0f}};

    auto rt = round_trip(p);
    auto& d_rt = std::get<DistortionEffect>(rt.insert_chain.effects[0].parameters);
    REQUIRE(d_rt.algorithm.waveshaper_curve.size() == 3);
    CHECK(d_rt.algorithm.waveshaper_curve[0].first == Approx(-1.0f));
    CHECK(d_rt.algorithm.waveshaper_curve[0].second == Approx(-0.8f));
}

// =============================================================================
// Delay with feedback filter round-trip
// =============================================================================

TEST_CASE("TISZ001A: DelayEffect with feedback filter round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    DelayEffect delay;
    delay.feedback = 0.5f;
    delay.filter = Filter{};
    delay.filter->config.category = FilterCategory::LowPass;
    delay.filter->cutoff = 4000.0f;
    delay.modulation_rate = 0.5f;
    delay.modulation_depth = 2.0f;
    p.insert_chain.effects = {{EffectId{1}, delay, true, 0.4f}};

    auto rt = round_trip(p);
    auto& d_rt = std::get<DelayEffect>(rt.insert_chain.effects[0].parameters);
    REQUIRE(d_rt.filter.has_value());
    CHECK(d_rt.filter->cutoff == Approx(4000.0f));
    CHECK(d_rt.modulation_rate == Approx(0.5f));
}

// =============================================================================
// Reverb convolution with impulse path
// =============================================================================

TEST_CASE("TISZ001A: ReverbEffect convolution impulse path round-trip", "[timbre-ir][serialisation]") {
    TimbreProfile p = make_subtractive_profile();

    ReverbEffect reverb;
    reverb.algorithm.type = ReverbAlgorithmType::Convolution;
    reverb.algorithm.impulse_path = "/ir/church_hall.wav";
    reverb.decay_time = 3.0f;
    p.insert_chain.effects = {{EffectId{1}, reverb, true, 0.3f}};

    auto rt = round_trip(p);
    auto& r_rt = std::get<ReverbEffect>(rt.insert_chain.effects[0].parameters);
    CHECK(r_rt.algorithm.type == ReverbAlgorithmType::Convolution);
    CHECK(r_rt.algorithm.impulse_path == "/ir/church_hall.wav");
}

// =============================================================================
// Full profile JSON identity: serialise(deserialise(json)) == json
// =============================================================================

TEST_CASE("TISZ001A: full profile JSON identity", "[timbre-ir][serialisation]") {
    auto p = make_subtractive_profile();
    p.semantic_descriptors.tags = {"lead", "bright"};
    p.modulation.lfos = {{LFOWaveformType::Saw, {false, 2.0f, Beat{1,4}}, 0.0f, 0.5f, true, 0.0f, 0.0f, {}}};
    p.modulation.routings = {{{ModulationSourceType::LFO, 0}, "source.filter.cutoff", 0.3f, std::nullopt}};

    auto j1 = timbre_to_json(p);
    auto p2 = timbre_from_json(j1);
    REQUIRE(p2.has_value());
    auto j2 = timbre_to_json(*p2);

    CHECK(j1 == j2);
}
