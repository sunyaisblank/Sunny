/**
 * @file mutations_test.cpp
 * @brief Unit tests for Score IR mutation operations — extended coverage
 *
 *
 * Coverage: add_voice, remove_voice, reorder_parts, assign_instrument,
 *           copy_region, retrograde_region, augment_region, diminute_region,
 *           invert_region, set_dynamic_region, scale_velocity_region,
 *           reorchestrate
 */

#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/validation.hpp>

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

/// Place a note in the first bar of the first part, replacing the rest
void place_note_in_bar(Score& score, std::uint32_t bar, SpelledPitch pitch, Beat duration) {
    auto& voice = score.parts[0].measures[bar - 1].voices[0];
    NoteGroup ng;
    ng.notes.push_back(Note{pitch, VelocityValue{{}, 80}});
    ng.duration = duration;
    voice.events[0].payload = ng;
}

void place_primary_beam_in_bar(Score& score,
                               std::uint32_t bar,
                               std::uint64_t event_base = 9600,
                               std::uint64_t beam_id_value = 960) {
    auto& voice = score.parts[0].measures[bar - 1].voices[0];
    voice.events.clear();
    voice.beam_groups.clear();
    const BeamGroupId beam_id{beam_id_value};
    BeamGroup beam{beam_id, {}, {}};
    for (std::uint64_t i = 0; i < 4; ++i) {
        NoteGroup group;
        group.notes.push_back(
            Note{SpelledPitch{static_cast<std::uint8_t>(i), 0, 4}, VelocityValue{{}, 80}});
        group.duration = Beat{1, 8};
        group.beam_group = beam_id;
        const EventId event_id{event_base + i};
        voice.events.push_back(
            Event{event_id, Beat{static_cast<std::int64_t>(i), 8}, std::move(group)});
        beam.event_ids.push_back(event_id);
    }
    voice.events.push_back(Event{EventId{event_base + 4}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});
    voice.beam_groups.push_back(std::move(beam));
}

} // anonymous namespace

TEST_CASE("typed chord-symbol insertion is atomic and undoable",
          "[score-ir][mutation][harmony][atomicity]") {
    auto score = make_valid_score(1);
    UndoStack history;

    ChordSymbolEvent chord;
    chord.root = SpelledPitch{4, 0, 4}; // G = V in C major
    chord.quality = "dominant";
    chord.roman = "V7";
    chord.numeral = ChordNumeral{5, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
    chord.degrees = {{9, -1, ChordDegreeType::Add}};

    const auto inserted =
        insert_chord_symbol(score, PartId{100}, 1, 0, Beat{1, 2}, chord, &history);
    REQUIRE(inserted.has_value());
    CHECK(history.can_undo());
    CHECK(score.version == 2);
    REQUIRE(score.parts[0].measures[0].voices[0].events.size() == 2);
    const auto* stored =
        std::get_if<ChordSymbolEvent>(&score.parts[0].measures[0].voices[0].events[1].payload);
    REQUIRE(stored != nullptr);
    CHECK(stored->numeral == chord.numeral);
    CHECK(stored->degrees == chord.degrees);
    CHECK(is_compilable(score));

    REQUIRE(undo(score, history));
    CHECK(score.version == 3);
    CHECK(score.parts[0].measures[0].voices[0].events.size() == 1);
    CHECK(score.parts[0].measures[0].voices[0].events[0].is_rest());

    ChordSymbolEvent contradictory = chord;
    contradictory.root = SpelledPitch{0, 0, 4}; // C contradicts V in C major
    const auto before_rejection = score_to_json(score);
    const auto rejected =
        insert_chord_symbol(score, PartId{100}, 1, 0, Beat{1, 2}, contradictory, &history);
    CHECK_FALSE(rejected.has_value());
    CHECK(score_to_json(score) == before_rejection);
    CHECK_FALSE(history.can_undo());

    contradictory = chord;
    contradictory.root.accidental = 12; // Same pitch class as G, wrong exact spelling.
    CHECK_FALSE(insert_chord_symbol(score, PartId{100}, 1, 0, Beat{1, 2}, contradictory, &history)
                    .has_value());
    CHECK(score_to_json(score) == before_rejection);
}

TEST_CASE("every closed numeral mode derives a coherent spelled root",
          "[score-ir][mutation][harmony][mode]") {
    const std::array modes{ChordNumeralMode::Major,
                           ChordNumeralMode::Minor,
                           ChordNumeralMode::NaturalMinor,
                           ChordNumeralMode::MelodicMinor,
                           ChordNumeralMode::HarmonicMinor};
    for (const auto mode : modes) {
        auto score = make_valid_score(1);
        ChordSymbolEvent chord;
        chord.root =
            mode == ChordNumeralMode::Major ? SpelledPitch{0, 0, 4} : SpelledPitch{5, 0, 4};
        chord.quality = mode == ChordNumeralMode::Major ? "major" : "minor";
        chord.numeral = ChordNumeral{1, 0, ChordNumeralKey{0, mode}};
        CAPTURE(static_cast<int>(mode));
        CHECK(insert_chord_symbol(score, PartId{100}, 1, 0, Beat{1, 2}, chord).has_value());
    }

    auto altered_score = make_valid_score(1);
    ChordSymbolEvent altered;
    altered.root = SpelledPitch{5, 1, 4}; // A-sharp = raised tonic in A harmonic minor
    altered.quality = "diminished";
    altered.numeral = ChordNumeral{1, 1, ChordNumeralKey{0, ChordNumeralMode::HarmonicMinor}};
    CHECK(insert_chord_symbol(altered_score, PartId{100}, 1, 0, Beat{1, 2}, altered).has_value());
}

TEST_CASE("event allocation fills document-local holes below uint64 max",
          "[score-ir][mutation][identity][atomicity]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events[0].id = EventId{std::numeric_limits<std::uint64_t>::max()};

    Note note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}};
    const auto result =
        insert_note(score, score.parts[0].id, 1, 0, Beat{1, 4}, std::move(note), Beat{1, 4});
    REQUIRE(result.has_value());
    REQUIRE(events.size() == 3);
    CHECK(events[0].id == EventId{std::numeric_limits<std::uint64_t>::max()});
    CHECK(events[1].id == EventId{2});
    CHECK(events[2].id == EventId{1});
    CHECK(is_compilable(score));
}

