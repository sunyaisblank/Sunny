/**
 * @file score_to_lilypond_test.cpp
 * @brief LilyPond Score IR compiler unit tests
 *
 *
 */

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/post_tonal/twelve_tone.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/infrastructure/formats/score_to_lilypond.hpp>

// Include Score IR creation function
#include <sunny/core/score/workflows.hpp>

using namespace sunny::infrastructure::formats;
using namespace sunny::core;

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

void insert_note_at(Score& score,
                    SpelledPitch pitch,
                    Beat duration,
                    std::size_t part_idx = 0,
                    std::optional<ArticulationType> art = std::nullopt,
                    bool tie = false) {
    auto& voice = score.parts[part_idx].measures[0].voices[0];
    Note note;
    note.pitch = pitch;
    note.velocity = {};
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
            Event{EventId{8100000 + i}, Beat{static_cast<std::int64_t>(i), 12}, group});
    }
}

} // namespace

// =============================================================================
// Single note emission
// =============================================================================

TEST_CASE("LilyPond reports non-standard Score tuning instead of silently dropping it",
          "[lilypond][compiler][tuning]") {
    auto score = make_test_score();
    score.tuning.name = "custom";
    score.tuning.cents_from_reference[60] = -901.25;
    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->report.tuning_definitions_requested == 1);
    CHECK(result->report.tuning_definitions_written == 0);
    CHECK(result->report.has_drops());
}

TEST_CASE("LilyPond reports every unrepresented top-level analysis layer",
          "[lilypond][compiler][analysis-residual]") {
    auto score = make_test_score();
    populate_unrepresented_analysis_state(score);

    const auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    REQUIRE(result->report.diagnostics.size() == 6);
    CHECK_FALSE(result->report.has_drops());
    CHECK(result->report.has_residuals());
    CHECK(result->report.diagnostics[0].message.find("SectionMap") != std::string::npos);
    CHECK(result->report.diagnostics[1].message.find("harmonic-analysis") != std::string::npos);
    CHECK(result->report.diagnostics[2].message.find("orchestration-analysis") !=
          std::string::npos);
    CHECK(result->report.diagnostics[3].message.find("twelve-tone row") != std::string::npos);
    CHECK(result->report.diagnostics[4].message.find("Stale harmonic") != std::string::npos);
    CHECK(result->report.diagnostics[5].message.find("Stale orchestration") != std::string::npos);
}

TEST_CASE("LilyPond reports non-default release velocity as a notation residual",
          "[lilypond][compiler][release-velocity]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 4});
    std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload)
        .notes[0]
        .release_velocity = 23;

    const auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK_FALSE(result->report.has_drops());
    CHECK(result->report.has_residuals());
    CHECK(result->report.diagnostics[0].message.find("release velocity") != std::string::npos);
    CHECK(result->report.diagnostics[0].location == ScoreTime{1, Beat::zero()});
    CHECK(result->report.diagnostics[0].part == score.parts[0].id);
}

TEST_CASE("LilyPond distinguishes semantic and numeric-only attack velocity",
          "[lilypond][compiler][attack-velocity][dynamic]") {
    SECTION("semantic VelocityValue.written becomes a dynamic command") {
        auto score = make_test_score();
        insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 4});
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity = VelocityValue{DynamicLevel::ff, 73};

        const auto result = compile_score_to_lilypond(score);
        REQUIRE(result);
        CHECK(result->ly.find("\\ff") != std::string::npos);
        CHECK(result->report.diagnostics.empty());
    }

    SECTION("numeric-only attack intensity remains an explicit residual") {
        auto score = make_test_score();
        insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 4});
        auto& note =
            std::get<NoteGroup>(score.parts[0].measures[0].voices[0].events[0].payload).notes[0];
        note.velocity.value = 73;

        const auto result = compile_score_to_lilypond(score);
        REQUIRE(result);
        REQUIRE(result->report.diagnostics.size() == 1);
        CHECK(result->report.diagnostics[0].message.find("numeric note attack velocity") !=
              std::string::npos);
        CHECK(result->report.diagnostics[0].location == ScoreTime{1, Beat::zero()});
        CHECK(result->report.diagnostics[0].part == score.parts[0].id);
    }
}

