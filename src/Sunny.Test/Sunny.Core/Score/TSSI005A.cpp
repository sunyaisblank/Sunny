/**
 * @file TSSI005A.cpp
 * @brief Unit tests for Score IR validation rules (SIVD001A) — extended coverage
 *
 * Component: TSSI005A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIVD001A
 * Coverage: S11 (tone row), M3 (parallel fifths), M4 (voice crossing),
 *           M5 (large leaps), M6 (leading tone), M7 (chordal seventh),
 *           M9 (missing orchestration), M10 (dynamic absent),
 *           R4 (grace note duration), R5 (tick rounding)
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIVD001A.h"
#include "Score/SIMT001A.h"
#include "PostTonal/SRTW001A.h"

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

}  // anonymous namespace

// =============================================================================
// S11: Tone row completeness
// =============================================================================

TEST_CASE("SIVD001A: S11 — tone row positive (all 12 PCs present)",
          "[score-ir][validation]") {
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
            case 0: letter = 0; accidental = 0; break;  // C
            case 1: letter = 0; accidental = 1; break;  // C#
            case 2: letter = 1; accidental = 0; break;  // D
            case 3: letter = 1; accidental = 1; break;  // D#
            case 4: letter = 2; accidental = 0; break;  // E
            case 5: letter = 3; accidental = 0; break;  // F
            case 6: letter = 3; accidental = 1; break;  // F#
            case 7: letter = 4; accidental = 0; break;  // G
            case 8: letter = 4; accidental = 1; break;  // G#
            case 9: letter = 5; accidental = 0; break;  // A
            case 10: letter = 5; accidental = 1; break; // A#
            case 11: letter = 6; accidental = 0; break;  // B
        }
        ng.notes.push_back(Note{SpelledPitch{letter, accidental, 4},
                                VelocityValue{{}, 80}});
        ng.duration = Beat{1, 12};
        Beat offset{i, 12};
        voice.events.push_back(Event{EventId{static_cast<uint64_t>(2000 + i)},
                                     offset, ng});
    }

    auto diags = validate_structural(score);
    bool found_s11 = false;
    for (const auto& d : diags) {
        if (d.rule == "S11") { found_s11 = true; break; }
    }
    CHECK_FALSE(found_s11);
}

TEST_CASE("SIVD001A: S11 — tone row negative (missing pitch class)",
          "[score-ir][validation]") {
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
            case 0: letter = 0; accidental = 0; break;
            case 1: letter = 0; accidental = 1; break;
            case 2: letter = 1; accidental = 0; break;
            case 3: letter = 1; accidental = 1; break;
            case 4: letter = 2; accidental = 0; break;
            case 5: letter = 3; accidental = 0; break;
            case 6: letter = 3; accidental = 1; break;
            case 7: letter = 4; accidental = 0; break;
            case 8: letter = 4; accidental = 1; break;
            case 9: letter = 5; accidental = 0; break;
            case 10: letter = 5; accidental = 1; break;
        }
        ng.notes.push_back(Note{SpelledPitch{letter, accidental, 4},
                                VelocityValue{{}, 80}});
        ng.duration = Beat{1, 11};
        Beat offset{i, 11};
        voice.events.push_back(Event{EventId{static_cast<uint64_t>(2000 + i)},
                                     offset, ng});
    }

    auto diags = validate_structural(score);
    bool found_s11 = false;
    for (const auto& d : diags) {
        if (d.rule == "S11") { found_s11 = true; break; }
    }
    CHECK(found_s11);
}

// =============================================================================
// M3: Parallel fifths
// =============================================================================

TEST_CASE("SIVD001A: M3 — parallel fifths detected", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Build two NoteGroups with parallel perfect fifths:
    // First: C4 + G4 (interval = 7 semitones)
    // Second: D4 + A4 (interval = 7 semitones) — both move up by M2
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});  // D4
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}});  // A4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{3001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{3002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m3 = false;
    for (const auto& d : diags) {
        if (d.rule == "M3") { found_m3 = true; break; }
    }
    CHECK(found_m3);
}

TEST_CASE("SIVD001A: M3 — no parallel fifths in contrary motion",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // First: C4 + G4 (P5)
    // Second: D4 + F4 (minor 3rd) — no parallel fifth
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.notes.push_back(Note{SpelledPitch{4, 0, 4}, VelocityValue{{}, 80}});  // G4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 4}, VelocityValue{{}, 80}});  // D4
    ng2.notes.push_back(Note{SpelledPitch{3, 0, 4}, VelocityValue{{}, 80}});  // F4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{3001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{3002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m3 = false;
    for (const auto& d : diags) {
        if (d.rule == "M3") { found_m3 = true; break; }
    }
    CHECK_FALSE(found_m3);
}

// =============================================================================
// M4: Voice crossing
// =============================================================================

TEST_CASE("SIVD001A: M4 — voice crossing detected", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Create 2 voices in bar 1. Voice 0 has C5 (higher) and voice 1 has C3 (lower).
    // Voice crossing occurs because voice 0 should be lower than voice 1.
    auto& measure = score.parts[0].measures[0];
    measure.voices.clear();

    NoteGroup ng_high;
    ng_high.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng_high.duration = Beat{1, 1};

    NoteGroup ng_low;
    ng_low.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}});  // C3
    ng_low.duration = Beat{1, 1};

    Voice v0{0, {Event{EventId{4001}, Beat::zero(), ng_high}}, {}};
    Voice v1{1, {Event{EventId{4002}, Beat::zero(), ng_low}}, {}};
    measure.voices.push_back(v0);
    measure.voices.push_back(v1);

    auto diags = validate_musical(score);
    bool found_m4 = false;
    for (const auto& d : diags) {
        if (d.rule == "M4") { found_m4 = true; break; }
    }
    CHECK(found_m4);
}

TEST_CASE("SIVD001A: M4 — no voice crossing when properly ordered",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Voice 0 = C3 (low), Voice 1 = C5 (high) — no crossing
    auto& measure = score.parts[0].measures[0];
    measure.voices.clear();

    NoteGroup ng_low;
    ng_low.notes.push_back(Note{SpelledPitch{0, 0, 3}, VelocityValue{{}, 80}});  // C3
    ng_low.duration = Beat{1, 1};

    NoteGroup ng_high;
    ng_high.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng_high.duration = Beat{1, 1};

    Voice v0{0, {Event{EventId{4003}, Beat::zero(), ng_low}}, {}};
    Voice v1{1, {Event{EventId{4004}, Beat::zero(), ng_high}}, {}};
    measure.voices.push_back(v0);
    measure.voices.push_back(v1);

    auto diags = validate_musical(score);
    bool found_m4 = false;
    for (const auto& d : diags) {
        if (d.rule == "M4") { found_m4 = true; break; }
    }
    CHECK_FALSE(found_m4);
}

// =============================================================================
// M5: Large leaps without recovery
// =============================================================================

TEST_CASE("SIVD001A: M5 — large leap without recovery", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // C4 -> C6 (24 semitone leap) -> G5 (no step recovery — 5 semitones down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.duration = Beat{1, 4};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 6}, VelocityValue{{}, 80}});  // C6
    ng2.duration = Beat{1, 4};

    NoteGroup ng3;
    ng3.notes.push_back(Note{SpelledPitch{4, 0, 5}, VelocityValue{{}, 80}});  // G5
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
        if (d.rule == "M5") { found_m5 = true; break; }
    }
    CHECK(found_m5);
}

TEST_CASE("SIVD001A: M5 — large leap with step recovery passes",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // C4 -> D5 (14 semitone leap) -> C5 (step recovery, 2 semitones down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});  // C4
    ng1.duration = Beat{1, 4};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}});  // D5
    ng2.duration = Beat{1, 4};

    NoteGroup ng3;
    ng3.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng3.duration = Beat{1, 4};

    RestEvent rest{Beat{1, 4}, true};

    voice.events.push_back(Event{EventId{5005}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{5006}, Beat{1, 4}, ng2});
    voice.events.push_back(Event{EventId{5007}, Beat{2, 4}, ng3});
    voice.events.push_back(Event{EventId{5008}, Beat{3, 4}, rest});

    auto diags = validate_musical(score);
    bool found_m5 = false;
    for (const auto& d : diags) {
        if (d.rule == "M5") { found_m5 = true; break; }
    }
    CHECK_FALSE(found_m5);
}

// =============================================================================
// M6: Unresolved leading tone
// =============================================================================

TEST_CASE("SIVD001A: M6 — unresolved leading tone", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Key is C major (root = C4). Leading tone is B (pc=11).
    // B4 followed by A4 (not resolving to C) triggers M6.
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}});  // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}});  // A4
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{6001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{6002}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m6 = false;
    for (const auto& d : diags) {
        if (d.rule == "M6") { found_m6 = true; break; }
    }
    CHECK(found_m6);
}

TEST_CASE("SIVD001A: M6 — leading tone properly resolved",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // B4 followed by C5 — proper resolution of leading tone
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}});  // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{0, 0, 5}, VelocityValue{{}, 80}});  // C5
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{6003}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{6004}, Beat{1, 2}, ng2});

    auto diags = validate_musical(score);
    bool found_m6 = false;
    for (const auto& d : diags) {
        if (d.rule == "M6") { found_m6 = true; break; }
    }
    CHECK_FALSE(found_m6);
}

// =============================================================================
// M7: Unresolved chordal seventh
// =============================================================================

TEST_CASE("SIVD001A: M7 — unresolved chordal seventh", "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Set up a note B4 (11 semitones above C root = major 7th)
    // followed by D5 (upward motion — not resolving down)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}});  // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{1, 0, 5}, VelocityValue{{}, 80}});  // D5
    ng2.duration = Beat{1, 2};

    voice.events.push_back(Event{EventId{7001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{7002}, Beat{1, 2}, ng2});

    // Add a harmonic annotation with chord root C (pc=0)
    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord = ChordVoicing{{60, 64, 67, 71}, 0, "maj7", 0};  // C-E-G-B
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha);

    auto diags = validate_musical(score);
    bool found_m7 = false;
    for (const auto& d : diags) {
        if (d.rule == "M7") { found_m7 = true; break; }
    }
    CHECK(found_m7);
}

TEST_CASE("SIVD001A: M7 — chordal seventh properly resolved downward",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // B4 (major 7th above C) followed by A4 (step down — proper resolution)
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    ng1.notes.push_back(Note{SpelledPitch{6, 0, 4}, VelocityValue{{}, 80}});  // B4
    ng1.duration = Beat{1, 2};

    NoteGroup ng2;
    ng2.notes.push_back(Note{SpelledPitch{5, 0, 4}, VelocityValue{{}, 80}});  // A4
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
        if (d.rule == "M7") { found_m7 = true; break; }
    }
    CHECK_FALSE(found_m7);
}

// =============================================================================
// M9: Missing orchestration annotation (>8 bar gap)
// =============================================================================

TEST_CASE("SIVD001A: M9 — orchestration gap triggers warning",
          "[score-ir][validation]") {
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
        if (d.rule == "M9") { found_m9 = true; break; }
    }
    CHECK(found_m9);
}

TEST_CASE("SIVD001A: M9 — no warning when part has no orchestration at all",
          "[score-ir][validation]") {
    auto score = make_valid_score(20);
    // No orchestration annotations at all — M9 only fires when the part
    // has _some_ annotations but gaps exist

    auto diags = validate_musical(score);
    bool found_m9 = false;
    for (const auto& d : diags) {
        if (d.rule == "M9") { found_m9 = true; break; }
    }
    CHECK_FALSE(found_m9);
}

// =============================================================================
// M10: Dynamic absent > 16 bars
// =============================================================================

TEST_CASE("SIVD001A: M10 — dynamic absent triggers info",
          "[score-ir][validation]") {
    auto score = make_valid_score(20);

    // 20 bars of rests, no dynamics at all — gap of 20 bars > 16

    auto diags = validate_musical(score);
    bool found_m10 = false;
    for (const auto& d : diags) {
        if (d.rule == "M10") { found_m10 = true; break; }
    }
    CHECK(found_m10);
}

TEST_CASE("SIVD001A: M10 — no warning with dynamic present within 16 bars",
          "[score-ir][validation]") {
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
        if (d.rule == "M10") { found_m10 = true; break; }
    }
    CHECK_FALSE(found_m10);
}

// =============================================================================
// R4: Grace note duration > 1/16
// =============================================================================

TEST_CASE("SIVD001A: R4 — grace note with excessive duration",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Grace note with duration 1/4 (exceeds 1/16 threshold)
    NoteGroup ng_grace;
    Note grace_note;
    grace_note.pitch = SpelledPitch{0, 0, 4};
    grace_note.velocity = VelocityValue{{}, 80};
    grace_note.grace = GraceType::Acciaccatura;
    ng_grace.notes.push_back(grace_note);
    ng_grace.duration = Beat{1, 4};

    // Fill rest of measure
    RestEvent rest{Beat{3, 4}, true};

    voice.events.push_back(Event{EventId{8001}, Beat::zero(), ng_grace});
    voice.events.push_back(Event{EventId{8002}, Beat{1, 4}, rest});

    auto diags = validate_rendering(score);
    bool found_r4 = false;
    for (const auto& d : diags) {
        if (d.rule == "R4") { found_r4 = true; break; }
    }
    CHECK(found_r4);
}

TEST_CASE("SIVD001A: R4 — grace note with acceptable duration",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Grace note with duration 1/32 (within 1/16 threshold)
    NoteGroup ng_grace;
    Note grace_note;
    grace_note.pitch = SpelledPitch{0, 0, 4};
    grace_note.velocity = VelocityValue{{}, 80};
    grace_note.grace = GraceType::Acciaccatura;
    ng_grace.notes.push_back(grace_note);
    ng_grace.duration = Beat{1, 32};

    RestEvent rest{Beat{31, 32}, true};

    voice.events.push_back(Event{EventId{8003}, Beat::zero(), ng_grace});
    voice.events.push_back(Event{EventId{8004}, Beat{1, 32}, rest});

    auto diags = validate_rendering(score);
    bool found_r4 = false;
    for (const auto& d : diags) {
        if (d.rule == "R4") { found_r4 = true; break; }
    }
    CHECK_FALSE(found_r4);
}

// =============================================================================
// R5: Tick rounding (no false positives for standard values)
// =============================================================================

TEST_CASE("SIVD001A: R5 — standard beat values produce no rounding errors",
          "[score-ir][validation]") {
    auto score = make_valid_score(1);

    // Default score has a whole-note rest at Beat::zero() — standard value
    auto diags = validate_rendering(score);
    bool found_r5 = false;
    for (const auto& d : diags) {
        if (d.rule == "R5") { found_r5 = true; break; }
    }
    CHECK_FALSE(found_r5);
}