TEST_CASE("part allocation is independent of high imported identifiers",
          "[score-ir][mutation][identity][atomicity]") {
    auto score = make_valid_score(1);
    score.parts[0].id = PartId{std::numeric_limits<std::uint64_t>::max()};

    PartDefinition violin;
    violin.name = "Violin";
    violin.instrument_type = InstrumentType::Violin;
    const auto result = add_part(score, violin, 1);
    REQUIRE(result.has_value());
    REQUIRE(score.parts.size() == 2);
    CHECK(score.parts[1].id == PartId{1});
    CHECK(is_compilable(score));
}

TEST_CASE("rejected candidate allocation consumes no hidden identity state",
          "[score-ir][mutation][identity][atomicity]") {
    auto after_rejection = make_valid_score(4);
    place_note_in_bar(after_rejection, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});
    place_note_in_bar(after_rejection, 2, SpelledPitch{1, 0, 4}, Beat{1, 1});
    auto clean = after_rejection;
    const Score before_failure = after_rejection;
    const ScoreRegion source{SCORE_START, ScoreTime{3, Beat::zero()}, {PartId{100}}};

    const auto failed = copy_region(after_rejection, source, ScoreTime{4, Beat::zero()});
    REQUIRE_FALSE(failed.has_value());
    CHECK(after_rejection.version == before_failure.version);
    CHECK(score_to_json(after_rejection) == score_to_json(before_failure));

    REQUIRE(copy_region(after_rejection, source, ScoreTime{3, Beat::zero()}));
    REQUIRE(copy_region(clean, source, ScoreTime{3, Beat::zero()}));
    for (std::size_t measure_index : {std::size_t{2}, std::size_t{3}}) {
        const auto& after_events =
            after_rejection.parts[0].measures[measure_index].voices[0].events;
        const auto& clean_events = clean.parts[0].measures[measure_index].voices[0].events;
        REQUIRE(after_events.size() == clean_events.size());
        for (std::size_t event_index = 0; event_index < after_events.size(); ++event_index)
            CHECK(after_events[event_index].id == clean_events[event_index].id);
    }
}

// =============================================================================
// Voice-Level Mutations
// =============================================================================

TEST_CASE("insert_note preserves measure topology by carving rests and merging exact chords",
          "[score-ir][mutation][topology]") {
    auto score = make_valid_score(1);
    Note c;
    c.pitch = SpelledPitch{0, 0, 4};
    c.velocity.value = 80;
    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat{1, 4}, c, Beat{1, 4}));

    const auto& after_insert = score.parts[0].measures[0].voices[0].events;
    REQUIRE(after_insert.size() == 3);
    CHECK(after_insert[0].is_rest());
    CHECK(after_insert[0].duration() == Beat{1, 4});
    CHECK(after_insert[1].is_note_group());
    CHECK(after_insert[2].is_rest());
    CHECK(after_insert[2].duration() == Beat{1, 2});
    CHECK(validate_structural(score).empty());

    Note e = c;
    e.pitch = SpelledPitch{2, 0, 4};
    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat{1, 4}, e, Beat{1, 4}));
    const auto& chord = score.parts[0].measures[0].voices[0].events;
    REQUIRE(chord.size() == 3);
    REQUIRE(chord[1].as_note_group() != nullptr);
    CHECK(chord[1].as_note_group()->notes.size() == 2);
    CHECK(validate_structural(score).empty());
}

TEST_CASE("insert_note rejects partial overlap and out-of-measure ranges transactionally",
          "[score-ir][mutation][topology][trust-boundary]") {
    auto score = make_valid_score(1);
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    REQUIRE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), note, Beat{1, 2}));
    const auto before = score.parts[0].measures[0].voices[0].events;

    CHECK_FALSE(insert_note(score, PartId{100}, 1, 0, Beat{1, 4}, note, Beat{1, 2}));
    CHECK(score.parts[0].measures[0].voices[0].events.size() == before.size());
    CHECK_FALSE(insert_note(score, PartId{100}, 1, 0, Beat{1, 1}, note, Beat{1, 4}));
    CHECK(score.parts[0].measures[0].voices[0].events.size() == before.size());
    CHECK(validate_structural(score).empty());
}