TEST_CASE("single C4 whole note produces c'1", "[lilypond][compiler]") {
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

TEST_CASE("output starts with \\version", "[lilypond][compiler]") {
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

TEST_CASE("contains \\key c \\major and \\time 4/4", "[lilypond][compiler]") {
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

TEST_CASE("staccato produces -.", "[lilypond][compiler]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 4}, 0, ArticulationType::Staccato);

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("-.") != std::string::npos);
}

// =============================================================================
// Tie
// =============================================================================

TEST_CASE("tie produces ~", "[lilypond][compiler]") {
    auto score = make_test_score();

    // Bar 1: tied C4 whole note (last event, so tie resolves across bar)
    {
        auto& voice = score.parts[0].measures[0].voices[0];
        voice.events.clear();

        Note note;
        note.pitch = SpelledPitch{0, 0, 4};
        note.velocity = {};
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
        note.velocity = {};
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

TEST_CASE("chained slur closes before reopening on the shared note",
          "[lilypond][compiler][span-endpoint]") {
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
            Event{EventId{8000200 + static_cast<std::uint64_t>(i)}, Beat{i, 4}, group});
    }

    auto compiled = compile_score_to_lilypond(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->ly.find("c'4(") != std::string::npos);
    CHECK(compiled->ly.find("d'4)(") != std::string::npos);
    CHECK(compiled->ly.find("e'4)") != std::string::npos);
}

// =============================================================================
// Chord syntax
// =============================================================================

TEST_CASE("chord produces angle-bracket syntax", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    NoteGroup ng;
    Note c4;
    c4.pitch = SpelledPitch{0, 0, 4};
    c4.velocity = {};
    Note e4;
    e4.pitch = SpelledPitch{2, 0, 4};
    e4.velocity = {};
    Note g4;
    g4.pitch = SpelledPitch{4, 0, 4};
    g4.velocity = {};
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

TEST_CASE("rest produces r or R with duration", "[lilypond][compiler]") {
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

TEST_CASE("two voices produce polyphony syntax", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& measure = score.parts[0].measures[0];

    // Voice 0: C4 whole note
    {
        auto& v0 = measure.voices[0];
        v0.events.clear();
        NoteGroup ng;
        Note c4;
        c4.pitch = SpelledPitch{0, 0, 4};
        c4.velocity = {};
        ng.notes.push_back(c4);
        ng.duration = Beat{1, 1};
        v0.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});
    }

    // Voice 1: E4 whole note
    {
        Voice v1;
        v1.voice_index = 1;
        NoteGroup ng;
        Note e4;
        e4.pitch = SpelledPitch{2, 0, 4};
        e4.velocity = {};
        ng.notes.push_back(e4);
        ng.duration = Beat{1, 1};
        v1.events.push_back(Event{EventId{8000003}, Beat::zero(), ng});
        measure.voices.push_back(v1);
    }

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    // Replaces a check that any `\\` appeared, which also passed when the
    // separator only wrapped annotations. Exactly one separator divides the
    // two genuine voices; the annotation spacer joins the first voice without.
    const std::string& ly = result->ly;
    CHECK(ly.find("<<") != std::string::npos);
    std::size_t separators = 0;
    for (auto at = ly.find("\\\\"); at != std::string::npos; at = ly.find("\\\\", at + 2))
        ++separators;
    CHECK(separators == 1);
    CHECK(ly.find("\\\\ { e'1") != std::string::npos);
}

// =============================================================================
// Hairpin crescendo
// =============================================================================

TEST_CASE("hairpin crescendo produces \\<", "[lilypond][compiler]") {
    auto score = make_test_score();
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 1});

    // Add a crescendo hairpin spanning bar 1
    Hairpin hp;
    hp.start = ScoreTime{1, Beat::zero()};
    hp.end = ScoreTime{2, Beat::zero()};
    hp.type = HairpinType::Crescendo;
    hp.target = DynamicLevel::f;
    score.parts[0].hairpins.push_back(hp);

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    const std::string& ly = result->ly;
    CHECK(ly.find("\\<") != std::string::npos);
    CHECK(ly.find("<>\\! <>\\f") != std::string::npos);
}

