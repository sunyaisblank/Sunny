/**
 * @file TSMX002A.cpp
 * @brief Unit tests for FMMX002A (MusicXML Score IR compiler)
 *
 * Component: TSMX002A
 * Domain: TS (Test) | Category: MX (MusicXML)
 *
 * Tests: FMMX002A
 */

#include <catch2/catch_test_macros.hpp>

#include "Format/FMMX002A.h"
#include "Format/FMMX001A.h"
#include "Score/SIWF001A.h"
#include "Score/SIMT001A.h"

using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

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

/// Insert a single note into a score at bar 1, voice 0, offset 0
void insert_c4_quarter(Score& score, std::size_t part_idx = 0) {
    auto& voice = score.parts[part_idx].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};  // C4
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 4};
    // Replace the whole-bar rest with a quarter note + 3-quarter rest
    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});
}

}  // namespace

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("FMMX002A: single note C4 round-trips through compile and parse",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();
    insert_c4_quarter(score);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts.size() >= 1);
    REQUIRE(parsed->parts[0].measures.size() >= 1);

    const auto& notes = parsed->parts[0].measures[0].notes;
    REQUIRE(notes.size() >= 1);

    // Find the first non-rest note
    const MusicXmlNote* found = nullptr;
    for (const auto& n : notes) {
        if (!n.is_rest) { found = &n; break; }
    }
    REQUIRE(found != nullptr);
    CHECK(found->pitch.letter == 0);
    CHECK(found->pitch.accidental == 0);
    CHECK(found->pitch.octave == 4);
}

TEST_CASE("FMMX002A: chord C-E-G produces <chord/> on second and third notes",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    // Replace bar 1 voice 0 events with a single NoteGroup of 3 notes
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.duration = Beat{1, 4};

    Note c4; c4.pitch = SpelledPitch{0, 0, 4}; c4.velocity = VelocityValue{std::nullopt, 80};
    Note e4; e4.pitch = SpelledPitch{2, 0, 4}; e4.velocity = VelocityValue{std::nullopt, 80};
    Note g4; g4.pitch = SpelledPitch{4, 0, 4}; g4.velocity = VelocityValue{std::nullopt, 80};
    ng.notes = {c4, e4, g4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());

    // Collect non-rest notes in the first measure
    std::vector<const MusicXmlNote*> pitched;
    for (const auto& n : parsed->parts[0].measures[0].notes) {
        if (!n.is_rest) pitched.push_back(&n);
    }
    REQUIRE(pitched.size() == 3);
    CHECK_FALSE(pitched[0]->is_chord);
    CHECK(pitched[1]->is_chord);
    CHECK(pitched[2]->is_chord);
}

TEST_CASE("FMMX002A: rest-only measure produces <rest/>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();
    // Default score has whole-measure rests; compile as-is
    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());
    REQUIRE(!parsed->parts[0].measures.empty());

    const auto& notes = parsed->parts[0].measures[0].notes;
    REQUIRE(!notes.empty());
    CHECK(notes[0].is_rest);
}

TEST_CASE("FMMX002A: two-voice measure produces <backup> between voices",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();
    PartId pid = score.parts[0].id;

    // Add a second voice to bar 1
    auto av_result = add_voice(score, 1, pid, 1);
    REQUIRE(av_result.has_value());

    // Insert a note in voice 0
    insert_c4_quarter(score);

    // Insert a note in voice 1
    auto& voice1 = score.parts[0].measures[0].voices[1];
    Note e4; e4.pitch = SpelledPitch{2, 0, 4}; e4.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng; ng.notes.push_back(e4); ng.duration = Beat{1, 4};
    voice1.events.clear();
    voice1.events.push_back(Event{EventId{9000010}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice1.events.push_back(Event{EventId{9000011}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<backup>") != std::string::npos);
}

TEST_CASE("FMMX002A: staccato articulation appears in output XML",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};
    note.articulation = ArticulationType::Staccato;
    NoteGroup ng; ng.notes.push_back(note); ng.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("staccato") != std::string::npos);
}

TEST_CASE("FMMX002A: tie forward produces <tie type=\"start\"/>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];

    // First C4 quarter with tie_forward
    Note tied_note;
    tied_note.pitch = SpelledPitch{0, 0, 4};
    tied_note.velocity = VelocityValue{std::nullopt, 80};
    tied_note.tie_forward = true;
    NoteGroup ng1; ng1.notes.push_back(tied_note); ng1.duration = Beat{1, 4};

    // Second C4 quarter (tie destination)
    Note dest_note;
    dest_note.pitch = SpelledPitch{0, 0, 4};
    dest_note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng2; ng2.notes.push_back(dest_note); ng2.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, ng2});
    RestEvent rest{Beat{1, 2}, true};
    voice.events.push_back(Event{EventId{9000003}, Beat{1, 2}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("tie") != std::string::npos);
    CHECK(compiled->xml.find("start") != std::string::npos);
}

TEST_CASE("FMMX002A: hairpin compiles to <wedge> direction",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    score.parts[0].hairpins.push_back(hp);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<wedge") != std::string::npos);
}

TEST_CASE("FMMX002A: tempo emits <sound tempo=\"...\">",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();  // bpm = 120

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("tempo") != std::string::npos);
    CHECK(compiled->xml.find("120") != std::string::npos);
}