TEST_CASE("event and region mutations close over BeamGroup topology",
          "[score-ir][mutation][beam][topology]") {
    SECTION("chord insertion removes the intersected primary group explicitly") {
        auto score = make_valid_score(1);
        place_primary_beam_in_bar(score, 1);
        REQUIRE(is_compilable(score));

        Note note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}};
        const auto result = insert_note(score, PartId{100}, 1, 0, Beat::zero(), note, Beat{1, 8});
        REQUIRE(result.has_value());
        REQUIRE(result->diagnostics.size() == 1);
        CHECK(result->diagnostics.front().rule == "MUT1");
        const auto& voice = score.parts[0].measures[0].voices[0];
        CHECK(voice.beam_groups.empty());
        for (const auto& event : voice.events) {
            if (const auto* group = event.as_note_group()) CHECK_FALSE(group->beam_group);
        }
        CHECK(is_compilable(score));
    }

    SECTION("copy strips source-local beam identity from destination clones") {
        auto score = make_valid_score(2);
        place_primary_beam_in_bar(score, 1);
        REQUIRE(is_compilable(score));

        const auto result =
            copy_region(score,
                        ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {PartId{100}}},
                        ScoreTime{2, Beat::zero()});
        REQUIRE(result.has_value());
        REQUIRE(result->diagnostics.size() == 1);
        CHECK(result->diagnostics.front().rule == "MUT1");
        const auto& source = score.parts[0].measures[0].voices[0];
        const auto& destination = score.parts[0].measures[1].voices[0];
        CHECK(source.beam_groups.size() == 1);
        CHECK(destination.beam_groups.empty());
        for (const auto& event : destination.events) {
            if (const auto* group = event.as_note_group()) CHECK_FALSE(group->beam_group);
        }
        CHECK(is_compilable(score));
    }

    SECTION("retrograde removes ordered beam membership before exchanging payloads") {
        auto score = make_valid_score(1);
        place_primary_beam_in_bar(score, 1);
        REQUIRE(is_compilable(score));

        const auto result = retrograde_region(
            score, ScoreRegion{SCORE_START, ScoreTime{1, Beat{1, 2}}, {PartId{100}}});
        REQUIRE(result.has_value());
        REQUIRE(result->diagnostics.size() == 1);
        CHECK(result->diagnostics.front().rule == "MUT1");
        CHECK(score.parts[0].measures[0].voices[0].beam_groups.empty());
        CHECK(is_compilable(score));
    }
}

TEST_CASE("event mutations reject closed-domain and tie violations atomically",
          "[score-ir][mutation][S25][atomicity]") {
    SECTION("invalid inserted payload") {
        auto score = make_valid_score(1);
        Note invalid;
        invalid.pitch.letter = 7;
        invalid.velocity.value = 200;
        const auto version = score.version;
        CHECK_FALSE(insert_note(score, PartId{100}, 1, 0, Beat::zero(), invalid, Beat{1, 4}));
        CHECK(score.version == version);
        CHECK(is_compilable(score));
    }

    SECTION("tie-breaking pitch change") {
        auto score = make_valid_score(1);
        auto& events = score.parts[0].measures[0].voices[0].events;
        const Note note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}};
        events = {Event{EventId{9500}, Beat::zero(), NoteGroup{{note}, Beat{1, 2}}},
                  Event{EventId{9501}, Beat{1, 2}, NoteGroup{{note}, Beat{1, 2}}}};
        REQUIRE(set_tie(score, EventId{9500}, 0, true));
        const auto version = score.version;
        CHECK_FALSE(modify_pitch(score, EventId{9500}, 0, SpelledPitch{1, 0, 4}));
        CHECK(score.version == version);
        const auto* group = score.parts[0].measures[0].voices[0].events[0].as_note_group();
        REQUIRE(group != nullptr);
        CHECK(group->notes[0].pitch == SpelledPitch{0, 0, 4});
        CHECK(group->notes[0].tie_forward);
        CHECK(is_compilable(score));
    }
}

TEST_CASE("region mutations reject malformed coordinates and Part filters without version changes",
          "[score-ir][mutation][region][trust-boundary][atomicity]") {
    auto score = make_valid_score(1);
    const auto version = score.version;

    CHECK_FALSE(transpose_region(
        score, ScoreRegion{ScoreTime{1, Beat{1, 2}}, ScoreTime{1, Beat{1, 4}}, {}}, MAJOR_SECOND));
    CHECK_FALSE(delete_region(
        score, ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {PartId{99999}}}));
    CHECK_FALSE(
        scale_velocity_region(score, ScoreRegion{SCORE_START, ScoreTime{1, Beat{5, 4}}, {}}, 0.5));
    CHECK(score.version == version);
    CHECK(is_compilable(score));
}

TEST_CASE("clearing one glissando articulation cascades to its paired endpoint",
          "[score-ir][mutation][span-endpoint]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    Note start{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}};
    Note end{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}};
    start.articulation = ArticulationType::GlissandoStart;
    end.articulation = ArticulationType::GlissandoEnd;
    events = {Event{EventId{9510}, Beat::zero(), NoteGroup{{start}, Beat{1, 2}}},
              Event{EventId{9511}, Beat{1, 2}, NoteGroup{{end}, Beat{1, 2}}}};
    REQUIRE(is_compilable(score));

    REQUIRE(set_articulation(score, EventId{9510}, 0, std::nullopt));
    const auto* first = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    const auto* second = score.parts[0].measures[0].voices[0].events[1].as_note_group();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK_FALSE(first->notes[0].articulation.has_value());
    CHECK_FALSE(second->notes[0].articulation.has_value());
    CHECK(is_compilable(score));
}

TEST_CASE("add_voice increases voice count", "[score-ir][mutation]") {
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

TEST_CASE("add_voice preserves canonical voice-index order", "[score-ir][mutation][identity]") {
    auto score = make_valid_score(1);

    REQUIRE(add_voice(score, 1, PartId{100}, 2));
    REQUIRE(add_voice(score, 1, PartId{100}, 1));

    const auto& voices = score.parts[0].measures[0].voices;
    REQUIRE(voices.size() == 3);
    CHECK(voices[0].voice_index == 0);
    CHECK(voices[1].voice_index == 1);
    CHECK(voices[2].voice_index == 2);
    CHECK(is_compilable(score));
}

TEST_CASE("add_voice assigns an explicit valid staff transactionally",
          "[score-ir][mutation][staff]") {
    auto score = make_valid_score(1);
    score.parts[0].definition.staff_count = 2;
    score.parts[0].definition.staff_clefs = {Clef::Treble, Clef::Bass};

    REQUIRE(add_voice(score, 1, PartId{100}, 1, 1));
    REQUIRE(score.parts[0].measures[0].voices.size() == 2);
    CHECK(score.parts[0].measures[0].voices[1].staff_index == 1);
    CHECK(is_compilable(score));

    const auto version = score.version;
    CHECK_FALSE(add_voice(score, 1, PartId{100}, 2, 2));
    CHECK(score.version == version);
    CHECK(score.parts[0].measures[0].voices.size() == 2);
}

TEST_CASE("remove_voice removes voice", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Add a second voice first
    auto add_result = add_voice(score, 1, PartId{100}, 1);
    REQUIRE(add_result.has_value());
    CHECK(score.parts[0].measures[0].voices.size() == 2);

    auto result = remove_voice(score, 1, PartId{100}, 1);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].measures[0].voices.size() == 1);
}

