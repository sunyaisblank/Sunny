/**
 * @file tuning_test.cpp
 * @brief Score-level tuning algebra, persistence, mutation, and projection
 */

#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <sunny/core/score/midi_compiler.hpp>
#include <sunny/core/score/mutations.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/views.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/tuning/historical_temperament.hpp>

using namespace sunny::core;

namespace {

Score make_score() {
    ScoreSpec spec;
    spec.id = ScoreId{44};
    spec.title = "Tuning test";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    PartDefinition part;
    part.name = "Instrument";
    part.abbreviation = "Inst.";
    part.instrument_type = InstrumentType::Custom;
    spec.parts.push_back(std::move(part));
    auto score = create_score(spec);
    REQUIRE(score.has_value());
    return std::move(*score);
}

} // namespace

TEST_CASE("Score tuning default is a complete canonical MIDI pitch function",
          "[score-ir][tuning]") {
    const ScoreTuning tuning;
    REQUIRE(validate_score_tuning(tuning).has_value());
    CHECK(is_standard_midi_tuning(tuning));
    CHECK(tuning.cents_from_reference.front() == -6900.0);
    CHECK(tuning.cents_from_reference[69] == 0.0);
    CHECK(tuning.cents_from_reference.back() == 5800.0);
    REQUIRE(score_tuned_frequency(tuning, 69).has_value());
    CHECK(*score_tuned_frequency(tuning, 69) == Catch::Approx(440.0));
    CHECK(*score_tuned_frequency(tuning, 60) == Catch::Approx(261.6255653005986));
    CHECK_FALSE(score_tuned_frequency(tuning, 128).has_value());
    CHECK_FALSE(score_tuned_frequency(tuning, 255).has_value());
}

TEST_CASE("Score creation accepts a complete tuning and rejects an invalid one",
          "[score-ir][tuning][workflow]") {
    ScoreSpec spec;
    spec.title = "Created tuned";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = SpelledPitch{0, 0, 4};
    spec.key_accidentals = 0;
    PartDefinition part;
    part.name = "Part";
    part.instrument_type = InstrumentType::Custom;
    spec.parts.push_back(part);
    spec.tuning.name = "creation custom";
    spec.tuning.cents_from_reference[60] = -901.25;
    auto created = create_score(spec);
    REQUIRE(created.has_value());
    CHECK(created->tuning == spec.tuning);

    spec.tuning.reference_frequency_hz = 0.0;
    auto rejected = create_score(spec);
    REQUIRE_FALSE(rejected.has_value());
    CHECK(rejected.error() == ErrorCode::InvalidFrequency);
}

TEST_CASE("Score tuning admits arbitrary finite non-octave and non-monotone maps",
          "[score-ir][tuning]") {
    ScoreTuning tuning;
    tuning.name = "finite arbitrary map";
    tuning.cents_from_reference[60] = -850.25;
    tuning.cents_from_reference[61] = -900.5;
    tuning.cents_from_reference[72] = 1199.0;
    REQUIRE(validate_score_tuning(tuning).has_value());
    CHECK(*score_tuned_frequency(tuning, 60) > *score_tuned_frequency(tuning, 61));
}

TEST_CASE("Score tuning closes reference and derived-frequency domains", "[score-ir][tuning]") {
    ScoreTuning tuning;

    tuning.reference_midi_note = 255;
    CHECK_FALSE(validate_score_tuning(tuning).has_value());

    tuning = ScoreTuning{};
    tuning.reference_frequency_hz = 0.0;
    CHECK_FALSE(validate_score_tuning(tuning).has_value());

    tuning = ScoreTuning{};
    tuning.cents_from_reference[69] = 0.01;
    CHECK_FALSE(validate_score_tuning(tuning).has_value());

    tuning = ScoreTuning{};
    tuning.cents_from_reference[1] = std::numeric_limits<double>::infinity();
    CHECK_FALSE(validate_score_tuning(tuning).has_value());

    tuning = ScoreTuning{};
    tuning.cents_from_reference[1] = std::numeric_limits<double>::max();
    CHECK_FALSE(validate_score_tuning(tuning).has_value());
}

TEST_CASE("S28 blocks an invalid root tuning before compilation",
          "[score-ir][tuning][validation]") {
    auto score = make_score();
    score.tuning.cents_from_reference[69] = 1.0;
    const auto diagnostics = validate_structural(score);
    CHECK(std::any_of(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.rule == "S28" && diagnostic.severity == ValidationSeverity::Error &&
               diagnostic.error_code == ErrorCode::InvalidFrequency;
    }));
    auto compiled = compile_to_midi(score);
    REQUIRE_FALSE(compiled.has_value());
    CHECK(compiled.error() == ErrorCode::InvariantViolation);
}

