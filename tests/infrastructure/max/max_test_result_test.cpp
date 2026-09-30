/**
 * @file max_test_result_test.cpp
 * @brief Pinned Cycling '74 max-test result and observation-mapping tests
 */

#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/max_test_result.hpp>
#include <sunny/infrastructure/max/validation_record.hpp>

using nlohmann::json;
using sunny::core::ErrorCode;
using namespace sunny::infrastructure;

namespace {

constexpr std::string_view DATABASE_HASH =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path().parent_path();
}

json read_json(const std::filesystem::path& path) {
    std::ifstream input{path};
    REQUIRE(input.good());
    return json::parse(input);
}

json harness_manifest() {
    return read_json(repository_root() / "max-package/misc/validation/max-test-harness.json");
}

MaxValidationObservation example_observation() {
    const auto encoded = read_json(
        repository_root() / "max-package/misc/validation/max-validation-observation.example.json");
    const auto parsed = max_validation_observation_from_json(encoded);
    REQUIRE(parsed);
    return *parsed;
}

MaxTestRunResult passing_run(const json& manifest) {
    std::set<std::string> names;
    for (const auto& mapping : manifest.at("artifact_assertions")) {
        for (const auto& name : mapping.at("required_assertions"))
            names.emplace(name.get<std::string>());
    }
    for (const auto& mapping : manifest.at("check_assertions")) {
        for (const auto& name : mapping.at("required_assertions"))
            names.emplace(name.get<std::string>());
    }

    MaxTestRunResult run;
    run.database_sha256 = DATABASE_HASH;
    run.test_id = 42;
    run.test_name = "sunny-runtime-smoke.maxtest";
    run.started_at = "2026-08-31 10:00:00";
    run.finished_at = "2026-08-31 10:00:02";
    std::uint64_t assertion_id = 100;
    for (const auto& name : names) {
        run.assertions.push_back({.assertion_id = assertion_id++,
                                  .assertion_name = name,
                                  .outcome = MaxTestAssertionOutcome::Pass,
                                  .finished_at = "2026-08-31 10:00:01"});
    }
    return run;
}

void reject_result(json value) {
    const auto parsed = max_test_run_result_from_json(value);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error() == ErrorCode::FormatError);
}

std::map<MaxValidationCheck, MaxValidationOutcome>
check_outcomes(const MaxValidationObservation& observation) {
    std::map<MaxValidationCheck, MaxValidationOutcome> result;
    for (const auto& check : observation.checks)
        result.emplace(check.check, check.outcome);
    return result;
}

} // namespace

TEST_CASE("normalized max-test result schema is closed and canonical", "[max][max-test][result]") {
    const auto run = passing_run(harness_manifest());
    REQUIRE(run.assertions.size() == 64);
    const auto canonical = max_test_run_result_to_json(run);
    const auto parsed = max_test_run_result_from_json(canonical);
    REQUIRE(parsed);
    CHECK(max_test_run_result_to_json(*parsed) == canonical);

    auto value = canonical;
    value["extra"] = true;
    reject_result(value);
    value = canonical;
    value["database_sha256"] = std::string(64, '0');
    reject_result(value);
    value = canonical;
    value["test"]["id"] = 0;
    reject_result(value);
    value = canonical;
    value["test"]["finished_at"] = "2026-08-31 09:59:59";
    reject_result(value);
    value = canonical;
    value["assertions"][0]["outcome"] = "passed";
    reject_result(value);
    value = canonical;
    value["assertions"][1]["id"] = value["assertions"][0]["id"];
    reject_result(value);
    value = canonical;
    value["assertions"][1]["name"] = value["assertions"][0]["name"];
    reject_result(value);
    value = canonical;
    value["assertions"][0]["finished_at"] = "2026-08-31 10:00:03";
    reject_result(value);
}

TEST_CASE("pinned max-test pass maps only its fourteen declared checks",
          "[max][max-test][application]") {
    const auto manifest = harness_manifest();
    const auto run = passing_run(manifest);
    const auto applied = apply_max_test_run(example_observation(),
                                            manifest,
                                            MAX_TEST_HARNESS_MANIFEST_SHA256,
                                            run,
                                            DATABASE_HASH,
                                            "max-test/results.db3");
    REQUIRE(applied);
    CHECK(std::all_of(applied->artifacts.begin(), applied->artifacts.end(), [](const auto& fact) {
        return fact.discovered && fact.instantiated;
    }));

    const auto outcomes = check_outcomes(*applied);
    constexpr MaxValidationCheck automated[]{
        MaxValidationCheck::PackageDiscovery,
        MaxValidationCheck::PublicClassSurface,
        MaxValidationCheck::SignalTopology,
        MaxValidationCheck::DspSetup,
        MaxValidationCheck::PerformCallback,
        MaxValidationCheck::FiniteSignalOutput,
        MaxValidationCheck::DisconnectedProcessing,
        MaxValidationCheck::ReconnectContinuity,
        MaxValidationCheck::ControlDispatch,
        MaxValidationCheck::ItmScheduleFire,
        MaxValidationCheck::ItmEqualTickOrdering,
        MaxValidationCheck::ItmClearReassign,
        MaxValidationCheck::ReleaseVelocityFormatting,
        MaxValidationCheck::StandaloneTransportIdentity,
    };
    for (const auto check : automated)
        CHECK(outcomes.at(check) == MaxValidationOutcome::Passed);
    CHECK(outcomes.at(MaxValidationCheck::LiveTransportDiscontinuities) ==
          MaxValidationOutcome::NotApplicable);
    CHECK(outcomes.at(MaxValidationCheck::ActiveDspTeardown) == MaxValidationOutcome::NotRun);

    for (const auto& fact : applied->checks) {
        const bool automated_check =
            std::find(std::begin(automated), std::end(automated), fact.check) !=
            std::end(automated);
        if (automated_check) {
            CHECK(fact.evidence_relative_path == "max-test/results.db3");
            CHECK(fact.summary.find(std::string{MAX_TEST_HARNESS_REVISION}) != std::string::npos);
        } else if (fact.check != MaxValidationCheck::LiveTransportDiscontinuities) {
            CHECK_FALSE(fact.evidence_relative_path);
        }
    }
    const auto round_trip =
        max_validation_observation_from_json(max_validation_observation_to_json(*applied));
    REQUIRE(round_trip);
}

