/**
 * @file TSCI001A.cpp
 * @brief Unit tests for Corpus IR types (CITP001A)
 *
 * Component: TSCI001A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CITP001A
 * Coverage: Identifiers, enumerations, value types, analysis records
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Corpus/CITP001A.h"

using namespace Sunny::Core;

// =============================================================================
// Identifiers
// =============================================================================

TEST_CASE("CITP001A: Identifier types are distinct", "[corpus-ir][types]") {
    IngestedWorkId wid{1};
    ComposerProfileId cid{1};
    ThematicUnitId tid{1};
    SignaturePatternId sid{1};

    CHECK(wid.value == 1);
    CHECK(cid.value == 1);
    CHECK(tid.value == 1);
    CHECK(sid.value == 1);
}

TEST_CASE("CITP001A: Identifier equality", "[corpus-ir][types]") {
    IngestedWorkId a{42}, b{42}, c{43};
    CHECK(a == b);
    CHECK_FALSE(a == c);
}

// =============================================================================
// Error Codes
// =============================================================================

TEST_CASE("CITP001A: Error codes are in 8000-8099 range", "[corpus-ir][types]") {
    CHECK(CorpusError::LowIngestionConfidence == 8000);
    CHECK(CorpusError::ScoreValidationFailed == 8001);
    CHECK(CorpusError::LowHarmonicCoverage == 8010);
    CHECK(CorpusError::SmallCorpus == 8020);
    CHECK(CorpusError::NotFound == 8030);
    CHECK(CorpusError::AnalysisFailed == 8034);
}

// =============================================================================
// IngestionConfidence
// =============================================================================

TEST_CASE("CITP001A: IngestionConfidence default values", "[corpus-ir][types]") {
    IngestionConfidence ic;
    CHECK(ic.key_confidence == 1.0f);
    CHECK(ic.metre_confidence == 1.0f);
    CHECK(ic.spelling_confidence == 1.0f);
    CHECK(ic.voice_separation_confidence == 1.0f);
    CHECK(ic.quantisation_residual == 0.0f);
    CHECK(ic.manual_corrections.empty());
}

TEST_CASE("CITP001A: IngestionConfidence for MIDI source", "[corpus-ir][types]") {
    IngestionConfidence ic;
    ic.key_confidence = 0.85f;
    ic.metre_confidence = 0.9f;
    ic.spelling_confidence = 0.7f;
    ic.voice_separation_confidence = 0.8f;
    ic.quantisation_residual = 12.5f;
    ic.source_format = "midi";

    CHECK(ic.key_confidence == Catch::Approx(0.85f));
    CHECK(ic.source_format == "midi");
}

// =============================================================================
// WorkMetadata
// =============================================================================

TEST_CASE("CITP001A: WorkMetadata construction", "[corpus-ir][types]") {
    WorkMetadata m;
    m.title = "Piano Sonata No. 14 in C# minor";
    m.composer = ComposerRef{1};
    m.opus = "Op. 27 No. 2";
    m.year_composed = 1801;
    m.period = "Middle";
    m.genre = "Piano Sonata";
    m.instrumentation = "Piano";
    m.source_format = "musicxml";
    m.is_reduction = false;

    CHECK(m.title == "Piano Sonata No. 14 in C# minor");
    REQUIRE(m.opus.has_value());
    CHECK(*m.opus == "Op. 27 No. 2");
    REQUIRE(m.year_composed.has_value());
    CHECK(*m.year_composed == 1801);
    CHECK(m.is_reduction == false);
}

// =============================================================================
// Harmonic Analysis Types
// =============================================================================

TEST_CASE("CITP001A: ProgressionPattern", "[corpus-ir][types]") {
    ProgressionPattern p;
    p.roman_numerals = {"I", "IV", "V7", "I"};
    p.length = 4;
    p.key_context = "C major";

    CHECK(p.roman_numerals.size() == 4);
    CHECK(p.length == 4);
    CHECK(p.key_context == "C major");
}

TEST_CASE("CITP001A: ModulationTechnique enum values", "[corpus-ir][types]") {
    CHECK(static_cast<int>(ModulationTechnique::PivotChord) == 0);
    CHECK(static_cast<int>(ModulationTechnique::Abrupt) == 6);
}

TEST_CASE("CITP001A: CadenceType enum values", "[corpus-ir][types]") {
    CHECK(static_cast<int>(CadenceType::PAC) == 0);
    CHECK(static_cast<int>(CadenceType::Phrygian) == 5);
}

TEST_CASE("CITP001A: HarmonicAnalysisRecord default", "[corpus-ir][types]") {
    HarmonicAnalysisRecord h;
    CHECK(h.chord_vocabulary.empty());
    CHECK(h.progression_inventory.empty());
    CHECK(h.modulation_inventory.empty());
    CHECK(h.cadence_inventory.empty());
    CHECK(h.harmonic_rhythm.mean_rate == 0.0f);
}

// =============================================================================
// Melodic Analysis Types
// =============================================================================

TEST_CASE("CITP001A: ContourShape enum values", "[corpus-ir][types]") {
    CHECK(static_cast<int>(ContourShape::Ascending) == 0);
    CHECK(static_cast<int>(ContourShape::Complex) == 8);
}

TEST_CASE("CITP001A: ThematicTransformation enum values", "[corpus-ir][types]") {
    CHECK(static_cast<int>(ThematicTransformation::Original) == 0);
    CHECK(static_cast<int>(ThematicTransformation::Developed) == 13);
}

TEST_CASE("CITP001A: ThematicUnit construction", "[corpus-ir][types]") {
    ThematicUnit tu;
    tu.id = ThematicUnitId{1};
    tu.label = "First Theme";
    tu.intervals = {2, 2, -1, 2};
    tu.contour = {1, 1, -1, 1};

    CHECK(tu.label == "First Theme");
    CHECK(tu.intervals.size() == 4);
    CHECK(tu.contour.size() == 4);
}

// =============================================================================
// Formal Analysis Types
// =============================================================================

TEST_CASE("CITP001A: FormClassification enum values", "[corpus-ir][types]") {
    CHECK(static_cast<int>(FormClassification::SonataAllegro) == 0);
    CHECK(static_cast<int>(FormClassification::Other) == 20);
}

TEST_CASE("CITP001A: FormalSection with subsections", "[corpus-ir][types]") {
    FormalSection expo;
    expo.label = "Exposition";
    expo.start_bar = 1;
    expo.end_bar = 60;
    expo.length_bars = 60;

    FormalSection theme1;
    theme1.label = "First Theme";
    theme1.start_bar = 1;
    theme1.end_bar = 20;
    theme1.length_bars = 20;

    expo.subsections.push_back(theme1);

    CHECK(expo.subsections.size() == 1);
    CHECK(expo.subsections[0].label == "First Theme");
}

// =============================================================================
// Style Profile Types
// =============================================================================

TEST_CASE("CITP001A: StyleProfile default values", "[corpus-ir][types]") {
    StyleProfile sp;
    CHECK(sp.sample_size == 0);
    CHECK(sp.confidence == 0.0f);
    CHECK(sp.signature_patterns.empty());
    CHECK(sp.harmonic_profile.chord_vocabulary_size == 0);
}

TEST_CASE("CITP001A: SignaturePattern construction", "[corpus-ir][types]") {
    SignaturePattern pat;
    pat.id = SignaturePatternId{1};
    pat.description = "Characteristic augmented sixth approach to cadence";
    pat.domain = PatternDomain::Cadential;
    pat.pattern_data = std::vector<std::string>{"bVI", "It6", "V"};
    pat.distinctiveness = 2.5f;

    CHECK(pat.domain == PatternDomain::Cadential);
    CHECK(pat.distinctiveness == Catch::Approx(2.5f));
    CHECK(std::holds_alternative<std::vector<std::string>>(pat.pattern_data));
}

// =============================================================================
// Comparison Types
// =============================================================================

TEST_CASE("CITP001A: StyleComparison default values", "[corpus-ir][types]") {
    StyleComparison sc;
    CHECK(sc.harmonic_divergence == 0.0f);
    CHECK(sc.melodic_divergence == 0.0f);
    CHECK(sc.most_similar_dimensions.empty());
}

TEST_CASE("CITP001A: TrendDirection enum", "[corpus-ir][types]") {
    CHECK(static_cast<int>(TrendDirection::Increasing) == 0);
    CHECK(static_cast<int>(TrendDirection::Stable) == 3);
}

// =============================================================================
// Query Result Types
// =============================================================================

TEST_CASE("CITP001A: AnnotatedExample default values", "[corpus-ir][types]") {
    AnnotatedExample ae;
    CHECK(ae.relevance_score == 0.0f);
    CHECK(ae.analysis_summary.empty());
    CHECK(ae.harmonic_reduction.empty());
}

TEST_CASE("CITP001A: HowWouldXHandleResult default", "[corpus-ir][types]") {
    HowWouldXHandleResult r;
    CHECK(r.relevant_examples.empty());
    CHECK(r.statistical_tendencies.empty());
    CHECK(r.signature_patterns.empty());
}

// =============================================================================
// WorkAnalysis aggregate
// =============================================================================

TEST_CASE("CITP001A: WorkAnalysis default construction", "[corpus-ir][types]") {
    WorkAnalysis wa;
    CHECK(wa.harmonic_analysis.chord_vocabulary.empty());
    CHECK(wa.melodic_analysis.per_voice_analysis.empty());
    CHECK(wa.rhythmic_analysis.syncopation_index == 0.0f);
    CHECK(wa.formal_analysis.form_type == FormClassification::Other);
    CHECK_FALSE(wa.orchestration_analysis.has_value());
    CHECK(wa.motivic_analysis.thematic_units.empty());
}

// =============================================================================
// PatternData variant
// =============================================================================

TEST_CASE("CITP001A: PatternData variant holds different types", "[corpus-ir][types]") {
    PatternData harmonic = std::vector<std::string>{"I", "IV", "V", "I"};
    CHECK(std::holds_alternative<std::vector<std::string>>(harmonic));

    PatternData melodic = std::vector<std::int8_t>{2, 2, -1, 2};
    CHECK(std::holds_alternative<std::vector<std::int8_t>>(melodic));

    PatternData rhythmic = std::vector<float>{1.0f, 0.5f, 0.5f, 2.0f};
    CHECK(std::holds_alternative<std::vector<float>>(rhythmic));

    PatternData textural = std::string{"parallel chord planing"};
    CHECK(std::holds_alternative<std::string>(textural));
}
