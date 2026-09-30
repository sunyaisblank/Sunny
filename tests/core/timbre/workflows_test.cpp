/**
 * @file workflows_test.cpp
 * @brief Unit tests for Timbre IR workflow functions
 *
 *
 * Coverage: Profile creation, source swap, effect chain operations,
 *           parameter path access, macros, modulation, automation,
 *           semantic analysis, preset search/load/save, validation.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>

using namespace sunny::core;
using Catch::Approx;

// =============================================================================
// Helpers
// =============================================================================

static Envelope make_adsr(float a, float d, float sus, float r) {
    Envelope e;
    e.stages = {{a, 1.0f, EnvelopeCurve::Linear},
                {d, sus, EnvelopeCurve::Exponential},
                {r, 0.0f, EnvelopeCurve::Exponential}};
    return e;
}

// =============================================================================
// Profile Creation
// =============================================================================

TEST_CASE("create profile returns valid SubtractiveSynth default", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{10}, "Lead");
    CHECK(p.id.value == 1);
    CHECK(p.part_id.value == 10);
    CHECK(p.name == "Lead");
    CHECK(std::holds_alternative<SubtractiveSynth>(p.source.data));

    auto diags = validate(p);
    bool has_error = false;
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) has_error = true;
    }
    CHECK(!has_error);
}

// =============================================================================
// Source Configuration
// =============================================================================

TEST_CASE("set_sound_source replaces source variant", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    CHECK(std::holds_alternative<SubtractiveSynth>(p.source.data));

    FMSynth fm;
    fm.operators = {FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    auto r = set_sound_source(p, std::move(src));
    CHECK(r.has_value());
    CHECK(std::holds_alternative<FMSynth>(p.source.data));
}

// =============================================================================
// Effect Chain
// =============================================================================

TEST_CASE("add_effect appends to chain", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    CHECK(p.insert_chain.effects.empty());

    DistortionEffect dist;
    dist.drive = 0.5f;
    auto r = add_effect(p, {EffectId{1}, dist, true, 1.0f});
    CHECK(r.has_value());
    CHECK(p.insert_chain.effects.size() == 1);

    ReverbEffect reverb;
    REQUIRE(add_effect(p, {EffectId{2}, reverb, true, 0.3f}).has_value());
    CHECK(p.insert_chain.effects.size() == 2);
}

TEST_CASE("remove_effect removes by id", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f}).has_value());
    REQUIRE(add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f}).has_value());
    REQUIRE(add_effect(p, {EffectId{3}, ChorusEffect{}, true, 0.5f}).has_value());
    auto r = remove_effect(p, EffectId{2});
    CHECK(r.has_value());
    REQUIRE(p.insert_chain.effects.size() == 2);
    CHECK(p.insert_chain.effects[0].id.value == 1);
    CHECK(p.insert_chain.effects[1].id.value == 3);
}

TEST_CASE("remove_effect fails for unknown id", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f}).has_value());
    auto r = remove_effect(p, EffectId{99});
    CHECK(!r.has_value());
}

TEST_CASE("reorder_effects changes chain order", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f}).has_value());
    REQUIRE(add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f}).has_value());
    REQUIRE(add_effect(p, {EffectId{3}, ChorusEffect{}, true, 0.5f}).has_value());
    auto r = reorder_effects(p, {EffectId{3}, EffectId{1}, EffectId{2}});
    CHECK(r.has_value());
    REQUIRE(p.insert_chain.effects.size() == 3);
    CHECK(p.insert_chain.effects[0].id.value == 3);
    CHECK(p.insert_chain.effects[1].id.value == 1);
    CHECK(p.insert_chain.effects[2].id.value == 2);
}

TEST_CASE("reorder_effects rejects mismatched ids", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f}).has_value());
    REQUIRE(add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f}).has_value());
    auto r = reorder_effects(p, {EffectId{1}});
    CHECK(!r.has_value());
}

TEST_CASE("reorder_effects rejects duplicate ids", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f}).has_value());
    REQUIRE(add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f}).has_value());
    auto r = reorder_effects(p, {EffectId{1}, EffectId{1}});
    CHECK(!r.has_value());
}

TEST_CASE("reorder_effects preserves payloads when a later id is missing",
          "[timbre-ir][workflow][atomicity]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    EQEffect equalizer;
    equalizer.bands.push_back(EQBand{800.0f, -3.0f, 0.7f, EQBandType::Peak});
    REQUIRE(add_effect(profile, {EffectId{1}, equalizer, true, 0.75f}));
    REQUIRE(add_effect(profile, {EffectId{2}, ReverbEffect{}, true, 0.3f}));
    const auto before = timbre_to_json(profile);

    CHECK_FALSE(reorder_effects(profile, {EffectId{1}, EffectId{99}}));
    CHECK(timbre_to_json(profile) == before);

    REQUIRE(reorder_effects(profile, {EffectId{2}, EffectId{1}}));
    REQUIRE(profile.insert_chain.effects[1].id == EffectId{1});
    const auto& bands = std::get<EQEffect>(profile.insert_chain.effects[1].parameters).bands;
    REQUIRE(bands.size() == 1);
    CHECK(bands[0].frequency == 800.0f);
    CHECK(bands[0].gain == -3.0f);
    CHECK(bands[0].q == 0.7f);
    CHECK(profile.insert_chain.effects[1].mix == 0.75f);
}

// =============================================================================
// Parameter Path Access — SubtractiveSynth
// =============================================================================

TEST_CASE("set/get parameter — source.filter.cutoff", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    auto r = set_parameter(p, "source.filter.cutoff", 5000.0f);
    CHECK(r.has_value());

    auto v = get_parameter(p, "source.filter.cutoff");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(5000.0f));
}

TEST_CASE("device parameter mapping has explicit deterministic curve semantics",
          "[timbre-ir][workflow][mapping]") {
    DeviceParameter mapping;
    mapping.source_min = 20.0f;
    mapping.source_max = 20020.0f;
    mapping.range_min = -1.0f;
    mapping.range_max = 1.0f;

    auto linear = map_device_parameter_value(10020.0f, mapping);
    REQUIRE(linear.has_value());
    CHECK(*linear == Approx(0.0f));

    mapping.curve.type = MappingCurveType::Exponential;
    auto exponential = map_device_parameter_value(10020.0f, mapping);
    REQUIRE(exponential.has_value());
    CHECK(*exponential == Approx(-0.5f));

    mapping.curve.type = MappingCurveType::Logarithmic;
    auto logarithmic = map_device_parameter_value(5020.0f, mapping);
    REQUIRE(logarithmic.has_value());
    CHECK(*logarithmic == Approx(0.0f));

    mapping.curve = {MappingCurveType::Custom, {{0.0f, 0.0f}, {0.5f, 0.8f}, {1.0f, 1.0f}}};
    auto custom = map_device_parameter_value(5020.0f, mapping);
    REQUIRE(custom.has_value());
    CHECK(*custom == Approx(-0.2f));
}

TEST_CASE("device parameter mapping rejects implicit clamping and malformed custom curves",
          "[timbre-ir][workflow][mapping]") {
    DeviceParameter mapping;
    mapping.source_min = 20.0f;
    mapping.source_max = 20000.0f;

    auto outside = map_device_parameter_value(20001.0f, mapping);
    REQUIRE_FALSE(outside.has_value());
    CHECK(outside.error() == ErrorCode::TimbreInvalidParameter);

    mapping.curve = {MappingCurveType::Custom, {{0.0f, 0.0f}, {0.8f, 1.0f}, {0.7f, 0.5f}}};
    auto malformed = map_device_parameter_value(1000.0f, mapping);
    REQUIRE_FALSE(malformed.has_value());
    CHECK(malformed.error() == ErrorCode::TimbreInvalidParameter);
}

TEST_CASE("set/get parameter — source.filter.resonance", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    REQUIRE(set_parameter(p, "source.filter.resonance", 0.75f).has_value());
    auto v = get_parameter(p, "source.filter.resonance");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.75f));
}

TEST_CASE("set/get parameter — source.oscillators[0].tune_cents", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    REQUIRE(set_parameter(p, "source.oscillators[0].tune_cents", 7.5f).has_value());
    auto v = get_parameter(p, "source.oscillators[0].tune_cents");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(7.5f));
}

TEST_CASE("set/get parameter — source.oscillators[0].level", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    REQUIRE(set_parameter(p, "source.oscillators[0].level", 0.6f).has_value());
    auto v = get_parameter(p, "source.oscillators[0].level");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.6f));
}

TEST_CASE("parameter path rejects out-of-bounds index", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = set_parameter(p, "source.oscillators[5].level", 0.5f);
    CHECK(!r.has_value());
}

TEST_CASE("parameter path rejects unknown path", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = set_parameter(p, "nonexistent.path", 0.5f);
    CHECK(!r.has_value());
}

TEST_CASE("parameter paths use one lossless canonical grammar", "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    const auto original = *get_parameter(profile, "source.filter.cutoff");

    for (const std::string path : {
             "",
             ".source.filter.cutoff",
             "source..filter.cutoff",
             "source.filter.cutoff.",
             "source.filter.cutoff.extra",
             "source.filter.cutoff[0]",
             "source.oscillators[00].level",
             "source.oscillators[-1].level",
             "source.oscillators[0junk].level",
             "source.oscillators[18446744073709551616].level",
             "source.oscillators[0]level",
             "semantic_descriptors.not_a_field",
         }) {
        CAPTURE(path);
        CHECK_FALSE(get_parameter(profile, path).has_value());
        CHECK_FALSE(set_parameter(profile, path, 0.25f).has_value());
    }

    CHECK(*get_parameter(profile, "source.filter.cutoff") == Approx(original));
}

TEST_CASE("parameter resolver covers nested source topology exactly", "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto& subtractive = std::get<SubtractiveSynth>(profile.source.data);
    subtractive.oscillator_mix = {0.75f};
    subtractive.oscillators[0].waveform.custom_harmonics = {{1, 0.4f, 0.2f}};
    subtractive.filter.config.bandwidth = 0.8f;
    subtractive.filter.envelope = make_adsr(10.0f, 20.0f, 0.5f, 30.0f);
    subtractive.amplifier = make_adsr(10.0f, 20.0f, 0.5f, 30.0f);
    subtractive.unison = UnisonConfig{};
    subtractive.portamento = PortamentoConfig{};

    CHECK(*get_parameter(profile, "source.oscillator_mix[0]") == Approx(0.75f));
    CHECK(*get_parameter(profile, "source.oscillators[0].waveform.custom_harmonics[0].phase") ==
          Approx(0.2f));
    CHECK(*get_parameter(profile, "source.filter.config.bandwidth") == Approx(0.8f));
    CHECK(*get_parameter(profile, "source.filter.envelope.stages[1].target_level") == Approx(0.5f));
    CHECK(*get_parameter(profile, "source.amplifier.stages[0].duration") == Approx(10.0f));
    CHECK(*get_parameter(profile, "source.unison.stereo_spread") == Approx(0.0f));
    CHECK(*get_parameter(profile, "source.portamento.time") == Approx(100.0f));
    CHECK_FALSE(get_parameter(profile, "source.filter.envelope.stages[8].duration").has_value());
    CHECK_FALSE(get_parameter(profile, "source.filter.cutoff.ignored").has_value());
}

TEST_CASE("parameter resolver covers every source variant family", "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    FMOperator op;
    op.fixed_frequency = 440.0f;
    op.envelope = make_adsr(1.0f, 2.0f, 0.5f, 3.0f);
    fm.operators.push_back(op);
    fm.algorithm.custom_routing.push_back({0, 0, 0.25f});
    profile.source.data = fm;
    CHECK(*get_parameter(profile, "source.operators[0].fixed_frequency") == Approx(440.0f));
    CHECK(*get_parameter(profile, "source.operators[0].envelope.stages[2].duration") ==
          Approx(3.0f));
    CHECK(*get_parameter(profile, "source.algorithm.custom_routing[0].depth") == Approx(0.25f));

    WavetableSynth wavetable;
    wavetable.filter = Filter{};
    wavetable.amplifier = make_adsr(4.0f, 5.0f, 0.6f, 6.0f);
    profile.source.data = wavetable;
    CHECK(*get_parameter(profile, "source.position") == Approx(0.0f));
    CHECK(*get_parameter(profile, "source.filter.envelope_depth") == Approx(0.0f));
    CHECK(*get_parameter(profile, "source.amplifier.stages[2].duration") == Approx(6.0f));

    GranularSynth granular;
    granular.grain_envelope.trapezoid_attack_ratio = 0.3f;
    profile.source.data = granular;
    CHECK(*get_parameter(profile, "source.grain_envelope.trapezoid_attack_ratio") == Approx(0.3f));

    AdditiveSynth additive;
    PartialDefinition partial;
    partial.envelope = make_adsr(7.0f, 8.0f, 0.4f, 9.0f);
    additive.partials.push_back(partial);
    additive.global_envelope = make_adsr(10.0f, 11.0f, 0.3f, 12.0f);
    profile.source.data = additive;
    CHECK(*get_parameter(profile, "source.partials[0].envelope.stages[0].duration") ==
          Approx(7.0f));
    CHECK(*get_parameter(profile, "source.global_envelope.stages[2].duration") == Approx(12.0f));

    PhysicalModelSource physical;
    physical.model.bow_pressure = 0.7f;
    profile.source.data = physical;
    CHECK(*get_parameter(profile, "source.model.bow_pressure") == Approx(0.7f));

    SamplerSource sampler;
    sampler.filter = Filter{};
    sampler.envelope_override = make_adsr(13.0f, 14.0f, 0.2f, 15.0f);
    sampler.microphone_positions.push_back({"Close", 0.8f, true});
    profile.source.data = sampler;
    CHECK(*get_parameter(profile, "source.microphone_positions[0].level") == Approx(0.8f));
    CHECK(*get_parameter(profile, "source.envelope_override.stages[2].duration") == Approx(15.0f));

    HybridSource hybrid;
    auto layer = std::make_unique<SoundSourceData>();
    GranularSynth nested;
    nested.position = 0.65f;
    layer->data = nested;
    hybrid.layers.push_back(std::move(layer));
    hybrid.routing.mix_levels = {0.9f};
    profile.source.data = std::move(hybrid);
    CHECK(*get_parameter(profile, "source.layers[0].position") == Approx(0.65f));
    CHECK(*get_parameter(profile, "source.routing.mix_levels[0]") == Approx(0.9f));
}

// =============================================================================
// Parameter Path Access — FMSynth
// =============================================================================

TEST_CASE("set/get parameter — FM feedback", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}, FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    REQUIRE(set_parameter(p, "source.feedback", 0.45f).has_value());
    auto v = get_parameter(p, "source.feedback");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.45f));
}

TEST_CASE("set/get parameter — FM operator level", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}, FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    REQUIRE(set_parameter(p, "source.operators[1].level", 0.6f).has_value());
    auto v = get_parameter(p, "source.operators[1].level");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.6f));
}

// =============================================================================
// Parameter Path Access — GranularSynth
// =============================================================================

TEST_CASE("set/get parameter — granular fields", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    GranularSynth gr;
    SoundSourceData src;
    src.data = std::move(gr);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    REQUIRE(set_parameter(p, "source.grain_size", 120.0f).has_value());
    REQUIRE(set_parameter(p, "source.grain_density", 40.0f).has_value());
    REQUIRE(set_parameter(p, "source.position", 0.7f).has_value());
    CHECK(*get_parameter(p, "source.grain_size") == Approx(120.0f));
    CHECK(*get_parameter(p, "source.grain_density") == Approx(40.0f));
    CHECK(*get_parameter(p, "source.position") == Approx(0.7f));
}

// =============================================================================
// Parameter Path Access — Effects
// =============================================================================

TEST_CASE("set/get parameter — effect mix and params", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ReverbEffect reverb;
    reverb.decay_time = 2.0f;
    REQUIRE(add_effect(p, {EffectId{1}, reverb, true, 0.5f}).has_value());
    REQUIRE(set_parameter(p, "insert_chain.effects[0].mix", 0.8f).has_value());
    CHECK(*get_parameter(p, "insert_chain.effects[0].mix") == Approx(0.8f));

    REQUIRE(set_parameter(p, "insert_chain.effects[0].decay", 3.5f).has_value());
    CHECK(*get_parameter(p, "insert_chain.effects[0].decay") == Approx(3.5f));
}

TEST_CASE("parameter resolver covers nested effect families", "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    DistortionEffect distortion;
    distortion.algorithm.ring_mod_frequency = 330.0f;
    profile.insert_chain.effects.push_back({EffectId{1}, distortion, true, 0.7f});

    DelayEffect delay;
    delay.filter = Filter{};
    delay.delay_time.ms = 250.0f;
    profile.insert_chain.effects.push_back({EffectId{2}, delay, true, 0.6f});

    EQEffect eq;
    eq.bands.push_back({800.0f, -3.0f, 1.2f, EQBandType::Peak});
    profile.insert_chain.effects.push_back({EffectId{3}, eq, true, 0.5f});

    CompressorEffect compressor;
    compressor.sidechain = SidechainConfig{PartId{2}, Filter{}};
    profile.insert_chain.effects.push_back({EffectId{4}, compressor, true, 0.4f});

    ReverbEffect reverb;
    reverb.early_reflections_level = 0.35f;
    profile.insert_chain.effects.push_back({EffectId{5}, reverb, true, 0.3f});

    CHECK(*get_parameter(profile, "insert_chain.effects[0].algorithm.ring_mod_frequency") ==
          Approx(330.0f));
    CHECK(*get_parameter(profile, "insert_chain.effects[1].delay_time.ms") == Approx(250.0f));
    CHECK(*get_parameter(profile, "insert_chain.effects[1].filter.cutoff") == Approx(1000.0f));
    CHECK(*get_parameter(profile, "insert_chain.effects[2].bands[0].gain") == Approx(-3.0f));
    CHECK(*get_parameter(profile, "insert_chain.effects[3].sidechain.filter.resonance") ==
          Approx(0.0f));
    CHECK(*get_parameter(profile, "insert_chain.effects[4].early_reflections_level") ==
          Approx(0.35f));
    CHECK_FALSE(get_parameter(profile, "insert_chain.effects[2].bands[1].gain").has_value());
    CHECK_FALSE(get_parameter(profile, "insert_chain.effects[4].decay_time.extra").has_value());
}

// =============================================================================
// Parameter Path Access — Semantic Descriptors
// =============================================================================

TEST_CASE("set/get parameter — semantic descriptors by path", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    REQUIRE(set_parameter(p, "semantic_descriptors.brightness", 0.9f).has_value());
    REQUIRE(set_parameter(p, "semantic_descriptors.warmth", 0.2f).has_value());
    CHECK(*get_parameter(p, "semantic_descriptors.brightness") == Approx(0.9f));
    CHECK(*get_parameter(p, "semantic_descriptors.warmth") == Approx(0.2f));
}

// =============================================================================
// Parameter Path Access — Modulation LFO
// =============================================================================

TEST_CASE("set/get parameter — modulation LFO rate", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos = {LFO{}};

    REQUIRE(set_parameter(p, "modulation.lfos[0].rate.hz", 4.5f).has_value());
    CHECK(*get_parameter(p, "modulation.lfos[0].rate.hz") == Approx(4.5f));

    REQUIRE(set_parameter(p, "modulation.lfos[0].phase", 0.5f).has_value());
    CHECK(*get_parameter(p, "modulation.lfos[0].phase") == Approx(0.5f));
}

TEST_CASE("parameter resolver covers modulation sources and macro curves",
          "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    LFO lfo;
    lfo.custom_points = {{0.0f, -1.0f}, {1.0f, 1.0f}};
    profile.modulation.lfos.push_back(lfo);

    StepSequencer sequencer;
    sequencer.steps = {0.1f, 0.9f};
    sequencer.smooth = 0.25f;
    profile.modulation.step_sequencers.push_back(sequencer);

    MacroKnob macro;
    macro.value = 0.4f;
    MacroMapping mapping;
    mapping.min = 0.2f;
    mapping.max = 0.8f;
    mapping.curve.type = MappingCurveType::Custom;
    mapping.curve.custom_points = {{0.0f, 0.0f}, {1.0f, 1.0f}};
    macro.mappings.push_back(mapping);
    profile.modulation.macro_knobs.push_back(macro);

    CHECK(*get_parameter(profile, "modulation.lfos[0].custom_points[1].y") == Approx(1.0f));
    CHECK(*get_parameter(profile, "modulation.step_sequencers[0].steps[1]") == Approx(0.9f));
    CHECK(*get_parameter(profile, "modulation.step_sequencers[0].smooth") == Approx(0.25f));
    CHECK(*get_parameter(profile, "modulation.macro_knobs[0].mappings[0].min") == Approx(0.2f));
    CHECK(
        *get_parameter(profile, "modulation.macro_knobs[0].mappings[0].curve.custom_points[1].x") ==
        Approx(1.0f));
    CHECK_FALSE(get_parameter(profile, "modulation.lfos[0].custom_points[1].z").has_value());
}

// =============================================================================
// Macro Knobs
// =============================================================================

TEST_CASE("owned modulation generators use checked stable u8 indices", "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    LFO lfo;
    lfo.waveform = LFOWaveformType::Custom;
    lfo.custom_points = {{0.0f, -1.0f}, {1.0f, 1.0f}};
    const auto lfo_index = create_lfo(profile, lfo);
    REQUIRE(lfo_index);
    CHECK(*lfo_index == 0);

    const auto envelope_index =
        create_modulation_envelope(profile, make_adsr(10.0f, 20.0f, 0.5f, 30.0f));
    REQUIRE(envelope_index);
    CHECK(*envelope_index == 0);

    StepSequencer sequencer;
    sequencer.steps = {0.0f, 0.5f, 1.0f};
    const auto step_index = create_step_sequencer(profile, sequencer);
    REQUIRE(step_index);
    CHECK(*step_index == 0);

    CHECK(profile.modulation.lfos.size() == 1);
    CHECK(profile.modulation.envelopes.size() == 1);
    CHECK(profile.modulation.step_sequencers.size() == 1);
}

TEST_CASE("owned modulation generator creation is atomic on invalid definitions",
          "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    LFO invalid_lfo;
    invalid_lfo.rate.hz = 0.0f;
    CHECK_FALSE(create_lfo(profile, invalid_lfo));

    Envelope invalid_envelope;
    CHECK_FALSE(create_modulation_envelope(profile, invalid_envelope));

    StepSequencer invalid_step;
    CHECK_FALSE(create_step_sequencer(profile, invalid_step));

    CHECK(profile.modulation.lfos.empty());
    CHECK(profile.modulation.envelopes.empty());
    CHECK(profile.modulation.step_sequencers.empty());
}

TEST_CASE("create_macro and set_macro", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    MacroKnob macro;
    macro.index = 0;
    macro.name = "Brightness";
    macro.value = 0.5f;

    auto r = create_macro(p, macro);
    CHECK(r.has_value());
    REQUIRE(p.modulation.macro_knobs.size() == 1);

    auto r2 = set_macro(p, 0, 0.8f);
    CHECK(r2.has_value());
    CHECK(p.modulation.macro_knobs[0].value == Approx(0.8f));
}

TEST_CASE("set_macro fails for unknown index", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = set_macro(p, 99, 0.5f);
    CHECK(!r.has_value());
}

TEST_CASE("macro workflows reject malformed definitions and values atomically",
          "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    MacroKnob macro;
    macro.index = 0;
    macro.name = "Broken";
    macro.value = std::numeric_limits<float>::infinity();
    CHECK_FALSE(create_macro(profile, macro));
    CHECK(profile.modulation.macro_knobs.empty());

    macro.value = 0.5f;
    REQUIRE(create_macro(profile, macro));
    CHECK_FALSE(set_macro(profile, 0, -0.1f));
    CHECK(profile.modulation.macro_knobs.front().value == Approx(0.5f));
}

TEST_CASE("create_macro rejects duplicate index", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    MacroKnob m1;
    m1.index = 0;
    m1.name = "Brightness";
    auto r1 = create_macro(p, m1);
    CHECK(r1.has_value());

    MacroKnob m2;
    m2.index = 0;
    m2.name = "Warmth";
    auto r2 = create_macro(p, m2);
    CHECK(!r2.has_value());
    CHECK(p.modulation.macro_knobs.size() == 1);
}

// =============================================================================
// Modulation
// =============================================================================

TEST_CASE("add_modulation adds routing", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos.push_back(LFO{}); // source index 0 must exist

    ModulationRouting route;
    route.source = {ModulationSourceType::LFO, 0};
    route.target = "source.filter.cutoff";
    route.depth = 0.5f;

    auto r = add_modulation(p, route);
    CHECK(r.has_value());
    REQUIRE(p.modulation.routings.size() == 1);
    CHECK(p.modulation.routings[0].target == "source.filter.cutoff");
}

TEST_CASE("add_modulation rejects invalid source index", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    // No LFOs added — source index 0 is invalid

    ModulationRouting route;
    route.source = {ModulationSourceType::LFO, 0};
    route.target = "source.filter.cutoff";
    route.depth = 0.5f;

    auto r = add_modulation(p, route);
    CHECK(!r.has_value());
}

TEST_CASE("add_modulation rejects invalid target path", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ModulationRouting route;
    route.source = {ModulationSourceType::Velocity, 0};
    route.target = "nonexistent.path";
    route.depth = 0.5f;

    auto r = add_modulation(p, route);
    CHECK(!r.has_value());
}

TEST_CASE("add_modulation rejects out-of-range depth", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ModulationRouting route;
    route.source = {ModulationSourceType::Velocity, 0};
    route.target = "source.filter.cutoff";
    route.depth = 1.5f;

    auto r = add_modulation(p, route);
    CHECK(!r.has_value());
}

TEST_CASE("add_modulation atomically enforces source algebra and finite depth",
          "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    ModulationRouting routing{{ModulationSourceType::Velocity, 0}, "source.filter.cutoff", 0.5f};
    const auto rejected = [&] {
        CHECK_FALSE(add_modulation(profile, routing));
        CHECK(profile.modulation.routings.empty());
    };

    SECTION("non-finite depth") {
        routing.depth = std::numeric_limits<float>::quiet_NaN();
        rejected();
    }
    SECTION("undefined type") {
        routing.source.type = static_cast<ModulationSourceType>(255);
        rejected();
    }
    SECTION("non-canonical built-in payload") {
        routing.source.index = 1;
        rejected();
    }
    SECTION("CC beyond MIDI domain") {
        routing.source = {ModulationSourceType::CC, 0, 128};
        rejected();
    }
    SECTION("self-following audio source") {
        routing.source = {ModulationSourceType::AudioFollower, 0, 0, profile.part_id};
        rejected();
    }
    SECTION("missing via source") {
        routing.via = ModulationSource{ModulationSourceType::LFO, 0};
        rejected();
    }
    SECTION("declared modulation envelope") {
        profile.modulation.envelopes.push_back(make_adsr(10.0f, 20.0f, 0.5f, 30.0f));
        routing.source = {ModulationSourceType::Envelope, 0};
        CHECK(add_modulation(profile, routing));
        REQUIRE(profile.modulation.routings.size() == 1);
        CHECK(profile.modulation.routings.front().source.type == ModulationSourceType::Envelope);
    }
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("add_automation appends breakpoints", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "source.filter.cutoff";
    auto1.breakpoints = {{ScoreTime{1, Beat{0, 1}}, 2000.0f}, {ScoreTime{4, Beat{0, 1}}, 8000.0f}};
    auto1.interpolation = AutomationInterpolation::Smooth;

    auto r = add_automation(p, auto1);
    CHECK(r.has_value());
    REQUIRE(p.parameter_automation.size() == 1);
}

TEST_CASE("add_automation rejects empty breakpoints", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "source.filter.cutoff";
    // breakpoints left empty

    auto r = add_automation(p, auto1);
    CHECK(!r.has_value());
}

TEST_CASE("add_automation rejects invalid parameter path", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "nonexistent.param";
    auto1.breakpoints = {{ScoreTime{1, Beat{0, 1}}, 0.5f}, {ScoreTime{2, Beat{0, 1}}, 1.0f}};

    auto r = add_automation(p, auto1);
    CHECK(!r.has_value());
}

TEST_CASE("add_automation atomically enforces the complete lane invariant",
          "[timbre-ir][workflow]") {
    auto profile = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    const auto unchanged = [&] { CHECK(profile.parameter_automation.empty()); };

    TimbreAutomation automation;
    automation.parameter_path = "source.filter.cutoff";
    automation.breakpoints = {{SCORE_START, 1000.0f}, {ScoreTime{2, Beat::zero()}, 2000.0f}};

    SECTION("breakpoint before score start") {
        automation.breakpoints[0].time = ScoreTime{0, Beat::zero()};
        CHECK_FALSE(add_automation(profile, automation));
        unchanged();
    }
    SECTION("non-finite value") {
        automation.breakpoints[1].value = std::numeric_limits<float>::infinity();
        CHECK_FALSE(add_automation(profile, automation));
        unchanged();
    }
    SECTION("duplicate time") {
        automation.breakpoints[1].time = automation.breakpoints[0].time;
        CHECK_FALSE(add_automation(profile, automation));
        unchanged();
    }
    SECTION("descending time") {
        automation.breakpoints = {{ScoreTime{3, Beat::zero()}, 1000.0f},
                                  {ScoreTime{2, Beat::zero()}, 2000.0f}};
        CHECK_FALSE(add_automation(profile, automation));
        unchanged();
    }
    SECTION("undefined interpolation") {
        automation.interpolation = static_cast<AutomationInterpolation>(255);
        CHECK_FALSE(add_automation(profile, automation));
        unchanged();
    }
    SECTION("one finite breakpoint at score start is a valid constant lane") {
        automation.breakpoints.resize(1);
        CHECK(add_automation(profile, automation));
        REQUIRE(profile.parameter_automation.size() == 1);
        CHECK(profile.parameter_automation.front().breakpoints.front().time == SCORE_START);
    }
}

TEST_CASE("morph_presets rejects degenerate interval", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{4, Beat{0, 1}};
    morph.end = ScoreTime{2, Beat{0, 1}}; // end before start

    auto r = morph_presets(p, morph);
    CHECK(!r.has_value());
}

TEST_CASE("morph_presets accepts valid interval", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{1, Beat{0, 1}};
    morph.end = ScoreTime{4, Beat{0, 1}};

    auto r = morph_presets(p, morph);
    CHECK(r.has_value());
    CHECK(p.preset_morphs.size() == 1);
}

// =============================================================================
// Semantic Descriptors
// =============================================================================

TEST_CASE("set_semantic_descriptors replaces descriptors", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    SemanticTimbreDescriptor d;
    d.brightness = 0.9f;
    d.warmth = 0.1f;
    d.tags = {"bright", "sharp"};

    set_semantic_descriptors(p, d);
    CHECK(p.semantic_descriptors.brightness == Approx(0.9f));
    CHECK(p.semantic_descriptors.tags.size() == 2);
}

// =============================================================================
// Timbre Analysis
// =============================================================================

TEST_CASE("analyze_timbre returns ParameterDerived descriptors", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto d = analyze_timbre(p);
    CHECK(d.derivation == DerivationMode::ParameterDerived);
}

TEST_CASE("analyze_timbre — bright subtractive has high brightness", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(set_parameter(p, "source.filter.cutoff", 18000.0f).has_value());
    REQUIRE(set_parameter(p, "source.filter.resonance", 0.8f).has_value());
    auto d = analyze_timbre(p);
    CHECK(d.brightness > 0.5f);
}

TEST_CASE("analyze_timbre — dark subtractive has low brightness", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(set_parameter(p, "source.filter.cutoff", 200.0f).has_value());
    REQUIRE(set_parameter(p, "source.filter.resonance", 0.0f).has_value());
    auto d = analyze_timbre(p);
    CHECK(d.brightness < 0.3f);
}

TEST_CASE("analyze_timbre — FM with high feedback is bright and rough", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}};
    fm.feedback = 0.9f;
    SoundSourceData src;
    src.data = std::move(fm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    auto d = analyze_timbre(p);
    CHECK(d.brightness > 0.6f);
    CHECK(d.roughness > 0.5f);
}

TEST_CASE("analyze_timbre — physical model sets attack from exciter", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PhysicalModelSource pm;
    pm.exciter.type = ExciterType::Bow;
    SoundSourceData src;
    src.data = std::move(pm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    auto d = analyze_timbre(p);
    CHECK(d.attack_character == AttackCharacter::Bowed);
    CHECK(d.sustain_character == SustainCharacter::Steady);
}

TEST_CASE("analyze_timbre — modulation increases movement", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos = {LFO{}};
    p.modulation.routings = {{{ModulationSourceType::LFO, 0}, "source.filter.cutoff", 0.8f}};

    auto d = analyze_timbre(p);
    CHECK(d.movement > 0.0f);
}

// =============================================================================
// Preset Search
// =============================================================================

TEST_CASE("search_presets filters by tag", "[timbre-ir][workflow]") {
    std::vector<TimbrePreset> library;

    TimbrePreset p1;
    p1.id = TimbrePresetId{1};
    p1.name = "Warm Pad";
    p1.tags = {"pad", "warm"};
    p1.semantic_descriptors.brightness = 0.3f;
    library.push_back(p1);

    TimbrePreset p2;
    p2.id = TimbrePresetId{2};
    p2.name = "Bright Lead";
    p2.tags = {"lead", "bright"};
    p2.semantic_descriptors.brightness = 0.9f;
    library.push_back(p2);

    TimbrePreset p3;
    p3.id = TimbrePresetId{3};
    p3.name = "Dark Pad";
    p3.tags = {"pad", "dark"};
    p3.semantic_descriptors.brightness = 0.1f;
    library.push_back(p3);

    PresetSearchQuery q;
    q.required_tags = {"pad"};
    auto results = search_presets(library, q);
    REQUIRE(results.size() == 2);
    // Both pads should appear
    bool found_warm = false, found_dark = false;
    for (const auto& r : results) {
        if (r.name == "Warm Pad") found_warm = true;
        if (r.name == "Dark Pad") found_dark = true;
    }
    CHECK(found_warm);
    CHECK(found_dark);
}

TEST_CASE("search_presets filters by name", "[timbre-ir][workflow]") {
    std::vector<TimbrePreset> library;

    TimbrePreset p1;
    p1.id = TimbrePresetId{1};
    p1.name = "Warm Pad";
    p1.tags = {"pad"};
    library.push_back(p1);

    TimbrePreset p2;
    p2.id = TimbrePresetId{2};
    p2.name = "Bright Lead";
    p2.tags = {"lead"};
    library.push_back(p2);

    PresetSearchQuery q;
    q.name_contains = "Bright";
    auto results = search_presets(library, q);
    REQUIRE(results.size() == 1);
    CHECK(results[0].name == "Bright Lead");
}

TEST_CASE("search_presets filters by brightness range", "[timbre-ir][workflow]") {
    std::vector<TimbrePreset> library;

    TimbrePreset p1;
    p1.id = TimbrePresetId{1};
    p1.name = "Dark";
    p1.semantic_descriptors.brightness = 0.1f;
    library.push_back(p1);

    TimbrePreset p2;
    p2.id = TimbrePresetId{2};
    p2.name = "Medium";
    p2.semantic_descriptors.brightness = 0.5f;
    library.push_back(p2);

    TimbrePreset p3;
    p3.id = TimbrePresetId{3};
    p3.name = "Bright";
    p3.semantic_descriptors.brightness = 0.9f;
    library.push_back(p3);

    PresetSearchQuery q;
    q.min_brightness = 0.4f;
    q.max_brightness = 0.6f;
    auto results = search_presets(library, q);
    REQUIRE(results.size() == 1);
    CHECK(results[0].name == "Medium");
}

TEST_CASE("search_presets respects max_results", "[timbre-ir][workflow]") {
    std::vector<TimbrePreset> library;
    for (int i = 0; i < 20; ++i) {
        TimbrePreset p;
        p.id = TimbrePresetId{static_cast<std::uint64_t>(i)};
        p.name = "Preset " + std::to_string(i);
        library.push_back(p);
    }

    PresetSearchQuery q;
    q.max_results = 5;
    auto results = search_presets(library, q);
    CHECK(results.size() == 5);
}

// =============================================================================
// Preset Load/Save
// =============================================================================

TEST_CASE("save_preset captures current parameters", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(set_parameter(p, "source.filter.cutoff", 5000.0f).has_value());
    p.semantic_descriptors.brightness = 0.8f;

    auto preset = save_preset(p, TimbrePresetId{100}, "My Preset");
    CHECK(preset.id.value == 100);
    CHECK(preset.name == "My Preset");
    CHECK(preset.semantic_descriptors.brightness == Approx(0.8f));
    CHECK(preset.parameter_state.count("source.filter.cutoff") == 1);
    CHECK(preset.parameter_state.at("source.filter.cutoff") == Approx(5000.0f));
}

TEST_CASE("save_preset captures FM source parameters", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "FM Test");

    FMSynth fm;
    fm.feedback = 0.4f;
    FMOperator op0;
    op0.ratio = 2.0f;
    op0.level = 0.8f;
    op0.detune = 5.0f;
    FMOperator op1;
    op1.ratio = 3.0f;
    op1.level = 0.5f;
    op1.detune = 0.0f;
    fm.operators = {op0, op1};
    SoundSourceData src;
    src.data = std::move(fm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    auto preset = save_preset(p, TimbrePresetId{200}, "FM Preset");
    CHECK(preset.parameter_state.count("source.feedback") == 1);
    CHECK(preset.parameter_state.at("source.feedback") == Approx(0.4f));
    CHECK(preset.parameter_state.count("source.operators[0].ratio") == 1);
    CHECK(preset.parameter_state.at("source.operators[0].ratio") == Approx(2.0f));
    CHECK(preset.parameter_state.count("source.operators[1].level") == 1);
    CHECK(preset.parameter_state.at("source.operators[1].level") == Approx(0.5f));
}

TEST_CASE("load_preset applies saved parameters", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    REQUIRE(set_parameter(p, "source.filter.cutoff", 5000.0f).has_value());
    REQUIRE(set_parameter(p, "source.filter.resonance", 0.7f).has_value());
    p.semantic_descriptors.brightness = 0.8f;

    auto preset = save_preset(p, TimbrePresetId{100}, "Saved");

    // Reset profile parameters
    REQUIRE(set_parameter(p, "source.filter.cutoff", 1000.0f).has_value());
    REQUIRE(set_parameter(p, "source.filter.resonance", 0.0f).has_value());
    // Load preset
    auto r = load_preset(p, preset);
    CHECK(r.has_value());
    CHECK(*get_parameter(p, "source.filter.cutoff") == Approx(5000.0f));
    CHECK(*get_parameter(p, "source.filter.resonance") == Approx(0.7f));
    CHECK(p.semantic_descriptors.brightness == Approx(0.8f));
}

// =============================================================================
// Preset Morph
// =============================================================================

TEST_CASE("morph_presets adds morph entry", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{1, Beat{0, 1}};
    morph.end = ScoreTime{4, Beat{0, 1}};
    morph.curve.type = MappingCurveType::SCurve;

    auto r = morph_presets(p, morph);
    CHECK(r.has_value());
    REQUIRE(p.preset_morphs.size() == 1);
    CHECK(p.preset_morphs[0].from_preset.value == 1);
}

// =============================================================================
// Validation wrapper
// =============================================================================

TEST_CASE("validate delegates to validate_timbre", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto diags = validate(p);
    bool has_error = false;
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) has_error = true;
    }
    CHECK(!has_error);
}

TEST_CASE("validate catches T2 violation", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    std::get<SubtractiveSynth>(p.source.data).oscillators.clear();

    auto diags = validate(p);
    bool has_t2 = false;
    for (const auto& d : diags) {
        if (d.rule == "T2") has_t2 = true;
    }
    CHECK(has_t2);
}

// =============================================================================
// Parameter Path Access — Additive partials
// =============================================================================

TEST_CASE("set/get parameter — additive partial amplitude", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    AdditiveSynth add;
    add.partials = {{1.0f, 1.0f, 0.0f, std::nullopt, 0.0f}, {2.0f, 0.5f, 0.0f, std::nullopt, 0.0f}};
    add.global_envelope = make_adsr(20.0f, 100.0f, 0.8f, 200.0f);
    SoundSourceData src;
    src.data = std::move(add);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    REQUIRE(set_parameter(p, "source.partials[1].amplitude", 0.25f).has_value());
    CHECK(*get_parameter(p, "source.partials[1].amplitude") == Approx(0.25f));
}

// =============================================================================
// Parameter Path Access — PhysicalModel
// =============================================================================

TEST_CASE("set/get parameter — physical model fields", "[timbre-ir][workflow]") {
    auto p = create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PhysicalModelSource pm;
    SoundSourceData src;
    src.data = std::move(pm);
    REQUIRE(set_sound_source(p, std::move(src)).has_value());
    REQUIRE(set_parameter(p, "source.coupling", 0.7f).has_value());
    REQUIRE(set_parameter(p, "source.damping", 0.3f).has_value());
    REQUIRE(set_parameter(p, "source.exciter.brightness", 0.9f).has_value());
    REQUIRE(set_parameter(p, "source.resonator.decay", 0.6f).has_value());
    CHECK(*get_parameter(p, "source.coupling") == Approx(0.7f));
    CHECK(*get_parameter(p, "source.damping") == Approx(0.3f));
    CHECK(*get_parameter(p, "source.exciter.brightness") == Approx(0.9f));
    CHECK(*get_parameter(p, "source.resonator.decay") == Approx(0.6f));
}
