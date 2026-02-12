/**
 * @file TSSI004A.cpp
 * @brief Unit tests for Score IR serialisation (SISZ001A)
 *
 * Component: TSSI004A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SISZ001A
 * Coverage: score_to_json, score_from_json, round-trip, schema version
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SISZ001A.h"
#include "Score/SIMT001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_serialisation_score() {
    Score score;
    score.id = ScoreId{42};
    score.metadata.title = "Serialisation Test";
    score.metadata.composer = "Test Composer";
    score.metadata.total_bars = 2;
    score.metadata.tags = {"test", "score-ir"};
    score.version = 5;

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

    for (std::uint32_t bar = 1; bar <= 2; ++bar) {
        if (bar == 1) {
            // Bar 1: C4 whole note
            NoteGroup ng;
            Note n;
            n.pitch = SpelledPitch{0, 0, 4};  // C4
            n.velocity = VelocityValue{{}, 80};
            ng.notes.push_back(n);
            ng.duration = ts->measure_duration();
            Event event{EventId{1001}, Beat::zero(), ng};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            piano.measures.push_back(measure);
        } else {
            // Bar 2: rest
            RestEvent rest{ts->measure_duration(), true};
            Event event{EventId{2001}, Beat::zero(), rest};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            piano.measures.push_back(measure);
        }
    }

    score.parts.push_back(piano);

    // Section
    ScoreSection section;
    section.id = SectionId{200};
    section.label = "A";
    section.start = SCORE_START;
    section.end = ScoreTime{3, Beat::zero()};
    section.form_function = FormFunction::Expository;
    score.section_map.push_back(section);

    return score;
}

}  // anonymous namespace

// =============================================================================
// SISZ001A: JSON Round-trip
// =============================================================================

TEST_CASE("SISZ001A: score round-trip via JSON", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    CHECK(restored->id == original.id);
    CHECK(restored->metadata.title == original.metadata.title);
    CHECK(restored->metadata.composer == original.metadata.composer);
    CHECK(restored->metadata.total_bars == original.metadata.total_bars);
    CHECK(restored->metadata.tags == original.metadata.tags);
    CHECK(restored->version == original.version);
    CHECK(restored->parts.size() == original.parts.size());
    CHECK(restored->tempo_map.size() == original.tempo_map.size());
    CHECK(restored->key_map.size() == original.key_map.size());
    CHECK(restored->time_map.size() == original.time_map.size());
    CHECK(restored->section_map.size() == original.section_map.size());
}

TEST_CASE("SISZ001A: JSON string round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json_str = score_to_json_string(original);
    auto restored = score_from_json_string(json_str);
    REQUIRE(restored.has_value());

    CHECK(restored->id == original.id);
    CHECK(restored->metadata.title == "Serialisation Test");
    CHECK(restored->parts.size() == 1);
}

TEST_CASE("SISZ001A: note content survives round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    // Bar 1 should have a note
    auto& bar1 = restored->parts[0].measures[0];
    REQUIRE(bar1.voices.size() == 1);
    REQUIRE(bar1.voices[0].events.size() == 1);
    CHECK(bar1.voices[0].events[0].is_note_group());

    auto* ng = bar1.voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    REQUIRE(ng->notes.size() == 1);
    CHECK(ng->notes[0].pitch.letter == 0);   // C
    CHECK(ng->notes[0].pitch.octave == 4);
    CHECK(ng->notes[0].velocity.value == 80);

    // Bar 2 should have a rest
    auto& bar2 = restored->parts[0].measures[1];
    CHECK(bar2.voices[0].events[0].is_rest());
}

TEST_CASE("SISZ001A: section round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    REQUIRE(restored->section_map.size() == 1);

    CHECK(restored->section_map[0].label == "A");
    CHECK(restored->section_map[0].start == SCORE_START);
    CHECK(restored->section_map[0].form_function.has_value());
    CHECK(*restored->section_map[0].form_function == FormFunction::Expository);
}

// =============================================================================
// SISZ001A: Schema Version
// =============================================================================

TEST_CASE("SISZ001A: schema version check", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    // Valid schema version
    CHECK(json.at("schema_version").get<int>() == SCORE_IR_SCHEMA_VERSION);

    // Modify to wrong version
    json["schema_version"] = 999;
    auto result = score_from_json(json);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: invalid JSON string", "[score-ir][serialisation]") {
    auto result = score_from_json_string("not valid json");
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// SISZ001A: part definition round-trip
// =============================================================================

TEST_CASE("SISZ001A: part definition round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    auto& part = restored->parts[0];
    CHECK(part.definition.name == "Piano");
    CHECK(part.definition.abbreviation == "Pno.");
    CHECK(part.definition.instrument_type == InstrumentType::Piano);
}