TEST_CASE("remove_voice fails on last voice", "[score-ir][mutation]") {
    auto score = make_valid_score(1);
    CHECK(score.parts[0].measures[0].voices.size() == 1);

    auto result = remove_voice(score, 1, PartId{100}, 0);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("remove_voice repairs a notation span ending in another measure",
          "[score-ir][mutation][span-endpoint]") {
    auto score = make_valid_score(2);
    REQUIRE(add_voice(score, 1, PartId{100}, 1));
    REQUIRE(add_voice(score, 2, PartId{100}, 1));
    auto& start = score.parts[0].measures[0].voices[1].events[0];
    auto& end = score.parts[0].measures[1].voices[1].events[0];
    start.payload = NoteGroup{{Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}}, Beat{1, 1}};
    end.payload = NoteGroup{{Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}}}, Beat{1, 1}};
    std::get<NoteGroup>(start.payload).slur_start = true;
    std::get<NoteGroup>(end.payload).slur_end = true;
    REQUIRE(is_compilable(score));

    REQUIRE(remove_voice(score, 1, PartId{100}, 1));
    REQUIRE(score.parts[0].measures[1].voices[1].events[0].as_note_group() != nullptr);
    CHECK_FALSE(score.parts[0].measures[1].voices[1].events[0].as_note_group()->slur_end);
    CHECK(is_compilable(score));
}

TEST_CASE("delete_measures rebases a cut tempo ramp at the splice",
          "[score-ir][mutation][tempo-transition][global-map]") {
    auto make_ramp_score = [] {
        auto score = make_valid_score(4);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{4, Beat::zero()};
        target.bpm = make_bpm(60);
        target.transition_type = TempoTransitionType::Linear;
        target.linear_duration = Beat{3, 1};
        score.tempo_map.push_back(target);
        return score;
    };

    SECTION("interior deletion materialises the source end-boundary rate") {
        auto score = make_ramp_score();
        REQUIRE(delete_measures(score, 2, 1));
        REQUIRE(score.tempo_map.size() == 3);
        CHECK(score.tempo_map[1].position == ScoreTime{2, Beat::zero()});
        CHECK(score.tempo_map[1].bpm == PositiveRational{80, 1});
        CHECK(score.tempo_map[1].transition_type == TempoTransitionType::Immediate);
        CHECK(score.tempo_map[2].position == ScoreTime{3, Beat::zero()});
        CHECK(score.tempo_map[2].linear_duration == Beat{1, 1});
        CHECK(is_compilable(score));
    }

    SECTION("deleting the first measure creates a new exact origin") {
        auto score = make_ramp_score();
        REQUIRE(delete_measures(score, 1, 1));
        REQUIRE(score.tempo_map.size() == 2);
        CHECK(score.tempo_map.front().position == SCORE_START);
        CHECK(score.tempo_map.front().bpm == PositiveRational{100, 1});
        CHECK(score.tempo_map.front().transition_type == TempoTransitionType::Immediate);
        CHECK(score.tempo_map.back().position == ScoreTime{3, Beat::zero()});
        CHECK(score.tempo_map.back().linear_duration == Beat{2, 1});
        CHECK(score.key_map.front().position == SCORE_START);
        CHECK(score.time_map.front().bar == 1);
        CHECK(is_compilable(score));
    }
}

TEST_CASE("insert_measures extends incoming ramps and every position-bearing span",
          "[score-ir][mutation][tempo-transition][global-map][topology]") {
    auto score = make_valid_score(4);
    TempoEvent target = score.tempo_map.front();
    target.position = ScoreTime{4, Beat::zero()};
    target.bpm = make_bpm(60);
    target.transition_type = TempoTransitionType::Linear;
    target.linear_duration = Beat{3, 1};
    score.tempo_map.push_back(target);

    score.parts[0].hairpins.push_back(Hairpin{ScoreTime{2, Beat::zero()},
                                              ScoreTime{4, Beat::zero()},
                                              HairpinType::Crescendo,
                                              std::nullopt});
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{2, Beat::zero()}, ScoreTime{4, Beat::zero()}, DirectiveType::Mute, 0});
    score.orchestration_annotations.push_back(OrchestrationAnnotation{score.parts[0].id,
                                                                      ScoreTime{2, Beat::zero()},
                                                                      ScoreTime{4, Beat::zero()},
                                                                      TexturalRole::Melody,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt});
    ScoreSection child{SectionId{9301},
                       "child",
                       ScoreTime{2, Beat::zero()},
                       ScoreTime{4, Beat::zero()},
                       {},
                       std::nullopt};
    score.section_map.push_back(ScoreSection{
        SectionId{9300}, "parent", SCORE_START, ScoreTime{5, Beat::zero()}, {child}, std::nullopt});
    REQUIRE(is_compilable(score));

    REQUIRE(insert_measures(score, 2, 2));
    REQUIRE(score.tempo_map.size() == 2);
    CHECK(score.tempo_map.back().position == ScoreTime{6, Beat::zero()});
    CHECK(score.tempo_map.back().linear_duration == Beat{5, 1});
    CHECK(score.parts[0].hairpins.front().end == ScoreTime{6, Beat::zero()});
    CHECK(score.parts[0].part_directives.front().end == ScoreTime{6, Beat::zero()});
    CHECK(score.orchestration_annotations.front().end == ScoreTime{6, Beat::zero()});
    CHECK(score.section_map.front().end == ScoreTime{7, Beat::zero()});
    CHECK(score.section_map.front().children.front().end == ScoreTime{6, Beat::zero()});
    CHECK(is_compilable(score));
}

