/**
 * @file compilation_diagnostics_test.cpp
 * @brief Unit tests for compilation report diagnostics and extended S4-S6 validation
 *
 *
 * Coverage: dropped note counting, dropped tempo/time-sig event counting,
 *           S4 ascending-order validation, S5 ascending-order validation,
 *           S6 ascending-order validation
 */

#include <catch2/catch_test_macros.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/time.hpp>
#include <sunny/core/score/validation.hpp>

using namespace sunny::core;

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
        SpelledPitch{5, 0, 0}, SpelledPitch{0, 0, 8}, SpelledPitch{0, 0, 3}, SpelledPitch{0, 0, 6}};
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
    note.pitch = SpelledPitch{0, 0, 11}; // C11 = MIDI 144

    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 1};

    voice.events[0].payload = ng;
}

/// Place a valid whole note.
void place_whole_note(Score& score, std::size_t part_idx, std::size_t bar_idx, SpelledPitch pitch) {
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

} // anonymous namespace

// =============================================================================
// CompilationReport — dropped notes
// =============================================================================

TEST_CASE("compile_to_midi reports dropped notes for out-of-range MIDI", "[score][midi][report]") {
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

TEST_CASE("compile_to_midi clean score has no drops", "[score][midi][report]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    CHECK(!result->report.has_drops());
    CHECK(!result->report.has_residuals());
    CHECK(result->report.dropped_notes == 0);
    CHECK(result->report.dropped_tempo_events == 0);
    CHECK(result->report.dropped_time_sig_events == 0);
    CHECK(result->report.dropped_key_sig_events == 0);
    CHECK(result->report.time_signature_events_requested == 1);
    CHECK(result->report.time_signature_events_written == 1);
    CHECK(result->report.time_signature_groupings_requested == 0);
    CHECK(result->report.time_signature_groupings_written == 0);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("CompilationReport distinguishes counted drops from diagnostic residuals",
          "[score][report][residual]") {
    CompilationReport report;
    CHECK_FALSE(report.has_drops());
    CHECK_FALSE(report.has_residuals());

    report.diagnostics.push_back(
        {"Target emitted a usable approximation", SCORE_START, std::nullopt});
    CHECK_FALSE(report.has_drops());
    CHECK(report.has_residuals());

    report.diagnostics.clear();
    report.dropped_notes = 1;
    CHECK(report.has_drops());
    CHECK(report.has_residuals());
}

// =============================================================================
// S4: TempoMap validates all entries, not just first
// =============================================================================

TEST_CASE("S4 validates all tempo map entries not just first", "[score][validation][s4]") {
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

TEST_CASE("S4 detects duplicate tempo map positions", "[score][validation][s4]") {
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

TEST_CASE("S5 validates all time signature entries not just first", "[score][validation][s5]") {
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

TEST_CASE("S5 detects duplicate time signature bars", "[score][validation][s5]") {
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

TEST_CASE("S6 validates all key signature entries not just first", "[score][validation][s6]") {
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

TEST_CASE("S6 detects duplicate key signature positions", "[score][validation][s6]") {
    auto score = make_compilable_score(4);

    KeySignatureEntry dup;
    dup.position = SCORE_START;
    dup.key.root = SpelledPitch{3, 0, 4};
    dup.key.accidentals = 2;
    score.key_map.push_back(dup);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S6"));
}

// =============================================================================
// S13: PartId and SectionId uniqueness
// =============================================================================

TEST_CASE("S13 detects duplicate PartId", "[score][validation][s13]") {
    auto score = make_compilable_score(2);

    // Duplicate the part with the same PartId
    Part dup = score.parts[0];
    dup.definition.name = "Piano Copy";
    score.parts.push_back(dup);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S13"));
}

TEST_CASE("S13 detects duplicate SectionId", "[score][validation][s13]") {
    auto score = make_compilable_score(4);

    ScoreSection s1;
    s1.id = SectionId{500};
    s1.label = "A";
    s1.start = SCORE_START;
    s1.end = ScoreTime{3, Beat::zero()};

    ScoreSection s2;
    s2.id = SectionId{500}; // duplicate
    s2.label = "B";
    s2.start = ScoreTime{3, Beat::zero()};
    s2.end = ScoreTime{5, Beat::zero()};

    score.section_map.push_back(s1);
    score.section_map.push_back(s2);

    auto diags = validate_structural(score);
    CHECK(has_rule(diags, "S13"));
}

TEST_CASE("S27 closes section labels spans and form-function domains", "[score][validation][s27]") {
    auto score = make_compilable_score(4);
    ScoreSection section;
    section.id = SectionId{600};
    section.label = "A";
    section.start = SCORE_START;
    section.end = ScoreTime{5, Beat::zero()};

    SECTION("valid complete-score span") {
        score.section_map.push_back(section);
        CHECK_FALSE(has_rule(validate_structural(score), "S27"));
    }
    SECTION("empty label") {
        section.label.clear();
        score.section_map.push_back(section);
        CHECK(has_rule(validate_structural(score), "S27"));
    }
    SECTION("start outside its bar") {
        section.start = ScoreTime{1, Beat{1, 1}};
        score.section_map.push_back(section);
        CHECK(has_rule(validate_structural(score), "S27"));
    }
    SECTION("end beyond score") {
        section.end = ScoreTime{6, Beat::zero()};
        score.section_map.push_back(section);
        CHECK(has_rule(validate_structural(score), "S27"));
    }
    SECTION("empty span") {
        section.end = section.start;
        score.section_map.push_back(section);
        CHECK(has_rule(validate_structural(score), "S27"));
    }
    SECTION("invalid form function") {
        section.form_function = static_cast<FormFunction>(255);
        score.section_map.push_back(section);
        CHECK(has_rule(validate_structural(score), "S27"));
    }
}

// =============================================================================
// S14: NonChordTone referential integrity
// =============================================================================

namespace {

bool has_warning_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    for (const auto& d : diags) {
        if (d.rule == rule && d.severity == ValidationSeverity::Warning) return true;
    }
    return false;
}

Score make_primary_beam_score() {
    auto score = make_compilable_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    const BeamGroupId beam_id{700};
    BeamGroup beam{beam_id, {}, {}};
    for (std::uint64_t i = 0; i < 4; ++i) {
        NoteGroup group;
        group.notes.push_back(Note{SpelledPitch{static_cast<std::uint8_t>(i), 0, 4}});
        group.duration = Beat{1, 8};
        group.beam_group = beam_id;
        const EventId event_id{710 + i};
        voice.events.push_back(Event{event_id, Beat{static_cast<std::int64_t>(i), 8}, group});
        beam.event_ids.push_back(event_id);
    }
    voice.events.push_back(Event{EventId{720}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});
    voice.beam_groups.push_back(std::move(beam));
    return score;
}

Score make_structured_lyric_score() {
    auto score = make_compilable_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    for (std::uint64_t i = 0; i < 4; ++i) {
        NoteGroup group;
        group.notes.push_back(Note{SpelledPitch{0, 0, 4}});
        group.duration = Beat{1, 4};
        voice.events.push_back(
            Event{EventId{800 + i}, Beat{static_cast<std::int64_t>(i), 4}, group});
    }
    auto& first = std::get<NoteGroup>(voice.events[0].payload).notes[0].lyrics;
    first.push_back(LyricSyllable{"Sun", 1, LyricSyllabic::Begin, false});
    first.push_back(LyricSyllable{"Bright", 2, LyricSyllabic::Single, false});
    auto& second = std::get<NoteGroup>(voice.events[1].payload).notes[0].lyrics;
    second.push_back(LyricSyllable{"ny", 1, LyricSyllabic::End, false});
    second.push_back(LyricSyllable{"day", 2, LyricSyllabic::Single, true});
    std::get<NoteGroup>(voice.events[3].payload)
        .notes[0]
        .lyrics.push_back(LyricSyllable{"now", 2, LyricSyllabic::Single, false});
    return score;
}

} // anonymous namespace

TEST_CASE("S14 closes the explicit primary BeamGroup profile", "[score][validation][s14][beam]") {
    const auto valid = make_primary_beam_score();
    REQUIRE(is_compilable(valid));

    SECTION("secondary break indices are rejected because level and hook state are absent") {
        auto score = valid;
        score.parts[0].measures[0].voices[0].beam_groups[0].beam_breaks.push_back(1);
        CHECK(has_rule(validate_structural(score), "S14"));
        CHECK_FALSE(is_compilable(score));
    }

    SECTION("a BeamGroup member requires the matching NoteGroup back-reference") {
        auto score = valid;
        auto& event = score.parts[0].measures[0].voices[0].events[1];
        std::get<NoteGroup>(event.payload).beam_group.reset();
        CHECK(has_rule(validate_structural(score), "S14"));
    }

    SECTION("a NoteGroup back-reference requires membership in the named local group") {
        auto score = valid;
        auto& event = score.parts[0].measures[0].voices[0].events[1];
        std::get<NoteGroup>(event.payload).beam_group = BeamGroupId{999};
        CHECK(has_rule(validate_structural(score), "S14"));
    }

    SECTION("members must be score ordered") {
        auto score = valid;
        auto& ids = score.parts[0].measures[0].voices[0].beam_groups[0].event_ids;
        std::swap(ids[1], ids[2]);
        CHECK(has_rule(validate_structural(score), "S14"));
    }

    SECTION("members must be contiguous among measured events") {
        auto score = valid;
        auto& ids = score.parts[0].measures[0].voices[0].beam_groups[0].event_ids;
        ids.erase(ids.begin() + 1);
        CHECK(has_rule(validate_structural(score), "S14"));
    }

    SECTION("a primary beam requires at least two events") {
        auto score = valid;
        auto& voice = score.parts[0].measures[0].voices[0];
        voice.beam_groups[0].event_ids.resize(1);
        for (std::size_t i = 1; i < 4; ++i)
            std::get<NoteGroup>(voice.events[i].payload).beam_group.reset();
        CHECK(has_rule(validate_structural(score), "S14"));
    }

    SECTION("grace NoteGroups are outside a normal primary beam") {
        auto score = valid;
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.grace = GraceType::Acciaccatura;
        CHECK(has_rule(validate_structural(score), "S14"));
    }
}

TEST_CASE("S25 and S26 close structured lyric payloads and verse topology",
          "[score][validation][s25][s26][lyric]") {
    const auto valid = make_structured_lyric_score();
    REQUIRE(is_compilable(valid));

    SECTION("local lyric payload domains are closed") {
        auto score = valid;
        auto& lyric = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
                          .notes[0]
                          .lyrics[0];
        lyric.verse = 0;
        CHECK(has_rule(validate_structural(score), "S25"));
    }

    SECTION("verse entries at one onset are canonical and unique") {
        auto score = valid;
        auto& lyrics = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
                           .notes[0]
                           .lyrics;
        lyrics.push_back(LyricSyllable{"again", 1, LyricSyllabic::Single, false});
        CHECK(has_rule(validate_structural(score), "S26"));
    }

    SECTION("middle and ending syllables require a preceding begin") {
        auto score = valid;
        auto& lyric = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
                          .notes[0]
                          .lyrics[0];
        lyric.syllabic = LyricSyllabic::Middle;
        CHECK(has_rule(validate_structural(score), "S26"));
    }

    SECTION("a melisma must reach a later ordinary NoteGroup") {
        auto score = valid;
        auto& final_lyric =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[3].payload)
                .notes[0]
                .lyrics[0];
        final_lyric.extend = true;
        CHECK(has_rule(validate_structural(score), "S26"));
    }

    SECTION("lyrics are onset-scoped and owned only by the first chord note") {
        auto score = valid;
        auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);
        Note upper{SpelledPitch{2, 0, 4}};
        upper.lyrics.push_back(LyricSyllable{"wrong", 1, LyricSyllabic::Single, false});
        group.notes.push_back(std::move(upper));
        CHECK(has_rule(validate_structural(score), "S26"));
    }
}

TEST_CASE("S14 detects NonChordTone referencing non-existent EventId", "[score][validation][s14]") {
    auto score = make_compilable_score(2);
    place_whole_note(score, 0, 0, C4);

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    ha.chord.notes = {MidiNote{60}, MidiNote{64}, MidiNote{67}};
    ha.chord.root = 0;
    ha.chord.quality = "major";
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;

    NonChordToneAnnotation nct;
    nct.event_id = EventId{999999}; // non-existent
    nct.note_index = 0;
    nct.type = NonChordToneType::PassingTone;
    ha.non_chord_tones.push_back(nct);

    score.harmonic_annotations.push_back(ha);

    auto diags = validate_structural(score);
    CHECK(has_warning_rule(diags, "S14"));
    CHECK(is_compilable(score));
    CHECK(compile_to_midi(score).has_value());
}

TEST_CASE("S14 detects NonChordTone with out-of-bounds note_index", "[score][validation][s14]") {
    auto score = make_compilable_score(2);
    place_whole_note(score, 0, 0, C4);

    // Get the actual EventId of the note we placed
    EventId note_id = score.parts[0].measures[0].voices[0].events[0].id;

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    ha.chord.notes = {MidiNote{60}, MidiNote{64}, MidiNote{67}};
    ha.chord.root = 0;
    ha.chord.quality = "major";
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;

    NonChordToneAnnotation nct;
    nct.event_id = note_id;
    nct.note_index = 5; // only 1 note, so index 5 is out of bounds
    nct.type = NonChordToneType::NeighborTone;
    ha.non_chord_tones.push_back(nct);

    score.harmonic_annotations.push_back(ha);

    auto diags = validate_structural(score);
    CHECK(has_warning_rule(diags, "S14"));
    CHECK(is_compilable(score));
    CHECK(compile_to_midi(score).has_value());
}
