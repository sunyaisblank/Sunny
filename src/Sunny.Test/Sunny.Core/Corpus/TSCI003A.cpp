/**
 * @file TSCI003A.cpp
 * @brief Unit tests for Corpus IR validation (CIVD001A)
 *
 * Component: TSCI003A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CIVD001A
 * Coverage: C1–C13 validation rules
 */

#include <catch2/catch_test_macros.hpp>

#include "Corpus/CIAN001A.h"
#include "Corpus/CIVD001A.h"
#include "Corpus/CIWF001A.h"
#include "Score/SIWF001A.h"

using namespace Sunny::Core;

namespace {

bool has_rule(const std::vector<Diagnostic>& diags, const std::string& rule) {
    for (const auto& d : diags)
        if (d.rule == rule) return true;
    return false;
}

bool has_error(const std::vector<Diagnostic>& diags) {
    for (const auto& d : diags)
        if (d.severity == ValidationSeverity::Error) return true;
    return false;
}

}  // anonymous namespace

// =============================================================================
// C1: Ingestion confidence below 0.7
// =============================================================================

TEST_CASE("CIVD001A: C1 warns for low ingestion confidence", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.ingestion_confidence.key_confidence = 0.6f;
    work.ingestion_confidence.source_format = "midi";

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C1"));
}

TEST_CASE("CIVD001A: C1 does not warn for high confidence", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.ingestion_confidence.key_confidence = 1.0f;
    work.ingestion_confidence.metre_confidence = 1.0f;
    work.ingestion_confidence.spelling_confidence = 1.0f;
    work.ingestion_confidence.voice_separation_confidence = 1.0f;
    work.ingestion_confidence.source_format = "musicxml";

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C1"));
}

// =============================================================================
// C3: Key estimation confidence below 0.5
// =============================================================================

TEST_CASE("CIVD001A: C3 warns for very low key confidence", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.ingestion_confidence.key_confidence = 0.4f;
    work.ingestion_confidence.source_format = "midi";

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C3"));
}

// =============================================================================
// C4: Inferred time signature
// =============================================================================

TEST_CASE("CIVD001A: C4 reports inferred time signature for MIDI", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.ingestion_confidence.metre_confidence = 0.9f;
    work.ingestion_confidence.source_format = "midi";

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C4"));
}

TEST_CASE("CIVD001A: C4 does not report for MusicXML", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.ingestion_confidence.source_format = "musicxml";

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C4"));
}

// =============================================================================
// C2: Score validation failed
// =============================================================================

TEST_CASE("CIVD001A: C2 errors when embedded Score fails structural validation", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};

    // Build a valid Score, then corrupt it to trigger structural validation failure
    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};  // C4
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno";
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    REQUIRE(score_result.has_value());

    Score score = std::move(*score_result);
    // Corrupt: remove all parts to violate S1 (parts non-empty)
    score.parts.clear();
    work.score = score;

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C2"));
    for (const auto& d : diags)
        if (d.rule == "C2") CHECK(d.severity == ValidationSeverity::Error);
}

TEST_CASE("CIVD001A: C2 passes for valid embedded Score", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};

    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = 4;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno";
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    REQUIRE(score_result.has_value());
    work.score = std::move(*score_result);

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C2"));
}

TEST_CASE("CIVD001A: C2 not checked when no Score present", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    // No score field set

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C2"));
}

// =============================================================================
// C5: Excessive voices
// =============================================================================

TEST_CASE("CIVD001A: C5 warns for excessive voices in a measure", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};

    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno";
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    REQUIRE(score_result.has_value());
    Score score = std::move(*score_result);

    // Add 7 voices to the first measure (exceeds 6)
    auto& measure = score.parts[0].measures[0];
    while (measure.voices.size() < 7) {
        Voice v;
        v.voice_index = static_cast<std::uint8_t>(measure.voices.size());
        RestEvent rest;
        rest.duration = Beat{1, 1};
        Event ev;
        ev.id = EventId{static_cast<std::uint64_t>(100 + measure.voices.size())};
        ev.offset = Beat::zero();
        ev.payload = rest;
        v.events.push_back(ev);
        measure.voices.push_back(v);
    }
    work.score = score;

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C5"));
}

