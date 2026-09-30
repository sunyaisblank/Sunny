/**
 * @file max_test_result.hpp
 * @brief Closed normalized result and mapping for the pinned Cycling '74 max-test harness
 */

#pragma once

#include <cstdint>
#include <expected>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <sunny/core/types/music_types.hpp>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <vector>

namespace sunny::infrastructure {

inline constexpr std::uint32_t MAX_TEST_RESULT_SCHEMA_VERSION = 1;
inline constexpr std::string_view MAX_TEST_HARNESS_REVISION =
    "8c5d833d4b1e454238ced7c866c34acedb69cc07";

/** Replaced only when the exact schema-1 max-test-harness.json bytes intentionally change. */
inline constexpr std::string_view MAX_TEST_HARNESS_MANIFEST_SHA256 =
    "1d26434e9c3a00b5604070c9bd950755365c2a82a7f642fd205fb17d95db39bc";

enum class MaxTestAssertionOutcome : std::uint8_t { Pass, Fail };

/** One unchanged assertion row extracted from the official max-test SQLite database. */
struct MaxTestAssertionResult {
    std::uint64_t assertion_id = 0;
    std::string assertion_name;
    MaxTestAssertionOutcome outcome = MaxTestAssertionOutcome::Fail;
    std::string finished_at;
};

/**
 * Closed normalized projection of one completed max-test database row and its assertion rows.
 * The database digest binds the projection to the exact retained SQLite evidence file.
 */
struct MaxTestRunResult {
    std::uint32_t schema_version = MAX_TEST_RESULT_SCHEMA_VERSION;
    std::string database_sha256;
    std::uint64_t test_id = 0;
    std::string test_name;
    std::string started_at;
    std::string finished_at;
    std::vector<MaxTestAssertionResult> assertions;
};

/** Strict parser for the thin SQLite extractor's normalized schema-1 output. */
[[nodiscard]] sunny::core::Result<MaxTestRunResult>
max_test_run_result_from_json(const nlohmann::json& value);

/** Canonical JSON encoding of one normalized max-test run. */
[[nodiscard]] nlohmann::json max_test_run_result_to_json(const MaxTestRunResult& result);

enum class MaxTestApplicationErrorCode : std::uint8_t {
    InvalidManifest,
    InvalidResult,
    InvalidObservation,
    IncompatibleHost,
    DatabaseDigestMismatch,
    ContractMismatch,
};

struct MaxTestApplicationError {
    MaxTestApplicationErrorCode code = MaxTestApplicationErrorCode::InvalidResult;
    std::string subject;
};

using MaxTestApplicationResult = std::expected<MaxValidationObservation, MaxTestApplicationError>;

/**
 * Apply only the checks explicitly automated by the exact pinned harness manifest. The retained
 * SQLite database is the evidence file for every resulting passed/failed check. Unautomated facts
 * remain unchanged and therefore cannot be promoted by a green smoke run.
 */
[[nodiscard]] MaxTestApplicationResult
apply_max_test_run(const MaxValidationObservation& observation,
                   const nlohmann::json& harness_manifest,
                   std::string_view harness_manifest_sha256,
                   const MaxTestRunResult& run,
                   std::string_view observed_database_sha256,
                   std::string evidence_relative_path);

[[nodiscard]] const char*
max_test_application_error_name(MaxTestApplicationErrorCode code) noexcept;

} // namespace sunny::infrastructure
