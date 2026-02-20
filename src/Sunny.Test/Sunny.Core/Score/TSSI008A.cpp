/**
 * @file TSSI008A.cpp
 * @brief Unit tests for Score IR views (SIVW001A) and queries (SIQR001A)
 *
 * Component: TSSI008A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIVW001A, SIQR001A
 * Coverage: part_extract, piano_reduction, region_view,
 *           query_notes_in_range, query_tempo_at, query_key_at,
 *           query_voice_count, query_note_density, query_diagnostics,
 *           query_harmony_range, query_melody_for, query_chord_progression,
 *           query_orchestration, query_parts_with_role, query_pitch_content,
 *           query_time_signature_at, query_sections, query_form_summary,
 *           query_find_motif
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIVW001A.h"
#include "Score/SIQR001A.h"
#include "Score/SIMT001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_valid_score(std::uint32_t total_bars = 4) {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Test";
    score.metadata.total_bars = total_bars;

    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    auto ts = make_time_signature(4, 4);
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    Part piano;
    piano.id = PartId{100};
    piano.definition.name = "Piano";
    piano.definition.abbreviation = "Pno.";
    piano.definition.instrument_type = InstrumentType::Piano;
    piano.definition.range = PitchRange{
        SpelledPitch{5, 0, 0},  // A0
        SpelledPitch{0, 0, 8},  // C8
        SpelledPitch{0, 0, 3},  // C3
        SpelledPitch{0, 0, 6}   // C6
    };

    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        RestEvent rest{ts->measure_duration(), true};
        Event event{EventId{bar * 1000}, Beat::zero(), rest};
        Voice voice{0, {event}, {}};
        Measure measure{bar, {voice}, std::nullopt, std::nullopt};
        piano.measures.push_back(measure);
    }

    score.parts.push_back(piano);
    return score;
}

/// Create a two-part score with notes in both parts
Score make_two_part_score() {
    auto score = make_valid_score(4);

    // Add a violin part
    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3},  // G3
        SpelledPitch{2, 0, 7},  // E7
        SpelledPitch{4, 0, 3},  // G3
        SpelledPitch{2, 0, 7}   // E7
    };
    (void)add_part(score, violin_def, 1);

    // Place C4 whole note in piano bar 1
    auto& piano_voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng_piano;
    ng_piano.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng_piano.duration = Beat{1, 1};
    piano_voice.events[0].payload = ng_piano;

    // Place E5 whole note in violin bar 1
    auto& violin_voice = score.parts[1].measures[0].voices[0];
    NoteGroup ng_violin;
    ng_violin.notes.push_back(Note{SpelledPitch{2, 0, 5}, VelocityValue{{}, 80}});
    ng_violin.duration = Beat{1, 1};
    violin_voice.events[0].payload = ng_violin;

    return score;
}

}  // anonymous namespace

// =============================================================================
// SIVW001A: Views
// =============================================================================

TEST_CASE("SIVW001A: part_extract produces single-part score",
          "[score-ir][view]") {
    auto score = make_two_part_score();
    CHECK(score.parts.size() == 2);

    auto extracted = part_extract(score, PartId{100});  // Piano
    CHECK(extracted.parts.size() == 1);
    CHECK(extracted.parts[0].definition.name == "Piano");
    CHECK(extracted.metadata.total_bars == score.metadata.total_bars);

    // Tempo and key maps should be preserved
    CHECK(extracted.tempo_map.size() == score.tempo_map.size());
    CHECK(extracted.key_map.size() == score.key_map.size());
}

TEST_CASE("SIVW001A: part_extract with nonexistent id returns placeholder",
          "[score-ir][view]") {
    auto score = make_valid_score();
    auto extracted = part_extract(score, PartId{999});
    // When the part is not found, the implementation returns a skeleton
    // with one placeholder part named "Unknown"
    REQUIRE(extracted.parts.size() == 1);
    CHECK(extracted.parts[0].definition.name == "Unknown");
}

TEST_CASE("SIVW001A: piano_reduction collapses to one part",
          "[score-ir][view]") {
    auto score = make_two_part_score();
    CHECK(score.parts.size() == 2);

    auto reduced = piano_reduction(score);
    CHECK(reduced.parts.size() == 1);

    // The reduced part should be a Piano
    CHECK(reduced.parts[0].definition.instrument_type == InstrumentType::Piano);

    // Should have measures matching the original score
    CHECK(reduced.parts[0].measures.size() == score.metadata.total_bars);
}

TEST_CASE("SIVW001A: region_view extracts bars 2-3 from 4-bar score",
          "[score-ir][view]") {
    auto score = make_valid_score(4);

    // Place notes in bars 2 and 3
    for (std::uint32_t bar : {2u, 3u}) {
        auto& voice = score.parts[0].measures[bar - 1].voices[0];
        NoteGroup ng;
        ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
        ng.duration = Beat{1, 1};
        voice.events[0].payload = ng;
    }

    ScoreRegion region;
    region.start = ScoreTime{2, Beat::zero()};
    region.end = ScoreTime{4, Beat::zero()};  // bars 2-3

    auto view = region_view(score, region);

    // Should have 2 bars, renumbered starting from 1
    CHECK(view.metadata.total_bars == 2);
    CHECK(view.parts.size() == 1);
    CHECK(view.parts[0].measures.size() == 2);
    CHECK(view.parts[0].measures[0].bar_number == 1);
    CHECK(view.parts[0].measures[1].bar_number == 2);

    // Both bars should have notes
    CHECK(view.parts[0].measures[0].voices[0].events[0].is_note_group());
    CHECK(view.parts[0].measures[1].voices[0].events[0].is_note_group());
}

// =============================================================================
// SIQR001A: Queries
// =============================================================================

TEST_CASE("SIQR001A: query_notes_in_range finds notes in region",
          "[score-ir][query]") {
    auto score = make_two_part_score();

    // Query bar 1 of all parts
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto notes = query_notes_in_range(score, region);
    // Piano has C4, Violin has E5 — both in bar 1
    CHECK(notes.size() == 2);

    // Verify they are note events
    for (const auto& located : notes) {
        REQUIRE(located.event != nullptr);
        CHECK(located.event->is_note_group());
    }
}

TEST_CASE("SIQR001A: query_notes_in_range returns empty for rest-only region",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    auto notes = query_notes_in_range(score, region);
    CHECK(notes.empty());
}

TEST_CASE("SIQR001A: query_tempo_at returns tempo at position",
          "[score-ir][query]") {
    auto score = make_valid_score();

    auto tempo = query_tempo_at(score, SCORE_START);
    CHECK(tempo.bpm.to_float() == 120.0);
    CHECK(tempo.beat_unit == BeatUnit::Quarter);
}

TEST_CASE("SIQR001A: query_key_at returns key at position",
          "[score-ir][query]") {
    auto score = make_valid_score();

    auto key = query_key_at(score, SCORE_START);
    CHECK(key.root.letter == 0);       // C
    CHECK(key.root.accidental == 0);   // Natural
    CHECK(key.accidentals == 0);       // No sharps/flats
}

TEST_CASE("SIQR001A: query_voice_count returns correct count",
          "[score-ir][query]") {
    auto score = make_valid_score(1);
    CHECK(query_voice_count(score, 1, PartId{100}) == 1);

    // Add a second voice
    auto result = add_voice(score, 1, PartId{100}, 1);
    REQUIRE(result.has_value());
    CHECK(query_voice_count(score, 1, PartId{100}) == 2);
}

TEST_CASE("SIQR001A: query_note_density calculates notes per bar",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    // All rests: density should be 0
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    double density = query_note_density(score, region);
    CHECK(density == 0.0);

    // Add 2 notes in bar 1 (replace rest with two half-note events)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{5001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{5002}, Beat{1, 2}, ng2});

    density = query_note_density(score, region);
    // 2 note events across 4 bars = 0.5 notes per bar
    CHECK(density > 0.0);
}

TEST_CASE("SIQR001A: query_diagnostics returns validation results",
          "[score-ir][query]") {
    auto score = make_valid_score();

    // A valid score should still produce some diagnostics (e.g., R1 missing preset)
    auto diags = query_diagnostics(score);
    // At minimum, R1 should fire (no instrument preset)
    bool found_r1 = false;
    for (const auto& d : diags) {
        if (d.rule == "R1") { found_r1 = true; break; }
    }
    CHECK(found_r1);
}

TEST_CASE("SIQR001A: query_diagnostics on empty-parts score returns S0",
          "[score-ir][query]") {
    Score score;
    score.id = ScoreId{1};
    score.metadata.total_bars = 1;

    auto diags = query_diagnostics(score);
    bool found_s0 = false;
    for (const auto& d : diags) {
        if (d.rule == "S0") { found_s0 = true; break; }
    }
    CHECK(found_s0);
}

// =============================================================================
// SIVW001A: Source immutability
// =============================================================================

TEST_CASE("SIVW001A: views do not modify source score", "[score-ir][view]") {
    auto score = make_two_part_score();
    auto original_version = score.version;
    auto original_part_count = score.parts.size();

    // Generate views
    auto extracted = part_extract(score, PartId{100});
    auto reduced = piano_reduction(score);

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};
    auto view = region_view(score, region);

    // Source should be unchanged
    CHECK(score.version == original_version);
    CHECK(score.parts.size() == original_part_count);
    CHECK(score.parts[0].definition.name == "Piano");
}

// =============================================================================
// SIQR001A: Extended Queries (§12.1)
// =============================================================================

TEST_CASE("SIQR001A: query_harmony_range returns overlapping annotations",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    // Add two harmonic annotations: bar 1 and bar 2
    HarmonicAnnotation ha1;
    ha1.position = SCORE_START;
    ha1.duration = Beat{1, 1};  // whole note
    ha1.roman_numeral = "I";
    ha1.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha1);

    HarmonicAnnotation ha2;
    ha2.position = ScoreTime{2, Beat::zero()};
    ha2.duration = Beat{1, 1};
    ha2.roman_numeral = "V";
    ha2.function = ScoreHarmonicFunction::Dominant;
    score.harmonic_annotations.push_back(ha2);

    // Query bar 1 only — should find ha1
    auto result = query_harmony_range(score, SCORE_START, ScoreTime{2, Beat::zero()});
    REQUIRE(result.size() == 1);
    CHECK(result[0].roman_numeral == "I");

    // Query both bars — should find both
    auto both = query_harmony_range(score, SCORE_START, ScoreTime{3, Beat::zero()});
    CHECK(both.size() == 2);

    // Query bar 3 only — empty
    auto empty = query_harmony_range(score, ScoreTime{3, Beat::zero()}, ScoreTime{4, Beat::zero()});
    CHECK(empty.empty());
}

TEST_CASE("SIQR001A: query_melody_for extracts melody line from voice 0",
          "[score-ir][query]") {
    auto score = make_valid_score(2);

    // Place C4 half note and E4 half note in bar 1
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.duration = Beat{1, 2};
    voice.events.push_back(Event{EventId{5001}, Beat::zero(), ng1});

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});  // E4
    ng2.duration = Beat{1, 2};
    voice.events.push_back(Event{EventId{5002}, Beat{1, 2}, ng2});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto melody = query_melody_for(score, PartId{100}, region);
    REQUIRE(melody.size() == 2);
    CHECK(melody[0].pitch.letter == 0);  // C
    CHECK(melody[0].pitch.octave == 4);
    CHECK(melody[1].pitch.letter == 2);  // E
    CHECK(melody[1].pitch.octave == 4);
}

TEST_CASE("SIQR001A: query_melody_for picks highest note from chord",
          "[score-ir][query]") {
    auto score = make_valid_score(1);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Chord: C4 + E4 + G4 — melody should be G4
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});  // E4
    ng.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{6001}, Beat::zero(), ng});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto melody = query_melody_for(score, PartId{100}, region);
    REQUIRE(melody.size() == 1);
    CHECK(melody[0].pitch.letter == 4);  // G
    CHECK(melody[0].pitch.octave == 4);
}

TEST_CASE("SIQR001A: query_chord_progression returns annotations in region",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    HarmonicAnnotation ha1;
    ha1.position = SCORE_START;
    ha1.duration = Beat{1, 1};
    ha1.roman_numeral = "I";
    ha1.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha1);

    HarmonicAnnotation ha2;
    ha2.position = ScoreTime{2, Beat::zero()};
    ha2.duration = Beat{1, 1};
    ha2.roman_numeral = "IV";
    ha2.function = ScoreHarmonicFunction::Predominant;
    score.harmonic_annotations.push_back(ha2);

    HarmonicAnnotation ha3;
    ha3.position = ScoreTime{3, Beat::zero()};
    ha3.duration = Beat{1, 1};
    ha3.roman_numeral = "V";
    ha3.function = ScoreHarmonicFunction::Dominant;
    score.harmonic_annotations.push_back(ha3);

    // Query bars 1-2
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto progression = query_chord_progression(score, region);
    REQUIRE(progression.size() == 2);
    CHECK(progression[0].roman_numeral == "I");
    CHECK(progression[1].roman_numeral == "IV");
}

TEST_CASE("SIQR001A: query_orchestration returns overlapping annotations",
          "[score-ir][query]") {
    auto score = make_two_part_score();

    OrchestrationAnnotation oa1;
    oa1.part_id = PartId{100};
    oa1.start = SCORE_START;
    oa1.end = ScoreTime{3, Beat::zero()};
    oa1.role = TexturalRole::HarmonicFill;
    score.orchestration_annotations.push_back(oa1);

    OrchestrationAnnotation oa2;
    oa2.part_id = score.parts[1].id;
    oa2.start = SCORE_START;
    oa2.end = ScoreTime{3, Beat::zero()};
    oa2.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa2);

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto orch = query_orchestration(score, region);
    CHECK(orch.size() == 2);

    // Check roles are as assigned
    bool found_melody = false;
    bool found_fill = false;
    for (const auto& [pid, role] : orch) {
        if (role == TexturalRole::Melody) found_melody = true;
        if (role == TexturalRole::HarmonicFill) found_fill = true;
    }
    CHECK(found_melody);
    CHECK(found_fill);
}

TEST_CASE("SIQR001A: query_parts_with_role filters by role",
          "[score-ir][query]") {
    auto score = make_two_part_score();

    OrchestrationAnnotation oa1;
    oa1.part_id = PartId{100};
    oa1.start = SCORE_START;
    oa1.end = ScoreTime{5, Beat::zero()};
    oa1.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa1);

    OrchestrationAnnotation oa2;
    oa2.part_id = score.parts[1].id;
    oa2.start = SCORE_START;
    oa2.end = ScoreTime{5, Beat::zero()};
    oa2.role = TexturalRole::BassLine;
    score.orchestration_annotations.push_back(oa2);

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    auto melody_parts = query_parts_with_role(score, TexturalRole::Melody, region);
    REQUIRE(melody_parts.size() == 1);
    CHECK(melody_parts[0] == PartId{100});

    auto bass_parts = query_parts_with_role(score, TexturalRole::BassLine, region);
    REQUIRE(bass_parts.size() == 1);
    CHECK(bass_parts[0] == score.parts[1].id);

    auto fill_parts = query_parts_with_role(score, TexturalRole::HarmonicFill, region);
    CHECK(fill_parts.empty());
}

TEST_CASE("SIQR001A: query_pitch_content returns deduplicated sorted PCs",
          "[score-ir][query]") {
    auto score = make_valid_score(2);

    // Bar 1: C4 and E4
    auto& voice1 = score.parts[0].measures[0].voices[0];
    voice1.events.clear();
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4 → PC 0
    ng1.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});  // E4 → PC 4
    ng1.duration = Beat{1, 1};
    voice1.events.push_back(Event{EventId{7001}, Beat::zero(), ng1});

    // Bar 2: C5 (duplicate PC 0) and G4
    auto& voice2 = score.parts[0].measures[1].voices[0];
    voice2.events.clear();
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5 → PC 0
    ng2.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4 → PC 7
    ng2.duration = Beat{1, 1};
    voice2.events.push_back(Event{EventId{7002}, Beat::zero(), ng2});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto pcs = query_pitch_content(score, region);
    // Should be {0, 4, 7} — deduplicated and sorted
    REQUIRE(pcs.size() == 3);
    CHECK(pcs[0] == 0);   // C
    CHECK(pcs[1] == 4);   // E
    CHECK(pcs[2] == 7);   // G
}

TEST_CASE("SIQR001A: query_time_signature_at returns active time signature",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    // Default is 4/4 from bar 1
    auto ts1 = query_time_signature_at(score, 1);
    CHECK(ts1.denominator == 4);

    // Add 3/4 starting at bar 3
    auto ts34 = make_time_signature(3, 4);
    REQUIRE(ts34.has_value());
    TimeSignatureEntry entry;
    entry.bar = 3;
    entry.time_signature = *ts34;
    score.time_map.push_back(entry);

    // Bar 2 should still be 4/4
    auto ts2 = query_time_signature_at(score, 2);
    CHECK(ts2.denominator == 4);

    // Bar 3 should now be 3/4
    auto ts3 = query_time_signature_at(score, 3);
    CHECK(ts3.denominator == 4);  // denominator stays 4
    // Check numerator encodes 3 beats
    CHECK(ts3.measure_duration() == ts34->measure_duration());
}

TEST_CASE("SIQR001A: query_sections returns full section list",
          "[score-ir][query]") {
    auto score = make_valid_score(8);

    ScoreSection secA;
    secA.id = SectionId{1};
    secA.label = "A";
    secA.start = SCORE_START;
    secA.end = ScoreTime{5, Beat::zero()};
    secA.form_function = FormFunction::Expository;

    ScoreSection secB;
    secB.id = SectionId{2};
    secB.label = "B";
    secB.start = ScoreTime{5, Beat::zero()};
    secB.end = ScoreTime{9, Beat::zero()};
    secB.form_function = FormFunction::Developmental;

    score.section_map = {secA, secB};

    auto sections = query_sections(score);
    REQUIRE(sections.size() == 2);
    CHECK(sections[0].label == "A");
    CHECK(sections[1].label == "B");
}

TEST_CASE("SIQR001A: query_form_summary produces correct entries",
          "[score-ir][query]") {
    auto score = make_valid_score(8);

    ScoreSection secA;
    secA.id = SectionId{1};
    secA.label = "Exposition";
    secA.start = SCORE_START;
    secA.end = ScoreTime{5, Beat::zero()};
    score.section_map.push_back(secA);

    ScoreSection secB;
    secB.id = SectionId{2};
    secB.label = "Development";
    secB.start = ScoreTime{5, Beat::zero()};
    secB.end = ScoreTime{9, Beat::zero()};
    score.section_map.push_back(secB);

    auto summary = query_form_summary(score);
    REQUIRE(summary.size() == 2);

    CHECK(summary[0].label == "Exposition");
    CHECK(summary[0].start == SCORE_START);
    CHECK(summary[0].tempo_bpm == 120.0);
    CHECK(summary[0].key.root.letter == 0);  // C major

    CHECK(summary[1].label == "Development");
    CHECK(summary[1].start == ScoreTime{5, Beat::zero()});
}

// =============================================================================
// SIVW001A: short_score family collapsing (§8.2.2)
// =============================================================================

TEST_CASE("SIVW001A: short_score groups three families into three parts",
          "[score-ir][view]") {
    auto score = make_valid_score(1);

    // Add Violin (Strings family)
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 1);

    // Add Flute (Woodwinds family)
    PartDefinition flute_def;
    flute_def.name = "Flute";
    flute_def.abbreviation = "Fl.";
    flute_def.instrument_type = InstrumentType::Flute;
    (void)add_part(score, flute_def, 2);

    // Add Trumpet (Brass family)
    PartDefinition trumpet_def;
    trumpet_def.name = "Trumpet";
    trumpet_def.abbreviation = "Tpt.";
    trumpet_def.instrument_type = InstrumentType::Trumpet;
    (void)add_part(score, trumpet_def, 3);

    // Place a note in each non-piano part
    for (std::size_t i = 1; i < score.parts.size(); ++i) {
        auto& voice = score.parts[i].measures[0].voices[0];
        NoteGroup ng;
        ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});
        ng.duration = Beat{1, 1};
        voice.events[0].payload = ng;
    }

    // Piano part has no notes — Keyboard family should be omitted
    auto result = short_score(score);

    // Should have 3 parts: Strings, Woodwinds, Brass
    CHECK(result.parts.size() == 3);

    bool found_strings = false, found_woodwinds = false, found_brass = false;
    for (const auto& part : result.parts) {
        if (part.definition.name == "Strings") found_strings = true;
        if (part.definition.name == "Woodwinds") found_woodwinds = true;
        if (part.definition.name == "Brass") found_brass = true;
    }
    CHECK(found_strings);
    CHECK(found_woodwinds);
    CHECK(found_brass);
}

TEST_CASE("SIVW001A: short_score merges two instruments in same family",
          "[score-ir][view]") {
    auto score = make_valid_score(1);

    // Remove the default piano
    score.parts.clear();

    // Add Violin and Viola (both Strings)
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 0);

    PartDefinition viola_def;
    viola_def.name = "Viola";
    viola_def.abbreviation = "Vla.";
    viola_def.instrument_type = InstrumentType::Viola;
    (void)add_part(score, viola_def, 1);

    // Place high note in Violin, low note in Viola
    auto& vln_voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5 (MIDI 79)
    ng1.duration = Beat{1, 1};
    vln_voice.events[0].payload = ng1;

    auto& vla_voice = score.parts[1].measures[0].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}});  // C3 (MIDI 48)
    ng2.duration = Beat{1, 1};
    vla_voice.events[0].payload = ng2;

    auto result = short_score(score);

    // Should merge into a single "Strings" part
    REQUIRE(result.parts.size() == 1);
    CHECK(result.parts[0].definition.name == "Strings");

    // Two voices: treble (G5) and bass (C3)
    REQUIRE(result.parts[0].measures[0].voices.size() == 2);
}

TEST_CASE("SIVW001A: short_score splits notes at C4 boundary",
          "[score-ir][view]") {
    auto score = make_valid_score(1);

    // Place notes spanning the C4 boundary in the Piano part
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // G5 (MIDI 79) → treble, B3 (MIDI 59) → bass
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5
    ng.notes.push_back(Note{SpelledPitch{6, 0, 3}, VelocityValue{{}, 80}});  // B3
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{9001}, Beat::zero(), ng});

    auto result = short_score(score);

    REQUIRE(result.parts.size() == 1);
    auto& treble = result.parts[0].measures[0].voices[0];
    auto& bass = result.parts[0].measures[0].voices[1];

    // Treble should have G5
    auto* treble_ng = treble.events[0].as_note_group();
    if (treble_ng) {
        CHECK(midi_value(treble_ng->notes[0].pitch) >= 60);
    }

    // Bass should have B3
    bool bass_has_note = false;
    for (const auto& ev : bass.events) {
        auto* bng = ev.as_note_group();
        if (bng) {
            CHECK(midi_value(bng->notes[0].pitch) < 60);
            bass_has_note = true;
        }
    }
    CHECK(bass_has_note);
}

TEST_CASE("SIVW001A: short_score omits families with only rests",
          "[score-ir][view]") {
    auto score = make_valid_score(1);

    // Add a Trumpet part with no notes
    PartDefinition trumpet_def;
    trumpet_def.name = "Trumpet";
    trumpet_def.abbreviation = "Tpt.";
    trumpet_def.instrument_type = InstrumentType::Trumpet;
    (void)add_part(score, trumpet_def, 1);

    // Place a note in the Piano (Keyboard family) only
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    auto result = short_score(score);

    // Only the Keyboard family should appear; Brass family omitted
    REQUIRE(result.parts.size() == 1);
    CHECK(result.parts[0].definition.name == "Keyboard");
}

// =============================================================================
// SIVW001A: short_score inner-voice marking (§8.2.2)
// =============================================================================

TEST_CASE("SIVW001A: short_score 3-note chord marks middle as Cue",
          "[score-ir][view][short-score]") {
    auto score = make_valid_score(1);
    score.parts.clear();

    // Violin with 3-note chord spanning the treble register
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 0);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5 (MIDI 72)
    ng.notes.push_back(Note{SpelledPitch{2, 0, 5}, VelocityValue{{}, 80}});  // E5 (MIDI 76)
    ng.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5 (MIDI 79)
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{12001}, Beat::zero(), ng});

    auto result = short_score(score);
    REQUIRE(result.parts.size() == 1);

    // Treble voice should have the 3 notes; middle one (E5) should be Cue
    auto& treble = result.parts[0].measures[0].voices[0];
    int cue_count = 0;
    int normal_count = 0;
    for (const auto& ev : treble.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (!ng_ptr) continue;
        for (const auto& note : ng_ptr->notes) {
            if (note.notation_head == NoteHeadType::Cue) ++cue_count;
            else ++normal_count;
        }
    }
    CHECK(cue_count == 1);    // E5 is the inner voice
    CHECK(normal_count == 2); // C5 (lowest) and G5 (highest)
}

TEST_CASE("SIVW001A: short_score inner note has NoteHeadType::Cue",
          "[score-ir][view][short-score]") {
    auto score = make_valid_score(1);
    score.parts.clear();

    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 0);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5 (low)
    ng.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}});  // D5 (inner)
    ng.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5 (high)
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{12010}, Beat::zero(), ng});

    auto result = short_score(score);
    auto& treble = result.parts[0].measures[0].voices[0];
    for (const auto& ev : treble.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (!ng_ptr) continue;
        for (const auto& note : ng_ptr->notes) {
            int mv = midi_value(note.pitch);
            if (mv == midi_value(SpelledPitch{1, 0, 5})) {
                CHECK(note.notation_head == NoteHeadType::Cue);
            }
        }
    }
}

TEST_CASE("SIVW001A: short_score single note retains Normal notehead",
          "[score-ir][view][short-score]") {
    auto score = make_valid_score(1);
    score.parts.clear();

    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 0);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{12020}, Beat::zero(), ng});

    auto result = short_score(score);
    auto& treble = result.parts[0].measures[0].voices[0];
    for (const auto& ev : treble.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (!ng_ptr) continue;
        for (const auto& note : ng_ptr->notes) {
            CHECK(note.notation_head != NoteHeadType::Cue);
        }
    }
}

TEST_CASE("SIVW001A: short_score two notes at offset both Normal",
          "[score-ir][view][short-score]") {
    auto score = make_valid_score(1);
    score.parts.clear();

    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 0);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{12030}, Beat::zero(), ng});

    auto result = short_score(score);
    auto& treble = result.parts[0].measures[0].voices[0];
    for (const auto& ev : treble.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (!ng_ptr) continue;
        for (const auto& note : ng_ptr->notes) {
            CHECK(note.notation_head != NoteHeadType::Cue);
        }
    }
}

// =============================================================================
// SIVW001A: piano_reduction fidelity (§8.2.1)
// =============================================================================

TEST_CASE("SIVW001A: piano_reduction melody note preserved as top voice with orchestration",
          "[score-ir][view][reduction]") {
    auto score = make_two_part_score();

    // Annotate violin as Melody role
    OrchestrationAnnotation oa;
    oa.part_id = score.parts[1].id;  // Violin
    oa.start = SCORE_START;
    oa.end = ScoreTime{5, Beat::zero()};
    oa.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa);

    auto reduced = piano_reduction(score);
    REQUIRE(reduced.parts.size() == 1);

    // Bar 1 treble voice should contain the violin's E5 melody note
    auto& treble = reduced.parts[0].measures[0].voices[0];
    bool found_e5 = false;
    for (const auto& ev : treble.events) {
        const auto* ng = ev.as_note_group();
        if (!ng) continue;
        for (const auto& note : ng->notes) {
            // E5 = letter 2, octave 5 (MIDI 76)
            if (note.pitch.letter == 2 && note.pitch.octave == 5)
                found_e5 = true;
        }
    }
    CHECK(found_e5);
}

TEST_CASE("SIVW001A: piano_reduction removes octave doublings",
          "[score-ir][view][reduction]") {
    auto score = make_valid_score(1);

    // Add a second part with an octave doubling
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 1);

    // Piano: C4 (MIDI 60)
    auto& piano_voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 1};
    piano_voice.events[0].payload = ng1;

    // Violin: C5 (MIDI 72) — same pitch class, different octave
    auto& vln_voice = score.parts[1].measures[0].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 1};
    vln_voice.events[0].payload = ng2;

    // Add orchestration annotations to trigger post-processing
    OrchestrationAnnotation oa;
    oa.part_id = score.parts[1].id;
    oa.start = SCORE_START;
    oa.end = ScoreTime{2, Beat::zero()};
    oa.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa);

    auto reduced = piano_reduction(score);

    // Treble voice: C4 and C5 are same PC. After dedup, only one should remain.
    auto& treble = reduced.parts[0].measures[0].voices[0];
    int c_count = 0;
    for (const auto& ev : treble.events) {
        const auto* ng = ev.as_note_group();
        if (!ng) continue;
        for (const auto& note : ng->notes) {
            if (midi_value(note.pitch) % 12 == 0) ++c_count;
        }
    }
    CHECK(c_count == 1);
}

TEST_CASE("SIVW001A: piano_reduction limits to 4 voices per staff",
          "[score-ir][view][reduction]") {
    auto score = make_valid_score(1);

    // Create 6 notes above MIDI 60 from multiple parts
    auto& piano_voice = score.parts[0].measures[0].voices[0];
    piano_voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}});  // D5
    ng.notes.push_back(Note{SpelledPitch{2, 0, 5}, VelocityValue{{}, 80}});  // E5
    ng.notes.push_back(Note{SpelledPitch{3, 0, 5}, VelocityValue{{}, 80}});  // F5
    ng.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5
    ng.notes.push_back(Note{SpelledPitch{5, 0, 5}, VelocityValue{{}, 80}});  // A5
    ng.duration = Beat{1, 1};
    piano_voice.events.push_back(Event{EventId{11001}, Beat::zero(), ng});

    // Add orchestration to trigger post-processing
    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};
    oa.start = SCORE_START;
    oa.end = ScoreTime{2, Beat::zero()};
    oa.role = TexturalRole::HarmonicFill;
    score.orchestration_annotations.push_back(oa);

    auto reduced = piano_reduction(score);

    // Count notes in treble voice at offset 0
    auto& treble = reduced.parts[0].measures[0].voices[0];
    int note_count = 0;
    for (const auto& ev : treble.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (ng_ptr) note_count += static_cast<int>(ng_ptr->notes.size());
    }
    CHECK(note_count <= 4);
}

TEST_CASE("SIVW001A: piano_reduction bass line preserved as bottom voice",
          "[score-ir][view][reduction]") {
    auto score = make_two_part_score();

    // Annotate piano as BassLine role
    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};  // Piano — has C4 (MIDI 60)
    oa.start = SCORE_START;
    oa.end = ScoreTime{5, Beat::zero()};
    oa.role = TexturalRole::BassLine;
    score.orchestration_annotations.push_back(oa);

    // Add a low note to piano for the bass staff
    auto& piano_voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}});  // C3 (MIDI 48)
    ng.duration = Beat{1, 1};
    piano_voice.events[0].payload = ng;

    auto reduced = piano_reduction(score);

    // Bass voice should contain C3
    auto& bass = reduced.parts[0].measures[0].voices[1];
    bool found_c3 = false;
    for (const auto& ev : bass.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (!ng_ptr) continue;
        for (const auto& note : ng_ptr->notes) {
            if (note.pitch.letter == 0 && note.pitch.octave == 3) found_c3 = true;
        }
    }
    CHECK(found_c3);
}

TEST_CASE("SIVW001A: piano_reduction fallback without orchestration retains all notes",
          "[score-ir][view][reduction]") {
    auto score = make_two_part_score();

    // No orchestration annotations — should behave as before (no filtering)
    auto reduced = piano_reduction(score);
    REQUIRE(reduced.parts.size() == 1);

    // Both C4 (Piano) and E5 (Violin) should appear
    bool found_c4 = false;
    bool found_e5 = false;
    for (const auto& m : reduced.parts[0].measures) {
        for (const auto& v : m.voices) {
            for (const auto& ev : v.events) {
                const auto* ng = ev.as_note_group();
                if (!ng) continue;
                for (const auto& note : ng->notes) {
                    int mv = midi_value(note.pitch);
                    if (mv == 60) found_c4 = true;
                    if (note.pitch.letter == 2 && note.pitch.octave == 5) found_e5 = true;
                }
            }
        }
    }
    CHECK(found_c4);
    CHECK(found_e5);
}

// =============================================================================
// SIQR001A: Extended Queries (§12.1, continued)
// =============================================================================

// =============================================================================
// SIVW001A: part_extract cue notes (§8.2.3)
// =============================================================================

namespace {

/// Build a two-part score where the second part has long rest runs.
/// Part 0 (Flute) has notes in every bar; Part 1 (Clarinet) has rests.
Score make_cue_note_test_score(std::uint32_t total_bars = 8) {
    Score score;
    score.id = ScoreId{50};
    score.metadata.title = "Cue Test";
    score.metadata.total_bars = total_bars;

    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    auto ts = make_time_signature(4, 4);
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    // Part 0: Flute with E5 in every bar
    Part flute;
    flute.id = PartId{200};
    flute.definition.name = "Flute";
    flute.definition.abbreviation = "Fl.";
    flute.definition.instrument_type = InstrumentType::Flute;
    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        NoteGroup ng;
        ng.notes.push_back(Note{SpelledPitch{2, 0, 5}, VelocityValue{{}, 80}});  // E5
        ng.duration = Beat{1, 1};
        Event ev{EventId{bar * 100}, Beat::zero(), ng};
        Voice voice{0, {ev}, {}};
        Measure measure{bar, {voice}, std::nullopt, std::nullopt};
        flute.measures.push_back(measure);
    }
    score.parts.push_back(flute);

    // Part 1: Clarinet with all rests
    Part clarinet;
    clarinet.id = PartId{201};
    clarinet.definition.name = "Clarinet";
    clarinet.definition.abbreviation = "Cl.";
    clarinet.definition.instrument_type = InstrumentType::Clarinet;
    for (std::uint32_t bar = 1; bar <= total_bars; ++bar) {
        RestEvent rest{ts->measure_duration(), true};
        Event ev{EventId{bar * 100 + 50}, Beat::zero(), rest};
        Voice voice{0, {ev}, {}};
        Measure measure{bar, {voice}, std::nullopt, std::nullopt};
        clarinet.measures.push_back(measure);
    }
    score.parts.push_back(clarinet);

    return score;
}

}  // anonymous namespace

TEST_CASE("SIVW001A: part_extract inserts cue note after 4-bar rest (default threshold)",
          "[score-ir][view][cue]") {
    auto score = make_cue_note_test_score(8);

    // Clarinet has 8 bars of rest; flute has notes in every bar.
    // With threshold 4, a cue note should appear in the last rest bar
    // before the run breaks (here, entire score is rests so cue goes in bar 8).
    auto extracted = part_extract(score, PartId{201});

    REQUIRE(extracted.parts.size() == 1);
    auto& cl = extracted.parts[0];

    // The last bar (bar 8) should have a cue note inserted
    auto& last_voice = cl.measures[7].voices[0];
    bool found_cue = false;
    for (const auto& ev : last_voice.events) {
        const auto* ng = ev.as_note_group();
        if (ng && !ng->notes.empty() &&
            ng->notes[0].notation_head == NoteHeadType::Cue) {
            found_cue = true;
        }
    }
    CHECK(found_cue);
}

TEST_CASE("SIVW001A: part_extract no cue note for 3-bar rest (below threshold)",
          "[score-ir][view][cue]") {
    auto score = make_cue_note_test_score(8);

    // Put a note in clarinet bar 1, so bars 2-4 are only 3 rests before bar 5 note
    auto& cl_voice_b1 = score.parts[1].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    cl_voice_b1.events[0].payload = ng;

    // Also put a note in clarinet bar 5
    auto& cl_voice_b5 = score.parts[1].measures[4].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 1};
    cl_voice_b5.events[0].payload = ng2;

    auto extracted = part_extract(score, PartId{201});
    auto& cl = extracted.parts[0];

    // Bars 2-4 are 3 rests (below threshold of 4). No cue note expected there.
    for (std::uint32_t bar = 2; bar <= 4; ++bar) {
        auto& voice = cl.measures[bar - 1].voices[0];
        for (const auto& ev : voice.events) {
            const auto* ng_ptr = ev.as_note_group();
            if (ng_ptr && !ng_ptr->notes.empty()) {
                CHECK(ng_ptr->notes[0].notation_head != NoteHeadType::Cue);
            }
        }
    }
}

TEST_CASE("SIVW001A: part_extract custom threshold of 2 triggers cue insertion",
          "[score-ir][view][cue]") {
    auto score = make_cue_note_test_score(8);

    // Place a note in clarinet bar 1 and bar 4, leaving bars 2-3 as 2-bar rest run
    auto& cl_b1 = score.parts[1].measures[0].voices[0];
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 1};
    cl_b1.events[0].payload = ng1;

    auto& cl_b4 = score.parts[1].measures[3].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 1};
    cl_b4.events[0].payload = ng2;

    // Threshold of 2: bars 2-3 form a 2-bar rest run, which meets the threshold
    auto extracted = part_extract(score, PartId{201}, 2);
    auto& cl = extracted.parts[0];

    // Cue note should be in bar 3 (last rest bar before bar 4's note)
    auto& cue_voice = cl.measures[2].voices[0];
    bool found_cue = false;
    for (const auto& ev : cue_voice.events) {
        const auto* ng_ptr = ev.as_note_group();
        if (ng_ptr && !ng_ptr->notes.empty() &&
            ng_ptr->notes[0].notation_head == NoteHeadType::Cue) {
            found_cue = true;
        }
    }
    CHECK(found_cue);
}

TEST_CASE("SIVW001A: part_extract cue note has NoteHeadType::Cue",
          "[score-ir][view][cue]") {
    auto score = make_cue_note_test_score(8);

    auto extracted = part_extract(score, PartId{201});
    auto& cl = extracted.parts[0];

    // Find any cue note and verify its notation_head
    bool found = false;
    for (const auto& m : cl.measures) {
        for (const auto& v : m.voices) {
            for (const auto& ev : v.events) {
                const auto* ng = ev.as_note_group();
                if (!ng) continue;
                for (const auto& note : ng->notes) {
                    if (note.notation_head == NoteHeadType::Cue) {
                        found = true;
                    }
                }
            }
        }
    }
    CHECK(found);
}

TEST_CASE("SIVW001A: part_extract cue pitch matches melody part with orchestration",
          "[score-ir][view][cue]") {
    auto score = make_cue_note_test_score(8);

    // Annotate flute as Melody role across the entire score
    OrchestrationAnnotation oa;
    oa.part_id = PartId{200};  // Flute
    oa.start = SCORE_START;
    oa.end = ScoreTime{9, Beat::zero()};
    oa.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa);

    auto extracted = part_extract(score, PartId{201});
    auto& cl = extracted.parts[0];

    // The cue note pitch should come from the flute melody (E5)
    for (const auto& m : cl.measures) {
        for (const auto& v : m.voices) {
            for (const auto& ev : v.events) {
                const auto* ng = ev.as_note_group();
                if (!ng) continue;
                for (const auto& note : ng->notes) {
                    if (note.notation_head == NoteHeadType::Cue) {
                        // Flute plays E5 (letter=2, octave=5)
                        CHECK(note.pitch.letter == 2);
                        CHECK(note.pitch.octave == 5);
                    }
                }
            }
        }
    }
}

TEST_CASE("SIQR001A: query_find_motif locates pitch class pattern",
          "[score-ir][query]") {
    auto score = make_valid_score(4);

    // Place notes: C4, D4, E4, C4 across bars 1-2
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // C4 (PC 0)
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8001}, Beat::zero(), ng1});

    // D4 (PC 2)
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8002}, Beat{1, 4}, ng2});

    // E4 (PC 4)
    NoteGroup ng3;
    ng3.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng3.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8003}, Beat{1, 2}, ng3});

    // C4 again (PC 0)
    NoteGroup ng4;
    ng4.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng4.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8004}, Beat{3, 4}, ng4});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    // Search for C-D-E (PCs 0, 2, 4)
    std::vector<PitchClass> pattern = {0, 2, 4};
    auto occurrences = query_find_motif(score, pattern, region);
    REQUIRE(occurrences.size() == 1);
    CHECK(occurrences[0].part_id == PartId{100});
    CHECK(occurrences[0].position == SCORE_START);

    // Search for pattern not present
    std::vector<PitchClass> absent = {0, 1, 2};
    auto none = query_find_motif(score, absent, region);
    CHECK(none.empty());
}
