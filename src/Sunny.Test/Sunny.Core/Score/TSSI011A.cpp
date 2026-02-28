/**
 * @file TSSI011A.cpp
 * @brief Unit tests for Score IR workflow functions (SIWF001A)
 *
 * Component: TSSI011A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SIWF001A
 * Coverage: create_score, set_formal_plan, set_section_harmony
 */

#include <catch2/catch_test_macros.hpp>

#include "Score/SIWF001A.h"
#include "Score/SIVD001A.h"
#include "Score/SIVW001A.h"

using namespace Sunny::Core;

// =============================================================================
// create_score
// =============================================================================

TEST_CASE("SIWF001A: create_score produces valid score",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Symphony No. 1";
    spec.total_bars = 16;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};  // C4
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

TEST_CASE("SIWF001A: create_score with 3 parts creates 3 parts with correct bar count",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Trio";
    spec.total_bars = 8;
    spec.bpm = 100;
    spec.key_root = SpelledPitch{4, 0, 4};  // G4
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

// =============================================================================
// set_formal_plan
// =============================================================================

TEST_CASE("SIWF001A: set_formal_plan sets section_map with correct bar ranges",
          "[score-ir][workflow]") {
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
        {"Recapitulation", 13, 16, FormFunction::Expository}
    };

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

// =============================================================================
// set_section_harmony
// =============================================================================

TEST_CASE("SIWF001A: set_section_harmony writes annotations with Roman numerals",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Harmony Test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};  // C major
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
        {SCORE_START, SpelledPitch{0, 0, 4}, "major", std::nullopt},       // C major → I
        {ScoreTime{2, Beat::zero()}, SpelledPitch{3, 0, 4}, "major", std::nullopt},  // F major → IV
        {ScoreTime{3, Beat::zero()}, SpelledPitch{4, 0, 4}, "major", std::nullopt},  // G major → V
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());

    // Should have 3 harmonic annotations
    REQUIRE(score.harmonic_annotations.size() == 3);

    // Check Roman numerals are computed
    CHECK(score.harmonic_annotations[0].roman_numeral == "I");
    CHECK(score.harmonic_annotations[1].roman_numeral == "IV");
    CHECK(score.harmonic_annotations[2].roman_numeral == "V");
}

// =============================================================================
// Undo support
// =============================================================================

TEST_CASE("SIWF001A: set_formal_plan undo restores original section map",
          "[score-ir][workflow]") {
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
    std::vector<SectionDefinition> sections = {
        {"A", 1, 4, FormFunction::Expository},
        {"B", 5, 8, FormFunction::Developmental}
    };

    auto plan_result = set_formal_plan(score, sections, &undo_stack);
    REQUIRE(plan_result.has_value());
    REQUIRE(score.section_map.size() == 2);

    // Undo should restore the empty section map
    REQUIRE(undo_stack.can_undo());
    auto undo_result = undo(score, undo_stack);
    REQUIRE(undo_result.has_value());
    CHECK(score.section_map.empty());
}

TEST_CASE("SIWF001A: set_section_harmony undo removes annotations from region",
          "[score-ir][workflow]") {
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

TEST_CASE("SIWF001A: set_section_harmony classifies V7 as Dominant",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Function Test";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};  // C major
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
        {SCORE_START, SpelledPitch{4, 0, 4}, "7", std::nullopt},  // G7 → V7
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    CHECK(score.harmonic_annotations[0].function == ScoreHarmonicFunction::Dominant);
}

TEST_CASE("SIWF001A: set_section_harmony classifies iii as Tonic",
          "[score-ir][workflow]") {
    ScoreSpec spec;
    spec.title = "Function Test 2";
    spec.total_bars = 4;
    spec.bpm = 120;
    spec.key_root = SpelledPitch{0, 0, 4};  // C major
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
        {SCORE_START, SpelledPitch{2, 0, 4}, "minor", std::nullopt},  // Em → iii
    };

    auto harm_result = set_section_harmony(score, region, progression);
    REQUIRE(harm_result.has_value());
    REQUIRE(score.harmonic_annotations.size() == 1);
    CHECK(score.harmonic_annotations[0].function == ScoreHarmonicFunction::Tonic);
}

// =============================================================================
// Entry-point validation (PS-1, PS-2)
// =============================================================================

TEST_CASE("SIWF001A: create_score rejects BPM zero",
          "[score-ir][workflow]") {
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

TEST_CASE("SIWF001A: create_score rejects BPM above max",
          "[score-ir][workflow]") {
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

TEST_CASE("SIWF001A: set_formal_plan rejects start_bar zero",
          "[score-ir][workflow]") {
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
        {"A", 0, 4, std::nullopt},  // start_bar == 0
    };
    auto result = set_formal_plan(score, sections);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBarRange);
}

TEST_CASE("SIWF001A: set_formal_plan rejects end_bar < start_bar",
          "[score-ir][workflow]") {
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
        {"A", 5, 2, std::nullopt},  // end < start
    };
    auto result = set_formal_plan(score, sections);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidBarRange);
}

TEST_CASE("SIVW001A: part_extract nonexistent part returns PartNotFound",
          "[score-ir][view]") {
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

    auto result = part_extract(score, PartId{99999});
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::PartNotFound);
}

TEST_CASE("SIVW001A: region_view rejects inverted region",
          "[score-ir][view]") {
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
    region.end = ScoreTime{2, Beat::zero()};  // end < start

    auto result = region_view(score, region);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidRegion);
}

// =============================================================================
// Annotation validation (PS-8)
// =============================================================================

TEST_CASE("SIVD001A: S15 detects unsorted harmonic annotations",
          "[score-ir][validation]") {
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

TEST_CASE("SIVD001A: S16 detects missing doubled_part on Doubling role",
          "[score-ir][validation]") {
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
