/**
 * @file TSCI004A.cpp
 * @brief Unit tests for Corpus IR serialisation (CISZ001A)
 *
 * Component: TSCI004A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CISZ001A
 * Coverage: JSON round-trip for IngestedWork, ComposerProfile, CorpusDatabase
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "Corpus/CISZ001A.h"

using namespace Sunny::Core;

// =============================================================================
// IngestedWork round-trip
// =============================================================================

TEST_CASE("CISZ001A: IngestedWork round-trip", "[corpus-ir][serialisation]") {
    IngestedWork work;
    work.id = IngestedWorkId{42};
    work.metadata.title = "Moonlight Sonata";
    work.metadata.composer = ComposerRef{1};
    work.metadata.opus = "Op. 27 No. 2";
    work.metadata.year_composed = 1801;
    work.metadata.instrumentation = "Piano";
    work.metadata.source_format = "musicxml";
    work.metadata.tags = {"sonata", "piano"};
    work.ingestion_confidence.key_confidence = 0.95f;
    work.ingestion_confidence.source_format = "musicxml";
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 30}, {"V", 25}, {"IV", 15}};
    work.analysis.formal_analysis.total_duration_bars = 200;
    work.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;

    auto j = ingested_work_to_json(work);
    auto result = ingested_work_from_json(j);
    REQUIRE(result.has_value());

    auto& w = *result;
    CHECK(w.id.value == 42);
    CHECK(w.metadata.title == "Moonlight Sonata");
    REQUIRE(w.metadata.opus.has_value());
    CHECK(*w.metadata.opus == "Op. 27 No. 2");
    CHECK(w.analysis_complete);
    CHECK(w.analysis.harmonic_analysis.chord_vocabulary.size() == 3);
    CHECK(w.analysis.harmonic_analysis.chord_vocabulary.at("I") == 30);
}

// =============================================================================
// ComposerProfile round-trip
// =============================================================================

TEST_CASE("CISZ001A: ComposerProfile round-trip", "[corpus-ir][serialisation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Frederic Chopin";
    profile.birth_year = 1810;
    profile.death_year = 1849;
    profile.tradition = "Western classical";
    profile.works = {IngestedWorkId{1}, IngestedWorkId{2}};
    profile.tags = {"Romantic", "piano"};

    profile.style_profile.sample_size = 2;
    profile.style_profile.confidence = 0.4f;
    profile.style_profile.harmonic_profile.chord_vocabulary_size = 20;
    profile.style_profile.harmonic_profile.chord_frequency = {{"I", 0.3f}, {"V", 0.2f}};

    PeriodProfile pp;
    pp.label = "Paris";
    pp.year_start = 1831;
    pp.year_end = 1849;
    pp.works = {IngestedWorkId{1}};
    profile.period_profiles.push_back(pp);

    auto j = composer_profile_to_json(profile);
    auto result = composer_profile_from_json(j);
    REQUIRE(result.has_value());

    auto& p = *result;
    CHECK(p.id.value == 1);
    CHECK(p.name == "Frederic Chopin");
    REQUIRE(p.birth_year.has_value());
    CHECK(*p.birth_year == 1810);
    CHECK(p.works.size() == 2);
    CHECK(p.style_profile.sample_size == 2);
    CHECK(p.style_profile.confidence == Catch::Approx(0.4f));
    CHECK(p.style_profile.harmonic_profile.chord_vocabulary_size == 20);
    CHECK(p.period_profiles.size() == 1);
    CHECK(p.period_profiles[0].label == "Paris");
}

// =============================================================================
// CorpusDatabase round-trip
// =============================================================================

TEST_CASE("CISZ001A: CorpusDatabase round-trip", "[corpus-ir][serialisation]") {
    CorpusDatabase db;

    ComposerProfile bach;
    bach.id = ComposerProfileId{1};
    bach.name = "J.S. Bach";
    bach.works = {IngestedWorkId{1}};
    db.composers[1] = bach;

    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.metadata.title = "WTC Prelude 1";
    work.metadata.composer = ComposerRef{1};
    work.metadata.source_format = "midi";
    work.ingestion_confidence.source_format = "midi";
    db.works[1] = work;

    auto j = corpus_to_json(db);
    auto result = corpus_from_json(j);
    REQUIRE(result.has_value());

    auto& c = *result;
    CHECK(c.composers.size() == 1);
    CHECK(c.works.size() == 1);
    CHECK(c.composers[1].name == "J.S. Bach");
    CHECK(c.works[1].metadata.title == "WTC Prelude 1");
}

// =============================================================================
// String round-trip
// =============================================================================

TEST_CASE("CISZ001A: corpus_to/from_json_string round-trip", "[corpus-ir][serialisation]") {
    CorpusDatabase db;
    ComposerProfile p;
    p.id = ComposerProfileId{1};
    p.name = "Test";
    db.composers[1] = p;

    auto str = corpus_to_json_string(db);
    auto result = corpus_from_json_string(str);
    REQUIRE(result.has_value());
    CHECK(result->composers.size() == 1);
    CHECK(result->composers[1].name == "Test");
}

// =============================================================================
// Schema version
// =============================================================================

TEST_CASE("CISZ001A: rejects wrong schema version", "[corpus-ir][serialisation]") {
    nlohmann::json j = {{"schema_version", 999}, {"id", 1}, {"name", "X"}};
    auto result = composer_profile_from_json(j);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Formal analysis round-trip
// =============================================================================

TEST_CASE("CISZ001A: formal analysis with sections survives round-trip", "[corpus-ir][serialisation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.metadata.title = "Test";
    work.metadata.source_format = "midi";
    work.ingestion_confidence.source_format = "midi";
    work.analysis_complete = true;

    FormalSection expo;
    expo.label = "Exposition";
    expo.start_bar = 1;
    expo.end_bar = 60;
    expo.length_bars = 60;
    expo.key = "C major";
    expo.character = "energetic";

    FormalSection dev;
    dev.label = "Development";
    dev.start_bar = 61;
    dev.end_bar = 120;
    dev.length_bars = 60;
    dev.key = "G major";

    work.analysis.formal_analysis.section_plan = {expo, dev};
    work.analysis.formal_analysis.form_type = FormClassification::SonataAllegro;
    work.analysis.formal_analysis.total_duration_bars = 200;

    auto j = ingested_work_to_json(work);
    auto result = ingested_work_from_json(j);
    REQUIRE(result.has_value());

    auto& fa = result->analysis.formal_analysis;
    CHECK(fa.section_plan.size() == 2);
    CHECK(fa.section_plan[0].label == "Exposition");
    CHECK(fa.section_plan[0].key == "C major");
    REQUIRE(fa.section_plan[0].character.has_value());
    CHECK(*fa.section_plan[0].character == "energetic");
    CHECK(fa.form_type == FormClassification::SonataAllegro);
    CHECK(fa.total_duration_bars == 200);
}

// =============================================================================
// Empty corpus
// =============================================================================

TEST_CASE("CISZ001A: empty corpus round-trip", "[corpus-ir][serialisation]") {
    CorpusDatabase db;
    auto j = corpus_to_json(db);
    auto result = corpus_from_json(j);
    REQUIRE(result.has_value());
    CHECK(result->composers.empty());
    CHECK(result->works.empty());
}
