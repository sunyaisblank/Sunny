/**
 * @file TSSI004A.cpp
 * @brief Unit tests for Score IR serialisation (SISZ001A)
 *
 * Component: TSSI004A
 * Domain: TS (Test) | Category: SI (Score IR)
 *
 * Tests: SISZ001A
 * Coverage: score_to_json, score_from_json, round-trip, schema version,
 *           stale regions, document state, orchestration extended fields,
 *           backward compatibility (v1 → v2)
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include "Score/SISZ001A.h"
#include "Score/SIMT001A.h"
#include "Score/SIVD001A.h"

using namespace Sunny::Core;

// =============================================================================
// Helpers
// =============================================================================

namespace {

Score make_serialisation_score() {
    Score score;
    score.id = ScoreId{42};
    score.metadata.title = "Serialisation Test";
    score.metadata.composer = "Test Composer";
    score.metadata.total_bars = 2;
    score.metadata.tags = {"test", "score-ir"};
    score.version = 5;

    TempoEvent tempo;
    tempo.position = SCORE_START;
    tempo.bpm = make_bpm(120);
    tempo.beat_unit = BeatUnit::Quarter;
    tempo.transition_type = TempoTransitionType::Immediate;
    tempo.linear_duration = Beat::zero();
    tempo.old_unit = BeatUnit::Quarter;
    tempo.new_unit = BeatUnit::Quarter;
    score.tempo_map.push_back(tempo);

    KeySignatureEntry key_entry;
    key_entry.position = SCORE_START;
    key_entry.key.root = SpelledPitch{0, 0, 4};
    key_entry.key.accidentals = 0;
    score.key_map.push_back(key_entry);

    auto ts = make_time_signature(4, 4);
    TimeSignatureEntry time_entry;
    time_entry.bar = 1;
    time_entry.time_signature = *ts;
    score.time_map.push_back(time_entry);

    Part piano;
    piano.id = PartId{100};
    piano.definition.name = "Piano";
    piano.definition.abbreviation = "Pno.";
    piano.definition.instrument_type = InstrumentType::Piano;

    for (std::uint32_t bar = 1; bar <= 2; ++bar) {
        if (bar == 1) {
            // Bar 1: C4 whole note
            NoteGroup ng;
            Note n;
            n.pitch = SpelledPitch{0, 0, 4};  // C4
            n.velocity = VelocityValue{{}, 80};
            ng.notes.push_back(n);
            ng.duration = ts->measure_duration();
            Event event{EventId{1001}, Beat::zero(), ng};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            piano.measures.push_back(measure);
        } else {
            // Bar 2: rest
            RestEvent rest{ts->measure_duration(), true};
            Event event{EventId{2001}, Beat::zero(), rest};
            Voice voice{0, {event}, {}};
            Measure measure{bar, {voice}, std::nullopt, std::nullopt};
            piano.measures.push_back(measure);
        }
    }

    score.parts.push_back(piano);

    // Section
    ScoreSection section;
    section.id = SectionId{200};
    section.label = "A";
    section.start = SCORE_START;
    section.end = ScoreTime{3, Beat::zero()};
    section.form_function = FormFunction::Expository;
    score.section_map.push_back(section);

    return score;
}

}  // anonymous namespace

// =============================================================================
// SISZ001A: JSON Round-trip
// =============================================================================

TEST_CASE("SISZ001A: score round-trip via JSON", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    CHECK(restored->id == original.id);
    CHECK(restored->metadata.title == original.metadata.title);
    CHECK(restored->metadata.composer == original.metadata.composer);
    CHECK(restored->metadata.total_bars == original.metadata.total_bars);
    CHECK(restored->metadata.tags == original.metadata.tags);
    CHECK(restored->version == original.version);
    CHECK(restored->parts.size() == original.parts.size());
    CHECK(restored->tempo_map.size() == original.tempo_map.size());
    CHECK(restored->key_map.size() == original.key_map.size());
    CHECK(restored->time_map.size() == original.time_map.size());
    CHECK(restored->section_map.size() == original.section_map.size());
}

TEST_CASE("SISZ001A: JSON string round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json_str = score_to_json_string(original);
    auto restored = score_from_json_string(json_str);
    REQUIRE(restored.has_value());

    CHECK(restored->id == original.id);
    CHECK(restored->metadata.title == "Serialisation Test");
    CHECK(restored->parts.size() == 1);
}

TEST_CASE("SISZ001A: note content survives round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    // Bar 1 should have a note
    auto& bar1 = restored->parts[0].measures[0];
    REQUIRE(bar1.voices.size() == 1);
    REQUIRE(bar1.voices[0].events.size() == 1);
    CHECK(bar1.voices[0].events[0].is_note_group());

    auto* ng = bar1.voices[0].events[0].as_note_group();
    REQUIRE(ng != nullptr);
    REQUIRE(ng->notes.size() == 1);
    CHECK(ng->notes[0].pitch.letter == 0);   // C
    CHECK(ng->notes[0].pitch.octave == 4);
    CHECK(ng->notes[0].velocity.value == 80);

    // Bar 2 should have a rest
    auto& bar2 = restored->parts[0].measures[1];
    CHECK(bar2.voices[0].events[0].is_rest());
}

TEST_CASE("SISZ001A: section round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    REQUIRE(restored->section_map.size() == 1);

    CHECK(restored->section_map[0].label == "A");
    CHECK(restored->section_map[0].start == SCORE_START);
    CHECK(restored->section_map[0].form_function.has_value());
    CHECK(*restored->section_map[0].form_function == FormFunction::Expository);
}

// =============================================================================
// SISZ001A: Schema Version
// =============================================================================

TEST_CASE("SISZ001A: schema version check", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    // Valid schema version
    CHECK(json.at("schema_version").get<int>() == SCORE_IR_SCHEMA_VERSION);

    // Modify to wrong version
    json["schema_version"] = 999;
    auto result = score_from_json(json);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: invalid JSON string", "[score-ir][serialisation]") {
    auto result = score_from_json_string("not valid json");
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// SISZ001A: part definition round-trip
// =============================================================================

TEST_CASE("SISZ001A: part definition round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());

    auto& part = restored->parts[0];
    CHECK(part.definition.name == "Piano");
    CHECK(part.definition.abbreviation == "Pno.");
    CHECK(part.definition.instrument_type == InstrumentType::Piano);
}

// =============================================================================
// SISZ001A: Stale regions round-trip
// =============================================================================

TEST_CASE("SISZ001A: stale regions survive round-trip",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    ScoreRegion sr;
    sr.start = SCORE_START;
    sr.end = ScoreTime{2, Beat::zero()};
    original.stale_harmonic_regions.push_back(sr);

    ScoreRegion sr2;
    sr2.start = ScoreTime{2, Beat::zero()};
    sr2.end = ScoreTime{3, Beat::zero()};
    original.stale_orchestration_regions.push_back(sr2);

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->stale_harmonic_regions.size() == 1);
    CHECK(restored->stale_harmonic_regions[0].start == SCORE_START);
    CHECK(restored->stale_orchestration_regions.size() == 1);
    CHECK(restored->stale_orchestration_regions[0].start == ScoreTime{2, Beat::zero()});
}

// =============================================================================
// SISZ001A: DocumentState round-trip
// =============================================================================

TEST_CASE("SISZ001A: document state survives round-trip",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    original.state = DocumentState::Valid;

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->state == DocumentState::Valid);
}

// =============================================================================
// SISZ001A: OrchestrationAnnotation extended fields round-trip
// =============================================================================

TEST_CASE("SISZ001A: orchestration extended fields survive round-trip",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};
    oa.start = SCORE_START;
    oa.end = ScoreTime{3, Beat::zero()};
    oa.role = TexturalRole::Melody;
    oa.texture = TextureType::Polyphonic;
    oa.dynamic_balance = DynamicBalance::Foreground;
    oa.doubled_part = PartId{200};
    oa.pedal_pitch = SpelledPitch{0, 0, 3};  // C3
    oa.dialogue_partner = PartId{300};
    original.orchestration_annotations.push_back(oa);

    auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    REQUIRE(restored->orchestration_annotations.size() == 1);

    const auto& r = restored->orchestration_annotations[0];
    CHECK(r.part_id == PartId{100});
    CHECK(r.role == TexturalRole::Melody);
    REQUIRE(r.texture.has_value());
    CHECK(*r.texture == TextureType::Polyphonic);
    REQUIRE(r.dynamic_balance.has_value());
    CHECK(*r.dynamic_balance == DynamicBalance::Foreground);
    REQUIRE(r.doubled_part.has_value());
    CHECK(r.doubled_part->value == 200);
    REQUIRE(r.pedal_pitch.has_value());
    CHECK(r.pedal_pitch->letter == 0);
    CHECK(r.pedal_pitch->octave == 3);
    REQUIRE(r.dialogue_partner.has_value());
    CHECK(r.dialogue_partner->value == 300);
}

// =============================================================================
// SISZ001A: Backward compatibility (v1/v2 schema loads as v3)
// =============================================================================

TEST_CASE("SISZ001A: v1 schema document loads successfully",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    // Downgrade schema version to 1 to simulate a v1 document
    json["schema_version"] = 1;

    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.title == "Serialisation Test");
    // v1 document will have default values for fields added in v2/v3
    CHECK(restored->state == DocumentState::Draft);
}

TEST_CASE("SISZ001A: v2 schema document loads successfully",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    json["schema_version"] = 2;

    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.title == "Serialisation Test");
}

// =============================================================================
// SISZ001A: Deserialisation trust boundary — input validation
// =============================================================================

TEST_CASE("SISZ001A: beat_from_json rejects denominator zero",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto json_str = score_to_json_string(original);

    // Corrupt a Beat denominator to 0 in the JSON
    auto j = nlohmann::json::parse(json_str);
    // Corrupt the tempo map's first entry position beat den
    j["tempo_map"][0]["position"]["beat"]["den"] = 0;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: beat_from_json rejects negative denominator",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    j["tempo_map"][0]["position"]["beat"]["den"] = -1;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: spelled_pitch_from_json rejects letter 200",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt the key map root letter to 200
    j["key_map"][0]["key"]["root"]["letter"] = 200;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: HarmonicAnnotation.chord survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord.notes = {60, 64, 67};  // C major triad
    ha.chord.root = 0;              // C
    ha.chord.quality = "major";
    ha.chord.inversion = 0;
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    ha.confidence = 1.0f;
    original.harmonic_annotations.push_back(ha);

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->harmonic_annotations.size() == 1);

    const auto& rc = restored->harmonic_annotations[0].chord;
    CHECK(rc.notes.size() == 3);
    CHECK(rc.notes[0] == 60);
    CHECK(rc.notes[1] == 64);
    CHECK(rc.notes[2] == 67);
    CHECK(rc.root == 0);
    CHECK(rc.quality == "major");
    CHECK(rc.inversion == 0);
}

TEST_CASE("SISZ001A: TempoEvent transition fields survive round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    // Add a second tempo event with non-default transition fields
    TempoEvent te;
    te.position = ScoreTime{2, Beat::zero()};
    te.bpm = make_bpm(80);
    te.beat_unit = BeatUnit::Half;
    te.transition_type = TempoTransitionType::Linear;
    te.linear_duration = Beat{2, 1};
    te.old_unit = BeatUnit::Quarter;
    te.new_unit = BeatUnit::Half;
    original.tempo_map.push_back(te);

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->tempo_map.size() == 2);

    const auto& rt = restored->tempo_map[1];
    CHECK(rt.transition_type == TempoTransitionType::Linear);
    CHECK(rt.linear_duration == Beat{2, 1});
    CHECK(rt.old_unit == BeatUnit::Quarter);
    CHECK(rt.new_unit == BeatUnit::Half);
}

TEST_CASE("SISZ001A: KeySignature.mode survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    // Set mode to major scale on the key map entry
    ScaleDefinition major_mode;
    major_mode.intervals = {0, 2, 4, 5, 7, 9, 11, 0, 0, 0, 0, 0};
    major_mode.note_count = 7;
    major_mode.name = "";
    major_mode.description = "";
    original.key_map[0].key.mode = major_mode;

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->key_map.size() == 1);

    const auto& rm = restored->key_map[0].key.mode;
    CHECK(rm.note_count == 7);
    auto ivs = rm.get_intervals();
    REQUIRE(ivs.size() == 7);
    CHECK(ivs[0] == 0);
    CHECK(ivs[1] == 2);
    CHECK(ivs[2] == 4);
    CHECK(ivs[3] == 5);
    CHECK(ivs[4] == 7);
    CHECK(ivs[5] == 9);
    CHECK(ivs[6] == 11);
}

TEST_CASE("SISZ001A: Measure.local_time survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    // Set local_time on bar 1
    TimeSignature local_ts;
    local_ts.groups = {3, 3, 2};
    local_ts.denominator = 8;
    original.parts[0].measures[0].local_time = local_ts;

    // Adjust event durations to match 8/8 = Beat{8,8} = Beat{1,1}
    // The measure duration for 3+3+2 / 8 is 8/8 = 1 whole note
    auto& ev = original.parts[0].measures[0].voices[0].events[0];
    if (auto* ng = std::get_if<NoteGroup>(&ev.payload)) {
        ng->duration = Beat{8, 8};  // 8/8 = 1 whole note
    }

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& lt = restored->parts[0].measures[0].local_time;
    REQUIRE(lt.has_value());
    CHECK(lt->groups == std::vector<int>{3, 3, 2});
    CHECK(lt->denominator == 8);
}

TEST_CASE("SISZ001A: score_from_json rejects duplicate EventIds",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt: set bar 2's event id to same as bar 1's (1001)
    j["parts"][0]["measures"][1]["voices"][0]["events"][0]["id"] = 1001;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: score_from_json runs full validation on load",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt total_bars to be wrong: say 3 bars but only 2 measures per part
    j["metadata"]["total_bars"] = 3;

    auto result = score_from_json(j);
    // S1 validation should detect measure count mismatch
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: score_from_json rejects unrecognised event type",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt event type
    j["parts"][0]["measures"][0]["voices"][0]["events"][0]["type"] = "nonexistent";

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: PartDefinition.range survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    original.parts[0].definition.range.absolute_low = SpelledPitch{0, 0, 1};
    original.parts[0].definition.range.absolute_high = SpelledPitch{0, 0, 8};
    original.parts[0].definition.range.comfortable_low = SpelledPitch{0, 0, 2};
    original.parts[0].definition.range.comfortable_high = SpelledPitch{0, 0, 7};

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& r = restored->parts[0].definition.range;
    CHECK(r.absolute_low.octave == 1);
    CHECK(r.absolute_high.octave == 8);
    CHECK(r.comfortable_low.octave == 2);
    CHECK(r.comfortable_high.octave == 7);
}

TEST_CASE("SISZ001A: articulation_vocabulary survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    original.parts[0].definition.articulation_vocabulary = {
        ArticulationType::Staccato,
        ArticulationType::Tenuto,
        ArticulationType::Accent
    };

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& vocab = restored->parts[0].definition.articulation_vocabulary;
    REQUIRE(vocab.size() == 3);
    CHECK(vocab[0] == ArticulationType::Staccato);
    CHECK(vocab[1] == ArticulationType::Tenuto);
    CHECK(vocab[2] == ArticulationType::Accent);
}

TEST_CASE("SISZ001A: articulation_map survives round-trip",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    ArticulationMapping ks;
    ks.type = ArticulationMapping::Type::Keyswitch;
    ks.keyswitch_pitch = SpelledPitch{0, 0, 1};  // C1

    ArticulationMapping cc;
    cc.type = ArticulationMapping::Type::CC;
    cc.cc_number = 64;
    cc.cc_value = 127;

    // Combined mapping that nests two sub-mappings
    ArticulationMapping combined;
    combined.type = ArticulationMapping::Type::Combined;
    combined.combined = {ks, cc};

    original.parts[0].definition.rendering.articulation_map[ArticulationType::Staccato] = ks;
    original.parts[0].definition.rendering.articulation_map[ArticulationType::Tenuto] = combined;

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& am = restored->parts[0].definition.rendering.articulation_map;
    REQUIRE(am.size() == 2);
    REQUIRE(am.count(ArticulationType::Staccato));
    CHECK(am.at(ArticulationType::Staccato).type == ArticulationMapping::Type::Keyswitch);
    CHECK(am.at(ArticulationType::Staccato).keyswitch_pitch.octave == 1);
    REQUIRE(am.count(ArticulationType::Tenuto));
    CHECK(am.at(ArticulationType::Tenuto).type == ArticulationMapping::Type::Combined);
    REQUIRE(am.at(ArticulationType::Tenuto).combined.size() == 2);
    CHECK(am.at(ArticulationType::Tenuto).combined[0].type == ArticulationMapping::Type::Keyswitch);
    CHECK(am.at(ArticulationType::Tenuto).combined[1].type == ArticulationMapping::Type::CC);
    CHECK(am.at(ArticulationType::Tenuto).combined[1].cc_number == 64);
}

TEST_CASE("SISZ001A: beat_from_json normalises non-canonical fractions",
          "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    // Manually inject a non-canonical beat into the JSON
    auto j = score_to_json(original);
    // Overwrite the first event's offset beat with {2, 4} instead of {1, 2}
    auto& events = j["parts"][0]["measures"][0]["voices"][0]["events"];
    if (!events.empty()) {
        events[0]["offset"]["num"] = 2;
        events[0]["offset"]["den"] = 4;
    }

    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    if (!restored->parts[0].measures.empty() &&
        !restored->parts[0].measures[0].voices.empty() &&
        !restored->parts[0].measures[0].voices[0].events.empty()) {
        const auto& beat = restored->parts[0].measures[0].voices[0].events[0].offset;
        // {2, 4} should normalise to {1, 2}
        CHECK(beat.numerator == 1);
        CHECK(beat.denominator == 2);
    }
}

// =============================================================================
// SISZ001A: Enum bounds-check negative paths
// =============================================================================

TEST_CASE("SISZ001A: out-of-range DocumentState rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["state"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: out-of-range BeatUnit rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["tempo_map"][0]["beat_unit"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: out-of-range InstrumentType rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["parts"][0]["definition"]["instrument_type"] = 200;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: out-of-range TexturalRole rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};
    oa.start = SCORE_START;
    oa.end = ScoreTime{3, Beat::zero()};
    oa.role = TexturalRole::Melody;
    original.orchestration_annotations.push_back(oa);
    j = score_to_json(original);
    j["orchestration_annotations"][0]["role"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: out-of-range FormFunction rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["section_map"][0]["form_function"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("SISZ001A: out-of-range PitchClass root rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord.root = 0;
    ha.chord.quality = "major";
    ha.roman_numeral = "I";
    ha.function = ScoreHarmonicFunction::Tonic;
    ha.key_context.root = SpelledPitch{0, 0, 4};
    ha.key_context.accidentals = 0;
    original.harmonic_annotations.push_back(ha);
    auto j = score_to_json(original);
    j["harmonic_annotations"][0]["chord"]["root"] = 15;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}
