/**
 * @file release_matrix.cpp
 * @brief Strict six-cell Max/Max for Live release-coverage implementation
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <nlohmann/json.hpp>
#include <optional>
#include <string_view>
#include <sunny/infrastructure/max/release_matrix.hpp>
#include <sunny/version.hpp>
#include <utility>

namespace sunny::infrastructure {

namespace {

using json = nlohmann::json;
namespace fs = std::filesystem;

struct RequiredCell {
    MaxOperatingSystem operating_system;
    MaxArchitecture architecture;
    MaxHostKind host_kind;
    std::string_view record_relative_path;
};

constexpr std::array<RequiredCell, 6> REQUIRED_CELLS{{
    {MaxOperatingSystem::MacOS,
     MaxArchitecture::X86_64,
     MaxHostKind::StandaloneMax,
     "records/macos-x86_64-standalone-max.json"},
    {MaxOperatingSystem::MacOS,
     MaxArchitecture::X86_64,
     MaxHostKind::MaxForLive,
     "records/macos-x86_64-max-for-live.json"},
    {MaxOperatingSystem::MacOS,
     MaxArchitecture::Arm64,
     MaxHostKind::StandaloneMax,
     "records/macos-arm64-standalone-max.json"},
    {MaxOperatingSystem::MacOS,
     MaxArchitecture::Arm64,
     MaxHostKind::MaxForLive,
     "records/macos-arm64-max-for-live.json"},
    {MaxOperatingSystem::Windows,
     MaxArchitecture::X86_64,
     MaxHostKind::StandaloneMax,
     "records/windows-x86_64-standalone-max.json"},
    {MaxOperatingSystem::Windows,
     MaxArchitecture::X86_64,
     MaxHostKind::MaxForLive,
     "records/windows-x86_64-max-for-live.json"},
}};

using Error = MaxReleaseMatrixError;
using ErrorCode = MaxReleaseMatrixErrorCode;

Error failure(ErrorCode code, std::string subject) {
    return {.code = code, .subject = std::move(subject)};
}

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool lowercase_sha256(const std::string& value) {
    return value.size() == 64 &&
           std::all_of(value.begin(),
                       value.end(),
                       [](const unsigned char character) {
                           return (character >= '0' && character <= '9') ||
                                  (character >= 'a' && character <= 'f');
                       }) &&
           std::any_of(
               value.begin(), value.end(), [](const char character) { return character != '0'; });
}

bool cell_identity_matches(const MaxReleaseMatrixCell& cell, const RequiredCell& required) {
    return cell.operating_system == required.operating_system &&
           cell.architecture == required.architecture && cell.host_kind == required.host_kind &&
           cell.record_relative_path == required.record_relative_path &&
           cell.record.environment.operating_system == required.operating_system &&
           cell.record.environment.architecture == required.architecture &&
           cell.record.environment.host_kind == required.host_kind;
}

bool same_package_bytes(const MaxReleaseMatrixCell& left, const MaxReleaseMatrixCell& right) {
    if (left.record.package_archive_sha256 != right.record.package_archive_sha256 ||
        left.record.artifacts.size() != right.record.artifacts.size())
        return false;
    for (std::size_t index = 0; index < left.record.artifacts.size(); ++index) {
        const auto& left_artifact = left.record.artifacts[index];
        const auto& right_artifact = right.record.artifacts[index];
        if (left_artifact.object_name != right_artifact.object_name ||
            left_artifact.package_relative_path != right_artifact.package_relative_path ||
            left_artifact.binary_sha256 != right_artifact.binary_sha256)
            return false;
    }
    return true;
}

std::optional<Error> validate_matrix(const MaxReleaseMatrix& matrix) {
    if (matrix.schema_version != MAX_RELEASE_MATRIX_SCHEMA_VERSION ||
        matrix.manifest_sha256 != MAX_RELEASE_MATRIX_MANIFEST_SHA256 ||
        matrix.sunny_version != sunny::SUNNY_VERSION ||
        matrix.package_version != sunny::SUNNY_VERSION ||
        matrix.cells.size() != REQUIRED_CELLS.size())
        return failure(ErrorCode::InvalidMatrix, "matrix_shape");

    for (std::size_t index = 0; index < matrix.cells.size(); ++index) {
        const auto& cell = matrix.cells[index];
        if (!lowercase_sha256(cell.record_sha256))
            return failure(ErrorCode::InvalidMatrix, cell.record_relative_path);
        const auto parsed =
            max_validation_record_from_json(max_validation_record_to_json(cell.record));
        if (!parsed) return failure(ErrorCode::InvalidRecord, cell.record_relative_path);
        if (!cell_identity_matches(cell, REQUIRED_CELLS[index]))
            return failure(ErrorCode::CellMismatch, cell.record_relative_path);
        if (!cell.record.complete())
            return failure(ErrorCode::IncompleteRecord, cell.record_relative_path);
        if (cell.record.sunny_version != matrix.sunny_version ||
            cell.record.package_version != matrix.package_version)
            return failure(ErrorCode::PackageMismatch, cell.record_relative_path);
        if (cell.record.source_revision != matrix.source_revision)
            return failure(ErrorCode::RevisionMismatch, cell.record_relative_path);
    }
    for (std::size_t index = 0; index < matrix.cells.size(); index += 2) {
        if (!same_package_bytes(matrix.cells[index], matrix.cells[index + 1]))
            return failure(ErrorCode::PackageMismatch,
                           std::string{REQUIRED_CELLS[index].record_relative_path});
    }
    return std::nullopt;
}

std::optional<Error> validate_manifest(const json& manifest, std::string_view digest) {
    if (digest != MAX_RELEASE_MATRIX_MANIFEST_SHA256)
        return failure(ErrorCode::ManifestDigestMismatch, "release_matrix_manifest");
    if (!exact_fields(manifest,
                      {"schema_version", "matrix_name", "sunny_version", "coverage", "cells"}) ||
        !manifest.at("schema_version").is_number_unsigned() ||
        manifest.at("schema_version") != MAX_RELEASE_MATRIX_MANIFEST_SCHEMA_VERSION ||
        manifest.at("matrix_name") != "sunny-max-supported-hosts" ||
        manifest.at("sunny_version") != sunny::SUNNY_VERSION || !manifest.at("cells").is_array() ||
        manifest.at("cells").size() != REQUIRED_CELLS.size())
        return failure(ErrorCode::InvalidManifest, "release_matrix_manifest");

    const auto& coverage = manifest.at("coverage");
    if (!exact_fields(coverage, {"unit", "configuration_scope", "underlying_evidence"}) ||
        coverage.at("unit") !=
            "one_complete_record_per_supported_platform_architecture_and_host_kind" ||
        coverage.at("configuration_scope") != "recorded_environment_only" ||
        coverage.at("underlying_evidence") != "verified_separately_per_record")
        return failure(ErrorCode::InvalidManifest, "coverage");

    for (std::size_t index = 0; index < REQUIRED_CELLS.size(); ++index) {
        const auto& encoded = manifest.at("cells").at(index);
        const auto& required = REQUIRED_CELLS[index];
        if (!exact_fields(
                encoded,
                {"operating_system", "architecture", "host_kind", "record_relative_path"}) ||
            encoded.at("operating_system") !=
                max_operating_system_name(required.operating_system) ||
            encoded.at("architecture") != max_architecture_name(required.architecture) ||
            encoded.at("host_kind") != max_host_kind_name(required.host_kind) ||
            encoded.at("record_relative_path") != required.record_relative_path)
            return failure(ErrorCode::InvalidManifest, "cells[" + std::to_string(index) + "]");
    }
    return std::nullopt;
}

Error map_file_error(const MaxValidationMaterializationError& error) {
    switch (error.code) {
    case MaxValidationMaterializationErrorCode::InputRootUnavailable:
        return failure(ErrorCode::InputRootUnavailable, error.subject);
    case MaxValidationMaterializationErrorCode::FileUnavailable:
        return failure(ErrorCode::FileUnavailable, error.subject);
    case MaxValidationMaterializationErrorCode::NonRegularFile:
        return failure(ErrorCode::NonRegularFile, error.subject);
    case MaxValidationMaterializationErrorCode::IoFailure:
        return failure(ErrorCode::IoFailure, error.subject);
    case MaxValidationMaterializationErrorCode::DigestMismatch:
        return failure(ErrorCode::RecordDigestMismatch, error.subject);
    case MaxValidationMaterializationErrorCode::InvalidObservation:
    case MaxValidationMaterializationErrorCode::InvalidRecord:
        return failure(ErrorCode::InvalidRecord, error.subject);
    }
    return failure(ErrorCode::IoFailure, error.subject);
}

std::expected<json, Error> read_record_json(const fs::path& record_root,
                                            const std::string& relative_path) {
    std::ifstream input{record_root / fs::path{relative_path}, std::ios::binary};
    if (!input) return std::unexpected(failure(ErrorCode::FileUnavailable, relative_path));
    try {
        auto value = json::parse(input);
        if (input.bad()) return std::unexpected(failure(ErrorCode::IoFailure, relative_path));
        return value;
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::InvalidRecord, relative_path));
    }
}

std::expected<std::pair<std::string, MaxValidationRecord>, Error>
read_bound_record(const fs::path& record_root, const std::string& relative_path) {
    auto digest_before = max_validation_evidence_file_sha256(record_root, relative_path);
    if (!digest_before) return std::unexpected(map_file_error(digest_before.error()));
    auto encoded = read_record_json(record_root, relative_path);
    if (!encoded) return std::unexpected(encoded.error());
    auto digest_after = max_validation_evidence_file_sha256(record_root, relative_path);
    if (!digest_after) return std::unexpected(map_file_error(digest_after.error()));
    if (*digest_before != *digest_after)
        return std::unexpected(failure(ErrorCode::IoFailure, relative_path));
    auto record = max_validation_record_from_json(*encoded);
    if (!record) return std::unexpected(failure(ErrorCode::InvalidRecord, relative_path));
    return std::pair{std::move(*digest_before), std::move(*record)};
}

std::optional<MaxOperatingSystem> operating_system_from_text(const json& value) {
    if (value == "macos") return MaxOperatingSystem::MacOS;
    if (value == "windows") return MaxOperatingSystem::Windows;
    return std::nullopt;
}

std::optional<MaxArchitecture> architecture_from_text(const json& value) {
    if (value == "x86_64") return MaxArchitecture::X86_64;
    if (value == "arm64") return MaxArchitecture::Arm64;
    return std::nullopt;
}

std::optional<MaxHostKind> host_kind_from_text(const json& value) {
    if (value == "standalone_max") return MaxHostKind::StandaloneMax;
    if (value == "max_for_live") return MaxHostKind::MaxForLive;
    return std::nullopt;
}

} // namespace

bool MaxReleaseMatrix::complete() const {
    return !validate_matrix(*this).has_value();
}

json max_release_matrix_to_json(const MaxReleaseMatrix& matrix) {
    json cells = json::array();
    for (const auto& cell : matrix.cells) {
        cells.push_back({{"operating_system", max_operating_system_name(cell.operating_system)},
                         {"architecture", max_architecture_name(cell.architecture)},
                         {"host_kind", max_host_kind_name(cell.host_kind)},
                         {"record_relative_path", cell.record_relative_path},
                         {"record_sha256", cell.record_sha256},
                         {"record", max_validation_record_to_json(cell.record)}});
    }
    return {{"schema_version", matrix.schema_version},
            {"manifest_sha256", matrix.manifest_sha256},
            {"sunny_version", matrix.sunny_version},
            {"package_version", matrix.package_version},
            {"source_revision", matrix.source_revision},
            {"cells", std::move(cells)},
            {"complete", matrix.complete()}};
}

MaxReleaseMatrixResult max_release_matrix_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "manifest_sha256",
                           "sunny_version",
                           "package_version",
                           "source_revision",
                           "cells",
                           "complete"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            !value.at("manifest_sha256").is_string() || !value.at("sunny_version").is_string() ||
            !value.at("package_version").is_string() || !value.at("source_revision").is_string() ||
            !value.at("cells").is_array() || !value.at("complete").is_boolean())
            return std::unexpected(failure(ErrorCode::InvalidMatrix, "matrix_shape"));

        MaxReleaseMatrix matrix;
        matrix.schema_version = value.at("schema_version").get<std::uint32_t>();
        matrix.manifest_sha256 = value.at("manifest_sha256").get<std::string>();
        matrix.sunny_version = value.at("sunny_version").get<std::string>();
        matrix.package_version = value.at("package_version").get<std::string>();
        matrix.source_revision = value.at("source_revision").get<std::string>();
        for (std::size_t index = 0; index < value.at("cells").size(); ++index) {
            const auto& encoded = value.at("cells").at(index);
            if (!exact_fields(encoded,
                              {"operating_system",
                               "architecture",
                               "host_kind",
                               "record_relative_path",
                               "record_sha256",
                               "record"}) ||
                !encoded.at("operating_system").is_string() ||
                !encoded.at("architecture").is_string() || !encoded.at("host_kind").is_string() ||
                !encoded.at("record_relative_path").is_string() ||
                !encoded.at("record_sha256").is_string())
                return std::unexpected(
                    failure(ErrorCode::InvalidMatrix, "cells[" + std::to_string(index) + "]"));
            auto operating_system = operating_system_from_text(encoded.at("operating_system"));
            auto architecture = architecture_from_text(encoded.at("architecture"));
            auto host_kind = host_kind_from_text(encoded.at("host_kind"));
            auto record = max_validation_record_from_json(encoded.at("record"));
            if (!operating_system || !architecture || !host_kind || !record)
                return std::unexpected(
                    failure(ErrorCode::InvalidRecord, "cells[" + std::to_string(index) + "]"));
            matrix.cells.push_back(
                {.operating_system = *operating_system,
                 .architecture = *architecture,
                 .host_kind = *host_kind,
                 .record_relative_path = encoded.at("record_relative_path").get<std::string>(),
                 .record_sha256 = encoded.at("record_sha256").get<std::string>(),
                 .record = std::move(*record)});
        }
        if (const auto error = validate_matrix(matrix)) return std::unexpected(*error);
        if (!value.at("complete").get<bool>())
            return std::unexpected(failure(ErrorCode::InvalidMatrix, "derived_complete"));
        return matrix;
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::InvalidMatrix, "matrix_shape"));
    }
}

MaxReleaseMatrixResult materialize_max_release_matrix(const json& manifest,
                                                      std::string_view manifest_sha256,
                                                      const fs::path& record_root) {
    if (const auto error = validate_manifest(manifest, manifest_sha256))
        return std::unexpected(*error);

    MaxReleaseMatrix matrix;
    matrix.manifest_sha256 = std::string{manifest_sha256};
    matrix.sunny_version = sunny::SUNNY_VERSION;
    matrix.package_version = sunny::SUNNY_VERSION;
    for (const auto& required : REQUIRED_CELLS) {
        auto bound = read_bound_record(record_root, std::string{required.record_relative_path});
        if (!bound) return std::unexpected(bound.error());
        if (matrix.cells.empty()) matrix.source_revision = bound->second.source_revision;
        matrix.cells.push_back({.operating_system = required.operating_system,
                                .architecture = required.architecture,
                                .host_kind = required.host_kind,
                                .record_relative_path = std::string{required.record_relative_path},
                                .record_sha256 = std::move(bound->first),
                                .record = std::move(bound->second)});
    }
    if (const auto error = validate_matrix(matrix)) return std::unexpected(*error);
    return matrix;
}

MaxReleaseMatrixVerificationResult verify_max_release_matrix_files(const MaxReleaseMatrix& matrix,
                                                                   const json& manifest,
                                                                   std::string_view manifest_sha256,
                                                                   const fs::path& record_root) {
    const auto parsed = max_release_matrix_from_json(max_release_matrix_to_json(matrix));
    if (!parsed) return std::unexpected(parsed.error());
    if (const auto error = validate_manifest(manifest, manifest_sha256))
        return std::unexpected(*error);
    if (parsed->manifest_sha256 != manifest_sha256)
        return std::unexpected(
            failure(ErrorCode::ManifestDigestMismatch, "release_matrix_manifest"));

    for (std::size_t index = 0; index < parsed->cells.size(); ++index) {
        const auto& cell = parsed->cells[index];
        auto bound = read_bound_record(record_root, cell.record_relative_path);
        if (!bound) return std::unexpected(bound.error());
        if (bound->first != cell.record_sha256)
            return std::unexpected(
                failure(ErrorCode::RecordDigestMismatch, cell.record_relative_path));
        if (max_validation_record_to_json(bound->second) !=
            max_validation_record_to_json(cell.record))
            return std::unexpected(failure(ErrorCode::InvalidRecord, cell.record_relative_path));
        if (!cell_identity_matches(cell, REQUIRED_CELLS[index]))
            return std::unexpected(failure(ErrorCode::CellMismatch, cell.record_relative_path));
    }
    return {};
}

const char* max_release_matrix_error_name(MaxReleaseMatrixErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidManifest:
        return "invalid_manifest";
    case ErrorCode::ManifestDigestMismatch:
        return "manifest_digest_mismatch";
    case ErrorCode::InputRootUnavailable:
        return "input_root_unavailable";
    case ErrorCode::FileUnavailable:
        return "file_unavailable";
    case ErrorCode::NonRegularFile:
        return "non_regular_file";
    case ErrorCode::IoFailure:
        return "io_failure";
    case ErrorCode::InvalidRecord:
        return "invalid_record";
    case ErrorCode::RecordDigestMismatch:
        return "record_digest_mismatch";
    case ErrorCode::CellMismatch:
        return "cell_mismatch";
    case ErrorCode::IncompleteRecord:
        return "incomplete_record";
    case ErrorCode::RevisionMismatch:
        return "revision_mismatch";
    case ErrorCode::PackageMismatch:
        return "package_mismatch";
    case ErrorCode::InvalidMatrix:
        return "invalid_matrix";
    }
    return "unknown";
}

} // namespace sunny::infrastructure
