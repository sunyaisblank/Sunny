/**
 * @file ingestion_test.cpp
 * @brief Unit tests for Corpus IR ingestion pipeline
 *
 *
 *
 * Uses write_midi / write_musicxml to generate valid test data
 * in-process, avoiding external file dependencies.
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <sunny/core/corpus/workflows.hpp>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/validation.hpp>
#include <sunny/infrastructure/compilation_workflows.hpp>
#include <sunny/infrastructure/corpus/ingestion.hpp>
#include <sunny/infrastructure/formats/midi_file.hpp>
#include <sunny/infrastructure/formats/musicxml.hpp>

using namespace sunny::core;
using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;
using namespace sunny::infrastructure::corpus;

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

    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    // 4 measures of C major quarter notes
    for (int m = 0; m < 4; ++m) {
        MusicXmlMeasure measure;
        measure.number = m + 1;

        if (m == 0) {
            measure.time_signature = {4, 4};
            measure.key_tonic = SpelledPitch{0, 0, 4}; // C4
            measure.key_is_major = true;
        }

        // 4 quarter notes per measure: C4 E4 G4 C5
        SpelledPitch pitches[] = {{0, 0, 4}, {2, 0, 4}, {4, 0, 4}, {0, 0, 5}};
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

const ManualCorrection* find_correction(const IngestedWork& work, std::string_view field) {
    const auto found =
        std::find_if(work.ingestion_confidence.manual_corrections.begin(),
                     work.ingestion_confidence.manual_corrections.end(),
                     [field](const auto& correction) { return correction.field == field; });
    return found == work.ingestion_confidence.manual_corrections.end() ? nullptr : &*found;
}

} // anonymous namespace

// =============================================================================
// Key estimation
// =============================================================================

TEST_CASE("C major scale MIDI → key estimate C major", "[corpus-ir][ingestion]") {
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

TEST_CASE("A minor MIDI → key estimate A minor", "[corpus-ir][ingestion]") {
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

TEST_CASE("Quantisation produces near-zero residual for grid-aligned notes",
          "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Quantise Test";
    opts.quantise_grid = 16;

    auto result = ingest_midi(midi_data, IngestedWorkId{3}, opts);
    REQUIRE(result.has_value());
    // Grid-aligned notes should have near-zero residual
    CHECK(result->ingestion_confidence.quantisation_residual < 0.1f);
    CHECK(result->ingestion_confidence.duration_quantisation_residual == 0.0f);
}

TEST_CASE("MIDI quantisation reports duration loss separately from onset loss",
          "[corpus-ir][ingestion][midi][quantisation][loss-accounting]") {
    SECTION("off-grid duration does not fabricate onset error") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 179, 0, 60, 80});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        IngestionOptions options;
        options.quantise_grid = 16;
        const auto ingested = ingest_midi(*bytes, IngestedWorkId{224}, options);
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->ingestion_confidence.quantisation_residual == 0.0f);
        CHECK(ingested->ingestion_confidence.duration_quantisation_residual ==
              Catch::Approx(59.0f / 1920.0f));

        const auto compiled = compile_to_midi(*ingested->score, 480);
        REQUIRE(compiled.has_value());
        REQUIRE(compiled->midi.notes.size() == 1);
        CHECK(compiled->midi.notes[0].tick == 0);
        CHECK(compiled->midi.notes[0].duration_ticks == 120);
    }

    SECTION("exact half-grid onset and duration round forward without floating ambiguity") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({60, 60, 0, 60, 80});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        IngestionOptions options;
        options.quantise_grid = 16;
        const auto ingested = ingest_midi(*bytes, IngestedWorkId{225}, options);
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->ingestion_confidence.quantisation_residual == Catch::Approx(1.0f / 32.0f));
        CHECK(ingested->ingestion_confidence.duration_quantisation_residual ==
              Catch::Approx(1.0f / 32.0f));

        const auto compiled = compile_to_midi(*ingested->score, 480);
        REQUIRE(compiled.has_value());
        REQUIRE(compiled->midi.notes.size() == 1);
        CHECK(compiled->midi.notes[0].tick == 120);
        CHECK(compiled->midi.notes[0].duration_ticks == 120);
    }
}

// =============================================================================
// Voice separation
// =============================================================================

TEST_CASE("Voice separation confidence reported", "[corpus-ir][ingestion]") {
    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Voice Sep Test";

    auto result = ingest_midi(midi_data, IngestedWorkId{4}, opts);
    REQUIRE(result.has_value());
    CHECK(result->ingestion_confidence.voice_separation_confidence > 0.0f);
    CHECK(result->ingestion_confidence.voice_separation_confidence <= 1.0f);
}

TEST_CASE("MIDI ingestion creates every required polyphonic voice without dropping tones",
          "[corpus-ir][ingestion][polyphony][trust-boundary]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.time_signatures.push_back({0, 4, 4});
    midi.notes.push_back({0, 480, 0, 60, 80});
    midi.notes.push_back({0, 960, 0, 64, 81});
    midi.notes.push_back({0, 480, 0, 67, 82});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{205}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& measure = ingested->score->parts[0].measures[0];
    REQUIRE(measure.voices.size() == 3);

    std::size_t pitch_count = 0;
    for (const auto& voice : measure.voices)
        for (const auto& event : voice.events)
            if (const auto* group = event.as_note_group()) pitch_count += group->notes.size();
    CHECK(pitch_count == 3);
    CHECK(is_compilable(*ingested->score));
}

TEST_CASE("MIDI voice confidence includes staggered sustained overlap",
          "[corpus-ir][ingestion][polyphony][confidence]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.time_signatures.push_back({0, 4, 4});
    midi.notes.push_back({0, 960, 0, 60, 80});
    midi.notes.push_back({480, 480, 0, 67, 81});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{2051}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    CHECK(ingested->ingestion_confidence.voice_separation_confidence == Catch::Approx(0.5F));
    const auto correction = std::find_if(
        ingested->ingestion_confidence.manual_corrections.begin(),
        ingested->ingestion_confidence.manual_corrections.end(),
        [](const auto& candidate) { return candidate.field == "midi.voice_separation"; });
    REQUIRE(correction != ingested->ingestion_confidence.manual_corrections.end());
    CHECK(correction->description.find("1 of 2") != std::string::npos);
}

TEST_CASE("equal-pitch voice allocation is stable in retained SMF order",
          "[corpus-ir][ingestion][polyphony][determinism]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.time_signatures.push_back({0, 4, 4});
    midi.notes.push_back({0, 480, 0, 60, 80, false, 0, 23});
    midi.notes.push_back({0, 480, 0, 60, 81, false, 0, 91});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{2052}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& voices = ingested->score->parts[0].measures[0].voices;
    REQUIRE(voices.size() == 2);
    REQUIRE(voices[0].events[0].as_note_group());
    REQUIRE(voices[1].events[0].as_note_group());
    CHECK(voices[0].events[0].as_note_group()->notes[0].release_velocity == 23);
    CHECK(voices[1].events[0].as_note_group()->notes[0].release_velocity == 91);
}

TEST_CASE("MIDI ingestion splits a cross-bar note into an exact valid tie chain",
          "[corpus-ir][ingestion][tie][trust-boundary]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.time_signatures.push_back({0, 4, 4});
    midi.notes.push_back({1440, 960, 0, 60, 80, false, 0, 73});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{206}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    REQUIRE(ingested->score->parts[0].measures.size() == 2);

    const auto& first_voice = ingested->score->parts[0].measures[0].voices[0];
    const auto& second_voice = ingested->score->parts[0].measures[1].voices[0];
    const auto first_note = std::find_if(first_voice.events.begin(),
                                         first_voice.events.end(),
                                         [](const Event& event) { return event.is_note_group(); });
    const auto second_note = std::find_if(second_voice.events.begin(),
                                          second_voice.events.end(),
                                          [](const Event& event) { return event.is_note_group(); });
    REQUIRE(first_note != first_voice.events.end());
    REQUIRE(second_note != second_voice.events.end());
    CHECK(first_note->as_note_group()->notes[0].tie_forward);
    CHECK_FALSE(second_note->as_note_group()->notes[0].tie_forward);
    CHECK(first_note->as_note_group()->notes[0].release_velocity == 73);
    CHECK(second_note->as_note_group()->notes[0].release_velocity == 73);
    CHECK(is_compilable(*ingested->score));

    const auto compiled = compile_to_midi(*ingested->score, 480);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->midi.notes.size() == 1);
    CHECK(compiled->midi.notes[0].tick == 1440);
    CHECK(compiled->midi.notes[0].duration_ticks == 960);
    CHECK(compiled->midi.notes[0].release_velocity == 73);
}

TEST_CASE("MIDI ingestion preserves ordered tempo metre and key metadata maps",
          "[corpus-ir][ingestion][midi][maps][trust-boundary]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 480, 0, 60, 80});
    midi.notes.push_back({1920, 480, 0, 63, 80});
    midi.notes.push_back({3360, 480, 0, 67, 80});
    midi.tempos.push_back({0, 500000});
    midi.tempos.push_back({1920, 600000});
    midi.time_signatures.push_back({0, 4, 4});
    midi.time_signatures.push_back({1920, 3, 4});
    midi.key_signatures.push_back({0, 0, false});
    midi.key_signatures.push_back({1920, -3, true});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{209}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& score = *ingested->score;
    REQUIRE(score.time_map.size() == 2);
    CHECK(score.time_map[0].bar == 1);
    CHECK(score.time_map[0].time_signature.numerator() == 4);
    CHECK(score.time_map[1].bar == 2);
    CHECK(score.time_map[1].time_signature.numerator() == 3);
    REQUIRE(score.tempo_map.size() == 2);
    CHECK(score.tempo_map[0].position == ScoreTime{1, Beat::zero()});
    CHECK(score.tempo_map[0].bpm == PositiveRational{120, 1});
    CHECK(score.tempo_map[1].position == ScoreTime{2, Beat::zero()});
    CHECK(score.tempo_map[1].bpm == PositiveRational{100, 1});
    REQUIRE(score.key_map.size() == 2);
    CHECK(score.key_map[0].key.root == SpelledPitch{0, 0, 4});
    CHECK(score.key_map[0].key.mode.name == "major");
    CHECK(score.key_map[1].position == ScoreTime{2, Beat::zero()});
    CHECK(score.key_map[1].key.root == SpelledPitch{0, 0, 4});
    CHECK(score.key_map[1].key.mode.name == "minor");
    CHECK(score.key_map[1].key.accidentals == -3);
    CHECK(is_compilable(score));
}

TEST_CASE("MIDI ingestion rejects a time-signature change outside Score's bar-boundary algebra",
          "[corpus-ir][ingestion][midi][maps][trust-boundary]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 960, 0, 60, 80});
    midi.time_signatures.push_back({0, 4, 4});
    midi.time_signatures.push_back({480, 3, 4});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{210}, IngestionOptions{});
    REQUIRE_FALSE(ingested.has_value());
    CHECK(ingested.error() == ErrorCode::IngestionFailed);
}

TEST_CASE("MIDI ingestion keeps metronome clicks distinct from Score grouping",
          "[corpus-ir][ingestion][midi][meter][loss-accounting]") {
    SECTION("canonical compound click agrees without inventing extra intent") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 1440, 0, 60, 80});
        midi.time_signatures.push_back(MidiTimeSignatureEvent{.tick = 0,
                                                              .numerator = 6,
                                                              .denominator = 8,
                                                              .clocks_per_metronome_click = 36,
                                                              .notated_32nds_per_quarter = 8});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{212}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->score->time_map.front().time_signature.groups() == std::vector<int>{3, 3});
        CHECK(std::none_of(ingested->ingestion_confidence.manual_corrections.begin(),
                           ingested->ingestion_confidence.manual_corrections.end(),
                           [](const auto& correction) {
                               return correction.field == "midi.time_signature_metronome_clicks";
                           }));
    }

    SECTION("a noncanonical click is explicit loss, not a replacement group partition") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 480, 0, 60, 80});
        midi.time_signatures.push_back(MidiTimeSignatureEvent{.tick = 0,
                                                              .numerator = 4,
                                                              .denominator = 4,
                                                              .clocks_per_metronome_click = 48,
                                                              .notated_32nds_per_quarter = 8});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{213}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->score->time_map.front().time_signature.groups() ==
              std::vector<int>{1, 1, 1, 1});
        const auto correction = std::find_if(
            ingested->ingestion_confidence.manual_corrections.begin(),
            ingested->ingestion_confidence.manual_corrections.end(),
            [](const auto& item) { return item.field == "midi.time_signature_metronome_clicks"; });
        REQUIRE(correction != ingested->ingestion_confidence.manual_corrections.end());
        CHECK(correction->description.find("1 Time Signature") != std::string::npos);
    }
}

TEST_CASE("MIDI ingestion rejects unrepresentable or contradictory complete metre payloads",
          "[corpus-ir][ingestion][midi][meter][trust-boundary]") {
    SECTION("nonstandard notated-32nd scaling") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 480, 0, 60, 80});
        midi.time_signatures.push_back(MidiTimeSignatureEvent{.tick = 0,
                                                              .numerator = 4,
                                                              .denominator = 4,
                                                              .clocks_per_metronome_click = 24,
                                                              .notated_32nds_per_quarter = 7});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{214}, IngestionOptions{});
        REQUIRE_FALSE(ingested.has_value());
        CHECK(ingested.error() == ErrorCode::IngestionFailed);
    }

    SECTION("same-tick signatures disagree in metronome semantics") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 480, 0, 60, 80});
        midi.time_signatures.push_back(MidiTimeSignatureEvent{.tick = 0,
                                                              .numerator = 4,
                                                              .denominator = 4,
                                                              .clocks_per_metronome_click = 24,
                                                              .notated_32nds_per_quarter = 8});
        midi.time_signatures.push_back(MidiTimeSignatureEvent{.tick = 0,
                                                              .numerator = 4,
                                                              .denominator = 4,
                                                              .clocks_per_metronome_click = 48,
                                                              .notated_32nds_per_quarter = 8});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{215}, IngestionOptions{});
        REQUIRE_FALSE(ingested.has_value());
        CHECK(ingested.error() == ErrorCode::IngestionFailed);
    }
}

TEST_CASE("MIDI minor key metadata derives the correct tonic spelling",
          "[corpus-ir][ingestion][midi][key][trust-boundary]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 480, 0, 69, 80});
    midi.key_signatures.push_back({0, 0, true});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{211}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& key = ingested->score->key_map.front().key;
    CHECK(key.root == SpelledPitch{5, 0, 4}); // A minor, not E-flat major-family inversion.
    CHECK(key.mode.name == "minor");
    CHECK(key.accidentals == 0);
}

// =============================================================================
// MIDI ingestion round-trip
// =============================================================================

TEST_CASE("MIDI ingestion produces Score with bars and notes", "[corpus-ir][ingestion]") {
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

TEST_CASE("MIDI ingestion exposes valid unmodelled event loss as a manual correction",
          "[corpus-ir][ingestion][loss-accounting]") {
    auto midi = make_c_major_midi();
    REQUIRE(midi.size() >= 26);

    const std::uint32_t track_length = (static_cast<std::uint32_t>(midi[18]) << 24) |
                                       (static_cast<std::uint32_t>(midi[19]) << 16) |
                                       (static_cast<std::uint32_t>(midi[20]) << 8) |
                                       static_cast<std::uint32_t>(midi[21]);
    const auto eot = midi.end() - 4;
    midi.insert(eot, {0x00, 0xE0, 0x00, 0x40});
    const std::uint32_t expanded = track_length + 4;
    midi[18] = static_cast<std::uint8_t>(expanded >> 24);
    midi[19] = static_cast<std::uint8_t>(expanded >> 16);
    midi[20] = static_cast<std::uint8_t>(expanded >> 8);
    midi[21] = static_cast<std::uint8_t>(expanded);

    const auto result = ingest_midi(midi, IngestedWorkId{77}, {});
    REQUIRE(result.has_value());
    REQUIRE(result->ingestion_confidence.manual_corrections.size() == 1);
    CHECK(result->ingestion_confidence.manual_corrections[0].field == "midi.pitch_bend_events");
    CHECK(result->ingestion_confidence.manual_corrections[0].description.find("1 valid") !=
          std::string::npos);
}

TEST_CASE("MIDI ingestion preserves canonical single-channel pedal switch spans",
          "[corpus-ir][ingestion][midi][controller][round-trip]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 1920, 2, 60, 80});
    midi.time_signatures.push_back({0, 4, 4});
    midi.control_changes.push_back({480, 2, 64, 127});
    midi.control_changes.push_back({1440, 2, 64, 0});
    midi.control_changes.push_back({960, 2, 67, 127});
    midi.control_changes.push_back({1920, 2, 67, 0});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{216}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& score = *ingested->score;
    REQUIRE(score.parts.size() == 1);
    CHECK(score.parts[0].definition.rendering.midi_channel == 3);
    REQUIRE(score.parts[0].part_directives.size() == 2);
    CHECK(score.parts[0].part_directives[0].directive == DirectiveType::SustainingPedal);
    CHECK(score.parts[0].part_directives[0].start == ScoreTime{1, Beat{1, 4}});
    CHECK(score.parts[0].part_directives[0].end == ScoreTime{1, Beat{3, 4}});
    CHECK(score.parts[0].part_directives[1].directive == DirectiveType::UnaCorda);
    CHECK(score.parts[0].part_directives[1].start == ScoreTime{1, Beat{1, 2}});
    CHECK(score.parts[0].part_directives[1].end == ScoreTime{2, Beat::zero()});
    CHECK(find_correction(*ingested, "midi.control_change_events") == nullptr);
    CHECK(find_correction(*ingested, "midi.pedal_switch_values") == nullptr);

    const auto compiled = compile_to_midi(score, 480);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->midi.control_changes.size() == 4);
    CHECK(compiled->midi.control_changes[0].tick == 480);
    CHECK(compiled->midi.control_changes[0].channel == 3);
    CHECK(compiled->midi.control_changes[0].controller == 64);
    CHECK(compiled->midi.control_changes[0].value == 127);
    CHECK(compiled->midi.control_changes[1].tick == 960);
    CHECK(compiled->midi.control_changes[1].controller == 67);
    CHECK(compiled->midi.control_changes[2].tick == 1440);
    CHECK(compiled->midi.control_changes[2].value == 0);
    CHECK(compiled->midi.control_changes[3].tick == 1920);
    CHECK(compiled->midi.control_changes[3].controller == 67);
    const auto reprojected_file = compiled_midi_to_file(compiled->midi);
    REQUIRE(reprojected_file.has_value());
    REQUIRE(reprojected_file->control_changes.size() == 4);
    CHECK(reprojected_file->control_changes[0].channel == 2);
    const auto reprojected_bytes = write_midi(*reprojected_file);
    REQUIRE(reprojected_bytes.has_value());
    const auto reparsed = parse_midi(*reprojected_bytes);
    REQUIRE(reparsed.has_value());
    REQUIRE(reparsed->control_changes.size() == reprojected_file->control_changes.size());
    for (std::size_t index = 0; index < reparsed->control_changes.size(); ++index) {
        CHECK(reparsed->control_changes[index].tick ==
              reprojected_file->control_changes[index].tick);
        CHECK(reparsed->control_changes[index].channel ==
              reprojected_file->control_changes[index].channel);
        CHECK(reparsed->control_changes[index].controller ==
              reprojected_file->control_changes[index].controller);
        CHECK(reparsed->control_changes[index].value ==
              reprojected_file->control_changes[index].value);
    }
    CHECK(is_compilable(score));
}

TEST_CASE("MIDI ingestion separates pedal semantics from byte normalisation",
          "[corpus-ir][ingestion][midi][controller][loss-accounting]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 1920, 0, 60, 80});
    midi.control_changes.push_back({480, 0, 64, 64});
    midi.control_changes.push_back({960, 0, 64, 63});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{217}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    REQUIRE(ingested->score->parts[0].part_directives.size() == 1);
    CHECK(ingested->score->parts[0].part_directives[0].directive == DirectiveType::SustainingPedal);
    const auto* normalised = find_correction(*ingested, "midi.pedal_switch_values");
    REQUIRE(normalised != nullptr);
    CHECK(normalised->description.find("2 CC64/CC67") != std::string::npos);
    CHECK(find_correction(*ingested, "midi.control_change_events") == nullptr);

    const auto compiled = compile_to_midi(*ingested->score, 480);
    REQUIRE(compiled.has_value());
    REQUIRE(compiled->midi.control_changes.size() == 2);
    CHECK(compiled->midi.control_changes[0].value == 127);
    CHECK(compiled->midi.control_changes[1].value == 0);
}

TEST_CASE("MIDI ingestion accounts for controller and program timelines without Score carriers",
          "[corpus-ir][ingestion][midi][controller][program][loss-accounting]") {
    MidiFile midi;
    midi.ppq = 480;
    midi.notes.push_back({0, 1920, 0, 60, 80});
    midi.control_changes.push_back({240, 0, 1, 80});
    midi.program_changes.push_back({0, 0, 41});
    const auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{218}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    CHECK(ingested->score->parts[0].part_directives.empty());
    const auto* controls = find_correction(*ingested, "midi.control_change_events");
    const auto* programs = find_correction(*ingested, "midi.program_change_events");
    REQUIRE(controls != nullptr);
    REQUIRE(programs != nullptr);
    CHECK(controls->description.find("1 valid") != std::string::npos);
    CHECK(programs->description.find("1 valid") != std::string::npos);
}

TEST_CASE("MIDI ingestion does not globalise ambiguous or channel-local pedal state",
          "[corpus-ir][ingestion][midi][controller][trust-boundary]") {
    SECTION("same-tick pedal change has no PartDirective ordering carrier") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 1920, 0, 60, 80});
        midi.control_changes.push_back({240, 0, 64, 127});
        midi.control_changes.push_back({480, 0, 64, 0});
        midi.control_changes.push_back({480, 0, 64, 127});
        midi.control_changes.push_back({960, 0, 64, 0});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{219}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->score->parts[0].part_directives.empty());
        const auto* controls = find_correction(*ingested, "midi.control_change_events");
        REQUIRE(controls != nullptr);
        CHECK(controls->description.find("4 valid") != std::string::npos);
    }

    SECTION("multiple note channels retain explicit ownership loss") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 1920, 0, 60, 80});
        midi.notes.push_back({0, 1920, 1, 67, 80});
        midi.control_changes.push_back({240, 0, 64, 127});
        midi.control_changes.push_back({960, 0, 64, 0});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{220}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->score->parts[0].part_directives.empty());
        CHECK(find_correction(*ingested, "midi.note_channel_ownership") != nullptr);
        const auto* controls = find_correction(*ingested, "midi.control_change_events");
        REQUIRE(controls != nullptr);
        CHECK(controls->description.find("2 valid") != std::string::npos);
    }

    SECTION("redundant and unmatched transitions remain residual around a completed span") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 1920, 0, 60, 80});
        midi.control_changes.push_back({240, 0, 64, 127});
        midi.control_changes.push_back({480, 0, 64, 127});
        midi.control_changes.push_back({960, 0, 64, 0});
        midi.control_changes.push_back({1200, 0, 64, 127});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{222}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        REQUIRE(ingested->score->parts[0].part_directives.size() == 1);
        CHECK(ingested->score->parts[0].part_directives[0].start == ScoreTime{1, Beat{1, 8}});
        CHECK(ingested->score->parts[0].part_directives[0].end == ScoreTime{1, Beat{1, 2}});
        const auto* controls = find_correction(*ingested, "midi.control_change_events");
        REQUIRE(controls != nullptr);
        CHECK(controls->description.find("2 valid") != std::string::npos);
    }

    SECTION("a pedal on a different channel is not assigned to the sole note Part") {
        MidiFile midi;
        midi.ppq = 480;
        midi.notes.push_back({0, 1920, 0, 60, 80});
        midi.control_changes.push_back({240, 1, 64, 127});
        midi.control_changes.push_back({960, 1, 64, 0});
        const auto bytes = write_midi(midi);
        REQUIRE(bytes.has_value());

        const auto ingested = ingest_midi(*bytes, IngestedWorkId{223}, IngestionOptions{});
        REQUIRE(ingested.has_value());
        REQUIRE(ingested->score.has_value());
        CHECK(ingested->score->parts[0].part_directives.empty());
        const auto* controls = find_correction(*ingested, "midi.control_change_events");
        REQUIRE(controls != nullptr);
        CHECK(controls->description.find("2 valid") != std::string::npos);
    }
}

TEST_CASE("MIDI ingestion reports discarded Type-1 track topology",
          "[corpus-ir][ingestion][midi][track][loss-accounting]") {
    MidiFile midi;
    midi.ppq = 120;
    midi.notes.push_back({0, 120, 0, 60, 80});
    auto bytes = write_midi(midi);
    REQUIRE(bytes.has_value());
    REQUIRE(bytes->size() >= 14);
    (*bytes)[9] = 1;
    (*bytes)[11] = 2;
    const std::vector<std::uint8_t> empty_track = {
        'M', 'T', 'r', 'k', 0, 0, 0, 4, 0, 0xFF, 0x2F, 0};
    bytes->insert(bytes->end(), empty_track.begin(), empty_track.end());

    const auto ingested = ingest_midi(*bytes, IngestedWorkId{221}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto* topology = find_correction(*ingested, "midi.track_topology");
    REQUIRE(topology != nullptr);
    CHECK(topology->description.find("2 source tracks") != std::string::npos);
}

// =============================================================================
// MusicXML ingestion
// =============================================================================

TEST_CASE("MusicXML ingestion produces valid Score", "[corpus-ir][ingestion]") {
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

TEST_CASE("MusicXML ingestion retains an explicit church-mode key identity",
          "[corpus-ir][ingestion][musicxml][key]") {
    const std::string xml = R"(<?xml version="1.0" encoding="UTF-8"?>
<score-partwise version="4.0">
  <part-list><score-part id="P1"><part-name>Piano</part-name></score-part></part-list>
  <part id="P1"><measure number="1"><attributes>
    <divisions>1</divisions>
    <key><fifths>0</fifths><mode>dorian</mode></key>
    <time><beats>4</beats><beat-type>4</beat-type></time>
  </attributes><note><rest/><duration>4</duration><voice>1</voice><type>whole</type></note>
  </measure></part>
</score-partwise>)";

    const auto result = ingest_musicxml(xml, IngestedWorkId{204}, IngestionOptions{});
    REQUIRE(result.has_value());
    REQUIRE(result->score.has_value());
    REQUIRE(result->score->key_map.size() == 1);
    const auto& key = result->score->key_map[0].key;
    CHECK(key.root.letter == 1); // D
    CHECK(key.root.accidental == 0);
    CHECK(key.mode.name == "dorian");
    CHECK(key.accidentals == 0);
    CHECK(is_compilable(*result->score));
}

TEST_CASE("MusicXML ingestion preserves chord ownership instead of dropping chord tones",
          "[corpus-ir][ingestion][musicxml][polyphony][trust-boundary]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};
    measure.notes.push_back({SpelledPitch{0, 0, 4}, Beat{1, 4}, false, false, 1});
    measure.notes.push_back({SpelledPitch{2, 0, 4}, Beat{1, 4}, false, true, 1});
    measure.notes.push_back({SpelledPitch{4, 0, 4}, Beat{1, 4}, false, true, 1});
    part.measures.push_back(measure);
    source.parts.push_back(part);
    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());

    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{207}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& voice = ingested->score->parts[0].measures[0].voices[0];
    const auto chord = std::find_if(voice.events.begin(),
                                    voice.events.end(),
                                    [](const Event& event) { return event.is_note_group(); });
    REQUIRE(chord != voice.events.end());
    REQUIRE(chord->as_note_group() != nullptr);
    CHECK(chord->as_note_group()->notes.size() == 3);
    CHECK(is_compilable(*ingested->score));
}

TEST_CASE("MusicXML ingestion reconstructs typed harmony as a Score point event",
          "[corpus-ir][ingestion][musicxml][harmony][roundtrip]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};
    measure.key_fifths = 0;
    measure.key_mode = "major";
    measure.key_tonic = SpelledPitch{0, 0, 4};
    measure.key_is_major = true;
    measure.notes.push_back({{}, Beat{1, 1}, true, false, 1});

    MusicXmlHarmony harmony;
    harmony.offset = Beat{1, 2};
    harmony.symbol.root = SpelledPitch{4, 0, 4};
    harmony.symbol.quality = "dominant";
    harmony.symbol.bass = SpelledPitch{6, 0, 3};
    harmony.symbol.roman = "V65";
    harmony.symbol.numeral = ChordNumeral{5, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
    harmony.symbol.inversion = 1;
    harmony.symbol.degrees = {{9, -1, ChordDegreeType::Add}};
    measure.harmonies.push_back(harmony);
    part.measures.push_back(measure);
    source.parts.push_back(part);

    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());
    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{209}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& events = ingested->score->parts[0].measures[0].voices[0].events;
    const auto found = std::find_if(
        events.begin(), events.end(), [](const Event& event) { return event.is_chord_symbol(); });
    REQUIRE(found != events.end());
    CHECK(found->offset == Beat{1, 2});
    const auto* restored = std::get_if<ChordSymbolEvent>(&found->payload);
    REQUIRE(restored != nullptr);
    CHECK(restored->root == harmony.symbol.root);
    CHECK(restored->quality == "dominant");
    CHECK(restored->bass == harmony.symbol.bass);
    CHECK(restored->roman == "V65");
    CHECK(restored->numeral == harmony.symbol.numeral);
    CHECK(restored->inversion == 1);
    CHECK(restored->degrees == harmony.symbol.degrees);
    CHECK(is_compilable(*ingested->score));
}

TEST_CASE("MusicXML ingestion rejects a harmony point at the measure endpoint",
          "[corpus-ir][ingestion][musicxml][harmony][trust-boundary]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};
    measure.notes.push_back({{}, Beat{1, 1}, true, false, 1});
    MusicXmlHarmony harmony;
    harmony.offset = Beat{1, 1};
    harmony.symbol.root = SpelledPitch{0, 0, 4};
    harmony.symbol.quality = "major";
    measure.harmonies.push_back(harmony);
    part.measures.push_back(measure);
    source.parts.push_back(part);

    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());
    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{210}, IngestionOptions{});
    REQUIRE_FALSE(ingested.has_value());
    CHECK(ingested.error() == ErrorCode::IngestionFailed);
}

TEST_CASE("MusicXML ingestion rejects an event that cannot tile its declared measure",
          "[corpus-ir][ingestion][musicxml][trust-boundary]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 1;
    measure.time_signature = {4, 4};
    measure.notes.push_back({SpelledPitch{0, 0, 4}, Beat{5, 4}, false, false, 1});
    part.measures.push_back(measure);
    source.parts.push_back(part);
    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());

    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{208}, IngestionOptions{});
    REQUIRE_FALSE(ingested.has_value());
    CHECK(ingested.error() == ErrorCode::IngestionFailed);
}

TEST_CASE("MusicXML ingestion preserves global metre and modal-key changes",
          "[corpus-ir][ingestion][musicxml][maps][trust-boundary]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";

    MusicXmlMeasure first;
    first.number = 1;
    first.time_signature = {4, 4};
    first.key_fifths = 0;
    first.key_mode = "major";
    first.key_tonic = SpelledPitch{0, 0, 4};
    first.key_is_major = true;
    first.notes.push_back({{}, Beat{1, 1}, true, false, 1});
    part.measures.push_back(first);

    MusicXmlMeasure second;
    second.number = 2;
    second.time_signature = {3, 4};
    second.key_fifths = 0;
    second.key_mode = "dorian";
    second.key_tonic = SpelledPitch{1, 0, 4};
    second.notes.push_back({{}, Beat{3, 4}, true, false, 1});
    part.measures.push_back(second);
    source.parts.push_back(part);
    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());

    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{212}, IngestionOptions{});
    REQUIRE(ingested.has_value());
    REQUIRE(ingested->score.has_value());
    const auto& score = *ingested->score;
    REQUIRE(score.time_map.size() == 2);
    CHECK(score.time_map[1].bar == 2);
    CHECK(score.time_map[1].time_signature.numerator() == 3);
    CHECK(score.time_map[1].time_signature.denominator() == 4);
    REQUIRE(score.key_map.size() == 2);
    CHECK(score.key_map[1].position == ScoreTime{2, Beat::zero()});
    CHECK(score.key_map[1].key.root == SpelledPitch{1, 0, 4});
    CHECK(score.key_map[1].key.mode.name == "dorian");
    CHECK(score.key_map[1].key.accidentals == 0);
    CHECK(is_compilable(score));
}

TEST_CASE("MusicXML corpus profile rejects divergent per-part metre",
          "[corpus-ir][ingestion][musicxml][maps][trust-boundary]") {
    MusicXmlScore source;
    for (int part_index = 0; part_index < 2; ++part_index) {
        MusicXmlPart part;
        part.id = part_index == 0 ? "P1" : "P2";
        part.name = "Part";
        MusicXmlMeasure measure;
        measure.number = 1;
        measure.time_signature = part_index == 0 ? std::pair{4, 4} : std::pair{3, 4};
        measure.notes.push_back({{}, part_index == 0 ? Beat{1, 1} : Beat{3, 4}, true, false, 1});
        part.measures.push_back(measure);
        source.parts.push_back(part);
    }
    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());

    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{213}, IngestionOptions{});
    REQUIRE_FALSE(ingested.has_value());
    CHECK(ingested.error() == ErrorCode::IngestionFailed);
}

TEST_CASE("MusicXML corpus profile rejects measure labels it cannot retain",
          "[corpus-ir][ingestion][musicxml][trust-boundary]") {
    MusicXmlScore source;
    MusicXmlPart part;
    part.id = "P1";
    part.name = "Piano";
    MusicXmlMeasure measure;
    measure.number = 0;
    measure.time_signature = {4, 4};
    measure.notes.push_back({{}, Beat{1, 1}, true, false, 1});
    part.measures.push_back(measure);
    source.parts.push_back(part);
    const auto xml = write_musicxml(source);
    REQUIRE(xml.has_value());

    const auto ingested = ingest_musicxml(*xml, IngestedWorkId{214}, IngestionOptions{});
    REQUIRE_FALSE(ingested.has_value());
    CHECK(ingested.error() == ErrorCode::IngestionFailed);
}

// =============================================================================
// Analysis runs on ingest
// =============================================================================

TEST_CASE("Analysis completes during ingestion", "[corpus-ir][ingestion]") {
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

TEST_CASE("Empty MIDI data returns error", "[corpus-ir][ingestion]") {
    std::vector<std::uint8_t> empty_data;
    IngestionOptions opts;
    opts.title = "Empty Test";

    auto result = ingest_midi(empty_data, IngestedWorkId{8}, opts);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// Error: malformed MusicXML
// =============================================================================

TEST_CASE("Malformed MusicXML returns error", "[corpus-ir][ingestion]") {
    IngestionOptions opts;
    opts.title = "Malformed Test";

    auto result = ingest_musicxml("not valid xml", IngestedWorkId{9}, opts);
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// IngestionConfidence population
// =============================================================================

TEST_CASE("IngestionConfidence fully populated for MIDI", "[corpus-ir][ingestion]") {
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

TEST_CASE("IngestionConfidence for MusicXML has high confidence", "[corpus-ir][ingestion]") {
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
    CHECK(ic.duration_quantisation_residual == 0.0f);
}

// =============================================================================
// Workflow wrapper: ingest_midi
// =============================================================================

TEST_CASE("ingest_midi stores work and assigns to composer", "[corpus-ir][ingestion][workflow]") {
    CorpusDatabase db;
    auto composer = create_composer_profile(ComposerProfileId{1}, "Mozart");
    db.composers[1] = composer;

    auto midi_data = make_c_major_midi();
    REQUIRE_FALSE(midi_data.empty());

    IngestionOptions opts;
    opts.title = "Sonata K.545";

    auto result = ingest_midi(db,
                              std::span<const std::uint8_t>(midi_data),
                              IngestedWorkId{1},
                              ComposerProfileId{1},
                              opts);
    REQUIRE(result.has_value());

    CHECK(db.works.count(1) == 1);
    CHECK(db.works[1].analysis_complete);
    CHECK(db.composers[1].works.size() == 1);
}

TEST_CASE("ingest_midi rejects invalid quantisation without mutation",
          "[corpus-ir][ingestion][workflow][transaction]") {
    CorpusDatabase db;
    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Mozart");

    IngestionOptions opts;
    opts.title = "Invalid grid";
    opts.quantise_grid = 0;
    auto midi_data = make_c_major_midi();

    auto result = ingest_midi(db,
                              std::span<const std::uint8_t>(midi_data),
                              IngestedWorkId{1},
                              ComposerProfileId{1},
                              opts);
    REQUIRE_FALSE(result.has_value());
    CHECK(result.error() == ErrorCode::CorpusInvalidParameter);
    CHECK(db.works.empty());
    CHECK(db.composers[1].works.empty());
}

TEST_CASE("ingestion wrappers reject missing composers and duplicate work IDs transactionally",
          "[corpus-ir][ingestion][workflow][transaction]") {
    CorpusDatabase db;
    auto midi_data = make_c_major_midi();
    IngestionOptions opts;
    opts.title = "Transactional ingestion";

    auto missing = ingest_midi(db,
                               std::span<const std::uint8_t>(midi_data),
                               IngestedWorkId{1},
                               ComposerProfileId{99},
                               opts);
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error() == ErrorCode::CorpusNotFound);
    CHECK(db.works.empty());

    db.composers[1] = create_composer_profile(ComposerProfileId{1}, "Mozart");
    WorkMetadata existing_metadata;
    existing_metadata.title = "Existing";
    db.works[1] = create_ingested_work(IngestedWorkId{1}, existing_metadata);

    auto duplicate = ingest_midi(db,
                                 std::span<const std::uint8_t>(midi_data),
                                 IngestedWorkId{1},
                                 ComposerProfileId{1},
                                 opts);
    REQUIRE_FALSE(duplicate.has_value());
    CHECK(duplicate.error() == ErrorCode::CorpusDuplicateId);
    CHECK(db.works.size() == 1);
    CHECK(db.works[1].metadata.title == "Existing");
    CHECK(db.composers[1].works.empty());
}

// =============================================================================
// Workflow wrapper: ingest_musicxml
// =============================================================================

TEST_CASE("ingest_musicxml stores work and assigns to composer",
          "[corpus-ir][ingestion][workflow]") {
    CorpusDatabase db;
    auto composer = create_composer_profile(ComposerProfileId{1}, "Beethoven");
    db.composers[1] = composer;

    auto xml = make_simple_musicxml();
    REQUIRE_FALSE(xml.empty());

    IngestionOptions opts;
    opts.title = "Sonata Op. 13";

    auto result = ingest_musicxml(db, xml, IngestedWorkId{1}, ComposerProfileId{1}, opts);
    REQUIRE(result.has_value());

    CHECK(db.works.count(1) == 1);
    CHECK(db.works[1].analysis_complete);
    CHECK(db.composers[1].works.size() == 1);
}
