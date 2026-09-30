/**
 * @file validation_record_test.cpp
 * @brief Versioned Ableton host-evidence record tests
 */

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/mix/workflows.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/score/workflows.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/core/timbre/workflows.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/ableton/validation_record.hpp>
#include <sunny/version.hpp>

using nlohmann::json;
using sunny::core::ErrorCode;
using namespace sunny::infrastructure;
using namespace sunny::infrastructure::formats;

namespace {

AbletonTargetSnapshot current_empty_snapshot() {
    CommandBuffer transport;
    transport.set_target_profile(modeled_target_profile({12, 3, 1, "12.3.1"}));
    auto observed = transport.target_snapshot();
    REQUIRE(observed);
    REQUIRE(*observed);
    auto strict = target_snapshot_from_json(target_snapshot_to_json(**observed));
    REQUIRE(strict);
    return std::move(*strict);
}

AbletonOperatorEnvironment environment(bool include_max = true) {
    AbletonOperatorEnvironment result{
        "Suite", "Windows 11", "x86_64", "0123456789abcdef", std::nullopt};
    if (include_max) result.max = AbletonMaxOperatorEnvironment{"9.0.5", "9.0.5", "licensed"};
    return result;
}

json compilation_evidence(const AbletonTargetSnapshot& snapshot, bool complete = true) {
    return {{"success", true},
            {"connected", true},
            {"complete", complete},
            {"target_profile", target_profile_to_json(snapshot.target_profile)},
            {"part_tracks", json::array()},
            {"score", json::object()},
            {"timbre", json::array()},
            {"mix", json::object()},
            {"postconditions", json::object()},
            {"diagnostics", json::array()},
            {"warnings", json::array()}};
}

json project_state() {
    sunny::core::ScoreSpec spec;
    spec.title = "Validation record";
    spec.total_bars = 1;
    spec.bpm = 120.0;
    spec.key_root = sunny::core::SpelledPitch{0, 0, 4};
    spec.parts.resize(1);
    spec.parts[0].name = "Part";
    spec.parts[0].instrument_type = sunny::core::InstrumentType::Synthesiser;
    auto score = sunny::core::create_score(spec);
    REQUIRE(score);
    auto profile = sunny::core::create_timbre_profile(
        sunny::core::TimbreProfileId{1}, score->parts[0].id, "Profile");
    sunny::core::SubtractiveSynth source;
    source.oscillators.push_back(sunny::core::Oscillator{});
    profile.source.data = std::move(source);
    auto mix = sunny::core::create_mix_graph(sunny::core::MixGraphId{1}, {score->parts[0].id});
    const auto encoded_score = sunny::core::score_to_json(*score);
    const auto encoded_timbre = sunny::core::timbre_to_json(profile);
    const auto encoded_mix = sunny::core::mix_to_json(mix);
    const auto parsed_score = sunny::core::score_from_json(encoded_score);
    const auto parsed_timbre = sunny::core::timbre_from_json(encoded_timbre);
    const auto parsed_mix = sunny::core::mix_from_json(encoded_mix);
    REQUIRE(parsed_score);
    REQUIRE(parsed_timbre);
    REQUIRE(parsed_mix);
    const auto canonical_score = sunny::core::score_to_json(*parsed_score);
    const auto canonical_timbre = sunny::core::timbre_to_json(*parsed_timbre);
    const auto canonical_mix = sunny::core::mix_to_json(*parsed_mix);
    REQUIRE(sunny::core::score_from_json(canonical_score));
    REQUIRE(sunny::core::timbre_from_json(canonical_timbre));
    REQUIRE(sunny::core::mix_from_json(canonical_mix));
    return {{"score", canonical_score},
            {"timbre", json::array({canonical_timbre})},
            {"mix", canonical_mix}};
}

AbletonProjectDeploymentPlan completed_plan(const AbletonTargetSnapshot& snapshot) {
    AbletonProjectDeploymentPlan plan;
    plan.ppq = 960;
    plan.project_state = project_state();
    plan.target_snapshot = snapshot;
    plan.mutations.push_back({AbletonDeploymentPhase::Score,
                              LomProtocol::set_property(LomPaths::song(), "tempo", 120.0)});
    plan.consumed = true;
    return plan;
}

AbletonProjectDeploymentAttempt completed_attempt(const AbletonProjectDeploymentPlan& plan) {
    AbletonProjectDeploymentAttempt attempt;
    attempt.status = AbletonProjectDeploymentStatus::Completed;
    attempt.target_before = plan.target_snapshot;
    attempt.target_after = plan.target_snapshot;
    attempt.compilation.emplace();
    attempt.compilation->score.target_profile = plan.target_snapshot.target_profile;
    attempt.mutation_journal.push_back(
        {0,
         AbletonDeploymentPhase::Score,
         plan.mutations[0].request,
         AbletonMutationOutcome::Acknowledged,
         LomValue{json{{"property", "tempo"}, {"requested", 120.0}, {"observed", 120.0}}},
         std::nullopt});
    return attempt;
}

AbletonValidationRecord completed_record() {
    const auto snapshot = current_empty_snapshot();
    auto plan = completed_plan(snapshot);
    auto attempt = completed_attempt(plan);
    auto record = make_ableton_validation_record(
        plan, attempt, environment(), compilation_evidence(snapshot), {"No cleanup required"});
    REQUIRE(record);
    return std::move(*record);
}

} // namespace