TEST_CASE("insert_measures rejects non-operations without changing version",
          "[score-ir][mutation][atomicity]") {
    auto score = make_valid_score(2);
    const auto version = score.version;
    CHECK_FALSE(insert_measures(score, 1, 0));
    CHECK_FALSE(insert_measures(score, 0, 1));
    CHECK(score.version == version);
    CHECK(score.metadata.total_bars == 2);
}

TEST_CASE("set_time_signature retiles rests, recomputes ramps, and rejects clipping",
          "[score-ir][mutation][tempo-transition][topology][atomicity]") {
    SECTION("rest-only affected bars and incoming ramp remain exact") {
        auto score = make_valid_score(4);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{4, Beat::zero()};
        target.bpm = make_bpm(60);
        target.transition_type = TempoTransitionType::Linear;
        target.linear_duration = Beat{3, 1};
        score.tempo_map.push_back(target);
        const auto three_four = make_time_signature(3, 4);
        REQUIRE(three_four.has_value());

        REQUIRE(set_time_signature(score, 2, *three_four));
        CHECK(score.tempo_map.back().linear_duration == Beat{5, 2});
        CHECK(score.parts[0].measures[1].voices[0].events[0].duration() == Beat{3, 4});
        CHECK(is_compilable(score));
    }

    SECTION("a shorter measure cannot clip music") {
        auto score = make_valid_score(1);
        place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});
        const auto version = score.version;
        const auto three_four = make_time_signature(3, 4);
        REQUIRE(three_four.has_value());

        CHECK_FALSE(set_time_signature(score, 1, *three_four));
        CHECK(score.version == version);
        CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == Beat{1, 1});
        CHECK(is_compilable(score));
    }
}

TEST_CASE("set_key_signature rejects invalid time and key domains atomically",
          "[score-ir][mutation][global-map][atomicity]") {
    auto score = make_valid_score(1);
    const auto version = score.version;
    const KeySignature valid_key = score.key_map.front().key;
    CHECK_FALSE(set_key_signature(score, ScoreTime{1, Beat{1, 1}}, valid_key));

    KeySignature invalid_key = valid_key;
    invalid_key.root.letter = 7;
    CHECK_FALSE(set_key_signature(score, SCORE_START, invalid_key));
    CHECK(score.version == version);
    CHECK(score.key_map.size() == 1);
    CHECK(is_compilable(score));
}

TEST_CASE("delete_measures clips crossing spans and removes collapsed spans",
          "[score-ir][mutation][global-map][topology]") {
    auto score = make_valid_score(5);
    score.parts[0].hairpins.push_back(Hairpin{ScoreTime{2, Beat::zero()},
                                              ScoreTime{5, Beat::zero()},
                                              HairpinType::Crescendo,
                                              std::nullopt});
    score.parts[0].part_directives = {
        PartDirective{
            ScoreTime{2, Beat::zero()}, ScoreTime{5, Beat::zero()}, DirectiveType::Mute, 0},
        PartDirective{
            ScoreTime{3, Beat::zero()}, ScoreTime{4, Beat::zero()}, DirectiveType::Solo, 0}};
    score.orchestration_annotations.push_back(OrchestrationAnnotation{score.parts[0].id,
                                                                      ScoreTime{2, Beat::zero()},
                                                                      ScoreTime{5, Beat::zero()},
                                                                      TexturalRole::Melody,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      std::nullopt});
    ScoreSection child{SectionId{9401},
                       "child",
                       ScoreTime{3, Beat::zero()},
                       ScoreTime{4, Beat::zero()},
                       {},
                       std::nullopt};
    score.section_map.push_back(ScoreSection{
        SectionId{9400}, "parent", SCORE_START, ScoreTime{6, Beat::zero()}, {child}, std::nullopt});
    REQUIRE(is_compilable(score));

    REQUIRE(delete_measures(score, 2, 2));
    REQUIRE(score.parts[0].hairpins.size() == 1);
    CHECK(score.parts[0].hairpins.front().start == ScoreTime{2, Beat::zero()});
    CHECK(score.parts[0].hairpins.front().end == ScoreTime{3, Beat::zero()});
    REQUIRE(score.parts[0].part_directives.size() == 1);
    CHECK(score.parts[0].part_directives.front().start == ScoreTime{2, Beat::zero()});
    CHECK(score.parts[0].part_directives.front().end == ScoreTime{3, Beat::zero()});
    REQUIRE(score.orchestration_annotations.size() == 1);
    CHECK(score.orchestration_annotations.front().start == ScoreTime{2, Beat::zero()});
    CHECK(score.orchestration_annotations.front().end == ScoreTime{3, Beat::zero()});
    REQUIRE(score.section_map.size() == 1);
    CHECK(score.section_map.front().end == ScoreTime{4, Beat::zero()});
    CHECK(score.section_map.front().children.empty());
    CHECK(is_compilable(score));
}

// =============================================================================
// Part Management Mutations
// =============================================================================