TEST_CASE("Pitch-class deviations expand relative to the selected exact reference",
          "[score-ir][tuning]") {
    auto tuning = score_tuning_from_pitch_class_deviations(
        "Werckmeister III", TUNING_WERCKMEISTER_III, 69, 442.0);
    REQUIRE(tuning.has_value());
    CHECK(tuning->cents_from_reference[69] == 0.0);
    CHECK(*score_tuned_frequency(*tuning, 69) == Catch::Approx(442.0));
    CHECK(tuning->cents_from_reference[60] ==
          Catch::Approx(-900.0 + TUNING_WERCKMEISTER_III[0] - TUNING_WERCKMEISTER_III[9]));
    CHECK_FALSE(is_standard_midi_tuning(*tuning));
}

TEST_CASE("Score tuning schema six round-trips exactly and older schemas migrate",
          "[score-ir][tuning][serialization]") {
    auto score = make_score();
    score.tuning.name = "custom";
    score.tuning.cents_from_reference[60] = -901.25;

    auto encoded = score_to_json(score);
    CHECK(encoded.at("schema_version") == SCORE_IR_SCHEMA_VERSION);
    CHECK(encoded.at("tuning").at("cents_from_reference").size() == 128);
    auto decoded = score_from_json(encoded);
    REQUIRE(decoded.has_value());
    CHECK(decoded->tuning == score.tuning);

    encoded["schema_version"] = 5;
    encoded.erase("tuning");
    auto migrated = score_from_json(encoded);
    REQUIRE(migrated.has_value());
    CHECK(is_standard_midi_tuning(migrated->tuning));

    encoded["schema_version"] = 6;
    CHECK_FALSE(score_from_json(encoded).has_value());

    encoded = score_to_json(score);
    encoded["tuning"]["cents_from_reference"].erase(
        encoded["tuning"]["cents_from_reference"].begin());
    CHECK_FALSE(score_from_json(encoded).has_value());
}

TEST_CASE("Score tuning mutation is undoable, versioned, and exhaustion-atomic",
          "[score-ir][tuning][mutation]") {
    auto score = make_score();
    const Score before = score;
    auto custom = score_tuning_from_pitch_class_deviations("Pythagorean", TUNING_PYTHAGOREAN);
    REQUIRE(custom.has_value());
    UndoStack undo;

    auto changed = set_score_tuning(score, *custom, &undo);
    REQUIRE(changed.has_value());
    CHECK(score.tuning == *custom);
    CHECK(score.version == before.version + 1);
    REQUIRE(sunny::core::undo(score, undo).has_value());
    CHECK(score.tuning == before.tuning);

    score.version = std::numeric_limits<std::uint64_t>::max();
    const auto encoded_before = score_to_json(score);
    const auto undo_size = undo.undo_entries.size();
    auto exhausted = set_score_tuning(score, *custom, &undo);
    REQUIRE_FALSE(exhausted.has_value());
    CHECK(exhausted.error() == ErrorCode::ArithmeticOverflow);
    CHECK(score_to_json(score) == encoded_before);
    CHECK(undo.undo_entries.size() == undo_size);
}

TEST_CASE("Derived scores preserve tuning and MIDI exposes unsupported tuning residual",
          "[score-ir][tuning][views][midi]") {
    auto score = make_score();
    auto custom =
        score_tuning_from_pitch_class_deviations("Meantone", TUNING_QUARTER_COMMA_MEANTONE);
    REQUIRE(custom.has_value());
    score.tuning = *custom;

    const auto reduced_result = piano_reduction(score, ScoreId{45});
    REQUIRE(reduced_result.has_value());
    const auto& reduced = *reduced_result;
    CHECK(reduced.tuning == score.tuning);

    auto compiled = compile_to_midi(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->report.tuning_definitions_requested == 1);
    CHECK(compiled->report.tuning_definitions_written == 0);
    CHECK(compiled->report.has_drops());
    REQUIRE_FALSE(compiled->report.diagnostics.empty());

    score.tuning = ScoreTuning{};
    compiled = compile_to_midi(score);
    REQUIRE(compiled.has_value());
    CHECK(compiled->report.tuning_definitions_requested == 1);
    CHECK(compiled->report.tuning_definitions_written == 1);
}
