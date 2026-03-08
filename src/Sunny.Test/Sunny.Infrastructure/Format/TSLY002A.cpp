/**
 * @file TSLY002A.cpp
 * @brief Unit tests for FMLY002A (LilyPond Score IR compiler)
 *
 * Component: TSLY002A
 * Domain: TS (Test) | Category: LY (LilyPond)
 *
 * Tests: FMLY002A
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMLY002A.h"

// Include Score IR creation function
#include "Score/SIWF001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

namespace {

Score make_test_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Test Score";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};  // C
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    piano_def.clef = Clef::Treble;
    piano_def.rendering.midi_channel = 1;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    return *result;
}

void insert_note_at(Score& score, SpelledPitch pitch, Beat duration,
                    std::size_t part_idx = 0,
                    std::optional<ArticulationType> art = std::nullopt,
                    bool tie = false) {
    auto& voice = score.parts[part_idx].measures[0].voices[0];
    Note note;
    note.pitch = pitch;
    note.velocity = VelocityValue{std::nullopt, 80};
    note.articulation = art;
    note.tie_forward = tie;
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = duration;
    voice.events.clear();
    voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});
    // Fill remaining measure with rest
    auto remaining = Beat{1, 1} - duration;
    if (remaining > Beat::zero()) {
        RestEvent rest{remaining, true};
        voice.events.push_back(Event{EventId{8000002}, duration, rest});
    }
}

}  // namespace

// =============================================================================
// Single note emission
// =============================================================================

TEST_CASE("FMLY002A: single C4 whole note produces c'1", "[lilypond][compiler]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 1});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("c'1") != std::string::npos);
}

// =============================================================================
// Version header
// =============================================================================

TEST_CASE("FMLY002A: output starts with \\version", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("\\version") != std::string::npos);
    CHECK(ly.find("\\version") < 20);
}

// =============================================================================
// Key and time signature
// =============================================================================

TEST_CASE("FMLY002A: contains \\key c \\major and \\time 4/4",
          "[lilypond][compiler]") {
    auto score = make_test_score();

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("\\key c \\major") != std::string::npos);
    CHECK(ly.find("\\time 4/4") != std::string::npos);
}

// =============================================================================
// Staccato articulation
// =============================================================================

TEST_CASE("FMLY002A: staccato produces -.", "[lilypond][compiler]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 4}, 0,
                   ArticulationType::Staccato);

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("-.") != std::string::npos);
}

// =============================================================================
// Tie
// =============================================================================

TEST_CASE("FMLY002A: tie produces ~", "[lilypond][compiler]") {
    auto score = make_test_score();

    // Bar 1: tied C4 whole note (last event, so tie resolves across bar)
    {
        auto& voice = score.parts[0].measures[0].voices[0];
        voice.events.clear();

        Note note;
        note.pitch = SpelledPitch{0, 0, 4};
        note.velocity = VelocityValue{std::nullopt, 80};
        note.tie_forward = true;
        NoteGroup ng;
        ng.notes.push_back(note);
        ng.duration = Beat{1, 1};
        voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});
    }

    // Bar 2: matching C4 whole note (tie target)
    {
        auto& voice = score.parts[0].measures[1].voices[0];
        voice.events.clear();

        Note note;
        note.pitch = SpelledPitch{0, 0, 4};
        note.velocity = VelocityValue{std::nullopt, 80};
        NoteGroup ng;
        ng.notes.push_back(note);
        ng.duration = Beat{1, 1};
        voice.events.push_back(Event{EventId{8000003}, Beat::zero(), ng});
    }

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("~") != std::string::npos);
}

// =============================================================================
// Chord syntax
// =============================================================================

TEST_CASE("FMLY002A: chord produces angle-bracket syntax", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng;
    Note c4; c4.pitch = SpelledPitch{0, 0, 4};
    c4.velocity = VelocityValue{std::nullopt, 80};
    Note e4; e4.pitch = SpelledPitch{2, 0, 4};
    e4.velocity = VelocityValue{std::nullopt, 80};
    Note g4; g4.pitch = SpelledPitch{4, 0, 4};
    g4.velocity = VelocityValue{std::nullopt, 80};
    ng.notes.push_back(c4);
    ng.notes.push_back(e4);
    ng.notes.push_back(g4);
    ng.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});

    // Fill remaining three quarters with a rest
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{8000002}, Beat{1, 4}, rest});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("<") != std::string::npos);
    CHECK(ly.find(">") != std::string::npos);
    CHECK(ly.find("c'") != std::string::npos);
    CHECK(ly.find("e'") != std::string::npos);
    CHECK(ly.find("g'") != std::string::npos);
}

// =============================================================================
// Rest
// =============================================================================

TEST_CASE("FMLY002A: rest produces r or R with duration", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    // Default score has whole-measure rests (R1)
    const std::string& ly = result->ly;
    CHECK(ly.find("R1") != std::string::npos);
}

// =============================================================================
// Polyphonic voices
// =============================================================================

TEST_CASE("FMLY002A: two voices produce polyphony syntax", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& measure = score.parts[0].measures[0];

    // Voice 0: C4 whole note
    {
        auto& v0 = measure.voices[0];
        v0.events.clear();
        NoteGroup ng;
        Note c4; c4.pitch = SpelledPitch{0, 0, 4};
        c4.velocity = VelocityValue{std::nullopt, 80};
        ng.notes.push_back(c4);
        ng.duration = Beat{1, 1};
        v0.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});
    }

    // Voice 1: E4 whole note
    {
        Voice v1;
        v1.voice_index = 1;
        NoteGroup ng;
        Note e4; e4.pitch = SpelledPitch{2, 0, 4};
        e4.velocity = VelocityValue{std::nullopt, 80};
        ng.notes.push_back(e4);
        ng.duration = Beat{1, 1};
        v1.events.push_back(Event{EventId{8000003}, Beat::zero(), ng});
        measure.voices.push_back(v1);
    }

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("<<") != std::string::npos);
    CHECK(ly.find("\\\\") != std::string::npos);
}

// =============================================================================
// Hairpin crescendo
// =============================================================================

TEST_CASE("FMLY002A: hairpin crescendo produces \\<", "[lilypond][compiler]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 1});

    // Add a crescendo hairpin spanning bar 1
    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    hp.target = std::nullopt;
    score.parts[0].hairpins.push_back(hp);

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("\\<") != std::string::npos);
}

// =============================================================================
// Multi-part produces multiple Staff blocks
// =============================================================================

TEST_CASE("FMLY002A: multi-part produces multiple \\new Staff blocks",
          "[lilypond][compiler]") {
    ScoreSpec spec;
    spec.title = "Two Parts";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    piano_def.clef = Clef::Treble;
    piano_def.rendering.midi_channel = 1;

    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.clef = Clef::Treble;
    violin_def.rendering.midi_channel = 2;

    spec.parts.push_back(piano_def);
    spec.parts.push_back(violin_def);

    auto score_result = create_score(spec);
    REQUIRE(score_result.has_value());
    auto& score = *score_result;

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    std::size_t count = 0;
    std::size_t pos = 0;
    while ((pos = ly.find("\\new Staff", pos)) != std::string::npos) {
        ++count;
        pos += 10;
    }
    CHECK(count >= 2);
}

// =============================================================================
// Dynamic ff
// =============================================================================

TEST_CASE("FMLY002A: dynamic ff produces \\ff", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};
    note.dynamic = DynamicLevel::ff;
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("\\ff") != std::string::npos);
}

// =============================================================================
// Uncompilable score returns unexpected
// =============================================================================

TEST_CASE("FMLY002A: uncompilable score returns unexpected",
          "[lilypond][compiler]") {
    auto score = make_test_score();

    // Clear parts to violate the compilability precondition
    score.parts.clear();

    auto result = compile_score_to_lilypond(score);
    REQUIRE_FALSE(result.has_value());
}

// =============================================================================
// Non-representable duration produces diagnostic
// =============================================================================

TEST_CASE("FMLY002A: non-representable duration produces diagnostic",
          "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    // Insert a note with duration 5/8 (not a standard LilyPond duration)
    NoteGroup ng;
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};
    ng.notes.push_back(note);
    ng.duration = Beat{5, 8};
    voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});

    // Fill remaining measure: 1/1 - 5/8 = 3/8 rest
    RestEvent rest{Beat{3, 8}, true};
    voice.events.push_back(Event{EventId{8000002}, Beat{5, 8}, rest});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    CHECK(result->report.diagnostics.size() > 0);
}