TEST_CASE("remove_part cascades identity-bearing orchestration and stale regions",
          "[score-ir][mutation][identity][atomicity]") {
    auto score = make_valid_score(1);
    PartDefinition violin;
    violin.name = "Violin";
    violin.instrument_type = InstrumentType::Violin;
    REQUIRE(add_part(score, violin, 1));
    const PartId removed = score.parts[1].id;
    score.orchestration_annotations.push_back(OrchestrationAnnotation{score.parts[0].id,
                                                                      SCORE_START,
                                                                      ScoreTime{2, Beat::zero()},
                                                                      TexturalRole::Doubling,
                                                                      std::nullopt,
                                                                      std::nullopt,
                                                                      removed,
                                                                      std::nullopt,
                                                                      std::nullopt});
    score.stale_harmonic_regions.push_back(
        ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {removed}});
    score.stale_orchestration_regions.push_back(
        ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {score.parts[0].id, removed}});
    REQUIRE(is_compilable(score));

    REQUIRE(remove_part(score, removed));
    CHECK(score.orchestration_annotations.empty());
    CHECK(score.stale_harmonic_regions.empty());
    REQUIRE(score.stale_orchestration_regions.size() == 1);
    CHECK(score.stale_orchestration_regions.front().parts ==
          std::vector<PartId>{score.parts[0].id});
    CHECK(is_compilable(score));
}

TEST_CASE("add_part rejects an invalid staff topology atomically",
          "[score-ir][mutation][staff][atomicity]") {
    auto score = make_valid_score(1);
    PartDefinition invalid;
    invalid.name = "Invalid";
    invalid.staff_count = 0;
    const auto version = score.version;
    CHECK_FALSE(add_part(score, invalid, 1));
    CHECK(score.version == version);
    CHECK(score.parts.size() == 1);
    CHECK(is_compilable(score));
}

TEST_CASE("reorder_parts changes order", "[score-ir][mutation]") {
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

TEST_CASE("assign_instrument changes instrument type", "[score-ir][mutation]") {
    auto score = make_valid_score();
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Piano);

    auto result = assign_instrument(score, PartId{100}, InstrumentType::Organ);
    REQUIRE(result.has_value());
    CHECK(score.parts[0].definition.instrument_type == InstrumentType::Organ);
}

TEST_CASE("set_articulation_mapping validates, replaces, removes, and participates in undo",
          "[score-ir][mutation][articulation-mapping]") {
    auto score = make_valid_score();
    UndoStack history;
    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::CC;
    mapping.cc_number = 64;
    mapping.cc_value = 127;

    auto added =
        set_articulation_mapping(score, PartId{100}, ArticulationType::Staccato, mapping, &history);
    REQUIRE(added);
    REQUIRE(
        score.parts[0].definition.rendering.articulation_map.contains(ArticulationType::Staccato));
    CHECK(history.can_undo());

    REQUIRE(undo(score, history));
    CHECK_FALSE(
        score.parts[0].definition.rendering.articulation_map.contains(ArticulationType::Staccato));
    REQUIRE(redo(score, history));
    REQUIRE(
        score.parts[0].definition.rendering.articulation_map.contains(ArticulationType::Staccato));

    auto removed =
        set_articulation_mapping(score, PartId{100}, ArticulationType::Staccato, std::nullopt);
    REQUIRE(removed);
    CHECK_FALSE(
        score.parts[0].definition.rendering.articulation_map.contains(ArticulationType::Staccato));

    ArticulationMapping invalid;
    invalid.type = ArticulationMapping::Type::VelocityLayer;
    invalid.velocity_min = 100;
    invalid.velocity_max = 20;
    auto rejected = set_articulation_mapping(score, PartId{100}, ArticulationType::Accent, invalid);
    REQUIRE_FALSE(rejected);
    CHECK(rejected.error() == ErrorCode::InvalidRenderingConfig);
    CHECK_FALSE(
        score.parts[0].definition.rendering.articulation_map.contains(ArticulationType::Accent));
}

// =============================================================================
// Region-Level Mutations
// =============================================================================

TEST_CASE("copy_region copies notes to destination", "[score-ir][mutation]") {
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
    CHECK(is_compilable(score));
}

TEST_CASE("delete_event atomically removes both ends of Voice notation spans",
          "[score-ir][mutation][span-endpoint]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();
    for (std::int64_t index = 0; index < 4; ++index) {
        Note note{SpelledPitch{static_cast<std::uint8_t>(index), 0, 4}, VelocityValue{{}, 80}};
        NoteGroup group{{note}, Beat{1, 4}};
        group.slur_start = index == 0 || index == 1;
        group.slur_end = index == 1 || index == 2;
        events.push_back(
            Event{EventId{9100 + static_cast<std::uint64_t>(index)}, Beat{index, 4}, group});
    }
    REQUIRE(is_compilable(score));

    REQUIRE(delete_event(score, EventId{9101}));
    REQUIRE(events[0].as_note_group() != nullptr);
    REQUIRE(events[2].as_note_group() != nullptr);
    CHECK_FALSE(events[0].as_note_group()->slur_start);
    CHECK(events[1].is_rest());
    CHECK_FALSE(events[2].as_note_group()->slur_end);
    CHECK(is_compilable(score));
}