TEST_CASE("CommandBuffer synthesizes a snapshot accepted by its advertised current schema",
          "[ableton][validation-record][snapshot]") {
    const auto snapshot = current_empty_snapshot();
    CHECK(snapshot.schema_version == SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION);
    CHECK(snapshot.song_state.at("is_playing") == false);
    CHECK(snapshot.song_state.at("master_track").at("mixer").at("crossfade_assign").is_null());
}

TEST_CASE("current bridge requests have one strict canonical decoder",
          "[ableton][validation-record][protocol]") {
    LomNoteData note;
    note.pitch = sunny::core::MidiNote::from_int(60).value();
    note.start_time = 0.0;
    note.duration = 1.0;
    note.velocity = 100;
    const auto original = LomProtocol::add_new_notes(LomPaths::clip(0, 0), {note});
    const auto encoded = json::parse(LomProtocol::serialize_request(original));
    const auto decoded = LomProtocol::deserialize_request(encoded);
    REQUIRE(decoded);
    CHECK(LomProtocol::serialize_request(*decoded) == LomProtocol::serialize_request(original));

    auto malformed = encoded;
    malformed["bridge_protocol_version"] = SUNNY_BRIDGE_PROTOCOL_VERSION + 1;
    CHECK_FALSE(LomProtocol::deserialize_request(malformed));
    malformed = encoded;
    malformed["extra"] = true;
    CHECK_FALSE(LomProtocol::deserialize_request(malformed));
    malformed = encoded;
    malformed["path"] = "song/tracks/00/clip_slots/0/clip";
    CHECK_FALSE(LomProtocol::deserialize_request(malformed));
    malformed = json::parse(
        LomProtocol::serialize_request(LomProtocol::get_property(LomPaths::song(), "tempo")));
    malformed["args"] = json::array();
    CHECK_FALSE(LomProtocol::deserialize_request(malformed));
}

TEST_CASE("completed Ableton validation evidence round trips without promoting completeness",
          "[ableton][validation-record][roundtrip]") {
    const auto record = completed_record();
    CHECK(record.sunny_version == sunny::SUNNY_VERSION);
    CHECK(record.execution_trace_complete());
    CHECK(record.target_may_be_partially_modified());
    REQUIRE(record.compilation_evidence);
    CHECK(record.compilation_evidence->at("complete") == true);

    const auto encoded = ableton_validation_record_to_json(record);
    CHECK(encoded.at("operator_environment").at("provenance") == "operator_supplied");
    CHECK(encoded.at("deployment").at("execution_trace_complete") == true);
    const auto parsed = ableton_validation_record_from_json(encoded);
    REQUIRE(parsed);
    CHECK(ableton_validation_record_to_json(*parsed) == encoded);

    auto incomplete = encoded;
    incomplete["deployment"]["compilation_evidence"]["complete"] = false;
    const auto parsed_incomplete = ableton_validation_record_from_json(incomplete);
    REQUIRE(parsed_incomplete);
    CHECK(parsed_incomplete->execution_trace_complete());
    CHECK_FALSE(parsed_incomplete->compilation_evidence->at("complete").get<bool>());
}

