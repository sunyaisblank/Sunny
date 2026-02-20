/**
 * @file TSSI006A.cpp
 * @brief Unit tests for Score IR mutation operations (SIMT001A) — extended coverage
 *
 * Component: TSSI006A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIMT001A
 * Coverage: add_voice, remove_voice, reorder_parts, assign_instrument,
 *           copy_region, retrograde_region, augment_region, diminute_region,
 *           invert_region, set_dynamic_region, scale_velocity_region,
 *           reorchestrate
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIMT001A.h"
#include "Score/SIVD001A.h"

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

/// Place a note in the first bar of the first part, replacing the rest
void place_note_in_bar(Score& score, std::uint32_t bar,
                       SpelledPitch pitch, Beat duration) {
    auto& voice = score.parts[0].measures[bar - 1].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{pitch, VelocityValue{{}, 80}});
    ng.duration = duration;
    voice.events[0].payload = ng;
}

}  // anonymous namespace

// =============================================================================
// Voice-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: add_voice increases voice count", "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    CHECK(score.parts[0].measures[0].voices.size() == 1);

    auto result = add_voice(score, 1, PartId{100}, 1);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].measures[0].voices.size() == 2);
    CHECK(score.parts[0].measures[0].voices[1].voice_index == 1);

    // The new voice should contain a whole-measure rest
    CHECK(score.parts[0].measures[0].voices[1].events.size() == 1);
    CHECK(score.parts[0].measures[0].voices[1].events[0].is_rest());
}

TEST_CASE("SIMT001A: remove_voice removes voice", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Add a second voice first
    auto add_result = add_voice(score, 1, PartId{100}, 1);
    REQUIRE(add_result.has_value());
    CHECK(score.parts[0].measures[0].voices.size() == 2);

    auto result = remove_voice(score, 1, PartId{100}, 1);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].measures[0].voices.size() == 1);
}

TEST_CASE("SIMT001A: remove_voice fails on last voice", "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    CHECK(score.parts[0].measures[0].voices.size() == 1);

    auto result = remove_voice(score, 1, PartId{100}, 0);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Part Management Mutations
// =============================================================================

TEST_CASE("SIMT001A: reorder_parts changes order", "[score-ir][mutation]") {
    auto score = make_valid_score(2);

    // Add a second part
    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;
    auto add_result = add_part(score, violin_def, 1);
    REQUIRE(add_result.has_value());
    CHECK(score.parts.size() == 2);

    PartId piano_id = score.parts[0].id;
    PartId violin_id = score.parts[1].id;

    // Reorder: violin first, piano second
    auto result = reorder_parts(score, {violin_id, piano_id});
    REQUIRE(result.has_value());
    CHECK(score.parts[0].id == violin_id);
    CHECK(score.parts[1].id == piano_id);
}

TEST_CASE("SIMT001A: assign_instrument changes instrument type",
          "[score-ir][mutation]") {
    auto score = make_valid_score();
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Piano);

    auto result = assign_instrument(score, PartId{100}, InstrumentType::Organ);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Organ);
}

// =============================================================================
// Region-Level Mutations
// =============================================================================

TEST_CASE("SIMT001A: copy_region copies notes to destination",
          "[score-ir][mutation]") {
    auto score = make_valid_score(2);

    // Place a C4 whole note in bar 1
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    ScoreRegion src;
    src.start = SCORE_START;
    src.end = ScoreTime{2, Beat::zero()};

    // Copy bar 1 content to bar 2
    ScoreTime dest{2, Beat::zero()};
    auto result = copy_region(score, src, dest);
    REQUIRE(result.has_value());

    // Bar 2 should now have a note
    auto& bar2_voice = score.parts[0].measures[1].voices[0];
    CHECK(bar2_voice.events[0].is_note_group());
}

TEST_CASE("SIMT001A: retrograde_region reverses event payloads",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Set up two events: C4 half note then E4 half note
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}});  // E4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{2001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{2002}, Beat{1, 2}, ng2});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = retrograde_region(score, region);
    REQUIRE(result.has_value());

    // After retrograde, the first event should have E4 and second C4
    auto* first_ng = voice.events[0].as_note_group();
    auto* second_ng = voice.events[1].as_note_group();
    REQUIRE(first_ng != nullptr);
    REQUIRE(second_ng != nullptr);
    CHECK(first_ng->notes[0].pitch.letter == 2);   // E
    CHECK(second_ng->notes[0].pitch.letter == 0);   // C
}

TEST_CASE("SIMT001A: augment_region doubles durations",
          "[score-ir][mutation]") {
    auto score = make_valid_score(2);

    // Place a C4 half note at bar 1
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    ng.duration = Beat{1, 2};
    voice.events.push_back(Event{EventId{3001}, Beat::zero(), ng});
    // Pad with rest
    RestEvent rest{Beat{1, 2}, true};
    voice.events.push_back(Event{EventId{3002}, Beat{1, 2}, rest});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = augment_region(score, region, Beat{2, 1});
    REQUIRE(result.has_value());

    // Half note * 2 = whole note
    auto* updated = voice.events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->duration == Beat{1, 1});
}

TEST_CASE("SIMT001A: diminute_region halves durations",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Place a C4 whole note at bar 1
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = diminute_region(score, region, Beat{2, 1});
    REQUIRE(result.has_value());

    // Whole note / 2 = half note
    auto* updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->duration == Beat{1, 2});
}

TEST_CASE("SIMT001A: invert_region inverts pitches around axis",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Place a C4 whole note — invert around C4 should stay C4
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    SpelledPitch axis{0, 0, 4};  // C4
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = invert_region(score, region, axis);
    REQUIRE(result.has_value());

    // C4 inverted around C4 = C4 (axis pitch stays the same)
    auto* updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(midi_value(updated->notes[0].pitch) == midi_value(axis));
}

TEST_CASE("SIMT001A: set_dynamic_region applies dynamic",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = set_dynamic_region(score, region, DynamicLevel::ff);
    REQUIRE(result.has_value());

    auto* ng = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    REQUIRE(ng->notes[0].dynamic.has_value());
    CHECK(*ng->notes[0].dynamic == DynamicLevel::ff);
}

TEST_CASE("SIMT001A: scale_velocity_region scales by factor",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    // Initial velocity is 80
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = scale_velocity_region(score, region, 0.5);
    REQUIRE(result.has_value());

    auto* ng = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    CHECK(ng->notes[0].velocity.value == 40);  // 80 * 0.5 = 40
}

// =============================================================================
// Orchestration Mutations
// =============================================================================

TEST_CASE("SIMT001A: reorchestrate copies notes to target part",
          "[score-ir][mutation]") {
    auto score = make_valid_score(1);

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
    auto add_result = add_part(score, violin_def, 1);
    REQUIRE(add_result.has_value());
    CHECK(score.parts.size() == 2);

    PartId violin_id = score.parts[1].id;

    // Place a note in the piano part (bar 1)
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};
    region.parts = {PartId{100}};  // Source from piano

    auto result = reorchestrate(score, region, violin_id);
    REQUIRE(result.has_value());

    // The violin part should now have a note in bar 1
    auto& violin_bar1 = score.parts[1].measures[0].voices[0];
    CHECK(violin_bar1.events[0].is_note_group());
}