TEST_CASE("CIVD001A: C5 does not warn for normal voice count", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};

    ScoreSpec spec;
    spec.title = "Test";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno";
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    auto score_result = create_score(spec);
    REQUIRE(score_result.has_value());
    work.score = std::move(*score_result);

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C5"));
}

// =============================================================================
// C6: Harmonic analysis coverage
// =============================================================================

TEST_CASE("CIVD001A: C6 errors for low harmonic coverage", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 10}};
    work.analysis.formal_analysis.total_duration_bars = 100;

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C6"));
    // C6 is an Error
    for (const auto& d : diags)
        if (d.rule == "C6") CHECK(d.severity == ValidationSeverity::Error);
}

TEST_CASE("CIVD001A: C6 passes for adequate harmonic coverage", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 100}};
    work.analysis.formal_analysis.total_duration_bars = 100;

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C6"));
}

// =============================================================================
// C7: Over-segmentation
// =============================================================================

TEST_CASE("CIVD001A: C7 warns for short sections", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 100}};
    work.analysis.formal_analysis.total_duration_bars = 100;

    FormalSection short_sec;
    short_sec.label = "Tiny";
    short_sec.start_bar = 1;
    short_sec.end_bar = 3;
    short_sec.length_bars = 3;
    work.analysis.formal_analysis.section_plan.push_back(short_sec);

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C7"));
}

// =============================================================================
// C8: No thematic units
// =============================================================================

TEST_CASE("CIVD001A: C8 warns when no thematic units found", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 100}};
    work.analysis.formal_analysis.total_duration_bars = 50;

    auto diags = validate_ingested_work(work);
    CHECK(has_rule(diags, "C8"));
}

TEST_CASE("CIVD001A: C8 does not warn when themes present", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 100}};
    work.analysis.formal_analysis.total_duration_bars = 50;

    ThematicUnit tu;
    tu.id = ThematicUnitId{1};
    tu.label = "Main Theme";
    work.analysis.motivic_analysis.thematic_units.push_back(tu);

    auto diags = validate_ingested_work(work);
    CHECK_FALSE(has_rule(diags, "C8"));
}

// =============================================================================
// C10: Small corpus
// =============================================================================

TEST_CASE("CIVD001A: C10 warns for fewer than 5 works", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    profile.works = {IngestedWorkId{1}, IngestedWorkId{2}};

    auto diags = validate_composer_profile(profile);
    CHECK(has_rule(diags, "C10"));
}

// =============================================================================
// C11: Moderate corpus
// =============================================================================

TEST_CASE("CIVD001A: C11 reports moderate corpus (5-9 works)", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    for (std::uint64_t i = 1; i <= 7; ++i)
        profile.works.push_back(IngestedWorkId{i});

    auto diags = validate_composer_profile(profile);
    CHECK_FALSE(has_rule(diags, "C10"));
    CHECK(has_rule(diags, "C11"));
}

TEST_CASE("CIVD001A: No C10/C11 for 10+ works", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    for (std::uint64_t i = 1; i <= 15; ++i)
        profile.works.push_back(IngestedWorkId{i});

    auto diags = validate_composer_profile(profile);
    CHECK_FALSE(has_rule(diags, "C10"));
    CHECK_FALSE(has_rule(diags, "C11"));
}

// =============================================================================
// C12: Small period corpus
// =============================================================================

TEST_CASE("CIVD001A: C12 warns for period with fewer than 3 works", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    for (std::uint64_t i = 1; i <= 10; ++i)
        profile.works.push_back(IngestedWorkId{i});

    PeriodProfile pp;
    pp.label = "Early";
    pp.works = {IngestedWorkId{1}};
    profile.period_profiles.push_back(pp);

    auto diags = validate_composer_profile(profile);
    CHECK(has_rule(diags, "C12"));
}

// =============================================================================
// C13: Weak signature pattern
// =============================================================================

TEST_CASE("CIVD001A: C13 reports weak signature pattern", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    for (std::uint64_t i = 1; i <= 10; ++i)
        profile.works.push_back(IngestedWorkId{i});

    SignaturePattern pat;
    pat.id = SignaturePatternId{1};
    pat.description = "Weak pattern";
    pat.distinctiveness = 1.2f;
    profile.style_profile.signature_patterns.push_back(pat);

    auto diags = validate_composer_profile(profile);
    CHECK(has_rule(diags, "C13"));
}