TEST_CASE("failed deployment records preserve uncertainty without claiming a complete trace",
          "[ableton][validation-record][failure]") {
    const auto snapshot = current_empty_snapshot();
    auto plan = completed_plan(snapshot);
    AbletonProjectDeploymentAttempt attempt;
    attempt.status = AbletonProjectDeploymentStatus::ApplyFailed;
    attempt.error = ErrorCode::SendFailed;
    attempt.target_before = snapshot;
    attempt.target_after = snapshot;
    attempt.mutation_journal.push_back({0,
                                        AbletonDeploymentPhase::Score,
                                        plan.mutations[0].request,
                                        AbletonMutationOutcome::Indeterminate,
                                        std::nullopt,
                                        std::string{"connection lost after send"}});
    auto record = make_ableton_validation_record(
        plan, attempt, environment(false), std::nullopt, {"Re-open the Set and inspect tempo"});
    REQUIRE(record);
    CHECK(record->target_may_be_partially_modified());
    CHECK_FALSE(record->execution_trace_complete());
    const auto encoded = ableton_validation_record_to_json(*record);
    CHECK(encoded.at("operator_environment").at("max").is_null());
    CHECK(ableton_validation_record_from_json(encoded));
    auto unknown_error = encoded;
    unknown_error["deployment"]["error_code"] = 4199;
    CHECK_FALSE(ableton_validation_record_from_json(unknown_error));
}

TEST_CASE("validation-record parser rejects provenance, sequence, lifecycle, and derived tampering",
          "[ableton][validation-record][tamper]") {
    const auto canonical = ableton_validation_record_to_json(completed_record());
    const auto rejected = [](json value) {
        const auto parsed = ableton_validation_record_from_json(value);
        REQUIRE_FALSE(parsed);
        CHECK(parsed.error() == ErrorCode::FormatError);
    };

    auto value = canonical;
    value["extra"] = true;
    rejected(value);
    value = canonical;
    value["sunny_version"] = "999.0.0";
    rejected(value);
    value = canonical;
    value["operator_environment"]["provenance"] = "observed";
    rejected(value);
    value = canonical;
    value["operator_environment"]["max"].erase("license_state");
    rejected(value);
    value = canonical;
    value["planned_mutations"][0]["sequence"] = 1;
    rejected(value);
    value = canonical;
    value["planned_mutations"][0]["request"]["bridge_protocol_version"] = 1;
    rejected(value);
    value = canonical;
    value["project_state"]["score"]["extra"] = true;
    rejected(value);
    value = canonical;
    value["project_state"]["score"]["parts"][0]["measures"][0]["voices"][0]["events"][0]["duration"]
         ["den"] = 0;
    rejected(value);
    value = canonical;
    value["deployment"]["mutation_journal"][0]["response_error"] = "contradiction";
    rejected(value);
    value = canonical;
    value["deployment"]["mutation_journal"][0]["outcome"] = "recorded_only";
    rejected(value);
    value = canonical;
    value["deployment"]["target_may_be_partially_modified"] = false;
    rejected(value);
    value = canonical;
    value["deployment"]["execution_trace_complete"] = false;
    rejected(value);
    value = canonical;
    value["deployment"]["compilation_evidence"]["target_profile"]["adapter_name"] = "other";
    rejected(value);
}

TEST_CASE(
    "validation-record construction requires one consumed plan and matching retained evidence",
    "[ableton][validation-record][construction]") {
    const auto snapshot = current_empty_snapshot();
    auto plan = completed_plan(snapshot);
    auto attempt = completed_attempt(plan);
    plan.consumed = false;
    CHECK_FALSE(make_ableton_validation_record(
        plan, attempt, environment(), compilation_evidence(snapshot)));
    plan.consumed = true;
    CHECK_FALSE(make_ableton_validation_record(plan, attempt, environment(), std::nullopt));

    auto bad_environment = environment();
    bad_environment.remote_script_revision.clear();
    CHECK_FALSE(make_ableton_validation_record(
        plan, attempt, std::move(bad_environment), compilation_evidence(snapshot)));
}
