/**
 * @file validation_rules_test.cpp
 * @brief Unit tests for Score IR validation rules — extended coverage
 *
 *
 * Coverage: S11 (tone row), M3 (parallel fifths), M4 (voice crossing),
 *           M5 (large leaps), M6 (leading tone), M7 (chordal seventh),
 *           M9 (missing orchestration), M10 (dynamic absent),
 *           R4 (grace note duration), R5 (tick rounding)
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/post_tonal/twelve_tone.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/time.hpp>
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

TEST_CASE("S13 rejects an unassigned root ScoreId", "[score-ir][validation][S13][identity]") {
    auto score = make_valid_score();
    score.id = ScoreId{0};

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S13" && diagnostic.severity == ValidationSeverity::Error &&
               diagnostic.message == "ScoreId zero is an unassigned sentinel";
    }));
    CHECK_FALSE(is_compilable(score));
}

} // anonymous namespace

TEST_CASE("S4 and S6 require in-meter points and exact map origins",
          "[score-ir][validation][S4][S6][trust-boundary]") {
    auto score = make_valid_score(1);
    TempoEvent invalid_tempo = score.tempo_map.front();
    invalid_tempo.position = ScoreTime{1, Beat{1, 1}};
    score.tempo_map.push_back(invalid_tempo);
    score.key_map.front().position = ScoreTime{1, Beat{1, 4}};

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S4" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S6" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S25 closes note and chord payload domains",
          "[score-ir][validation][S25][trust-boundary]") {
    auto score = make_valid_score(1);
    Note invalid_note;
    invalid_note.pitch.letter = 7;
    invalid_note.velocity.value = 200;
    invalid_note.articulation = static_cast<ArticulationType>(255);
    invalid_note.notation_head = static_cast<NoteHeadType>(255);
    NoteGroup group{{invalid_note}, Beat{1, 1}};
    score.parts[0].measures[0].voices[0].events[0].payload = group;

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S25" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));

    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes.clear();
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S15 closes the complete HarmonicAnnotation payload",
          "[score-ir][validation][S15][trust-boundary]") {
    const auto make_annotation = [](const Score& score) {
        HarmonicAnnotation annotation;
        annotation.position = SCORE_START;
        annotation.duration = Beat{1, 1};
        annotation.chord =
            ChordVoicing{{MidiNote{60}, MidiNote{64}, MidiNote{67}}, PitchClass{0}, "major", 0};
        annotation.key_context = score.key_map.front().key;
        annotation.roman_numeral = "I";
        annotation.function = ScoreHarmonicFunction::Tonic;
        return annotation;
    };
    const auto rejects = [&](HarmonicAnnotation annotation) {
        auto score = make_valid_score(1);
        score.harmonic_annotations.push_back(std::move(annotation));
        const auto diagnostics = validate_structural(score);
        CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
            return diagnostic.rule == "S15" && diagnostic.severity == ValidationSeverity::Error;
        }));
        CHECK_FALSE(is_compilable(score));
    };
    const auto base_score = make_valid_score(1);

    SECTION("empty voicing") {
        auto annotation = make_annotation(base_score);
        annotation.chord.notes.clear();
        rejects(std::move(annotation));
    }
    SECTION("non-ascending voicing") {
        auto annotation = make_annotation(base_score);
        annotation.chord.notes = {MidiNote{60}, MidiNote{67}, MidiNote{64}};
        rejects(std::move(annotation));
    }
    SECTION("incomplete registered quality") {
        auto annotation = make_annotation(base_score);
        annotation.chord.notes = {MidiNote{60}, MidiNote{64}};
        rejects(std::move(annotation));
    }
    SECTION("inversion disagrees with bass") {
        auto annotation = make_annotation(base_score);
        annotation.chord.inversion = 1;
        rejects(std::move(annotation));
    }
    SECTION("confidence is not finite") {
        auto annotation = make_annotation(base_score);
        annotation.confidence = std::numeric_limits<float>::quiet_NaN();
        rejects(std::move(annotation));
    }
    SECTION("Roman label is empty") {
        auto annotation = make_annotation(base_score);
        annotation.roman_numeral.clear();
        rejects(std::move(annotation));
    }
    SECTION("non-chord-tone enum is outside its closed domain") {
        auto annotation = make_annotation(base_score);
        annotation.non_chord_tones.push_back(
            NonChordToneAnnotation{EventId{1000}, 0, static_cast<NonChordToneType>(255)});
        rejects(std::move(annotation));
    }
}

TEST_CASE("S21 validates harmonic-annotation key identity",
          "[score-ir][validation][S21][harmony][trust-boundary]") {
    auto score = make_valid_score(1);
    HarmonicAnnotation annotation;
    annotation.position = SCORE_START;
    annotation.duration = Beat{1, 1};
    annotation.chord =
        ChordVoicing{{MidiNote{60}, MidiNote{64}, MidiNote{67}}, PitchClass{0}, "major", 0};
    annotation.key_context = score.key_map.front().key;
    annotation.key_context.accidentals = 1;
    annotation.roman_numeral = "I";
    annotation.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(std::move(annotation));

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

// =============================================================================
// S8: Tuplet span and nesting graph
// =============================================================================

TEST_CASE("S8 — nested tuplets use cumulative ancestor scaling", "[score-ir][validation][tuplet]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    const TupletContext outer{TupletId{1}, 3, 2, Beat{1, 4}, std::nullopt};
    const TupletContext inner{TupletId{2}, 3, 2, Beat{1, 8}, TupletId{1}};
    const auto group = [](Beat duration, TupletContext context) {
        NoteGroup result;
        result.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
        result.duration = duration;
        result.tuplet_context = context;
        return result;
    };

    events.push_back(Event{EventId{8101}, Beat::zero(), group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{8102}, Beat{1, 6}, group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{8103}, Beat{1, 3}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8104}, Beat{7, 18}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8105}, Beat{4, 9}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8106}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S8";
    }));
}

TEST_CASE("S8 — cyclic tuplet nesting is rejected", "[score-ir][validation][tuplet]") {
    auto score = make_valid_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    NoteGroup first;
    first.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    first.duration = Beat{1, 4};
    first.tuplet_context = TupletContext{TupletId{1}, 1, 1, Beat{1, 4}, TupletId{2}};
    NoteGroup second = first;
    second.tuplet_context = TupletContext{TupletId{2}, 1, 1, Beat{1, 4}, TupletId{1}};
    events.push_back(Event{EventId{8201}, Beat::zero(), first});
    events.push_back(Event{EventId{8202}, Beat{1, 4}, second});
    events.push_back(Event{EventId{8203}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S8" && diagnostic.severity == ValidationSeverity::Error;
    }));
}

// =============================================================================
// S11: Tone row completeness
// =============================================================================

TEST_CASE("S11 — tone row positive (all 12 PCs present)", "[score-ir][validation]") {
    auto score = make_valid_score(1);
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());
    score.tone_row = *row;

    // Put all 12 pitch classes into the bar via NoteGroups
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    for (int i = 0; i < 12; ++i) {
        NoteGroup ng;
        // Map pitch class to a SpelledPitch (C=0, C#=0+1, D=1, D#=1+1, etc.)
        // Use the natural letter names with accidentals
        uint8_t letter = 0;
        int8_t accidental = 0;
        switch (i) {
        case 0:
            letter = 0;
            accidental = 0;
            break; // C
        case 1:
            letter = 0;
            accidental = 1;
            break; // C#
        case 2:
            letter = 1;
            accidental = 0;
            break; // D
        case 3:
            letter = 1;
            accidental = 1;
            break; // D#
        case 4:
            letter = 2;
            accidental = 0;
            break; // E
        case 5:
            letter = 3;
            accidental = 0;
            break; // F
        case 6:
            letter = 3;
            accidental = 1;
            break; // F#
        case 7:
            letter = 4;
            accidental = 0;
            break; // G
        case 8:
            letter = 4;
            accidental = 1;
            break; // G#
        case 9:
            letter = 5;
            accidental = 0;
            break; // A
        case 10:
            letter = 5;
            accidental = 1;
            break; // A#
        case 11:
            letter = 6;
            accidental = 0;
            break; // B
        }
        ng.notes.push_back(Note{SpelledPitch{letter, accidental, 4}, VelocityValue{{}, 80}});
        ng.duration = Beat{1, 12};
        Beat offset{i, 12};
        voice.events.push_back(Event{EventId{static_cast<uint64_t>(2000 + i)}, offset, ng});
    }

    auto diags = validate_structural(score);
    bool found_s11 = false;
    for (const auto& d : diags) {
        if (d.rule == "S11") {
            found_s11 = true;
            break;
        }
    }
    CHECK_FALSE(found_s11);
}

TEST_CASE("S11 — tone row negative (missing pitch class)", "[score-ir][validation]") {
    auto score = make_valid_score(1);
    std::array<PitchClass, 12> pcs = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    auto row = make_tone_row(pcs);
    REQUIRE(row.has_value());
    score.tone_row = *row;

    // Put only 11 of the 12 pitch classes (omit B = pc 11)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    for (int i = 0; i < 11; ++i) {
        NoteGroup ng;
        uint8_t letter = 0;
        int8_t accidental = 0;
        switch (i) {
        case 0:
            letter = 0;
            accidental = 0;
            break;
        case 1:
            letter = 0;
            accidental = 1;
            break;
        case 2:
            letter = 1;
            accidental = 0;
            break;
        case 3:
            letter = 1;
            accidental = 1;
            break;
        case 4:
            letter = 2;
            accidental = 0;
            break;
        case 5:
            letter = 3;
            accidental = 0;
            break;
        case 6:
            letter = 3;
            accidental = 1;
            break;
        case 7:
            letter = 4;
            accidental = 0;
            break;
        case 8:
            letter = 4;
            accidental = 1;
            break;
        case 9:
            letter = 5;
            accidental = 0;
            break;
        case 10:
            letter = 5;
            accidental = 1;
            break;
        }
        ng.notes.push_back(Note{SpelledPitch{letter, accidental, 4}, VelocityValue{{}, 80}});
        ng.duration = Beat{1, 11};
        Beat offset{i, 11};
        voice.events.push_back(Event{EventId{static_cast<uint64_t>(2000 + i)}, offset, ng});
    }

    auto diags = validate_structural(score);
    bool found_s11 = false;
    for (const auto& d : diags) {
        if (d.rule == "S11") {
            found_s11 = true;
            break;
        }
    }
    CHECK(found_s11);
}

// =============================================================================
// M3: Parallel fifths
// =============================================================================

TEST_CASE("M3 — parallel fifths detected", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Build two NoteGroups with parallel perfect fifths:
    // First: C4 + G4 (interval = 7 semitones)
    // Second: D4 + A4 (interval = 7 semitones) — both move up by M2
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}}); // G4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}}); // D4
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}}); // A4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{3001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{3002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m3 = false;
    for (const auto& d : diags) {
        if (d.rule == "M3") {
            found_m3 = true;
            break;
        }
    }
    CHECK(found_m3);
}

TEST_CASE("M3 — no parallel fifths in contrary motion", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // First: C4 + G4 (P5)
    // Second: D4 + F4 (minor 3rd) — no parallel fifth
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}}); // G4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}}); // D4
    ng2.notes.push_back(Note{SpelledPitch{3, 0, 4}, VelocityValue{{}, 80}}); // F4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{3001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{3002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m3 = false;
    for (const auto& d : diags) {
        if (d.rule == "M3") {
            found_m3 = true;
            break;
        }
    }
    CHECK_FALSE(found_m3);
}

// =============================================================================
// M4: Voice crossing
// =============================================================================

TEST_CASE("M4 — voice crossing detected", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Create 2 voices in bar 1. Voice 0 has C5 (higher) and voice 1 has C3 (lower).
    // Voice crossing occurs because voice 0 should be lower than voice 1.
    auto& measure = score.parts[0].measures[0];
    measure.voices.clear();

    NoteGroup ng_high;
    ng_high.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}}); // C5
    ng_high.duration = Beat{1, 1};

    NoteGroup ng_low;
    ng_low.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}}); // C3
    ng_low.duration = Beat{1, 1};

    Voice v0{0, {Event{EventId{4001}, Beat::zero(), ng_high}}, {}};
    Voice v1{1, {Event{EventId{4002}, Beat::zero(), ng_low}}, {}};
    measure.voices.push_back(v0);
    measure.voices.push_back(v1);

    auto diags = validate_musical(score);
    bool found_m4 = false;
    for (const auto& d : diags) {
        if (d.rule == "M4") {
            found_m4 = true;
            break;
        }
    }
    CHECK(found_m4);
}

TEST_CASE("M4 — no voice crossing when properly ordered", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Voice 0 = C3 (low), Voice 1 = C5 (high) — no crossing
    auto& measure = score.parts[0].measures[0];
    measure.voices.clear();

    NoteGroup ng_low;
    ng_low.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}}); // C3
    ng_low.duration = Beat{1, 1};

    NoteGroup ng_high;
    ng_high.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}}); // C5
    ng_high.duration = Beat{1, 1};

    Voice v0{0, {Event{EventId{4003}, Beat::zero(), ng_low}}, {}};
    Voice v1{1, {Event{EventId{4004}, Beat::zero(), ng_high}}, {}};
    measure.voices.push_back(v0);
    measure.voices.push_back(v1);

    auto diags = validate_musical(score);
    bool found_m4 = false;
    for (const auto& d : diags) {
        if (d.rule == "M4") {
            found_m4 = true;
            break;
        }
    }
    CHECK_FALSE(found_m4);
}

// =============================================================================
// M5: Large leaps without recovery
// =============================================================================

TEST_CASE("M5 — large leap without recovery", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // C4 -> C6 (24 semitone leap) -> G5 (no step recovery — 5 semitones down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.duration = Beat{1, 4};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 6}, VelocityValue{{}, 80}}); // C6
    ng2.duration = Beat{1, 4};

    NoteGroup ng3;
    ng3.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}}); // G5
    ng3.duration = Beat{1, 4};

    // Pad with a rest to fill the measure
    RestEvent rest{Beat{1, 4}, true};

    voice.events.push_back(Event{EventId{5001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{5002}, Beat{1, 4}, ng2});
    voice.events.push_back(Event{EventId{5003}, Beat{2, 4}, ng3});
    voice.events.push_back(Event{EventId{5004}, Beat{3, 4}, rest});

    auto diags = validate_musical(score);
    bool found_m5 = false;
    for (const auto& d : diags) {
        if (d.rule == "M5") {
            found_m5 = true;
            break;
        }
    }
    CHECK(found_m5);
}

TEST_CASE("M5 — large leap with step recovery passes", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // C4 -> D5 (14 semitone leap) -> C5 (step recovery, 2 semitones down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}}); // C4
    ng1.duration = Beat{1, 4};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}}); // D5
    ng2.duration = Beat{1, 4};

    NoteGroup ng3;
    ng3.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}}); // C5
    ng3.duration = Beat{1, 4};

    RestEvent rest{Beat{1, 4}, true};

    voice.events.push_back(Event{EventId{5005}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{5006}, Beat{1, 4}, ng2});
    voice.events.push_back(Event{EventId{5007}, Beat{2, 4}, ng3});
    voice.events.push_back(Event{EventId{5008}, Beat{3, 4}, rest});

    auto diags = validate_musical(score);
    bool found_m5 = false;
    for (const auto& d : diags) {
        if (d.rule == "M5") {
            found_m5 = true;
            break;
        }
    }
    CHECK_FALSE(found_m5);
}

// =============================================================================
// M6: Unresolved leading tone
// =============================================================================

TEST_CASE("M6 — unresolved leading tone", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Key is C major (root = C4). Leading tone is B (pc=11).
    // B4 followed by A4 (not resolving to C) triggers M6.
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}}); // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}}); // A4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{6001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{6002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m6 = false;
    for (const auto& d : diags) {
        if (d.rule == "M6") {
            found_m6 = true;
            break;
        }
    }
    CHECK(found_m6);
}

TEST_CASE("M6 — leading tone properly resolved", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // B4 followed by C5 — proper resolution of leading tone
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}}); // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}}); // C5
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{6003}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{6004}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m6 = false;
    for (const auto& d : diags) {
        if (d.rule == "M6") {
            found_m6 = true;
            break;
        }
    }
    CHECK_FALSE(found_m6);
}

// =============================================================================
// M7: Unresolved chordal seventh
// =============================================================================

TEST_CASE("M7 — unresolved chordal seventh", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Set up a note B4 (11 semitones above C root = major 7th)
    // followed by D5 (upward motion — not resolving down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}}); // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}}); // D5
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{7001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{7002}, Beat{1, 2}, ng2});

    // Add a harmonic annotation with chord root C (pc=0)
    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord = ChordVoicing{{60, 64, 67, 71}, 0, "maj7", 0}; // C-E-G-B
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha);

    auto diags = validate_musical(score);
    bool found_m7 = false;
    for (const auto& d : diags) {
        if (d.rule == "M7") {
            found_m7 = true;
            break;
        }
    }
    CHECK(found_m7);
}

TEST_CASE("M7 — chordal seventh properly resolved downward", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // B4 (major 7th above C) followed by A4 (step down — proper resolution)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}}); // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}}); // A4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{7003}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{7004}, Beat{1, 2}, ng2});

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord = ChordVoicing{{60, 64, 67, 71}, 0, "maj7", 0};
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha);

    auto diags = validate_musical(score);
    bool found_m7 = false;
    for (const auto& d : diags) {
        if (d.rule == "M7") {
            found_m7 = true;
            break;
        }
    }
    CHECK_FALSE(found_m7);
}

// =============================================================================
// M9: Missing orchestration annotation (>8 bar gap)
// =============================================================================

TEST_CASE("M9 — orchestration gap triggers warning", "[score-ir][validation]") {
    auto score = make_valid_score(20);

    // Add one orchestration annotation at bar 1 only (covers bars 1-2)
    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};
    oa.start = SCORE_START;
    oa.end = ScoreTime{3, Beat::zero()};
    oa.role = TexturalRole::Melody;
    score.orchestration_annotations.push_back(oa);

    // Bars 3-20 have no orchestration coverage: gap of 18 bars > 8

    auto diags = validate_musical(score);
    bool found_m9 = false;
    for (const auto& d : diags) {
        if (d.rule == "M9") {
            found_m9 = true;
            break;
        }
    }
    CHECK(found_m9);
}

TEST_CASE("M9 — no warning when part has no orchestration at all", "[score-ir][validation]") {
    auto score = make_valid_score(20);
    // No orchestration annotations at all — M9 only fires when the part
    // has _some_ annotations but gaps exist

    auto diags = validate_musical(score);
    bool found_m9 = false;
    for (const auto& d : diags) {
        if (d.rule == "M9") {
            found_m9 = true;
            break;
        }
    }
    CHECK_FALSE(found_m9);
}

// =============================================================================
// M10: Dynamic absent > 16 bars
// =============================================================================

TEST_CASE("M10 — dynamic absent triggers info", "[score-ir][validation]") {
    auto score = make_valid_score(20);

    // 20 bars of rests, no dynamics at all — gap of 20 bars > 16

    auto diags = validate_musical(score);
    bool found_m10 = false;
    for (const auto& d : diags) {
        if (d.rule == "M10") {
            found_m10 = true;
            break;
        }
    }
    CHECK(found_m10);
}

TEST_CASE("M10 — no warning with dynamic present within 16 bars", "[score-ir][validation]") {
    auto score = make_valid_score(16);

    // Add a note with a dynamic in bar 1
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    Note n;
    n.pitch = SpelledPitch{0, 0, 4};
    n.velocity = VelocityValue{{}, 80};
    n.dynamic = DynamicLevel::mf;
    ng.notes.push_back(n);
    ng.duration = Beat{1, 1};
    voice.events[0].payload = ng;

    auto diags = validate_musical(score);
    bool found_m10 = false;
    for (const auto& d : diags) {
        if (d.rule == "M10") {
            found_m10 = true;
            break;
        }
    }
    CHECK_FALSE(found_m10);
}

// =============================================================================
// R4: Grace allocation > half the following ordinary note duration
// =============================================================================

TEST_CASE("R4 — grace note with excessive duration", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Grace allocation 1/4 exceeds half the following 1/4 note.
    NoteGroup ng_grace;
    Note grace_note;
    grace_note.pitch = SpelledPitch{0, 0, 4};
    grace_note.velocity = VelocityValue{{}, 80};
    grace_note.grace = GraceType::Acciaccatura;
    ng_grace.notes.push_back(grace_note);
    ng_grace.duration = Beat{1, 4};

    NoteGroup following;
    Note following_note;
    following_note.pitch = SpelledPitch{2, 0, 4};
    following.notes.push_back(following_note);
    following.duration = Beat{1, 4};
    RestEvent rest{Beat{1, 2}, true};

    voice.events.push_back(Event{EventId{8001}, Beat::zero(), ng_grace});
    voice.events.push_back(Event{EventId{8002}, Beat{1, 4}, following});
    voice.events.push_back(Event{EventId{8003}, Beat{1, 2}, rest});

    auto diags = validate_rendering(score);
    bool found_r4 = false;
    for (const auto& d : diags) {
        if (d.rule == "R4") {
            found_r4 = true;
            break;
        }
    }
    CHECK(found_r4);
}

TEST_CASE("R4 — grace note with acceptable duration", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Grace allocation 1/16 is exactly half the following 1/8 note.
    NoteGroup ng_grace;
    Note grace_note;
    grace_note.pitch = SpelledPitch{0, 0, 4};
    grace_note.velocity = VelocityValue{{}, 80};
    grace_note.grace = GraceType::Acciaccatura;
    ng_grace.notes.push_back(grace_note);
    ng_grace.duration = Beat{1, 16};

    NoteGroup following;
    Note following_note;
    following_note.pitch = SpelledPitch{2, 0, 4};
    following.notes.push_back(following_note);
    following.duration = Beat{1, 8};
    RestEvent rest{Beat{13, 16}, true};

    voice.events.push_back(Event{EventId{8004}, Beat::zero(), ng_grace});
    voice.events.push_back(Event{EventId{8005}, Beat{1, 16}, following});
    voice.events.push_back(Event{EventId{8006}, Beat{3, 16}, rest});

    auto diags = validate_rendering(score);
    bool found_r4 = false;
    for (const auto& d : diags) {
        if (d.rule == "R4") {
            found_r4 = true;
            break;
        }
    }
    CHECK_FALSE(found_r4);
}

TEST_CASE("S17 — a NoteGroup has one coherent grace timing class",
          "[score-ir][validation][grace]") {
    auto score = make_valid_score(1);
    Note initial;
    initial.pitch = SpelledPitch{0, 0, 4};
    score.parts[0].measures[0].voices[0].events[0].payload = NoteGroup{{initial}, Beat{1, 1}};
    auto& group = std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload);

    Note ordinary;
    ordinary.pitch = SpelledPitch{2, 0, 4};

    SECTION("grace and ordinary notes cannot share a group") {
        group.notes[0].grace = GraceType::Acciaccatura;
        group.notes.push_back(ordinary);
    }

    SECTION("different grace types cannot share a group") {
        group.notes[0].grace = GraceType::Acciaccatura;
        ordinary.grace = GraceType::Appoggiatura;
        group.notes.push_back(ordinary);
    }

    SECTION("a grace note cannot carry a duration tie") {
        group.notes[0].grace = GraceType::Acciaccatura;
        group.notes[0].tie_forward = true;
    }

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S17" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S7 — an ordinary duration tie cannot terminate on a grace note",
          "[score-ir][validation][grace][tie]") {
    auto score = make_valid_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];

    Note source;
    source.pitch = SpelledPitch{0, 0, 4};
    source.tie_forward = true;
    Note destination = source;
    destination.tie_forward = false;
    destination.grace = GraceType::Acciaccatura;

    voice.events.clear();
    voice.events.push_back(Event{EventId{8101}, Beat::zero(), NoteGroup{{source}, Beat{1, 4}}});
    voice.events.push_back(Event{EventId{8102}, Beat{1, 4}, NoteGroup{{destination}, Beat{1, 4}}});
    voice.events.push_back(Event{EventId{8103}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S7" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S19 — Direction variants require coherent payloads",
          "[score-ir][validation][direction]") {
    auto score = make_valid_score(1);
    ScoreDirection direction;

    SECTION("Text requires non-empty text") {
        direction.type = DirectionType::Text;
    }

    SECTION("TempoText requires non-empty text") {
        direction.type = DirectionType::TempoText;
        direction.text = "";
    }

    SECTION("ClefChange requires new_clef") {
        direction.type = DirectionType::ClefChange;
    }

    SECTION("OttavaStart requires a modelled octave displacement") {
        direction.type = DirectionType::OttavaStart;
        direction.ottava_shift = 7;
    }

    score.parts[0].measures[0].voices[0].events.push_back(
        Event{EventId{8201}, Beat{1, 2}, direction});
    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S19" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S19 — irrelevant Direction payload is advisory and valid payload compiles",
          "[score-ir][validation][direction]") {
    auto score = make_valid_score(1);

    ScoreDirection ottava;
    ottava.type = DirectionType::OttavaStart;
    ottava.ottava_shift = -24;
    score.parts[0].measures[0].voices[0].events.push_back(Event{EventId{8202}, Beat{1, 4}, ottava});

    ScoreDirection pedal;
    pedal.type = DirectionType::PedalDown;
    pedal.text = "irrelevant";
    pedal.new_clef = Clef::Bass;
    pedal.ottava_shift = 12;
    score.parts[0].measures[0].voices[0].events.push_back(Event{EventId{8203}, Beat{1, 2}, pedal});

    ScoreDirection ottava_end;
    ottava_end.type = DirectionType::OttavaEnd;
    score.parts[0].measures[0].voices[0].events.push_back(
        Event{EventId{8204}, Beat{3, 4}, ottava_end});

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S19" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK(std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
              return diagnostic.rule == "S19" && diagnostic.severity == ValidationSeverity::Warning;
          }) == 3);
    CHECK(is_compilable(score));
}

TEST_CASE("S20 — part spans and Divisi payload are structural contracts",
          "[score-ir][validation][part-span]") {
    auto score = make_valid_score(2);

    SECTION("a zero-length hairpin is rejected") {
        score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat{1, 4}},
                                                  ScoreTime{1, Beat{1, 4}},
                                                  HairpinType::Crescendo,
                                                  DynamicLevel::f});
    }

    SECTION("overlapping hairpins are rejected") {
        score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat::zero()},
                                                  ScoreTime{2, Beat::zero()},
                                                  HairpinType::Crescendo,
                                                  DynamicLevel::f});
        score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat{1, 2}},
                                                  ScoreTime{3, Beat::zero()},
                                                  HairpinType::Diminuendo,
                                                  DynamicLevel::p});
    }

    SECTION("Divisi requires at least two divisions") {
        score.parts[0].part_directives.push_back(PartDirective{
            ScoreTime{1, Beat::zero()}, ScoreTime{2, Beat::zero()}, DirectiveType::Divisi, 1});
    }

    SECTION("a range endpoint beyond the score is rejected") {
        score.parts[0].part_directives.push_back(PartDirective{
            ScoreTime{1, Beat::zero()}, ScoreTime{4, Beat::zero()}, DirectiveType::Pizzicato, 0});
    }

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S20" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S20 — irrelevant divisi_count is advisory", "[score-ir][validation][part-span]") {
    auto score = make_valid_score(1);
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat::zero()}, ScoreTime{2, Beat::zero()}, DirectiveType::Arco, 3});

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S20" && diagnostic.severity == ValidationSeverity::Warning;
    }));
    CHECK(is_compilable(score));
}

TEST_CASE("S21 — standard key tonic mode and fifths form one identity",
          "[score-ir][validation][key]") {
    auto score = make_valid_score(1);
    const auto major = find_scale("major");
    REQUIRE(major.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{0, 0, 4}, *major, 1};

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S21 — a coherent church-mode signature remains compilable",
          "[score-ir][validation][key]") {
    auto score = make_valid_score(1);
    const auto dorian = find_scale("dorian");
    REQUIRE(dorian.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{1, 0, 4}, *dorian, 0};

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21";
    }));
    CHECK(is_compilable(score));
}

TEST_CASE("S21 — scale interval storage is a closed canonical profile",
          "[score-ir][validation][key]") {
    auto score = make_valid_score(1);

    SECTION("a zero-note scale is not a key mode") {
        score.key_map[0].key.mode = {};
    }
    SECTION("intervals must begin at the root") {
        score.key_map[0].key.mode.intervals[0] = 1;
    }
    SECTION("intervals must increase strictly") {
        score.key_map[0].key.mode.intervals[2] = 2;
    }
    SECTION("intervals are pitch-class offsets") {
        score.key_map[0].key.mode.intervals[6] = 12;
    }
    SECTION("unused fixed-capacity storage must be zero") {
        score.key_map[0].key.mode.intervals[7] = 1;
    }

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S21 — a registered scale name cannot contradict its intervals",
          "[score-ir][validation][key]") {
    auto score = make_valid_score(1);
    score.key_map[0].key.mode.intervals[2] = 3;

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21" &&
               diagnostic.message.find("registered interval definition") != std::string::npos;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S21 — a coherent named custom scale retains an independent signature",
          "[score-ir][validation][key]") {
    auto score = make_valid_score(1);
    score.key_map[0].key.mode = ScaleDefinition{"sunny_hexatonic",
                                                {0, 1, 4, 6, 8, 11, 0, 0, 0, 0, 0, 0},
                                                6,
                                                "project-defined analysis scale"};
    score.key_map[0].key.accidentals = 3;

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S21";
    }));
    CHECK(is_compilable(score));
}

TEST_CASE("S22 — staff topology rejects impossible Voice ownership",
          "[score-ir][validation][staff]") {
    auto score = make_valid_score(1);
    score.parts[0].definition.staff_count = 2;
    score.parts[0].definition.staff_clefs = {Clef::Treble};
    score.parts[0].measures[0].voices[0].staff_index = 2;

    const auto diagnostics = validate_structural(score);
    CHECK(std::count_if(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
              return diagnostic.rule == "S22" && diagnostic.severity == ValidationSeverity::Error;
          }) == 2);
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S22 — differentiated clefs and an assigned lower staff are valid",
          "[score-ir][validation][staff]") {
    auto score = make_valid_score(1);
    score.parts[0].definition.staff_count = 2;
    score.parts[0].definition.staff_clefs = {Clef::Treble, Clef::Bass};
    score.parts[0].measures[0].voices[0].staff_index = 1;

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S22";
    }));
    CHECK(is_compilable(score));
}

TEST_CASE("S23 — orphaned or overlapping notation endpoints are rejected",
          "[score-ir][validation][span-endpoint]") {
    auto score = make_valid_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];

    SECTION("orphaned slur stop") {
        Note note;
        note.pitch = SpelledPitch{0, 0, 4};
        note.velocity = VelocityValue{std::nullopt, 80};
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 1};
        group.slur_end = true;
        voice.events = {Event{EventId{8301}, Beat::zero(), group}};
    }

    SECTION("unterminated glissando") {
        Note note;
        note.pitch = SpelledPitch{0, 0, 4};
        note.velocity = VelocityValue{std::nullopt, 80};
        note.articulation = ArticulationType::GlissandoStart;
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 1};
        voice.events = {Event{EventId{8302}, Beat::zero(), group}};
    }

    SECTION("overlapping ottavas") {
        ScoreDirection first;
        first.type = DirectionType::OttavaStart;
        first.ottava_shift = 12;
        ScoreDirection second = first;
        voice.events.push_back(Event{EventId{8303}, Beat{1, 4}, first});
        voice.events.push_back(Event{EventId{8304}, Beat{1, 2}, second});
    }

    SECTION("orphaned pedal lift") {
        ScoreDirection up;
        up.type = DirectionType::PedalUp;
        voice.events.push_back(Event{EventId{8305}, Beat{1, 2}, up});
    }

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S23" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S23 — chained slur and balanced Staff spans are coherent",
          "[score-ir][validation][span-endpoint]") {
    auto score = make_valid_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    ScoreDirection ottava_start;
    ottava_start.type = DirectionType::OttavaStart;
    ottava_start.ottava_shift = 12;
    ScoreDirection pedal_down;
    pedal_down.type = DirectionType::PedalDown;
    voice.events.push_back(Event{EventId{8310}, Beat::zero(), ottava_start});
    voice.events.push_back(Event{EventId{8311}, Beat::zero(), pedal_down});

    for (std::int64_t i = 0; i < 4; ++i) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(i), 0, 4};
        note.velocity = VelocityValue{std::nullopt, 80};
        if (i == 0) note.articulation = ArticulationType::GlissandoStart;
        if (i == 2) note.articulation = ArticulationType::GlissandoEnd;
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 4};
        group.slur_start = i == 0 || i == 1;
        group.slur_end = i == 1 || i == 2;
        voice.events.push_back(
            Event{EventId{8312 + static_cast<std::uint64_t>(i)}, Beat{i, 4}, group});
    }

    ScoreDirection ottava_end;
    ottava_end.type = DirectionType::OttavaEnd;
    ScoreDirection pedal_up;
    pedal_up.type = DirectionType::PedalUp;
    voice.events.insert(voice.events.end() - 1, Event{EventId{8316}, Beat{3, 4}, ottava_end});
    voice.events.insert(voice.events.end() - 1, Event{EventId{8317}, Beat{3, 4}, pedal_up});

    const auto diagnostics = validate_structural(score);
    CHECK(std::none_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S23";
    }));
    CHECK(is_compilable(score));
}

TEST_CASE("S23 — dual sustaining-pedal representations must agree",
          "[score-ir][validation][span-endpoint][pedal]") {
    auto score = make_valid_score(1);
    ScoreDirection down;
    down.type = DirectionType::PedalDown;
    ScoreDirection up;
    up.type = DirectionType::PedalUp;
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.push_back(Event{EventId{8320}, Beat{1, 4}, down});
    events.push_back(Event{EventId{8321}, Beat{3, 4}, up});
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 2}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::SustainingPedal, 0});

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S23" &&
               diagnostic.message.find("agree exactly") != std::string::npos;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S24 — tempo transitions have one exact incoming interpretation",
          "[score-ir][validation][tempo-transition]") {
    auto score = make_valid_score(2);
    TempoEvent target = score.tempo_map.front();
    target.position = ScoreTime{2, Beat::zero()};
    target.bpm = make_bpm(90);
    score.tempo_map.push_back(target);

    SECTION("the initial event cannot describe an incoming ramp") {
        score.tempo_map.front().transition_type = TempoTransitionType::Linear;
        score.tempo_map.front().linear_duration = Beat{1, 1};
    }

    SECTION("a linear duration cannot leave a hidden jump") {
        score.tempo_map.back().transition_type = TempoTransitionType::Linear;
        score.tempo_map.back().linear_duration = Beat{1, 2};
    }

    SECTION("an immediate event cannot carry an ignored ramp duration") {
        score.tempo_map.back().linear_duration = Beat{1, 1};
    }

    SECTION("invalid in-memory enum payloads are rejected") {
        score.tempo_map.back().beat_unit = static_cast<BeatUnit>(255);
    }

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S24" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK_FALSE(is_compilable(score));
}

TEST_CASE("S24 — linear and metric destination events close their exact rates",
          "[score-ir][validation][tempo-transition]") {
    SECTION("linear destination spans the complete preceding interval") {
        auto score = make_valid_score(2);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{2, Beat::zero()};
        target.bpm = make_bpm(90);
        target.transition_type = TempoTransitionType::Linear;
        target.linear_duration = Beat{1, 1};
        score.tempo_map.push_back(target);

        const auto diagnostics = validate_structural(score);
        CHECK(std::none_of(diagnostics.begin(),
                           diagnostics.end(),
                           [](const Diagnostic& diagnostic) { return diagnostic.rule == "S24"; }));
        CHECK(is_compilable(score));
    }

    SECTION("quarter = 120 becoming half = 120 yields quarter = 240") {
        auto score = make_valid_score(2);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{2, Beat::zero()};
        target.bpm = make_bpm(120);
        target.beat_unit = BeatUnit::Half;
        target.transition_type = TempoTransitionType::MetricModulation;
        target.old_unit = BeatUnit::Quarter;
        target.new_unit = BeatUnit::Half;
        score.tempo_map.push_back(target);

        const auto diagnostics = validate_structural(score);
        CHECK(std::none_of(diagnostics.begin(),
                           diagnostics.end(),
                           [](const Diagnostic& diagnostic) { return diagnostic.rule == "S24"; }));
        CHECK(is_compilable(score));

        score.tempo_map.back().bpm = make_bpm(60);
        const auto invalid = validate_structural(score);
        CHECK(std::any_of(invalid.begin(), invalid.end(), [](const Diagnostic& diagnostic) {
            return diagnostic.rule == "S24" &&
                   diagnostic.message.find("exact old-unit/new-unit") != std::string::npos;
        }));
        CHECK_FALSE(is_compilable(score));
    }
}

// =============================================================================
// R5: Tick rounding (no false positives for standard values)
// =============================================================================

TEST_CASE("R5 — standard beat values produce no rounding errors", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Default score has a whole-note rest at Beat::zero() — standard value
    auto diags = validate_rendering(score);
    bool found_r5 = false;
    for (const auto& d : diags) {
        if (d.rule == "R5") {
            found_r5 = true;
            break;
        }
    }
    CHECK_FALSE(found_r5);
}

TEST_CASE("R3 — an articulation without any map still reports fallback use",
          "[score-ir][validation][articulation-mapping]") {
    auto score = make_valid_score(1);
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.articulation = ArticulationType::Staccato;
    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 1};
    score.parts[0].measures[0].voices[0].events[0].payload = group;

    const auto diagnostics = validate_rendering(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "R3" && diagnostic.error_code == ErrorCode::UnmappedArticulation;
    }));
}

TEST_CASE("R6 and R7 reject rendering-domain and velocity-semantic contradictions",
          "[score-ir][validation][articulation-mapping][trust-boundary]") {
    auto score = make_valid_score(1);
    score.parts[0].definition.rendering.pan = 1.5F;
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.dynamic = DynamicLevel::f;
    note.velocity.written = DynamicLevel::p;
    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 1};
    score.parts[0].measures[0].voices[0].events[0].payload = group;

    const auto diagnostics = validate_rendering(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "R6" && diagnostic.severity == ValidationSeverity::Error;
    }));
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "R7" && diagnostic.severity == ValidationSeverity::Error;
    }));
}
