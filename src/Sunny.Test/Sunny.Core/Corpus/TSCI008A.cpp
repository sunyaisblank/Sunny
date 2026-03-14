/**
 * @file TSCI008A.cpp
 * @brief Unit tests for Corpus IR aggregation completeness
 *
 * Component: TSCI008A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CIWF001A (style profile aggregation for textural, dynamic,
 *        orchestration, and motivic domains)
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Corpus/CIWF001A.h"

using namespace Sunny::Core;

namespace {

/// Build a minimal work with populated analysis fields.
IngestedWork make_analyzed_work(
    IngestedWorkId id,
    float avg_density,
    float avg_span,
    float thematic_economy,
    float thematic_density,
    const std::string& dynamic_low,
    const std::string& dynamic_high,
    float dynamic_change_rate,
    std::uint32_t subito_count
) {
    IngestedWork work;
    work.id = id;
    work.analysis_complete = true;

    work.analysis.textural_analysis.average_density = avg_density;
    work.analysis.textural_analysis.average_register_span = avg_span;
    work.analysis.textural_analysis.density_curve.push_back(
        {ScoreTime{1, Beat::zero()}, static_cast<std::uint8_t>(avg_density)});

    work.analysis.dynamic_analysis.dynamic_range_low = dynamic_low;
    work.analysis.dynamic_analysis.dynamic_range_high = dynamic_high;
    work.analysis.dynamic_analysis.dynamic_change_rate = dynamic_change_rate;
    work.analysis.dynamic_analysis.subito_dynamics_count = subito_count;
    work.analysis.dynamic_analysis.dynamic_distribution[dynamic_low] = 5;
    work.analysis.dynamic_analysis.dynamic_distribution[dynamic_high] = 10;

    work.analysis.motivic_analysis.thematic_economy = thematic_economy;
    work.analysis.motivic_analysis.thematic_density = thematic_density;

    return work;
}

}  // anonymous namespace

// =============================================================================
// Textural aggregation
// =============================================================================

TEST_CASE("CIWF001A: Textural aggregation averages density and span", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 2.0f, 24.0f, 0, 0, "p", "f", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 4.0f, 36.0f, 0, 0, "pp", "ff", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)wf_assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    auto r = wf_rebuild_style_profile(db, ComposerProfileId{1});
    REQUIRE(r.has_value());

    const auto& tp = db.composers[1].style_profile.textural_profile;
    CHECK_THAT(tp.average_density, Catch::Matchers::WithinAbs(3.0, 0.01));
    CHECK_THAT(tp.register_span_preference, Catch::Matchers::WithinAbs(30.0, 0.01));
}

// =============================================================================
// Dynamic aggregation
// =============================================================================

TEST_CASE("CIWF001A: Dynamic aggregation finds extremes and most frequent", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0, 0, "pp", "f", 0.5f, 2);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0, 0, "p", "ff", 0.3f, 1);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)wf_assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)wf_rebuild_style_profile(db, ComposerProfileId{1});

    const auto& dp = db.composers[1].style_profile.dynamic_profile;
    CHECK(dp.dynamic_range_low == "pp");
    CHECK(dp.dynamic_range_high == "ff");
    CHECK_THAT(dp.dynamic_change_rate, Catch::Matchers::WithinAbs(0.4, 0.01));
    CHECK_THAT(dp.subito_frequency, Catch::Matchers::WithinAbs(1.5, 0.01));
    CHECK_FALSE(dp.most_frequent_dynamic.empty());
}

// =============================================================================
// Orchestration aggregation
// =============================================================================

TEST_CASE("CIWF001A: Orchestration aggregation merges instrument maps", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    IngestedWork w1;
    w1.id = IngestedWorkId{1};
    w1.analysis_complete = true;
    w1.analysis.orchestration_analysis = OrchestrationAnalysisRecord{};
    w1.analysis.orchestration_analysis->instrument_usage = {
        {"Violin", 0.8f}, {"Cello", 0.6f}
    };
    w1.analysis.orchestration_analysis->melody_carrier_distribution = {
        {"Violin", 0.9f}, {"Cello", 0.1f}
    };

    IngestedWork w2;
    w2.id = IngestedWorkId{2};
    w2.analysis_complete = true;
    w2.analysis.orchestration_analysis = OrchestrationAnalysisRecord{};
    w2.analysis.orchestration_analysis->instrument_usage = {
        {"Violin", 1.0f}, {"Viola", 0.5f}
    };
    w2.analysis.orchestration_analysis->melody_carrier_distribution = {
        {"Violin", 0.7f}, {"Viola", 0.3f}
    };

    db.works[1] = w1;
    db.works[2] = w2;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)wf_assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)wf_rebuild_style_profile(db, ComposerProfileId{1});

    const auto& sp = db.composers[1].style_profile;
    REQUIRE(sp.orchestration_profile.has_value());
    const auto& op = *sp.orchestration_profile;
    CHECK(op.preferred_instruments.count("Violin") > 0);
    CHECK_THAT(op.preferred_instruments.at("Violin"), Catch::Matchers::WithinAbs(0.9, 0.01));
}

// =============================================================================
// Motivic aggregation
// =============================================================================

TEST_CASE("CIWF001A: Motivic aggregation averages economy and density", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0.6f, 0.4f, "p", "f", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0.8f, 0.2f, "p", "f", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)wf_assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)wf_rebuild_style_profile(db, ComposerProfileId{1});

    const auto& mp = db.composers[1].style_profile.motivic_profile;
    CHECK_THAT(mp.thematic_economy, Catch::Matchers::WithinAbs(0.7, 0.01));
    CHECK_THAT(mp.development_density, Catch::Matchers::WithinAbs(0.3, 0.01));
}

// =============================================================================
// Full 9-domain rebuild
// =============================================================================

TEST_CASE("CIWF001A: rebuild_style_profile populates all sub-profiles", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 3.0f, 24.0f, 0.5f, 0.3f, "p", "ff", 0.4f, 1);
    w1.analysis.harmonic_analysis.chord_vocabulary = {{"I", 20}, {"V", 15}};
    w1.analysis.harmonic_analysis.harmonic_rhythm.mean_rate = 2.0f;
    w1.analysis.formal_analysis.total_duration_bars = 32;
    w1.analysis.formal_analysis.form_type = FormClassification::Ternary;
    w1.analysis.voice_leading_analysis.common_tone_retention_rate = 0.6f;
    w1.analysis.voice_leading_analysis.average_voice_independence = 0.5f;
    w1.analysis.rhythmic_analysis.syncopation_index = 0.1f;
    w1.analysis.rhythmic_analysis.metrical_complexity = 0.2f;

    VoiceMelodicAnalysis vma;
    vma.conjunct_proportion = 0.7f;
    vma.chromaticism_rate = 0.1f;
    w1.analysis.melodic_analysis.per_voice_analysis.push_back(vma);

    db.works[1] = w1;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});

    (void)wf_rebuild_style_profile(db, ComposerProfileId{1});

    const auto& sp = db.composers[1].style_profile;
    CHECK(sp.sample_size == 1);
    CHECK(sp.confidence > 0.0f);
    CHECK(sp.harmonic_profile.chord_vocabulary_size == 2);
    CHECK(sp.melodic_profile.conjunct_proportion > 0.0f);
    CHECK(sp.rhythmic_profile.syncopation_index > 0.0f);
    CHECK(sp.formal_profile.average_work_length > 0.0f);
    CHECK(sp.voice_leading_profile.common_tone_retention > 0.0f);
    CHECK(sp.textural_profile.average_density > 0.0f);
    CHECK_FALSE(sp.dynamic_profile.dynamic_range_low.empty());
    CHECK(sp.motivic_profile.thematic_economy > 0.0f);
}

// =============================================================================
// Motivic: zeros average correctly
// =============================================================================

TEST_CASE("CIWF001A: Motivic zeros average to zero", "[corpus-ir][aggregation]") {
    CorpusDatabase db;

    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Test");
    db.composers[1] = composer;

    auto w1 = make_analyzed_work(IngestedWorkId{1}, 1, 12, 0.0f, 0.0f, "mf", "mf", 0, 0);
    auto w2 = make_analyzed_work(IngestedWorkId{2}, 1, 12, 0.0f, 0.0f, "mf", "mf", 0, 0);
    db.works[1] = w1;
    db.works[2] = w2;
    (void)wf_assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)wf_assign_work_to_composer(db, IngestedWorkId{2}, ComposerProfileId{1});

    (void)wf_rebuild_style_profile(db, ComposerProfileId{1});

    const auto& mp = db.composers[1].style_profile.motivic_profile;
    CHECK_THAT(mp.thematic_economy, Catch::Matchers::WithinAbs(0.0, 0.001));
    CHECK_THAT(mp.development_density, Catch::Matchers::WithinAbs(0.0, 0.001));
}