TEST_CASE("CIVD001A: C13 does not report strong signature", "[corpus-ir][validation]") {
    ComposerProfile profile;
    profile.id = ComposerProfileId{1};
    profile.name = "Test";
    for (std::uint64_t i = 1; i <= 10; ++i)
        profile.works.push_back(IngestedWorkId{i});

    SignaturePattern pat;
    pat.id = SignaturePatternId{1};
    pat.description = "Strong pattern";
    pat.distinctiveness = 3.0f;
    profile.style_profile.signature_patterns.push_back(pat);

    auto diags = validate_composer_profile(profile);
    CHECK_FALSE(has_rule(diags, "C13"));
}

// =============================================================================
// Corpus-level validation
// =============================================================================

TEST_CASE("CIVD001A: is_corpus_valid for empty corpus", "[corpus-ir][validation]") {
    CorpusDatabase db;
    CHECK(is_corpus_valid(db));
}

TEST_CASE("CIVD001A: is_corpus_valid returns false with errors", "[corpus-ir][validation]") {
    CorpusDatabase db;
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 5}};
    work.analysis.formal_analysis.total_duration_bars = 100;  // coverage < 80%
    db.works[1] = work;

    CHECK_FALSE(is_corpus_valid(db));
}

// =============================================================================
// Diagnostic sort order
// =============================================================================

TEST_CASE("CIVD001A: diagnostics sorted by severity", "[corpus-ir][validation]") {
    IngestedWork work;
    work.id = IngestedWorkId{1};
    work.analysis_complete = true;
    work.ingestion_confidence.key_confidence = 0.4f;
    work.ingestion_confidence.source_format = "midi";
    work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 5}};
    work.analysis.formal_analysis.total_duration_bars = 100;

    auto diags = validate_ingested_work(work);
    REQUIRE(diags.size() >= 2);

    for (std::size_t i = 1; i < diags.size(); ++i) {
        CHECK(static_cast<int>(diags[i - 1].severity) <=
              static_cast<int>(diags[i].severity));
    }
}

// =============================================================================
// M6: Empty/boundary input analysis
// =============================================================================

TEST_CASE("CIAN001A: empty and single-pitch scores produce empty harmonic analysis",
          "[corpus-ir][analysis]") {

    // Shared score specification: one part, one bar, 4/4 in C major
    ScoreSpec spec;
    spec.title = "Boundary Test";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};  // C4
    spec.time_sig_num = 4;
    spec.time_sig_den = 4;
    PartDefinition pd;
    pd.name = "Piano";
    pd.abbreviation = "Pno";
    pd.instrument_type = InstrumentType::Piano;
    pd.clef = Clef::Treble;
    spec.parts.push_back(pd);

    SECTION("rest-only score yields empty chord vocabulary") {
        auto score_result = create_score(spec);
        REQUIRE(score_result.has_value());

        // create_score fills every measure with whole-measure rests
        auto harmonic = analyze_harmonic(*score_result);
        CHECK(harmonic.chord_vocabulary.empty());
    }

    SECTION("single pitch per beat is excluded from chord recognition") {
        auto score_result = create_score(spec);
        REQUIRE(score_result.has_value());
        Score score = std::move(*score_result);

        // Insert a single C4 quarter note at beat 0
        Note note;
        note.pitch = SpelledPitch{0, 0, 4};  // C4
        auto insert = wf_insert_note(
            score, score.parts[0].id, 1, 0,
            Beat::zero(), note, Beat{1, 4});
        REQUIRE(insert.has_value());

        // recognize_chord requires >= 2 pitch classes, so a single
        // pitch produces no chord vocabulary entries
        auto harmonic = analyze_harmonic(score);
        CHECK(harmonic.chord_vocabulary.empty());
    }
}

// =============================================================================
// Boundary-value tests targeting mutation: < to <= and > to >=
// =============================================================================

