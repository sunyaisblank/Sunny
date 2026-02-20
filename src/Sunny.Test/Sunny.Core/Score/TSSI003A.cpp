/**
 * @file TSSI003A.cpp
 * @brief Unit tests for Score IR validation (SIVD001A) and mutations (SIMT001A)
 *
 * Component: TSSI003A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIVD001A, SIMT001A
 * Coverage: validate_score, validate_structural, is_compilable,
 *           insert_note, delete_event, modify_pitch, insert_measures,
 *           delete_measures, add_part, remove_part, transpose_region
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIVD001A.h"
#include "Score/SIMT001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

/// Create a minimal valid Score
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

}  // anonymous namespace

// =============================================================================
// SIVD001A: Structural Validation
// =============================================================================

TEST_CASE("SIVD001A: valid score produces no errors", "[score-ir][validation]") {
    auto score = make_valid_score();
    auto diags = validate_structural(score);
    CHECK(diags.empty());
    CHECK(is_compilable(score));
}

TEST_CASE("SIVD001A: S0 — empty parts", "[score-ir][validation]") {
    Score score;
    score.id = ScoreId{1};
    score.metadata.total_bars = 4;
    // No parts

    auto diags = validate_structural(score);
    REQUIRE_FALSE(diags.empty());
    CHECK(diags[0].rule == "S0");
    CHECK(diags[0].severity == ValidationSeverity::Error);
}

TEST_CASE("SIVD001A: S1 — measure count mismatch", "[score-ir][validation]") {
    auto score = make_valid_score(4);
    // Remove one measure from the part
    score.parts[0].measures.pop_back();

    auto diags = validate_structural(score);
    bool found_s1 = false;
    for (const auto& d : diags) {
        if (d.rule == "S1") { found_s1 = true; break; }
    }
    CHECK(found_s1);
}

TEST_CASE("SIVD001A: S2 — measure fill error", "[score-ir][validation]") {
    auto score = make_valid_score(1);
    // Replace the rest with a shorter rest (doesn't fill measure)
    auto& voice = score.parts[0].measures[0].voices[0];
    RestEvent short_rest{Beat{1, 4}, true};  // Quarter note rest in 4/4
    voice.events[0].payload = short_rest;

    auto diags = validate_structural(score);
    bool found_s2 = false;
    for (const auto& d : diags) {
        if (d.rule == "S2") { found_s2 = true; break; }
    }
    CHECK(found_s2);
}

TEST_CASE("SIVD001A: S4 — missing tempo at bar 1", "[score-ir][validation]") {
    auto score = make_valid_score();
    score.tempo_map.clear();

    auto diags = validate_structural(score);
    bool found_s4 = false;
    for (const auto& d : diags) {
        if (d.rule == "S4") { found_s4 = true; break; }
    }
    CHECK(found_s4);
}

TEST_CASE("SIVD001A: S5 — missing time sig at bar 1", "[score-ir][validation]") {
    auto score = make_valid_score();
    score.time_map.clear();

    auto diags = validate_structural(score);
    bool found_s5 = false;
    for (const auto& d : diags) {
        if (d.rule == "S5") { found_s5 = true; break; }
    }
    CHECK(found_s5);
}

TEST_CASE("SIVD001A: S6 — missing key sig at bar 1", "[score-ir][validation]") {
    auto score = make_valid_score();
    score.key_map.clear();

    auto diags = validate_structural(score);
    bool found_s6 = false;
    for (const auto& d : diags) {
        if (d.rule == "S6") { found_s6 = true; break; }
    }
    CHECK(found_s6);
}

TEST_CASE("SIVD001A: S0b — empty voices", "[score-ir][validation]") {
    auto score = make_valid_score(1);
    score.parts[0].measures[0].voices.clear();

    auto diags = validate_structural(score);
    bool found_s0b = false;
    for (const auto& d : diags) {
        if (d.rule == "S0b") { found_s0b = true; break; }
    }
    CHECK(found_s0b);
}

// =============================================================================
// SIVD001A: Musical Validation
// =============================================================================

TEST_CASE("SIVD001A: M8 — stale harmonic annotations", "[score-ir][validation]") {
    auto score = make_valid_score(1);
    // Add a note event so score has content
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;
    // No harmonic annotations → M8 warning

    auto diags = validate_musical(score);
    bool found_m8 = false;
    for (const auto& d : diags) {
        if (d.rule == "M8") { found_m8 = true; break; }
    }
    CHECK(found_m8);
}

// =============================================================================
// SIVD001A: Rendering Validation
// =============================================================================

TEST_CASE("SIVD001A: R1 — missing instrument preset", "[score-ir][validation]") {
    auto score = make_valid_score();
    // Part has no instrument preset by default

    auto diags = validate_rendering(score);
    bool found_r1 = false;
    for (const auto& d : diags) {
        if (d.rule == "R1") { found_r1 = true; break; }
    }
    CHECK(found_r1);
}

// =============================================================================
// SIMT001A: Event-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: insert_note", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // First, replace the whole-note rest with appropriate events
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Insert a rest for 3/4 of the measure
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{500}, Beat::zero(), rest});

    // Insert C4 quarter note at beat 3/4
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};  // C4
    note.velocity = VelocityValue{{}, 80};

    auto result = insert_note(
        score, PartId{100}, 1, 0,
        Beat{3, 4}, note, Beat{1, 4}
    );
    REQUIRE(result.has_value());

    // Verify the note was inserted
    CHECK(voice.events.size() == 2);
    CHECK(voice.events[1].is_note_group());
    CHECK(score.version == 2);  // Version incremented
}

TEST_CASE("SIMT001A: delete_event replaces with rest", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Replace rest with a note first
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    auto result = delete_event(score, target);
    REQUIRE(result.has_value());

    // Should now be a rest
    CHECK(voice.events[0].is_rest());
    CHECK(voice.events[0].duration() == Beat{1, 1});
}

TEST_CASE("SIMT001A: modify_pitch", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Set up a note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    SpelledPitch new_pitch{4, 0, 4};  // G4

    auto result = modify_pitch(score, target, 0, new_pitch);
    REQUIRE(result.has_value());

    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch == new_pitch);
}

TEST_CASE("SIMT001A: modify_duration", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;
    auto result = modify_duration(score, target, Beat{1, 2});
    REQUIRE(result.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == Beat{1, 2});
}

// =============================================================================
// SIMT001A: Measure-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: insert_measures", "[score-ir][mutation]") {
    auto score = make_valid_score(4);
    CHECK(score.metadata.total_bars == 4);
    CHECK(score.parts[0].measures.size() == 4);

    auto result = insert_measures(score, 2, 2);
    REQUIRE(result.has_value());

    CHECK(score.metadata.total_bars == 6);
    CHECK(score.parts[0].measures.size() == 6);

    // Verify bar numbering
    for (std::uint32_t i = 0; i < score.parts[0].measures.size(); ++i) {
        CHECK(score.parts[0].measures[i].bar_number == i + 1);
    }
}

TEST_CASE("SIMT001A: delete_measures", "[score-ir][mutation]") {
    auto score = make_valid_score(4);

    auto result = delete_measures(score, 2, 2);
    REQUIRE(result.has_value());

    CHECK(score.metadata.total_bars == 2);
    CHECK(score.parts[0].measures.size() == 2);

    // Cannot delete all measures
    auto fail = delete_measures(score, 1, 2);
    CHECK_FALSE(fail.has_value());
}

TEST_CASE("SIMT001A: set_time_signature", "[score-ir][mutation]") {
    auto score = make_valid_score();

    auto ts_3_4 = make_time_signature(3, 4);
    REQUIRE(ts_3_4.has_value());

    auto result = set_time_signature(score, 3, *ts_3_4);
    REQUIRE(result.has_value());

    // Should have two entries now: bar 1 (4/4) and bar 3 (3/4)
    CHECK(score.time_map.size() == 2);
    CHECK(score.time_map[1].bar == 3);
    CHECK(score.time_map[1].time_signature.numerator() == 3);
}

// =============================================================================
// SIMT001A: Part-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: add_part", "[score-ir][mutation]") {
    auto score = make_valid_score();

    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;

    auto result = add_part(score, violin_def, 0);
    REQUIRE(result.has_value());

    CHECK(score.parts.size() == 2);
    CHECK(score.parts[0].definition.name == "Violin I");
    CHECK(score.parts[0].measures.size() == score.metadata.total_bars);
}

TEST_CASE("SIMT001A: remove_part", "[score-ir][mutation]") {
    auto score = make_valid_score();

    // Add a second part first
    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;
    (void)add_part(score, violin_def, 1);

    CHECK(score.parts.size() == 2);
    PartId to_remove = score.parts[1].id;

    auto result = remove_part(score, to_remove);
    REQUIRE(result.has_value());
    CHECK(score.parts.size() == 1);

    // Cannot remove the last part
    auto fail = remove_part(score, score.parts[0].id);
    CHECK_FALSE(fail.has_value());
}

// =============================================================================
// SIMT001A: Region-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: transpose_region", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Place a C4 quarter note at bar 1
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};
    // Empty parts = all parts

    // Transpose up a major second (2 semitones, 1 diatonic step)
    auto result = transpose_region(score, region, MAJOR_SECOND);
    REQUIRE(result.has_value());

    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    // C4 + M2 = D4
    CHECK(updated->notes[0].pitch.letter == 1);  // D
    CHECK(updated->notes[0].pitch.accidental == 0);
    CHECK(updated->notes[0].pitch.octave == 4);
}

TEST_CASE("SIMT001A: delete_region replaces with rests", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Place a note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = delete_region(score, region);
    REQUIRE(result.has_value());

    CHECK(voice.events[0].is_rest());
}

// =============================================================================
// SIMT001A: Version counter
// =============================================================================

TEST_CASE("SIMT001A: version increments on mutation", "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    CHECK(score.version == 1);

    auto result = modify_duration(
        score,
        score.parts[0].measures[0].voices[0].events[0].id,
        Beat{1, 2}
    );
    REQUIRE(result.has_value());
    CHECK(score.version == 2);

    // Another mutation
    result = modify_duration(
        score,
        score.parts[0].measures[0].voices[0].events[0].id,
        Beat{1, 4}
    );
    REQUIRE(result.has_value());
    CHECK(score.version == 3);
}
