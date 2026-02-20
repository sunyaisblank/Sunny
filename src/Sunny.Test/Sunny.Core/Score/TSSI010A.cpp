/**
 * @file TSSI010A.cpp
 * @brief Unit tests for Score IR harmonic analysis (SIHA001A)
 *
 * Component: TSSI010A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIHA001A
 * Coverage: derive_harmonic_layer, refresh_stale_regions,
 *           cadence detection, key change handling,
 *           unrecognised chord handling
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIHA001A.h"
#include "Score/SIMT001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

constexpr SpelledPitch C4{0, 0, 4};
constexpr SpelledPitch D4{1, 0, 4};
constexpr SpelledPitch E4{2, 0, 4};
constexpr SpelledPitch F4{3, 0, 4};
constexpr SpelledPitch G4{4, 0, 4};
constexpr SpelledPitch A4{5, 0, 4};
constexpr SpelledPitch B4{6, 0, 4};
constexpr SpelledPitch C5{0, 0, 5};
constexpr SpelledPitch D5{1, 0, 5};
constexpr SpelledPitch E5{2, 0, 5};
constexpr SpelledPitch Gs4{4, 1, 4};

Score make_score_with_key(std::uint32_t total_bars,
                          SpelledPitch key_root,
                          std::int8_t accidentals,
                          bool minor = false)
{
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Harmonic Test";
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
    key_entry.key.root = key_root;
    key_entry.key.accidentals = accidentals;
    auto scale = minor ? find_scale("minor") : find_scale("major");
    if (scale) key_entry.key.mode = *scale;
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

/// Place a whole-note chord in a bar (0-based bar_idx).
void place_chord(Score& score, std::size_t part_idx, std::size_t bar_idx,
                 std::initializer_list<SpelledPitch> pitches)
{
    auto& voice = score.parts[part_idx].measures[bar_idx].voices[0];

    NoteGroup ng;
    for (const auto& p : pitches) {
        Note note;
        note.pitch = p;
        ng.notes.push_back(note);
    }
    ng.duration = Beat{1, 1};

    voice.events[0].payload = ng;
}

/// Place two half-note chords in a bar (0-based bar_idx).
void place_two_chords(Score& score, std::size_t part_idx, std::size_t bar_idx,
                      std::initializer_list<SpelledPitch> first,
                      std::initializer_list<SpelledPitch> second)
{
    auto& voice = score.parts[part_idx].measures[bar_idx].voices[0];
    voice.events.clear();

    NoteGroup ng1;
    for (const auto& p : first) {
        Note note;
        note.pitch = p;
        ng1.notes.push_back(note);
    }
    ng1.duration = Beat{1, 2};
    voice.events.push_back(Event{EventId{(bar_idx + 1) * 1000 + 1}, Beat::zero(), ng1});

    NoteGroup ng2;
    for (const auto& p : second) {
        Note note;
        note.pitch = p;
        ng2.notes.push_back(note);
    }
    ng2.duration = Beat{1, 2};
    voice.events.push_back(Event{EventId{(bar_idx + 1) * 1000 + 2}, Beat{1, 2}, ng2});
}

}  // anonymous namespace

// =============================================================================
// Harmonic Analysis Tests
// =============================================================================

TEST_CASE("derive_harmonic_layer: I-IV-V-I recognised in C major",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(4, C4, 0);

    place_chord(score, 0, 0, {C4, E4, G4});
    place_chord(score, 0, 1, {F4, A4, C5});
    place_chord(score, 0, 2, {G4, B4, D5});
    place_chord(score, 0, 3, {C4, E4, G4});

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(layer.size() == 4);

    CHECK(layer[0].roman_numeral == "I");
    CHECK(layer[0].function == ScoreHarmonicFunction::Tonic);
    CHECK(layer[1].roman_numeral == "IV");
    CHECK(layer[1].function == ScoreHarmonicFunction::Predominant);
    CHECK(layer[2].roman_numeral == "V");
    CHECK(layer[2].function == ScoreHarmonicFunction::Dominant);
    CHECK(layer[3].roman_numeral == "I");
    CHECK(layer[3].function == ScoreHarmonicFunction::Tonic);
}

TEST_CASE("derive_harmonic_layer: minor key progression",
          "[score-ir][harmonic]") {
    // A minor: root = A4, 0 accidentals, minor mode
    auto score = make_score_with_key(4, A4, 0, true);

    // Am, Dm, E, Am
    place_chord(score, 0, 0, {A4, C5, E5});
    place_chord(score, 0, 1, {D4, F4, A4});
    place_chord(score, 0, 2, {E4, Gs4, B4});
    place_chord(score, 0, 3, {A4, C5, E5});

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(layer.size() == 4);

    CHECK(layer[0].roman_numeral == "i");
    CHECK(layer[1].roman_numeral == "iv");
    // V in minor — the G# makes this a major V
    CHECK(layer[2].roman_numeral == "V");
    CHECK(layer[3].roman_numeral == "i");
}

TEST_CASE("derive_harmonic_layer: cadence detection at score end",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(2, C4, 0);

    // V -> I at the end
    place_chord(score, 0, 0, {G4, B4, D5});
    place_chord(score, 0, 1, {C4, E4, G4});

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(layer.size() == 2);

    // The final annotation should have a cadence tag
    CHECK(layer[1].cadence.has_value());
}

TEST_CASE("derive_harmonic_layer: mid-bar harmonic change",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(1, C4, 0);

    // C triad on beat 1, G triad on beat 3
    place_two_chords(score, 0, 0, {C4, E4, G4}, {G4, B4, D5});

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(layer.size() == 2);

    CHECK(layer[0].roman_numeral == "I");
    CHECK(layer[1].roman_numeral == "V");

    // First chord occupies beats 1-2, second occupies beats 3-4
    CHECK(layer[0].position.bar == 1);
    CHECK(layer[1].position.bar == 1);
    CHECK(layer[1].position.beat > Beat::zero());
}

TEST_CASE("derive_harmonic_layer: unrecognised chord gets Ambiguous",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(1, C4, 0);

    // Cluster chord: C, C#, D — no standard triad recognition
    Note n1; n1.pitch = C4;
    Note n2; n2.pitch = SpelledPitch{0, 1, 4};  // C#4
    Note n3; n3.pitch = D4;

    NoteGroup ng;
    ng.notes = {n1, n2, n3};
    ng.duration = Beat{1, 1};

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events[0].payload = ng;

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(!layer.empty());
    CHECK(layer[0].function == ScoreHarmonicFunction::Ambiguous);
    CHECK(layer[0].confidence < 1.0f);
}

TEST_CASE("derive_harmonic_layer: refresh_stale_regions re-analyses only stale bars",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(4, C4, 0);

    place_chord(score, 0, 0, {C4, E4, G4});   // I
    place_chord(score, 0, 1, {F4, A4, C5});   // IV
    place_chord(score, 0, 2, {G4, B4, D5});   // V
    place_chord(score, 0, 3, {C4, E4, G4});   // I

    // Derive initial layer
    auto initial = derive_harmonic_layer(score);
    REQUIRE(initial.has_value());
    score.harmonic_annotations = *initial;

    // Mutate bar 2: change IV to V
    place_chord(score, 0, 1, {G4, B4, D5});

    // Mark bar 2 as stale
    ScoreRegion stale;
    stale.start = ScoreTime{2, Beat::zero()};
    stale.end = ScoreTime{3, Beat::zero()};
    score.stale_harmonic_regions.push_back(stale);

    auto refresh_result = refresh_stale_regions(score);
    REQUIRE(refresh_result.has_value());

    // Bar 1 should still be I, bar 2 should now be V
    REQUIRE(score.harmonic_annotations.size() >= 2);
    CHECK(score.harmonic_annotations[0].roman_numeral == "I");
    CHECK(score.harmonic_annotations[1].roman_numeral == "V");

    // Stale regions should be cleared
    CHECK(score.stale_harmonic_regions.empty());
}

TEST_CASE("derive_harmonic_layer: empty score produces empty layer",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(4, C4, 0);
    // All rests — no notes

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());
    CHECK(result->empty());
}

TEST_CASE("derive_harmonic_layer: key change mid-score",
          "[score-ir][harmonic]") {
    auto score = make_score_with_key(4, C4, 0);

    // Add key change to G major at bar 3
    KeySignatureEntry key2;
    key2.position = ScoreTime{3, Beat::zero()};
    key2.key.root = G4;
    key2.key.accidentals = 1;
    auto major_scale = find_scale("major");
    if (major_scale) key2.key.mode = *major_scale;
    score.key_map.push_back(key2);

    // Bars 1-2 in C major: I, V
    place_chord(score, 0, 0, {C4, E4, G4});
    place_chord(score, 0, 1, {G4, B4, D5});

    // Bars 3-4 in G major: I, IV (C major triad is now IV in G)
    place_chord(score, 0, 2, {G4, B4, D5});
    place_chord(score, 0, 3, {C4, E4, G4});

    auto result = derive_harmonic_layer(score);
    REQUIRE(result.has_value());

    const auto& layer = *result;
    REQUIRE(layer.size() == 4);

    // In C major: I, V
    CHECK(layer[0].roman_numeral == "I");
    CHECK(layer[1].roman_numeral == "V");

    // In G major: I, IV
    CHECK(layer[2].roman_numeral == "I");
    CHECK(layer[3].roman_numeral == "IV");

    // Key contexts should reflect the change
    CHECK(layer[0].key_context.root.letter == 0);  // C
    CHECK(layer[2].key_context.root.letter == 4);  // G
}
