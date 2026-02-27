/**
 * @file TSSI012A.cpp
 * @brief Unit tests for compilation report diagnostics and extended S4-S6 validation
 *
 * Component: TSSI012A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SICM001A (CompilationReport), SIVD001A (S4/S5/S6 extended)
 * Coverage: dropped note counting, dropped tempo/time-sig event counting,
 *           S4 ascending-order validation, S5 ascending-order validation,
 *           S6 ascending-order validation
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SICM001A.h"
#include "Score/SIVD001A.h"
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

/// Place a note with an extreme pitch that produces a MIDI value outside [0,127].
/// SpelledPitch{0, 0, 11} = C11 has midi_value 144, exceeding the MIDI ceiling.
void place_out_of_range_note(Score& score, std::size_t part_idx, std::size_t bar_idx) {
    auto& voice = score.parts[part_idx].measures[bar_idx].voices[0];

    Note note;
    note.pitch = SpelledPitch{0, 0, 11};  // C11 = MIDI 144

    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 1};

    voice.events[0].payload = ng;
}

/// Place a valid whole note.
void place_whole_note(Score& score, std::size_t part_idx, std::size_t bar_idx,
                      SpelledPitch pitch) {
    auto& voice = score.parts[part_idx].measures[bar_idx].voices[0];

    Note note;
    note.pitch = pitch;

    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 1};

    voice.events[0].payload = ng;
}

constexpr SpelledPitch C4{0, 0, 4};

bool has_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    for (const auto& d : diags) {
        if (d.rule == rule && d.severity == ValidationSeverity::Error) return true;
    }
    return false;
}

std::size_t count_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    std::size_t count = 0;
    for (const auto& d : diags) {
        if (d.rule == rule && d.severity == ValidationSeverity::Error) ++count;
    }
    return count;
}

}  // anonymous namespace

// =============================================================================
// CompilationReport — dropped notes
// =============================================================================

TEST_CASE("compile_to_midi reports dropped notes for out-of-range MIDI",
          "[score][midi][report]") {
    auto score = make_compilable_score(2);
    place_out_of_range_note(score, 0, 0);
    place_whole_note(score, 0, 1, C4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& report = result->report;
    CHECK(report.dropped_notes == 1);
    CHECK(report.has_drops());
    CHECK(!report.diagnostics.empty());

    // The valid note should still compile
    CHECK(result->midi.notes.size() == 1);
    CHECK(result->midi.notes[0].note == 60);
}

TEST_CASE("compile_to_note_events reports dropped notes for out-of-range MIDI",
          "[score][note-events][report]") {
    auto score = make_compilable_score(2);
    place_out_of_range_note(score, 0, 0);
    place_whole_note(score, 0, 1, C4);

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());

    const auto& report = result->report;
    CHECK(report.dropped_notes == 1);
    CHECK(report.has_drops());

    // The valid note should still compile
    CHECK(result->events.size() == 1);
}

TEST_CASE("compile_to_midi clean score has no drops",
          "[score][midi][report]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    CHECK(!result->report.has_drops());
    CHECK(result->report.dropped_notes == 0);
    CHECK(result->report.dropped_tempo_events == 0);
    CHECK(result->report.dropped_time_sig_events == 0);
    CHECK(result->report.diagnostics.empty());
}

// =============================================================================
// S4: TempoMap validates all entries, not just first
// =============================================================================

TEST_CASE("S4 validates all tempo map entries not just first",
          "[score][validation][s4]") {
    auto score = make_compilable_score(4);

    // First entry is valid (bar 1). Add a second entry that is out of order
    // (at bar 2 but repeated, making a non-ascending sequence).
    TempoEvent tempo2;
    tempo2.position = ScoreTime{3, Beat::zero()};
    tempo2.bpm = make_bpm(90);
    tempo2.beat_unit = BeatUnit::Quarter;
    tempo2.transition_type = TempoTransitionType::Immediate;
    tempo2.linear_duration = Beat::zero();
    tempo2.old_unit = BeatUnit::Quarter;
    tempo2.new_unit = BeatUnit::Quarter;

    TempoEvent tempo3;
    tempo3.position = ScoreTime{2, Beat::zero()};
    tempo3.bpm = make_bpm(100);
    tempo3.beat_unit = BeatUnit::Quarter;
    tempo3.transition_type = TempoTransitionType::Immediate;
    tempo3.linear_duration = Beat::zero();
    tempo3.old_unit = BeatUnit::Quarter;
    tempo3.new_unit = BeatUnit::Quarter;

    // Append in wrong order: bar 3 then bar 2
    score.tempo_map.push_back(tempo2);
    score.tempo_map.push_back(tempo3);

    auto diags = validate_structural(score);
    // The first entry at bar 1 passes, but the third entry (bar 2) is not
    // ascending relative to the second entry (bar 3).
    CHECK(has_rule(diags, "S4"));
}

TEST_CASE("S4 detects duplicate tempo map positions",
          "[score][validation][s4]") {
    auto score = make_compilable_score(4);

    TempoEvent tempo_dup;
    tempo_dup.position = SCORE_START;
    tempo_dup.bpm = make_bpm(100);
    tempo_dup.beat_unit = BeatUnit::Quarter;
    tempo_dup.transition_type = TempoTransitionType::Immediate;
    tempo_dup.linear_duration = Beat::zero();
    tempo_dup.old_unit = BeatUnit::Quarter;
    tempo_dup.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo_dup);

    auto diags = validate_structural(score);
    // Two entries at bar 1 beat 0 violates ascending order
    CHECK(has_rule(diags, "S4"));
}

// =============================================================================
// S5: TimeSignatureMap validates all entries
// =============================================================================

TEST_CASE("S5 validates all time signature entries not just first",
          "[score][validation][s5]") {
    auto score = make_compilable_score(4);

    // Add a second entry at bar 3, then a third at bar 2 (out of order)
    auto ts34 = make_time_signature(3, 4);
    REQUIRE(ts34.has_value());

    TimeSignatureEntry tse3;
    tse3.bar = 3;
    tse3.time_signature = *ts34;
    score.time_map.push_back(tse3);

    TimeSignatureEntry tse2;
    tse2.bar = 2;
    tse2.time_signature = *ts34;
    score.time_map.push_back(tse2);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S5"));
}

TEST_CASE("S5 detects duplicate time signature bars",
          "[score][validation][s5]") {
    auto score = make_compilable_score(4);

    auto ts34 = make_time_signature(3, 4);
    REQUIRE(ts34.has_value());

    TimeSignatureEntry dup;
    dup.bar = 1;
    dup.time_signature = *ts34;
    score.time_map.push_back(dup);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S5"));
}

// =============================================================================
// S6: KeySignatureMap validates all entries
// =============================================================================

TEST_CASE("S6 validates all key signature entries not just first",
          "[score][validation][s6]") {
    auto score = make_compilable_score(4);

    // Add entries at bar 3 then bar 2 (out of order)
    KeySignatureEntry ke3;
    ke3.position = ScoreTime{3, Beat::zero()};
    ke3.key.root = SpelledPitch{0, 0, 4};
    ke3.key.accidentals = 1;
    score.key_map.push_back(ke3);

    KeySignatureEntry ke2;
    ke2.position = ScoreTime{2, Beat::zero()};
    ke2.key.root = SpelledPitch{0, 0, 4};
    ke2.key.accidentals = -1;
    score.key_map.push_back(ke2);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S6"));
}

TEST_CASE("S6 detects duplicate key signature positions",
          "[score][validation][s6]") {
    auto score = make_compilable_score(4);

    KeySignatureEntry dup;
    dup.position = SCORE_START;
    dup.key.root = SpelledPitch{3, 0, 4};
    dup.key.accidentals = 2;
    score.key_map.push_back(dup);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S6"));
}
