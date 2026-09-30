/**
 * @file workflows_test.cpp
 * @brief Unit tests for Score IR workflow functions
 *
 *
 * Coverage: create_score, set_formal_plan, set_section_harmony
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/core/score/views.hpp>
#include <sunny/core/score/workflows.hpp>

using namespace sunny::core;

// =============================================================================
// create_score
// =============================================================================

TEST_CASE("create_score produces valid score", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Symphony No. 1";
    spec.total_bars = 16;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4}; // C4
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition violin_def;
    violin_def.name = "Violin I";
    violin_def.abbreviation = "Vln. I";
    violin_def.instrument_type = InstrumentType::Violin;
    spec.parts.push_back(violin_def);

    PartDefinition cello_def;
    cello_def.name = "Cello";
    cello_def.abbreviation = "Vc.";
    cello_def.instrument_type = InstrumentType::Cello;
    spec.parts.push_back(cello_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());

    auto& score = *result;

    // Structural validation should pass
    auto diags = validate_structural(score);
    bool has_error = false;
    for (const auto& d : diags) {
        if (d.severity == ValidationSeverity::Error) has_error = true;
    }
    CHECK_FALSE(has_error);
}

TEST_CASE("create_score with 3 parts creates 3 parts with correct bar count",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Trio";
    spec.total_bars = 8;
    spec.bpm = 100;
    spec.key_root = SpelledPitch{4, 0, 4}; // G4
    spec.key_accidentals = 1;
    spec.time_sig_num = 3;
    spec.time_sig_den = 4;

    PartDefinition flute_def;
    flute_def.name = "Flute";
    flute_def.abbreviation = "Fl.";
    flute_def.instrument_type = InstrumentType::Flute;
    spec.parts.push_back(flute_def);

    PartDefinition oboe_def;
    oboe_def.name = "Oboe";
    oboe_def.abbreviation = "Ob.";
    oboe_def.instrument_type = InstrumentType::Oboe;
    spec.parts.push_back(oboe_def);

    PartDefinition bassoon_def;
    bassoon_def.name = "Bassoon";
    bassoon_def.abbreviation = "Bsn.";
    bassoon_def.instrument_type = InstrumentType::Bassoon;
    spec.parts.push_back(bassoon_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());

    auto& score = *result;
    CHECK(score.parts.size() == 3);
    for (const auto& part : score.parts) {
        CHECK(part.measures.size() == 8);
    }
    CHECK(score.metadata.total_bars == 8);
}

TEST_CASE("score construction uses deterministic document-local identifiers",
          "[score-ir][workflow][identity]") {
    ScoreSpec spec;
    spec.title = "Identity";
    spec.total_bars = 2;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition part;
    part.name = "Piano";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(part);

    const auto first = create_score(spec);
    const auto second = create_score(spec);
    REQUIRE(first.has_value());
    REQUIRE(second.has_value());
    REQUIRE(first->parts.size() == 1);
    REQUIRE(second->parts.size() == 1);
    CHECK(first->parts[0].id == PartId{1});
    CHECK(second->parts[0].id == first->parts[0].id);
    REQUIRE(first->parts[0].measures.size() == 2);
    REQUIRE(second->parts[0].measures.size() == 2);
    CHECK(first->parts[0].measures[0].voices[0].events[0].id == EventId{1});
    CHECK(first->parts[0].measures[1].voices[0].events[0].id == EventId{2});
    CHECK(second->parts[0].measures[1].voices[0].events[0].id == EventId{2});
}

TEST_CASE("score construction preserves caller-selected root identity",
          "[score-ir][workflow][identity]") {
    ScoreSpec spec;
    spec.id = ScoreId{42};
    spec.title = "Root identity";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition part;
    part.name = "Piano";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(part);

    const auto result = create_score(spec);
    REQUIRE(result.has_value());
    CHECK(result->id == ScoreId{42});

    spec.id = ScoreId{0};
    const auto sentinel = create_score(spec);
    REQUIRE_FALSE(sentinel.has_value());
    CHECK(sentinel.error() == ErrorCode::InvalidMutation);
}

TEST_CASE("formal-plan identifiers fill document-local holes below uint64 max",
          "[score-ir][workflow][identity][atomicity]") {
    ScoreSpec spec;
    spec.title = "Sections";
    spec.total_bars = 2;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition part;
    part.name = "Piano";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(part);
    auto score = create_score(spec).value();
    ScoreSection old_section;
    old_section.id = SectionId{std::numeric_limits<std::uint64_t>::max()};
    old_section.label = "old";
    old_section.start = SCORE_START;
    old_section.end = ScoreTime{3, Beat::zero()};
    score.section_map.push_back(std::move(old_section));

    const auto result = set_formal_plan(score, {SectionDefinition{"new", 1, 2, std::nullopt}});
    REQUIRE(result.has_value());
    REQUIRE(score.section_map.size() == 1);
    CHECK(score.section_map[0].id == SectionId{1});
    CHECK(is_compilable(score));
}

TEST_CASE("create_score derives a complete minor key when fifths are omitted",
          "[score-ir][workflow][key]") {
    ScoreSpec spec;
    spec.title = "C minor";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.minor = true;
    PartDefinition part;
    part.name = "Piano";
    part.abbreviation = "Pno.";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(part);

    const auto result = create_score(spec);
    REQUIRE(result.has_value());
    REQUIRE(result->key_map.size() == 1);
    CHECK(result->key_map[0].key.mode.name == "minor");
    CHECK(result->key_map[0].key.accidentals == -3);
    CHECK(is_compilable(*result));

    spec.key_accidentals = 0;
    CHECK_FALSE(create_score(spec).has_value());
}

// =============================================================================
// set_formal_plan
// =============================================================================

TEST_CASE("set_formal_plan sets section_map with correct bar ranges", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Sonata";
    spec.total_bars = 16;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    std::vector<SectionDefinition> sections = {
        {"Exposition", 1, 8, FormFunction::Expository},
        {"Development", 9, 12, FormFunction::Developmental},
        {"Recapitulation", 13, 16, FormFunction::Expository}};

    auto plan_result = set_formal_plan(score, sections);
    REQUIRE(plan_result.has_value());
    REQUIRE(score.section_map.size() == 3);
    CHECK(score.section_map[0].label == "Exposition");
    CHECK(score.section_map[0].start.bar == 1);
    CHECK(score.section_map[1].label == "Development");
    CHECK(score.section_map[1].start.bar == 9);
    CHECK(score.section_map[2].label == "Recapitulation");
    CHECK(score.section_map[2].start.bar == 13);
}

TEST_CASE("formal workflows reject version exhaustion before changing state or history",
          "[score-ir][workflow][version]") {
    ScoreSpec spec;
    spec.title = "Exhausted";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition part;
    part.name = "Piano";
    part.abbreviation = "Pno.";
    part.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(part);

    auto created = create_score(spec);
    REQUIRE(created.has_value());
    auto& score = *created;
    score.version = std::numeric_limits<std::uint64_t>::max();
    UndoStack stack;

    const auto result = set_formal_plan(score, {{"A", 1, 4, FormFunction::Expository}}, &stack);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::ArithmeticOverflow);
    CHECK(score.version == std::numeric_limits<std::uint64_t>::max());
    CHECK(score.section_map.empty());
    CHECK(stack.undo_entries.empty());
    CHECK(stack.redo_entries.empty());
}

// =============================================================================
// set_section_harmony
// =============================================================================

TEST_CASE("set_section_harmony writes annotations with Roman numerals", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Harmony Test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4}; // C major
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{0, 0, 4}, "major", std::nullopt},                // C major → I
        {ScoreTime{2, Beat::zero()}, SpelledPitch{3, 0, 4}, "major", std::nullopt}, // F major → IV
        {ScoreTime{3, Beat::zero()}, SpelledPitch{4, 0, 4}, "major", std::nullopt}, // G major → V
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());

    // Should have 3 harmonic annotations
    REQUIRE(score.harmonic_annotations.size() == 3);

    // Check Roman numerals are computed
    CHECK(score.harmonic_annotations[0].roman_numeral == "I");
    CHECK(score.harmonic_annotations[1].roman_numeral == "IV");
    CHECK(score.harmonic_annotations[2].roman_numeral == "V");
    CHECK(score.harmonic_annotations[0].chord.notes ==
          std::vector<MidiNote>{MidiNote{60}, MidiNote{64}, MidiNote{67}});
    CHECK(score.harmonic_annotations[1].chord.notes ==
          std::vector<MidiNote>{MidiNote{65}, MidiNote{69}, MidiNote{72}});
    CHECK(score.harmonic_annotations[2].chord.notes ==
          std::vector<MidiNote>{MidiNote{67}, MidiNote{71}, MidiNote{74}});
    CHECK(is_compilable(score));
}

TEST_CASE("set_section_harmony constructs an exact slash-bass inversion",
          "[score-ir][workflow][harmony][inversion]") {
    ScoreSpec spec;
    spec.title = "G7 over B";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition piano;
    piano.name = "Piano";
    piano.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano);
    auto score = create_score(spec).value();

    const ScoreRegion region{SCORE_START, ScoreTime{2, Beat::zero()}, {}};
    const std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{4, 0, 4}, "7", SpelledPitch{6, 0, 3}}};

    const auto result = set_section_harmony(score, region, progression);
    REQUIRE(result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    const auto& annotation = score.harmonic_annotations.front();
    CHECK(annotation.chord.notes ==
          std::vector<MidiNote>{MidiNote{59}, MidiNote{67}, MidiNote{74}, MidiNote{77}});
    CHECK(annotation.chord.root == PitchClass{7});
    CHECK(annotation.chord.inversion == 1);
    CHECK(annotation.roman_numeral == "V7");
    CHECK(annotation.function == ScoreHarmonicFunction::Dominant);
    CHECK(is_compilable(score));
}

TEST_CASE("set_section_harmony analyses each chord under its active local key",
          "[score-ir][workflow][harmony][key-map]") {
    ScoreSpec spec;
    spec.title = "Modulation";
    spec.total_bars = 2;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition piano;
    piano.name = "Piano";
    piano.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano);
    auto score = create_score(spec).value();

    KeySignatureEntry modulation;
    modulation.position = ScoreTime{2, Beat::zero()};
    modulation.key = score.key_map.front().key;
    modulation.key.root = SpelledPitch{4, 0, 4};
    modulation.key.accidentals = 1;
    score.key_map.push_back(modulation);

    const ScoreRegion region{SCORE_START, ScoreTime{3, Beat::zero()}, {}};
    const std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{1, 0, 4}, "major", std::nullopt},
        {ScoreTime{2, Beat::zero()}, SpelledPitch{1, 0, 4}, "major", std::nullopt}};

    const auto result = set_section_harmony(score, region, progression);
    REQUIRE(result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 2);
    CHECK(score.harmonic_annotations[0].roman_numeral == "II");
    CHECK(score.harmonic_annotations[0].key_context.root == SpelledPitch{0, 0, 4});
    CHECK(score.harmonic_annotations[1].roman_numeral == "V");
    CHECK(score.harmonic_annotations[1].key_context.root == SpelledPitch{4, 0, 4});
    CHECK(is_compilable(score));
}

TEST_CASE("set_section_harmony uses the stored mode rather than a fifths heuristic",
          "[score-ir][workflow][harmony][mode]") {
    ScoreSpec spec;
    spec.title = "A minor";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{5, 0, 4};
    spec.minor = true;
    PartDefinition piano;
    piano.name = "Piano";
    piano.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano);
    auto score = create_score(spec).value();
    REQUIRE(score.key_map.front().key.accidentals == 0);

    const ScoreRegion region{SCORE_START, ScoreTime{2, Beat::zero()}, {}};
    const std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{3, 0, 4}, "major", std::nullopt}};

    const auto result = set_section_harmony(score, region, progression);
    REQUIRE(result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    CHECK(score.harmonic_annotations.front().roman_numeral == "VI");
    CHECK(score.harmonic_annotations.front().key_context.mode.name == "minor");
    CHECK(is_compilable(score));
}

TEST_CASE("set_section_harmony rejects unrepresentable chords atomically",
          "[score-ir][workflow][harmony][atomicity]") {
    ScoreSpec spec;
    spec.title = "Atomic harmony";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition piano;
    piano.name = "Piano";
    piano.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano);
    const ScoreRegion region{SCORE_START, ScoreTime{2, Beat::zero()}, {}};

    SECTION("unknown quality") {
        auto score = create_score(spec).value();
        const auto version = score.version;
        const auto result = set_section_harmony(
            score, region, {{SCORE_START, SpelledPitch{0, 0, 4}, "unknown", std::nullopt}});
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::InvalidChordQuality);
        CHECK(score.version == version);
        CHECK(score.harmonic_annotations.empty());
    }

    SECTION("non-chord slash bass") {
        auto score = create_score(spec).value();
        const auto version = score.version;
        const auto result = set_section_harmony(
            score, region, {{SCORE_START, SpelledPitch{0, 0, 4}, "major", SpelledPitch{1, 0, 3}}});
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::InvalidMutation);
        CHECK(score.version == version);
        CHECK(score.harmonic_annotations.empty());
    }
}

// =============================================================================
// Undo support
// =============================================================================

TEST_CASE("set_formal_plan undo restores original section map", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Undo Test";
    spec.total_bars = 8;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    // Original: empty section map
    CHECK(score.section_map.empty());

    UndoStack undo_stack;
    std::vector<SectionDefinition> sections = {{"A", 1, 4, FormFunction::Expository},
                                               {"B", 5, 8, FormFunction::Developmental}};

    auto plan_result = set_formal_plan(score, sections, &undo_stack);
    REQUIRE(plan_result.has_value());
    REQUIRE(score.section_map.size() == 2);

    // Undo should restore the empty section map
    REQUIRE(undo_stack.can_undo());
    auto undo_result = undo(score, undo_stack);
    REQUIRE(undo_result.has_value());
    CHECK(score.section_map.empty());
}

TEST_CASE("set_section_harmony undo removes annotations from region", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Harmony Undo";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    // Initially no annotations
    CHECK(score.harmonic_annotations.empty());

    UndoStack undo_stack;
    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{3, Beat::zero()};

    std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{0, 0, 4}, "major", std::nullopt},
        {ScoreTime{2, Beat::zero()}, SpelledPitch{4, 0, 4}, "major", std::nullopt},
    };

    auto harm_result = set_section_harmony(score, region, progression, &undo_stack);
    REQUIRE(harm_result.has_value());
    CHECK(score.harmonic_annotations.size() == 2);

    // Undo should remove the annotations
    REQUIRE(undo_stack.can_undo());
    auto undo_result = undo(score, undo_stack);
    REQUIRE(undo_result.has_value());
    CHECK(score.harmonic_annotations.empty());
}

// =============================================================================
// Audit remediation: degree-based harmonic function classification
// =============================================================================

TEST_CASE("set_section_harmony classifies V7 as Dominant", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Function Test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4}; // C major
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    // G dominant 7th — should produce "V7" and classify as Dominant
    std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{4, 0, 4}, "7", std::nullopt}, // G7 → V7
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    CHECK(score.harmonic_annotations[0].function == ScoreHarmonicFunction::Dominant);
}

TEST_CASE("set_section_harmony classifies iii as Tonic", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Function Test 2";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4}; // C major
    spec.key_accidentals = 0;
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;

    PartDefinition piano_def;
    piano_def.name = "Piano";
    piano_def.abbreviation = "Pno.";
    piano_def.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(piano_def);

    auto result = create_score(spec);
    REQUIRE(result.has_value());
    auto& score = *result;

    ScoreRegion region;
    region.start = SCORE_START;
    region.end = ScoreTime{5, Beat::zero()};

    // E minor — should produce "iii" and classify as Tonic
    std::vector<ChordSymbolEntry> progression = {
        {SCORE_START, SpelledPitch{2, 0, 4}, "minor", std::nullopt}, // Em → iii
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    CHECK(score.harmonic_annotations[0].function == ScoreHarmonicFunction::Tonic);
}

// =============================================================================
// Entry-point validation (PS-1, PS-2)
// =============================================================================

TEST_CASE("create_score rejects BPM zero", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Bad BPM";
    spec.total_bars = 4;
    spec.bpm = 0;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto result = create_score(spec);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBPM);
}

TEST_CASE("create_score rejects BPM above max", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Fast";
    spec.total_bars = 4;
    spec.bpm = 1000;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto result = create_score(spec);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBPM);
}

TEST_CASE("set_formal_plan rejects start_bar zero", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Plan test";
    spec.total_bars = 8;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    std::vector<SectionDefinition> sections = {
        {"A", 0, 4, std::nullopt}, // start_bar == 0
    };
    auto result = set_formal_plan(score, sections);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBarRange);
}

TEST_CASE("set_formal_plan rejects end_bar < start_bar", "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Plan test";
    spec.total_bars = 8;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    std::vector<SectionDefinition> sections = {
        {"A", 5, 2, std::nullopt}, // end < start
    };
    auto result = set_formal_plan(score, sections);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBarRange);
}

TEST_CASE("part_extract nonexistent part returns PartNotFound", "[score-ir][view]") {
    ScoreSpec spec;
    spec.title = "Extract test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    auto result = part_extract(score, ScoreId{2}, PartId{99999});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::PartNotFound);
}

TEST_CASE("region_view rejects inverted region", "[score-ir][view]") {
    ScoreSpec spec;
    spec.title = "Region test";
    spec.total_bars = 8;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    ScoreRegion region;
    region.start = ScoreTime{5, Beat::zero()};
    region.end = ScoreTime{2, Beat::zero()}; // end < start

    auto result = region_view(score, ScoreId{2}, region);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidRegion);
}

// =============================================================================
// Annotation validation (PS-8)
// =============================================================================

TEST_CASE("S15 detects unsorted harmonic annotations", "[score-ir][validation]") {
    ScoreSpec spec;
    spec.title = "S15 test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    // Insert two annotations in reverse order
    HarmonicAnnotation ha1;
    ha1.position = ScoreTime{3, Beat::zero()};
    ha1.duration = Beat{1, 1};
    ha1.function = ScoreHarmonicFunction::Tonic;
    score.harmonic_annotations.push_back(ha1);

    HarmonicAnnotation ha2;
    ha2.position = ScoreTime{1, Beat::zero()};
    ha2.duration = Beat{1, 1};
    ha2.function = ScoreHarmonicFunction::Dominant;
    score.harmonic_annotations.push_back(ha2);

    auto diags = validate_structural(score);
    bool found_s15 = false;
    for (const auto& d : diags) {
        if (d.rule == "S15") found_s15 = true;
    }
    CHECK(found_s15);
}

TEST_CASE("S16 detects missing doubled_part on Doubling role", "[score-ir][validation]") {
    ScoreSpec spec;
    spec.title = "S16 test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno.";
    pd.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(pd);
    auto score = create_score(spec).value();

    OrchestrationAnnotation ann;
    ann.part_id = score.parts[0].id;
    ann.start = SCORE_START;
    ann.end = ScoreTime{5, Beat::zero()};
    ann.role = TexturalRole::Doubling;
    // deliberately omit doubled_part
    score.orchestration_annotations.push_back(ann);

    auto diags = validate_structural(score);
    bool found_s16 = false;
    for (const auto& d : diags) {
        if (d.rule == "S16") found_s16 = true;
    }
    CHECK(found_s16);
}

TEST_CASE("S16 advisory fields do not block structural compilation",
          "[score-ir][validation][compilation-policy]") {
    ScoreSpec spec;
    spec.title = "S16 warning test";
    spec.total_bars = 1;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};
    PartDefinition definition;
    definition.name = "Piano";
    definition.abbreviation = "Pno.";
    definition.instrument_type = InstrumentType::Piano;
    spec.parts.push_back(definition);
    auto score = create_score(spec).value();

    OrchestrationAnnotation annotation;
    annotation.part_id = score.parts[0].id;
    annotation.start = SCORE_START;
    annotation.end = ScoreTime{2, Beat::zero()};
    annotation.role = TexturalRole::Melody;
    annotation.doubled_part = score.parts[0].id;
    score.orchestration_annotations.push_back(annotation);

    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const Diagnostic& diagnostic) {
        return diagnostic.rule == "S16" && diagnostic.severity == ValidationSeverity::Warning;
    }));
    CHECK(is_compilable(score));
    CHECK(compile_to_midi(score).has_value());
}
