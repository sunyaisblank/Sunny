/**
 * @file serialization_test.cpp
 * @brief Unit tests for Score IR serialisation
 *
 *
 * Coverage: score_to_json, score_from_json, round-trip, schema version,
 *           stale regions, legacy-state migration, orchestration extended fields,
 *           backward compatibility (v1 → v2)
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/serialization_primitives.hpp>
#include <sunny/core/score/validation.hpp>

using namespace sunny::core;

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
            n.pitch = SpelledPitch{0, 0, 4}; // C4
            n.velocity = VelocityValue{{}, 80};
            n.release_velocity = 23;
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

} // anonymous namespace

// =============================================================================
// JSON Round-trip
// =============================================================================

TEST_CASE("score round-trip via JSON", "[score-ir][serialisation]") {
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
    CHECK(score_to_json(*restored) == json);
}

TEST_CASE("Beat JSON is canonical and a parse-serialise fixed point",
          "[score-ir][serialisation][beat]") {
    CHECK(beat_to_json(Beat{4, 4}) == nlohmann::json{{"num", 1}, {"den", 1}});

    const nlohmann::json noncanonical = {{"num", -6}, {"den", 8}};
    const auto decoded = beat_from_json(noncanonical);
    CHECK(decoded.numerator() == -3);
    CHECK(decoded.denominator() == 4);
    CHECK(beat_to_json(decoded) == nlohmann::json{{"num", -3}, {"den", 4}});
}

TEST_CASE("tempo JSON canonicalises equivalent rates and rejects invalid ratios",
          "[score-ir][serialisation][tempo][trust-boundary]") {
    auto encoded = score_to_json(make_serialisation_score());
    encoded["tempo_map"][0]["bpm_num"] = 240;
    encoded["tempo_map"][0]["bpm_den"] = 2;

    const auto restored = score_from_json(encoded);
    REQUIRE(restored.has_value());
    REQUIRE(restored->tempo_map.size() == 1);
    CHECK(restored->tempo_map[0].bpm == PositiveRational{120, 1});

    const auto canonical = score_to_json(*restored);
    CHECK(canonical["tempo_map"][0]["bpm_num"] == 120);
    CHECK(canonical["tempo_map"][0]["bpm_den"] == 1);
    const auto reparsed = score_from_json(canonical);
    REQUIRE(reparsed.has_value());
    CHECK(score_to_json(*reparsed) == canonical);

    encoded["tempo_map"][0]["bpm_num"] = 0;
    CHECK_FALSE(score_from_json(encoded).has_value());
    encoded["tempo_map"][0]["bpm_num"] = 120;
    encoded["tempo_map"][0]["bpm_den"] = -1;
    CHECK_FALSE(score_from_json(encoded).has_value());
}

TEST_CASE("Score JSON string round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    auto json_str = score_to_json_string(original);
    auto restored = score_from_json_string(json_str);
    REQUIRE(restored.has_value());

    CHECK(restored->id == original.id);
    CHECK(restored->metadata.title == "Serialisation Test");
    CHECK(restored->parts.size() == 1);
}

TEST_CASE("note content survives round-trip", "[score-ir][serialisation]") {
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
    CHECK(ng->notes[0].pitch.letter == 0); // C
    CHECK(ng->notes[0].pitch.octave == 4);
    CHECK(ng->notes[0].velocity.value == 80);

    // Bar 2 should have a rest
    auto& bar2 = restored->parts[0].measures[1];
    CHECK(bar2.voices[0].events[0].is_rest());
}

TEST_CASE("structured lyrics round-trip and legacy lyric strings migrate",
          "[score-ir][serialisation][lyric][migration]") {
    auto original = make_serialisation_score();
    NoteGroup continuation;
    continuation.notes.push_back(Note{SpelledPitch{0, 0, 4}, VelocityValue{{}, 80}});
    continuation.notes[0].lyrics.push_back(LyricSyllable{"light", 1, LyricSyllabic::End, false});
    continuation.duration = Beat{1, 1};
    original.parts[0].measures[1].voices[0].events[0].payload = continuation;
    auto& note =
        std::get<NoteGroup>(original.parts[0].measures[0].voices[0].events[0].payload).notes[0];
    note.lyrics = {LyricSyllable{"Sun", 1, LyricSyllabic::Begin, false},
                   LyricSyllable{"Bright", 2, LyricSyllabic::Single, true}};

    auto json = score_to_json(original);
    const auto& encoded = json["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0];
    REQUIRE(encoded.at("lyrics").size() == 2);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    const auto& restored_lyrics =
        std::get<NoteGroup>(restored->parts[0].measures[0].voices[0].events[0].payload)
            .notes[0]
            .lyrics;
    REQUIRE(restored_lyrics.size() == 2);
    CHECK(restored_lyrics[0].text == "Sun");
    CHECK(restored_lyrics[0].syllabic == LyricSyllabic::Begin);
    CHECK(restored_lyrics[1].verse == 2);
    CHECK(restored_lyrics[1].extend);

    auto legacy = score_to_json(make_serialisation_score());
    legacy["schema_version"] = 3;
    auto& legacy_note = legacy["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0];
    legacy_note.erase("lyrics");
    legacy_note["lyric"] = "legacy";
    auto migrated = score_from_json(legacy);
    REQUIRE(migrated.has_value());
    const auto& migrated_lyrics =
        std::get<NoteGroup>(migrated->parts[0].measures[0].voices[0].events[0].payload)
            .notes[0]
            .lyrics;
    REQUIRE(migrated_lyrics.size() == 1);
    CHECK(migrated_lyrics[0].text == "legacy");
    CHECK(migrated_lyrics[0].verse == 1);
    CHECK(migrated_lyrics[0].syllabic == LyricSyllabic::Single);
}

TEST_CASE("RestEvent tuplet context survives round-trip", "[score-ir][serialisation][tuplet]") {
    auto original = make_serialisation_score();
    auto* rest = std::get_if<RestEvent>(&original.parts[0].measures[1].voices[0].events[0].payload);
    REQUIRE(rest != nullptr);
    rest->tuplet_context = TupletContext{TupletId{77}, 1, 1, Beat{1, 1}, std::nullopt};

    const auto json = score_to_json(original);
    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    const auto* restored_rest = restored->parts[0].measures[1].voices[0].events[0].as_rest();
    REQUIRE(restored_rest != nullptr);
    REQUIRE(restored_rest->tuplet_context.has_value());
    CHECK(restored_rest->tuplet_context->id == TupletId{77});
    CHECK(restored_rest->tuplet_context->actual == 1);
    CHECK(restored_rest->tuplet_context->normal == 1);
    CHECK(restored_rest->tuplet_context->normal_type == Beat{1, 1});
}

TEST_CASE("section round-trip", "[score-ir][serialisation]") {
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
// Schema Version
// =============================================================================

TEST_CASE("schema version check", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    // Valid schema version
    CHECK(json.at("schema_version").get<int>() == SCORE_IR_SCHEMA_VERSION);

    // Modify to wrong version
    json["schema_version"] = 999;
    auto result = score_from_json(json);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("release velocity is mandatory in schema seven and migrates from older schemas",
          "[score-ir][serialisation][release-velocity]") {
    auto encoded = score_to_json(make_serialisation_score());
    auto& note = encoded["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0];
    REQUIRE(note["release_velocity"] == 23);

    auto restored = score_from_json(encoded);
    REQUIRE(restored.has_value());
    CHECK(restored->parts[0]
              .measures[0]
              .voices[0]
              .events[0]
              .as_note_group()
              ->notes[0]
              .release_velocity == 23);

    note.erase("release_velocity");
    CHECK_FALSE(score_from_json(encoded));

    encoded["schema_version"] = 6;
    auto migrated = score_from_json(encoded);
    REQUIRE(migrated.has_value());
    CHECK(migrated->parts[0]
              .measures[0]
              .voices[0]
              .events[0]
              .as_note_group()
              ->notes[0]
              .release_velocity == 64);

    encoded = score_to_json(make_serialisation_score());
    encoded["schema_version"] = 6;
    auto legacy_labelled_with_field = score_from_json(encoded);
    REQUIRE(legacy_labelled_with_field.has_value());
    CHECK(legacy_labelled_with_field->parts[0]
              .measures[0]
              .voices[0]
              .events[0]
              .as_note_group()
              ->notes[0]
              .release_velocity == 23);

    encoded = score_to_json(make_serialisation_score());
    encoded["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0]["release_velocity"] =
        128;
    CHECK_FALSE(score_from_json(encoded));
}

TEST_CASE("schema eight preserves typed chord numeral and degree algebra",
          "[score-ir][serialisation][harmony][migration]") {
    auto score = make_serialisation_score();
    ChordSymbolEvent chord;
    chord.root = SpelledPitch{4, 0, 4}; // G
    chord.quality = "dominant";
    chord.bass = SpelledPitch{6, 0, 3}; // B
    chord.roman = "V65";
    chord.numeral = ChordNumeral{5, 0, ChordNumeralKey{0, ChordNumeralMode::Major}};
    chord.inversion = 1;
    chord.degrees = {{9, -1, ChordDegreeType::Add}, {5, 1, ChordDegreeType::Alter}};
    score.parts[0].measures[0].voices[0].events.push_back(Event{EventId{1002}, Beat{1, 2}, chord});

    auto encoded = score_to_json(score);
    REQUIRE(encoded["schema_version"] == 8);
    const auto& event = encoded["parts"][0]["measures"][0]["voices"][0]["events"][1];
    CHECK(event["numeral"] ==
          nlohmann::json{{"root", 5}, {"alteration", 0}, {"key", {{"fifths", 0}, {"mode", 0}}}});
    CHECK(event["inversion"] == 1);
    CHECK(event["degrees"][0] == nlohmann::json{{"value", 9}, {"alteration", -1}, {"type", 0}});

    auto restored = score_from_json(encoded);
    REQUIRE(restored.has_value());
    const auto* restored_chord =
        std::get_if<ChordSymbolEvent>(&restored->parts[0].measures[0].voices[0].events[1].payload);
    REQUIRE(restored_chord != nullptr);
    CHECK(restored_chord->numeral == chord.numeral);
    CHECK(restored_chord->inversion == chord.inversion);
    CHECK(restored_chord->degrees == chord.degrees);

    auto malformed = encoded;
    malformed["parts"][0]["measures"][0]["voices"][0]["events"][1]["numeral"]["key"].erase("mode");
    CHECK_FALSE(score_from_json(malformed));

    auto legacy = encoded;
    legacy["schema_version"] = 7;
    auto& legacy_event = legacy["parts"][0]["measures"][0]["voices"][0]["events"][1];
    legacy_event.erase("numeral");
    legacy_event.erase("inversion");
    legacy_event.erase("degrees");
    auto migrated = score_from_json(legacy);
    REQUIRE(migrated.has_value());
    const auto* migrated_chord =
        std::get_if<ChordSymbolEvent>(&migrated->parts[0].measures[0].voices[0].events[1].payload);
    REQUIRE(migrated_chord != nullptr);
    CHECK_FALSE(migrated_chord->numeral.has_value());
    CHECK_FALSE(migrated_chord->inversion.has_value());
    CHECK(migrated_chord->degrees.empty());
}

TEST_CASE("invalid JSON string", "[score-ir][serialisation]") {
    auto result = score_from_json_string("not valid json");
    CHECK_FALSE(result.has_value());
}

// =============================================================================
// part definition round-trip
// =============================================================================

TEST_CASE("part definition round-trip", "[score-ir][serialisation]") {
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
// Stale regions round-trip
// =============================================================================

TEST_CASE("stale regions survive round-trip", "[score-ir][serialisation]") {
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
// Removed non-authoritative document state
// =============================================================================

TEST_CASE("schema-v5 omits and rejects the removed document state", "[score-ir][serialisation]") {
    auto json = score_to_json(make_serialisation_score());
    CHECK_FALSE(json.contains("state"));

    json["state"] = 1;
    const auto restored = score_from_json(json);
    REQUIRE_FALSE(restored.has_value());
    CHECK(restored.error() == ErrorCode::FormatError);
}

// =============================================================================
// OrchestrationAnnotation extended fields round-trip
// =============================================================================

TEST_CASE("orchestration extended fields survive round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    OrchestrationAnnotation oa;
    oa.part_id = PartId{100};
    oa.start = SCORE_START;
    oa.end = ScoreTime{3, Beat::zero()};
    oa.role = TexturalRole::Melody;
    oa.texture = TextureType::Polyphonic;
    oa.dynamic_balance = DynamicBalance::Foreground;
    oa.doubled_part = PartId{200};
    oa.pedal_pitch = SpelledPitch{0, 0, 3}; // C3
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
// Backward compatibility (v1-v4 schema loads as v5)
// =============================================================================

TEST_CASE("v1 schema document loads successfully", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    // Downgrade schema version to 1 to simulate a v1 document
    json["schema_version"] = 1;

    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.title == "Serialisation Test");
}

TEST_CASE("v2 schema document loads successfully", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();
    auto json = score_to_json(original);

    json["schema_version"] = 2;

    auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.title == "Serialisation Test");
}

TEST_CASE("v4 legacy document state is range-checked and discarded", "[score-ir][serialisation]") {
    auto json = score_to_json(make_serialisation_score());
    json["schema_version"] = 4;
    json["state"] = 2;

    const auto restored = score_from_json(json);
    REQUIRE(restored.has_value());
    CHECK(restored->metadata.title == "Serialisation Test");
    CHECK_FALSE(score_to_json(*restored).contains("state"));
}

// =============================================================================
// Deserialisation trust boundary — input validation
// =============================================================================

TEST_CASE("beat_from_json rejects denominator zero", "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto json_str = score_to_json_string(original);

    // Corrupt a Beat denominator to 0 in the JSON
    auto j = nlohmann::json::parse(json_str);
    // Corrupt the tempo map's first entry position beat den
    j["tempo_map"][0]["position"]["beat"]["den"] = 0;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("beat_from_json rejects negative denominator",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    j["tempo_map"][0]["position"]["beat"]["den"] = -1;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("spelled_pitch_from_json rejects letter 200",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt the key map root letter to 200
    j["key_map"][0]["key"]["root"]["letter"] = 200;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("HarmonicAnnotation.chord survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    HarmonicAnnotation ha;
    ha.position = SCORE_START;
    ha.duration = Beat{1, 1};
    ha.chord.notes = {60, 64, 67}; // C major triad
    ha.chord.root = 0;             // C
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

TEST_CASE("TempoEvent transition fields survive round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    // Add a second tempo event with non-default transition fields
    TempoEvent te;
    te.position = ScoreTime{2, Beat::zero()};
    te.bpm = make_bpm(80);
    te.beat_unit = BeatUnit::Half;
    te.transition_type = TempoTransitionType::Linear;
    te.linear_duration = Beat{1, 1};
    te.old_unit = BeatUnit::Quarter;
    te.new_unit = BeatUnit::Half;
    original.tempo_map.push_back(te);

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    REQUIRE(restored->tempo_map.size() == 2);

    const auto& rt = restored->tempo_map[1];
    CHECK(rt.transition_type == TempoTransitionType::Linear);
    CHECK(rt.linear_duration == Beat{1, 1});
    CHECK(rt.old_unit == BeatUnit::Quarter);
    CHECK(rt.new_unit == BeatUnit::Half);
}

TEST_CASE("KeySignature.mode survives round-trip", "[score-ir][serialisation][trust-boundary]") {
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

TEST_CASE("named ScaleDefinition identity and description survive score round-trip",
          "[score-ir][serialisation][trust-boundary][key]") {
    auto original = make_serialisation_score();
    const auto harmonic_minor = find_scale("harmonic_minor");
    REQUIRE(harmonic_minor.has_value());
    original.key_map[0].key.mode = *harmonic_minor;
    original.key_map[0].key.accidentals = 4;

    const auto encoded = score_to_json(original);
    CHECK(encoded["key_map"][0]["key"]["mode_name"] == "harmonic_minor");
    CHECK(encoded["key_map"][0]["key"]["mode_description"] == "Minor with raised 7th");

    const auto restored = score_from_json(encoded);
    REQUIRE(restored.has_value());
    CHECK(restored->key_map[0].key.mode.name == "harmonic_minor");
    CHECK(restored->key_map[0].key.mode.description == "Minor with raised 7th");
    CHECK(restored->key_map[0].key.mode.intervals == harmonic_minor->intervals);
}

TEST_CASE("schema-v4 rejects a named scale whose payload contradicts the registry",
          "[score-ir][serialisation][trust-boundary][key]") {
    auto encoded = score_to_json(make_serialisation_score());
    encoded["key_map"][0]["key"]["mode_intervals"][2] = 3;
    CHECK_FALSE(score_from_json(encoded).has_value());

    encoded = score_to_json(make_serialisation_score());
    encoded["key_map"][0]["key"].erase("mode_intervals");
    CHECK_FALSE(score_from_json(encoded).has_value());
}

TEST_CASE("legacy key scale without a persisted name migrates as anonymous",
          "[score-ir][serialisation][trust-boundary][key]") {
    auto encoded = score_to_json(make_serialisation_score());
    encoded["schema_version"] = 3;
    encoded["key_map"][0]["key"].erase("mode_name");
    encoded["key_map"][0]["key"].erase("mode_description");

    const auto restored = score_from_json(encoded);
    REQUIRE(restored.has_value());
    CHECK(restored->key_map[0].key.mode.name.empty());
    CHECK(restored->key_map[0].key.mode.note_count == 7);
    CHECK(restored->key_map[0].key.mode.intervals[6] == 11);
}

TEST_CASE("Measure.local_time survives round-trip", "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    // Set local_time on bar 1
    original.parts[0].measures[0].local_time = TimeSignature{{3, 3, 2}, 8};

    // Adjust event durations to match 8/8 = Beat{8,8} = Beat{1,1}
    // The measure duration for 3+3+2 / 8 is 8/8 = 1 whole note
    auto& ev = original.parts[0].measures[0].voices[0].events[0];
    if (auto* ng = std::get_if<NoteGroup>(&ev.payload)) {
        ng->duration = Beat{8, 8}; // 8/8 = 1 whole note
    }

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& lt = restored->parts[0].measures[0].local_time;
    REQUIRE(lt.has_value());
    CHECK(lt->groups() == std::vector<int>{3, 3, 2});
    CHECK(lt->denominator() == 8);
}

TEST_CASE("time-signature JSON rejects invalid grouped metre before Score construction",
          "[score-ir][serialisation][meter][trust-boundary]") {
    auto encoded = score_to_json(make_serialisation_score());

    encoded["time_map"][0]["groups"] = nlohmann::json::array();
    CHECK_FALSE(score_from_json(encoded).has_value());

    encoded = score_to_json(make_serialisation_score());
    encoded["time_map"][0]["groups"] = nlohmann::json::array({3, 0, 2});
    CHECK_FALSE(score_from_json(encoded).has_value());

    encoded = score_to_json(make_serialisation_score());
    encoded["time_map"][0]["denominator"] = 3;
    CHECK_FALSE(score_from_json(encoded).has_value());

    encoded = score_to_json(make_serialisation_score());
    encoded["parts"][0]["measures"][0]["local_time"] =
        nlohmann::json{{"groups", nlohmann::json::array({3, -1, 2})}, {"denominator", 8}};
    CHECK_FALSE(score_from_json(encoded).has_value());
}

TEST_CASE("score_from_json rejects duplicate EventIds",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt: set bar 2's event id to same as bar 1's (1001)
    j["parts"][0]["measures"][1]["voices"][0]["events"][0]["id"] = 1001;

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("score_from_json runs full validation on load",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt total_bars to be wrong: say 3 bars but only 2 measures per part
    j["metadata"]["total_bars"] = 3;

    auto result = score_from_json(j);
    // S1 validation should detect measure count mismatch
    CHECK_FALSE(result.has_value());
}

TEST_CASE("score_from_json blocks rendering-domain errors before publication",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto invalid_channel = score_to_json(original);
    invalid_channel["parts"][0]["definition"]["rendering"]["midi_channel"] = 0;
    auto result = score_from_json(invalid_channel);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::ValidationOnLoadFailed);

    auto invalid_velocity = score_to_json(original);
    auto& note = invalid_velocity["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0];
    note["velocity"]["value"] = 200;
    result = score_from_json(invalid_velocity);
    REQUIRE_FALSE(result);
    CHECK(result.error() == ErrorCode::ValidationOnLoadFailed);
}

TEST_CASE("score_from_json rejects unrecognised event type",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);

    // Corrupt event type
    j["parts"][0]["measures"][0]["voices"][0]["events"][0]["type"] = "nonexistent";

    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("PartDefinition.range survives round-trip", "[score-ir][serialisation][trust-boundary]") {
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

TEST_CASE("multi-staff topology survives round-trip",
          "[score-ir][serialisation][staff][trust-boundary]") {
    auto original = make_serialisation_score();
    original.parts[0].definition.staff_count = 2;
    original.parts[0].definition.staff_clefs = {Clef::Treble, Clef::Bass};
    original.parts[0].measures[0].voices[0].staff_index = 1;
    original.parts[0].measures[1].voices[0].staff_index = 1;

    const auto j = score_to_json(original);
    CHECK(j["parts"][0]["definition"]["staff_clefs"].size() == 2);
    CHECK(j["parts"][0]["measures"][0]["voices"][0]["staff_index"] == 1);

    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    CHECK(restored->parts[0].definition.staff_count == 2);
    CHECK(restored->parts[0].definition.staff_clefs == std::vector<Clef>{Clef::Treble, Clef::Bass});
    CHECK(restored->parts[0].measures[0].voices[0].staff_index == 1);
}

TEST_CASE("legacy scores default Voice ownership and staff clefs",
          "[score-ir][serialisation][staff][backward-compat]") {
    auto j = score_to_json(make_serialisation_score());
    j["parts"][0]["definition"].erase("staff_clefs");
    for (auto& measure : j["parts"][0]["measures"])
        for (auto& voice : measure["voices"])
            voice.erase("staff_index");

    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());
    CHECK(restored->parts[0].definition.staff_clefs.empty());
    CHECK(restored->parts[0].measures[0].voices[0].staff_index == 0);
}

TEST_CASE("articulation_vocabulary survives round-trip",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();

    original.parts[0].definition.articulation_vocabulary = {
        ArticulationType::Staccato, ArticulationType::Tenuto, ArticulationType::Accent};

    auto j = score_to_json(original);
    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    const auto& vocab = restored->parts[0].definition.articulation_vocabulary;
    REQUIRE(vocab.size() == 3);
    CHECK(vocab[0] == ArticulationType::Staccato);
    CHECK(vocab[1] == ArticulationType::Tenuto);
    CHECK(vocab[2] == ArticulationType::Accent);
}

TEST_CASE("articulation_map survives round-trip", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    ArticulationMapping ks;
    ks.type = ArticulationMapping::Type::Keyswitch;
    ks.keyswitch_pitch = SpelledPitch{0, 0, 1}; // C1

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

TEST_CASE("articulation mapping JSON is a canonical validated tagged union",
          "[score-ir][serialisation][articulation-mapping][trust-boundary]") {
    ArticulationMapping cc;
    cc.type = ArticulationMapping::Type::CC;
    cc.cc_number = 11;
    cc.cc_value = 92;
    cc.duration_scale = 0.25F; // Inactive storage must not leak into transport.

    const auto canonical = articulation_mapping_to_json(cc);
    CHECK(canonical.size() == 3);
    CHECK(canonical.contains("type"));
    CHECK(canonical.contains("cc_number"));
    CHECK(canonical.contains("cc_value"));
    CHECK_FALSE(canonical.contains("duration_scale"));
    auto parsed = articulation_mapping_from_json(canonical);
    REQUIRE(parsed);
    CHECK(parsed->type == ArticulationMapping::Type::CC);
    CHECK(parsed->cc_number == 11);
    CHECK(parsed->cc_value == 92);

    auto out_of_range = canonical;
    out_of_range["cc_value"] = 128;
    REQUIRE_FALSE(articulation_mapping_from_json(out_of_range));

    nlohmann::json empty_combined{{"type", static_cast<int>(ArticulationMapping::Type::Combined)},
                                  {"combined", nlohmann::json::array()}};
    REQUIRE_FALSE(articulation_mapping_from_json(empty_combined));

    nlohmann::json too_deep{
        {"type", static_cast<int>(ArticulationMapping::Type::NoteDurationScale)},
        {"duration_scale", 1.0}};
    for (int i = 0; i < 16; ++i) {
        too_deep = nlohmann::json{{"type", static_cast<int>(ArticulationMapping::Type::Combined)},
                                  {"combined", nlohmann::json::array({too_deep})}};
    }
    REQUIRE_FALSE(articulation_mapping_from_json(too_deep));
}

TEST_CASE("beat_from_json normalises non-canonical fractions", "[score-ir][serialisation]") {
    auto original = make_serialisation_score();

    // Manually inject a non-canonical beat into the JSON
    auto j = score_to_json(original);
    // Preserve the valid topology while spelling a whole-note duration as {2, 2}.
    auto& events = j["parts"][0]["measures"][0]["voices"][0]["events"];
    if (!events.empty()) {
        events[0]["duration"]["num"] = 2;
        events[0]["duration"]["den"] = 2;
    }

    auto restored = score_from_json(j);
    REQUIRE(restored.has_value());

    if (!restored->parts[0].measures.empty() && !restored->parts[0].measures[0].voices.empty() &&
        !restored->parts[0].measures[0].voices[0].events.empty()) {
        const auto& beat = restored->parts[0].measures[0].voices[0].events[0].duration();
        // {2, 2} should normalise to {1, 1}
        CHECK(beat.numerator() == 1);
        CHECK(beat.denominator() == 1);
    }
}

// =============================================================================
// Enum bounds-check negative paths
// =============================================================================

TEST_CASE("out-of-range legacy document state rejects",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["schema_version"] = 4;
    j["state"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("out-of-range narrow integers reject instead of wrapping",
          "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    HarmonicAnnotation annotation;
    annotation.position = SCORE_START;
    annotation.duration = Beat{1, 1};
    annotation.chord.root = 0;
    annotation.chord.inversion = 0;
    annotation.function = ScoreHarmonicFunction::Tonic;
    annotation.key_context.root = SpelledPitch{0, 0, 4};
    original.harmonic_annotations.push_back(annotation);

    auto j = score_to_json(original);

    SECTION("velocity") {
        j["parts"][0]["measures"][0]["voices"][0]["events"][0]["notes"][0]["velocity"]["value"] =
            256;

        auto result = score_from_json(j);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }

    SECTION("chord root") {
        j["harmonic_annotations"][0]["chord"]["root"] = std::uint64_t{1} << 32;

        auto result = score_from_json(j);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }

    SECTION("chord inversion") {
        j["harmonic_annotations"][0]["chord"]["inversion"] = std::uint64_t{1} << 32;

        auto result = score_from_json(j);
        REQUIRE_FALSE(result.has_value());
        CHECK(result.error() == ErrorCode::FormatError);
    }
}

TEST_CASE("out-of-range BeatUnit rejects", "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["tempo_map"][0]["beat_unit"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("out-of-range InstrumentType rejects", "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["parts"][0]["definition"]["instrument_type"] = 200;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("out-of-range TexturalRole rejects", "[score-ir][serialisation][trust-boundary]") {
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

TEST_CASE("out-of-range FormFunction rejects", "[score-ir][serialisation][trust-boundary]") {
    auto original = make_serialisation_score();
    auto j = score_to_json(original);
    j["section_map"][0]["form_function"] = 99;
    auto result = score_from_json(j);
    CHECK_FALSE(result.has_value());
}

TEST_CASE("out-of-range PitchClass root rejects", "[score-ir][serialisation][trust-boundary]") {
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
