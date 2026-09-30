/**
 * @file host_run_result.hpp
 * @brief Native evaluation of residual named-Max and Max-for-Live host measurements
 */

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/validation_record.hpp>

namespace sunny::infrastructure {

inline constexpr std::uint32_t MAX_HOST_RUN_RESULT_SCHEMA_VERSION = 1;

/** Replaced only when the exact schema-1 max-host-run-plan.json bytes intentionally change. */
inline constexpr std::string_view MAX_HOST_RUN_PLAN_SHA256 =
    "29b9772b4df459eb04228d841478956742ff59580a4f326d8d35f72e0a43e529";

enum class MaxHostRunApplicationErrorCode : std::uint8_t {
    InvalidPlan,
    InvalidResult,
    InvalidObservation,
    ProvenanceMismatch,
    EvidenceUnavailable,
    EvidenceDigestMismatch,
    MeasurementContractMismatch,
};

struct MaxHostRunApplicationError {
    MaxHostRunApplicationErrorCode code = MaxHostRunApplicationErrorCode::InvalidResult;
    std::string subject;
};

using MaxHostRunApplicationResult =
    std::expected<MaxValidationObservation, MaxHostRunApplicationError>;
using MaxHostRunVerificationResult = std::expected<void, MaxHostRunApplicationError>;

/**
 * Re-hash the closed residual file set and validate/evaluate its structured facts and float-WAVE
 * captures. A well-formed run may contain failed criteria; failure outcomes are evidence, not a
 * schema error.
 */
[[nodiscard]] MaxHostRunVerificationResult
verify_max_host_run_result(const nlohmann::json& host_plan,
                           std::string_view host_plan_sha256,
                           const nlohmann::json& indexed_result,
                           std::string_view indexed_result_sha256,
                           const std::filesystem::path& evidence_root,
                           std::string_view result_evidence_relative_path);

/**
 * Apply the four shared residual outcomes and, for M4L, the thirteen device assertion outcomes,
 * five artifact facts, and discontinuity outcome to a provenance-identical observation.
 */
[[nodiscard]] MaxHostRunApplicationResult
apply_max_host_run_result(const MaxValidationObservation& observation,
                          const nlohmann::json& host_plan,
                          std::string_view host_plan_sha256,
                          const nlohmann::json& indexed_result,
                          std::string_view indexed_result_sha256,
                          const std::filesystem::path& evidence_root,
                          std::string result_evidence_relative_path);

[[nodiscard]] const char*
max_host_run_application_error_name(MaxHostRunApplicationErrorCode code) noexcept;

} // namespace sunny::infrastructure
