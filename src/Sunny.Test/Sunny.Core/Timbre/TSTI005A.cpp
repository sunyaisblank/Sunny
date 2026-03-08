/**
 * @file TSTI005A.cpp
 * @brief Unit tests for Timbre IR workflow functions (TIWF001A)
 *
 * Component: TSTI005A
 * Domain: TS (Test) | Category: TI (Timbre IR)
 *
 * Tests: TIWF001A
 * Coverage: Profile creation, source swap, effect chain operations,
 *           parameter path access, macros, modulation, automation,
 *           semantic analysis, preset search/load/save, validation.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Timbre/TIWF001A.h"

using namespace Sunny::Core;
using Catch::Approx;

// =============================================================================
// Helpers
// =============================================================================

static Envelope make_adsr(float a, float d, float sus, float r) {
    Envelope e;
    e.stages = {
        {a, 1.0f, EnvelopeCurve::Linear},
        {d, sus, EnvelopeCurve::Exponential},
        {r, 0.0f, EnvelopeCurve::Exponential}
    };
    return e;
}

// =============================================================================
// Profile Creation
// =============================================================================

TEST_CASE("TIWF001A: create profile returns valid SubtractiveSynth default", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{10}, "Lead");
    CHECK(p.id.value == 1);
    CHECK(p.part_id.value == 10);
    CHECK(p.name == "Lead");
    CHECK(std::holds_alternative<SubtractiveSynth>(p.source.data));

    auto diags = wf_validate(p);
    bool has_error = false;
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) has_error = true;
    }
    CHECK(!has_error);
}

// =============================================================================
// Source Configuration
// =============================================================================

TEST_CASE("TIWF001A: set_sound_source replaces source variant", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    CHECK(std::holds_alternative<SubtractiveSynth>(p.source.data));

    FMSynth fm;
    fm.operators = {FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    auto r = wf_set_sound_source(p, std::move(src));
    CHECK(r.has_value());
    CHECK(std::holds_alternative<FMSynth>(p.source.data));
}

// =============================================================================
// Effect Chain
// =============================================================================

TEST_CASE("TIWF001A: add_effect appends to chain", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    CHECK(p.insert_chain.effects.empty());

    DistortionEffect dist;
    dist.drive = 0.5f;
    auto r = wf_add_effect(p, {EffectId{1}, dist, true, 1.0f});
    CHECK(r.has_value());
    CHECK(p.insert_chain.effects.size() == 1);

    ReverbEffect reverb;
    wf_add_effect(p, {EffectId{2}, reverb, true, 0.3f});
    CHECK(p.insert_chain.effects.size() == 2);
}

TEST_CASE("TIWF001A: remove_effect removes by id", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f});
    wf_add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f});
    wf_add_effect(p, {EffectId{3}, ChorusEffect{}, true, 0.5f});

    auto r = wf_remove_effect(p, EffectId{2});
    CHECK(r.has_value());
    REQUIRE(p.insert_chain.effects.size() == 2);
    CHECK(p.insert_chain.effects[0].id.value == 1);
    CHECK(p.insert_chain.effects[1].id.value == 3);
}

TEST_CASE("TIWF001A: remove_effect fails for unknown id", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f});

    auto r = wf_remove_effect(p, EffectId{99});
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: reorder_effects changes chain order", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f});
    wf_add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f});
    wf_add_effect(p, {EffectId{3}, ChorusEffect{}, true, 0.5f});

    auto r = wf_reorder_effects(p, {EffectId{3}, EffectId{1}, EffectId{2}});
    CHECK(r.has_value());
    REQUIRE(p.insert_chain.effects.size() == 3);
    CHECK(p.insert_chain.effects[0].id.value == 3);
    CHECK(p.insert_chain.effects[1].id.value == 1);
    CHECK(p.insert_chain.effects[2].id.value == 2);
}

TEST_CASE("TIWF001A: reorder_effects rejects mismatched ids", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f});
    wf_add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f});

    auto r = wf_reorder_effects(p, {EffectId{1}});
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: reorder_effects rejects duplicate ids", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_add_effect(p, {EffectId{1}, DistortionEffect{}, true, 1.0f});
    wf_add_effect(p, {EffectId{2}, ReverbEffect{}, true, 0.3f});

    auto r = wf_reorder_effects(p, {EffectId{1}, EffectId{1}});
    CHECK(!r.has_value());
}

// =============================================================================
// Parameter Path Access — SubtractiveSynth
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — source.filter.cutoff", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    auto r = wf_set_parameter(p, "source.filter.cutoff", 5000.0f);
    CHECK(r.has_value());

    auto v = wf_get_parameter(p, "source.filter.cutoff");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(5000.0f));
}

TEST_CASE("TIWF001A: set/get parameter — source.filter.resonance", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    wf_set_parameter(p, "source.filter.resonance", 0.75f);
    auto v = wf_get_parameter(p, "source.filter.resonance");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.75f));
}

TEST_CASE("TIWF001A: set/get parameter — source.oscillators[0].tune_cents", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    wf_set_parameter(p, "source.oscillators[0].tune_cents", 7.5f);
    auto v = wf_get_parameter(p, "source.oscillators[0].tune_cents");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(7.5f));
}

TEST_CASE("TIWF001A: set/get parameter — source.oscillators[0].level", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    wf_set_parameter(p, "source.oscillators[0].level", 0.6f);
    auto v = wf_get_parameter(p, "source.oscillators[0].level");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.6f));
}

TEST_CASE("TIWF001A: parameter path rejects out-of-bounds index", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = wf_set_parameter(p, "source.oscillators[5].level", 0.5f);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: parameter path rejects unknown path", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = wf_set_parameter(p, "nonexistent.path", 0.5f);
    CHECK(!r.has_value());
}

// =============================================================================
// Parameter Path Access — FMSynth
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — FM feedback", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}, FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    wf_set_sound_source(p, std::move(src));

    wf_set_parameter(p, "source.feedback", 0.45f);
    auto v = wf_get_parameter(p, "source.feedback");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.45f));
}

TEST_CASE("TIWF001A: set/get parameter — FM operator level", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}, FMOperator{}};
    SoundSourceData src;
    src.data = std::move(fm);
    wf_set_sound_source(p, std::move(src));

    wf_set_parameter(p, "source.operators[1].level", 0.6f);
    auto v = wf_get_parameter(p, "source.operators[1].level");
    REQUIRE(v.has_value());
    CHECK(*v == Approx(0.6f));
}

// =============================================================================
// Parameter Path Access — GranularSynth
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — granular fields", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    GranularSynth gr;
    SoundSourceData src;
    src.data = std::move(gr);
    wf_set_sound_source(p, std::move(src));

    wf_set_parameter(p, "source.grain_size", 120.0f);
    wf_set_parameter(p, "source.grain_density", 40.0f);
    wf_set_parameter(p, "source.position", 0.7f);

    CHECK(*wf_get_parameter(p, "source.grain_size") == Approx(120.0f));
    CHECK(*wf_get_parameter(p, "source.grain_density") == Approx(40.0f));
    CHECK(*wf_get_parameter(p, "source.position") == Approx(0.7f));
}

// =============================================================================
// Parameter Path Access — Effects
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — effect mix and params", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ReverbEffect reverb;
    reverb.decay_time = 2.0f;
    wf_add_effect(p, {EffectId{1}, reverb, true, 0.5f});

    wf_set_parameter(p, "insert_chain.effects[0].mix", 0.8f);
    CHECK(*wf_get_parameter(p, "insert_chain.effects[0].mix") == Approx(0.8f));

    wf_set_parameter(p, "insert_chain.effects[0].decay", 3.5f);
    CHECK(*wf_get_parameter(p, "insert_chain.effects[0].decay") == Approx(3.5f));
}

// =============================================================================
// Parameter Path Access — Semantic Descriptors
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — semantic descriptors by path", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    wf_set_parameter(p, "semantic_descriptors.brightness", 0.9f);
    wf_set_parameter(p, "semantic_descriptors.warmth", 0.2f);

    CHECK(*wf_get_parameter(p, "semantic_descriptors.brightness") == Approx(0.9f));
    CHECK(*wf_get_parameter(p, "semantic_descriptors.warmth") == Approx(0.2f));
}

// =============================================================================
// Parameter Path Access — Modulation LFO
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — modulation LFO rate", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos = {LFO{}};

    wf_set_parameter(p, "modulation.lfos[0].rate.hz", 4.5f);
    CHECK(*wf_get_parameter(p, "modulation.lfos[0].rate.hz") == Approx(4.5f));

    wf_set_parameter(p, "modulation.lfos[0].phase", 0.5f);
    CHECK(*wf_get_parameter(p, "modulation.lfos[0].phase") == Approx(0.5f));
}

// =============================================================================
// Macro Knobs
// =============================================================================

TEST_CASE("TIWF001A: create_macro and set_macro", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    MacroKnob macro;
    macro.index = 0;
    macro.name = "Brightness";
    macro.value = 0.5f;

    auto r = wf_create_macro(p, macro);
    CHECK(r.has_value());
    REQUIRE(p.modulation.macro_knobs.size() == 1);

    auto r2 = wf_set_macro(p, 0, 0.8f);
    CHECK(r2.has_value());
    CHECK(p.modulation.macro_knobs[0].value == Approx(0.8f));
}

TEST_CASE("TIWF001A: set_macro fails for unknown index", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto r = wf_set_macro(p, 99, 0.5f);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: create_macro rejects duplicate index", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    MacroKnob m1;
    m1.index = 0;
    m1.name = "Brightness";
    auto r1 = wf_create_macro(p, m1);
    CHECK(r1.has_value());

    MacroKnob m2;
    m2.index = 0;
    m2.name = "Warmth";
    auto r2 = wf_create_macro(p, m2);
    CHECK(!r2.has_value());
    CHECK(p.modulation.macro_knobs.size() == 1);
}

// =============================================================================
// Modulation
// =============================================================================

TEST_CASE("TIWF001A: add_modulation adds routing", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos.push_back(LFO{});  // source index 0 must exist

    ModulationRouting route;
    route.source = {ModulationSourceType::LFO, 0};
    route.target = "source.filter.cutoff";
    route.depth = 0.5f;

    auto r = wf_add_modulation(p, route);
    CHECK(r.has_value());
    REQUIRE(p.modulation.routings.size() == 1);
    CHECK(p.modulation.routings[0].target == "source.filter.cutoff");
}

TEST_CASE("TIWF001A: add_modulation rejects invalid source index", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    // No LFOs added — source index 0 is invalid

    ModulationRouting route;
    route.source = {ModulationSourceType::LFO, 0};
    route.target = "source.filter.cutoff";
    route.depth = 0.5f;

    auto r = wf_add_modulation(p, route);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: add_modulation rejects invalid target path", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ModulationRouting route;
    route.source = {ModulationSourceType::Velocity, 0};
    route.target = "nonexistent.path";
    route.depth = 0.5f;

    auto r = wf_add_modulation(p, route);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: add_modulation rejects out-of-range depth", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    ModulationRouting route;
    route.source = {ModulationSourceType::Velocity, 0};
    route.target = "source.filter.cutoff";
    route.depth = 1.5f;

    auto r = wf_add_modulation(p, route);
    CHECK(!r.has_value());
}

// =============================================================================
// Automation
// =============================================================================

TEST_CASE("TIWF001A: add_automation appends breakpoints", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "source.filter.cutoff";
    auto1.breakpoints = {
        {ScoreTime{1, Beat{0, 1}}, 2000.0f},
        {ScoreTime{4, Beat{0, 1}}, 8000.0f}
    };
    auto1.interpolation = AutomationInterpolation::Smooth;

    auto r = wf_add_automation(p, auto1);
    CHECK(r.has_value());
    REQUIRE(p.parameter_automation.size() == 1);
}

TEST_CASE("TIWF001A: add_automation rejects empty breakpoints", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "source.filter.cutoff";
    // breakpoints left empty

    auto r = wf_add_automation(p, auto1);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: add_automation rejects invalid parameter path", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    TimbreAutomation auto1;
    auto1.parameter_path = "nonexistent.param";
    auto1.breakpoints = {
        {ScoreTime{1, Beat{0, 1}}, 0.5f},
        {ScoreTime{2, Beat{0, 1}}, 1.0f}
    };

    auto r = wf_add_automation(p, auto1);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: morph_presets rejects degenerate interval", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{4, Beat{0, 1}};
    morph.end = ScoreTime{2, Beat{0, 1}};  // end before start

    auto r = wf_morph_presets(p, morph);
    CHECK(!r.has_value());
}

TEST_CASE("TIWF001A: morph_presets accepts valid interval", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{1, Beat{0, 1}};
    morph.end = ScoreTime{4, Beat{0, 1}};

    auto r = wf_morph_presets(p, morph);
    CHECK(r.has_value());
    CHECK(p.preset_morphs.size() == 1);
}

// =============================================================================
// Semantic Descriptors
// =============================================================================

TEST_CASE("TIWF001A: set_semantic_descriptors replaces descriptors", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    SemanticTimbreDescriptor d;
    d.brightness = 0.9f;
    d.warmth = 0.1f;
    d.tags = {"bright", "sharp"};

    wf_set_semantic_descriptors(p, d);
    CHECK(p.semantic_descriptors.brightness == Approx(0.9f));
    CHECK(p.semantic_descriptors.tags.size() == 2);
}

// =============================================================================
// Timbre Analysis
// =============================================================================

TEST_CASE("TIWF001A: analyze_timbre returns ParameterDerived descriptors", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto d = wf_analyze_timbre(p);
    CHECK(d.derivation == DerivationMode::ParameterDerived);
}

TEST_CASE("TIWF001A: analyze_timbre — bright subtractive has high brightness", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_set_parameter(p, "source.filter.cutoff", 18000.0f);
    wf_set_parameter(p, "source.filter.resonance", 0.8f);

    auto d = wf_analyze_timbre(p);
    CHECK(d.brightness > 0.5f);
}

TEST_CASE("TIWF001A: analyze_timbre — dark subtractive has low brightness", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_set_parameter(p, "source.filter.cutoff", 200.0f);
    wf_set_parameter(p, "source.filter.resonance", 0.0f);

    auto d = wf_analyze_timbre(p);
    CHECK(d.brightness < 0.3f);
}

TEST_CASE("TIWF001A: analyze_timbre — FM with high feedback is bright and rough", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    FMSynth fm;
    fm.operators = {FMOperator{}};
    fm.feedback = 0.9f;
    SoundSourceData src;
    src.data = std::move(fm);
    wf_set_sound_source(p, std::move(src));

    auto d = wf_analyze_timbre(p);
    CHECK(d.brightness > 0.6f);
    CHECK(d.roughness > 0.5f);
}

TEST_CASE("TIWF001A: analyze_timbre — physical model sets attack from exciter", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PhysicalModelSource pm;
    pm.exciter.type = ExciterType::Bow;
    SoundSourceData src;
    src.data = std::move(pm);
    wf_set_sound_source(p, std::move(src));

    auto d = wf_analyze_timbre(p);
    CHECK(d.attack_character == AttackCharacter::Bowed);
    CHECK(d.sustain_character == SustainCharacter::Steady);
}

TEST_CASE("TIWF001A: analyze_timbre — modulation increases movement", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    p.modulation.lfos = {LFO{}};
    p.modulation.routings = {{{ModulationSourceType::LFO, 0}, "source.filter.cutoff", 0.8f}};

    auto d = wf_analyze_timbre(p);
    CHECK(d.movement > 0.0f);
}

// =============================================================================
// Preset Search
// =============================================================================

TEST_CASE("TIWF001A: search_presets filters by tag", "[timbre-ir][workflow]") {
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
    auto results = wf_search_presets(library, q);
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

TEST_CASE("TIWF001A: search_presets filters by name", "[timbre-ir][workflow]") {
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
    auto results = wf_search_presets(library, q);
    REQUIRE(results.size() == 1);
    CHECK(results[0].name == "Bright Lead");
}

TEST_CASE("TIWF001A: search_presets filters by brightness range", "[timbre-ir][workflow]") {
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
    auto results = wf_search_presets(library, q);
    REQUIRE(results.size() == 1);
    CHECK(results[0].name == "Medium");
}

TEST_CASE("TIWF001A: search_presets respects max_results", "[timbre-ir][workflow]") {
    std::vector<TimbrePreset> library;
    for (int i = 0; i < 20; ++i) {
        TimbrePreset p;
        p.id = TimbrePresetId{static_cast<std::uint64_t>(i)};
        p.name = "Preset " + std::to_string(i);
        library.push_back(p);
    }

    PresetSearchQuery q;
    q.max_results = 5;
    auto results = wf_search_presets(library, q);
    CHECK(results.size() == 5);
}

// =============================================================================
// Preset Load/Save
// =============================================================================

TEST_CASE("TIWF001A: save_preset captures current parameters", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_set_parameter(p, "source.filter.cutoff", 5000.0f);
    p.semantic_descriptors.brightness = 0.8f;

    auto preset = wf_save_preset(p, TimbrePresetId{100}, "My Preset");
    CHECK(preset.id.value == 100);
    CHECK(preset.name == "My Preset");
    CHECK(preset.semantic_descriptors.brightness == Approx(0.8f));
    CHECK(preset.parameter_state.count("source.filter.cutoff") == 1);
    CHECK(preset.parameter_state.at("source.filter.cutoff") == Approx(5000.0f));
}

TEST_CASE("TIWF001A: save_preset captures FM source parameters", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "FM Test");

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
    wf_set_sound_source(p, std::move(src));

    auto preset = wf_save_preset(p, TimbrePresetId{200}, "FM Preset");
    CHECK(preset.parameter_state.count("source.feedback") == 1);
    CHECK(preset.parameter_state.at("source.feedback") == Approx(0.4f));
    CHECK(preset.parameter_state.count("source.operators[0].ratio") == 1);
    CHECK(preset.parameter_state.at("source.operators[0].ratio") == Approx(2.0f));
    CHECK(preset.parameter_state.count("source.operators[1].level") == 1);
    CHECK(preset.parameter_state.at("source.operators[1].level") == Approx(0.5f));
}

TEST_CASE("TIWF001A: load_preset applies saved parameters", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    wf_set_parameter(p, "source.filter.cutoff", 5000.0f);
    wf_set_parameter(p, "source.filter.resonance", 0.7f);
    p.semantic_descriptors.brightness = 0.8f;

    auto preset = wf_save_preset(p, TimbrePresetId{100}, "Saved");

    // Reset profile parameters
    wf_set_parameter(p, "source.filter.cutoff", 1000.0f);
    wf_set_parameter(p, "source.filter.resonance", 0.0f);

    // Load preset
    auto r = wf_load_preset(p, preset);
    CHECK(r.has_value());
    CHECK(*wf_get_parameter(p, "source.filter.cutoff") == Approx(5000.0f));
    CHECK(*wf_get_parameter(p, "source.filter.resonance") == Approx(0.7f));
    CHECK(p.semantic_descriptors.brightness == Approx(0.8f));
}

// =============================================================================
// Preset Morph
// =============================================================================

TEST_CASE("TIWF001A: morph_presets adds morph entry", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PresetMorph morph;
    morph.from_preset = TimbrePresetId{1};
    morph.to_preset = TimbrePresetId{2};
    morph.start = ScoreTime{1, Beat{0, 1}};
    morph.end = ScoreTime{4, Beat{0, 1}};
    morph.curve.type = MappingCurveType::SCurve;

    auto r = wf_morph_presets(p, morph);
    CHECK(r.has_value());
    REQUIRE(p.preset_morphs.size() == 1);
    CHECK(p.preset_morphs[0].from_preset.value == 1);
}

// =============================================================================
// Validation wrapper
// =============================================================================

TEST_CASE("TIWF001A: wf_validate delegates to validate_timbre", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    auto diags = wf_validate(p);
    bool has_error = false;
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) has_error = true;
    }
    CHECK(!has_error);
}

TEST_CASE("TIWF001A: wf_validate catches T2 violation", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");
    std::get<SubtractiveSynth>(p.source.data).oscillators.clear();

    auto diags = wf_validate(p);
    bool has_t2 = false;
    for (const auto& d : diags) {
        if (d.rule == "T2") has_t2 = true;
    }
    CHECK(has_t2);
}

// =============================================================================
// Parameter Path Access — Additive partials
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — additive partial amplitude", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    AdditiveSynth add;
    add.partials = {{1.0f, 1.0f, 0.0f, std::nullopt, 0.0f},
                    {2.0f, 0.5f, 0.0f, std::nullopt, 0.0f}};
    add.global_envelope = make_adsr(20.0f, 100.0f, 0.8f, 200.0f);
    SoundSourceData src;
    src.data = std::move(add);
    wf_set_sound_source(p, std::move(src));

    wf_set_parameter(p, "source.partials[1].amplitude", 0.25f);
    CHECK(*wf_get_parameter(p, "source.partials[1].amplitude") == Approx(0.25f));
}

// =============================================================================
// Parameter Path Access — PhysicalModel
// =============================================================================

TEST_CASE("TIWF001A: set/get parameter — physical model fields", "[timbre-ir][workflow]") {
    auto p = wf_create_timbre_profile(TimbreProfileId{1}, PartId{1}, "Test");

    PhysicalModelSource pm;
    SoundSourceData src;
    src.data = std::move(pm);
    wf_set_sound_source(p, std::move(src));

    wf_set_parameter(p, "source.coupling", 0.7f);
    wf_set_parameter(p, "source.damping", 0.3f);
    wf_set_parameter(p, "source.exciter.brightness", 0.9f);
    wf_set_parameter(p, "source.resonator.decay", 0.6f);

    CHECK(*wf_get_parameter(p, "source.coupling") == Approx(0.7f));
    CHECK(*wf_get_parameter(p, "source.damping") == Approx(0.3f));
    CHECK(*wf_get_parameter(p, "source.exciter.brightness") == Approx(0.9f));
    CHECK(*wf_get_parameter(p, "source.resonator.decay") == Approx(0.6f));
}