TEST_CASE("PartDirective boundaries remain visible at exact LilyPond time",
          "[lilypond][compiler][directive]") {
    auto score = make_test_score(1);
    score.parts[0].part_directives.push_back(PartDirective{
        ScoreTime{1, Beat{1, 4}}, ScoreTime{1, Beat{3, 4}}, DirectiveType::Divisi, 3});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    const auto start = result->ly.find("\\italic \"divisi a 3\"");
    const auto end = result->ly.find("\\italic \"end divisi a 3\"");
    REQUIRE(start != std::string::npos);
    REQUIRE(end != std::string::npos);
    CHECK(start < end);
    CHECK(result->ly.find("s4", start) != std::string::npos);
}

// =============================================================================
// Multi-part produces multiple Staff blocks
// =============================================================================

TEST_CASE("multi-part produces multiple \\new Staff blocks", "[lilypond][compiler]") {
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

TEST_CASE("multi-staff Part becomes a PianoStaff with separated Voice ownership",
          "[lilypond][compiler][staff]") {
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
    for (std::size_t staff = 0; staff < 2; ++staff) {
        auto& voice = score.parts[0].measures[0].voices[staff];
        voice.events.clear();
        Note note;
        note.pitch = SpelledPitch{0, 0, static_cast<std::int8_t>(4 - staff)};
        note.velocity = {};
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 1};
        voice.events.push_back(
            Event{EventId{8100000 + static_cast<std::uint64_t>(staff)}, Beat::zero(), group});
    }

    auto compiled = compile_score_to_lilypond(score);
    REQUIRE(compiled.has_value());
    const auto& ly = compiled->ly;
    const auto piano_staff = ly.find("\\new PianoStaff");
    const auto treble = ly.find("\\clef treble", piano_staff);
    const auto upper_note = ly.find("c'1", treble);
    const auto bass = ly.find("\\clef bass", upper_note);
    const auto lower_note = ly.find("c1", bass);
    REQUIRE(piano_staff != std::string::npos);
    REQUIRE(treble != std::string::npos);
    REQUIRE(upper_note != std::string::npos);
    REQUIRE(bass != std::string::npos);
    REQUIRE(lower_note != std::string::npos);
    CHECK(treble < upper_note);
    CHECK(upper_note < bass);
    CHECK(bass < lower_note);
}

// =============================================================================
// Dynamic ff
// =============================================================================

TEST_CASE("dynamic ff produces \\ff", "[lilypond][compiler]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};
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

TEST_CASE("uncompilable score returns unexpected", "[lilypond][compiler]") {
    auto score = make_test_score();

    // Clear parts to violate the compilability precondition
    score.parts.clear();

    auto result = compile_score_to_lilypond(score);
    REQUIRE_FALSE(result.has_value());
}

// =============================================================================
// Unwritable dyadic durations are tied written values (issue #10)
// =============================================================================

// Replaces a test that asserted `c'1*5/8`: LilyPond prints that multiplier as a
// whole-note glyph, so the written music misstated the rhythm. Gould (Behind
// Bars) requires every written note to carry a real value; 5/8 is a half
// tied to an eighth.
TEST_CASE("a 5/8 note is written as a tied half and eighth", "[lilypond][compiler][regression]") {
    auto score = make_test_score();

    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    NoteGroup ng;
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};
    ng.notes.push_back(note);
    ng.duration = Beat{5, 8};
    voice.events.push_back(Event{EventId{8000001}, Beat::zero(), ng});
    RestEvent rest{Beat{3, 8}, true};
    voice.events.push_back(Event{EventId{8000002}, Beat{5, 8}, rest});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());

    CHECK(result->ly.find("c'2~ c'8") != std::string::npos);
    CHECK(result->ly.find("1*5/8") == std::string::npos);
    CHECK(result->ly.find("r4.") != std::string::npos);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("a 5/16 note is written as a tied quarter and sixteenth",
          "[lilypond][compiler][regression]") {
    auto score = make_test_score(1);
    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    REQUIRE(insert_note(score, score.parts[0].id, 1, 0, Beat::zero(), note, Beat{5, 16}));

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    INFO(result->ly);
    CHECK(result->ly.find("c'4~ c'16") != std::string::npos);
    CHECK(result->ly.find("1*5/16") == std::string::npos);
}

