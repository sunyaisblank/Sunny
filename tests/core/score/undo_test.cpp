/**
 * @file undo_test.cpp
 * @brief Unit tests for Score IR undo stack
 *
 *
 * Coverage: modify_pitch undo, modify_duration undo, insert_note undo,
 *           empty stack, redo clear on new mutation, transpose_region undo,
 *           redo restores pitch, set_dynamic undo, insert_hairpin undo,
 *           delete_measures undo, remove_part undo, retrograde self-inverse,
 *           assign_instrument undo, version monotonicity through undo-redo
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/score/harmony_analysis.hpp>
#include <sunny/core/score/mutations.hpp>

using namespace sunny::core;

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
        SpelledPitch{5, 0, 0}, // A0
        SpelledPitch{0, 0, 8}, // C8
        SpelledPitch{0, 0, 3}, // C3
        SpelledPitch{0, 0, 6}  // C6
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

} // anonymous namespace

// =============================================================================
// Undo Stack: Initial State
// =============================================================================

TEST_CASE("undo stack is empty initially", "[score-ir][undo]") {
    UndoStack stack;
    CHECK_FALSE(stack.can_undo());
    CHECK_FALSE(stack.can_redo());
}

TEST_CASE("undo history keeps only the newest capacity snapshots", "[score-ir][undo][regression]") {
    // Five pitch changes C4 -> D4 -> E4 -> F4 -> G4 -> A4 with capacity 3 keep
    // the snapshots taken before the last three: E4, F4 and G4. Three undos
    // therefore return to E4 and a fourth has nothing left to restore.
    auto score = make_valid_score(1);
    UndoStack stack;
    stack.capacity = 3;
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup group;
    group.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    group.duration = Beat{1, 1};
    voice.events[0].payload = group;
    const EventId target = voice.events[0].id;

    for (std::uint8_t letter = 1; letter <= 5; ++letter)
        REQUIRE(modify_pitch(score, target, 0, SpelledPitch{letter, 0, 4}, &stack));
    CHECK(stack.undo_entries.size() == 3);

    for (int step = 0; step < 3; ++step)
        REQUIRE(undo(score, stack));
    CHECK(score.parts[0].measures[0].voices[0].events[0].as_note_group()->notes[0].pitch ==
          SpelledPitch{2, 0, 4});
    CHECK_FALSE(undo(score, stack).has_value());
}

TEST_CASE("undo on empty stack fails", "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto result = undo(score, stack);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Undo: modify_pitch
// =============================================================================

TEST_CASE("modify_pitch with undo restores original pitch", "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Set up a C4 note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    SpelledPitch original_pitch{0, 0, 4}; // C4
    SpelledPitch new_pitch{4, 0, 4};      // G4

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
    updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch == original_pitch);
    CHECK_FALSE(stack.can_undo());
    CHECK(stack.can_redo());
}

// =============================================================================
// Undo: modify_duration
// =============================================================================

TEST_CASE("modify_duration with undo restores original duration", "[score-ir][undo]") {
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

TEST_CASE("insert_note with undo removes the inserted event", "[score-ir][undo]") {
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

    auto result = insert_note(score, PartId{100}, 1, 0, Beat{3, 4}, note, Beat{1, 4}, &stack);
    REQUIRE(result.has_value());
    CHECK(voice.events.size() == 2);
    CHECK(stack.can_undo());

    // Undo should remove the inserted note
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    // After undo, the voice should revert (event count may change)
    // The inserted note should be gone; verify no note group remains at offset 3/4
    bool has_note_at_34 = false;
    for (const auto& ev : score.parts[0].measures[0].voices[0].events) {
        if (ev.is_note_group() && ev.offset == Beat{3, 4}) {
            has_note_at_34 = true;
        }
    }
    CHECK_FALSE(has_note_at_34);
}

// =============================================================================
// Undo + Redo: new mutation clears redo stack
// =============================================================================

TEST_CASE("new mutation after undo clears redo stack", "[score-ir][undo]") {
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

TEST_CASE("transpose_region with undo restores original pitches", "[score-ir][undo]") {
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
    CHECK(updated->notes[0].pitch.letter == 1); // D
    CHECK(updated->notes[0].pitch.octave == 4);

    // Undo
    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    // Should be back to C4
    updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].pitch.letter == 0); // C
    CHECK(updated->notes[0].pitch.octave == 4);
}

// =============================================================================
// Redo
// =============================================================================

TEST_CASE("redo succeeds and moves entry back to undo stack", "[score-ir][undo]") {
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

TEST_CASE("version increases through undo and redo", "[score-ir][undo]") {
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

TEST_CASE("redo after undo restores post-mutation pitch", "[score-ir][undo]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    SpelledPitch original_pitch{0, 0, 4};
    SpelledPitch new_pitch{1, 0, 4}; // D4

    auto r = modify_pitch(score, target, 0, new_pitch, &stack);
    REQUIRE(r.has_value());

    // Undo back to C4
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events[0].as_note_group()->notes[0].pitch ==
          original_pitch);

    // Redo back to D4
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events[0].as_note_group()->notes[0].pitch ==
          new_pitch);
}

// =============================================================================
// Undo: set_dynamic
// =============================================================================

TEST_CASE("set_dynamic undo restores original dynamic", "[score-ir][undo]") {
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

    updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->notes[0].dynamic == DynamicLevel::mf);
}

// =============================================================================
// Undo: insert_hairpin
// =============================================================================

TEST_CASE("insert_hairpin undo removes the hairpin", "[score-ir][undo]") {
    auto score = make_valid_score(2);
    UndoStack stack;

    CHECK(score.parts[0].hairpins.empty());

    auto result = insert_hairpin(score,
                                 PartId{100},
                                 SCORE_START,
                                 ScoreTime{2, Beat::zero()},
                                 HairpinType::Crescendo,
                                 std::nullopt,
                                 &stack);
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

TEST_CASE("delete_measures undo re-inserts measures with content", "[score-ir][undo]") {
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
    CHECK(restored->notes[0].pitch.letter == 2); // E
}

// =============================================================================
// Undo: remove_part
// =============================================================================

TEST_CASE("remove_part undo re-inserts part with content", "[score-ir][undo]") {
    auto score = make_valid_score(2);
    UndoStack stack;

    // Add a second part so we can remove the first
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}, SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}};
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
            CHECK(restored->notes[0].pitch.letter == 0); // C
        }
    }
    CHECK(found_piano);
}

// =============================================================================
// Undo: retrograde_region is self-inverse
// =============================================================================

TEST_CASE("retrograde_region undo restores original order", "[score-ir][undo]") {
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
    voice.events.push_back(Event{EventId{7003}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{1, Beat{1, 2}};

    // Retrograde: should swap C4 and E4 payloads
    auto result = retrograde_region(score, region, &stack);
    REQUIRE(result.has_value());

    // First event should now have E4, second C4
    CHECK(score.parts[0].measures[0].voices[0].events[0].as_note_group()->notes[0].pitch.letter ==
          2);
    CHECK(score.parts[0].measures[0].voices[0].events[1].as_note_group()->notes[0].pitch.letter ==
          0);

    // Undo: retrograde is self-inverse
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    CHECK(score.parts[0].measures[0].voices[0].events[0].as_note_group()->notes[0].pitch.letter ==
          0);
    CHECK(score.parts[0].measures[0].voices[0].events[1].as_note_group()->notes[0].pitch.letter ==
          2);
}

// =============================================================================
// Undo: assign_instrument
// =============================================================================

TEST_CASE("assign_instrument undo restores original instrument", "[score-ir][undo]") {
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

TEST_CASE("version monotonically increases through undo-redo cycle", "[score-ir][undo]") {
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

TEST_CASE("exhausted versions reject raw edits and history traversal atomically",
          "[score-ir][undo][version]") {
    constexpr auto exhausted = std::numeric_limits<std::uint64_t>::max();

    SECTION("raw mutation") {
        auto score = make_valid_score(1);
        UndoStack stack;
        score.version = exhausted;

        const auto before_instrument = score.parts[0].definition.instrument_type;
        const auto result = assign_instrument(score, PartId{100}, InstrumentType::Violin, &stack);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ArithmeticOverflow);
        CHECK(score.version == exhausted);
        CHECK(score.parts[0].definition.instrument_type == before_instrument);
        CHECK(stack.undo_entries.empty());
        CHECK(stack.redo_entries.empty());

        const auto invalid_target =
            assign_instrument(score, PartId{999}, InstrumentType::Violin, &stack);
        REQUIRE_FALSE(invalid_target.has_value());
        CHECK(invalid_target.error() == ErrorCode::ArithmeticOverflow);
    }

    SECTION("unavailable history precedes exhaustion") {
        auto score = make_valid_score(1);
        score.version = exhausted;
        UndoStack stack;

        const auto undo_result = undo(score, stack);
        const auto redo_result = redo(score, stack);

        REQUIRE_FALSE(undo_result.has_value());
        REQUIRE_FALSE(redo_result.has_value());
        CHECK(undo_result.error() == ErrorCode::InvalidMutation);
        CHECK(redo_result.error() == ErrorCode::InvalidMutation);
        CHECK(score.version == exhausted);
    }

    SECTION("undo") {
        auto score = make_valid_score(1);
        UndoStack stack;
        REQUIRE(assign_instrument(score, PartId{100}, InstrumentType::Violin, &stack).has_value());
        score.version = exhausted;
        const auto undo_size = stack.undo_entries.size();

        const auto result = undo(score, stack);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ArithmeticOverflow);
        CHECK(score.version == exhausted);
        CHECK(score.parts[0].definition.instrument_type == InstrumentType::Violin);
        CHECK(stack.undo_entries.size() == undo_size);
        CHECK(stack.redo_entries.empty());
    }

    SECTION("redo") {
        auto score = make_valid_score(1);
        UndoStack stack;
        REQUIRE(assign_instrument(score, PartId{100}, InstrumentType::Violin, &stack).has_value());
        REQUIRE(undo(score, stack).has_value());
        score.version = exhausted;
        const auto redo_size = stack.redo_entries.size();

        const auto result = redo(score, stack);

        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::ArithmeticOverflow);
        CHECK(score.version == exhausted);
        CHECK(score.parts[0].definition.instrument_type == InstrumentType::Piano);
        CHECK(stack.undo_entries.empty());
        CHECK(stack.redo_entries.size() == redo_size);
    }
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
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}}); // E4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}}); // G4
    ng1.duration = Beat{1, 1};
    voice.events[0].payload = ng1;

    // Bar 2: G major triad (G4, B4, D5) — to be voice-led
    auto& voice2 = score.parts[0].measures[1].voices[0];
    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}}); // G4
    ng2.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}}); // B4
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}}); // D5
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
    ha1.chord.notes = {60, 64, 67}; // C4, E4, G4
    score.harmonic_annotations.push_back(ha1);

    HarmonicAnnotation ha2;
    ha2.position = ScoreTime{2, Beat::zero()};
    ha2.duration = Beat{1, 1};
    ha2.roman_numeral = "V";
    ha2.function = ScoreHarmonicFunction::Dominant;
    ha2.chord.root = 7;
    ha2.chord.quality = "major";
    ha2.chord.notes = {55, 59, 62}; // G3, B3, D4
    score.harmonic_annotations.push_back(ha2);

    return score;
}

} // anonymous namespace

TEST_CASE("apply_voice_leading NearestTone redistributes pitches", "[score-ir][voice-leading]") {
    auto score = make_voice_leading_score();

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(score, region, VoiceLeadingStyle::NearestTone);
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

TEST_CASE("apply_voice_leading undo restores original pitches", "[score-ir][voice-leading]") {
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

    auto result = apply_voice_leading(score, region, VoiceLeadingStyle::NearestTone, &stack);
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

TEST_CASE("apply_voice_leading fails without harmonic annotations", "[score-ir][voice-leading]") {
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

    auto result = apply_voice_leading(score, region, VoiceLeadingStyle::NearestTone);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("apply_voice_leading SmoothBach locks bass", "[score-ir][voice-leading]") {
    auto score = make_voice_leading_score();

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    auto result = apply_voice_leading(score, region, VoiceLeadingStyle::SmoothBach);
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
    CHECK(lowest_midi % 12 == 7); // G
}

// =============================================================================
// Undo Grouping (SS-IR §11.7)
// =============================================================================

TEST_CASE("group of 3 mutations undoes as one", "[score-ir][undo][group]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;
    EventId target = voice.events[0].id;

    SpelledPitch p0{0, 0, 4}, p1{2, 0, 4}, p2{4, 0, 4};
    SpelledPitch q0{1, 0, 4}, q1{3, 0, 4}, q2{5, 0, 4};

    stack.begin_group("test group");
    (void)modify_pitch(score, target, 0, q0, &stack);
    (void)modify_pitch(score, target, 1, q1, &stack);
    (void)modify_pitch(score, target, 2, q2, &stack);
    stack.end_group();

    CHECK(stack.undo_entries.size() == 1);

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == p0);
    CHECK(restored->notes[1].pitch == p1);
    CHECK(restored->notes[2].pitch == p2);
}

TEST_CASE("group of 3 mutations redoes as one", "[score-ir][undo][group]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;
    EventId target = voice.events[0].id;

    SpelledPitch q0{1, 0, 4}, q1{3, 0, 4}, q2{5, 0, 4};

    stack.begin_group("test group");
    (void)modify_pitch(score, target, 0, q0, &stack);
    (void)modify_pitch(score, target, 1, q1, &stack);
    (void)modify_pitch(score, target, 2, q2, &stack);
    stack.end_group();

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());

    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == q0);
    CHECK(restored->notes[1].pitch == q1);
    CHECK(restored->notes[2].pitch == q2);
}

TEST_CASE("nested groups collapse into single undo entry", "[score-ir][undo][group]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;
    EventId target = voice.events[0].id;

    stack.begin_group("outer");
    (void)modify_pitch(score, target, 0, SpelledPitch{1, 0, 4}, &stack);
    stack.begin_group("inner");
    (void)modify_pitch(score, target, 0, SpelledPitch{2, 0, 4}, &stack);
    stack.end_group();
    stack.end_group();

    CHECK(stack.undo_entries.size() == 1);

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == SpelledPitch{0, 0, 4});
}

TEST_CASE("empty group is no-op", "[score-ir][undo][group]") {
    UndoStack stack;
    auto before = stack.undo_entries.size();

    stack.begin_group("empty");
    stack.end_group();

    CHECK(stack.undo_entries.size() == before);
}

TEST_CASE("UndoGroup RAII produces single entry after scope exit", "[score-ir][undo][group]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;
    EventId target = voice.events[0].id;

    {
        UndoGroup guard(stack, "RAII test");
        (void)modify_pitch(score, target, 0, SpelledPitch{1, 0, 4}, &stack);
        (void)modify_pitch(score, target, 1, SpelledPitch{3, 0, 4}, &stack);
    }

    CHECK(stack.undo_entries.size() == 1);
    CHECK(stack.undo_entries[0].description == "RAII test");
}

// =============================================================================
// Mutation Atomicity and Undo Correctness (RC-D audit remediation)
// =============================================================================

TEST_CASE("undo restores the snapshot despite external corruption; "
          "only an empty stack fails",
          "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // Set up a note
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    EventId target = voice.events[0].id;
    auto r = modify_pitch(score, target, 0, SpelledPitch{4, 0, 4}, &stack);
    REQUIRE(r.has_value());
    CHECK(stack.can_undo());
    CHECK(stack.undo_entries.size() == 1);

    // Corrupt state out-of-band: remove all events. Under the closure
    // engine the inverse failed here; snapshot restoration is infallible,
    // so undo succeeds and rebuilds the exact pre-mutation document.
    score.parts[0].measures[0].voices[0].events.clear();

    auto undo_result = undo(score, stack);
    REQUIRE(undo_result.has_value());

    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == SpelledPitch{0, 0, 4});
    CHECK(stack.can_redo());

    // The only remaining failure mode is an empty undo stack; a failed
    // undo leaves both stacks untouched.
    CHECK(stack.undo_entries.empty());
    auto second = undo(score, stack);
    CHECK_FALSE(second.has_value());
    CHECK(stack.undo_entries.empty());
    CHECK(stack.redo_entries.size() == 1);
}

TEST_CASE("redo restores the snapshot despite external corruption; "
          "only an empty stack fails",
          "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    EventId target = score.parts[0].measures[0].voices[0].events[0].id;
    auto r = modify_duration(score, target, Beat{1, 2}, &stack);
    REQUIRE(r.has_value());

    // Undo succeeds
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(stack.can_redo());
    CHECK(stack.redo_entries.size() == 1);

    // Corrupt state out-of-band: remove all events. Under the closure
    // engine the forward replay failed here; snapshot restoration is
    // infallible, so redo succeeds and rebuilds the post-mutation state.
    score.parts[0].measures[0].voices[0].events.clear();

    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());

    REQUIRE_FALSE(score.parts[0].measures[0].voices[0].events.empty());
    CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == Beat{1, 2});
    CHECK(stack.can_undo());

    // The only remaining failure mode is an empty redo stack; a failed
    // redo leaves both stacks untouched.
    CHECK(stack.redo_entries.empty());
    auto second = redo(score, stack);
    CHECK_FALSE(second.has_value());
    CHECK(stack.redo_entries.empty());
    CHECK(stack.undo_entries.size() == 1);
}

TEST_CASE("reorder_parts rejects duplicate PartIds", "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(2);

    // Add a second part
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}, SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}};
    auto add_result = add_part(score, violin_def, 1);
    REQUIRE(add_result.has_value());
    REQUIRE(score.parts.size() == 2);

    PartId piano_id = score.parts[0].id;

    // Attempt to reorder with a duplicate: [piano, piano] instead of [piano, violin]
    auto result = reorder_parts(score, {piano_id, piano_id});
    CHECK_FALSE(result.has_value());
}

TEST_CASE("insert_measures is atomic across all parts", "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(4);

    // Add a second part
    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}, SpelledPitch{4, 0, 3}, SpelledPitch{2, 0, 7}};
    auto add_result = add_part(score, violin_def, 1);
    REQUIRE(add_result.has_value());

    auto r = insert_measures(score, 2, 3);
    REQUIRE(r.has_value());

    // All parts should have equal measure counts after insertion
    CHECK(score.parts[0].measures.size() == score.parts[1].measures.size());
    CHECK(score.metadata.total_bars == 7);
}

TEST_CASE("undo-redo-undo cycle does not accumulate events", "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    // modify_pitch preserves EventId through undo-redo cycles; under the
    // snapshot engine every mutation does.
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    auto original_count = voice.events.size();
    EventId target = voice.events[0].id;
    SpelledPitch original_pitch{0, 0, 4};

    // Modify pitch
    auto r = modify_pitch(score, target, 0, SpelledPitch{4, 0, 4}, &stack);
    REQUIRE(r.has_value());

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    // Redo
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());

    // Undo again
    auto ur2 = undo(score, stack);
    REQUIRE(ur2.has_value());

    // Event count should match original — no accumulation
    CHECK(score.parts[0].measures[0].voices[0].events.size() == original_count);

    // Pitch should be restored to original
    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == original_pitch);
}

TEST_CASE("move_region undo restores the exact pre-move document, "
          "unrelated events included",
          "[score-ir][undo][atomicity]") {
    auto score = make_valid_score(4);
    UndoStack stack;

    // Place a C4 note in bar 1 (the region to be moved)
    auto& voice1 = score.parts[0].measures[0].voices[0];
    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng1.duration = Beat{1, 1};
    voice1.events[0].payload = ng1;

    // Place an unrelated E5 note in bar 2 before the move. Its identity
    // must survive the move_region undo untouched — the historical defect
    // deleted unrelated events via an ID-range filter.
    Note unrelated_note;
    unrelated_note.pitch = SpelledPitch{2, 0, 5}; // E5
    unrelated_note.velocity = VelocityValue{{}, 90};

    auto ins_result =
        insert_note(score, PartId{100}, 2, 0, Beat::zero(), unrelated_note, Beat{1, 4}, nullptr);
    REQUIRE(ins_result.has_value());

    EventId unrelated_id{0};
    for (const auto& ev : score.parts[0].measures[1].voices[0].events) {
        if (ev.is_note_group()) {
            unrelated_id = ev.id;
            break;
        }
    }
    REQUIRE(unrelated_id.value != 0);

    ScoreRegion src;
    src.start = SCORE_START;
    src.end = ScoreTime{2, Beat::zero()};

    ScoreTime dest{3, Beat::zero()};

    // Move bar 1 content to bar 3, then undo
    auto mr = move_region(score, src, dest, &stack);
    REQUIRE(mr.has_value());

    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    // The unrelated note in bar 2 still exists with its original id
    bool found_unrelated = false;
    for (const auto& ev : score.parts[0].measures[1].voices[0].events) {
        if (ev.id == unrelated_id) {
            found_unrelated = true;
            break;
        }
    }
    CHECK(found_unrelated);

    // The moved-to bar holds no copied note after undo
    bool bar3_has_note = false;
    for (const auto& ev : score.parts[0].measures[2].voices[0].events) {
        if (ev.is_note_group()) bar3_has_note = true;
    }
    CHECK_FALSE(bar3_has_note);

    // The moved-from bar has its C4 note back
    auto* restored = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(restored != nullptr);
    CHECK(restored->notes[0].pitch == SpelledPitch{0, 0, 4});
}

// =============================================================================
// Undo-redo-undo: insert_note cycle does not orphan events
// =============================================================================

TEST_CASE("insert_note undo-redo-undo preserves the EventId and "
          "returns to original event count",
          "[score-ir][undo][stale-id]") {
    auto score = make_valid_score(1);
    UndoStack stack;

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{500}, Beat::zero(), rest});
    auto original_count = voice.events.size();

    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{{}, 80};

    auto r = insert_note(score, PartId{100}, 1, 0, Beat{3, 4}, note, Beat{1, 4}, &stack);
    REQUIRE(r.has_value());
    CHECK(voice.events.size() == original_count + 1);

    // Capture the inserted note's identity
    EventId inserted_id{0};
    for (const auto& ev : score.parts[0].measures[0].voices[0].events) {
        if (ev.is_note_group() && ev.offset == Beat{3, 4}) {
            inserted_id = ev.id;
        }
    }
    REQUIRE(inserted_id.value != 0);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());

    // Redo restores the exact post-mutation state, including the original
    // EventId — the snapshot engine never reallocates ids on redo.
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    CHECK(score.parts[0].measures[0].voices[0].events.size() == original_count + 1);

    bool found_same_id = false;
    for (const auto& ev : score.parts[0].measures[0].voices[0].events) {
        if (ev.id == inserted_id && ev.is_note_group() && ev.offset == Beat{3, 4}) {
            found_same_id = true;
        }
    }
    CHECK(found_same_id);

    // Undo again — the preserved id is removed cleanly
    auto ur2 = undo(score, stack);
    REQUIRE(ur2.has_value());

    // Verify no note group remains at offset 3/4
    bool has_note_at_34 = false;
    for (const auto& ev : score.parts[0].measures[0].voices[0].events) {
        if (ev.is_note_group() && ev.offset == Beat{3, 4}) {
            has_note_at_34 = true;
        }
    }
    CHECK_FALSE(has_note_at_34);
}

// =============================================================================
// Undo-redo-undo: copy_region cycle does not orphan events
// =============================================================================

TEST_CASE("copy_region undo-redo-undo preserves the copied EventId "
          "and leaves no orphan events",
          "[score-ir][undo][stale-id]") {
    auto score = make_valid_score(4);
    UndoStack stack;

    // Place a note in bar 1
    auto& voice1 = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 1};
    voice1.events[0].payload = ng;

    ScoreRegion src;
    src.start = SCORE_START;
    src.end = ScoreTime{2, Beat::zero()};
    ScoreTime dest{3, Beat::zero()};

    // Count events in bar 3 before copy
    auto count_bar3_notes = [&]() {
        std::size_t count = 0;
        for (const auto& ev : score.parts[0].measures[2].voices[0].events) {
            if (ev.is_note_group()) ++count;
        }
        return count;
    };

    CHECK(count_bar3_notes() == 0);

    auto r = copy_region(score, src, dest, &stack);
    REQUIRE(r.has_value());
    CHECK(count_bar3_notes() == 1);

    // Capture the copied note's identity in bar 3
    EventId copied_id{0};
    for (const auto& ev : score.parts[0].measures[2].voices[0].events) {
        if (ev.is_note_group()) copied_id = ev.id;
    }
    REQUIRE(copied_id.value != 0);

    // Undo
    auto ur = undo(score, stack);
    REQUIRE(ur.has_value());
    CHECK(count_bar3_notes() == 0);

    // Redo restores the exact post-copy state, including the copied note's
    // original EventId — the snapshot engine never reallocates ids on redo.
    auto rr = redo(score, stack);
    REQUIRE(rr.has_value());
    CHECK(count_bar3_notes() == 1);

    bool found_same_id = false;
    for (const auto& ev : score.parts[0].measures[2].voices[0].events) {
        if (ev.id == copied_id && ev.is_note_group()) found_same_id = true;
    }
    CHECK(found_same_id);

    // Undo again — the preserved id is removed cleanly
    auto ur2 = undo(score, stack);
    REQUIRE(ur2.has_value());
    CHECK(count_bar3_notes() == 0);
}