TEST_CASE("CIVD001A: boundary values at validation thresholds",
          "[corpus-ir][validation]") {

    SECTION("C1 at exactly 0.7 confidence") {
        IngestedWork work;
        work.id = IngestedWorkId{1};
        work.ingestion_confidence.key_confidence = 0.7f;
        work.ingestion_confidence.metre_confidence = 0.7f;
        work.ingestion_confidence.spelling_confidence = 0.7f;
        work.ingestion_confidence.voice_separation_confidence = 0.7f;
        work.ingestion_confidence.source_format = "musicxml";

        auto diags = validate_ingested_work(work);
        CHECK_FALSE(has_rule(diags, "C1"));

        // Sanity: 0.699f must trigger C1
        work.ingestion_confidence.key_confidence = 0.699f;
        diags = validate_ingested_work(work);
        CHECK(has_rule(diags, "C1"));
    }

    SECTION("C3 at exactly 0.5 key confidence") {
        IngestedWork work;
        work.id = IngestedWorkId{1};
        work.ingestion_confidence.key_confidence = 0.5f;
        work.ingestion_confidence.source_format = "midi";

        auto diags = validate_ingested_work(work);
        CHECK_FALSE(has_rule(diags, "C3"));
    }

    SECTION("C4 at exactly 1.0 metre confidence") {
        IngestedWork work;
        work.id = IngestedWorkId{1};
        work.ingestion_confidence.metre_confidence = 1.0f;
        work.ingestion_confidence.source_format = "midi";

        auto diags = validate_ingested_work(work);
        CHECK_FALSE(has_rule(diags, "C4"));
    }

    SECTION("C5 at exactly 6 voices") {
        IngestedWork work;
        work.id = IngestedWorkId{1};

        ScoreSpec spec;
        spec.title = "Boundary C5";
        spec.total_bars = 1;
        spec.bpm = 120.0;
        spec.key_root = SpelledPitch{0, 0, 4};
        spec.time_sig_num = 4;
        spec.time_sig_den = 4;
        PartDefinition pd;
        pd.name = "Piano";
        pd.abbreviation = "Pno";
        pd.instrument_type = InstrumentType::Piano;
        pd.clef = Clef::Treble;
        spec.parts.push_back(pd);

        auto score_result = create_score(spec);
        REQUIRE(score_result.has_value());
        Score score = std::move(*score_result);

        // Pad to exactly 6 voices (create_score gives 1 voice)
        auto& measure = score.parts[0].measures[0];
        while (measure.voices.size() < 6) {
            Voice v;
            v.voice_index = static_cast<std::uint8_t>(measure.voices.size());
            RestEvent rest;
            rest.duration = Beat{1, 1};
            Event ev;
            ev.id = EventId{static_cast<std::uint64_t>(200 + measure.voices.size())};
            ev.offset = Beat::zero();
            ev.payload = rest;
            v.events.push_back(ev);
            measure.voices.push_back(v);
        }
        REQUIRE(measure.voices.size() == 6);
        work.score = score;

        auto diags = validate_ingested_work(work);
        CHECK_FALSE(has_rule(diags, "C5"));
    }

    SECTION("C7 at exactly 4 bar section") {
        IngestedWork work;
        work.id = IngestedWorkId{1};
        work.analysis_complete = true;
        work.analysis.harmonic_analysis.chord_vocabulary = {{"I", 100}};
        work.analysis.formal_analysis.total_duration_bars = 100;

        FormalSection sec;
        sec.label = "Boundary";
        sec.start_bar = 1;
        sec.end_bar = 4;
        sec.length_bars = 4;
        work.analysis.formal_analysis.section_plan.push_back(sec);

        auto diags = validate_ingested_work(work);
        CHECK_FALSE(has_rule(diags, "C7"));
    }

    SECTION("C10 at exactly 5 works") {
        ComposerProfile profile;
        profile.id = ComposerProfileId{1};
        profile.name = "Test";
        for (std::uint64_t i = 1; i <= 5; ++i)
            profile.works.push_back(IngestedWorkId{i});

        auto diags = validate_composer_profile(profile);
        CHECK_FALSE(has_rule(diags, "C10"));
    }

    SECTION("C13 at exactly 1.5 distinctiveness") {
        ComposerProfile profile;
        profile.id = ComposerProfileId{1};
        profile.name = "Test";
        for (std::uint64_t i = 1; i <= 10; ++i)
            profile.works.push_back(IngestedWorkId{i});

        SignaturePattern pat;
        pat.id = SignaturePatternId{1};
        pat.description = "Boundary pattern";
        pat.distinctiveness = 1.5f;
        profile.style_profile.signature_patterns.push_back(pat);

        auto diags = validate_composer_profile(profile);
        CHECK_FALSE(has_rule(diags, "C13"));
    }
}
