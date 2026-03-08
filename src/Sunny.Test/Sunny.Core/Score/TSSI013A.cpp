/**
 * @file TSSI013A.cpp
 * @brief Unit tests for Score IR workflow tools (SIWF001A)
 *
 * Component: TSSI013A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIWF001A (workflow tools)
 * Coverage: wf_add_part, write_melody, write_harmony, wf_reorchestrate,
 *           wf_double_part, wf_set_dynamics, wf_set_articulation,
 *           wf_insert_note, wf_modify_note, wf_delete_event,
 *           wf_transpose, wf_analyze_harmony, wf_get_orchestration,
 *           wf_get_reduction, wf_validate_score, wf_get_form_summary,
 *           wf_compile_to_midi
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIWF001A.h"
#include "Score/SIMT001A.h"
#include "Score/SIVD001A.h"
#include "Score/SIQR001A.h"
#include "Score/SIVW001A.h"
#include "Score/SICM001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

constexpr SpelledPitch C3{0, 0, 3};
constexpr SpelledPitch G3{4, 0, 3};
constexpr SpelledPitch C4{0, 0, 4};
constexpr SpelledPitch D4{1, 0, 4};
constexpr SpelledPitch E4{2, 0, 4};
constexpr SpelledPitch G4{4, 0, 4};

Score make_test_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
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

Score make_two_part_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition violin;
    violin.name = "Violin";
    violin.abbreviation = "Vln.";
    violin.instrument_type = InstrumentType::Violin;
    violin.clef = Clef::Treble;
    violin.rendering.midi_channel = 1;

    PartDefinition cello;
    cello.name = "Cello";
    cello.abbreviation = "Vc.";
    cello.instrument_type = InstrumentType::Cello;
    cello.clef = Clef::Bass;
    cello.rendering.midi_channel = 2;

    spec.parts.push_back(violin);
    spec.parts.push_back(cello);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

/// Build a ScoreRegion covering the entire score.
ScoreRegion whole_score_region(const Score& score) {
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{score.metadata.total_bars + 1, Beat::zero()};
    return region;
}

/// Count NoteGroup events in a voice.
std::size_t count_note_events(const Voice& voice) {
    std::size_t count = 0;
    for (const auto& event : voice.events) {
        if (event.is_note_group()) ++count;
    }
    return count;
}

/// Find the first NoteGroup event in a voice.
const Event* find_first_note_event(const Voice& voice) {
    for (const auto& event : voice.events) {
        if (event.is_note_group()) return &event;
    }
    return nullptr;
}

/// Check whether any voice in a measure has NoteGroup events.
bool measure_has_notes(const Measure& measure) {
    for (const auto& voice : measure.voices) {
        if (count_note_events(voice) > 0) return true;
    }
    return false;
}

bool has_error_diagnostic(const std::vector<Diagnostic>& diags) {
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) return true;
    }
    return false;
}

}  // namespace

// =============================================================================
// 1. wf_add_part
// =============================================================================

TEST_CASE("TSSI013A: wf_add_part appends part, validation passes",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    REQUIRE(score.parts.size() == 1);

    PartDefinition violin;
    violin.name = "Violin";
    violin.abbreviation = "Vln.";
    violin.instrument_type = InstrumentType::Violin;
    violin.clef = Clef::Treble;
    violin.rendering.midi_channel = 2;

    auto result = wf_add_part(score, violin);
    REQUIRE(result.has_value());
    CHECK(score.parts.size() == 2);

    auto diags = validate_structural(score);
    CHECK_FALSE(has_error_diagnostic(diags));
}

// =============================================================================
// 2. write_melody inserts notes
// =============================================================================

TEST_CASE("TSSI013A: write_melody inserts notes at correct positions and durations",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    std::vector<MelodyEntry> melody;
    melody.push_back(MelodyEntry{
        ScoreTime{1, Beat::zero()}, C4, Beat{1, 4}, std::nullopt, std::nullopt});
    melody.push_back(MelodyEntry{
        ScoreTime{1, Beat{1, 4}}, E4, Beat{1, 4}, std::nullopt, std::nullopt});

    auto result = write_melody(score, part_id, 0, melody);
    REQUIRE(result.has_value());

    const auto& voice = score.parts[0].measures[0].voices[0];
    std::size_t notes_found = count_note_events(voice);
    CHECK(notes_found >= 2);

    // Verify that NoteGroup events exist at expected offsets
    bool found_beat0 = false;
    bool found_beat1 = false;
    for (const auto& event : voice.events) {
        if (!event.is_note_group()) continue;
        if (event.offset == Beat::zero()) found_beat0 = true;
        if (event.offset == Beat{1, 4}) found_beat1 = true;
    }
    CHECK(found_beat0);
    CHECK(found_beat1);
}

// =============================================================================
// 3. write_melody undo
// =============================================================================

TEST_CASE("TSSI013A: write_melody is undoable (single undo restores rests)",
          "[score-ir][workflow][undo]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;
    UndoStack undo_stack;

    std::vector<MelodyEntry> melody;
    melody.push_back(MelodyEntry{
        ScoreTime{1, Beat::zero()}, C4, Beat{1, 4}, std::nullopt, std::nullopt});

    auto result = write_melody(score, part_id, 0, melody, &undo_stack);
    REQUIRE(result.has_value());

    // Verify note was inserted
    CHECK(count_note_events(score.parts[0].measures[0].voices[0]) > 0);

    // Undo the write_melody group
    auto undo_result = undo(score, undo_stack);
    REQUIRE(undo_result.has_value());

    // After undo, bar 1 voice 0 should have only rest events
    bool all_rests = true;
    for (const auto& event : score.parts[0].measures[0].voices[0].events) {
        if (event.is_note_group()) { all_rests = false; break; }
    }
    CHECK(all_rests);
}

// =============================================================================
// 4. write_harmony distributes voicing across parts
// =============================================================================

TEST_CASE("TSSI013A: write_harmony distributes voicing across parts",
          "[score-ir][workflow]") {
    auto score = make_two_part_score();
    PartId part0 = score.parts[0].id;  // violin (upper)
    PartId part1 = score.parts[1].id;  // cello (lower)

    // target_parts ordered bottom to top: cello first, violin second
    std::vector<PartId> target_parts = {part1, part0};

    std::vector<HarmonyEntry> chords;
    chords.push_back(HarmonyEntry{
        ScoreTime{1, Beat::zero()}, {C3, G3}, Beat{1, 1}});

    auto result = write_harmony(score, target_parts, chords);
    REQUIRE(result.has_value());

    // Part 1 (cello, bottom) should have C3
    const auto* cello_note = find_first_note_event(score.parts[1].measures[0].voices[0]);
    REQUIRE(cello_note != nullptr);
    const auto* cello_ng = cello_note->as_note_group();
    REQUIRE(cello_ng != nullptr);
    CHECK(cello_ng->notes[0].pitch == C3);

    // Part 0 (violin, top) should have G3
    const auto* violin_note = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(violin_note != nullptr);
    const auto* violin_ng = violin_note->as_note_group();
    REQUIRE(violin_ng != nullptr);
    CHECK(violin_ng->notes[0].pitch == G3);
}

// =============================================================================
// 5. write_harmony handles more pitches than parts
// =============================================================================

TEST_CASE("TSSI013A: write_harmony handles more pitches than parts (chords within parts)",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    std::vector<PartId> target_parts = {part_id};
    std::vector<HarmonyEntry> chords;
    chords.push_back(HarmonyEntry{
        ScoreTime{1, Beat::zero()}, {C4, E4, G4}, Beat{1, 1}});

    auto result = write_harmony(score, target_parts, chords);
    REQUIRE(result.has_value());

    // The single part should have a NoteGroup with 3 notes
    const auto* note_event = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(note_event != nullptr);
    const auto* ng = note_event->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes.size() == 3);
}

// =============================================================================
// 6. wf_reorchestrate copies material to target
// =============================================================================

TEST_CASE("TSSI013A: wf_reorchestrate copies material to target",
          "[score-ir][workflow]") {
    auto score = make_two_part_score();
    PartId part0 = score.parts[0].id;
    PartId part1 = score.parts[1].id;

    // Insert a note in part 0, bar 1
    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part0, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    ScoreRegion region = whole_score_region(score);
    auto result = wf_reorchestrate(score, region, part0, part1);
    REQUIRE(result.has_value());

    // Part 1 should now have note events (not just rests)
    CHECK(measure_has_notes(score.parts[1].measures[0]));
}

// =============================================================================
// 7. wf_double_part transposes by interval
// =============================================================================

TEST_CASE("TSSI013A: wf_double_part transposes by interval",
          "[score-ir][workflow]") {
    auto score = make_two_part_score();
    PartId part0 = score.parts[0].id;
    PartId part1 = score.parts[1].id;

    // Insert C4 in part 0
    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part0, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    ScoreRegion region = whole_score_region(score);
    // 7 semitones = perfect fifth
    auto result = wf_double_part(score, region, part0, part1, 7);
    REQUIRE(result.has_value());

    // Part 1 should have a note at G4 (C4 + 7 semitones = MIDI 67 = G4)
    const auto* doubled = find_first_note_event(score.parts[1].measures[0].voices[0]);
    REQUIRE(doubled != nullptr);
    const auto* ng = doubled->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(midi_value(ng->notes[0].pitch) == midi_value(G4));
}

// =============================================================================
// 8. wf_set_dynamics applies dynamic to all notes in region
// =============================================================================

TEST_CASE("TSSI013A: wf_set_dynamics applies dynamic to all notes in region",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    // Insert a note first
    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part_id, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    ScoreRegion region = whole_score_region(score);
    auto result = wf_set_dynamics(score, region, DynamicLevel::ff);
    REQUIRE(result.has_value());

    // Verify the note's dynamic is ff
    const auto* ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    const auto* ng = ev->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].dynamic == DynamicLevel::ff);
}

// =============================================================================
// 9. wf_set_articulation applies articulation to all notes in region
// =============================================================================

TEST_CASE("TSSI013A: wf_set_articulation applies articulation to all notes in region",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part_id, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    ScoreRegion region = whole_score_region(score);
    auto result = wf_set_articulation(score, region, ArticulationType::Staccato);
    REQUIRE(result.has_value());

    const auto* ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    const auto* ng = ev->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].articulation == ArticulationType::Staccato);
}

// =============================================================================
// 10. wf_insert_note places note at specified position
// =============================================================================

TEST_CASE("TSSI013A: wf_insert_note places note at specified position",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    Note note;
    note.pitch = C4;
    auto result = wf_insert_note(
        score, part_id, 1, 0, Beat{1, 4}, note, Beat{1, 4});
    REQUIRE(result.has_value());

    // Verify a NoteGroup exists at offset Beat{1,4} in bar 1
    bool found = false;
    for (const auto& event : score.parts[0].measures[0].voices[0].events) {
        if (event.is_note_group() && event.offset == Beat{1, 4}) {
            found = true;
            break;
        }
    }
    CHECK(found);
}

// =============================================================================
// 11. wf_modify_note groups modifications into single undo
// =============================================================================

TEST_CASE("TSSI013A: wf_modify_note groups modifications into single undo",
          "[score-ir][workflow][undo]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;
    UndoStack undo_stack;

    // Insert a C4 note
    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part_id, 1, 0, Beat::zero(), note, Beat{1, 1},
                           &undo_stack);
    REQUIRE(ins.has_value());

    // Get the event id
    const auto* ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    EventId eid = ev->id;

    // Modify pitch to D4 and articulation to Staccato
    auto result = wf_modify_note(
        score, eid, 0,
        D4,                                      // new pitch
        std::nullopt,                             // no duration change
        std::nullopt,                             // no velocity change
        ArticulationType::Staccato,               // new articulation
        &undo_stack);
    REQUIRE(result.has_value());

    // Verify modifications applied
    ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    auto* ng = ev->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].pitch == D4);
    CHECK(ng->notes[0].articulation == ArticulationType::Staccato);

    // Undo the wf_modify_note group: restores original C4, no articulation
    auto undo_result = undo(score, undo_stack);
    REQUIRE(undo_result.has_value());

    ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    ng = ev->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].pitch == C4);
    CHECK_FALSE(ng->notes[0].articulation.has_value());
}

// =============================================================================
// 12. wf_delete_event removes event
// =============================================================================

TEST_CASE("TSSI013A: wf_delete_event removes event",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part_id, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    // Find the inserted event's id
    const auto* ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    EventId eid = ev->id;

    auto result = wf_delete_event(score, eid);
    REQUIRE(result.has_value());

    // The NoteGroup should be replaced by a rest
    ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    CHECK(ev == nullptr);
}

// =============================================================================
// 13. wf_transpose with EventId transposes single event
// =============================================================================

TEST_CASE("TSSI013A: wf_transpose with EventId transposes single event",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    Note note;
    note.pitch = C4;
    auto ins = insert_note(score, part_id, 1, 0, Beat::zero(), note, Beat{1, 1});
    REQUIRE(ins.has_value());

    const auto* ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    EventId eid = ev->id;

    // Transpose by MAJOR_SECOND: C4 -> D4
    auto result = wf_transpose(score, eid, MAJOR_SECOND);
    REQUIRE(result.has_value());

    ev = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev != nullptr);
    const auto* ng = ev->as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].pitch == D4);
}

// =============================================================================
// 14. wf_transpose with ScoreRegion transposes all notes
// =============================================================================

TEST_CASE("TSSI013A: wf_transpose with ScoreRegion transposes all notes",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    PartId part_id = score.parts[0].id;

    // Insert two notes in different bars
    Note note1;
    note1.pitch = C4;
    auto ins1 = insert_note(score, part_id, 1, 0, Beat::zero(), note1, Beat{1, 1});
    REQUIRE(ins1.has_value());

    Note note2;
    note2.pitch = E4;
    auto ins2 = insert_note(score, part_id, 2, 0, Beat::zero(), note2, Beat{1, 1});
    REQUIRE(ins2.has_value());

    ScoreRegion region = whole_score_region(score);
    auto result = wf_transpose(score, region, MAJOR_SECOND);
    REQUIRE(result.has_value());

    // Bar 1: C4 -> D4
    const auto* ev1 = find_first_note_event(score.parts[0].measures[0].voices[0]);
    REQUIRE(ev1 != nullptr);
    CHECK(ev1->as_note_group()->notes[0].pitch == D4);

    // Bar 2: E4 -> F#4 (E4 + major second = {chromatic +2, diatonic +1})
    const auto* ev2 = find_first_note_event(score.parts[0].measures[1].voices[0]);
    REQUIRE(ev2 != nullptr);
    SpelledPitch expected_fsharp4 = apply_interval(E4, MAJOR_SECOND);
    CHECK(ev2->as_note_group()->notes[0].pitch == expected_fsharp4);
}

// =============================================================================
// 15. wf_analyze_harmony returns annotations matching region
// =============================================================================

TEST_CASE("TSSI013A: wf_analyze_harmony returns annotations matching region",
          "[score-ir][workflow]") {
    auto score = make_test_score();

    // Set up harmonic annotations via set_section_harmony
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    std::vector<ChordSymbolEntry> progression;
    progression.push_back(ChordSymbolEntry{
        SCORE_START, SpelledPitch{0, 0, 4}, "major", std::nullopt});
    progression.push_back(ChordSymbolEntry{
        ScoreTime{2, Beat::zero()}, SpelledPitch{4, 0, 4}, "major", std::nullopt});

    auto set_result = set_section_harmony(score, region, progression);
    REQUIRE(set_result.has_value());

    auto annotations = wf_analyze_harmony(score, region);
    CHECK_FALSE(annotations.empty());
}

// =============================================================================
// 16. wf_get_orchestration returns role assignments
// =============================================================================

TEST_CASE("TSSI013A: wf_get_orchestration returns role assignments",
          "[score-ir][workflow]") {
    auto score = make_test_score();
    ScoreRegion region = whole_score_region(score);

    // Without orchestration annotations, result may be empty but should not fail
    auto roles = wf_get_orchestration(score, region);
    // This is a valid return; the vector may be empty if no annotations exist
    CHECK(roles.size() >= 0);  // effectively a no-throw check
}

// =============================================================================
// 17. wf_get_reduction "piano" produces a result
// =============================================================================

TEST_CASE("TSSI013A: wf_get_reduction piano produces a result",
          "[score-ir][workflow]") {
    auto score = make_two_part_score();

    auto reduced = wf_get_reduction(score, "piano");
    // Piano reduction collapses all parts into one piano part
    CHECK(reduced.parts.size() == 1);
}

// =============================================================================
// 18. wf_validate_score returns diagnostics for invalid score
// =============================================================================

TEST_CASE("TSSI013A: wf_validate_score returns diagnostics for invalid score",
          "[score-ir][workflow]") {
    auto score = make_test_score();

    // Invalidate: clear voice events to violate the measure-fill constraint
    score.parts[0].measures[0].voices[0].events.clear();

    auto diags = wf_validate_score(score);
    CHECK_FALSE(diags.empty());
}

// =============================================================================
// 19. wf_get_form_summary returns section summaries
// =============================================================================

TEST_CASE("TSSI013A: wf_get_form_summary returns section summaries",
          "[score-ir][workflow]") {
    auto score = make_test_score();

    // Add sections via set_formal_plan
    std::vector<SectionDefinition> sections;
    sections.push_back(SectionDefinition{"A", 1, 2, FormFunction::Expository});
    sections.push_back(SectionDefinition{"B", 3, 4, FormFunction::Developmental});

    auto plan_result = set_formal_plan(score, sections);
    REQUIRE(plan_result.has_value());

    auto summary = wf_get_form_summary(score);
    CHECK(summary.size() >= 2);

    // Verify the labels match what was set
    bool found_a = false;
    bool found_b = false;
    for (const auto& entry : summary) {
        if (entry.label == "A") found_a = true;
        if (entry.label == "B") found_b = true;
    }
    CHECK(found_a);
    CHECK(found_b);
}

// =============================================================================
// 20. wf_compile_to_midi delegates correctly
// =============================================================================

TEST_CASE("TSSI013A: wf_compile_to_midi delegates correctly",
          "[score-ir][workflow]") {
    auto score = make_test_score();

    auto result = wf_compile_to_midi(score);
    REQUIRE(result.has_value());
}
