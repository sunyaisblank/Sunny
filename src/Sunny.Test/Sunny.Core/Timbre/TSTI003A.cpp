/**
 * @file TSTI003A.cpp
 * @brief Unit tests for Timbre IR validation (TIVD001A)
 *
 * Component: TSTI003A
 * Domain: TS (Test) | Category: TI (Timbre IR)
 *
 * Tests: TIVD001A
 * Coverage: Validation rules T1-T5, T7, T9; is_timbre_valid
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Timbre/TIVD001A.h"
#include "Scale/SCDF001A.h"
#include "Rhythm/RHTS001A.h"

using namespace Sunny::Core;

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

/// Construct a minimal valid TimbreProfile with one SubtractiveSynth oscillator
static TimbreProfile make_valid_profile(PartId part = PartId{1}) {
    TimbreProfile p;
    p.id = TimbreProfileId{1};
    p.part_id = part;
    p.name = "Test Profile";

    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    p.source.data = std::move(sub);

    return p;
}

/// Construct a minimal Score with the given Part IDs
static Score make_score(std::vector<std::uint64_t> part_ids) {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Test Score";
    score.metadata.total_bars = 4;

    TempoEvent tempo;
    tempo.position = ScoreTime{1, Beat::zero()};
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};  // C4
    auto scale = find_scale("major");
    if (scale) key_entry.key.mode = *scale;
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    TimeSignatureEntry ts_entry;
    ts_entry.bar = 1;
    auto ts = make_time_signature(4, 4);
    if (ts) ts_entry.time_signature = *ts;
    score.time_map.push_back(ts_entry);

    for (auto id : part_ids) {
        Part part;
        part.id = PartId{id};
        part.definition.name = "Part " + std::to_string(id);
        part.definition.instrument_type = InstrumentType::Synthesiser;
        for (std::uint32_t bar = 1; bar <= score.metadata.total_bars; ++bar) {
            Measure m;
            m.bar_number = bar;
            Voice v;
            v.voice_index = 0;
            RestEvent rest;
            rest.duration = Beat{4, 4};
            v.events.push_back(Event{EventId{bar * 100 + id}, Beat::zero(),
                                     rest});
            m.voices.push_back(v);
            part.measures.push_back(m);
        }
        score.parts.push_back(std::move(part));
    }

    return score;
}

// =============================================================================
// T1: Part-TimbreProfile correspondence
// =============================================================================

TEST_CASE("TIVD001A: T1 passes when all Parts have profiles", "[timbre-ir][validation]") {
    auto score = make_score({1, 2, 3});
    std::vector<TimbreProfile> profiles;
    profiles.push_back(make_valid_profile(PartId{1}));
    profiles.push_back(make_valid_profile(PartId{2}));
    profiles.push_back(make_valid_profile(PartId{3}));

    auto diags = validate_timbre_correspondence(score, profiles);
    CHECK(diags.empty());
}

TEST_CASE("TIVD001A: T1 Error for missing TimbreProfile", "[timbre-ir][validation]") {
    auto score = make_score({1, 2, 3});
    std::vector<TimbreProfile> profiles;
    profiles.push_back(make_valid_profile(PartId{1}));
    // Parts 2 and 3 have no profile

    auto diags = validate_timbre_correspondence(score, profiles);
    CHECK(diags.size() == 2);
    CHECK(diags[0].severity == ValidationSeverity::Error);
    CHECK(diags[0].rule == "T1");
}

// =============================================================================
// T2: SoundSource validity
// =============================================================================

TEST_CASE("TIVD001A: T2 Error for SubtractiveSynth without oscillators", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    // Clear oscillators
    std::get<SubtractiveSynth>(profile.source.data).oscillators.clear();

    auto diags = validate_timbre(profile);
    REQUIRE(!diags.empty());
    CHECK(diags[0].rule == "T2");
    CHECK(diags[0].severity == ValidationSeverity::Error);
    CHECK(diags[0].error_code == TimbreError::InvalidSource);
}

TEST_CASE("TIVD001A: T2 Error for oscillator_mix size mismatch", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto& sub = std::get<SubtractiveSynth>(profile.source.data);
    sub.oscillator_mix = {0.5f, 0.5f};  // 2 entries but only 1 oscillator

    auto diags = validate_timbre(profile);
    bool found_t2 = false;
    for (const auto& d : diags) {
        if (d.rule == "T2") found_t2 = true;
    }
    CHECK(found_t2);
}

TEST_CASE("TIVD001A: T2 Error for FMSynth with no operators", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    FMSynth fm;
    // Leave operators empty
    profile.source.data = std::move(fm);

    auto diags = validate_timbre(profile);
    REQUIRE(!diags.empty());
    CHECK(diags[0].rule == "T2");
}

TEST_CASE("TIVD001A: T2 Error for FM preset number out of range", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    FMSynth fm;
    fm.operators = {FMOperator{}};
    fm.algorithm.use_preset = true;
    fm.algorithm.preset_number = 33;  // invalid: max 32
    profile.source.data = std::move(fm);

    auto diags = validate_timbre(profile);
    bool found_t2 = false;
    for (const auto& d : diags) {
        if (d.rule == "T2") found_t2 = true;
    }
    CHECK(found_t2);
}

TEST_CASE("TIVD001A: T2 Error for AdditiveSynth with no partials", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    AdditiveSynth add;
    add.global_envelope = make_adsr(50.0f, 200.0f, 0.6f, 500.0f);
    profile.source.data = std::move(add);

    auto diags = validate_timbre(profile);
    REQUIRE(!diags.empty());
    CHECK(diags[0].rule == "T2");
}

TEST_CASE("TIVD001A: T2 Error for Sampler with empty library", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    SamplerSource sampler;
    sampler.library = "";
    profile.source.data = std::move(sampler);

    auto diags = validate_timbre(profile);
    REQUIRE(!diags.empty());
    CHECK(diags[0].rule == "T2");
}

TEST_CASE("TIVD001A: T2 passes for valid SubtractiveSynth", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto diags = validate_timbre(profile);

    bool has_t2_error = false;
    for (const auto& d : diags) {
        if (d.rule == "T2" && d.severity == ValidationSeverity::Error) {
            has_t2_error = true;
        }
    }
    CHECK(!has_t2_error);
}

// =============================================================================
// T3: Filter cutoff vs Nyquist
// =============================================================================

TEST_CASE("TIVD001A: T3 Warning for filter cutoff above Nyquist", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto& sub = std::get<SubtractiveSynth>(profile.source.data);
    sub.filter.cutoff = 25000.0f;  // above 44100/2 = 22050

    auto diags = validate_timbre(profile, 44100.0f);
    bool found_t3 = false;
    for (const auto& d : diags) {
        if (d.rule == "T3") {
            found_t3 = true;
            CHECK(d.severity == ValidationSeverity::Warning);
        }
    }
    CHECK(found_t3);
}

TEST_CASE("TIVD001A: T3 passes for filter cutoff below Nyquist", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto& sub = std::get<SubtractiveSynth>(profile.source.data);
    sub.filter.cutoff = 8000.0f;

    auto diags = validate_timbre(profile, 44100.0f);
    bool found_t3 = false;
    for (const auto& d : diags) {
        if (d.rule == "T3") found_t3 = true;
    }
    CHECK(!found_t3);
}

// =============================================================================
// T4: Oscillator detune range
// =============================================================================

TEST_CASE("TIVD001A: T4 Warning for excessive detune", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto& sub = std::get<SubtractiveSynth>(profile.source.data);
    sub.oscillators[0].tune_cents = 150.0f;  // exceeds ±100

    auto diags = validate_timbre(profile);
    bool found_t4 = false;
    for (const auto& d : diags) {
        if (d.rule == "T4") {
            found_t4 = true;
            CHECK(d.severity == ValidationSeverity::Warning);
        }
    }
    CHECK(found_t4);
}

// =============================================================================
// T5: FM feedback stability
// =============================================================================

TEST_CASE("TIVD001A: T5 Warning for FM feedback above threshold", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    FMSynth fm;
    fm.operators = {FMOperator{}};
    fm.feedback = 0.99f;
    profile.source.data = std::move(fm);

    auto diags = validate_timbre(profile);
    bool found_t5 = false;
    for (const auto& d : diags) {
        if (d.rule == "T5") {
            found_t5 = true;
            CHECK(d.severity == ValidationSeverity::Warning);
        }
    }
    CHECK(found_t5);
}

// =============================================================================
// T7: Modulation target path
// =============================================================================

TEST_CASE("TIVD001A: T7 Warning for invalid modulation target path", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    profile.modulation.routings = {
        {{ModulationSourceType::LFO, 0}, "invalid.path.here", 0.5f}
    };

    auto diags = validate_timbre(profile);
    bool found_t7 = false;
    for (const auto& d : diags) {
        if (d.rule == "T7") {
            found_t7 = true;
            CHECK(d.severity == ValidationSeverity::Warning);
        }
    }
    CHECK(found_t7);
}

TEST_CASE("TIVD001A: T7 passes for valid modulation target path", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    profile.modulation.routings = {
        {{ModulationSourceType::LFO, 0}, "source.filter.cutoff", 0.5f}
    };

    auto diags = validate_timbre(profile);
    bool found_t7 = false;
    for (const auto& d : diags) {
        if (d.rule == "T7") found_t7 = true;
    }
    CHECK(!found_t7);
}

// =============================================================================
// T9: Unmapped rendering parameters
// =============================================================================

TEST_CASE("TIVD001A: T9 Warning for device with empty parameter map", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    profile.rendering.device_type.device_name = "Analog";
    // parameter_map left empty

    auto diags = validate_timbre(profile);
    bool found_t9 = false;
    for (const auto& d : diags) {
        if (d.rule == "T9") {
            found_t9 = true;
            CHECK(d.severity == ValidationSeverity::Warning);
        }
    }
    CHECK(found_t9);
}

// =============================================================================
// is_timbre_valid
// =============================================================================

TEST_CASE("TIVD001A: is_timbre_valid returns true for valid profile", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    CHECK(is_timbre_valid(profile));
}

TEST_CASE("TIVD001A: is_timbre_valid returns false when T2 Error present", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    std::get<SubtractiveSynth>(profile.source.data).oscillators.clear();
    CHECK(!is_timbre_valid(profile));
}

// =============================================================================
// T2: HybridSource recursive validation
// =============================================================================

TEST_CASE("TIVD001A: T2 validates HybridSource layers recursively", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();

    // Create a hybrid with one valid and one invalid layer
    auto valid_layer = std::make_unique<SoundSourceData>();
    SubtractiveSynth sub;
    sub.oscillators = {Oscillator{}};
    sub.amplifier = make_adsr(5.0f, 100.0f, 0.8f, 200.0f);
    valid_layer->data = std::move(sub);

    auto invalid_layer = std::make_unique<SoundSourceData>();
    AdditiveSynth add;
    add.global_envelope = make_adsr(50.0f, 200.0f, 0.6f, 500.0f);
    // partials left empty — T2 violation
    invalid_layer->data = std::move(add);

    HybridSource hybrid;
    hybrid.layers.push_back(std::move(valid_layer));
    hybrid.layers.push_back(std::move(invalid_layer));
    hybrid.routing.type = LayerRoutingType::Mix;
    hybrid.routing.mix_levels = {0.5f, 0.5f};
    profile.source.data = std::move(hybrid);

    auto diags = validate_timbre(profile);
    bool found_t2 = false;
    for (const auto& d : diags) {
        if (d.rule == "T2" && d.severity == ValidationSeverity::Error) {
            found_t2 = true;
        }
    }
    CHECK(found_t2);
}

// =============================================================================
// Diagnostic ordering
// =============================================================================

TEST_CASE("TIVD001A: diagnostics are sorted by severity", "[timbre-ir][validation]") {
    auto profile = make_valid_profile();
    auto& sub = std::get<SubtractiveSynth>(profile.source.data);
    sub.oscillators.clear();               // T2 Error
    sub.filter.cutoff = 25000.0f;          // T3 Warning (still checked despite T2)

    // Also add a rendering config issue
    profile.rendering.device_type.device_name = "Analog";

    auto diags = validate_timbre(profile);
    REQUIRE(diags.size() >= 2);

    // Errors come first
    CHECK(diags.front().severity == ValidationSeverity::Error);
}

// =============================================================================
// Boundary values at validation thresholds
// =============================================================================

TEST_CASE("TIVD001A: boundary values at validation thresholds", "[timbre-ir][validation]") {
    SECTION("T3 cutoff exactly at Nyquist") {
        // T3 triggers on cutoff > nyquist (strict). At exactly Nyquist
        // the condition is false, so no warning should be emitted.
        auto profile = make_valid_profile();
        auto& sub = std::get<SubtractiveSynth>(profile.source.data);
        sub.filter.cutoff = 22050.0f;

        auto diags = validate_timbre(profile, 44100.0f);
        bool found_t3 = false;
        for (const auto& d : diags) {
            if (d.rule == "T3") found_t3 = true;
        }
        CHECK_FALSE(found_t3);
    }

    SECTION("T4 detune exactly 100 cents") {
        // T4 triggers on abs(tune_cents) > 100.0f (strict). At exactly
        // 100 the condition is false, so no warning should be emitted.
        // A mutation of > to >= would incorrectly trigger T4 here.
        auto profile = make_valid_profile();
        auto& sub = std::get<SubtractiveSynth>(profile.source.data);
        sub.oscillators[0].tune_cents = 100.0f;

        auto diags = validate_timbre(profile);
        bool found_t4 = false;
        for (const auto& d : diags) {
            if (d.rule == "T4") found_t4 = true;
        }
        CHECK_FALSE(found_t4);
    }

    SECTION("T5 feedback exactly at threshold") {
        // T5 triggers on abs(feedback) > 0.95f (strict). At exactly
        // 0.95 the condition is false, so no warning should be emitted.
        // A mutation of > to >= would incorrectly trigger T5 here.
        auto profile = make_valid_profile();
        FMSynth fm;
        fm.operators = {FMOperator{}};
        fm.feedback = 0.95f;
        profile.source.data = std::move(fm);

        auto diags = validate_timbre(profile);
        bool found_t5 = false;
        for (const auto& d : diags) {
            if (d.rule == "T5") found_t5 = true;
        }
        CHECK_FALSE(found_t5);
    }
}
