/**
 * @file score_to_musicxml_test.cpp
 * @brief MusicXML Score IR compiler unit tests
 *
 *
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/post_tonal/twelve_tone.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>
#include <sunny/infrastructure/formats/score_to_musicxml.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_test_score(std::uint32_t bars = 4) {
    ScoreSpec spec;
    spec.title = "Test Score";
    spec.total_bars = bars;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4}; // C
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
    note.pitch = SpelledPitch{0, 0, 4}; // C4
    note.velocity = {};
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 4};
    // Replace the whole-bar rest with a quarter note + 3-quarter rest
    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});
}

std::size_t count_occurrences(const std::string& text, const std::string& fragment) {
    std::size_t count = 0;
    std::size_t position = 0;
    while ((position = text.find(fragment, position)) != std::string::npos) {
        ++count;
        position += fragment.size();
    }
    return count;
}

void populate_unrepresented_analysis_state(Score& score) {
    score.section_map.push_back(ScoreSection{SectionId{1},
                                             "Whole work",
                                             SCORE_START,
                                             ScoreTime{5, Beat::zero()},
                                             {},
                                             FormFunction::Expository});

    HarmonicAnnotation harmony{};
    harmony.position = SCORE_START;
    harmony.duration = Beat{1, 1};
    harmony.chord = ChordVoicing{{60, 64, 67}, 0, "major", 0};
    harmony.key_context = score.key_map.front().key;
    harmony.roman_numeral = "I";
    harmony.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(harmony);

    OrchestrationAnnotation orchestration{};
    orchestration.part_id = score.parts.front().id;
    orchestration.start = SCORE_START;
    orchestration.end = ScoreTime{2, Beat::zero()};
    orchestration.role = TexturalRole::Melody;
    orchestration.texture = TextureType::Monophonic;
    orchestration.dynamic_balance = DynamicBalance::Foreground;
    score.orchestration_annotations.push_back(orchestration);
    score.stale_harmonic_regions.push_back(
        ScoreRegion{SCORE_START, ScoreTime{2, Beat::zero()}, {}});
    score.stale_orchestration_regions.push_back(
        ScoreRegion{ScoreTime{2, Beat::zero()}, ScoreTime{3, Beat::zero()}, {}});

    std::array<PitchClass, 12> row_values = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
    const auto row = make_tone_row(row_values);
    REQUIRE(row.has_value());
    score.tone_row = *row;

    constexpr std::array<SpelledPitch, 12> pitches = {SpelledPitch{0, 0, 4},
                                                      SpelledPitch{0, 1, 4},
                                                      SpelledPitch{1, 0, 4},
                                                      SpelledPitch{2, -1, 4},
                                                      SpelledPitch{2, 0, 4},
                                                      SpelledPitch{3, 0, 4},
                                                      SpelledPitch{3, 1, 4},
                                                      SpelledPitch{4, 0, 4},
                                                      SpelledPitch{5, -1, 4},
                                                      SpelledPitch{5, 0, 4},
                                                      SpelledPitch{6, -1, 4},
                                                      SpelledPitch{6, 0, 4}};
    auto& voice = score.parts.front().measures.front().voices.front();
    voice.events.clear();
    for (std::size_t i = 0; i < pitches.size(); ++i) {
        Note note{};
        note.pitch = pitches[i];
        NoteGroup group{};
        group.notes.push_back(note);
        group.duration = Beat{1, 12};
        voice.events.push_back(
            Event{EventId{9100000 + i}, Beat{static_cast<std::int64_t>(i), 12}, group});
    }
}

} // namespace

// =============================================================================
// Tests
// =============================================================================

TEST_CASE("MusicXML reports non-standard Score tuning instead of silently dropping it",
          "[musicxml][format][tuning]") {
    auto score = make_test_score();
    score.tuning.name = "custom";
    score.tuning.cents_from_reference[60] = -901.25;
    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->report.tuning_definitions_requested == 1);
    CHECK(compiled->report.tuning_definitions_written == 0);
    CHECK(compiled->report.has_drops());
}

TEST_CASE("MusicXML reports every unrepresented top-level analysis layer",
          "[musicxml][format][analysis-residual]") {
    auto score = make_test_score();
    populate_unrepresented_analysis_state(score);

    const auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->report.diagnostics.size() == 6);
    CHECK_FALSE(compiled->report.has_drops());
    CHECK(compiled->report.has_residuals());
    CHECK(compiled->report.diagnostics[0].message.find("SectionMap") != std::string::npos);
    CHECK(compiled->report.diagnostics[1].message.find("harmonic-analysis") != std::string::npos);
    CHECK(compiled->report.diagnostics[2].message.find("orchestration-analysis") !=
          std::string::npos);
    CHECK(compiled->report.diagnostics[3].message.find("twelve-tone row") != std::string::npos);
    CHECK(compiled->report.diagnostics[4].message.find("Stale harmonic") != std::string::npos);
    CHECK(compiled->report.diagnostics[5].message.find("Stale orchestration") != std::string::npos);
}

TEST_CASE("MusicXML reports non-default release velocity as a notation residual",
          "[musicxml][format][release-velocity]") {
    auto score = make_test_score();
    insert_c4_quarter(score);
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
        .notes[0]
        .release_velocity = 23;

    const auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->report.diagnostics.size() == 1);
    CHECK_FALSE(compiled->report.has_drops());
    CHECK(compiled->report.has_residuals());
    CHECK(compiled->report.diagnostics[0].message.find("release velocity") != std::string::npos);
    CHECK(compiled->report.diagnostics[0].location == ScoreTime{1, Beat::zero()});
    CHECK(compiled->report.diagnostics[0].part == score.parts[0].id);
}

TEST_CASE("MusicXML distinguishes semantic and numeric-only attack velocity",
          "[musicxml][format][attack-velocity][dynamic]") {
    SECTION("semantic VelocityValue.written becomes a dynamic direction") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity = VelocityValue{DynamicLevel::ff, 73};

        const auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled);
        CHECK(compiled->xml.find("<ff") != std::string::npos);
        CHECK(compiled->report.diagnostics.empty());
    }

    SECTION("numeric-only attack intensity remains an explicit residual") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity.value = 73;

        const auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled);
        REQUIRE(compiled->report.diagnostics.size() == 1);
        CHECK(compiled->report.diagnostics[0].message.find("numeric note attack velocity") !=
              std::string::npos);
        CHECK(compiled->report.diagnostics[0].location == ScoreTime{1, Beat::zero()});
        CHECK(compiled->report.diagnostics[0].part == score.parts[0].id);
    }
}

TEST_CASE("single note C4 round-trips through compile and parse", "[musicxml][format]") {
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
        if (!n.is_rest) {
            found = &n;
            break;
        }
    }
    REQUIRE(found != nullptr);
    CHECK(found->pitch.letter == 0);
    CHECK(found->pitch.accidental == 0);
    CHECK(found->pitch.octave == 4);
}

TEST_CASE("chord C-E-G produces <chord/> on second and third notes", "[musicxml][format]") {
    auto score = make_test_score();

    // Replace bar 1 voice 0 events with a single NoteGroup of 3 notes
    auto& voice = score.parts[0].measures[0].voices[0];
    NoteGroup ng;
    ng.duration = Beat{1, 4};

    Note c4;
    c4.pitch = SpelledPitch{0, 0, 4};
    c4.velocity = {};
    Note e4;
    e4.pitch = SpelledPitch{2, 0, 4};
    e4.velocity = {};
    Note g4;
    g4.pitch = SpelledPitch{4, 0, 4};
    g4.velocity = {};
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

TEST_CASE("rest-only measure produces <rest/>", "[musicxml][format]") {
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

TEST_CASE("two-voice measure produces <backup> between voices", "[musicxml][format]") {
    auto score = make_test_score();
    PartId pid = score.parts[0].id;

    // Add a second voice to bar 1
    auto av_result = add_voice(score, 1, pid, 1);
    REQUIRE(av_result.has_value());

    // Insert a note in voice 0
    insert_c4_quarter(score);

    // Insert a note in voice 1
    auto& voice1 = score.parts[0].measures[0].voices[1];
    Note e4;
    e4.pitch = SpelledPitch{2, 0, 4};
    e4.velocity = {};
    NoteGroup ng;
    ng.notes.push_back(e4);
    ng.duration = Beat{1, 4};
    voice1.events.clear();
    voice1.events.push_back(Event{EventId{9000010}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice1.events.push_back(Event{EventId{9000011}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<backup>") != std::string::npos);
}

TEST_CASE("staccato articulation appears in output XML", "[musicxml][format]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};
    note.articulation = ArticulationType::Staccato;
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("staccato") != std::string::npos);
}

TEST_CASE("first-class Ornament renders and takes precedence over legacy trill articulation",
          "[musicxml][format][ornament]") {
    auto score = make_test_score();
    insert_c4_quarter(score);

    auto& note =
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
    note.articulation = ArticulationType::Trill;
    note.ornament = Ornament{OrnamentType::Trill, 1, std::int8_t{1}, ArpeggioDirection::None};

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<trill-mark trill-step=\"half\"") != std::string::npos);
    CHECK(compiled->xml.find("<accidental-mark>sharp</accidental-mark>") != std::string::npos);
    const auto first = compiled->xml.find("<trill-mark");
    REQUIRE(first != std::string::npos);
    CHECK(compiled->xml.find("<trill-mark", first + 1) == std::string::npos);
    CHECK(compiled->report.diagnostics.empty());
}

TEST_CASE("Ornament variants use native MusicXML elements", "[musicxml][format][ornament]") {
    struct Case {
        OrnamentType type;
        ArpeggioDirection direction;
        const char* fragment;
    };
    constexpr Case cases[] = {
        {OrnamentType::Mordent, ArpeggioDirection::None, "<mordent"},
        {OrnamentType::InvertedMordent, ArpeggioDirection::None, "<inverted-mordent"},
        {OrnamentType::Turn, ArpeggioDirection::None, "<turn"},
        {OrnamentType::InvertedTurn, ArpeggioDirection::None, "<inverted-turn"},
        {OrnamentType::Shake, ArpeggioDirection::None, "<shake"},
        {OrnamentType::Arpeggio, ArpeggioDirection::Down, "<arpeggiate direction=\"down\""},
    };

    for (const auto& test_case : cases) {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.ornament = Ornament{test_case.type, 0, std::nullopt, test_case.direction};

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find(test_case.fragment) != std::string::npos);
    }
}

TEST_CASE("TechnicalDirection renders native fields and diagnoses extension fallbacks",
          "[musicxml][format][technical]") {
    auto score = make_test_score();
    insert_c4_quarter(score);
    auto& note =
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];

    TechnicalDirection fingering{};
    fingering.type = TechnicalDirection::Type::Fingering;
    fingering.fingering = {1, 3};
    note.technical.push_back(fingering);
    TechnicalDirection string_number{};
    string_number.type = TechnicalDirection::Type::StringNumber;
    string_number.number = 2;
    note.technical.push_back(string_number);
    TechnicalDirection bend{};
    bend.type = TechnicalDirection::Type::Bend;
    bend.bend_cents = 150;
    note.technical.push_back(bend);
    TechnicalDirection breath{};
    breath.type = TechnicalDirection::Type::BreathMark;
    note.technical.push_back(breath);
    TechnicalDirection caesura{};
    caesura.type = TechnicalDirection::Type::Caesura;
    note.technical.push_back(caesura);
    TechnicalDirection position{};
    position.type = TechnicalDirection::Type::Position;
    position.number = 3;
    note.technical.push_back(position);
    TechnicalDirection hammer_on{};
    hammer_on.type = TechnicalDirection::Type::HammerOn;
    note.technical.push_back(hammer_on);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<fingering>1</fingering>") != std::string::npos);
    CHECK(compiled->xml.find("<fingering>3</fingering>") != std::string::npos);
    CHECK(compiled->xml.find("<string>2</string>") != std::string::npos);
    CHECK(compiled->xml.find("<bend-alter>1.5</bend-alter>") != std::string::npos);
    CHECK(compiled->xml.find("<breath-mark") != std::string::npos);
    CHECK(compiled->xml.find("<caesura") != std::string::npos);
    CHECK(compiled->xml.find("<other-technical>position 3</other-technical>") != std::string::npos);
    CHECK(compiled->xml.find("<other-technical>hammer-on</other-technical>") != std::string::npos);
    CHECK(compiled->report.diagnostics.size() == 2);
}

TEST_CASE("tie forward produces <tie type=\"start\" />", "[musicxml][format]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];

    // First C4 quarter with tie_forward
    Note tied_note;
    tied_note.pitch = SpelledPitch{0, 0, 4};
    tied_note.velocity = {};
    tied_note.tie_forward = true;
    NoteGroup ng1;
    ng1.notes.push_back(tied_note);
    ng1.duration = Beat{1, 4};

    // Second C4 quarter (tie destination)
    Note dest_note;
    dest_note.pitch = SpelledPitch{0, 0, 4};
    dest_note.velocity = {};
    NoteGroup ng2;
    ng2.notes.push_back(dest_note);
    ng2.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng1});
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, ng2});
    RestEvent rest{Beat{1, 2}, true};
    voice.events.push_back(Event{EventId{9000003}, Beat{1, 2}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(count_occurrences(compiled->xml, "<tie type=\"start\"") == 1);
    CHECK(count_occurrences(compiled->xml, "<tied type=\"start\"") == 1);
    CHECK(count_occurrences(compiled->xml, "<tie type=\"stop\"") == 1);
    CHECK(count_occurrences(compiled->xml, "<tied type=\"stop\"") == 1);

    const auto duration = compiled->xml.find("<duration>");
    const auto sound_tie = compiled->xml.find("<tie type=\"start\"");
    const auto voice_element = compiled->xml.find("<voice>", duration);
    REQUIRE(duration != std::string::npos);
    REQUIRE(sound_tie != std::string::npos);
    REQUIRE(voice_element != std::string::npos);
    CHECK(duration < sound_tie);
    CHECK(sound_tie < voice_element);
}

TEST_CASE("grace type, lyrics, and notehead metadata reach MusicXML",
          "[musicxml][format][note-metadata]") {
    SECTION("acciaccatura is explicitly slashed") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.grace = GraceType::Acciaccatura;

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<grace slash=\"yes\"") != std::string::npos);
    }

    SECTION("appoggiatura is explicitly unslashed") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.grace = GraceType::Appoggiatura;

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<grace slash=\"no\"") != std::string::npos);
    }

    SECTION("lyric text and native notehead shape are preserved") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.lyrics.push_back(LyricSyllable{"la & <", 1, LyricSyllabic::Single, false});
        note.notation_head = NoteHeadType::Diamond;

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<notehead>diamond</notehead>") != std::string::npos);
        CHECK(compiled->xml.find("<lyric number=\"1\">") != std::string::npos);
        CHECK(compiled->xml.find("<syllabic>single</syllabic>") != std::string::npos);
        CHECK(compiled->xml.find("<text>la &amp; &lt;</text>") != std::string::npos);
    }

    SECTION("Cue means sounding cue size rather than a silent cue note") {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.notation_head = NoteHeadType::Cue;

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<type size=\"cue\">quarter</type>") != std::string::npos);
        CHECK(compiled->xml.find("<cue") == std::string::npos);
    }
}

TEST_CASE("structured verses and melisma project to MusicXML lyric state",
          "[musicxml][format][lyric]") {
    auto score = make_test_score();
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    for (std::uint64_t i = 0; i < 4; ++i) {
        NoteGroup group;
        group.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{}});
        group.duration = Beat{1, 4};
        voice.events.push_back(
            Event{EventId{9850000 + i}, Beat{static_cast<std::int64_t>(i), 4}, group});
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
    REQUIRE(is_compilable(score));

    const auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<lyric number=\"1\">") != std::string::npos);
    CHECK(compiled->xml.find("<syllabic>begin</syllabic>") != std::string::npos);
    CHECK(compiled->xml.find("<syllabic>end</syllabic>") != std::string::npos);
    CHECK(compiled->xml.find("<lyric number=\"2\">") != std::string::npos);
    CHECK(compiled->xml.find("<extend type=\"start\"") != std::string::npos);
    CHECK(compiled->xml.find("<extend type=\"stop\"") != std::string::npos);
    CHECK(compiled->report.diagnostics.empty());
}

TEST_CASE("legacy articulation variants are rendered or explicitly preserved",
          "[musicxml][format][articulation]") {
    struct Case {
        ArticulationType type;
        const char* fragment;
        std::size_t diagnostics;
    };
    constexpr Case cases[] = {
        {ArticulationType::Sforzando, "<sfz", 0},
        {ArticulationType::ForzandoPiano, "<fp", 0},
        {ArticulationType::InvertedMordent, "<inverted-mordent", 0},
        {ArticulationType::InvertedTurn, "<inverted-turn", 0},
        {ArticulationType::Tremolo, "<tremolo type=\"unmeasured\">0</tremolo>", 0},
        {ArticulationType::OpenString, "<open-string", 0},
        {ArticulationType::Stopped, "<stopped", 0},
        {ArticulationType::GlissandoStart, "<glissando type=\"start\"", 0},
        {ArticulationType::GlissandoEnd, "<glissando type=\"stop\"", 0},
        {ArticulationType::Muted, "<other-technical>muted</other-technical>", 1},
        {ArticulationType::BendUp, "<other-technical>bend-up</other-technical>", 1},
        {ArticulationType::BendDown, "<other-technical>bend-down</other-technical>", 1},
    };

    for (const auto& test_case : cases) {
        auto score = make_test_score();
        insert_c4_quarter(score);
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        if (test_case.type == ArticulationType::GlissandoStart ||
            test_case.type == ArticulationType::GlissandoEnd) {
            note.articulation = ArticulationType::GlissandoStart;
            Note endpoint_note;
            endpoint_note.pitch = SpelledPitch{1, 0, 4};
            endpoint_note.velocity = {};
            endpoint_note.articulation = ArticulationType::GlissandoEnd;
            NoteGroup endpoint;
            endpoint.notes.push_back(endpoint_note);
            endpoint.duration = Beat{3, 4};
            score.parts[0].measures[0].voices[0].events[1].payload = endpoint;
        } else {
            note.articulation = test_case.type;
        }

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find(test_case.fragment) != std::string::npos);
        CHECK(compiled->report.diagnostics.size() == test_case.diagnostics);
    }
}

TEST_CASE("hairpin compiles to <wedge> direction", "[musicxml][format]") {
    auto score = make_test_score();

    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    hp.target = DynamicLevel::f;
    score.parts[0].hairpins.push_back(hp);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<wedge") != std::string::npos);
    CHECK(compiled->xml.find("<f />") != std::string::npos);
}

TEST_CASE("global and Voice point events retain exact within-measure time",
          "[musicxml][format][timing]") {
    SECTION("point-only denominators participate in divisions computation") {
        auto score = make_test_score(1);
        auto tempo = score.tempo_map.front();
        tempo.position = ScoreTime{1, Beat{1, 7}};
        tempo.bpm = PositiveRational{90, 1};
        score.tempo_map.push_back(tempo);

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<divisions>7</divisions>") != std::string::npos);
        CHECK(compiled->xml.find("<offset sound=\"yes\">4</offset>") != std::string::npos);
    }

    SECTION("tempo and hairpin boundaries use absolute offsets from measure start") {
        auto score = make_test_score(1);
        auto tempo = score.tempo_map.front();
        tempo.position = ScoreTime{1, Beat{1, 2}};
        tempo.bpm = PositiveRational{90, 1};
        score.tempo_map.push_back(tempo);

        Hairpin hairpin;
        hairpin.start = ScoreTime{1, Beat{1, 2}};
        hairpin.end = ScoreTime{1, Beat{3, 4}};
        hairpin.type = HairpinType::Crescendo;
        hairpin.target = DynamicLevel::f;
        score.parts[0].hairpins.push_back(hairpin);

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(count_occurrences(compiled->xml, "<offset sound=\"yes\">2</offset>") == 2);
        CHECK(count_occurrences(compiled->xml, "<offset sound=\"yes\">3</offset>") == 2);
        CHECK(count_occurrences(compiled->xml, "<wedge") == 2);
        CHECK(compiled->xml.find("<f />") != std::string::npos);
    }

    SECTION("a next-bar-zero score endpoint remains at the preceding measure end") {
        auto score = make_test_score(1);

        Hairpin hairpin;
        hairpin.start = ScoreTime{1, Beat{1, 2}};
        hairpin.end = ScoreTime{2, Beat::zero()};
        hairpin.type = HairpinType::Diminuendo;
        hairpin.target = DynamicLevel::p;
        score.parts[0].hairpins.push_back(hairpin);

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(count_occurrences(compiled->xml, "<offset sound=\"yes\">4</offset>") == 2);
        CHECK(compiled->xml.find("<wedge type=\"stop\"") != std::string::npos);
        CHECK(compiled->xml.find("<p />") != std::string::npos);
    }

    SECTION("point Direction and ChordSymbol compensate for an already advanced cursor") {
        auto score = make_test_score(1);
        auto& voice = score.parts[0].measures[0].voices[0];

        ScoreDirection direction;
        direction.type = DirectionType::Text;
        direction.text = "inside rest";
        voice.events.push_back(Event{EventId{9000101}, Beat{1, 2}, direction});

        ChordSymbolEvent chord;
        chord.root = SpelledPitch{4, 0, 4};
        chord.quality = "dominant";
        chord.roman = "V7";
        chord.extensions = {"b9"};
        voice.events.push_back(Event{EventId{9000102}, Beat{1, 2}, chord});

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(count_occurrences(compiled->xml, "<offset sound=\"yes\">-2</offset>") == 1);
        CHECK(count_occurrences(compiled->xml, "<offset>-2</offset>") == 1);
        CHECK(compiled->xml.find("<kind text=\"V7 b9\">dominant</kind>") != std::string::npos);
        CHECK(compiled->report.diagnostics.size() == 1);
    }

    SECTION("typed harmony projects to MusicXML numeral inversion and degree semantics") {
        auto score = make_test_score(1);
        auto& voice = score.parts[0].measures[0].voices[0];

        ChordSymbolEvent chord;
        chord.root = SpelledPitch{4, 0, 4}; // G = V in C major
        chord.quality = "dominant";
        chord.bass = SpelledPitch{6, 0, 3};
        chord.roman = "V65";
        chord.numeral = ChordNumeral{5, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
        chord.inversion = 1;
        chord.degrees = {{9, -1, ChordDegreeType::Add},
                         {5, 1, ChordDegreeType::Alter},
                         {3, 0, ChordDegreeType::Subtract}};
        voice.events.push_back(Event{EventId{9000105}, Beat{1, 2}, chord});

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<numeral-root text=\"V65\">5</numeral-root>") !=
              std::string::npos);
        CHECK(compiled->xml.find("<numeral-fifths>0</numeral-fifths>") != std::string::npos);
        CHECK(compiled->xml.find("<numeral-mode>major</numeral-mode>") != std::string::npos);
        CHECK(compiled->xml.find("<kind text=\"\">dominant</kind>") != std::string::npos);
        CHECK(compiled->xml.find("<inversion>1</inversion>") != std::string::npos);
        CHECK(compiled->xml.find("<bass-step>B</bass-step>") != std::string::npos);
        CHECK(compiled->xml.find("<degree-value>9</degree-value>") != std::string::npos);
        CHECK(compiled->xml.find("<degree-alter>-1</degree-alter>") != std::string::npos);
        CHECK(compiled->xml.find("<degree-type>add</degree-type>") != std::string::npos);
        CHECK(compiled->xml.find("<degree-type>alter</degree-type>") != std::string::npos);
        CHECK(compiled->xml.find("<degree-type>subtract</degree-type>") != std::string::npos);
        CHECK(compiled->report.diagnostics.empty());

        const auto parsed = parse_musicxml(compiled->xml);
        REQUIRE(parsed.has_value());
        REQUIRE(parsed->parts[0].measures[0].harmonies.size() == 1);
        const auto& restored = parsed->parts[0].measures[0].harmonies[0];
        CHECK(restored.offset == Beat{1, 2});
        CHECK(restored.symbol.root == chord.root);
        CHECK(restored.symbol.bass == chord.bass);
        CHECK(restored.symbol.roman == chord.roman);
        CHECK(restored.symbol.numeral == chord.numeral);
        CHECK(restored.symbol.inversion == chord.inversion);
        CHECK(restored.symbol.degrees == chord.degrees);
    }

    SECTION("all closed numeral modes and an altered root use their exact vocabulary") {
        struct ModeCase {
            ChordNumeralMode mode;
            const char* expected;
        };
        constexpr std::array cases{
            ModeCase{ChordNumeralMode::Major, "major"},
            ModeCase{ChordNumeralMode::Minor, "minor"},
            ModeCase{ChordNumeralMode::NaturalMinor, "natural minor"},
            ModeCase{ChordNumeralMode::MelodicMinor, "melodic minor"},
            ModeCase{ChordNumeralMode::HarmonicMinor, "harmonic minor"},
        };
        for (const auto& item : cases) {
            auto score = make_test_score(1);
            ChordSymbolEvent chord;
            chord.root = item.mode == ChordNumeralMode::Major ? SpelledPitch{0, 0, 4}
                                                              : SpelledPitch{5, 0, 4};
            chord.quality = item.mode == ChordNumeralMode::Major ? "major" : "minor";
            chord.numeral = ChordNumeral{1, 0, ChordNumeralKey{0, item.mode}};
            score.parts[0].measures[0].voices[0].events.push_back(Event{
                EventId{9000200 + static_cast<std::uint64_t>(item.mode)}, Beat::zero(), chord});

            const auto compiled = compile_score_to_musicxml(score);
            REQUIRE(compiled.has_value());
            const std::string expected =
                std::string{"<numeral-mode>"} + item.expected + "</numeral-mode>";
            CAPTURE(expected);
            CHECK(compiled->xml.find(expected) != std::string::npos);
        }

        auto altered_score = make_test_score(1);
        ChordSymbolEvent altered;
        altered.root = SpelledPitch{5, 1, 4};
        altered.quality = "diminished";
        altered.numeral = ChordNumeral{1, 1, ChordNumeralKey{0, ChordNumeralMode::HarmonicMinor}};
        altered_score.parts[0].measures[0].voices[0].events.push_back(
            Event{EventId{9000210}, Beat::zero(), altered});
        const auto compiled = compile_score_to_musicxml(altered_score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<numeral-alter>1</numeral-alter>") != std::string::npos);
    }

    SECTION("typed numeral keeps unknown quality beside diagnosed free-form extensions") {
        auto score = make_test_score(1);
        ChordSymbolEvent chord;
        chord.root = SpelledPitch{0, 0, 4};
        chord.quality = "quartal";
        chord.extensions = {"add13"};
        chord.numeral = ChordNumeral{1, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
        score.parts[0].measures[0].voices[0].events.push_back(
            Event{EventId{9000211}, Beat::zero(), chord});

        const auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<kind text=\"quartal add13\">other</kind>") != std::string::npos);
        REQUIRE(compiled->report.diagnostics.size() == 1);
        CHECK(compiled->report.diagnostics[0].message.find("free-form extensions") !=
              std::string::npos);
    }

    SECTION("cursor-bound clef and repeat events temporarily reposition and restore") {
        auto score = make_test_score(1);
        auto& voice = score.parts[0].measures[0].voices[0];

        ScoreDirection clef;
        clef.type = DirectionType::ClefChange;
        clef.new_clef = Clef::Bass;
        voice.events.push_back(Event{EventId{9000103}, Beat{1, 2}, clef});
        ScoreDirection repeat;
        repeat.type = DirectionType::RepeatStart;
        voice.events.push_back(Event{EventId{9000104}, Beat{1, 2}, repeat});

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(count_occurrences(compiled->xml, "<backup>") == 2);
        CHECK(count_occurrences(compiled->xml, "<forward>") == 2);
        CHECK(compiled->xml.find("<sign>F</sign>") != std::string::npos);
        CHECK(compiled->xml.find("<repeat direction=\"forward\"") != std::string::npos);
    }
}

TEST_CASE("PartDirective boundaries remain visible at exact MusicXML time",
          "[musicxml][format][directive]") {
    auto score = make_test_score(1);
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::Divisi, 3});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<words>divisi a 3</words>") != std::string::npos);
    CHECK(compiled->xml.find("<words>end divisi a 3</words>") != std::string::npos);
    CHECK(compiled->xml.find("<offset>1</offset>") != std::string::npos);
    CHECK(compiled->xml.find("<offset>3</offset>") != std::string::npos);
}

TEST_CASE("grace allocation advances the MusicXML cursor without adding note duration",
          "[musicxml][format][grace][timing]") {
    auto score = make_test_score(1);
    insert_c4_quarter(score);
    auto& note =
        std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
    note.grace = GraceType::Acciaccatura;

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<grace slash=\"yes\"") != std::string::npos);
    CHECK(count_occurrences(compiled->xml, "<forward>") == 1);
    CHECK(compiled->xml.find("<duration>1</duration>") != std::string::npos);
}

TEST_CASE("OttavaStart translates semitone intent to MusicXML octave-shift size and direction",
          "[musicxml][format][direction]") {
    struct Case {
        std::int8_t semitones;
        const char* fragment;
    };
    constexpr Case cases[] = {
        {12, "<octave-shift type=\"down\" size=\"8\""},
        {-12, "<octave-shift type=\"up\" size=\"8\""},
        {24, "<octave-shift type=\"down\" size=\"15\""},
        {-24, "<octave-shift type=\"up\" size=\"15\""},
    };

    for (const auto& test_case : cases) {
        auto score = make_test_score(1);
        ScoreDirection ottava;
        ottava.type = DirectionType::OttavaStart;
        ottava.ottava_shift = test_case.semitones;
        score.parts[0].measures[0].voices[0].events.push_back(
            Event{EventId{9000105}, Beat{1, 2}, ottava});
        ScoreDirection ottava_end;
        ottava_end.type = DirectionType::OttavaEnd;
        score.parts[0].measures[0].voices[0].events.push_back(
            Event{EventId{9000106}, Beat{3, 4}, ottava_end});

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find(test_case.fragment) != std::string::npos);
        CHECK(compiled->report.diagnostics.empty());
    }
}

TEST_CASE("chained slur emits incoming stop before outgoing start",
          "[musicxml][format][span-endpoint]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    for (std::int64_t i = 0; i < 4; ++i) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(i), 0, 4};
        note.velocity = {};
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 4};
        group.slur_start = i == 0 || i == 1;
        group.slur_end = i == 1 || i == 2;
        voice.events.push_back(
            Event{EventId{9000200 + static_cast<std::uint64_t>(i)}, Beat{i, 4}, group});
    }

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto first_start = compiled->xml.find("<slur type=\"start\"");
    const auto incoming_stop = compiled->xml.find("<slur type=\"stop\"", first_start + 1);
    const auto outgoing_start = compiled->xml.find("<slur type=\"start\"", first_start + 1);
    const auto final_stop = compiled->xml.find("<slur type=\"stop\"", incoming_stop + 1);
    REQUIRE(first_start != std::string::npos);
    REQUIRE(incoming_stop != std::string::npos);
    REQUIRE(outgoing_start != std::string::npos);
    REQUIRE(final_stop != std::string::npos);
    CHECK(first_start < incoming_stop);
    CHECK(incoming_stop < outgoing_start);
    CHECK(outgoing_start < final_stop);
}

TEST_CASE("tempo emits <sound tempo=\"...\">", "[musicxml][format]") {
    auto score = make_test_score(); // bpm = 120

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("tempo") != std::string::npos);
    CHECK(compiled->xml.find("120") != std::string::npos);
}

TEST_CASE("tempo transition ownership is explicit in MusicXML",
          "[musicxml][format][tempo-transition]") {
    SECTION("linear label is at the start and the structured rate is at the endpoint") {
        auto score = make_test_score(2);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{2, Beat::zero()};
        target.bpm = make_bpm(90);
        target.transition_type = TempoTransitionType::Linear;
        target.linear_duration = Beat{1, 1};
        score.tempo_map.push_back(target);

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        const auto initial = compiled->xml.find("<per-minute>120</per-minute>");
        const auto start = compiled->xml.find("linear tempo transition");
        const auto endpoint = compiled->xml.find("<per-minute>90</per-minute>");
        REQUIRE(initial != std::string::npos);
        REQUIRE(start != std::string::npos);
        REQUIRE(endpoint != std::string::npos);
        CHECK(initial < start);
        CHECK(start < endpoint);
        REQUIRE(compiled->report.diagnostics.size() == 1);
    }

    SECTION("metric modulation emits the exact unit relation and target rate") {
        auto score = make_test_score(2);
        TempoEvent target = score.tempo_map.front();
        target.position = ScoreTime{2, Beat::zero()};
        target.bpm = make_bpm(120);
        target.beat_unit = BeatUnit::Half;
        target.transition_type = TempoTransitionType::MetricModulation;
        target.old_unit = BeatUnit::Quarter;
        target.new_unit = BeatUnit::Half;
        score.tempo_map.push_back(target);

        auto compiled = compile_score_to_musicxml(score);
        REQUIRE(compiled.has_value());
        CHECK(compiled->xml.find("<metronome-relation>equals</metronome-relation>") !=
              std::string::npos);
        CHECK(compiled->xml.find("<beat-unit>half</beat-unit>") != std::string::npos);
        CHECK(compiled->xml.find("<sound tempo=\"240\"") != std::string::npos);
        CHECK(compiled->report.diagnostics.empty());
    }
}

TEST_CASE("key signature change at bar 2 emits <key> with correct <fifths>", "[musicxml][format]") {
    auto score = make_test_score();

    // Add G major at bar 2: root = G (letter 4, accidental 0), fifths = 1
    KeySignatureEntry key2;
    key2.position = ScoreTime{2, Beat::zero()};
    key2.key.root = SpelledPitch{4, 0, 4}; // G
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

TEST_CASE("multi-part score produces correct <part> count", "[musicxml][format]") {
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

TEST_CASE("multi-staff Part emits numbered clefs and exact Voice staff ownership",
          "[musicxml][format][staff]") {
    ScoreSpec spec;
    spec.title = "Grand Staff";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano;
    piano.name = "Piano";
    piano.abbreviation = "Pno.";
    piano.instrument_type = InstrumentType::Piano;
    piano.staff_count = 2;
    piano.staff_clefs = {Clef::Treble, Clef::Bass};
    spec.parts.push_back(piano);

    auto created = create_score(spec);
    REQUIRE(created.has_value());
    auto score = *created;
    REQUIRE(score.parts[0].measures[0].voices.size() == 2);

    for (std::size_t staff = 0; staff < 2; ++staff) {
        auto& voice = score.parts[0].measures[0].voices[staff];
        voice.events.clear();
        Note note;
        note.pitch = SpelledPitch{0, 0, static_cast<std::int8_t>(4 - staff)};
        note.velocity = {};
        if (staff == 1) note.dynamic = DynamicLevel::p;
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 1};
        voice.events.push_back(
            Event{EventId{9100000 + static_cast<std::uint64_t>(staff)}, Beat::zero(), group});
    }

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto& xml = compiled->xml;
    CHECK(xml.find("<staves>2</staves>") != std::string::npos);
    CHECK(xml.find("<clef number=\"1\">") != std::string::npos);
    CHECK(xml.find("<clef number=\"2\">") != std::string::npos);
    CHECK(xml.find("<sign>G</sign>") != std::string::npos);
    CHECK(xml.find("<sign>F</sign>") != std::string::npos);
    CHECK(count_occurrences(xml, "<staff>1</staff>") == 1);
    CHECK(count_occurrences(xml, "<staff>2</staff>") == 2);
}

TEST_CASE("enharmonic spelling C# preserved through compile-parse round-trip",
          "[musicxml][format]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 1, 4}; // C#4
    note.velocity = {};
    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 4};

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
        if (!n.is_rest) {
            found = &n;
            break;
        }
    }
    REQUIRE(found != nullptr);
    CHECK(found->pitch.letter == 0);     // C (not D)
    CHECK(found->pitch.accidental == 1); // sharp (not flat)
}

TEST_CASE("uncompilable score returns std::unexpected", "[musicxml][format]") {
    Score score;
    score.id = ScoreId{1};
    score.metadata.title = "Invalid";
    score.metadata.total_bars = 0;
    // Empty parts vector violates the compilability precondition

    auto result = compile_score_to_musicxml(score);
    REQUIRE_FALSE(result.has_value());
}

TEST_CASE("tuplet NoteGroup produces <time-modification>", "[musicxml][format]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};

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
    CHECK(compiled->xml.find("<type>eighth</type>") != std::string::npos);
    CHECK(count_occurrences(compiled->xml, "<tuplet type=\"start\"") == 1);
    CHECK(count_occurrences(compiled->xml, "<tuplet type=\"stop\"") == 1);
}

TEST_CASE("nested tuplets preserve cumulative sound ratio and numbered brackets",
          "[musicxml][format][tuplet]") {
    auto score = make_test_score(1);
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
    events.push_back(Event{EventId{9700001}, Beat::zero(), group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{9700002}, Beat{1, 6}, group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{9700003}, Beat{1, 3}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{9700004}, Beat{7, 18}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{9700005}, Beat{4, 9}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{9700006}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(count_occurrences(compiled->xml, "<actual-notes>3</actual-notes>") == 2);
    CHECK(count_occurrences(compiled->xml, "<actual-notes>9</actual-notes>") == 3);
    CHECK(count_occurrences(compiled->xml, "<normal-notes>4</normal-notes>") == 3);
    CHECK(compiled->xml.find("<normal-type>quarter</normal-type>") != std::string::npos);
    CHECK(compiled->xml.find("<tuplet type=\"start\" number=\"1\"") != std::string::npos);
    CHECK(compiled->xml.find("<tuplet type=\"start\" number=\"2\"") != std::string::npos);
    CHECK(compiled->xml.find("<tuplet type=\"stop\" number=\"1\"") != std::string::npos);
    CHECK(compiled->xml.find("<tuplet type=\"stop\" number=\"2\"") != std::string::npos);
}

TEST_CASE("a Rest can be a first-class tuplet member", "[musicxml][format][tuplet]") {
    auto score = make_test_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    const TupletContext context{TupletId{3}, 3, 2, Beat{1, 8}, std::nullopt};
    NoteGroup note;
    note.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    note.duration = Beat{1, 12};
    note.tuplet_context = context;
    events.push_back(Event{EventId{9710001}, Beat::zero(), note});
    events.push_back(Event{EventId{9710002}, Beat{1, 12}, RestEvent{Beat{1, 12}, true, context}});
    events.push_back(Event{EventId{9710003}, Beat{1, 6}, note});
    events.push_back(Event{EventId{9710004}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(count_occurrences(compiled->xml, "<time-modification>") == 3);
    const auto tuplet_rest = compiled->xml.find("<rest");
    REQUIRE(tuplet_rest != std::string::npos);
    const auto rest_end = compiled->xml.find("</note>", tuplet_rest);
    REQUIRE(rest_end != std::string::npos);
    const auto rest_time_modification = compiled->xml.find("<time-modification>", tuplet_rest);
    REQUIRE(rest_time_modification != std::string::npos);
    CHECK(rest_time_modification < rest_end);
}

TEST_CASE("transposing instrument emits <transpose>", "[musicxml][format]") {
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
    clar_def.transposition = -2; // Bb clarinet: written-to-sounding = -2 semitones
    clar_def.rendering.midi_channel = 1;
    spec.parts.push_back(clar_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto score = *result;

    insert_c4_quarter(score);

    // Replaces a check that merely found `<transpose`. MusicXML 4.0 adds
    // <transpose> to the written <pitch> to obtain sounding pitch, so concert
    // C4 on a B-flat clarinet (sounding a major second below written) is
    // written D4 with diatonic -1 and chromatic -2, and the concert C-major
    // signature is written D major (two sharps).
    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto& xml = compiled->xml;
    INFO(xml);
    CHECK(xml.find("<transpose>\n") != std::string::npos);
    CHECK(xml.find("<diatonic>-1</diatonic>") != std::string::npos);
    CHECK(xml.find("<chromatic>-2</chromatic>") != std::string::npos);
    CHECK(xml.find("<step>D</step>") != std::string::npos);
    CHECK(xml.find("<step>C</step>") == std::string::npos);
    CHECK(xml.find("<fifths>2</fifths>") != std::string::npos);
}

TEST_CASE("dynamic marking emits <dynamics>", "[musicxml][format]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};
    note.dynamic = DynamicLevel::ff;

    NoteGroup ng;
    ng.notes.push_back(note);
    ng.duration = Beat{1, 4};

    voice.events.clear();
    voice.events.push_back(Event{EventId{9000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 4}, true};
    voice.events.push_back(Event{EventId{9000002}, Beat{1, 4}, rest});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("dynamics") != std::string::npos);
}

TEST_CASE("rehearsal mark emits <rehearsal>", "[musicxml][format]") {
    auto score = make_test_score();

    RehearsalMark rm;
    rm.position = ScoreTime{1, Beat::zero()};
    rm.label = "A";
    score.rehearsal_marks.push_back(rm);

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("rehearsal") != std::string::npos);
}

TEST_CASE("MusicXML key uses stored fifths and native church mode", "[musicxml][format][key]") {
    auto score = make_test_score(1);
    auto dorian = find_scale("dorian");
    REQUIRE(dorian.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{1, 0, 4}, *dorian, 0};

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<fifths>0</fifths>") != std::string::npos);
    CHECK(compiled->xml.find("<mode>dorian</mode>") != std::string::npos);
    CHECK(compiled->report.diagnostics.empty());
}

TEST_CASE("MusicXML custom scale preserves traditional fifths without inventing altered steps",
          "[musicxml][format][key]") {
    auto score = make_test_score(1);
    auto harmonic_minor = find_scale("harmonic_minor");
    REQUIRE(harmonic_minor.has_value());
    score.key_map[0].key.mode = *harmonic_minor;
    score.key_map[0].key.accidentals = -4;

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<fifths>-4</fifths>") != std::string::npos);
    CHECK(compiled->xml.find("<mode>none</mode>") != std::string::npos);
    CHECK(compiled->xml.find("<key-step>") == std::string::npos);
    REQUIRE(compiled->report.diagnostics.size() == 1);
    CHECK(compiled->report.diagnostics[0].message.find("altered-step data absent") !=
          std::string::npos);
}

TEST_CASE("mid-measure key change retains its exact MusicXML cursor position",
          "[musicxml][format][key][timing]") {
    auto score = make_test_score(1);
    auto major = find_scale("major");
    REQUIRE(major.has_value());
    score.key_map.push_back(KeySignatureEntry{
        ScoreTime{1, Beat{1, 2}},
        KeySignature{SpelledPitch{4, 0, 4}, *major, 1},
    });

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto initial_key = compiled->xml.find("<fifths>0</fifths>");
    const auto forward = compiled->xml.find("<forward>", initial_key);
    const auto changed_key = compiled->xml.find("<fifths>1</fifths>", forward);
    const auto backup = compiled->xml.find("<backup>", changed_key);
    const auto measured_rest = compiled->xml.find("<rest", backup);
    REQUIRE(initial_key != std::string::npos);
    REQUIRE(forward != std::string::npos);
    REQUIRE(changed_key != std::string::npos);
    REQUIRE(backup != std::string::npos);
    REQUIRE(measured_rest != std::string::npos);
    CHECK(initial_key < forward);
    CHECK(forward < changed_key);
    CHECK(changed_key < backup);
    CHECK(backup < measured_rest);
}

TEST_CASE("additive meter groups reach MusicXML beats text", "[musicxml][format][meter]") {
    auto score = make_test_score(1);
    score.time_map[0].time_signature = TimeSignature{{3, 2}, 8};
    auto* rest = std::get_if<RestEvent>(&score.parts[0].measures[0].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->duration = Beat{5, 8};

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<beats>3+2</beats>") != std::string::npos);
    CHECK(compiled->xml.find("<beat-type>8</beat-type>") != std::string::npos);
}

TEST_CASE("measure-local MusicXML meter resets to the global map next measure",
          "[musicxml][format][meter][local]") {
    auto score = make_test_score(2);
    score.parts[0].measures[0].local_time = TimeSignature{{3}, 4};
    auto* rest = std::get_if<RestEvent>(&score.parts[0].measures[0].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->duration = Beat{3, 4};

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto three_four = compiled->xml.find("<beats>3</beats>");
    const auto four_four = compiled->xml.find("<beats>4</beats>", three_four + 1);
    REQUIRE(three_four != std::string::npos);
    REQUIRE(four_four != std::string::npos);
    CHECK(three_four < four_four);
}

TEST_CASE("hidden rest uses MusicXML print-object no", "[musicxml][format][rest]") {
    auto score = make_test_score(1);
    auto* rest = std::get_if<RestEvent>(&score.parts[0].measures[0].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->visible = false;

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->xml.find("<note print-object=\"no\">") != std::string::npos);
}

TEST_CASE("manual BeamGroup emits MusicXML begin continue end", "[musicxml][format][beam]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    const BeamGroupId beam_id{88};
    BeamGroup beam;
    beam.id = beam_id;
    for (std::uint64_t i = 0; i < 4; ++i) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(i), 0, 4};
        note.velocity = {};
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 8};
        group.beam_group = beam_id;
        const EventId id{9800001 + i};
        beam.event_ids.push_back(id);
        voice.events.push_back(Event{id, Beat{static_cast<std::int64_t>(i), 8}, group});
    }
    voice.beam_groups.push_back(beam);
    voice.events.push_back(Event{EventId{9800005}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(count_occurrences(compiled->xml, "<beam number=\"1\">begin</beam>") == 1);
    CHECK(count_occurrences(compiled->xml, "<beam number=\"1\">continue</beam>") == 2);
    CHECK(count_occurrences(compiled->xml, "<beam number=\"1\">end</beam>") == 1);
}

// =============================================================================
// Written durations (issue #10)
// =============================================================================

TEST_CASE("triplet eighths inserted one by one export with a 3:2 time modification",
          "[musicxml][format][tuplet][regression]") {
    // Three 1/12 notes fill one quarter beat: written eighths under 3:2.
    auto score = make_test_score(1);
    const auto part = score.parts[0].id;
    for (int index = 0; index < 3; ++index) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(index), 0, 4};
        REQUIRE(insert_note(score, part, 1, 0, Beat{index, 12}, note, Beat{1, 12}));
    }
    for (const auto& event : score.parts[0].measures[0].voices[0].events) {
        if (const auto* group = event.as_note_group()) {
            REQUIRE(group->tuplet_context.has_value());
            CHECK(group->tuplet_context->actual == 3);
            CHECK(group->tuplet_context->normal == 2);
            CHECK(group->tuplet_context->normal_type == Beat{1, 8});
        }
    }

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto& xml = compiled->xml;
    INFO(xml);
    CHECK(count_occurrences(xml, "<type>eighth</type>") == 3);
    CHECK(count_occurrences(xml, "<actual-notes>3</actual-notes>") == 3);
    CHECK(count_occurrences(xml, "<normal-notes>2</normal-notes>") == 3);
    CHECK(count_occurrences(xml, "<tuplet type=\"start\"") == 1);
    CHECK(count_occurrences(xml, "<tuplet type=\"stop\"") == 1);
    // insert_note marks harmony stale, which has its own residual; no
    // written-value residual may appear.
    CHECK(std::ranges::none_of(compiled->report.diagnostics, [](const auto& diagnostic) {
        return diagnostic.message.find("written") != std::string::npos;
    }));
}

TEST_CASE("a 5/16 note exports as a tied quarter and sixteenth", "[musicxml][format][regression]") {
    auto score = make_test_score(1);
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    REQUIRE(insert_note(score, score.parts[0].id, 1, 0, Beat::zero(), note, Beat{5, 16}));

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto& xml = compiled->xml;
    INFO(xml);
    CHECK(count_occurrences(xml, "<step>C</step>") == 2);
    const auto quarter = xml.find("<type>quarter</type>");
    const auto sixteenth = xml.find("<type>16th</type>");
    REQUIRE(quarter != std::string::npos);
    REQUIRE(sixteenth != std::string::npos);
    CHECK(quarter < sixteenth);
    CHECK(count_occurrences(xml, "<tie type=\"start\" />") == 1);
    CHECK(count_occurrences(xml, "<tie type=\"stop\" />") == 1);
    CHECK(count_occurrences(xml, "<tied type=\"start\" />") == 1);
    CHECK(count_occurrences(xml, "<tied type=\"stop\" />") == 1);
    CHECK(std::ranges::none_of(compiled->report.diagnostics, [](const auto& diagnostic) {
        return diagnostic.message.find("written") != std::string::npos;
    }));
}

TEST_CASE("a context-free 1/12 note exports as an eighth under 3:2",
          "[musicxml][format][tuplet][regression]") {
    // A Score built without a TupletContext still has one written form: an
    // eighth note under a 3:2 time modification (1/8 x 2/3 = 1/12).
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 12};
    voice.events.push_back(Event{EventId{9100001}, Beat::zero(), group});
    voice.events.push_back(Event{EventId{9100002}, Beat{1, 12}, RestEvent{Beat{1, 6}, true}});
    voice.events.push_back(Event{EventId{9100003}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    const auto& xml = compiled->xml;
    INFO(xml);
    const auto note_start = xml.find("<step>C</step>");
    REQUIRE(note_start != std::string::npos);
    const auto note_end = xml.find("</note>", note_start);
    const auto note_xml = xml.substr(note_start, note_end - note_start);
    CHECK(note_xml.find("<type>eighth</type>") != std::string::npos);
    CHECK(note_xml.find("<actual-notes>3</actual-notes>") != std::string::npos);
    CHECK(note_xml.find("<normal-notes>2</normal-notes>") != std::string::npos);
}

TEST_CASE("a duration with no written form is reported rather than mislabelled",
          "[musicxml][format][regression]") {
    // 1/2048 of a whole note is shorter than MusicXML's smallest note type
    // (1024th), so no <type> can be written and the loss must be reported.
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 2048};
    voice.events.push_back(Event{EventId{9200001}, Beat::zero(), group});
    voice.events.push_back(
        Event{EventId{9200002}, Beat{1, 2048}, RestEvent{Beat{2047, 2048}, true}});

    auto compiled = compile_score_to_musicxml(score);
    REQUIRE(compiled.has_value());
    CHECK(std::ranges::any_of(compiled->report.diagnostics, [](const auto& diagnostic) {
        return diagnostic.message.find("written") != std::string::npos;
    }));
}