TEST_CASE("one failed assertion has a bounded conservative mapping",
          "[max][max-test][application][failure]") {
    const auto manifest = harness_manifest();
    auto run = passing_run(manifest);
    const auto failed =
        std::find_if(run.assertions.begin(), run.assertions.end(), [](const auto& row) {
            return row.assertion_name == "sunny-lfo-output";
        });
    REQUIRE(failed != run.assertions.end());
    failed->outcome = MaxTestAssertionOutcome::Fail;

    const auto applied = apply_max_test_run(example_observation(),
                                            manifest,
                                            MAX_TEST_HARNESS_MANIFEST_SHA256,
                                            run,
                                            DATABASE_HASH,
                                            "max-test/results.db3");
    REQUIRE(applied);
    CHECK_FALSE(applied->artifacts.front().discovered);
    CHECK_FALSE(applied->artifacts.front().instantiated);
    for (std::size_t index = 1; index < applied->artifacts.size(); ++index) {
        CHECK(applied->artifacts[index].discovered);
        CHECK(applied->artifacts[index].instantiated);
    }

    const auto outcomes = check_outcomes(*applied);
    CHECK(outcomes.at(MaxValidationCheck::PackageDiscovery) == MaxValidationOutcome::Passed);
    CHECK(outcomes.at(MaxValidationCheck::DspSetup) == MaxValidationOutcome::Passed);
    CHECK(outcomes.at(MaxValidationCheck::PerformCallback) == MaxValidationOutcome::Passed);
    CHECK(outcomes.at(MaxValidationCheck::PublicClassSurface) == MaxValidationOutcome::Failed);
    CHECK(outcomes.at(MaxValidationCheck::SignalTopology) == MaxValidationOutcome::Failed);
    CHECK(outcomes.at(MaxValidationCheck::FiniteSignalOutput) == MaxValidationOutcome::Failed);
    CHECK(outcomes.at(MaxValidationCheck::ControlDispatch) == MaxValidationOutcome::Failed);
}

TEST_CASE("max-test application rejects manifest result digest host and assertion drift",
          "[max][max-test][application][adversarial]") {
    const auto manifest = harness_manifest();
    const auto run = passing_run(manifest);
    const auto observation = example_observation();

    auto result = apply_max_test_run(
        observation, manifest, std::string(64, 'a'), run, DATABASE_HASH, "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxTestApplicationErrorCode::InvalidManifest);

    auto changed_manifest = manifest;
    changed_manifest["harness"]["package_version"] = "99";
    result = apply_max_test_run(observation,
                                changed_manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                run,
                                DATABASE_HASH,
                                "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxTestApplicationErrorCode::InvalidManifest);

    result = apply_max_test_run(observation,
                                manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                run,
                                std::string(64, 'b'),
                                "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxTestApplicationErrorCode::DatabaseDigestMismatch);

    auto live_observation = observation;
    live_observation.environment.host_kind = MaxHostKind::MaxForLive;
    live_observation.environment.live_version = "12.3.1";
    live_observation.environment.max_for_live_version = "9.0.5";
    live_observation.checks.at(17).outcome = MaxValidationOutcome::NotRun;
    live_observation.checks.at(18).outcome = MaxValidationOutcome::NotApplicable;
    result = apply_max_test_run(live_observation,
                                manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                run,
                                DATABASE_HASH,
                                "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxTestApplicationErrorCode::IncompatibleHost);

    auto wrong_test = run;
    wrong_test.test_name = "another.maxtest";
    result = apply_max_test_run(observation,
                                manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                wrong_test,
                                DATABASE_HASH,
                                "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().subject == "test.name");

    auto missing_assertion = run;
    missing_assertion.assertions.pop_back();
    result = apply_max_test_run(observation,
                                manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                missing_assertion,
                                DATABASE_HASH,
                                "max-test/results.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().subject == "assertions");

    result = apply_max_test_run(observation,
                                manifest,
                                MAX_TEST_HARNESS_MANIFEST_SHA256,
                                run,
                                DATABASE_HASH,
                                "../escape.db3");
    REQUIRE_FALSE(result);
    CHECK(result.error().subject == "derived_observation");
}

TEST_CASE("compiled max-test manifest digest matches the exact authored bytes",
          "[max][max-test][manifest]") {
    const auto path = repository_root() / "max-package/misc/validation/max-test-harness.json";
    const auto digest = max_validation_regular_file_sha256(path, "max-test-harness.json");
    REQUIRE(digest);
    CHECK(*digest == MAX_TEST_HARNESS_MANIFEST_SHA256);
}