TEST_CASE("triplet eighths inserted one by one are written as a LilyPond tuplet",
          "[lilypond][compiler][tuplet][regression]") {
    // Three notes of 1/12 whole note fill one quarter beat: a 3:2 tuplet of
    // eighths, written \tuplet 3/2 { c'8 d'8 e'8 }.
    auto score = make_test_score(1);
    const auto part = score.parts[0].id;
    for (int index = 0; index < 3; ++index) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(index), 0, 4};
        REQUIRE(insert_note(score, part, 1, 0, Beat{index, 12}, note, Beat{1, 12}));
    }

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    INFO(result->ly);
    CHECK(result->ly.find("\\tuplet 3/2 { c'8 d'8 e'8 }") != std::string::npos);
    CHECK(result->ly.find("1*1/12") == std::string::npos);
}

TEST_CASE("a tie across an annotated barline stays in one LilyPond voice",
          "[lilypond][compiler][regression]") {
    // Bar 1 carries the initial tempo mark. `<< {...} \\ {...} >>` would create
    // new voices with forced stems (Notation Reference 1.5.2) and strand the
    // tie; a parallel spacer without `\\` keeps one voice.
    // The continuation is inserted first: a tie must always reach a note.
    auto score = make_test_score(2);
    Note release;
    release.pitch = SpelledPitch{0, 0, 4};
    REQUIRE(insert_note(score, score.parts[0].id, 2, 0, Beat::zero(), release, Beat{1, 1}));
    Note held;
    held.pitch = SpelledPitch{0, 0, 4};
    held.tie_forward = true;
    REQUIRE(insert_note(score, score.parts[0].id, 1, 0, Beat::zero(), held, Beat{1, 1}));

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    INFO(result->ly);
    CHECK(result->ly.find("\\tempo") != std::string::npos);
    CHECK(result->ly.find("c'1~") != std::string::npos);
    CHECK(result->ly.find("\\\\") == std::string::npos);
}

TEST_CASE("a B-flat clarinet part transposes concert pitch up a major second",
          "[lilypond][compiler][transposition][regression]") {
    // Written C4 sounds B-flat 3, so \transpose bes c' maps concert to written.
    auto score = make_test_score(1);
    score.parts[0].definition.transposition = -2;
    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\transpose bes c'") != std::string::npos);
}

TEST_CASE("invisible rests compile to spacer rests", "[lilypond][compiler][rest]") {
    auto score = make_test_score(1);
    auto* rest = std::get_if<RestEvent>(&score.parts[0].measures[0].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->visible = false;

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("s1") != std::string::npos);
    CHECK(result->ly.find("R1") == std::string::npos);
}