TEST_CASE("delete_event closes glissando and duplicate pedal representations",
          "[score-ir][mutation][span-endpoint][pedal]") {
    SECTION("glissando counterpart metadata is cleared") {
        auto score = make_valid_score(1);
        auto& events = score.parts[0].measures[0].voices[0].events;
        Note first{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}};
        first.articulation = ArticulationType::GlissandoStart;
        Note second{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}};
        second.articulation = ArticulationType::GlissandoEnd;
        events = {Event{EventId{9110}, Beat::zero(), NoteGroup{{first}, Beat{1, 2}}},
                  Event{EventId{9111}, Beat{1, 2}, NoteGroup{{second}, Beat{1, 2}}}};
        REQUIRE(is_compilable(score));

        REQUIRE(delete_event(score, EventId{9110}));
        REQUIRE(events[1].as_note_group() != nullptr);
        CHECK_FALSE(events[1].as_note_group()->notes.front().articulation.has_value());
        CHECK(is_compilable(score));
    }

    SECTION("one pedal endpoint deletes its mate and exact PartDirective alias") {
        auto score = make_valid_score(1);
        auto& events = score.parts[0].measures[0].voices[0].events;
        ScoreDirection down;
        down.type = DirectionType::PedalDown;
        ScoreDirection up;
        up.type = DirectionType::PedalUp;
        events.push_back(Event{EventId{9120}, Beat{1, 4}, down});
        events.push_back(Event{EventId{9121}, Beat{3, 4}, up});
        score.parts[0].part_directives.push_back(PartDirective{
            ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::SustainingPedal, 0});
        REQUIRE(is_compilable(score));

        REQUIRE(delete_event(score, EventId{9120}));
        CHECK(std::ranges::none_of(events, [](const Event& event) {
            return event.id == EventId{9120} || event.id == EventId{9121};
        }));
        CHECK(score.parts[0].part_directives.empty());
        CHECK(is_compilable(score));
    }
}

TEST_CASE("delete_region repairs a span crossing its boundary",
          "[score-ir][mutation][span-endpoint]") {
    auto score = make_valid_score(2);
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});
    place_note_in_bar(score, 2, SpelledPitch{1, 0, 4}, Beat{1, 1});
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).slur_start = true;
    std::get<NoteGroup>(score.parts[0].measures[1].voices[0].events[0].payload).slur_end = true;
    REQUIRE(is_compilable(score));

    REQUIRE(delete_region(score, ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {}}));
    CHECK(score.parts[0].measures[0].voices[0].events[0].is_rest());
    CHECK_FALSE(score.parts[0].measures[1].voices[0].events[0].as_note_group()->slur_end);
    CHECK(is_compilable(score));
}

TEST_CASE("copy_region preserves complete spans and strips boundary-crossing endpoints",
          "[score-ir][mutation][span-endpoint][topology]") {
    SECTION("partial selection does not clone an orphan start") {
        auto score = make_valid_score(4);
        for (std::uint32_t bar = 1; bar <= 3; ++bar)
            place_note_in_bar(
                score, bar, SpelledPitch{static_cast<std::uint8_t>(bar - 1), 0, 4}, Beat{1, 1});
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).slur_start =
            true;
        std::get<NoteGroup>(score.parts[0].measures[2].voices[0].events[0].payload).slur_end = true;
        REQUIRE(is_compilable(score));

        REQUIRE(copy_region(score,
                            ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {}},
                            ScoreTime{4, Beat::zero()}));
        const auto* clone = score.parts[0].measures[3].voices[0].events[0].as_note_group();
        REQUIRE(clone != nullptr);
        CHECK_FALSE(clone->slur_start);
        CHECK(is_compilable(score));
    }

    SECTION("complete selected span remains paired at the destination") {
        auto score = make_valid_score(4);
        place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});
        place_note_in_bar(score, 2, SpelledPitch{1, 0, 4}, Beat{1, 1});
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).slur_start =
            true;
        std::get<NoteGroup>(score.parts[0].measures[1].voices[0].events[0].payload).slur_end = true;
        REQUIRE(is_compilable(score));

        REQUIRE(copy_region(score,
                            ScoreRegion{SCORE_START, ScoreTime{3, Beat::zero()}, {}},
                            ScoreTime{3, Beat::zero()}));
        const auto* copied_start = score.parts[0].measures[2].voices[0].events[0].as_note_group();
        const auto* copied_end = score.parts[0].measures[3].voices[0].events[0].as_note_group();
        REQUIRE(copied_start != nullptr);
        REQUIRE(copied_end != nullptr);
        CHECK(copied_start->slur_start);
        CHECK(copied_end->slur_end);
        CHECK(is_compilable(score));
    }
}

TEST_CASE("retrograde_region reverses event payloads", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Set up two events: C4 half note then E4 half note
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}}); // E4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{2001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{2002}, Beat{1, 2}, ng2});

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{2, Beat::zero()};

    auto result = retrograde_region(score, region);
    REQUIRE(result.has_value());

    // After retrograde, the first event should have E4 and second C4
    const auto& transformed = score.parts[0].measures[0].voices[0].events;
    auto* first_ng = transformed[0].as_note_group();
    auto* second_ng = transformed[1].as_note_group();
    REQUIRE(first_ng != nullptr);
    REQUIRE(second_ng != nullptr);
    CHECK(first_ng->notes[0].pitch.letter == 2);  // E
    CHECK(second_ng->notes[0].pitch.letter == 0); // C
}

