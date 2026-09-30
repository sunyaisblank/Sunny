/**
 * @file workflows_test.cpp
 * @brief Unit tests for Corpus IR workflow functions
 *
 *
 * Coverage: Profile management, analysis, query, comparison, evolution
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <sunny/core/corpus/workflows.hpp>

using namespace sunny::core;

namespace {

/// Populate a minimal corpus with two composers and analysed works.
CorpusDatabase make_corpus() {
    CorpusDatabase db;

    // Composer A
    auto profileA = create_composer_profile(ComposerProfileId{1}, "Composer A");
    db.composers[1] = std::move(profileA);

    // Composer B
    auto profileB = create_composer_profile(ComposerProfileId{2}, "Composer B");
    db.composers[2] = std::move(profileB);

    // Works for A
    for (std::uint64_t i = 1; i <= 5; ++i) {
        WorkMetadata meta;
        meta.title = "Work A-" + std::to_string(i);
        meta.composer = ComposerRef{1};
        meta.instrumentation = "Piano";
        meta.source_format = "musicxml";

        auto work = create_ingested_work(IngestedWorkId{i}, meta);
        work.analysis_complete = true;
        work.analysis.harmonic_analysis.chord_vocabulary = {
            {"I", 20 + static_cast<std::uint32_t>(i)}, {"V", 15}, {"IV", 10}};
        work.analysis.harmonic_analysis.harmonic_rhythm.mean_rate =
            2.0f + static_cast<float>(i) * 0.1f;
        work.analysis.formal_analysis.total_duration_bars = 100 + i * 10;
        work.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;
        work.analysis.melodic_analysis.per_voice_analysis.push_back({});
        work.analysis.melodic_analysis.per_voice_analysis[0].conjunct_proportion = 0.6f;
        work.analysis.melodic_analysis.per_voice_analysis[0].chromaticism_rate = 0.1f;
        work.analysis.rhythmic_analysis.syncopation_index = 0.2f;
        work.analysis.voice_leading_analysis.common_tone_retention_rate = 0.7f;
        work.analysis.voice_leading_analysis.average_voice_independence = 0.5f;

        db.works[i] = std::move(work);
        (void)assign_work_to_composer(db, IngestedWorkId{i}, ComposerProfileId{1});
    }

    // Works for B
    for (std::uint64_t i = 6; i <= 8; ++i) {
        WorkMetadata meta;
        meta.title = "Work B-" + std::to_string(i);
        meta.composer = ComposerRef{2};
        meta.instrumentation = "Orchestra";
        meta.source_format = "musicxml";

        auto work = create_ingested_work(IngestedWorkId{i}, meta);
        work.analysis_complete = true;
        work.analysis.harmonic_analysis.chord_vocabulary = {{"i", 25}, {"V", 20}, {"bVI", 8}};
        work.analysis.harmonic_analysis.harmonic_rhythm.mean_rate = 3.0f;
        work.analysis.formal_analysis.total_duration_bars = 200;
        work.analysis.formal_analysis.form_type = FormClassification::Ternary;

        db.works[i] = std::move(work);
        (void)assign_work_to_composer(db, IngestedWorkId{i}, ComposerProfileId{2});
    }

    return db;
}

} // anonymous namespace

// =============================================================================
// Profile Management
// =============================================================================

TEST_CASE("create_composer_profile", "[corpus-ir][workflow]") {
    auto p = create_composer_profile(ComposerProfileId{1}, "Test Composer");
    CHECK(p.id.value == 1);
    CHECK(p.name == "Test Composer");
    CHECK(p.works.empty());
}

TEST_CASE("create_ingested_work", "[corpus-ir][workflow]") {
    WorkMetadata meta;
    meta.title = "Nocturne";
    meta.composer = ComposerRef{1};
    meta.source_format = "midi";

    auto work = create_ingested_work(IngestedWorkId{1}, meta);
    CHECK(work.id.value == 1);
    CHECK(work.metadata.title == "Nocturne");
    CHECK_FALSE(work.analysis_complete);
}

TEST_CASE("assign_work_to_composer", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");

    WorkMetadata meta;
    meta.title = "Prelude";
    meta.source_format = "musicxml";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);

    auto r = assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    CHECK(r.has_value());
    CHECK(db.composers[1].works.size() == 1);
    CHECK(db.composers[1].works[0].value == 1);
}

TEST_CASE("assign_work_to_composer idempotent", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);

    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    (void)assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1});
    CHECK(db.composers[1].works.size() == 1);
}

TEST_CASE("idempotent composer assignment preserves period membership", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);
    PeriodProfile period;
    period.label = "Early";
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, period).has_value());
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());
    REQUIRE(
        assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early").has_value());

    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());

    CHECK(db.composers[1].period_profiles[0].works == std::vector{IngestedWorkId{1}});
    CHECK(db.works[1].metadata.period == "Early");
}

TEST_CASE("assign_work_to_composer transfers exclusive ownership", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");
    db.composers[2] = create_composer_profile(ComposerProfileId{2}, "Handel");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);

    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{2}).has_value());

    CHECK(db.composers[1].works.empty());
    CHECK(db.composers[2].works == std::vector{IngestedWorkId{1}});
    CHECK(db.works[1].metadata.composer == ComposerProfileId{2});
    CHECK(db.composers[1].style_profile.sample_size == 0);
    CHECK(db.composers[2].style_profile.sample_size == 0);
}

TEST_CASE("composer reassignment refreshes derived profiles and invalidates signatures",
          "[corpus-ir][workflow][lifecycle]") {
    auto db = make_corpus();
    REQUIRE(db.composers[1].style_profile.sample_size == 5);
    REQUIRE(db.composers[2].style_profile.sample_size == 3);
    db.composers[1].style_profile.signature_patterns.push_back({});

    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{2}).has_value());

    CHECK(db.composers[1].style_profile.sample_size == 4);
    CHECK(db.composers[2].style_profile.sample_size == 4);
    CHECK(db.composers[1].style_profile.signature_patterns.empty());
}

TEST_CASE("assign_work_to_composer fails for missing work", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");
    auto r = assign_work_to_composer(db, IngestedWorkId{999}, ComposerProfileId{1});
    CHECK_FALSE(r.has_value());
}

TEST_CASE("add_period_profile", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Beethoven");

    PeriodProfile pp;
    pp.label = "Early";
    pp.year_start = 1795;
    pp.year_end = 1802;
    auto r = add_period_profile(db, ComposerProfileId{1}, pp);
    CHECK(r.has_value());
    CHECK(db.composers[1].period_profiles.size() == 1);
}

TEST_CASE("add_period_profile rejects duplicate label", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Beethoven");

    PeriodProfile pp;
    pp.label = "Early";
    (void)add_period_profile(db, ComposerProfileId{1}, pp);
    auto r = add_period_profile(db, ComposerProfileId{1}, pp);
    CHECK_FALSE(r.has_value());
}

TEST_CASE("assign_work_to_period", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Beethoven");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());

    PeriodProfile pp;
    pp.label = "Early";
    (void)add_period_profile(db, ComposerProfileId{1}, pp);
    auto r = assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early");
    CHECK(r.has_value());
    CHECK(db.composers[1].period_profiles[0].works.size() == 1);
}

TEST_CASE("assign_work_to_period transfers exclusive membership", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Beethoven");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());

    PeriodProfile early;
    early.label = "Early";
    PeriodProfile late;
    late.label = "Late";
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, early).has_value());
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, late).has_value());

    REQUIRE(
        assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early").has_value());
    REQUIRE(assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Late").has_value());

    CHECK(db.composers[1].period_profiles[0].works.empty());
    CHECK(db.composers[1].period_profiles[1].works == std::vector{IngestedWorkId{1}});
    CHECK(db.works[1].metadata.period == "Late");
    CHECK(db.composers[1].period_profiles[0].profile.sample_size == 0);
    CHECK(db.composers[1].period_profiles[1].profile.sample_size == 0);
}

TEST_CASE("period assignment and clearing synchronise metadata and analysed aggregates",
          "[corpus-ir][workflow][lifecycle]") {
    auto db = make_corpus();
    PeriodProfile early;
    early.label = "Early";
    early.year_start = 1800;
    early.year_end = 1810;
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, early).has_value());
    db.composers[1].style_profile.signature_patterns.push_back({});

    REQUIRE(
        assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early").has_value());
    REQUIRE(db.works[1].metadata.period == "Early");
    CHECK(db.composers[1].period_profiles[0].profile.sample_size == 1);
    CHECK(db.composers[1].style_profile.signature_patterns.size() == 1);

    REQUIRE(clear_work_period_assignment(db, IngestedWorkId{1}).has_value());
    CHECK_FALSE(db.works[1].metadata.period.has_value());
    CHECK(db.composers[1].period_profiles[0].works.empty());
    CHECK(db.composers[1].period_profiles[0].profile.sample_size == 0);
    CHECK(db.composers[1].style_profile.signature_patterns.size() == 1);
}

TEST_CASE("add_period_profile rejects bounded overlap without mutation",
          "[corpus-ir][workflow][transaction]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Composer");
    PeriodProfile early;
    early.label = "Early";
    early.year_start = 1800;
    early.year_end = 1810;
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, early).has_value());

    PeriodProfile overlap;
    overlap.label = "Middle";
    overlap.year_start = 1810;
    overlap.year_end = 1820;
    auto result = add_period_profile(db, ComposerProfileId{1}, overlap);

    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::CorpusInvalidParameter);
    CHECK(db.composers[1].period_profiles.size() == 1);
}

TEST_CASE("remove_ingested_work cleans reverse references and refreshes profiles",
          "[corpus-ir][workflow][lifecycle][transaction]") {
    auto db = make_corpus();
    PeriodProfile early;
    early.label = "Early";
    REQUIRE(add_period_profile(db, ComposerProfileId{1}, early).has_value());
    REQUIRE(
        assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{1}, "Early").has_value());
    db.composers[1].style_profile.signature_patterns.push_back({});

    REQUIRE(remove_ingested_work(db, IngestedWorkId{1}).has_value());

    CHECK_FALSE(db.works.contains(1));
    CHECK(std::ranges::find(db.composers[1].works, IngestedWorkId{1}) ==
          db.composers[1].works.end());
    CHECK(db.composers[1].period_profiles[0].works.empty());
    CHECK(db.composers[1].style_profile.sample_size == 4);
    CHECK(db.composers[1].period_profiles[0].profile.sample_size == 0);
    CHECK(db.composers[1].style_profile.signature_patterns.empty());

    const auto work_count = db.works.size();
    const auto composer_works = db.composers[1].works;
    auto missing = remove_ingested_work(db, IngestedWorkId{999});
    REQUIRE_FALSE(missing.has_value());
    CHECK(db.works.size() == work_count);
    CHECK(db.composers[1].works == composer_works);
}

TEST_CASE("assign_work_to_period rejects a different composer owner", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Bach");
    db.composers[2] = create_composer_profile(ComposerProfileId{2}, "Handel");
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);
    REQUIRE(assign_work_to_composer(db, IngestedWorkId{1}, ComposerProfileId{1}).has_value());

    PeriodProfile period;
    period.label = "Early";
    REQUIRE(add_period_profile(db, ComposerProfileId{2}, period).has_value());

    auto result = assign_work_to_period(db, IngestedWorkId{1}, ComposerProfileId{2}, "Early");
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::InvalidMutation);
}

// =============================================================================
// Analysis
// =============================================================================

TEST_CASE("analyze_work refuses a work without a Score", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    WorkMetadata meta;
    meta.source_format = "midi";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, meta);

    auto r = analyze_work(db, IngestedWorkId{1});
    REQUIRE_FALSE(r.has_value());
    CHECK(r.error() == ErrorCode::InvalidMutation);
    CHECK_FALSE(db.works[1].analysis_complete);
}

TEST_CASE("analyze_work fails for missing work", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto r = analyze_work(db, IngestedWorkId{999});
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Rebuild Style Profile
// =============================================================================

TEST_CASE("rebuild_style_profile aggregates works", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    auto r = rebuild_style_profile(db, ComposerProfileId{1});
    CHECK(r.has_value());

    const auto& sp = db.composers[1].style_profile;
    CHECK(sp.sample_size == 5);
    CHECK(sp.confidence > 0.0f);
    CHECK(sp.harmonic_profile.chord_vocabulary_size == 3); // I, V, IV
    CHECK(sp.harmonic_profile.harmonic_rhythm_mean > 2.0f);
    CHECK(sp.melodic_profile.conjunct_proportion > 0.0f);
}

TEST_CASE("rebuild_style_profile fails for missing composer", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto r = rebuild_style_profile(db, ComposerProfileId{999});
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Detect Signature Patterns
// =============================================================================

TEST_CASE("detect_signature_patterns", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    // Add a recurring progression to one of A's works
    ProgressionPattern prog;
    prog.roman_numerals = {"I", "IV", "V", "I"};
    prog.length = 4;
    prog.occurrences = {
        ScoreTime{1, Beat{0, 1}}, ScoreTime{20, Beat{0, 1}}, ScoreTime{50, Beat{0, 1}}};
    db.works[1].analysis.harmonic_analysis.progression_inventory.push_back(prog);

    auto r = detect_signature_patterns(db, ComposerProfileId{1});
    CHECK(r.has_value());
    CHECK_FALSE(db.composers[1].style_profile.signature_patterns.empty());
}

// =============================================================================
// Query: Style Profile
// =============================================================================

TEST_CASE("query_style_profile", "[corpus-ir][workflow]") {
    auto db = make_corpus();
    (void)rebuild_style_profile(db, ComposerProfileId{1});

    auto r = query_style_profile(db, ComposerProfileId{1});
    CHECK(r.has_value());
    CHECK((*r)->sample_size == 5);
}

TEST_CASE("query_style_profile fails for missing composer", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto r = query_style_profile(db, ComposerProfileId{999});
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Query: Find Examples
// =============================================================================

TEST_CASE("find_examples by section label", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    // Add a formal section to one work
    FormalSection sec;
    sec.label = "Development";
    sec.start_bar = 50;
    sec.end_bar = 100;
    sec.length_bars = 50;
    db.works[1].analysis.formal_analysis.section_plan.push_back(sec);

    auto results = find_examples(db, ComposerProfileId{1}, "Development");
    CHECK_FALSE(results.empty());
    CHECK(results[0].formal_context == "Development");
}

TEST_CASE("find_examples returns empty for no match", "[corpus-ir][workflow]") {
    auto db = make_corpus();
    auto results = find_examples(db, ComposerProfileId{1}, "Nonexistent");
    CHECK(results.empty());
}

// =============================================================================
// Query: Progression Examples
// =============================================================================

TEST_CASE("get_progression_examples", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    ProgressionPattern prog;
    prog.roman_numerals = {"I", "V"};
    prog.length = 2;
    prog.occurrences = {ScoreTime{10, Beat{0, 1}}};
    db.works[1].analysis.harmonic_analysis.progression_inventory.push_back(prog);

    auto results = get_progression_examples(db, ComposerProfileId{1}, {"I", "V"});
    CHECK(results.size() == 1);
    CHECK(results[0].relevance_score == 1.0f);
}

// =============================================================================
// Query: Formal Template
// =============================================================================

TEST_CASE("get_formal_template", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    auto r = get_formal_template(db, ComposerProfileId{1}, FormClassification::SonataAllegro);
    CHECK(r.has_value());
    CHECK(r->average_work_length > 0.0f);
}

TEST_CASE("get_formal_template for unrepresented form", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    auto r = get_formal_template(db, ComposerProfileId{1}, FormClassification::Fugue);
    CHECK(r.has_value());
    CHECK(r->average_work_length == 0.0f);
}

// =============================================================================
// Query: HowWouldXHandle
// =============================================================================

TEST_CASE("how_would_x_handle returns tendencies", "[corpus-ir][workflow]") {
    auto db = make_corpus();
    (void)rebuild_style_profile(db, ComposerProfileId{1});

    auto r = how_would_x_handle(db, ComposerProfileId{1}, "development section");
    CHECK(r.has_value());
    CHECK_FALSE(r->statistical_tendencies.empty());
}

TEST_CASE("how_would_x_handle fails for missing composer", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto r = how_would_x_handle(db, ComposerProfileId{999}, "test");
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Comparison
// =============================================================================

TEST_CASE("compare_composers", "[corpus-ir][workflow]") {
    auto db = make_corpus();
    (void)rebuild_style_profile(db, ComposerProfileId{1});
    (void)rebuild_style_profile(db, ComposerProfileId{2});

    auto r = compare_composers(db, ComposerProfileId{1}, ComposerProfileId{2});
    CHECK(r.has_value());
    CHECK(r->harmonic_divergence >= 0.0f);
    CHECK_FALSE(r->most_similar_dimensions.empty());
    CHECK_FALSE(r->most_divergent_dimensions.empty());
}

TEST_CASE("compare_composers fails for missing", "[corpus-ir][workflow]") {
    auto db = make_corpus();
    auto r = compare_composers(db, ComposerProfileId{1}, ComposerProfileId{999});
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Evolution
// =============================================================================

TEST_CASE("analyze_evolution with periods", "[corpus-ir][workflow]") {
    auto db = make_corpus();

    // Add period profiles with different style characteristics
    PeriodProfile early;
    early.label = "Early";
    early.year_start = 1800;
    early.year_end = 1810;
    early.profile.harmonic_profile.chord_vocabulary_size = 10;
    early.profile.harmonic_profile.chromatic_density = 0.05f;

    PeriodProfile late;
    late.label = "Late";
    late.year_start = 1820;
    late.year_end = 1830;
    late.profile.harmonic_profile.chord_vocabulary_size = 25;
    late.profile.harmonic_profile.chromatic_density = 0.2f;

    db.composers[1].period_profiles = {early, late};

    auto r = analyze_evolution(db, ComposerProfileId{1});
    CHECK(r.has_value());
    CHECK(r->periods.size() == 2);
    CHECK(r->trends.size() >= 2);

    // Should detect increasing harmonic vocabulary
    bool found_increasing = false;
    for (const auto& t : r->trends) {
        if (t.dimension == "harmonic_vocabulary" && t.direction == TrendDirection::Increasing) {
            found_increasing = true;
        }
    }
    CHECK(found_increasing);
}

TEST_CASE("analyze_evolution fails for missing composer", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto r = analyze_evolution(db, ComposerProfileId{999});
    CHECK_FALSE(r.has_value());
}

// =============================================================================
// Validation wrapper
// =============================================================================

TEST_CASE("validate_corpus", "[corpus-ir][workflow]") {
    CorpusDatabase db;
    auto diags = validate_corpus(db);
    CHECK(diags.empty());
}
