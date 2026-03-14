/**
 * @file TSCI006A.cpp
 * @brief Unit tests for Corpus IR analysis engine (CIAN001A)
 *
 * Component: TSCI006A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CIAN001A
 * Coverage: Each analytical domain verified against a Score containing
 *           known musical content with independently computed expected values.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Corpus/CIAN001A.h"
#include "Corpus/CIWF001A.h"
#include "Score/SIWF001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers — build minimal valid Scores with known content
// =============================================================================

namespace {

/// Create a 4-bar Score in C major, 4/4, 120 BPM with one Piano part
Score make_basic_score() {
    ScoreSpec spec;
    spec.title = "Test Analysis";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};  // C
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.clef = Clef::Treble;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

/// Create a two-part Score (Violin + Cello)
Score make_two_part_score() {
    ScoreSpec spec;
    spec.title = "Two Part Analysis";
    spec.total_bars = 4;
    spec.bpm = 100.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition violin;
    violin.name = "Violin";
    violin.abbreviation = "Vln.";
    violin.instrument_type = InstrumentType::Violin;
    violin.clef = Clef::Treble;
    violin.rendering.midi_channel = 1;
    spec.parts.push_back(violin);

    PartDefinition cello;
    cello.name = "Cello";
    cello.abbreviation = "Vc.";
    cello.instrument_type = InstrumentType::Cello;
    cello.clef = Clef::Bass;
    cello.rendering.midi_channel = 2;
    spec.parts.push_back(cello);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

/// Insert a note at bar_idx (0-indexed), replacing the whole-bar rest
void put_note(Score& score, std::size_t part, std::uint32_t bar_idx,
              SpelledPitch pitch, Beat offset, Beat duration,
              std::optional<DynamicLevel> dyn = std::nullopt) {
    auto& voice = score.parts[part].measures[bar_idx].voices[0];
    voice.events.clear();

    Note note;
    note.pitch = pitch;
    note.velocity = VelocityValue{dyn, 80};
    if (dyn) note.dynamic = dyn;
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = duration;

    if (offset != Beat::zero()) {
        RestEvent pre{offset, true};
        voice.events.push_back(Event{EventId{90001}, Beat::zero(), pre});
    }
    voice.events.push_back(Event{EventId{90002}, offset, ng});

    Beat after = offset + duration;
    Beat bar_dur{1, 1};
    if (after < bar_dur) {
        RestEvent post{bar_dur - after, true};
        voice.events.push_back(Event{EventId{90003}, after, post});
    }
}

SpelledPitch C4{0, 0, 4};
SpelledPitch D4{1, 0, 4};
SpelledPitch E4{2, 0, 4};
SpelledPitch F4{3, 0, 4};
SpelledPitch G4{4, 0, 4};
SpelledPitch A4{5, 0, 4};
SpelledPitch C3{0, 0, 3};
SpelledPitch G3{4, 0, 3};

}  // namespace


// =============================================================================
// Full analysis
// =============================================================================

TEST_CASE("CIAN001A: analyze_score produces all domains", "[corpus-ir][analysis]") {
    auto score = make_basic_score();
    auto wa = analyze_score(score);

    // Formal analysis should reflect 4 bars
    CHECK(wa.formal_analysis.total_duration_bars == 4);

    // Orchestration should be nullopt for single-part
    CHECK_FALSE(wa.orchestration_analysis.has_value());

    // Motivic analysis present but empty
    CHECK(wa.motivic_analysis.thematic_density == 0.0f);
}

// =============================================================================
// Melodic analysis
// =============================================================================

TEST_CASE("CIAN001A: melodic range from note content", "[corpus-ir][analysis][melodic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, G4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 2, E4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 3, C4, Beat::zero(), Beat{1, 4});

    auto ma = analyze_melodic(score);
    REQUIRE(ma.per_voice_analysis.size() == 1);
    // C4 = MIDI 60, G4 = MIDI 67
    CHECK(ma.per_voice_analysis[0].range_low == 60);
    CHECK(ma.per_voice_analysis[0].range_high == 67);
}

TEST_CASE("CIAN001A: melodic conjunct proportion for stepwise melody", "[corpus-ir][analysis][melodic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 2, E4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 3, F4, Beat::zero(), Beat{1, 4});

    auto ma = analyze_melodic(score);
    REQUIRE(ma.per_voice_analysis.size() == 1);
    // All intervals are steps (M2) — conjunct proportion should be 1.0
    CHECK(ma.per_voice_analysis[0].conjunct_proportion == Catch::Approx(1.0f));
}

// =============================================================================
// Rhythmic analysis
// =============================================================================

TEST_CASE("CIAN001A: rhythmic duration distribution", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4});

    auto ra = analyze_rhythmic(score);
    // Two quarter notes inserted
    CHECK(ra.duration_distribution.count("quarter") > 0);
    CHECK(ra.duration_distribution["quarter"] >= 2);
}

TEST_CASE("CIAN001A: rhythmic onset density per bar", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});

    auto ra = analyze_rhythmic(score);
    REQUIRE(ra.onset_density.size() == 4);
    // Bar 1 has 1 onset, bars 2-4 have 0
    CHECK(ra.onset_density[0] >= 1.0f);
}

TEST_CASE("CIAN001A: tempo profile from tempo map", "[corpus-ir][analysis][rhythmic]") {
    auto score = make_basic_score();
    auto ra = analyze_rhythmic(score);
    REQUIRE(!ra.tempo_profile.empty());
    CHECK(ra.tempo_profile[0].second == Catch::Approx(120.0f));
}

// =============================================================================
// Formal analysis
// =============================================================================

TEST_CASE("CIAN001A: formal analysis total bars", "[corpus-ir][analysis][formal]") {
    auto score = make_basic_score();
    auto fa = analyze_formal(score);
    CHECK(fa.total_duration_bars == 4);
}

TEST_CASE("CIAN001A: formal analysis with sections", "[corpus-ir][analysis][formal]") {
    auto score = make_basic_score();
    ScoreSection sec_a;
    sec_a.id = SectionId{1};
    sec_a.label = "A";
    sec_a.start = ScoreTime{1, Beat::zero()};
    sec_a.end = ScoreTime{3, Beat::zero()};
    score.section_map.push_back(sec_a);

    ScoreSection sec_b;
    sec_b.id = SectionId{2};
    sec_b.label = "B";
    sec_b.start = ScoreTime{3, Beat::zero()};
    sec_b.end = ScoreTime{5, Beat::zero()};
    score.section_map.push_back(sec_b);

    auto fa = analyze_formal(score);
    REQUIRE(fa.section_plan.size() == 2);
    CHECK(fa.section_plan[0].label == "A");
    CHECK(fa.section_plan[0].length_bars == 2);
    CHECK(fa.section_plan[1].label == "B");
    CHECK(fa.section_plan[1].length_bars == 2);

    // Two sections → BinarySimple
    CHECK(fa.form_type == FormClassification::BinarySimple);
}

TEST_CASE("CIAN001A: ternary form ABA detected", "[corpus-ir][analysis][formal]") {
    ScoreSpec spec;
    spec.title = "ABA";
    spec.total_bars = 6;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.clef = Clef::Treble;
    piano.rendering.midi_channel = 1;
    spec.parts.push_back(piano);
    auto score = *create_score(spec);

    score.section_map.push_back(ScoreSection{SectionId{1}, "A", {1, Beat::zero()}, {3, Beat::zero()}, {}});
    score.section_map.push_back(ScoreSection{SectionId{2}, "B", {3, Beat::zero()}, {5, Beat::zero()}, {}});
    score.section_map.push_back(ScoreSection{SectionId{3}, "A", {5, Beat::zero()}, {7, Beat::zero()}, {}});

    auto fa = analyze_formal(score);
    CHECK(fa.form_type == FormClassification::Ternary);
}

// =============================================================================
// Textural analysis
// =============================================================================

TEST_CASE("CIAN001A: textural density for two-part score", "[corpus-ir][analysis][textural]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4});
    put_note(score, 1, 0, C3, Beat::zero(), Beat{1, 4});

    auto ta = analyze_textural(score);
    // Bar 1 has both parts active → density 2
    REQUIRE(ta.density_curve.size() == 4);
    CHECK(ta.density_curve[0].second == 2);
    CHECK(ta.average_density > 0.0f);
}

TEST_CASE("CIAN001A: textural register span", "[corpus-ir][analysis][textural]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4});   // MIDI 67
    put_note(score, 1, 0, C3, Beat::zero(), Beat{1, 4});   // MIDI 48

    auto ta = analyze_textural(score);
    // Span = 67 - 48 = 19 semitones in bar 1, 0 in bars 2-4
    CHECK(ta.average_register_span > 0.0f);
}

// =============================================================================
// Dynamic analysis
// =============================================================================

TEST_CASE("CIAN001A: dynamic distribution from note markings", "[corpus-ir][analysis][dynamic]") {
    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4}, DynamicLevel::f);
    put_note(score, 0, 1, D4, Beat::zero(), Beat{1, 4}, DynamicLevel::p);

    auto da = analyze_dynamic(score);
    CHECK(da.dynamic_distribution.count("f") > 0);
    CHECK(da.dynamic_distribution.count("p") > 0);
    CHECK(da.dynamic_range_low == "p");
    CHECK(da.dynamic_range_high == "f");
}

TEST_CASE("CIAN001A: hairpin count from part hairpins", "[corpus-ir][analysis][dynamic]") {
    auto score = make_basic_score();
    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    score.parts[0].hairpins.push_back(hp);

    auto da = analyze_dynamic(score);
    CHECK(da.hairpin_count == 1);
}

// =============================================================================
// Orchestration analysis
// =============================================================================

TEST_CASE("CIAN001A: orchestration nullopt for single part", "[corpus-ir][analysis][orch]") {
    auto score = make_basic_score();
    auto oa = analyze_orchestration(score);
    CHECK_FALSE(oa.has_value());
}

TEST_CASE("CIAN001A: orchestration instrument usage for two parts", "[corpus-ir][analysis][orch]") {
    auto score = make_two_part_score();
    put_note(score, 0, 0, G4, Beat::zero(), Beat{1, 4});
    // Cello is silent

    auto oa = analyze_orchestration(score);
    REQUIRE(oa.has_value());
    CHECK(oa->instrument_usage.count("Violin") > 0);
    CHECK(oa->instrument_usage.count("Cello") > 0);
    // Violin plays in 1 of 4 bars = 0.25
    CHECK(oa->instrument_usage["Violin"] == Catch::Approx(0.25f));
    // Cello plays in 0 of 4 bars = 0.0
    CHECK(oa->instrument_usage["Cello"] == Catch::Approx(0.0f));
}

// =============================================================================
// Integration with wf_analyze_work
// =============================================================================

TEST_CASE("CIAN001A: wf_analyze_work with Score populates analysis", "[corpus-ir][analysis][workflow]") {
    CorpusDatabase db;
    auto profile = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = profile;

    WorkMetadata meta;
    meta.title = "Test Work";
    meta.source_format = "test";
    auto work = wf_create_ingested_work(IngestedWorkId{1}, meta);
    db.works[1] = work;

    auto score = make_basic_score();
    put_note(score, 0, 0, C4, Beat::zero(), Beat{1, 4});
    put_note(score, 0, 1, E4, Beat::zero(), Beat{1, 4});

    auto r = wf_analyze_work(db, IngestedWorkId{1}, &score);
    REQUIRE(r.has_value());

    CHECK(db.works[1].analysis_complete);
    // Melodic analysis should have found notes
    CHECK(!db.works[1].analysis.melodic_analysis.per_voice_analysis.empty());
    // Formal analysis should know total bars
    CHECK(db.works[1].analysis.formal_analysis.total_duration_bars == 4);
    // Rhythmic analysis should have duration data
    CHECK(!db.works[1].analysis.rhythmic_analysis.onset_density.empty());
}