TEST_CASE("FMMX002A: key signature change at bar 2 emits <key> with correct <fifths>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    // Add G major at bar 2: root = G (letter 4, accidental 0), fifths = 1
    KeySignatureEntry key2;
    key2.position = ScoreTime{2, Beat::zero()};
    key2.key.root = SpelledPitch{4, 0, 4};  // G
    key2.key.accidentals = 1;
    auto major_scale = find_scale("major");
    if (major_scale) key2.key.mode = *major_scale;
    score.key_map.push_back(key2);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->parts[0].measures.size() >= 2);

    const auto& m2 = parsed->parts[0].measures[1];
    REQUIRE(m2.key_tonic.has_value());
    // G major: letter 4, accidental 0
    CHECK(m2.key_tonic->letter == 4);
    CHECK(m2.key_tonic->accidental == 0);
}

TEST_CASE("FMMX002A: multi-part score produces correct <part> count",
          "[musicxml][format][FMMX002A]") {
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
    spec.parts.push_back(piano_def);

    PartDefinition violin_def;
    violin_def.name = "Violin";
    violin_def.abbreviation = "Vln.";
    violin_def.instrument_type = InstrumentType::Violin;
    violin_def.clef = Clef::Treble;
    violin_def.rendering.midi_channel = 2;
    spec.parts.push_back(violin_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto score = *result;

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());
    CHECK(parsed->parts.size() == 2);
}

TEST_CASE("FMMX002A: enharmonic spelling C# preserved through compile-parse round-trip",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 1, 4};  // C#4
    note.velocity = VelocityValue{std::nullopt, 80};
    NoteGroup ng; ng.notes.push_back(note); ng.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());

    auto parsed = parse_musicxml(compiled->xml);
    REQUIRE(parsed.has_value());

    const MusicXmlNote* found = nullptr;
    for (const auto& n : parsed->parts[0].measures[0].notes) {
        if (!n.is_rest) { found = &n; break; }
    }
    REQUIRE(found != nullptr);
    CHECK(found->pitch.letter == 0);       // C (not D)
    CHECK(found->pitch.accidental == 1);   // sharp (not flat)
}

TEST_CASE("FMMX002A: uncompilable score returns std::unexpected",
          "[musicxml][format][FMMX002A]") {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Invalid";
    score.metadata.total_bars = 0;
    // Empty parts vector violates the compilability precondition

    auto result = compile_score_to_musicxml(score);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("FMMX002A: tuplet NoteGroup produces <time-modification>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};

    // Three triplet eighth notes filling a quarter-note span.
    // A 3:2 eighth-note tuplet occupies 2 * 1/8 = 1/4 of a whole note.
    // Each note's stored duration is the sounding duration: 1/4 / 3 = 1/12.
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 12};
    ng.tuplet_context = TupletContext{TupletId{1}, 3, 2, Beat{1, 8}, std::nullopt};

    NoteGroup ng2 = ng;
    NoteGroup ng3 = ng;

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 12}, ng2});
    voice.events.push_back(Event{EventId{9000003}, Beat{1, 6}, ng3});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000004}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("time-modification") != std::string::npos);
}

TEST_CASE("FMMX002A: transposing instrument emits <transpose>",
          "[musicxml][format][FMMX002A]") {
    ScoreSpec spec;
    spec.title = "Transposing";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition clar_def;
    clar_def.name = "Clarinet";
    clar_def.abbreviation = "Cl.";
    clar_def.instrument_type = InstrumentType::Clarinet;
    clar_def.clef = Clef::Treble;
    clar_def.transposition = -2;  // Bb clarinet: written-to-sounding = -2 semitones
    clar_def.rendering.midi_channel = 1;
    spec.parts.push_back(clar_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto score = *result;

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<transpose") != std::string::npos);
}

TEST_CASE("FMMX002A: dynamic marking emits <dynamics>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = VelocityValue{std::nullopt, 80};
    note.dynamic = DynamicLevel::ff;

    NoteGroup ng; ng.notes.push_back(note); ng.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("dynamics") != std::string::npos);
}

TEST_CASE("FMMX002A: rehearsal mark emits <rehearsal>",
          "[musicxml][format][FMMX002A]") {
    auto score = make_test_score();

    RehearsalMark rm;
    rm.position = ScoreTime{1, Beat::zero()};
    rm.label = "A";
    score.rehearsal_marks.push_back(rm);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("rehearsal") != std::string::npos);
}
