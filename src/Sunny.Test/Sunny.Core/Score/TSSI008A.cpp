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
