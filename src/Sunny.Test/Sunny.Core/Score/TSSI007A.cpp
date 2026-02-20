/**
 * @file TSSI007A.cpp
 * @brief Unit tests for Score IR undo stack (SIMT001A)
 *
 * Component: TSSI007A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIMT001A (undo/redo)
 * Coverage: modify_pitch undo, modify_duration undo, insert_note undo,
 *           empty stack, redo clear on new mutation, transpose_region undo,
 *           redo restores pitch, set_dynamic undo, insert_hairpin undo,
 *           delete_measures undo, remove_part undo, retrograde self-inverse,
 *           assign_instrument undo, version monotonicity through undo-redo
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIMT001A.h"
#include "Score/SIHA001A.h"

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

}  // anonymous namespace

// =============================================================================
// Undo Stack: Initial State
// =============================================================================

TEST_CASE("SIMT001A: undo stack is empty initially", "[score-ir][undo]") {
    UndoStack stack;
    CHECK_FALSE(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
}

TEST_CASE("SIMT001A: undo on empty stack fails", "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto result = undo(score, stack);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Undo: modify_pitch
// =============================================================================

TEST_CASE("SIMT001A: modify_pitch with undo restores original pitch",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Set up a C4 note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    SpelledPitch original_pitch{0, 0, 4};  // C4
    SpelledPitch new_pitch{4, 0, 4};       // G4

    // Modify pitch with undo tracking
    auto result = modify_pitch(score, target, 0, new_pitch, &stack);
    REQUIRE(result.has_value());
    CHECK(stack.can_undo());

    // Verify pitch changed
    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch == new_pitch);

    // Undo
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    // Verify pitch restored
    updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch == original_pitch);
    CHECK_FALSE(stack.can_undo());
    CHECK(stack.can_redo());
}

// =============================================================================
// Undo: modify_duration
// =============================================================================

TEST_CASE("SIMT001A: modify_duration with undo restores original duration",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;
    Beat original_dur = score.parts[0].measures[0].voices[0].events[0].duration();

    auto result = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == Beat{1, 2});

    // Undo
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == original_dur);
}

// =============================================================================
// Undo: insert_note
// =============================================================================

TEST_CASE("SIMT001A: insert_note with undo removes the inserted event",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Replace the whole-note rest with a 3/4 rest
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{500}, Beat::zero(), rest});

    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{{}, 80};

    auto result = insert_note(
        score, PartId{100}, 1, 0,
        Beat{3, 4}, note, Beat{1, 4}, &stack
    );
    REQUIRE(result.has_value());
    CHECK(voice.events.size() == 2);
    CHECK(stack.can_undo());

    // Undo should remove the inserted note
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    // After undo, the voice should revert (event count may change)
    // The inserted note should be gone; verify no note group remains at offset 3/4
    bool has_note_at_34 = false;
    for (const auto& ev : voice.events) {
        if (ev.is_note_group() && ev.offset == Beat{3, 4}) {
            has_note_at_34 = true;
        }
    }
    CHECK_FALSE(has_note_at_34);
}

// =============================================================================
// Undo + Redo: new mutation clears redo stack
// =============================================================================

TEST_CASE("SIMT001A: new mutation after undo clears redo stack",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Set up a note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;

    // Mutation 1: change pitch to G4
    auto r1 = modify_pitch(score, target, 0, SpelledPitch{4, 0, 4}, &stack);
    REQUIRE(r1.has_value());

    // Undo — now redo stack should have one entry
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(stack.can_redo());

    // New mutation: change duration
    auto r2 = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(r2.has_value());

    // Redo stack should be empty after a new mutation
    CHECK_FALSE(stack.can_redo());
    CHECK(stack.can_undo());
}

// =============================================================================
// Undo: transpose_region
// =============================================================================

TEST_CASE("SIMT001A: transpose_region with undo restores original pitches",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Place a C4 whole note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    // Transpose up a major second
    auto result = transpose_region(score, region, MAJOR_SECOND, &stack);
    REQUIRE(result.has_value());

    // Should now be D4
    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch.letter == 1);  // D
    CHECK(updated->notes[0].pitch.octave == 4);

    // Undo
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    // Should be back to C4
    updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch.letter == 0);  // C
    CHECK(updated->notes[0].pitch.octave == 4);
}

// =============================================================================
// Redo
// =============================================================================

TEST_CASE("SIMT001A: redo succeeds and moves entry back to undo stack",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;

    // Mutation: change duration to half note
    auto r = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(r.has_value());

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(stack.can_redo());
    CHECK_FALSE(stack.can_undo());

    // Redo should succeed and transfer entry back to undo stack
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    CHECK_FALSE(stack.can_redo());
    CHECK(stack.can_undo());
}

// =============================================================================
// Version counter with undo
// =============================================================================

TEST_CASE("SIMT001A: version increases through undo and redo",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;
    CHECK(score.version == 1);

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;
    auto r = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(r.has_value());
    CHECK(score.version == 2);

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    // Version should increase monotonically (never reused), even on undo
    CHECK(score.version >= 3);
}

// =============================================================================
// Redo: modify_pitch restores post-mutation state
// =============================================================================

TEST_CASE("SIMT001A: redo after undo restores post-mutation pitch",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    SpelledPitch original_pitch{0, 0, 4};
    SpelledPitch new_pitch{1, 0, 4};  // D4

    auto r = modify_pitch(score, target, 0, new_pitch, &stack);
    REQUIRE(r.has_value());

    // Undo back to C4
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(voice.events[0].as_note_group()->notes[0].pitch == original_pitch);

    // Redo back to D4
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    CHECK(voice.events[0].as_note_group()->notes[0].pitch == new_pitch);
}

// =============================================================================
// Undo: set_dynamic
// =============================================================================

TEST_CASE("SIMT001A: set_dynamic undo restores original dynamic",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Place a note with mf dynamic
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.notes[0].dynamic = DynamicLevel::mf;
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    auto result = set_dynamic(score, PartId{100}, SCORE_START, DynamicLevel::ff, &stack);
    REQUIRE(result.has_value());

    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].dynamic == DynamicLevel::ff);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].dynamic == DynamicLevel::mf);
}

// =============================================================================
// Undo: insert_hairpin
// =============================================================================

TEST_CASE("SIMT001A: insert_hairpin undo removes the hairpin",
          "[score-ir][undo]") {
    auto score = make_valid_score(2);
    UndoStack stack;

    CHECK(score.parts[0].hairpins.empty());

    auto result = insert_hairpin(
        score, PartId{100},
        SCORE_START, ScoreTime{2, Beat::zero()},
        HairpinType::Crescendo, std::nullopt, &stack
    );
    REQUIRE(result.has_value());
    CHECK(score.parts[0].hairpins.size() == 1);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(score.parts[0].hairpins.empty());
}

// =============================================================================
// Undo: delete_measures
// =============================================================================

TEST_CASE("SIMT001A: delete_measures undo re-inserts measures with content",
          "[score-ir][undo]") {
    auto score = make_valid_score(4);
    UndoStack stack;

    // Place a note in bar 2
    auto& voice = score.parts[0].measures[1].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    CHECK(score.metadata.total_bars == 4);

    // Delete bars 2 and 3
    auto result = delete_measures(score, 2, 2, &stack);
    REQUIRE(result.has_value());
    CHECK(score.metadata.total_bars == 2);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(score.metadata.total_bars == 4);

    // Bar 2 should have its note restored
    auto* restored = score.parts[0].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch.letter == 2);  // E
}

// =============================================================================
// Undo: remove_part
// =============================================================================

TEST_CASE("SIMT001A: remove_part undo re-inserts part with content",
          "[score-ir][undo]") {
    auto score = make_valid_score(2);
    UndoStack stack;

    // Add a second part so we can remove the first
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7},
        SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}
    };
    auto add_result = add_part(score, violin_def, 1);
    REQUIRE(add_result.has_value());
    CHECK(score.parts.size() == 2);

    // Place a note in the piano part
    auto& piano_voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    piano_voice.events[0].payload = ng;

    // Remove the piano part
    auto result = remove_part(score, PartId{100}, &stack);
    REQUIRE(result.has_value());
    CHECK(score.parts.size() == 1);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(score.parts.size() == 2);

    // Find the piano part and verify its content
    bool found_piano = false;
    for (const auto& part : score.parts) {
        if (part.id == PartId{100}) {
            found_piano = true;
            auto* restored = part.measures[0].voices[0].events[0].as_note_group();
            REQUIRE(restored != nullptr);
            CHECK(restored->notes[0].pitch.letter == 0);  // C
        }
    }
    CHECK(found_piano);
}

// =============================================================================
// Undo: retrograde_region is self-inverse
// =============================================================================

TEST_CASE("SIMT001A: retrograde_region undo restores original order",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Place two quarter notes: C4 and E4
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 4};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng2.duration = Beat{1, 4};

    voice.events.push_back(Event{EventId{7001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{7002}, Beat{1, 4}, ng2});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    // Retrograde: should swap C4 and E4 payloads
    auto result = retrograde_region(score, region, &stack);
    REQUIRE(result.has_value());

    // First event should now have E4, second C4
    CHECK(voice.events[0].as_note_group()->notes[0].pitch.letter == 2);
    CHECK(voice.events[1].as_note_group()->notes[0].pitch.letter == 0);

    // Undo: retrograde is self-inverse
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    CHECK(voice.events[0].as_note_group()->notes[0].pitch.letter == 0);
    CHECK(voice.events[1].as_note_group()->notes[0].pitch.letter == 2);
}

// =============================================================================
// Undo: assign_instrument
// =============================================================================

TEST_CASE("SIMT001A: assign_instrument undo restores original instrument",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Piano);

    auto result = assign_instrument(score, PartId{100}, InstrumentType::Violin, &stack);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Violin);

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Piano);
}

// =============================================================================
// Version monotonicity through full undo-redo cycle
// =============================================================================

TEST_CASE("SIMT001A: version monotonically increases through undo-redo cycle",
          "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto v0 = score.version;

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;
    auto r1 = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(r1.has_value());
    auto v1 = score.version;
    CHECK(v1 > v0);

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    auto v2 = score.version;
    CHECK(v2 > v1);

    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    auto v3 = score.version;
    CHECK(v3 > v2);

    auto ur2 = undo(score, stack);
    REQUIRE(ur2.has_value());
    auto v4 = score.version;
    CHECK(v4 > v3);
}

// =============================================================================
// apply_voice_leading (§11.6)
// =============================================================================

namespace {

/// Create a score with harmonic annotations suitable for voice leading tests
Score make_voice_leading_score() {
    auto score = make_valid_score(2);

    // Bar 1: C major triad (C4, E4, G4)
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});  // E4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4
    ng1.duration = Beat{1, 1};
    voice.events[0].payload = ng1;

    // Bar 2: G major triad (G4, B4, D5) — to be voice-led
    auto& voice2 = score.parts[0].measures[1].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4
    ng2.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}});  // B4
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}});  // D5
    ng2.duration = Beat{1, 1};
    voice2.events[0].payload = ng2;

    // Add harmonic annotations for both bars
    HarmonicAnnotation ha1;
    ha1.position = SCORE_START;
    ha1.duration = Beat{1, 1};
    ha1.roman_numeral = "I";
    ha1.function = ScoreHarmonicFunction::Tonic;
    ha1.chord.root = 0;
    ha1.chord.quality = "major";
    ha1.chord.notes = {60, 64, 67};  // C4, E4, G4
    score.harmonic_annotations.push_back(ha1);

    HarmonicAnnotation ha2;
    ha2.position = ScoreTime{2, Beat::zero()};
    ha2.duration = Beat{1, 1};
    ha2.roman_numeral = "V";
    ha2.function = ScoreHarmonicFunction::Dominant;
    ha2.chord.root = 7;
    ha2.chord.quality = "major";
    ha2.chord.notes = {55, 59, 62};  // G3, B3, D4
    score.harmonic_annotations.push_back(ha2);

    return score;
}

}  // anonymous namespace

TEST_CASE("SIMT001A: apply_voice_leading NearestTone redistributes pitches",
          "[score-ir][voice-leading]") {
    auto score = make_voice_leading_score();

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(
        score, region, VoiceLeadingStyle::NearestTone);
    REQUIRE(result.has_value());

    // Bar 2 notes should now target G major pitch classes {7, 11, 2}
    auto* ng = score.parts[0].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    REQUIRE(ng->notes.size() == 3);

    // Each note's MIDI value should be a G major chord tone (mod 12)
    for (const auto& note : ng->notes) {
        int pc = midi_value(note.pitch) % 12;
        bool is_g_major_tone = (pc == 7 || pc == 11 || pc == 2);
        CHECK(is_g_major_tone);
    }
}

TEST_CASE("SIMT001A: apply_voice_leading undo restores original pitches",
          "[score-ir][voice-leading]") {
    auto score = make_voice_leading_score();
    UndoStack stack;

    // Capture original pitches
    auto* ng_before = score.parts[0].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(ng_before != nullptr);
    std::vector<SpelledPitch> original_pitches;
    for (const auto& note : ng_before->notes) {
        original_pitches.push_back(note.pitch);
    }

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(
        score, region, VoiceLeadingStyle::NearestTone, &stack);
    REQUIRE(result.has_value());
    CHECK(stack.can_undo());

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    // Pitches should be restored
    auto* ng_after = score.parts[0].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(ng_after != nullptr);
    REQUIRE(ng_after->notes.size() == original_pitches.size());
    for (std::size_t i = 0; i < original_pitches.size(); ++i) {
        CHECK(ng_after->notes[i].pitch == original_pitches[i]);
    }
}

TEST_CASE("SIMT001A: apply_voice_leading fails without harmonic annotations",
          "[score-ir][voice-leading]") {
    auto score = make_valid_score(2);

    // Place notes but no harmonic annotations
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(
        score, region, VoiceLeadingStyle::NearestTone);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SIMT001A: apply_voice_leading SmoothBach locks bass",
          "[score-ir][voice-leading]") {
    auto score = make_voice_leading_score();

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(
        score, region, VoiceLeadingStyle::SmoothBach);
    REQUIRE(result.has_value());

    // Bar 2 notes should target G major pitch classes
    auto* ng = score.parts[0].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    REQUIRE(!ng->notes.empty());

    // With lock_bass, the lowest note should be the chord root (G, pc 7)
    int lowest_midi = 127;
    for (const auto& note : ng->notes) {
        int mv = midi_value(note.pitch);
        if (mv < lowest_midi) lowest_midi = mv;
    }
    CHECK(lowest_midi % 12 == 7);  // G
}
