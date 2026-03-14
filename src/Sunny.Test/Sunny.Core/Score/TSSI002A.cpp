/**
 * @file TSSI002A.cpp
 * @brief Unit tests for Score IR document hierarchy (SIDC001A) and
 *        temporal system (SITM001A)
 *
 * Component: TSSI002A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIDC001A, SITM001A
 * Coverage: Score construction, instrument_family, Event variant,
 *           ScoreTime↔AbsoluteBeat↔RealTime↔TickTime conversions,
 *           beat_unit_duration, effective_quarter_bpm
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Score/SITM001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers: build a minimal valid Score
// =============================================================================

namespace {

/// Create a minimal Score: 4 bars, 4/4, 120 BPM, C major, one piano part
Score make_test_score(std::uint32_t total_bars = 4) {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Test Score";
    score.metadata.total_bars = total_bars;

    // Tempo: 120 BPM at quarter = bar 1
    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    // Key: C major at bar 1
    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};  // C4
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    // Time: 4/4 at bar 1
    auto ts = make_time_signature(4, 4);
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    // One piano part with empty measures (whole-note rests)
    Part piano;
    piano.id = PartId{100};
    piano.definition.name = "Piano";
    piano.definition.abbreviation = "Pno.";
    piano.definition.instrument_type = InstrumentType::Piano;
    piano.definition.clef = Clef::Treble;
    piano.definition.range = PitchRange{
        SpelledPitch{5, 0, 0},   // A0 absolute low
        SpelledPitch{0, 0, 8},   // C8 absolute high
        SpelledPitch{0, 0, 3},   // C3 comfortable low
        SpelledPitch{0, 0, 6}    // C6 comfortable high
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
// SIDC001A: instrument_family
// =============================================================================

TEST_CASE("SIDC001A: instrument_family classification", "[score-ir][document]") {
    CHECK(instrument_family(InstrumentType::Violin) == InstrumentFamily::Strings);
    CHECK(instrument_family(InstrumentType::Guitar) == InstrumentFamily::Strings);
    CHECK(instrument_family(InstrumentType::Flute) == InstrumentFamily::Woodwinds);
    CHECK(instrument_family(InstrumentType::AltoSax) == InstrumentFamily::Woodwinds);
    CHECK(instrument_family(InstrumentType::FrenchHorn) == InstrumentFamily::Brass);
    CHECK(instrument_family(InstrumentType::Tuba) == InstrumentFamily::Brass);
    CHECK(instrument_family(InstrumentType::Timpani) == InstrumentFamily::Percussion);
    CHECK(instrument_family(InstrumentType::SnareDrum) == InstrumentFamily::Percussion);
    CHECK(instrument_family(InstrumentType::Piano) == InstrumentFamily::Keyboard);
    CHECK(instrument_family(InstrumentType::Organ) == InstrumentFamily::Keyboard);
    CHECK(instrument_family(InstrumentType::SopranoVoice) == InstrumentFamily::Voice);
    CHECK(instrument_family(InstrumentType::BassVoice) == InstrumentFamily::Voice);
    CHECK(instrument_family(InstrumentType::Synthesiser) == InstrumentFamily::Electronic);
    CHECK(instrument_family(InstrumentType::Custom) == InstrumentFamily::Electronic);
}

// =============================================================================
// SIDC001A: Event variant accessors
// =============================================================================

TEST_CASE("SIDC001A: Event variant type checking", "[score-ir][document]") {
    SECTION("NoteGroup event") {
        NoteGroup ng;
        ng.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
        ng.duration = Beat{1, 4};

        Event event{EventId{1}, Beat::zero(), ng};
        CHECK(event.is_note_group());
        CHECK_FALSE(event.is_rest());
        CHECK_FALSE(event.is_direction());
        CHECK(event.duration() == Beat{1, 4});
        CHECK(event.as_note_group() != nullptr);
    }

    SECTION("Rest event") {
        RestEvent rest{Beat{1, 1}, true};
        Event event{EventId{2}, Beat::zero(), rest};
        CHECK(event.is_rest());
        CHECK_FALSE(event.is_note_group());
        CHECK(event.duration() == Beat{1, 1});
    }

    SECTION("Direction event has zero duration") {
        ScoreDirection dir{DirectionType::Text, "Allegro", std::nullopt, 0};
        Event event{EventId{3}, Beat::zero(), dir};
        CHECK(event.is_direction());
        CHECK(event.duration() == Beat::zero());
    }
}

// =============================================================================
// SIDC001A: Score construction
// =============================================================================

TEST_CASE("SIDC001A: minimal Score construction", "[score-ir][document]") {
    auto score = make_test_score();

    CHECK(score.parts.size() == 1);
    CHECK(score.metadata.total_bars == 4);
    CHECK(score.metadata.title == "Test Score");
    CHECK(score.tempo_map.size() == 1);
    CHECK(score.key_map.size() == 1);
    CHECK(score.time_map.size() == 1);
    CHECK(score.parts[0].measures.size() == 4);
    CHECK(score.parts[0].measures[0].voices.size() == 1);
    CHECK(score.version == 1);
}

// =============================================================================
// SITM001A: score_time_to_absolute_beat
// =============================================================================

TEST_CASE("SITM001A: ScoreTime to AbsoluteBeat in 4/4", "[score-ir][temporal]") {
    auto score = make_test_score();

    SECTION("Bar 1, beat 0 = 0") {
        auto result = score_time_to_absolute_beat(
            SCORE_START, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Beat::zero());
    }

    SECTION("Bar 1, beat 1/4 = 1/4") {
        auto result = score_time_to_absolute_beat(
            ScoreTime{1, Beat{1, 4}}, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Beat{1, 4});
    }

    SECTION("Bar 2, beat 0 = 1 whole note (4/4 measure)") {
        auto result = score_time_to_absolute_beat(
            ScoreTime{2, Beat::zero()}, score.time_map
        );
        REQUIRE(result.has_value());
        // 4/4 measure = Beat{4,4} = Beat{1,1}
        CHECK(*result == Beat{1, 1});
    }

    SECTION("Bar 3, beat 1/2 = 2 + 1/2 whole notes") {
        auto result = score_time_to_absolute_beat(
            ScoreTime{3, Beat{1, 2}}, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Beat{5, 2});
    }
}

// =============================================================================
// SITM001A: absolute_beat_to_score_time (inverse)
// =============================================================================

TEST_CASE("SITM001A: AbsoluteBeat to ScoreTime round-trip", "[score-ir][temporal]") {
    auto score = make_test_score();

    std::vector<ScoreTime> test_times = {
        {1, Beat::zero()},
        {1, Beat{1, 4}},
        {2, Beat::zero()},
        {3, Beat{1, 2}},
        {4, Beat{3, 4}},
    };

    for (const auto& time : test_times) {
        auto abs = score_time_to_absolute_beat(time, score.time_map);
        REQUIRE(abs.has_value());

        auto back = absolute_beat_to_score_time(
            *abs, score.time_map, score.metadata.total_bars
        );
        REQUIRE(back.has_value());
        CHECK(back->bar == time.bar);
        CHECK(back->beat == time.beat);
    }
}

// =============================================================================
// SITM001A: absolute_beat_to_tick
// =============================================================================

TEST_CASE("SITM001A: AbsoluteBeat to TickTime", "[score-ir][temporal]") {
    SECTION("Zero = 0 ticks") {
        CHECK(absolute_beat_to_tick(Beat::zero()) == 0);
    }

    SECTION("Quarter note = 480 ticks at 480 PPQ") {
        // Beat{1,4} = quarter note
        // tick = Beat{1,4} * 4 * 480 = 480
        CHECK(absolute_beat_to_tick(Beat{1, 4}) == 480);
    }

    SECTION("Whole note = 1920 ticks at 480 PPQ") {
        CHECK(absolute_beat_to_tick(Beat{1, 1}) == 1920);
    }

    SECTION("Eighth note = 240 ticks") {
        CHECK(absolute_beat_to_tick(Beat{1, 8}) == 240);
    }

    SECTION("Sixteenth note = 120 ticks") {
        CHECK(absolute_beat_to_tick(Beat{1, 16}) == 120);
    }

    SECTION("Triplet eighth = 160 ticks") {
        // 1/12 of a whole note → 4*480/12 = 160
        CHECK(absolute_beat_to_tick(Beat{1, 12}) == 160);
    }
}

// =============================================================================
// SITM001A: tick_to_absolute_beat (inverse)
// =============================================================================

TEST_CASE("SITM001A: TickTime to AbsoluteBeat round-trip", "[score-ir][temporal]") {
    std::vector<Beat> test_beats = {
        Beat::zero(),
        Beat{1, 4},
        Beat{1, 2},
        Beat{1, 1},
        Beat{3, 8},
    };

    for (const auto& beat : test_beats) {
        std::int64_t tick = absolute_beat_to_tick(beat);
        Beat back = tick_to_absolute_beat(tick);
        // Should round-trip exactly for common beat values
        CHECK(back == beat);
    }
}

// =============================================================================
// SITM001A: RealTime at constant tempo
// =============================================================================

TEST_CASE("SITM001A: AbsoluteBeat to RealTime at 120 BPM", "[score-ir][temporal]") {
    auto score = make_test_score();

    SECTION("Beat 0 = 0 seconds") {
        auto result = absolute_beat_to_real_time(
            Beat::zero(), score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(0.0));
    }

    SECTION("Quarter note at 120 BPM = 0.5 seconds") {
        // 120 BPM quarter = 120 quarter notes per minute = 0.5s per quarter
        auto result = absolute_beat_to_real_time(
            Beat{1, 4}, score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(0.5));
    }

    SECTION("Whole note at 120 BPM = 2 seconds") {
        auto result = absolute_beat_to_real_time(
            Beat{1, 1}, score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(2.0));
    }

    SECTION("4 bars of 4/4 at 120 BPM = 8 seconds") {
        // 4 whole notes = 4 * 2 = 8 seconds
        auto result = absolute_beat_to_real_time(
            Beat{4, 1}, score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(8.0));
    }
}

// =============================================================================
// SITM001A: score_time_to_real_time (composed)
// =============================================================================

TEST_CASE("SITM001A: ScoreTime to RealTime", "[score-ir][temporal]") {
    auto score = make_test_score();

    SECTION("Start = 0 seconds") {
        auto result = score_time_to_real_time(
            SCORE_START, score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(0.0));
    }

    SECTION("Bar 2, beat 0 = 2 seconds at 120 BPM") {
        auto result = score_time_to_real_time(
            ScoreTime{2, Beat::zero()}, score.tempo_map, score.time_map
        );
        REQUIRE(result.has_value());
        CHECK(*result == Catch::Approx(2.0));
    }
}

// =============================================================================
// SITM001A: beat_unit_duration
// =============================================================================

TEST_CASE("SITM001A: beat_unit_duration", "[score-ir][temporal]") {
    CHECK(beat_unit_duration(BeatUnit::Whole) == Beat{1, 1});
    CHECK(beat_unit_duration(BeatUnit::Half) == Beat{1, 2});
    CHECK(beat_unit_duration(BeatUnit::Quarter) == Beat{1, 4});
    CHECK(beat_unit_duration(BeatUnit::Eighth) == Beat{1, 8});
    CHECK(beat_unit_duration(BeatUnit::Sixteenth) == Beat{1, 16});
    CHECK(beat_unit_duration(BeatUnit::DottedHalf) == Beat{3, 4});
    CHECK(beat_unit_duration(BeatUnit::DottedQuarter) == Beat{3, 8});
    CHECK(beat_unit_duration(BeatUnit::DottedEighth) == Beat{3, 16});
}

// =============================================================================
// SITM001A: effective_quarter_bpm
// =============================================================================

TEST_CASE("SITM001A: effective_quarter_bpm normalisation", "[score-ir][temporal]") {
    SECTION("Quarter = 120 → quarter BPM = 120") {
        TempoEvent event;
        event.bpm = make_bpm(120);
        event.beat_unit = BeatUnit::Quarter;
        CHECK(effective_quarter_bpm(event) == Catch::Approx(120.0));
    }

    SECTION("Half = 60 → quarter BPM = 120") {
        TempoEvent event;
        event.bpm = make_bpm(60);
        event.beat_unit = BeatUnit::Half;
        // half note = Beat{1,2}, unit_in_quarters = 0.5 * 4 = 2
        // quarter BPM = 60 * 2 = 120
        CHECK(effective_quarter_bpm(event) == Catch::Approx(120.0));
    }

    SECTION("Dotted quarter = 80 → quarter BPM = 120") {
        TempoEvent event;
        event.bpm = make_bpm(80);
        event.beat_unit = BeatUnit::DottedQuarter;
        // dotted quarter = Beat{3,8}, unit_in_quarters = 3/8 * 4 = 1.5
        // quarter BPM = 80 * 1.5 = 120
        CHECK(effective_quarter_bpm(event) == Catch::Approx(120.0));
    }
}

// =============================================================================
// SITM001A: absolute_beat_to_score_time at exact score end
// =============================================================================

TEST_CASE("SITM001A: absolute_beat_to_score_time at exact score end returns bar N+1",
          "[score-ir][temporal]") {
    // 4 bars of 4/4: measure_duration = Beat{1,1}, total = Beat{4,1}
    auto score = make_test_score(4);

    auto result = absolute_beat_to_score_time(
        Beat{4, 1}, score.time_map, score.metadata.total_bars
    );
    REQUIRE(result.has_value());
    // Exact score end maps to bar total_bars+1 (i.e. 5), beat zero.
    // This kills the mutant that would produce total_bars-1 (bar 3).
    CHECK(result->bar == 5);
    CHECK(result->beat == Beat::zero());
}

// =============================================================================
// SITM001A: linear tempo ramp — absolute_beat_to_real_time
// =============================================================================

TEST_CASE("SITM001A: linear tempo ramp real-time at midpoint",
          "[score-ir][temporal]") {
    using Catch::Matchers::WithinAbs;

    // 4/4 time signature
    TimeSignatureMap time_map;
    auto ts = make_time_signature(4, 4);
    time_map.push_back(TimeSignatureEntry{1, *ts});

    // 120 BPM ramping linearly over 2 whole notes to 60 BPM
    TempoMap tempo_map;

    TempoEvent e0;
    e0.position = SCORE_START;
    e0.bpm = make_bpm(120);
    e0.beat_unit = BeatUnit::Quarter;
    e0.transition_type = TempoTransitionType::Linear;
    e0.linear_duration = Beat{2, 1};
    e0.old_unit = BeatUnit::Quarter;
    e0.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e0);

    TempoEvent e1;
    e1.position = ScoreTime{3, Beat::zero()};
    e1.bpm = make_bpm(60);
    e1.beat_unit = BeatUnit::Quarter;
    e1.transition_type = TempoTransitionType::Immediate;
    e1.linear_duration = Beat::zero();
    e1.old_unit = BeatUnit::Quarter;
    e1.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e1);

    // Query at 1 whole note — midpoint of the 2-whole-note ramp.
    // Interpolation: t0=0, t1=0.5, b0=120, b1=90.
    // Closed-form: 60 * 4 * ln(90/120) / (90 - 120) ≈ 2.3014 seconds.
    auto result = absolute_beat_to_real_time(Beat{1, 1}, tempo_map, time_map);
    REQUIRE(result.has_value());

    double expected = 60.0 * 4.0 * std::log(90.0 / 120.0) / (90.0 - 120.0);
    CHECK_THAT(*result, WithinAbs(expected, 0.01));

    // Confirm it differs from the constant-120 result (2.0 seconds)
    CHECK(*result > 2.0);
}

TEST_CASE("SITM001A: linear ramp past transition uses end BPM",
          "[score-ir][temporal]") {
    using Catch::Matchers::WithinAbs;

    TimeSignatureMap time_map;
    auto ts = make_time_signature(4, 4);
    time_map.push_back(TimeSignatureEntry{1, *ts});

    TempoMap tempo_map;

    TempoEvent e0;
    e0.position = SCORE_START;
    e0.bpm = make_bpm(120);
    e0.beat_unit = BeatUnit::Quarter;
    e0.transition_type = TempoTransitionType::Linear;
    e0.linear_duration = Beat{2, 1};
    e0.old_unit = BeatUnit::Quarter;
    e0.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e0);

    TempoEvent e1;
    e1.position = ScoreTime{3, Beat::zero()};
    e1.bpm = make_bpm(60);
    e1.beat_unit = BeatUnit::Quarter;
    e1.transition_type = TempoTransitionType::Immediate;
    e1.linear_duration = Beat::zero();
    e1.old_unit = BeatUnit::Quarter;
    e1.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e1);

    // Query at 3 whole notes: 1 whole note past the ramp end.
    // Ramp portion (0 to 2 whole notes): b0=120, b1=60, 8 quarter notes.
    //   60 * 8 * ln(60/120) / (60-120) ≈ 5.5452 seconds.
    // Constant portion (2 to 3 whole notes at 60 BPM): 60*4/60 = 4.0 seconds.
    // Total ≈ 9.5452 seconds.
    auto result = absolute_beat_to_real_time(Beat{3, 1}, tempo_map, time_map);
    REQUIRE(result.has_value());

    double ramp = 60.0 * 8.0 * std::log(60.0 / 120.0) / (60.0 - 120.0);
    double constant = 60.0 * 4.0 / 60.0;
    CHECK_THAT(*result, WithinAbs(ramp + constant, 0.01));
}

TEST_CASE("SITM001A: linear ramp with identical BPM uses constant path",
          "[score-ir][temporal]") {
    using Catch::Matchers::WithinAbs;

    TimeSignatureMap time_map;
    auto ts = make_time_signature(4, 4);
    time_map.push_back(TimeSignatureEntry{1, *ts});

    TempoMap tempo_map;

    TempoEvent e0;
    e0.position = SCORE_START;
    e0.bpm = make_bpm(120);
    e0.beat_unit = BeatUnit::Quarter;
    e0.transition_type = TempoTransitionType::Linear;
    e0.linear_duration = Beat{2, 1};
    e0.old_unit = BeatUnit::Quarter;
    e0.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e0);

    TempoEvent e1;
    e1.position = ScoreTime{3, Beat::zero()};
    e1.bpm = make_bpm(120);
    e1.beat_unit = BeatUnit::Quarter;
    e1.transition_type = TempoTransitionType::Immediate;
    e1.linear_duration = Beat::zero();
    e1.old_unit = BeatUnit::Quarter;
    e1.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e1);

    // Both events at 120 BPM — the abs(b1-b0) < 1e-10 branch applies.
    // Should give same result as constant 120 BPM: 60*4/120 = 2.0 seconds.
    auto result = absolute_beat_to_real_time(Beat{1, 1}, tempo_map, time_map);
    REQUIRE(result.has_value());
    CHECK_THAT(*result, WithinAbs(2.0, 1e-9));
}

TEST_CASE("SITM001A: multi-tempo-segment boundary",
          "[score-ir][temporal]") {
    using Catch::Matchers::WithinAbs;

    TimeSignatureMap time_map;
    auto ts = make_time_signature(4, 4);
    time_map.push_back(TimeSignatureEntry{1, *ts});

    // Three Immediate tempo changes: 120 at bar 1, 90 at bar 3, 60 at bar 5
    TempoMap tempo_map;

    TempoEvent e0;
    e0.position = SCORE_START;
    e0.bpm = make_bpm(120);
    e0.beat_unit = BeatUnit::Quarter;
    e0.transition_type = TempoTransitionType::Immediate;
    e0.linear_duration = Beat::zero();
    e0.old_unit = BeatUnit::Quarter;
    e0.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e0);

    TempoEvent e1;
    e1.position = ScoreTime{3, Beat::zero()};
    e1.bpm = make_bpm(90);
    e1.beat_unit = BeatUnit::Quarter;
    e1.transition_type = TempoTransitionType::Immediate;
    e1.linear_duration = Beat::zero();
    e1.old_unit = BeatUnit::Quarter;
    e1.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e1);

    TempoEvent e2;
    e2.position = ScoreTime{5, Beat::zero()};
    e2.bpm = make_bpm(60);
    e2.beat_unit = BeatUnit::Quarter;
    e2.transition_type = TempoTransitionType::Immediate;
    e2.linear_duration = Beat::zero();
    e2.old_unit = BeatUnit::Quarter;
    e2.new_unit = BeatUnit::Quarter;
    tempo_map.push_back(e2);

    // Query at 3 whole notes (bar 4 downbeat, inside second segment at 90 BPM).
    // Segment 1: 0 to 2 whole notes at 120 BPM = 60*8/120 = 4.0 seconds.
    // Segment 2: 2 to 3 whole notes at 90 BPM  = 60*4/90  ≈ 2.6667 seconds.
    // Total ≈ 6.6667 seconds.
    auto result = absolute_beat_to_real_time(Beat{3, 1}, tempo_map, time_map);
    REQUIRE(result.has_value());

    double seg1 = 60.0 * 8.0 / 120.0;
    double seg2 = 60.0 * 4.0 / 90.0;
    CHECK_THAT(*result, WithinAbs(seg1 + seg2, 0.01));
}
