/**
 * @file sunny_max_evidence.cpp
 * @brief Materialize and verify content-addressed Max named-host evidence
 */

#include <array>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/host_run_result.hpp>
#include <sunny/infrastructure/max/max_test_result.hpp>
#include <sunny/infrastructure/max/release_matrix.hpp>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <system_error>
#include <utility>

namespace {

using nlohmann::json;
using sunny::infrastructure::MaxValidationMaterializationError;
using sunny::infrastructure::MaxValidationMaterializationInputs;

void print_usage(std::ostream& output) {
    output
        << "Usage:\n"
        << "  sunny-max-evidence assemble OBSERVATION PACKAGE_ARCHIVE PACKAGE_ROOT EVIDENCE_ROOT\n"
        << "  sunny-max-evidence verify RECORD PACKAGE_ARCHIVE PACKAGE_ROOT EVIDENCE_ROOT\n"
        << "  sunny-max-evidence apply-max-test MANIFEST RESULT OBSERVATION EVIDENCE_ROOT "
           "DATABASE_RELATIVE_PATH\n"
        << "  sunny-max-evidence apply-host-run PLAN RESULT OBSERVATION EVIDENCE_ROOT "
           "RESULT_RELATIVE_PATH\n"
        << "  sunny-max-evidence verify-host-run PLAN RESULT EVIDENCE_ROOT "
           "RESULT_RELATIVE_PATH\n"
        << "  sunny-max-evidence assemble-matrix MANIFEST RECORD_ROOT\n"
        << "  sunny-max-evidence verify-matrix MATRIX MANIFEST RECORD_ROOT\n\n"
        << "assemble writes one canonical schema-1 record to stdout. verify recomputes every "
           "bound SHA-256. apply-max-test verifies and maps one normalized pinned max-test run. "
           "apply-host-run natively evaluates the four shared residual measurements plus the "
           "conditional M4L discontinuity measurement and updates their facts; verify-host-run "
           "re-hashes and re-evaluates that transitive evidence set. "
           "assemble-matrix embeds and binds the six required complete records; verify-matrix "
           "re-hashes those record files.\n";
}

bool read_json_file(const std::filesystem::path& path, json& value) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        std::cerr << "sunny-max-evidence: cannot open JSON input: " << path.generic_string()
                  << "\n";
        return false;
    }
    try {
        value = json::parse(input);
        return true;
    } catch (const json::exception& error) {
        std::cerr << "sunny-max-evidence: invalid JSON input: " << error.what() << "\n";
        return false;
    }
}

void report_materialization_error(const MaxValidationMaterializationError& error) {
    std::cerr << "sunny-max-evidence: "
              << sunny::infrastructure::max_validation_materialization_error_name(error.code)
              << ": " << error.subject << "\n";
}

void report_matrix_error(const sunny::infrastructure::MaxReleaseMatrixError& error) {
    std::cerr << "sunny-max-evidence: "
              << sunny::infrastructure::max_release_matrix_error_name(error.code) << ": "
              << error.subject << "\n";
}

void report_host_run_error(const sunny::infrastructure::MaxHostRunApplicationError& error) {
    std::cerr << "sunny-max-evidence: "
              << sunny::infrastructure::max_host_run_application_error_name(error.code) << ": "
              << error.subject << "\n";
}

std::optional<std::string> max_test_database_sidecar(const std::filesystem::path& evidence_root,
                                                     const std::string& evidence_relative_path) {
    auto database = evidence_root / std::filesystem::path{evidence_relative_path};
    constexpr std::array<std::string_view, 3> suffixes{"-journal", "-wal", "-shm"};
    for (const auto suffix : suffixes) {
        auto sidecar = database;
        sidecar += suffix;
        std::error_code error;
        const auto status = std::filesystem::symlink_status(sidecar, error);
        if (error && error != std::errc::no_such_file_or_directory) return sidecar.generic_string();
        if (!error && std::filesystem::exists(status)) return sidecar.generic_string();
    }
    return std::nullopt;
}

int assemble(const std::filesystem::path& observation_path,
             const MaxValidationMaterializationInputs& inputs) {
    json encoded;
    if (!read_json_file(observation_path, encoded)) return 3;
    const auto observation = sunny::infrastructure::max_validation_observation_from_json(encoded);
    if (!observation) {
        std::cerr << "sunny-max-evidence: observation violates the native schema-1 contract\n";
        return 3;
    }
    const auto record =
        sunny::infrastructure::materialize_max_validation_record(*observation, inputs);
    if (!record) {
        report_materialization_error(record.error());
        return 4;
    }
    std::cout << sunny::infrastructure::max_validation_record_to_json(*record).dump(2) << '\n';
    return 0;
}

