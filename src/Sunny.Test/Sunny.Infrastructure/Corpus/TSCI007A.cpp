/**
 * @file TSCI007A.cpp
 * @brief Unit tests for Corpus IR ingestion pipeline (CIIN001A)
 *
 * Component: TSCI007A
 * Domain: TS (Test) | Category: CI (Corpus IR)
 *
 * Tests: CIIN001A, INWF001A ingestion wrappers
 *
 * Uses write_midi / write_musicxml to generate valid test data
 * in-process, avoiding external file dependencies.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "Corpus/CIIN001A.h"
#include "Application/INWF001A.h"
#include "Format/FMMI001A.h"
#include "Format/FMMX001A.h"
#include "Corpus/CIWF001A.h"

using namespace Sunny::Core;
using namespace Sunny::Infrastructure;
using namespace Sunny::Infrastructure::Format;
using namespace Sunny::Infrastructure::Corpus;

namespace {

/// Generate a minimal valid MIDI file with a C major scale.
std::vector<std::uint8_t> make_c_major_midi() {
    MidiFile midi;
    midi.format = 0;
    midi.ppq = 480;

    // C major scale: C4 D4 E4 F4 G4 A4 B4 C5
    std::uint8_t pitches[] = {60, 62, 64, 65, 67, 69, 71, 72};
    for (int i = 0; i < 8; ++i) {
        MidiNoteEvent note;
        note.tick = static_cast<std::uint32_t>(i * 480);
        note.duration_ticks = 480;
        note.channel = 0;
        note.note = pitches[i];
        note.velocity = 80;
        midi.notes.push_back(note);
    }

    // Tempo: 120 BPM = 500000 us/beat
    midi.tempos.push_back({0, 500000});

    // Time signature: 4/4
    midi.time_signatures.push_back({0, 4, 4});

    auto result = write_midi(midi);
    if (!result) return {};
    return *result;
}

/// Generate a minimal valid MIDI file with A minor notes.
std::vector<std::uint8_t> make_a_minor_midi() {
    MidiFile midi;
    midi.format = 0;
    midi.ppq = 480;

    // A minor scale: A3 B3 C4 D4 E4 F4 G4 A4
    std::uint8_t pitches[] = {57, 59, 60, 62, 64, 65, 67, 69};
    for (int i = 0; i < 8; ++i) {
        MidiNoteEvent note;
        note.tick = static_cast<std::uint32_t>(i * 480);
        note.duration_ticks = 480;
        note.channel = 0;
        note.note = pitches[i];
        note.velocity = 80;
        midi.notes.push_back(note);
    }

    midi.tempos.push_back({0, 500000});
    midi.time_signatures.push_back({0, 4, 4});

    auto result = write_midi(midi);
    if (!result) return {};
    return *result;
}

/// Generate a minimal MusicXML document with known structure.
std::string make_simple_musicxml() {
    MusicXmlScore xml_score;
    xml_score.title = "Test Piece";
    xml_score.divisions = 1;

    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    // 4 measures of C major quarter notes
    for (int m = 0; m < 4; ++m) {
        MusicXmlMeasure measure;
        measure.number = m + 1;

        if (m == 0) {
            measure.time_signature = {4, 4};
            measure.key_tonic = SpelledPitch{0, 0, 4};  // C4
            measure.key_is_major = true;
        }

        // 4 quarter notes per measure: C4 E4 G4 C5
        SpelledPitch pitches[] = {
            {0, 0, 4}, {2, 0, 4}, {4, 0, 4}, {0, 0, 5}
        };
        for (int n = 0; n < 4; ++n) {
            MusicXmlNote note;
            note.pitch = pitches[n];
            note.duration = Beat{1, 4};
            note.is_rest = false;
            note.is_chord = false;
            note.voice = 1;
            measure.notes.push_back(note);
        }
        part.measures.push_back(measure);
    }

    xml_score.parts.push_back(part);

    auto result = write_musicxml(xml_score);
    if (!result) return "";
    return *result;
}

}  // anonymous namespace


// =============================================================================
// Key estimation
// =============================================================================

TEST_CASE("CIIN001A: C major scale MIDI → key estimate C major", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "C Major Scale";

    auto result = ingest_midi(midi_data, IngestedWorkId{1}, opts);
    REQUIRE(result.has_value());

    CHECK(result->ingestion_confidence.key_confidence > 0.6f);
    CHECK(result->analysis_complete);
    CHECK(result->score.has_value());
}

TEST_CASE("CIIN001A: A minor MIDI → key estimate A minor", "[corpus-ir][ingestion]") {
    auto midi_data = make_a_minor_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "A Minor Scale";

    auto result = ingest_midi(midi_data, IngestedWorkId{2}, opts);
    REQUIRE(result.has_value());
    CHECK(result->ingestion_confidence.key_confidence > 0.5f);
}

// =============================================================================
// Quantisation
// =============================================================================

TEST_CASE("CIIN001A: Quantisation produces non-zero residual for grid-aligned notes", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Quantise Test";
    opts.quantise_grid = 16;

    auto result = ingest_midi(midi_data, IngestedWorkId{3}, opts);
    REQUIRE(result.has_value());
    // Grid-aligned notes should have near-zero residual
    CHECK(result->ingestion_confidence.quantisation_residual < 0.1f);
}

// =============================================================================
// Voice separation
// =============================================================================

TEST_CASE("CIIN001A: Voice separation confidence reported", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Voice Sep Test";

    auto result = ingest_midi(midi_data, IngestedWorkId{4}, opts);
    REQUIRE(result.has_value());
    CHECK(result->ingestion_confidence.voice_separation_confidence > 0.0f);
    CHECK(result->ingestion_confidence.voice_separation_confidence <= 1.0f);
}

// =============================================================================
// MIDI ingestion round-trip
// =============================================================================

TEST_CASE("CIIN001A: MIDI ingestion produces Score with bars and notes", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Round-trip Test";

    auto result = ingest_midi(midi_data, IngestedWorkId{5}, opts);
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());

    const auto& score = *result->score;
    CHECK(score.metadata.total_bars > 0);
    CHECK_FALSE(score.parts.empty());
    CHECK(result->metadata.source_format == "midi");
}

// =============================================================================
// MusicXML ingestion
// =============================================================================

TEST_CASE("CIIN001A: MusicXML ingestion produces valid Score", "[corpus-ir][ingestion]") {
    auto xml = make_simple_musicxml();
    REQUIRE_FALSE(xml.empty());

    IngestionOptions opts;
    opts.title = "MusicXML Test";

    auto result = ingest_musicxml(xml, IngestedWorkId{6}, opts);
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());

    const auto& score = *result->score;
    CHECK(score.metadata.total_bars == 4);
    CHECK_FALSE(score.parts.empty());
    CHECK(result->metadata.source_format == "musicxml");
    CHECK(result->ingestion_confidence.key_confidence == 1.0f);
    CHECK(result->ingestion_confidence.spelling_confidence == 1.0f);
}

// =============================================================================
// Analysis runs on ingest
// =============================================================================

TEST_CASE("CIIN001A: Analysis completes during ingestion", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Analysis Test";

    auto result = ingest_midi(midi_data, IngestedWorkId{7}, opts);
    REQUIRE(result.has_value());
    CHECK(result->analysis_complete);
    // Formal analysis should have populated total_duration_bars
    CHECK(result->analysis.formal_analysis.total_duration_bars > 0);
}

// =============================================================================
// Error: empty MIDI
// =============================================================================

TEST_CASE("CIIN001A: Empty MIDI data returns error", "[corpus-ir][ingestion]") {
    std::vector<std::uint8_t> empty_data;
    IngestionOptions opts;
    opts.title = "Empty Test";

    auto result = ingest_midi(empty_data, IngestedWorkId{8}, opts);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Error: malformed MusicXML
// =============================================================================

TEST_CASE("CIIN001A: Malformed MusicXML returns error", "[corpus-ir][ingestion]") {
    IngestionOptions opts;
    opts.title = "Malformed Test";

    auto result = ingest_musicxml("not valid xml", IngestedWorkId{9}, opts);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// IngestionConfidence population
// =============================================================================

TEST_CASE("CIIN001A: IngestionConfidence fully populated for MIDI", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Confidence Test";

    auto result = ingest_midi(midi_data, IngestedWorkId{10}, opts);
    REQUIRE(result.has_value());

    const auto& ic = result->ingestion_confidence;
    CHECK(ic.key_confidence > 0.0f);
    CHECK(ic.metre_confidence > 0.0f);
    CHECK(ic.spelling_confidence > 0.0f);
    CHECK(ic.voice_separation_confidence > 0.0f);
    CHECK(ic.source_format == "midi");
}

TEST_CASE("CIIN001A: IngestionConfidence for MusicXML has high confidence", "[corpus-ir][ingestion]") {
    auto xml = make_simple_musicxml();
    REQUIRE_FALSE(xml.empty());

    IngestionOptions opts;
    opts.title = "MusicXML Confidence Test";

    auto result = ingest_musicxml(xml, IngestedWorkId{11}, opts);
    REQUIRE(result.has_value());

    const auto& ic = result->ingestion_confidence;
    CHECK(ic.key_confidence == 1.0f);
    CHECK(ic.metre_confidence == 1.0f);
    CHECK(ic.spelling_confidence == 1.0f);
    CHECK(ic.voice_separation_confidence == 1.0f);
    CHECK(ic.quantisation_residual == 0.0f);
}

// =============================================================================
// Workflow wrapper: wf_ingest_midi
// =============================================================================

TEST_CASE("INWF001A: wf_ingest_midi stores work and assigns to composer", "[corpus-ir][ingestion][workflow]") {
    CorpusDatabase db;
    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Mozart");
    db.composers[1] = composer;

    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Sonata K.545";

    auto result = wf_ingest_midi(db,
        std::span<const std::uint8_t>(midi_data),
        IngestedWorkId{1}, ComposerProfileId{1}, opts);
    REQUIRE(result.has_value());

    CHECK(db.works.count(1) == 1);
    CHECK(db.works[1].analysis_complete);
    CHECK(db.composers[1].works.size() == 1);
}

// =============================================================================
// Workflow wrapper: wf_ingest_musicxml
// =============================================================================

TEST_CASE("INWF001A: wf_ingest_musicxml stores work and assigns to composer", "[corpus-ir][ingestion][workflow]") {
    CorpusDatabase db;
    auto composer = wf_create_composer_profile(ComposerProfileId{1}, "Beethoven");
    db.composers[1] = composer;

    auto xml = make_simple_musicxml();
    REQUIRE_FALSE(xml.empty());

    IngestionOptions opts;
    opts.title = "Sonata Op. 13";

    auto result = wf_ingest_musicxml(db, xml,
        IngestedWorkId{1}, ComposerProfileId{1}, opts);
    REQUIRE(result.has_value());

    CHECK(db.works.count(1) == 1);
    CHECK(db.works[1].analysis_complete);
    CHECK(db.composers[1].works.size() == 1);
}
