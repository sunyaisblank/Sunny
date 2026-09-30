/**
 * @file release_matrix.hpp
 * @brief Closed release coverage over Sunny's supported Max host cells
 */

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <vector>

namespace sunny::infrastructure {

inline constexpr std::uint32_t MAX_RELEASE_MATRIX_SCHEMA_VERSION = 1;
inline constexpr std::uint32_t MAX_RELEASE_MATRIX_MANIFEST_SCHEMA_VERSION = 1;
inline constexpr std::string_view MAX_RELEASE_MATRIX_MANIFEST_SHA256 =
    "ea3acb87e102d4e6b3e206467a63add9ec955774ad1aa79d42dfb1aff497424f";

/** One required platform/architecture/host cell and its embedded, byte-addressed record. */
struct MaxReleaseMatrixCell {
    MaxOperatingSystem operating_system = MaxOperatingSystem::MacOS;
    MaxArchitecture architecture = MaxArchitecture::X86_64;
    MaxHostKind host_kind = MaxHostKind::StandaloneMax;
    std::string record_relative_path;
    std::string record_sha256;
    MaxValidationRecord record;
};

/**
 * Self-contained release coverage for the exact six supported host cells. Completeness covers only
 * each embedded record's explicit environment and does not re-verify its underlying evidence.
 */
struct MaxReleaseMatrix {
    std::uint32_t schema_version = MAX_RELEASE_MATRIX_SCHEMA_VERSION;
    std::string manifest_sha256;
    std::string sunny_version;
    std::string package_version;
    std::string source_revision;
    std::vector<MaxReleaseMatrixCell> cells;

    [[nodiscard]] bool complete() const;
};

enum class MaxReleaseMatrixErrorCode : std::uint8_t {
    InvalidManifest,
    ManifestDigestMismatch,
    InputRootUnavailable,
    FileUnavailable,
    NonRegularFile,
    IoFailure,
    InvalidRecord,
    RecordDigestMismatch,
    CellMismatch,
    IncompleteRecord,
    RevisionMismatch,
    PackageMismatch,
    InvalidMatrix,
};

struct MaxReleaseMatrixError {
    MaxReleaseMatrixErrorCode code = MaxReleaseMatrixErrorCode::InvalidManifest;
    std::string subject;
};

using MaxReleaseMatrixResult = std::expected<MaxReleaseMatrix, MaxReleaseMatrixError>;
using MaxReleaseMatrixVerificationResult = std::expected<void, MaxReleaseMatrixError>;

/** Strict parser for a self-contained, complete schema-1 release matrix. */
[[nodiscard]] MaxReleaseMatrixResult max_release_matrix_from_json(const nlohmann::json& value);

/** Canonical JSON encoding with a checked derived `complete` member. */
[[nodiscard]] nlohmann::json max_release_matrix_to_json(const MaxReleaseMatrix& matrix);

/**
 * Read the six deterministic record paths below `record_root`, bind their exact file bytes, and
 * require every record plus all cross-record release invariants to hold.
 */
[[nodiscard]] MaxReleaseMatrixResult
materialize_max_release_matrix(const nlohmann::json& manifest,
                               std::string_view manifest_sha256,
                               const std::filesystem::path& record_root);

/** Re-hash and re-parse the manifest and all six record files bound by an existing matrix. */
[[nodiscard]] MaxReleaseMatrixVerificationResult
verify_max_release_matrix_files(const MaxReleaseMatrix& matrix,
                                const nlohmann::json& manifest,
                                std::string_view manifest_sha256,
                                const std::filesystem::path& record_root);

[[nodiscard]] const char* max_release_matrix_error_name(MaxReleaseMatrixErrorCode code) noexcept;

} // namespace sunny::infrastructure