TEST_CASE("nested tuplets compile as nested LilyPond time scaling",
          "[lilypond][compiler][tuplet]") {
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
    events.push_back(Event{EventId{8090001}, Beat::zero(), group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{8090002}, Beat{1, 6}, group(Beat{1, 6}, outer)});
    events.push_back(Event{EventId{8090003}, Beat{1, 3}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8090004}, Beat{7, 18}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8090005}, Beat{4, 9}, group(Beat{1, 18}, inner)});
    events.push_back(Event{EventId{8090006}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    const auto outer_start = result->ly.find("\\tuplet 3/2 { c'4");
    REQUIRE(outer_start != std::string::npos);
    const auto inner_start = result->ly.find("\\tuplet 3/2 { c'8", outer_start + 1);
    REQUIRE(inner_start != std::string::npos);
    CHECK(outer_start < inner_start);
    CHECK(result->ly.find("} }", inner_start) != std::string::npos);
}

TEST_CASE("tuplet rests retain their written duration", "[lilypond][compiler][tuplet][rest]") {
    auto score = make_test_score(1);
    auto& events = score.parts[0].measures[0].voices[0].events;
    events.clear();

    const TupletContext context{TupletId{3}, 3, 2, Beat{1, 8}, std::nullopt};
    NoteGroup note;
    note.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    note.duration = Beat{1, 12};
    note.tuplet_context = context;
    events.push_back(Event{EventId{8091001}, Beat::zero(), note});
    events.push_back(Event{EventId{8091002}, Beat{1, 12}, RestEvent{Beat{1, 12}, true, context}});
    events.push_back(Event{EventId{8091003}, Beat{1, 6}, note});
    events.push_back(Event{EventId{8091004}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    const auto tuplet = result->ly.find("\\tuplet 3/2");
    REQUIRE(tuplet != std::string::npos);
    const auto rest = result->ly.find("r8", tuplet);
    REQUIRE(rest != std::string::npos);
    CHECK(rest < result->ly.find("}", tuplet));
}

TEST_CASE("chord noteheads ties and articulations remain per-note", "[lilypond][compiler][chord]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note c;
    c.pitch = SpelledPitch{0, 0, 4};
    c.velocity = {};
    c.tie_forward = true;
    c.notation_head = NoteHeadType::Diamond;
    Note e;
    e.pitch = SpelledPitch{2, 0, 4};
    e.velocity = {};
    e.articulation = ArticulationType::Staccato;
    e.notation_head = NoteHeadType::Cross;

    NoteGroup first;
    first.notes = {c, e};
    first.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8100001}, Beat::zero(), first});

    c.tie_forward = false;
    c.notation_head.reset();
    e.articulation.reset();
    e.notation_head.reset();
    NoteGroup second;
    second.notes = {c, e};
    second.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8100002}, Beat{1, 4}, second});
    voice.events.push_back(Event{EventId{8100003}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\tweak style #'diamond c'~") != std::string::npos);
    CHECK(result->ly.find("\\tweak style #'cross e'-.") != std::string::npos);
}

TEST_CASE("grace chords retain grace type and structural allocation",
          "[lilypond][compiler][grace]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note c;
    c.pitch = SpelledPitch{0, 0, 4};
    c.velocity = {};
    c.grace = GraceType::Acciaccatura;
    Note e = c;
    e.pitch = SpelledPitch{2, 0, 4};
    NoteGroup grace;
    grace.notes = {c, e};
    grace.duration = Beat{1, 4};
    voice.events.push_back(Event{EventId{8200001}, Beat::zero(), grace});
    voice.events.push_back(Event{EventId{8200002}, Beat{1, 4}, RestEvent{Beat{3, 4}, true}});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\acciaccatura { <c' e'>4 } s4") != std::string::npos);
}

TEST_CASE("ornament technical lyric and cue metadata reach LilyPond",
          "[lilypond][compiler][metadata]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    Note note;
    note.pitch = SpelledPitch{0, 0, 4};
    note.velocity = {};
    note.ornament = Ornament{OrnamentType::InvertedTurn, 0, std::nullopt, ArpeggioDirection::None};
    note.notation_head = NoteHeadType::Cue;
    note.lyrics.push_back(LyricSyllable{"sing & play", 1, LyricSyllabic::Single, false});

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

    NoteGroup group;
    group.notes.push_back(note);
    group.duration = Beat{1, 1};
    voice.events.push_back(Event{EventId{8300001}, Beat::zero(), group});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\tweak font-size #-2 c'1") != std::string::npos);
    CHECK(result->ly.find("\\reverseturn") != std::string::npos);
    CHECK(result->ly.find("-1-3\\2") != std::string::npos);
    CHECK(result->ly.find("\\bendAfter #(/ 150 100)") != std::string::npos);
    CHECK(result->ly.find("sing & play") != std::string::npos);
    CHECK(result->ly.find("\\new Lyrics \\lyricsto") != std::string::npos);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("structured verses and melisma use native LilyPond Lyrics contexts",
          "[lilypond][compiler][lyric]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();
    for (std::uint64_t i = 0; i < 4; ++i) {
        NoteGroup group;
        group.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{}});
        group.duration = Beat{1, 4};
        voice.events.push_back(
            Event{EventId{8350000 + i}, Beat{static_cast<std::int64_t>(i), 4}, group});
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

    const auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\new NullVoice = \"partAStaff0Voice0LyricsAnchor\"") !=
          std::string::npos);
    CHECK(result->ly.find("\\set extendersOverRests = ##t") != std::string::npos);
    CHECK(result->ly.find(
              "\\set stanza = \"1.\" \\set extendersOverRests = ##t \"Sun\" -- \"ny\" _ _") !=
          std::string::npos);
    CHECK(result->ly.find("\\set stanza = \"2.\" \\set extendersOverRests = ##t \"Bright\" \"day\" "
                          "__ _ \"now\"") != std::string::npos);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("previously silent articulation variants have explicit LilyPond dispositions",
          "[lilypond][compiler][articulation]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    const std::array articulations{
        ArticulationType::Sforzando,
        ArticulationType::ForzandoPiano,
        ArticulationType::InvertedMordent,
        ArticulationType::InvertedTurn,
        ArticulationType::Harmonic,
        ArticulationType::OpenString,
        ArticulationType::Stopped,
        ArticulationType::Muted,
    };
    for (std::size_t i = 0; i < articulations.size(); ++i) {
        Note note;
        note.pitch = SpelledPitch{static_cast<std::uint8_t>(i % 7), 0, 4};
        note.velocity = {};
        note.articulation = articulations[i];
        NoteGroup group;
        group.notes.push_back(note);
        group.duration = Beat{1, 8};
        voice.events.push_back(
            Event{EventId{8400001 + i}, Beat{static_cast<std::int64_t>(i), 8}, group});
    }

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\sfz") != std::string::npos);
    CHECK(result->ly.find("\\fp") != std::string::npos);
    CHECK(result->ly.find("\\upmordent") != std::string::npos);
    CHECK(result->ly.find("\\reverseturn") != std::string::npos);
    CHECK(result->ly.find("\\flageolet") != std::string::npos);
    CHECK(result->ly.find("\\open") != std::string::npos);
    CHECK(result->ly.find("\\stopped") != std::string::npos);
    CHECK(result->ly.find("muted") != std::string::npos);
    CHECK(result->report.diagnostics.size() == 1);
}

