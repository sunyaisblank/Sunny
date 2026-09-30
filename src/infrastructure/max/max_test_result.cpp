/**
 * @file max_test_result.cpp
 * @brief C++-authoritative application of pinned max-test SQLite observations
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/max_test_result.hpp>
#include <utility>

namespace sunny::infrastructure {

namespace {

using json = nlohmann::json;
using sunny::core::ErrorCode;

constexpr std::array<std::string_view, 5> OBJECT_NAMES{
    "sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"};

constexpr std::array<MaxValidationCheck, 19> VALIDATION_CHECKS{
    MaxValidationCheck::PackageDiscovery,
    MaxValidationCheck::PublicClassSurface,
    MaxValidationCheck::SignalTopology,
    MaxValidationCheck::DspSetup,
    MaxValidationCheck::PerformCallback,
    MaxValidationCheck::FiniteSignalOutput,
    MaxValidationCheck::DisconnectedProcessing,
    MaxValidationCheck::ReconnectContinuity,
    MaxValidationCheck::ControlDispatch,
    MaxValidationCheck::ActiveDspTeardown,
    MaxValidationCheck::ItmScheduleFire,
    MaxValidationCheck::ItmEqualTickOrdering,
    MaxValidationCheck::ItmClearReassign,
    MaxValidationCheck::ReleaseVelocityFormatting,
    MaxValidationCheck::SchedulerTiming,
    MaxValidationCheck::DownstreamMidiDelivery,
    MaxValidationCheck::RenderedAudio,
    MaxValidationCheck::LiveTransportDiscontinuities,
    MaxValidationCheck::StandaloneTransportIdentity,
};

constexpr std::array<std::string_view, 5> PATCHERS{
    "patchers/sunny-runtime-smoke.maxtest.maxpat",
    "patchers/sunny.assert-host-status.maxpat",
    "patchers/sunny.assert-event-status.maxpat",
    "patchers/sunny.assert-event-list.maxpat",
    "patchers/sunny.assert-host-lifecycle.maxpat",
};

enum class Automation : std::uint8_t { MaxTest, NotAutomated, NotApplicable };

struct AssertionMapping {
    std::string subject;
    std::vector<std::string> required_assertions;
};

struct CheckMapping {
    MaxValidationCheck check = MaxValidationCheck::PackageDiscovery;
    Automation automation = Automation::NotAutomated;
    std::vector<std::string> required_assertions;
};

struct HarnessContract {
    std::string revision;
    std::string database_test_name;
    std::vector<AssertionMapping> artifact_mappings;
    std::vector<CheckMapping> check_mappings;
    std::set<std::string> expected_assertions;
};

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool valid_text(std::string_view value, std::size_t maximum) {
    if (value.empty() || value.size() > maximum) return false;
    return std::none_of(value.begin(), value.end(), [](const unsigned char character) {
        return character < 0x20 || character == 0x7f;
    });
}

bool lowercase_hex(std::string_view value, std::size_t length) {
    if (value.size() != length ||
        !std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        }))
        return false;
    return std::any_of(
        value.begin(), value.end(), [](const char character) { return character != '0'; });
}

bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

unsigned decimal_pair(std::string_view value, std::size_t offset) {
    return static_cast<unsigned>(value[offset] - '0') * 10U +
           static_cast<unsigned>(value[offset + 1] - '0');
}

unsigned decimal_year(std::string_view value) {
    unsigned result = 0;
    for (std::size_t index = 0; index < 4; ++index)
        result = result * 10U + static_cast<unsigned>(value[index] - '0');
    return result;
}

bool valid_local_timestamp(std::string_view value) {
    if (value.size() != 19 || value[4] != '-' || value[7] != '-' || value[10] != ' ' ||
        value[13] != ':' || value[16] != ':')
        return false;
    constexpr std::array<std::size_t, 14> digits{0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    if (!std::all_of(digits.begin(), digits.end(), [&](const auto offset) {
            return ascii_digit(value[offset]);
        }))
        return false;
    const auto year = decimal_year(value);
    const auto month = decimal_pair(value, 5);
    const auto day = decimal_pair(value, 8);
    const auto hour = decimal_pair(value, 11);
    const auto minute = decimal_pair(value, 14);
    const auto second = decimal_pair(value, 17);
    if (year < 1970 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 60)
        return false;
    constexpr std::array<unsigned, 12> month_days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    auto maximum_day = month_days[month - 1];
    const bool leap_year = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    if (month == 2 && leap_year) maximum_day = 29;
    return day >= 1 && day <= maximum_day;
}

const char* assertion_outcome_name(MaxTestAssertionOutcome outcome) {
    switch (outcome) {
    case MaxTestAssertionOutcome::Pass:
        return "Pass";
    case MaxTestAssertionOutcome::Fail:
        return "Fail";
    }
    return "";
}

std::optional<MaxTestAssertionOutcome> assertion_outcome_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& name = value.get_ref<const std::string&>();
    if (name == "Pass") return MaxTestAssertionOutcome::Pass;
    if (name == "Fail") return MaxTestAssertionOutcome::Fail;
    return std::nullopt;
}

bool result_shape_valid(const MaxTestRunResult& result) {
    if (result.schema_version != MAX_TEST_RESULT_SCHEMA_VERSION ||
        !lowercase_hex(result.database_sha256, 64) || result.test_id == 0 ||
        !valid_text(result.test_name, 512) || !valid_local_timestamp(result.started_at) ||
        !valid_local_timestamp(result.finished_at) || result.finished_at < result.started_at ||
        result.assertions.empty() || result.assertions.size() > 4096)
        return false;

    std::uint64_t preceding_id = 0;
    std::set<std::string> names;
    for (const auto& assertion : result.assertions) {
        if (assertion.assertion_id <= preceding_id || !valid_text(assertion.assertion_name, 512) ||
            std::string_view(assertion_outcome_name(assertion.outcome)).empty() ||
            !valid_local_timestamp(assertion.finished_at) ||
            assertion.finished_at < result.started_at ||
            assertion.finished_at > result.finished_at ||
            !names.emplace(assertion.assertion_name).second)
            return false;
        preceding_id = assertion.assertion_id;
    }
    return true;
}

std::optional<std::vector<std::string>> assertion_names(const json& value) {
    if (!value.is_array()) return std::nullopt;
    std::vector<std::string> names;
    std::set<std::string> unique;
    for (const auto& encoded : value) {
        if (!encoded.is_string()) return std::nullopt;
        auto name = encoded.get<std::string>();
        if (!valid_text(name, 512) || !unique.emplace(name).second) return std::nullopt;
        names.push_back(std::move(name));
    }
    return names;
}

std::optional<Automation> automation_from_name(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& name = value.get_ref<const std::string&>();
    if (name == "max_test") return Automation::MaxTest;
    if (name == "not_automated") return Automation::NotAutomated;
    if (name == "not_applicable") return Automation::NotApplicable;
    return std::nullopt;
}

Automation expected_automation(MaxValidationCheck check) {
    switch (check) {
    case MaxValidationCheck::PackageDiscovery:
    case MaxValidationCheck::PublicClassSurface:
    case MaxValidationCheck::SignalTopology:
    case MaxValidationCheck::DspSetup:
    case MaxValidationCheck::PerformCallback:
    case MaxValidationCheck::FiniteSignalOutput:
    case MaxValidationCheck::DisconnectedProcessing:
    case MaxValidationCheck::ReconnectContinuity:
    case MaxValidationCheck::ControlDispatch:
    case MaxValidationCheck::ItmScheduleFire:
    case MaxValidationCheck::ItmEqualTickOrdering:
    case MaxValidationCheck::ItmClearReassign:
    case MaxValidationCheck::ReleaseVelocityFormatting:
    case MaxValidationCheck::StandaloneTransportIdentity:
        return Automation::MaxTest;
    case MaxValidationCheck::LiveTransportDiscontinuities:
        return Automation::NotApplicable;
    default:
        return Automation::NotAutomated;
    }
}

std::optional<HarnessContract> harness_contract_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "harness",
                           "patchers",
                           "test_runs",
                           "artifact_assertions",
                           "check_assertions"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            value.at("schema_version").get<std::uint64_t>() != 1)
            return std::nullopt;

        const auto& harness = value.at("harness");
        if (!exact_fields(harness,
                          {"name",
                           "repository",
                           "revision",
                           "package_version",
                           "minimum_max_version",
                           "host_kind",
                           "result_database"}) ||
            harness.at("name") != "cycling74-max-test" ||
            harness.at("repository") != "https://github.com/Cycling74/max-test.git" ||
            harness.at("revision") != MAX_TEST_HARNESS_REVISION ||
            harness.at("package_version") != "1.2.1" ||
            harness.at("minimum_max_version") != "8.2" ||
            harness.at("host_kind") != "standalone_max")
            return std::nullopt;

        const auto& database = harness.at("result_database");
        if (!exact_fields(database,
                          {"format",
                           "test_table",
                           "test_id_column",
                           "test_name_column",
                           "test_start_column",
                           "test_finish_column",
                           "assertion_table",
                           "assertion_id_column",
                           "assertion_test_id_column",
                           "assertion_name_column",
                           "assertion_outcome_column",
                           "assertion_finish_column",
                           "admitted_outcomes"}) ||
            database.at("format") != "sqlite3" || database.at("test_table") != "tests" ||
            database.at("test_id_column") != "test_id" ||
            database.at("test_name_column") != "test_name" ||
            database.at("test_start_column") != "test_start" ||
            database.at("test_finish_column") != "test_finish" ||
            database.at("assertion_table") != "assertions" ||
            database.at("assertion_id_column") != "assertion_id" ||
            database.at("assertion_test_id_column") != "test_id_ext" ||
            database.at("assertion_name_column") != "assertion_name" ||
            database.at("assertion_outcome_column") != "assertion_value" ||
            database.at("assertion_finish_column") != "assertion_finish" ||
            database.at("admitted_outcomes") != json::array({"Pass", "Fail"}))
            return std::nullopt;

        if (!value.at("patchers").is_array() || value.at("patchers").size() != PATCHERS.size())
            return std::nullopt;
        for (std::size_t index = 0; index < PATCHERS.size(); ++index) {
            if (value.at("patchers").at(index) != PATCHERS[index]) return std::nullopt;
        }

        const auto& test_runs = value.at("test_runs");
        if (!test_runs.is_array() || test_runs.size() != 1 ||
            !exact_fields(
                test_runs.front(),
                {"database_test_name", "patcher", "self_start_object", "terminator_object"}) ||
            test_runs.front().at("database_test_name") != "sunny-runtime-smoke.maxtest" ||
            test_runs.front().at("patcher") != PATCHERS.front() ||
            test_runs.front().at("self_start_object") != "loadbang" ||
            test_runs.front().at("terminator_object") != "test.terminate")
            return std::nullopt;

        HarnessContract contract;
        contract.revision = harness.at("revision").get<std::string>();
        contract.database_test_name = test_runs.front().at("database_test_name").get<std::string>();

        const auto& artifacts = value.at("artifact_assertions");
        if (!artifacts.is_array() || artifacts.size() != OBJECT_NAMES.size()) return std::nullopt;
        for (std::size_t index = 0; index < OBJECT_NAMES.size(); ++index) {
            const auto& mapping = artifacts.at(index);
            if (!exact_fields(mapping, {"object_name", "required_assertions"}) ||
                mapping.at("object_name") != OBJECT_NAMES[index])
                return std::nullopt;
            auto names = assertion_names(mapping.at("required_assertions"));
            if (!names || names->empty()) return std::nullopt;
            contract.expected_assertions.insert(names->begin(), names->end());
            contract.artifact_mappings.push_back({.subject = std::string{OBJECT_NAMES[index]},
                                                  .required_assertions = std::move(*names)});
        }

        const auto& checks = value.at("check_assertions");
        if (!checks.is_array() || checks.size() != VALIDATION_CHECKS.size()) return std::nullopt;
        for (std::size_t index = 0; index < VALIDATION_CHECKS.size(); ++index) {
            const auto& mapping = checks.at(index);
            if (!exact_fields(mapping, {"check", "automation", "required_assertions"}) ||
                !mapping.at("check").is_string() ||
                mapping.at("check").get_ref<const std::string&>() !=
                    max_validation_check_name(VALIDATION_CHECKS[index]))
                return std::nullopt;
            auto automation = automation_from_name(mapping.at("automation"));
            auto names = assertion_names(mapping.at("required_assertions"));
            if (!automation || *automation != expected_automation(VALIDATION_CHECKS[index]) ||
                !names || ((*automation == Automation::MaxTest) != !names->empty()))
                return std::nullopt;
            if (*automation == Automation::MaxTest)
                contract.expected_assertions.insert(names->begin(), names->end());
            contract.check_mappings.push_back({.check = VALIDATION_CHECKS[index],
                                               .automation = *automation,
                                               .required_assertions = std::move(*names)});
        }
        if (contract.expected_assertions.size() != 64) return std::nullopt;
        return contract;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

MaxTestApplicationError failure(MaxTestApplicationErrorCode code, std::string subject) {
    return {.code = code, .subject = std::move(subject)};
}

bool mapping_passed(const AssertionMapping& mapping,
                    const std::map<std::string, MaxTestAssertionOutcome>& assertions) {
    return std::all_of(mapping.required_assertions.begin(),
                       mapping.required_assertions.end(),
                       [&](const auto& name) {
                           const auto found = assertions.find(name);
                           return found != assertions.end() &&
                                  found->second == MaxTestAssertionOutcome::Pass;
                       });
}

std::pair<bool, std::vector<std::string>>
check_result(const CheckMapping& mapping,
             const std::map<std::string, MaxTestAssertionOutcome>& assertions) {
    std::vector<std::string> failed;
    for (const auto& name : mapping.required_assertions) {
        const auto found = assertions.find(name);
        if (found == assertions.end() || found->second != MaxTestAssertionOutcome::Pass)
            failed.push_back(name);
    }
    return {failed.empty(), std::move(failed)};
}

std::string check_summary(const HarnessContract& contract,
                          const MaxTestRunResult& run,
                          const CheckMapping& mapping,
                          const std::vector<std::string>& failed) {
    std::string summary = "cycling74-max-test@" + contract.revision +
                          " test_id=" + std::to_string(run.test_id) + ": ";
    if (failed.empty()) {
        summary += std::to_string(mapping.required_assertions.size()) + "/" +
                   std::to_string(mapping.required_assertions.size()) +
                   " required assertions passed.";
        return summary;
    }
    summary += "failed assertions: ";
    for (std::size_t index = 0; index < failed.size(); ++index) {
        if (index != 0) summary += ", ";
        summary += failed[index];
    }
    summary += '.';
    return summary;
}

} // namespace

sunny::core::Result<MaxTestRunResult> max_test_run_result_from_json(const json& value) {
    try {
        if (!exact_fields(value, {"schema_version", "database_sha256", "test", "assertions"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            !value.at("database_sha256").is_string() || !value.at("assertions").is_array())
            return std::unexpected(ErrorCode::FormatError);
        const auto& test = value.at("test");
        if (!exact_fields(test, {"id", "name", "started_at", "finished_at"}) ||
            !test.at("id").is_number_unsigned() || !test.at("name").is_string() ||
            !test.at("started_at").is_string() || !test.at("finished_at").is_string())
            return std::unexpected(ErrorCode::FormatError);

        const auto schema = value.at("schema_version").get<std::uint64_t>();
        if (schema > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::FormatError);
        MaxTestRunResult result;
        result.schema_version = static_cast<std::uint32_t>(schema);
        result.database_sha256 = value.at("database_sha256").get<std::string>();
        result.test_id = test.at("id").get<std::uint64_t>();
        result.test_name = test.at("name").get<std::string>();
        result.started_at = test.at("started_at").get<std::string>();
        result.finished_at = test.at("finished_at").get<std::string>();
        for (const auto& encoded : value.at("assertions")) {
            if (!exact_fields(encoded, {"id", "name", "outcome", "finished_at"}) ||
                !encoded.at("id").is_number_unsigned() || !encoded.at("name").is_string() ||
                !encoded.at("finished_at").is_string())
                return std::unexpected(ErrorCode::FormatError);
            auto outcome = assertion_outcome_from_json(encoded.at("outcome"));
            if (!outcome) return std::unexpected(ErrorCode::FormatError);
            result.assertions.push_back(
                {.assertion_id = encoded.at("id").get<std::uint64_t>(),
                 .assertion_name = encoded.at("name").get<std::string>(),
                 .outcome = *outcome,
                 .finished_at = encoded.at("finished_at").get<std::string>()});
        }
        if (!result_shape_valid(result)) return std::unexpected(ErrorCode::FormatError);
        return result;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

json max_test_run_result_to_json(const MaxTestRunResult& result) {
    json assertions = json::array();
    for (const auto& assertion : result.assertions) {
        assertions.push_back({{"id", assertion.assertion_id},
                              {"name", assertion.assertion_name},
                              {"outcome", assertion_outcome_name(assertion.outcome)},
                              {"finished_at", assertion.finished_at}});
    }
    return {{"schema_version", result.schema_version},
            {"database_sha256", result.database_sha256},
            {"test",
             {{"id", result.test_id},
              {"name", result.test_name},
              {"started_at", result.started_at},
              {"finished_at", result.finished_at}}},
            {"assertions", std::move(assertions)}};
}

MaxTestApplicationResult apply_max_test_run(const MaxValidationObservation& supplied_observation,
                                            const json& harness_manifest,
                                            std::string_view harness_manifest_sha256,
                                            const MaxTestRunResult& supplied_run,
                                            std::string_view observed_database_sha256,
                                            std::string evidence_relative_path) {
    if (harness_manifest_sha256 != MAX_TEST_HARNESS_MANIFEST_SHA256)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::InvalidManifest, "manifest_sha256"));
    auto contract = harness_contract_from_json(harness_manifest);
    if (!contract)
        return std::unexpected(failure(MaxTestApplicationErrorCode::InvalidManifest, "manifest"));

    auto run = max_test_run_result_from_json(max_test_run_result_to_json(supplied_run));
    if (!run) return std::unexpected(failure(MaxTestApplicationErrorCode::InvalidResult, "result"));
    auto observation = max_validation_observation_from_json(
        max_validation_observation_to_json(supplied_observation));
    if (!observation)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::InvalidObservation, "observation"));
    if (observation->environment.host_kind != MaxHostKind::StandaloneMax)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::IncompatibleHost, "environment.host_kind"));
    if (run->database_sha256 != observed_database_sha256)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::DatabaseDigestMismatch, "database_sha256"));
    if (run->test_name != contract->database_test_name)
        return std::unexpected(failure(MaxTestApplicationErrorCode::ContractMismatch, "test.name"));

    std::map<std::string, MaxTestAssertionOutcome> assertions;
    std::set<std::string> observed_names;
    for (const auto& assertion : run->assertions) {
        assertions.emplace(assertion.assertion_name, assertion.outcome);
        observed_names.emplace(assertion.assertion_name);
    }
    if (observed_names != contract->expected_assertions)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::ContractMismatch, "assertions"));

    if (observation->artifacts.size() != contract->artifact_mappings.size() ||
        observation->checks.size() != contract->check_mappings.size())
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::InvalidObservation, "closed_sets"));

    for (std::size_t index = 0; index < contract->artifact_mappings.size(); ++index) {
        const auto& mapping = contract->artifact_mappings[index];
        if (observation->artifacts[index].object_name != mapping.subject)
            return std::unexpected(
                failure(MaxTestApplicationErrorCode::ContractMismatch, "artifact_order"));
        const bool passed = mapping_passed(mapping, assertions);
        observation->artifacts[index].discovered = passed;
        observation->artifacts[index].instantiated = passed;
    }

    for (std::size_t index = 0; index < contract->check_mappings.size(); ++index) {
        const auto& mapping = contract->check_mappings[index];
        auto& fact = observation->checks[index];
        if (fact.check != mapping.check)
            return std::unexpected(
                failure(MaxTestApplicationErrorCode::ContractMismatch, "check_order"));
        if (mapping.automation != Automation::MaxTest) continue;
        const auto [passed, failed] = check_result(mapping, assertions);
        fact.outcome = passed ? MaxValidationOutcome::Passed : MaxValidationOutcome::Failed;
        fact.summary = check_summary(*contract, *run, mapping, failed);
        fact.evidence_relative_path = evidence_relative_path;
    }

    auto validated =
        max_validation_observation_from_json(max_validation_observation_to_json(*observation));
    if (!validated)
        return std::unexpected(
            failure(MaxTestApplicationErrorCode::ContractMismatch, "derived_observation"));
    return std::move(*validated);
}

const char* max_test_application_error_name(MaxTestApplicationErrorCode code) noexcept {
    switch (code) {
    case MaxTestApplicationErrorCode::InvalidManifest:
        return "invalid_manifest";
    case MaxTestApplicationErrorCode::InvalidResult:
        return "invalid_result";
    case MaxTestApplicationErrorCode::InvalidObservation:
        return "invalid_observation";
    case MaxTestApplicationErrorCode::IncompatibleHost:
        return "incompatible_host";
    case MaxTestApplicationErrorCode::DatabaseDigestMismatch:
        return "database_digest_mismatch";
    case MaxTestApplicationErrorCode::ContractMismatch:
        return "contract_mismatch";
    }
    return "unknown";
}

} // namespace sunny::infrastructure
