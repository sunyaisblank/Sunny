/**
 * @file TSCI002A.cpp
 * @brief Unit tests for Corpus IR document model (CIDC001A)
 *
 * Component: TSCI002A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CIDC001A
 * Coverage: IngestedWork, ComposerProfile, PeriodProfile, CorpusDatabase
 */

#include <catch2/catch_test_macros.hpp>

#include "Corpus/CIDC001A.h"

using namespace Sunny::Core;

// =============================================================================
// IngestedWork
// =============================================================================

TEST_CASE("CIDC001A: IngestedWork default values", "[corpus-ir][document]") {
    IngestedWork work;
    CHECK(work.metadata.title.empty());
    CHECK_FALSE(work.analysis_complete);
    CHECK(work.ingestion_confidence.key_confidence == 1.0f);
}

TEST_CASE("CIDC001A: IngestedWork with metadata", "[corpus-ir][document]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.metadata.title = "Prelude in C major";
    work.metadata.composer = ComposerRef{10};
    work.metadata.opus = "BWV 846";
    work.metadata.year_composed = 1722;
    work.metadata.instrumentation = "Keyboard";
    work.metadata.source_format = "musicxml";

    CHECK(work.id.value == 1);
    CHECK(work.metadata.title == "Prelude in C major");
    REQUIRE(work.metadata.opus.has_value());
    CHECK(*work.metadata.opus == "BWV 846");
}

// =============================================================================
// ComposerProfile
// =============================================================================

TEST_CASE("CIDC001A: ComposerProfile default values", "[corpus-ir][document]") {
    ComposerProfile profile;
    CHECK(profile.name.empty());
    CHECK(profile.works.empty());
    CHECK(profile.period_profiles.empty());
    CHECK(profile.style_profile.sample_size == 0);
}

TEST_CASE("CIDC001A: ComposerProfile with works and periods", "[corpus-ir][document]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Ludwig van Beethoven";
    profile.birth_year = 1770;
    profile.death_year = 1827;
    profile.tradition = "Western classical";

    profile.works = {IngestedWorkId{1}, IngestedWorkId{2}, IngestedWorkId{3}};

    PeriodProfile early;
    early.label = "Early";
    early.year_start = 1795;
    early.year_end = 1802;
    early.works = {IngestedWorkId{1}};

    PeriodProfile middle;
    middle.label = "Middle";
    middle.year_start = 1803;
    middle.year_end = 1812;
    middle.works = {IngestedWorkId{2}};

    profile.period_profiles = {early, middle};

    CHECK(profile.name == "Ludwig van Beethoven");
    REQUIRE(profile.birth_year.has_value());
    CHECK(*profile.birth_year == 1770);
    CHECK(profile.works.size() == 3);
    CHECK(profile.period_profiles.size() == 2);
    CHECK(profile.period_profiles[0].label == "Early");
}

// =============================================================================
// PeriodProfile
// =============================================================================

TEST_CASE("CIDC001A: PeriodProfile construction", "[corpus-ir][document]") {
    PeriodProfile pp;
    pp.label = "Late";
    pp.year_start = 1813;
    pp.year_end = 1826;
    pp.works = {IngestedWorkId{10}, IngestedWorkId{11}};
    pp.profile.sample_size = 2;

    CHECK(pp.label == "Late");
    CHECK(pp.year_start == 1813);
    CHECK(pp.works.size() == 2);
    CHECK(pp.profile.sample_size == 2);
}

// =============================================================================
// EvolutionaryAnalysis
// =============================================================================

TEST_CASE("CIDC001A: EvolutionaryAnalysis default", "[corpus-ir][document]") {
    EvolutionaryAnalysis ea;
    CHECK(ea.periods.empty());
    CHECK(ea.trends.empty());
}

// =============================================================================
// CorpusDatabase
// =============================================================================

TEST_CASE("CIDC001A: CorpusDatabase default values", "[corpus-ir][document]") {
    CorpusDatabase db;
    CHECK(db.composers.empty());
    CHECK(db.works.empty());
}

TEST_CASE("CIDC001A: CorpusDatabase with composers and works", "[corpus-ir][document]") {
    CorpusDatabase db;

    ComposerProfile bach;
    bach.id = ComposerProfileId{1};
    bach.name = "Johann Sebastian Bach";
    bach.works = {IngestedWorkId{1}, IngestedWorkId{2}};
    db.composers[1] = bach;

    IngestedWork work1;
    work1.id = IngestedWorkId{1};
    work1.metadata.title = "Prelude in C";
    db.works[1] = work1;

    IngestedWork work2;
    work2.id = IngestedWorkId{2};
    work2.metadata.title = "Fugue in C";
    db.works[2] = work2;

    CHECK(db.composers.size() == 1);
    CHECK(db.works.size() == 2);
    CHECK(db.composers[1].works.size() == 2);
}