TEST_CASE("point metadata uses an exact parallel spacer timeline", "[lilypond][compiler][timing]") {
    auto score = make_test_score(1);
    insert_note_at(score, SpelledPitch{0, 0, 4}, Beat{1, 1});
    auto& voice = score.parts[0].measures[0].voices[0];

    ScoreDirection text;
    text.type = DirectionType::Text;
    text.text = "dolce";
    voice.events.push_back(Event{EventId{8500001}, Beat{1, 4}, text});

    ChordSymbolEvent chord;
    chord.root = SpelledPitch{0, 0, 4};
    chord.quality = "maj7";
    chord.bass = SpelledPitch{4, 0, 3};
    chord.roman = "Imaj7";
    chord.extensions = {"#11"};
    voice.events.push_back(Event{EventId{8500002}, Beat{1, 2}, chord});

    TempoEvent later = score.tempo_map.front();
    later.position = ScoreTime{1, Beat{3, 4}};
    later.bpm = PositiveRational{181, 2};
    later.beat_unit = BeatUnit::Eighth;
    score.tempo_map.push_back(later);

    score.parts[0].hairpins.push_back(Hairpin{ScoreTime{1, Beat{1, 8}},
                                              ScoreTime{1, Beat{7, 8}},
                                              HairpinType::Crescendo,
                                              DynamicLevel::f});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    const auto& ly = result->ly;
    const auto start = ly.find("<>\\<");
    const auto direction = ly.find("<>^\"dolce\"");
    const auto harmony = ly.find("Cmaj7/G [Imaj7] #11");
    const auto tempo = ly.find("\\tempo 8 = #(/ 181 2)");
    const auto stop = ly.find("<>\\!");
    const auto target = ly.find("<>\\f", stop);
    REQUIRE(start != std::string::npos);
    REQUIRE(direction != std::string::npos);
    REQUIRE(harmony != std::string::npos);
    REQUIRE(tempo != std::string::npos);
    REQUIRE(stop != std::string::npos);
    REQUIRE(target != std::string::npos);
    CHECK(start < direction);
    CHECK(direction < harmony);
    CHECK(harmony < tempo);
    CHECK(tempo < stop);
    CHECK(stop < target);
    CHECK(result->report.diagnostics.size() == 1);
}

