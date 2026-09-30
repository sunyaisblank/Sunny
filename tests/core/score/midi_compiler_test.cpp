/**
 * @file midi_compiler_test.cpp
 * @brief Unit tests for Score IR MIDI compilation
 *
 *
 * Coverage: compile_to_midi, velocity resolution pipeline,
 *           tie chain accumulation, tempo/time sig/key sig meta events,
 *           multi-part compilation, articulation effects
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/queries.hpp>
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

/// Replace the whole-measure rest in a bar with a single whole note.
/// bar_idx is 0-based.
void place_whole_note(Score& score,
                      std::size_t part_idx,
                      std::size_t bar_idx,
                      SpelledPitch pitch,
                      std::optional<DynamicLevel> dyn = std::nullopt,
                      std::optional<ArticulationType> art = std::nullopt,
                      bool tie_fwd = false) {
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
constexpr SpelledPitch G4{4, 0, 4};

} // anonymous namespace

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
    CHECK(n.part_id == PartId{100});
}

TEST_CASE("part dynamics and hairpins use the latest marking across voices",
          "[score][midi][note-events][hairpin][query]") {
    for (const bool latest_in_first_voice : {false, true}) {
        for (const bool written_velocity : {false, true}) {
            auto score = make_compilable_score(1);
            auto marked_note = [written_velocity](EventId id, Beat offset, DynamicLevel dynamic) {
                Note note;
                note.pitch = C4;
                if (written_velocity)
                    note.velocity.written = dynamic;
                else
                    note.dynamic = dynamic;
                NoteGroup group;
                group.duration = Beat{1, 2};
                group.notes.push_back(note);
                return Event{id, offset, group};
            };
            Voice latest{0,
                         {Event{EventId{10}, Beat::zero(), RestEvent{Beat{1, 2}, false}},
                          marked_note(EventId{11}, Beat{1, 2}, DynamicLevel::f)},
                         {}};
            Voice earlier{1,
                          {marked_note(EventId{12}, Beat::zero(), DynamicLevel::p),
                           marked_note(EventId{13}, Beat{1, 2}, DynamicLevel::p)},
                          {}};
            auto& unmarked = std::get<NoteGroup>(earlier.events[1].payload).notes[0];
            unmarked.dynamic.reset();
            unmarked.velocity.written.reset();
            auto& voices = score.parts[0].measures[0].voices;
            voices = latest_in_first_voice ? std::vector<Voice>{latest, earlier}
                                           : std::vector<Voice>{earlier, latest};
            voices[0].voice_index = 0;
            voices[1].voice_index = 1;
            score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat{1, 2}},
                                                      ScoreTime{2, Beat::zero()},
                                                      HairpinType::Crescendo,
                                                      DynamicLevel::ff});

            CHECK(query_dynamics_at(score, PartId{100}, ScoreTime{1, Beat{1, 4}}) ==
                  DynamicLevel::p);
            CHECK(query_dynamics_at(score, PartId{100}, ScoreTime{1, Beat{1, 2}}) ==
                  DynamicLevel::f);
            const auto midi = compile_to_midi(score, 480);
            REQUIRE(midi);
            REQUIRE(midi->midi.notes.size() == 3);
            for (const auto& note : midi->midi.notes)
                CHECK(note.velocity ==
                      default_velocity(note.tick == 0 ? DynamicLevel::p : DynamicLevel::f));
            const auto note_events = compile_to_note_events(score);
            REQUIRE(note_events);
            REQUIRE(note_events->events.size() == 3);
            for (const auto& note : note_events->events)
                CHECK(note.velocity == default_velocity(note.start_time == Beat::zero()
                                                            ? DynamicLevel::p
                                                            : DynamicLevel::f));
        }
    }
}

TEST_CASE("part dynamic query preserves simultaneous marking precedence",
          "[score][query][dynamic]") {
    auto score = make_compilable_score(1);
    CHECK_FALSE(query_dynamics_at(score, PartId{100}, SCORE_START));
    CHECK_FALSE(query_dynamics_at(score, PartId{999}, SCORE_START));
    place_whole_note(score, 0, 0, C4, DynamicLevel::p);
    auto& voices = score.parts[0].measures[0].voices;
    auto later_voice = voices[0];
    later_voice.voice_index = 1;
    later_voice.events[0].id = EventId{2000};
    auto& later_note = std::get<NoteGroup>(later_voice.events[0].payload).notes[0];
    later_note.dynamic = DynamicLevel::f;
    later_note.velocity.written = DynamicLevel::pp;
    later_note.velocity.value = 127;
    voices.push_back(std::move(later_voice));
    CHECK(query_dynamics_at(score, PartId{100}, SCORE_START) == DynamicLevel::f);
    auto& chord = std::get<NoteGroup>(voices[1].events[0].payload);
    Note last_note;
    last_note.pitch = E4;
    last_note.velocity.written = DynamicLevel::mf;
    chord.notes.push_back(last_note);
    CHECK(query_dynamics_at(score, PartId{100}, SCORE_START) == DynamicLevel::mf);
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
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
        .notes[0]
        .release_velocity = 17;
    std::get<NoteGroup>(score.parts[0].measures[1].voices[0].events[0].payload)
        .notes[0]
        .release_velocity = 93;

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 1);
    CHECK(midi.notes[0].tick == 0);
    CHECK(midi.notes[0].duration_ticks == 3840);
    CHECK(midi.notes[0].note == 60);
    CHECK(midi.notes[0].release_velocity == 93);

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events.has_value());
    REQUIRE(note_events->events.size() == 1);
    CHECK(note_events->events[0].start_time == Beat::zero());
    CHECK(note_events->events[0].duration == Beat{2, 1});
    CHECK(note_events->events[0].pitch == 60);
    CHECK(note_events->events[0].release_velocity == 93);
}

TEST_CASE("compile_to_midi: point directions do not interrupt a tie",
          "[score][midi][tie][topology]") {
    auto score = make_compilable_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    Note first;
    first.pitch = C4;
    first.tie_forward = true;
    Note second;
    second.pitch = C4;
    ScoreDirection direction;
    direction.type = DirectionType::Text;
    direction.text = "still tied";

    events.push_back(Event{EventId{9051}, Beat::zero(), NoteGroup{{first}, Beat{1, 2}}});
    events.push_back(Event{EventId{9052}, Beat{1, 2}, direction});
    events.push_back(Event{EventId{9053}, Beat{1, 2}, NoteGroup{{second}, Beat{1, 2}}});

    CHECK(is_compilable(score));
    const auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    REQUIRE(result->midi.notes.size() == 1);
    CHECK(result->midi.notes[0].tick == 0);
    CHECK(result->midi.notes[0].duration_ticks == 1920);

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events);
    REQUIRE(note_events->events.size() == 1);
    CHECK(note_events->events[0].duration == Beat{1, 1});
}

TEST_CASE("performance projections retain semantic dynamics on consumed tie continuations",
          "[score][midi][note-events][tie][dynamic]") {
    auto score = make_compilable_score(4);
    place_whole_note(score, 0, 0, C4, std::nullopt, std::nullopt, true);
    place_whole_note(score, 0, 1, C4, DynamicLevel::p, std::nullopt, true);
    place_whole_note(score, 0, 2, C4, DynamicLevel::ff);
    place_whole_note(score, 0, 3, E4);

    auto& head =
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
    head.velocity.value = 73;
    head.release_velocity = 17;
    auto& middle =
        std::get<NoteGroup>(score.parts[0].measures[1].voices[0].events[0].payload).notes[0];
    middle.release_velocity = 41;
    auto& terminal =
        std::get<NoteGroup>(score.parts[0].measures[2].voices[0].events[0].payload).notes[0];
    terminal.release_velocity = 93;

    const auto midi = compile_to_midi(score, 480);
    REQUIRE(midi);
    REQUIRE(midi->midi.notes.size() == 2);
    CHECK(midi->midi.notes[0].velocity == 73);
    CHECK(midi->midi.notes[0].duration_ticks == 5760);
    CHECK(midi->midi.notes[0].release_velocity == 93);
    CHECK(midi->midi.notes[1].velocity == default_velocity(DynamicLevel::ff));

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events);
    REQUIRE(note_events->events.size() == 2);
    CHECK(note_events->events[0].velocity == 73);
    CHECK(note_events->events[0].duration == Beat{3, 1});
    CHECK(note_events->events[0].release_velocity == 93);
    CHECK(note_events->events[1].velocity == default_velocity(DynamicLevel::ff));
}

TEST_CASE("NoteEvent tie collapse consumes only the matching chord pitch",
          "[score][note-events][tie][chord][ordering]") {
    auto score = make_compilable_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note tied_c;
    tied_c.pitch = C4;
    tied_c.tie_forward = true;
    tied_c.release_velocity = 17;
    Note first_e;
    first_e.pitch = E4;
    first_e.release_velocity = 41;
    Note terminal_c;
    terminal_c.pitch = C4;
    terminal_c.release_velocity = 93;
    Note later_g;
    later_g.pitch = G4;
    later_g.release_velocity = 52;

    voice.events.push_back(
        Event{EventId{9061}, Beat::zero(), NoteGroup{{tied_c, first_e}, Beat{1, 2}}});
    voice.events.push_back(
        Event{EventId{9062}, Beat{1, 2}, NoteGroup{{terminal_c, later_g}, Beat{1, 2}}});

    REQUIRE(is_compilable(score));
    const auto result = compile_to_note_events(score);
    REQUIRE(result);
    REQUIRE(result->events.size() == 3);
    CHECK(result->events[0].pitch == 60);
    CHECK(result->events[0].start_time == Beat::zero());
    CHECK(result->events[0].duration == Beat{1, 1});
    CHECK(result->events[0].release_velocity == 93);
    CHECK(result->events[1].pitch == 64);
    CHECK(result->events[1].start_time == Beat::zero());
    CHECK(result->events[1].duration == Beat{1, 2});
    CHECK(result->events[1].release_velocity == 41);
    CHECK(result->events[2].pitch == 67);
    CHECK(result->events[2].start_time == Beat{1, 2});
    CHECK(result->events[2].duration == Beat{1, 2});
    CHECK(result->events[2].release_velocity == 52);
}

TEST_CASE("NoteEvent compilation rejects an orphan tie before projection",
          "[score][note-events][tie][trust-boundary]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, std::nullopt, std::nullopt, true);

    const auto result = compile_to_note_events(score);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvariantViolation);
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
    CHECK(midi.tempos[1].microseconds_per_beat == 666667);
}

TEST_CASE("compile_to_midi: linear tempo uses bounded deterministic step samples",
          "[score][midi][tempo-transition]") {
    auto score = make_compilable_score(4);

    TempoEvent target = score.tempo_map.front();
    target.position = ScoreTime{3, Beat::zero()};
    target.bpm = make_bpm(60);
    target.transition_type = TempoTransitionType::Linear;
    target.linear_duration = Beat{2, 1};
    score.tempo_map.push_back(target);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    const auto& tempos = result->midi.tempos;
    REQUIRE(tempos.size() == 121);
    CHECK(tempos.front().tick == 0);
    CHECK(tempos.front().microseconds_per_beat == 500000);
    CHECK(tempos[1].tick == 32);
    CHECK(tempos[1].microseconds_per_beat == 502092);
    CHECK(tempos[60].tick == 1920);
    CHECK(tempos[60].microseconds_per_beat == 666667);
    CHECK(tempos.back().tick == 3840);
    CHECK(tempos.back().microseconds_per_beat == 1000000);
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics.front().message.find("120 SMF Set Tempo steps") !=
          std::string::npos);
}

TEST_CASE("compile_to_midi: unrepresentable 24-bit tempo is rejected",
          "[score][midi][tempo-transition]") {
    auto score = make_compilable_score(1);
    score.tempo_map.front().bpm = make_bpm(1);

    const auto result = compile_to_midi(score, 480);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidMidiTempo);
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
    CHECK(midi.time_signatures[0].denominator == 4);
    CHECK(midi.time_signatures[0].clocks_per_metronome_click == 24);
    CHECK(midi.time_signatures[0].notated_32nds_per_quarter == 8);

    CHECK(midi.time_signatures[1].tick == 3840);
    CHECK(midi.time_signatures[1].numerator == 3);
    CHECK(midi.time_signatures[1].denominator == 4);
    CHECK(midi.time_signatures[1].clocks_per_metronome_click == 24);
    CHECK(midi.time_signatures[1].notated_32nds_per_quarter == 8);
    CHECK(result->report.time_signature_events_requested == 2);
    CHECK(result->report.time_signature_events_written == 2);
    CHECK(result->report.time_signature_groupings_requested == 0);
    CHECK(result->report.time_signature_groupings_written == 0);
}

TEST_CASE("SMF metre projection carries clicks without conflating ordered grouping",
          "[score][midi][meter][grouping]") {
    SECTION(
        "canonical compound metre has a dotted-quarter click without explicit grouping intent") {
        auto score = make_compilable_score(1);
        const auto compound = make_time_signature(6, 8);
        REQUIRE(compound.has_value());
        score.time_map.front().time_signature = *compound;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            compound->measure_duration();

        const auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.time_signatures.size() == 1);
        CHECK(result->midi.time_signatures[0].clocks_per_metronome_click == 36);
        CHECK(result->midi.time_signatures[0].notated_32nds_per_quarter == 8);
        CHECK(result->report.time_signature_groupings_requested == 0);
        CHECK(result->report.time_signature_groupings_written == 0);
        CHECK_FALSE(result->report.has_drops());
        CHECK(result->report.diagnostics.empty());
    }

    SECTION("a matching repeated click still is not an SMF grouping field") {
        auto score = make_compilable_score(1);
        const auto grouped = TimeSignature::from_groups({2, 2}, 4);
        REQUIRE(grouped.has_value());
        score.time_map.front().time_signature = *grouped;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            grouped->measure_duration();

        const auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.time_signatures.size() == 1);
        CHECK(result->midi.time_signatures[0].clocks_per_metronome_click == 48);
        CHECK(result->report.time_signature_groupings_requested == 1);
        CHECK(result->report.time_signature_groupings_written == 0);
        CHECK(result->report.has_drops());
        REQUIRE(result->report.diagnostics.size() == 1);
        CHECK(result->report.diagnostics[0].message.find("matching repeated group-span click") !=
              std::string::npos);
    }

    SECTION("additive grouping") {
        auto score = make_compilable_score(1);
        const auto additive = make_additive_time_signature({3, 2}, 8);
        REQUIRE(additive.has_value());
        score.time_map.front().time_signature = *additive;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            additive->measure_duration();

        const auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.time_signatures.size() == 1);
        CHECK(result->midi.time_signatures[0].numerator == 5);
        CHECK(result->midi.time_signatures[0].denominator == 8);
        CHECK(result->midi.time_signatures[0].clocks_per_metronome_click == 12);
        CHECK(result->report.time_signature_events_requested == 1);
        CHECK(result->report.time_signature_events_written == 1);
        CHECK(result->report.time_signature_groupings_requested == 1);
        CHECK(result->report.time_signature_groupings_written == 0);
        CHECK(result->report.dropped_time_sig_events == 0);
        CHECK(result->report.has_drops());
        REQUIRE(result->report.diagnostics.size() == 1);
        CHECK(result->report.diagnostics[0].message.find("metronome-click interval") !=
              std::string::npos);
    }

    SECTION("sub-clock grouping falls back without claiming the partition") {
        auto score = make_compilable_score(1);
        const auto sub_clock = TimeSignature::from_groups({1}, 128);
        REQUIRE(sub_clock.has_value());
        score.time_map.front().time_signature = *sub_clock;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            sub_clock->measure_duration();

        const auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.time_signatures.size() == 1);
        CHECK(result->midi.time_signatures[0].clocks_per_metronome_click == 24);
        CHECK(result->report.time_signature_events_written == 1);
        CHECK(result->report.time_signature_groupings_requested == 0);
        CHECK(result->report.time_signature_groupings_written == 0);
        CHECK_FALSE(result->report.has_drops());
        CHECK(result->report.diagnostics.empty());
    }

    SECTION("overwide click interval falls back without claiming the partition") {
        auto score = make_compilable_score(1);
        const auto overwide_click = TimeSignature::from_groups({3}, 1);
        REQUIRE(overwide_click.has_value());
        score.time_map.front().time_signature = *overwide_click;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            overwide_click->measure_duration();

        const auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.time_signatures.size() == 1);
        CHECK(result->midi.time_signatures[0].clocks_per_metronome_click == 24);
        CHECK(result->report.time_signature_events_written == 1);
        CHECK(result->report.time_signature_groupings_requested == 1);
        CHECK(result->report.time_signature_groupings_written == 0);
        CHECK(result->report.has_drops());
        REQUIRE(result->report.diagnostics.size() == 1);
        CHECK(result->report.diagnostics[0].message.find("canonical 24-clock") !=
              std::string::npos);
    }
}

TEST_CASE("SMF metre projection rejects source widths before integer narrowing",
          "[score][midi][meter][trust-boundary]") {
    auto score = make_compilable_score(1);

    SECTION("numerator") {
        const auto signature = make_time_signature(256, 4);
        REQUIRE(signature.has_value());
        score.time_map.front().time_signature = *signature;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            signature->measure_duration();
    }

    SECTION("denominator") {
        const auto signature = make_time_signature(4, 256);
        REQUIRE(signature.has_value());
        score.time_map.front().time_signature = *signature;
        std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
            signature->measure_duration();
    }

    const auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    CHECK(result->midi.time_signatures.empty());
    CHECK(result->report.time_signature_events_requested == 1);
    CHECK(result->report.time_signature_events_written == 0);
    CHECK(result->report.time_signature_groupings_requested == 0);
    CHECK(result->report.time_signature_groupings_written == 0);
    CHECK(result->report.dropped_time_sig_events == 1);
    CHECK(result->report.has_drops());
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics[0].message.find("1..255") != std::string::npos);
}

TEST_CASE("performance projections reject unequal Part-local bar durations",
          "[score][midi][local-meter]") {
    auto score = make_compilable_score(1);
    const auto local = make_time_signature(3, 4);
    REQUIRE(local.has_value());
    score.parts[0].measures[0].local_time = *local;
    std::get<RestEvent>(score.parts[0].measures[0].voices[0].events[0].payload).duration =
        Beat{3, 4};

    const auto midi = compile_to_midi(score, 480);
    REQUIRE_FALSE(midi.has_value());
    CHECK(midi.error() == ErrorCode::TargetValueUnrepresentable);

    const auto note_events = compile_to_note_events(score);
    REQUIRE_FALSE(note_events.has_value());
    CHECK(note_events.error() == ErrorCode::TargetValueUnrepresentable);
}

TEST_CASE("equal-duration Part-local meter grouping remains an explicit performance residual",
          "[score][midi][local-meter]") {
    auto score = make_compilable_score(1);
    const auto local = make_time_signature(2, 2);
    REQUIRE(local.has_value());
    score.parts[0].measures[0].local_time = *local;

    const auto midi = compile_to_midi(score, 480);
    REQUIRE(midi.has_value());
    CHECK(midi->report.dropped_time_sig_events == 1);
    CHECK(midi->report.has_drops());
    REQUIRE_FALSE(midi->report.diagnostics.empty());
    CHECK(midi->report.diagnostics.front().message.find("one global meter timeline") !=
          std::string::npos);

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events.has_value());
    CHECK(note_events->report.dropped_time_sig_events == 1);
    CHECK(note_events->report.has_drops());
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
        if (n.channel == 1) {
            found_ch1 = true;
            CHECK(n.note == 60);
            CHECK(n.part_id == PartId{100});
        }
        if (n.channel == 2) {
            found_ch2 = true;
            CHECK(n.note == 64);
            CHECK(n.part_id == PartId{200});
        }
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

TEST_CASE("compile_to_midi: explicit numeric and written velocities have defined precedence",
          "[score][midi][velocity]") {
    SECTION("numeric velocity is used when no semantic dynamic is present") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity.value = 73;

        auto result = compile_to_midi(score, 480);
        REQUIRE(result);
        CHECK(result->midi.notes[0].velocity == 73);
    }

    SECTION("written velocity is semantic and takes precedence over its numeric cache") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity.value = 120;
        note.velocity.written = DynamicLevel::p;

        auto result = compile_to_midi(score, 480);
        REQUIRE(result);
        CHECK(result->midi.notes[0].velocity == default_velocity(DynamicLevel::p));
    }
}

TEST_CASE("compile_to_midi: combined articulation mapping emits and transforms deterministically",
          "[score][midi][articulation-mapping]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Staccato);

    ArticulationMapping keyswitch;
    keyswitch.type = ArticulationMapping::Type::Keyswitch;
    keyswitch.keyswitch_pitch = SpelledPitch{0, 0, 1};
    ArticulationMapping cc;
    cc.type = ArticulationMapping::Type::CC;
    cc.cc_number = 64;
    cc.cc_value = 127;
    ArticulationMapping program;
    program.type = ArticulationMapping::Type::ProgramChange;
    program.program = 41;
    ArticulationMapping velocity;
    velocity.type = ArticulationMapping::Type::VelocityLayer;
    velocity.velocity_min = 20;
    velocity.velocity_max = 100;
    ArticulationMapping duration;
    duration.type = ArticulationMapping::Type::NoteDurationScale;
    duration.duration_scale = 0.25F;
    ArticulationMapping combined;
    combined.type = ArticulationMapping::Type::Combined;
    combined.combined = {keyswitch, cc, program, velocity, duration};
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = combined;

    auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    REQUIRE(result->midi.notes.size() == 1);
    CHECK(result->midi.notes[0].velocity == 75);
    CHECK(result->midi.notes[0].duration_ticks == 480);
    REQUIRE(result->midi.keyswitches.size() == 1);
    CHECK(result->midi.keyswitches[0].note == 24);
    CHECK(result->midi.keyswitches[0].velocity == 127);
    REQUIRE(result->midi.control_changes.size() == 1);
    CHECK(result->midi.control_changes[0].controller == 64);
    CHECK(result->midi.control_changes[0].value == 127);
    REQUIRE(result->midi.program_changes.size() == 1);
    CHECK(result->midi.program_changes[0].program == 41);
    CHECK(result->report.articulation_mappings_applied == 1);
    CHECK(result->report.articulations_defaulted == 0);

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events);
    REQUIRE(note_events->events.size() == 1);
    CHECK(note_events->events[0].velocity == 75);
    CHECK(note_events->events[0].duration == Beat{1, 4});
    CHECK(note_events->report.articulation_mappings_applied == 1);
    CHECK(note_events->report.articulations_defaulted == 0);
    REQUIRE(note_events->report.diagnostics.size() == 1);
    CHECK(note_events->report.diagnostics[0].message.find(
              "omitted 3 MIDI articulation control event(s)") != std::string::npos);
}

TEST_CASE("compile_to_midi: a custom mapping replaces rather than compounds the default",
          "[score][midi][articulation-mapping]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Staccato);
    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::NoteDurationScale;
    mapping.duration_scale = 0.75F;
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = mapping;

    auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    CHECK(result->midi.notes[0].duration_ticks == 1440);
    CHECK(result->report.articulation_mappings_applied == 1);

    const auto note_events = compile_to_note_events(score);
    REQUIRE(note_events);
    REQUIRE(note_events->events.size() == 1);
    CHECK(note_events->events[0].duration == Beat{3, 4});
    CHECK(note_events->report.articulation_mappings_applied == 1);
    CHECK(note_events->report.diagnostics.empty());
}

TEST_CASE("NoteEvent articulation duration requires exact Beat representation",
          "[score][note-events][articulation-mapping][target-boundary]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Staccato);
    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::NoteDurationScale;
    mapping.duration_scale = std::numeric_limits<float>::denorm_min();
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = mapping;

    const auto midi = compile_to_midi(score, 480);
    REQUIRE(midi);
    REQUIRE(midi->midi.notes.size() == 1);
    CHECK(midi->midi.notes[0].duration_ticks == 1);

    const auto note_events = compile_to_note_events(score);
    REQUIRE_FALSE(note_events);
    CHECK(note_events.error() == ErrorCode::TargetValueUnrepresentable);
}

TEST_CASE("compile_to_midi: identical chord articulation controls are deduplicated",
          "[score][midi][articulation-mapping]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, std::nullopt, ArticulationType::Tenuto);
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);
    Note second = group.notes[0];
    second.pitch = E4;
    group.notes.push_back(second);

    ArticulationMapping mapping;
    mapping.type = ArticulationMapping::Type::CC;
    mapping.cc_number = 32;
    mapping.cc_value = 9;
    score.parts[0].definition.rendering.articulation_map[ArticulationType::Tenuto] = mapping;

    auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    CHECK(result->midi.notes.size() == 2);
    CHECK(result->midi.control_changes.size() == 1);
    CHECK(result->report.articulation_mappings_applied == 2);
}

TEST_CASE("compile_to_midi: invalid rendering configuration is rejected",
          "[score][midi][articulation-mapping][trust-boundary]") {
    auto score = make_compilable_score(1);
    score.parts[0].definition.rendering.midi_channel = 0;
    auto result = compile_to_midi(score, 480);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::InvalidRenderingConfig);
}

TEST_CASE("compile_to_midi: uncompilable score returns error", "[score][midi]") {
    Score bad;
    bad.id = ScoreId{1};
    bad.metadata.total_bars = 1;

    auto result = compile_to_midi(bad);
    CHECK(!result.has_value());
}

TEST_CASE("compile_to_midi: invalid PPQ is rejected before narrowing", "[score][midi]") {
    auto score = make_compilable_score(1);

    for (const int ppq : {0, -1, 65536}) {
        auto result = compile_to_midi(score, ppq);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::InvalidMidiPPQ);
    }
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

TEST_CASE("Part-local key metadata remains an explicit SMF residual",
          "[score][midi][key][local-key]") {
    auto score = make_compilable_score(1);
    const auto minor = find_scale("minor");
    REQUIRE(minor.has_value());
    score.parts[0].measures[0].local_key = KeySignature{SpelledPitch{0, 0, 4}, *minor, -3};

    const auto result = compile_to_midi(score, 480);

    REQUIRE(result.has_value());
    REQUIRE(result->midi.key_signatures.size() == 1);
    CHECK(result->report.dropped_key_sig_events == 1);
    CHECK(result->report.has_drops());
    REQUIRE_FALSE(result->report.diagnostics.empty());
    CHECK(result->report.diagnostics.front().message.find("Part-local key override") !=
          std::string::npos);
}

TEST_CASE("compile_to_midi: non-major/minor scale mode is explicit degradation evidence",
          "[score][midi][key]") {
    auto score = make_compilable_score(1);
    auto dorian = find_scale("dorian");
    REQUIRE(dorian.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{1, 0, 4}, *dorian, 0};

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.key_signatures.size() == 1);
    CHECK(result->midi.key_signatures[0].accidentals == 0);
    CHECK(result->midi.key_signatures[0].minor);
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics[0].message.find("major/minor mode") != std::string::npos);
    CHECK(result->report.diagnostics[0].message.find("dorian") != std::string::npos);
    CHECK_FALSE(result->report.has_drops());
    CHECK(result->report.has_residuals());
}

TEST_CASE("compile_to_midi: key metadata outside the SMF fifths domain is an explicit drop",
          "[score][midi][key][report]") {
    auto score = make_compilable_score(1);
    const auto major = find_scale("major");
    REQUIRE(major.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{0, 2, 4}, *major, 14};

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    CHECK(result->midi.key_signatures.empty());
    CHECK(result->report.dropped_key_sig_events == 1);
    CHECK(result->report.has_drops());
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics[0].message.find("seven flats through seven sharps") !=
          std::string::npos);
    CHECK(result->report.diagnostics[0].message.find("14 fifths") != std::string::npos);
}

TEST_CASE("compile_to_midi: acciaccatura is short and right-aligned in its allocation",
          "[score][midi][grace]") {
    auto score = make_compilable_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    Note grace;
    grace.pitch = C4;
    grace.grace = GraceType::Acciaccatura;
    NoteGroup grace_group{{grace}, Beat{1, 16}};

    Note main;
    main.pitch = E4;
    NoteGroup main_group{{main}, Beat{15, 16}};
    events.push_back(Event{EventId{9001}, Beat::zero(), grace_group});
    events.push_back(Event{EventId{9002}, Beat{1, 16}, main_group});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    REQUIRE(result->midi.notes.size() == 2);
    CHECK(result->midi.notes[0].note == 60);
    CHECK(result->midi.notes[0].tick == 60);
    CHECK(result->midi.notes[0].duration_ticks == 60);
    CHECK(result->midi.notes[1].note == 64);
    CHECK(result->midi.notes[1].tick == 120);
    CHECK(result->midi.notes[1].duration_ticks == 1800);
}

TEST_CASE("compile_to_midi: appoggiatura occupies its full structural allocation",
          "[score][midi][grace]") {
    auto score = make_compilable_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    Note grace;
    grace.pitch = C4;
    grace.grace = GraceType::Appoggiatura;
    NoteGroup grace_group{{grace}, Beat{1, 8}};

    Note main;
    main.pitch = E4;
    NoteGroup main_group{{main}, Beat{7, 8}};
    events.push_back(Event{EventId{9011}, Beat::zero(), grace_group});
    events.push_back(Event{EventId{9012}, Beat{1, 8}, main_group});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result);
    REQUIRE(result->midi.notes.size() == 2);
    CHECK(result->midi.notes[0].note == 60);
    CHECK(result->midi.notes[0].tick == 0);
    CHECK(result->midi.notes[0].duration_ticks == 240);
    CHECK(result->midi.notes[1].tick == 240);
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
    CHECK(result->events[0].velocity == default_velocity(DynamicLevel::mf));
}

TEST_CASE("compile_to_note_events: explicit numeric velocity is resolved",
          "[score][note-events][velocity]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
        .notes[0]
        .velocity.value = 73;

    auto result = compile_to_note_events(score);
    REQUIRE(result);
    REQUIRE(result->events.size() == 1);
    CHECK(result->events[0].velocity == 73);
}

TEST_CASE("compile_to_note_events: default articulation shapes exact velocity and duration",
          "[score][note-events][articulation]") {
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Marcato);

    const auto result = compile_to_note_events(score);
    REQUIRE(result);
    REQUIRE(result->events.size() == 1);
    CHECK(result->events[0].velocity == 118);
    CHECK(result->events[0].duration == Beat{17, 20});
    CHECK(result->report.articulations_defaulted == 1);
    CHECK(result->report.articulation_mappings_applied == 0);
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

TEST_CASE("compile_to_note_events: rest-only bars produce zero events", "[score][note-events]") {
    auto score = make_compilable_score(4);
    // All bars are rests by default

    auto result = compile_to_note_events(score);
    REQUIRE(result.has_value());
    CHECK(result->events.empty());
}

TEST_CASE("compile_to_note_events: grace timing matches MIDI compilation",
          "[score][note-events][grace]") {
    auto score = make_compilable_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    Note grace;
    grace.pitch = C4;
    grace.grace = GraceType::Acciaccatura;
    events.push_back(Event{EventId{9021}, Beat::zero(), NoteGroup{{grace}, Beat{1, 16}}});
    events.push_back(Event{EventId{9022}, Beat{1, 16}, RestEvent{Beat{15, 16}, true}});

    auto result = compile_to_note_events(score);
    REQUIRE(result);
    REQUIRE(result->events.size() == 1);
    CHECK(result->events[0].start_time == Beat{1, 32});
    CHECK(result->events[0].duration == Beat{1, 32});
    CHECK_FALSE(result->events[0].muted);
}

// =============================================================================
// Articulation velocity offset assertions
// =============================================================================

TEST_CASE("articulation velocity offsets are exact", "[score][midi]") {

    SECTION("Accent adds exactly 20 from mf base") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Accent);

        auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.notes.size() == 1);
        CHECK(result->midi.notes[0].velocity == 108);
    }

    SECTION("Marcato adds exactly 30 from mf base") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4, DynamicLevel::mf, ArticulationType::Marcato);

        auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.notes.size() == 1);
        CHECK(result->midi.notes[0].velocity == 118);
    }

    SECTION("Sforzando saturates at 127 from fff") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4, DynamicLevel::fff, ArticulationType::Sforzando);

        auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.notes.size() == 1);
        CHECK(result->midi.notes[0].velocity == 127);
    }

    SECTION("Sforzando adds exactly 40 from pp") {
        auto score = make_compilable_score(1);
        place_whole_note(score, 0, 0, C4, DynamicLevel::pp, ArticulationType::Sforzando);

        auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        REQUIRE(result->midi.notes.size() == 1);
        CHECK(result->midi.notes[0].velocity == 80);
    }
}

// =============================================================================
// Hairpin interpolation — indirect test of hairpin_ratio (L87–91)
// =============================================================================

TEST_CASE("hairpin interpolates velocity between dynamics", "[score][midi]") {
    // 4-bar score; crescendo from bar 1 to bar 3, target ff.
    // Default dynamic is mf (88). Hairpin target ff (116).
    // A note at bar 2 (midpoint) has no explicit dynamic, so the
    // pipeline resolves: start_vel = 88, target_vel = 116, ratio = 0.5,
    // velocity = 88 + 0.5 * (116 - 88) = 102.
    auto score = make_compilable_score(4);

    // Place a note only in bar 2 (bar_idx 1) with no explicit dynamic.
    place_whole_note(score, 0, 1, C4);

    Hairpin cresc;
    cresc.start = ScoreTime{1, Beat::zero()};
    cresc.end = ScoreTime{3, Beat::zero()};
    cresc.type = HairpinType::Crescendo;
    cresc.target = DynamicLevel::ff;
    score.parts[0].hairpins.push_back(cresc);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());

    const auto& midi = result->midi;
    REQUIRE(midi.notes.size() == 1);
    CHECK(midi.notes[0].velocity == 102);
}

TEST_CASE("targetless hairpin moves by one semantic dynamic step", "[score][midi][hairpin]") {
    auto score = make_compilable_score(2);
    place_whole_note(score, 0, 1, C4);
    score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat::zero()},
                                              ScoreTime{3, Beat::zero()},
                                              HairpinType::Crescendo,
                                              std::nullopt});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.notes.size() == 1);
    // mf (88) to f (104), sampled halfway.
    CHECK(result->midi.notes[0].velocity == 96);
}

TEST_CASE("hairpin interpolation uses cumulative meter duration", "[score][midi][hairpin][meter]") {
    auto score = make_compilable_score(2);
    const auto three_four = make_time_signature(3, 4);
    const auto four_four = make_time_signature(4, 4);
    REQUIRE(three_four.has_value());
    REQUIRE(four_four.has_value());
    score.time_map[0].time_signature = *three_four;
    score.time_map.push_back(TimeSignatureEntry{2, *four_four});
    score.parts[0].measures[0].voices[0].events[0].payload = RestEvent{Beat{3, 4}, true};
    place_whole_note(score, 0, 1, C4);
    score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat::zero()},
                                              ScoreTime{3, Beat::zero()},
                                              HairpinType::Crescendo,
                                              DynamicLevel::ff});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.notes.size() == 1);
    // Bar 2 begins 3/4 through a 7/4 span: 88 + (116 - 88) * 3/7 = 100.
    CHECK(result->midi.notes[0].velocity == 100);
}

TEST_CASE("standard pedal PartDirectives compile to exact MIDI CC ranges",
          "[score][midi][directive][pedal]") {
    auto score = make_compilable_score(1);
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::SustainingPedal, 0});
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 2}}, ScoreTime{2, Beat::zero()}, DirectiveType::UnaCorda, 0});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.control_changes.size() == 4);
    CHECK(result->midi.control_changes[0].tick == 480);
    CHECK(result->midi.control_changes[0].controller == 64);
    CHECK(result->midi.control_changes[0].value == 127);
    CHECK(result->midi.control_changes[1].tick == 960);
    CHECK(result->midi.control_changes[1].controller == 67);
    CHECK(result->midi.control_changes[1].value == 127);
    CHECK(result->midi.control_changes[2].tick == 1440);
    CHECK(result->midi.control_changes[2].controller == 64);
    CHECK(result->midi.control_changes[2].value == 0);
    CHECK(result->midi.control_changes[3].tick == 1920);
    CHECK(result->midi.control_changes[3].controller == 67);
    CHECK(result->midi.control_changes[3].value == 0);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("pedal Directions and an equivalent PartDirective share one MIDI CC64 range",
          "[score][midi][direction][pedal]") {
    auto score = make_compilable_score(1);
    ScoreDirection down;
    down.type = DirectionType::PedalDown;
    ScoreDirection up;
    up.type = DirectionType::PedalUp;
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.push_back(Event{EventId{8501}, Beat{1, 4}, down});
    events.push_back(Event{EventId{8502}, Beat{3, 4}, up});
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::SustainingPedal, 0});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.control_changes.size() == 2);
    CHECK(result->midi.control_changes[0].tick == 480);
    CHECK(result->midi.control_changes[0].controller == 64);
    CHECK(result->midi.control_changes[0].value == 127);
    CHECK(result->midi.control_changes[1].tick == 1440);
    CHECK(result->midi.control_changes[1].controller == 64);
    CHECK(result->midi.control_changes[1].value == 0);
}

TEST_CASE("terminal PedalDown is safely released at the MIDI score endpoint",
          "[score][midi][direction][pedal]") {
    auto score = make_compilable_score(1);
    ScoreDirection down;
    down.type = DirectionType::PedalDown;
    score.parts[0].measures[0].voices[0].events.push_back(Event{EventId{8503}, Beat{1, 2}, down});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.control_changes.size() == 2);
    CHECK(result->midi.control_changes[0].tick == 960);
    CHECK(result->midi.control_changes[0].value == 127);
    CHECK(result->midi.control_changes[1].tick == 1920);
    CHECK(result->midi.control_changes[1].value == 0);
}

TEST_CASE("unmapped PartDirective remains explicit MIDI compilation evidence",
          "[score][midi][directive]") {
    auto score = make_compilable_score(1);
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat::zero()}, ScoreTime{2, Beat::zero()}, DirectiveType::Pizzicato, 0});

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics[0].message.find("pizzicato") != std::string::npos);
    CHECK(result->midi.control_changes.empty());
}

// =============================================================================
// Regression: tick rounding and same-key unisons (issue #9)
// =============================================================================

TEST_CASE("compile_to_midi: every 1/(4k) tuplet note ends exactly where the next begins",
          "[score][midi][regression]") {
    // SMF 1.0 pairs a Note Off with an earlier Note On of the same key, so a
    // repeated key must end on the tick of the next attack. At PPQ 480 a 4/4
    // bar spans 1920 ticks; note i of a 4k-fold division starts at
    // round(1920 i / 4k) = round(480 i / k) and ends where note i + 1 starts.
    for (int k = 1; k <= 13; ++k) {
        CAPTURE(k);
        auto score = make_compilable_score(1);
        auto& voice = score.parts[0].measures[0].voices[0];
        voice.events.clear();
        const int count = 4 * k;
        for (int i = 0; i < count; ++i) {
            Note note;
            note.pitch = C4;
            NoteGroup group;
            group.notes.push_back(note);
            group.duration = Beat{1, count};
            voice.events.push_back(
                Event{EventId{static_cast<std::uint64_t>(10'000 + i)}, Beat{i, count}, group});
        }

        auto result = compile_to_midi(score, 480);
        REQUIRE(result.has_value());
        const auto& notes = result->midi.notes;
        REQUIRE(notes.size() == static_cast<std::size_t>(count));
        for (std::size_t i = 0; i < notes.size(); ++i) {
            CAPTURE(i);
            const auto index = static_cast<double>(i);
            CHECK(notes[i].tick == std::llround(480.0 * index / k));
            CHECK(notes[i].tick + notes[i].duration_ticks ==
                  std::llround(480.0 * (index + 1.0) / k));
        }
    }
}

TEST_CASE("compile_to_midi: a nested same-key unison on one channel sounds as one note",
          "[score][midi][regression]") {
    // Part A holds a whole-note C4; part B plays a quarter-note C4 at the same
    // onset on the same channel. SMF cannot pair the inner Note Off with the
    // inner Note On, so the unison is emitted once and reported.
    auto score = make_compilable_score(1);
    place_whole_note(score, 0, 0, C4);

    Part second = score.parts[0];
    second.id = PartId{200};
    second.definition.name = "Second";
    auto& events = second.measures[0].voices[0].events;
    Note note;
    note.pitch = C4;
    NoteGroup quarter;
    quarter.notes.push_back(note);
    quarter.duration = Beat{1, 4};
    events = {Event{EventId{7001}, Beat::zero(), quarter},
              Event{EventId{7002}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}}};
    score.parts.push_back(second);

    auto result = compile_to_midi(score, 480);
    REQUIRE(result.has_value());
    REQUIRE(result->midi.notes.size() == 1);
    CHECK(result->midi.notes[0].tick == 0);
    CHECK(result->midi.notes[0].duration_ticks == 1920);
    CHECK(result->midi.notes[0].note == 60);
    CHECK(std::ranges::any_of(result->report.diagnostics, [](const CompilationDiagnostic& d) {
        return d.message.find("unison") != std::string::npos && d.part == PartId{200};
    }));
}