TEST_CASE("retrograde repacks unequal durations, preserves points, and removes unstable spans",
          "[score-ir][mutation][span-endpoint][topology]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    NoteGroup short_group{{Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}}, Beat{1, 4}};
    short_group.slur_start = true;
    NoteGroup long_group{{Note{SpelledPitch{2, 0, 4}, VelocityValue{{}, 80}}}, Beat{3, 4}};
    long_group.slur_end = true;
    ScoreDirection text;
    text.type = DirectionType::Text;
    text.text = "keep time";
    events = {Event{EventId{9200}, Beat::zero(), short_group},
              Event{EventId{9201}, Beat{1, 4}, long_group},
              Event{EventId{9202}, Beat{1, 2}, text}};
    REQUIRE(is_compilable(score));

    const auto result =
        retrograde_region(score, ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {}});
    REQUIRE(result.has_value());
    REQUIRE(result->diagnostics.size() == 1);
    const auto& transformed = score.parts[0].measures[0].voices[0].events;
    const auto first_note = std::ranges::find_if(transformed, [](const Event& event) {
        return event.is_note_group() && event.offset == Beat::zero();
    });
    const auto second_note = std::ranges::find_if(transformed, [](const Event& event) {
        return event.is_note_group() && event.offset == Beat{3, 4};
    });
    const auto point =
        std::ranges::find_if(transformed, [](const Event& event) { return event.is_direction(); });
    REQUIRE(first_note != transformed.end());
    REQUIRE(second_note != transformed.end());
    REQUIRE(point != transformed.end());
    CHECK(first_note->as_note_group()->notes.front().pitch.letter == 2);
    CHECK(second_note->as_note_group()->notes.front().pitch.letter == 0);
    CHECK_FALSE(first_note->as_note_group()->slur_end);
    CHECK_FALSE(second_note->as_note_group()->slur_start);
    CHECK(point->offset == Beat{1, 2});
    CHECK(is_compilable(score));
}

TEST_CASE("augment_region doubles durations", "[score-ir][mutation]") {
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
    auto* updated = score.parts[0].measures[0].voices[0].events[0].as_note_group();
    REQUIRE(updated != nullptr);
    CHECK(updated->duration == Beat{1, 1});
    CHECK(score.parts[0].measures[0].voices[0].events.size() == 1);
    CHECK(is_compilable(score));
}

TEST_CASE("diminute_region halves durations", "[score-ir][mutation]") {
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
    REQUIRE(score.parts[0].measures[0].voices[0].events.size() == 2);
    CHECK(score.parts[0].measures[0].voices[0].events[1].is_rest());
    CHECK(score.parts[0].measures[0].voices[0].events[1].offset == Beat{1, 2});
    CHECK(is_compilable(score));
}

TEST_CASE("duration growth cannot consume musical content and is atomic",
          "[score-ir][mutation][topology][atomicity]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    const NoteGroup first{{Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}}, Beat{1, 4}};
    const NoteGroup second{{Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}}}, Beat{1, 4}};
    events = {Event{EventId{9300}, Beat::zero(), first},
              Event{EventId{9301}, Beat{1, 4}, second},
              Event{EventId{9302}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}}};
    REQUIRE(is_compilable(score));
    const std::uint64_t version_before = score.version;

    const auto result =
        augment_region(score, ScoreRegion{SCORE_START, ScoreTime{1, Beat{1, 4}}, {}}, Beat{2, 1});
    CHECK_FALSE(result.has_value());
    CHECK(score.version == version_before);
    REQUIRE(score.parts[0].measures[0].voices[0].events.size() == 3);
    CHECK(score.parts[0].measures[0].voices[0].events[0].duration() == Beat{1, 4});
    CHECK(score.parts[0].measures[0].voices[0].events[1].duration() == Beat{1, 4});
    CHECK(is_compilable(score));
}

TEST_CASE("invert_region inverts pitches around axis", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Place a C4 whole note — invert around C4 should stay C4
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});

    SpelledPitch axis{0, 0, 4}; // C4
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

TEST_CASE("set_dynamic_region applies dynamic", "[score-ir][mutation]") {
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

TEST_CASE("scale_velocity_region scales by factor", "[score-ir][mutation]") {
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
    CHECK(ng->notes[0].velocity.value == 40); // 80 * 0.5 = 40
}

// =============================================================================
// Orchestration Mutations
// =============================================================================

TEST_CASE("reorchestrate copies notes to target part", "[score-ir][mutation]") {
    auto score = make_valid_score(1);

    // Add a violin part
    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.range = PitchRange{
        SpelledPitch{4, 0, 3}, // G3
        SpelledPitch{2, 0, 7}, // E7
        SpelledPitch{4, 0, 3}, // G3
        SpelledPitch{2, 0, 7}  // E7
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
    region.parts = {PartId{100}}; // Source from piano

    auto result = reorchestrate(score, region, violin_id);
    REQUIRE(result.has_value());

    // The violin part should now have a note in bar 1
    auto& violin_bar1 = score.parts[1].measures[0].voices[0];
    CHECK(violin_bar1.events[0].is_note_group());
    CHECK(is_compilable(score));
}

TEST_CASE("reorchestration does not invent cross-Part Voice-span identity",
          "[score-ir][mutation][span-endpoint][orchestration]") {
    auto score = make_valid_score(2);
    place_note_in_bar(score, 1, SpelledPitch{0, 0, 4}, Beat{1, 1});
    place_note_in_bar(score, 2, SpelledPitch{1, 0, 4}, Beat{1, 1});
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).slur_start = true;
    std::get<NoteGroup>(score.parts[0].measures[1].voices[0].events[0].payload).slur_end = true;

    PartDefinition violin;
    violin.name = "Violin";
    violin.instrument_type = InstrumentType::Violin;
    REQUIRE(add_part(score, violin, 1));
    const PartId target = score.parts[1].id;
    REQUIRE(is_compilable(score));

    ScoreRegion region{SCORE_START, ScoreTime{3, Beat::zero()}, {PartId{100}}};
    const auto result = reorchestrate(score, region, target);
    REQUIRE(result.has_value());
    REQUIRE(result->diagnostics.size() == 1);
    CHECK(result->diagnostics.front().rule == "MUT1");
    const auto* first = score.parts[1].measures[0].voices[0].events[0].as_note_group();
    const auto* second = score.parts[1].measures[1].voices[0].events[0].as_note_group();
    REQUIRE(first != nullptr);
    REQUIRE(second != nullptr);
    CHECK_FALSE(first->slur_start);
    CHECK_FALSE(second->slur_end);
    CHECK(is_compilable(score));
}