int verify(const std::filesystem::path& record_path,
           const MaxValidationMaterializationInputs& inputs) {
    json encoded;
    if (!read_json_file(record_path, encoded)) return 3;
    const auto record = sunny::infrastructure::max_validation_record_from_json(encoded);
    if (!record) {
        std::cerr << "sunny-max-evidence: record violates the native schema-1 contract\n";
        return 3;
    }
    const auto verified =
        sunny::infrastructure::verify_max_validation_record_files(*record, inputs);
    if (!verified) {
        report_materialization_error(verified.error());
        return 4;
    }
    std::cout << "verified\n";
    return 0;
}

int apply_max_test(const std::filesystem::path& manifest_path,
                   const std::filesystem::path& result_path,
                   const std::filesystem::path& observation_path,
                   const std::filesystem::path& evidence_root,
                   std::string evidence_relative_path) {
    json encoded_manifest;
    json encoded_result;
    json encoded_observation;
    if (!read_json_file(manifest_path, encoded_manifest) ||
        !read_json_file(result_path, encoded_result) ||
        !read_json_file(observation_path, encoded_observation))
        return 3;

    const auto run = sunny::infrastructure::max_test_run_result_from_json(encoded_result);
    const auto observation =
        sunny::infrastructure::max_validation_observation_from_json(encoded_observation);
    if (!run || !observation) {
        std::cerr << "sunny-max-evidence: max-test result or observation violates its native "
                     "schema-1 contract\n";
        return 3;
    }

    const auto manifest_digest = sunny::infrastructure::max_validation_regular_file_sha256(
        manifest_path, "harness_manifest");
    if (!manifest_digest) {
        report_materialization_error(manifest_digest.error());
        return 4;
    }
    if (const auto sidecar = max_test_database_sidecar(evidence_root, evidence_relative_path)) {
        std::cerr << "sunny-max-evidence: max_test_database_not_quiescent: " << *sidecar << '\n';
        return 4;
    }
    const auto database_digest = sunny::infrastructure::max_validation_evidence_file_sha256(
        evidence_root, evidence_relative_path);
    if (!database_digest) {
        report_materialization_error(database_digest.error());
        return 4;
    }
    if (const auto sidecar = max_test_database_sidecar(evidence_root, evidence_relative_path)) {
        std::cerr << "sunny-max-evidence: max_test_database_not_quiescent: " << *sidecar << '\n';
        return 4;
    }

    auto applied = sunny::infrastructure::apply_max_test_run(*observation,
                                                             encoded_manifest,
                                                             *manifest_digest,
                                                             *run,
                                                             *database_digest,
                                                             std::move(evidence_relative_path));
    if (!applied) {
        std::cerr << "sunny-max-evidence: "
                  << sunny::infrastructure::max_test_application_error_name(applied.error().code)
                  << ": " << applied.error().subject << '\n';
        return 5;
    }
    std::cout << sunny::infrastructure::max_validation_observation_to_json(*applied).dump(2)
              << '\n';
    return 0;
}

int apply_host_run(const std::filesystem::path& plan_path,
                   const std::filesystem::path& result_path,
                   const std::filesystem::path& observation_path,
                   const std::filesystem::path& evidence_root,
                   std::string result_relative_path) {
    json plan;
    json indexed_result;
    json encoded_observation;
    if (!read_json_file(plan_path, plan) || !read_json_file(result_path, indexed_result) ||
        !read_json_file(observation_path, encoded_observation))
        return 3;
    const auto observation =
        sunny::infrastructure::max_validation_observation_from_json(encoded_observation);
    if (!observation) {
        std::cerr << "sunny-max-evidence: observation violates the native schema-1 contract\n";
        return 3;
    }
    const auto plan_digest =
        sunny::infrastructure::max_validation_regular_file_sha256(plan_path, "host_run_plan");
    const auto result_digest =
        sunny::infrastructure::max_validation_regular_file_sha256(result_path, "host_run_result");
    if (!plan_digest) {
        report_materialization_error(plan_digest.error());
        return 4;
    }
    if (!result_digest) {
        report_materialization_error(result_digest.error());
        return 4;
    }
    auto applied =
        sunny::infrastructure::apply_max_host_run_result(*observation,
                                                         plan,
                                                         *plan_digest,
                                                         indexed_result,
                                                         *result_digest,
                                                         evidence_root,
                                                         std::move(result_relative_path));
    if (!applied) {
        report_host_run_error(applied.error());
        return 5;
    }
    std::cout << sunny::infrastructure::max_validation_observation_to_json(*applied).dump(2)
              << '\n';
    return 0;
}