TEST_CASE("linear tempo label belongs to the start and target rate to the endpoint",
          "[lilypond][compiler][tempo-transition]") {
    auto score = make_test_score(2);
    TempoEvent target = score.tempo_map.front();
    target.position = ScoreTime{2, Beat::zero()};
    target.bpm = make_bpm(90);
    target.transition_type = TempoTransitionType::Linear;
    target.linear_duration = Beat{1, 1};
    score.tempo_map.push_back(target);

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    const auto initial = result->ly.find("\\tempo 4 = 120");
    const auto start = result->ly.find("linear tempo transition");
    const auto endpoint = result->ly.find("\\tempo 4 = 90");
    REQUIRE(initial != std::string::npos);
    REQUIRE(start != std::string::npos);
    REQUIRE(endpoint != std::string::npos);
    CHECK(initial < start);
    CHECK(start < endpoint);
    REQUIRE(result->report.diagnostics.size() == 1);
}

TEST_CASE("manual beam groups produce explicit primary beam brackets",
          "[lilypond][compiler][beam]") {
    auto score = make_test_score(1);
    auto& voice = score.parts[0].measures[0].voices[0];
    voice.events.clear();

    const BeamGroupId beam_id{77};
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
        const EventId id{8600001 + i};
        beam.event_ids.push_back(id);
        voice.events.push_back(Event{id, Beat{static_cast<std::int64_t>(i), 8}, group});
    }
    voice.beam_groups.push_back(beam);
    voice.events.push_back(Event{EventId{8600005}, Beat{1, 2}, RestEvent{Beat{1, 2}, true}});

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("c'8[") != std::string::npos);
    CHECK(result->ly.find("f'8]") != std::string::npos);
}

TEST_CASE("measure-local key and time overrides reach LilyPond",
          "[lilypond][compiler][local-meter]") {
    auto score = make_test_score(1);
    auto& measure = score.parts[0].measures[0];
    auto minor = find_scale("minor");
    REQUIRE(minor.has_value());
    measure.local_key = KeySignature{SpelledPitch{2, -1, 4}, *minor, -6};
    measure.local_time = TimeSignature{{3}, 4};
    auto* rest = std::get_if<RestEvent>(&measure.voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->duration = Beat{3, 4};

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\key ees \\minor") != std::string::npos);
    CHECK(result->ly.find("\\time 3/4") != std::string::npos);
    CHECK(result->ly.find("R2.") != std::string::npos);
}

TEST_CASE("church modes retain their native LilyPond mode command", "[lilypond][compiler][key]") {
    auto score = make_test_score(1);
    auto dorian = find_scale("dorian");
    REQUIRE(dorian.has_value());
    score.key_map[0].key = KeySignature{SpelledPitch{1, 0, 4}, *dorian, 0};

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\key d \\dorian") != std::string::npos);
    CHECK(result->report.diagnostics.empty());
}

TEST_CASE("custom scale retains exact stored fifths through LilyPond keyAlterations",
          "[lilypond][compiler][key]") {
    auto score = make_test_score(1);
    auto harmonic_minor = find_scale("harmonic_minor");
    REQUIRE(harmonic_minor.has_value());
    score.key_map[0].key.mode = *harmonic_minor;
    score.key_map[0].key.accidentals = 3;

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\set Staff.keyAlterations = #`((3 . 1/2) (0 . 1/2) (4 . 1/2))") !=
          std::string::npos);
    REQUIRE(result->report.diagnostics.size() == 1);
    CHECK(result->report.diagnostics[0].message.find("harmonic_minor") != std::string::npos);
    CHECK(result->report.diagnostics[0].message.find("exact stored traditional signature") !=
          std::string::npos);
}

TEST_CASE("additive meter groups use LilyPond compoundMeter", "[lilypond][compiler][meter]") {
    auto score = make_test_score(1);
    score.time_map[0].time_signature = TimeSignature{{3, 2}, 8};
    auto* rest = std::get_if<RestEvent>(&score.parts[0].measures[0].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->duration = Beat{5, 8};

    auto result = compile_score_to_lilypond(score);
    REQUIRE(result.has_value());
    CHECK(result->ly.find("\\compoundMeter #'(3 2 8)") != std::string::npos);
}
