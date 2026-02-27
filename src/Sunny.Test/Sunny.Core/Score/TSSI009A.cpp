/**
 * @file TSSI009A.cpp
 * @brief Unit tests for Score IR MIDI compilation (SICM001A)
 *
 * Component: TSSI009A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SICM001A
 * Coverage: compile_to_midi, velocity resolution pipeline,
 *           tie chain accumulation, tempo/time sig/key sig meta events,
 *           multi-part compilation, articulation effects
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SICM001A.h"
#include "Score/SITM001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_compilable_score(std::uint32_t total_bars = 4) {
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
        SpelledPitch{5, 0, 0},
        SpelledPitch{0, 0, 8},
        SpelledPitch{0, 0, 3},
        SpelledPitch{0, 0, 6}
    };
    piano.definition.rendering.midi_channel = 1;

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

/// Replace the whole-measure rest in a bar with a single whole note.
/// bar_idx is 0-based.
void place_whole_note(Score& score, std::size_t part_idx, std::size_t bar_idx,
                      SpelledPitch pitch,
                      std::optional<DynamicLevel> dyn = std::nullopt,
                      std::optional<ArticulationType> art = std::nullopt,
                      bool tie_fwd = false)
{
    auto& voice = score.parts[part_idx].measures[bar_idx].voices[0];

    Note note;
    note.pitch = pitch;
    note.dynamic = dyn;
    note.articulation = art;
    note.tie_forward = tie_fwd;

    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 1};

    voice.events[0].payload = ng;
}

constexpr SpelledPitch C4{0, 0, 4};
constexpr SpelledPitch E4{2, 0, 4};

}  // anonymous namespace

// =============================================================================
// MIDI Compilation Tests
// =============================================================================

TEST_CASE("compile_to_midi: single C4 whole note at 120 BPM", "[score][midi]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    CHECK(midi.ppq == 480);
    REQUIRE(midi.notes.size() == 1);

    const auto& n = midi.notes[0];
    CHECK(n.tick == 0);
    CHECK(n.duration_ticks == 1920);
    CHECK(n.note == 60);
    CHECK(n.channel == 1);
    CHECK(n.velocity == default_velocity(DynamicLevel::mf));
}

TEST_CASE("compile_to_midi: ff dynamic with staccato halves duration", "[score][midi]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::ff, ArticulationType::Staccato);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 1);

    const auto& n = midi.notes[0];
    CHECK(n.velocity == default_velocity(DynamicLevel::ff));
    CHECK(n.duration_ticks == 960);
}

TEST_CASE("compile_to_midi: hairpin interpolation p to f", "[score][midi]") {
    auto score = make_compilable_score(4);

    place_whole_note(score, 0, 0, C4, DynamicLevel::p);
    place_whole_note(score, 0, 1, C4);
    place_whole_note(score, 0, 2, C4);
    place_whole_note(score, 0, 3, C4);

    Hairpin cresc;
    cresc.start = ScoreTime{1, Beat::zero()};
    cresc.end = ScoreTime{5, Beat::zero()};
    cresc.type = HairpinType::Crescendo;
    cresc.target = DynamicLevel::f;
    score.parts[0].hairpins.push_back(cresc);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 4);

    CHECK(midi.notes[0].velocity < midi.notes[1].velocity);
    CHECK(midi.notes[1].velocity < midi.notes[2].velocity);
    CHECK(midi.notes[2].velocity < midi.notes[3].velocity);

    CHECK(midi.notes[0].velocity >= 50);
    CHECK(midi.notes[3].velocity <= 110);
}

TEST_CASE("compile_to_midi: tied notes merge into single event", "[score][midi]") {
    auto score = make_compilable_score(2);

    place_whole_note(score, 0, 0, C4, std::nullopt, std::nullopt, true);
    place_whole_note(score, 0, 1, C4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 1);
    CHECK(midi.notes[0].tick == 0);
    CHECK(midi.notes[0].duration_ticks == 3840);
    CHECK(midi.notes[0].note == 60);
}

TEST_CASE("compile_to_midi: tempo meta events at correct ticks", "[score][midi]") {
    auto score = make_compilable_score(4);

    TempoEvent tempo2;
    tempo2.position = ScoreTime{3, Beat::zero()};
    tempo2.bpm = make_bpm(90);
    tempo2.beat_unit = BeatUnit::Quarter;
    tempo2.transition_type = TempoTransitionType::Immediate;
    tempo2.linear_duration = Beat::zero();
    tempo2.old_unit = BeatUnit::Quarter;
    tempo2.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo2);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.tempos.size() == 2);

    CHECK(midi.tempos[0].tick == 0);
    CHECK(midi.tempos[0].microseconds_per_beat == 500000);

    CHECK(midi.tempos[1].tick == 3840);
    CHECK(midi.tempos[1].microseconds_per_beat == 666666);
}

TEST_CASE("compile_to_midi: time signature meta events", "[score][midi]") {
    auto score = make_compilable_score(4);

    auto ts34 = make_time_signature(3, 4);
    REQUIRE(ts34.has_value());

    TimeSignatureEntry tse;
    tse.bar = 3;
    tse.time_signature = *ts34;
    score.time_map.push_back(tse);

    for (std::uint32_t bar_idx = 2; bar_idx < 4; ++bar_idx) {
        auto& voice = score.parts[0].measures[bar_idx].voices[0];
        voice.events.clear();
        RestEvent rest{ts34->measure_duration(), true};
        voice.events.push_back(Event{EventId{(bar_idx + 1) * 1000}, Beat::zero(), rest});
    }

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.time_signatures.size() == 2);

    CHECK(midi.time_signatures[0].tick == 0);
    CHECK(midi.time_signatures[0].numerator == 4);
    CHECK(midi.time_signatures[0].denominator == 2);

    CHECK(midi.time_signatures[1].tick == 3840);
    CHECK(midi.time_signatures[1].numerator == 3);
    CHECK(midi.time_signatures[1].denominator == 2);
}

TEST_CASE("compile_to_midi: multi-part multi-channel", "[score][midi]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    Part violin;
    violin.id = PartId{200};
    violin.definition.name = "Violin";
    violin.definition.abbreviation = "Vln.";
    violin.definition.instrument_type = InstrumentType::Violin;
    violin.definition.rendering.midi_channel = 2;

    Note vnote;
    vnote.pitch = E4;
    NoteGroup vng;
    vng.notes.push_back(vnote);
    vng.duration = Beat{1, 1};

    Event vevent{EventId{5001}, Beat::zero(), vng};
    Voice vvoice{0, {vevent}, {}};
    Measure vmeasure{1, {vvoice}, std::nullopt, std::nullopt};
    violin.measures.push_back(vmeasure);
    score.parts.push_back(violin);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 2);

    bool found_ch1 = false, found_ch2 = false;
    for (const auto& n : midi.notes) {
        if (n.channel == 1) { found_ch1 = true; CHECK(n.note == 60); }
        if (n.channel == 2) { found_ch2 = true; CHECK(n.note == 64); }
    }
    CHECK(found_ch1);
    CHECK(found_ch2);
}

TEST_CASE("compile_to_midi: empty score produces no notes", "[score][midi]") {
    auto score = make_compilable_score(4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    CHECK(midi.notes.empty());
    CHECK(!midi.tempos.empty());
    CHECK(!midi.time_signatures.empty());
}

TEST_CASE("compile_to_midi: accent increases velocity by 20", "[score][midi]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Accent);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 1);
    CHECK(midi.notes[0].velocity == 108);
    CHECK(midi.notes[0].duration_ticks == 1920);
}

TEST_CASE("compile_to_midi: uncompilable score returns error", "[score][midi]") {
    Score bad;
    bad.id = ScoreId{1};
    bad.metadata.total_bars = 1;

    auto result = compile_to_midi(bad);
    CHECK(!result.has_value());
}

TEST_CASE("compile_to_midi: key signature meta event", "[score][midi]") {
    auto score = make_compilable_score(4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.key_signatures.size() == 1);
    CHECK(midi.key_signatures[0].tick == 0);
    CHECK(midi.key_signatures[0].accidentals == 0);
    CHECK(midi.key_signatures[0].minor == false);
}

// =============================================================================
// compile_to_note_events (§9)
// =============================================================================

TEST_CASE("compile_to_note_events: 2-bar score with one note per bar produces 2 events",
          "[score][note-events]") {
    auto score = make_compilable_score(2);
    place_whole_note(score, 0, 0, C4);
    place_whole_note(score, 0, 1, E4);

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());
    CHECK(result->events.size() == 2);
}

TEST_CASE("compile_to_note_events: pitch equals midi_value of source SpelledPitch",
          "[score][note-events]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 1);

    CHECK(result->events[0].pitch == static_cast<MidiNote>(midi_value(C4)));
    CHECK(result->events[0].pitch == 60);
}

TEST_CASE("compile_to_note_events: start times monotonically increase across bars",
          "[score][note-events]") {
    auto score = make_compilable_score(4);
    place_whole_note(score, 0, 0, C4);
    place_whole_note(score, 0, 1, C4);
    place_whole_note(score, 0, 2, C4);
    place_whole_note(score, 0, 3, C4);

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());
    REQUIRE(result->events.size() == 4);

    for (std::size_t i = 1; i < result->events.size(); ++i) {
        CHECK(result->events[i].start_time > result->events[i - 1].start_time);
    }
}

TEST_CASE("compile_to_note_events: rest-only bars produce zero events",
          "[score][note-events]") {
    auto score = make_compilable_score(4);
    // All bars are rests by default

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());
    CHECK(result->events.empty());
}