int verify_host_run(const std::filesystem::path& plan_path,
                    const std::filesystem::path& result_path,
                    const std::filesystem::path& evidence_root,
                    std::string_view result_relative_path) {
    json plan;
    json indexed_result;
    if (!read_json_file(plan_path, plan) || !read_json_file(result_path, indexed_result)) return 3;
    const auto plan_digest =
        sunny::infrastructure::max_validation_regular_file_sha256(plan_path, "host_run_plan");
    const auto result_digest =
        sunny::infrastructure::max_validation_regular_file_sha256(result_path, "host_run_result");
    if (!plan_digest) {
        report_materialization_error(plan_digest.error());
        return 4;
    }
    if (!result_digest) {
        report_materialization_error(result_digest.error());
        return 4;
    }
    const auto verified = sunny::infrastructure::verify_max_host_run_result(
        plan, *plan_digest, indexed_result, *result_digest, evidence_root, result_relative_path);
    if (!verified) {
        report_host_run_error(verified.error());
        return 5;
    }
    std::cout << "verified\n";
    return 0;
}

int assemble_matrix(const std::filesystem::path& manifest_path,
                    const std::filesystem::path& record_root) {
    json manifest;
    if (!read_json_file(manifest_path, manifest)) return 3;
    const auto digest = sunny::infrastructure::max_validation_regular_file_sha256(
        manifest_path, "release_matrix_manifest");
    if (!digest) {
        report_materialization_error(digest.error());
        return 4;
    }
    const auto matrix =
        sunny::infrastructure::materialize_max_release_matrix(manifest, *digest, record_root);
    if (!matrix) {
        report_matrix_error(matrix.error());
        return 5;
    }
    std::cout << sunny::infrastructure::max_release_matrix_to_json(*matrix).dump(2) << '\n';
    return 0;
}

int verify_matrix(const std::filesystem::path& matrix_path,
                  const std::filesystem::path& manifest_path,
                  const std::filesystem::path& record_root) {
    json encoded_matrix;
    json manifest;
    if (!read_json_file(matrix_path, encoded_matrix) || !read_json_file(manifest_path, manifest))
        return 3;
    const auto matrix = sunny::infrastructure::max_release_matrix_from_json(encoded_matrix);
    if (!matrix) {
        report_matrix_error(matrix.error());
        return 3;
    }
    const auto digest = sunny::infrastructure::max_validation_regular_file_sha256(
        manifest_path, "release_matrix_manifest");
    if (!digest) {
        report_materialization_error(digest.error());
        return 4;
    }
    const auto verified = sunny::infrastructure::verify_max_release_matrix_files(
        *matrix, manifest, *digest, record_root);
    if (!verified) {
        report_matrix_error(verified.error());
        return 5;
    }
    std::cout << "verified\n";
    return 0;
}

int run(int argc, char** argv) {
    if (argc < 2) {
        print_usage(std::cerr);
        return 2;
    }
    if (argc == 2 && std::string_view{argv[1]} == "--help") {
        print_usage(std::cout);
        return 0;
    }
    const std::string_view command{argv[1]};
    if (command == "assemble" || command == "verify") {
        if (argc != 6) {
            print_usage(std::cerr);
            return 2;
        }
        const MaxValidationMaterializationInputs inputs{
            .package_archive = argv[3], .package_root = argv[4], .evidence_root = argv[5]};
        if (command == "assemble") return assemble(argv[2], inputs);
        return verify(argv[2], inputs);
    }
    if (command == "apply-max-test") {
        if (argc != 7) {
            print_usage(std::cerr);
            return 2;
        }
        return apply_max_test(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
    if (command == "apply-host-run") {
        if (argc != 7) {
            print_usage(std::cerr);
            return 2;
        }
        return apply_host_run(argv[2], argv[3], argv[4], argv[5], argv[6]);
    }
    if (command == "verify-host-run") {
        if (argc != 6) {
            print_usage(std::cerr);
            return 2;
        }
        return verify_host_run(argv[2], argv[3], argv[4], argv[5]);
    }
    if (command == "assemble-matrix") {
        if (argc != 4) {
            print_usage(std::cerr);
            return 2;
        }
        return assemble_matrix(argv[2], argv[3]);
    }
    if (command == "verify-matrix") {
        if (argc != 5) {
            print_usage(std::cerr);
            return 2;
        }
        return verify_matrix(argv[2], argv[3], argv[4]);
    }
    std::cerr << "sunny-max-evidence: unknown command: " << command << "\n";
    print_usage(std::cerr);
    return 2;
}

} // namespace

int main(int argc, char** argv) noexcept {
    try {
        return run(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "sunny-max-evidence: fatal error: " << error.what() << "\n";
    } catch (...) {
        std::cerr << "sunny-max-evidence: fatal unknown error\n";
    }
    return 1;
}
