/**
 * @file validation_materializer.cpp
 * @brief Content-addressed materialization for named Max host observations
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <span>
#include <string>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <unordered_map>
#include <utility>

namespace sunny::infrastructure {

namespace {

namespace fs = std::filesystem;

using Error = MaxValidationMaterializationError;
using ErrorCode = MaxValidationMaterializationErrorCode;

Error failure(ErrorCode code, std::string subject) {
    return {.code = code, .subject = std::move(subject)};
}

class Sha256 final {
  public:
    bool update(std::span<const std::uint8_t> bytes) noexcept {
        if (bytes.size() > std::numeric_limits<std::uint64_t>::max() - byte_count_) return false;
        byte_count_ += static_cast<std::uint64_t>(bytes.size());

        std::size_t offset = 0;
        if (buffer_size_ != 0) {
            const auto count = std::min(bytes.size(), buffer_.size() - buffer_size_);
            std::copy_n(bytes.begin(), count, buffer_.begin() + buffer_size_);
            buffer_size_ += count;
            offset += count;
            if (buffer_size_ == buffer_.size()) {
                transform(buffer_);
                buffer_size_ = 0;
            }
        }
        while (bytes.size() - offset >= buffer_.size()) {
            std::array<std::uint8_t, 64> block{};
            std::copy_n(
                bytes.begin() + static_cast<std::ptrdiff_t>(offset), block.size(), block.begin());
            transform(block);
            offset += block.size();
        }
        const auto remaining = bytes.size() - offset;
        if (remaining != 0) {
            std::copy_n(
                bytes.begin() + static_cast<std::ptrdiff_t>(offset), remaining, buffer_.begin());
            buffer_size_ = remaining;
        }
        return true;
    }

    bool finish(std::array<std::uint8_t, 32>& digest) noexcept {
        if (byte_count_ > std::numeric_limits<std::uint64_t>::max() / 8U) return false;
        const auto bit_count = byte_count_ * 8U;

        buffer_[buffer_size_++] = 0x80U;
        if (buffer_size_ > 56) {
            std::fill(
                buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.end(), 0U);
            transform(buffer_);
            buffer_size_ = 0;
        }
        std::fill(
            buffer_.begin() + static_cast<std::ptrdiff_t>(buffer_size_), buffer_.begin() + 56, 0U);
        for (std::size_t index = 0; index < 8; ++index) {
            buffer_[56 + index] =
                static_cast<std::uint8_t>(bit_count >> static_cast<unsigned>((7 - index) * 8));
        }
        transform(buffer_);

        for (std::size_t word = 0; word < state_.size(); ++word) {
            for (std::size_t byte = 0; byte < 4; ++byte) {
                digest[word * 4 + byte] = static_cast<std::uint8_t>(
                    state_[word] >> static_cast<unsigned>((3 - byte) * 8));
            }
        }
        return true;
    }

  private:
    void transform(const std::array<std::uint8_t, 64>& block) noexcept {
        static constexpr std::array<std::uint32_t, 64> constants{
            0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U,
            0x923f82a4U, 0xab1c5ed5U, 0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
            0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U, 0xe49b69c1U, 0xefbe4786U,
            0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
            0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U,
            0x06ca6351U, 0x14292967U, 0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
            0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U, 0xa2bfe8a1U, 0xa81a664bU,
            0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
            0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU,
            0x5b9cca4fU, 0x682e6ff3U, 0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
            0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
        };

        std::array<std::uint32_t, 64> schedule{};
        for (std::size_t index = 0; index < 16; ++index) {
            const auto offset = index * 4;
            schedule[index] = static_cast<std::uint32_t>(block[offset]) << 24U |
                              static_cast<std::uint32_t>(block[offset + 1]) << 16U |
                              static_cast<std::uint32_t>(block[offset + 2]) << 8U |
                              static_cast<std::uint32_t>(block[offset + 3]);
        }
        for (std::size_t index = 16; index < schedule.size(); ++index) {
            const auto left = schedule[index - 15];
            const auto right = schedule[index - 2];
            const auto sigma0 = std::rotr(left, 7) ^ std::rotr(left, 18) ^ (left >> 3U);
            const auto sigma1 = std::rotr(right, 17) ^ std::rotr(right, 19) ^ (right >> 10U);
            schedule[index] = schedule[index - 16] + sigma0 + schedule[index - 7] + sigma1;
        }

        auto a = state_[0];
        auto b = state_[1];
        auto c = state_[2];
        auto d = state_[3];
        auto e = state_[4];
        auto f = state_[5];
        auto g = state_[6];
        auto h = state_[7];
        for (std::size_t index = 0; index < schedule.size(); ++index) {
            const auto sum1 = std::rotr(e, 6) ^ std::rotr(e, 11) ^ std::rotr(e, 25);
            const auto choice = (e & f) ^ (~e & g);
            const auto temporary1 = h + sum1 + choice + constants[index] + schedule[index];
            const auto sum0 = std::rotr(a, 2) ^ std::rotr(a, 13) ^ std::rotr(a, 22);
            const auto majority = (a & b) ^ (a & c) ^ (b & c);
            const auto temporary2 = sum0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state_[0] += a;
        state_[1] += b;
        state_[2] += c;
        state_[3] += d;
        state_[4] += e;
        state_[5] += f;
        state_[6] += g;
        state_[7] += h;
    }

    std::array<std::uint32_t, 8> state_{0x6a09e667U,
                                        0xbb67ae85U,
                                        0x3c6ef372U,
                                        0xa54ff53aU,
                                        0x510e527fU,
                                        0x9b05688cU,
                                        0x1f83d9abU,
                                        0x5be0cd19U};
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0;
    std::uint64_t byte_count_ = 0;
};

bool path_within(const fs::path& root, const fs::path& candidate) {
    auto root_component = root.begin();
    auto candidate_component = candidate.begin();
    for (; root_component != root.end(); ++root_component, ++candidate_component) {
        if (candidate_component == candidate.end() || *candidate_component != *root_component)
            return false;
    }
    return candidate_component != candidate.end();
}

std::expected<fs::path, Error> prepare_root(const fs::path& root, std::string label) {
    std::error_code error;
    const auto canonical = fs::canonical(root, error);
    if (error || !fs::is_directory(canonical, error) || error)
        return std::unexpected(failure(ErrorCode::InputRootUnavailable, std::move(label)));
    return canonical;
}

std::expected<fs::path, Error> prepare_direct_file(const fs::path& path, std::string label) {
    std::error_code error;
    const auto status = fs::symlink_status(path, error);
    if (error || !fs::exists(status))
        return std::unexpected(failure(ErrorCode::FileUnavailable, std::move(label)));
    if (fs::is_symlink(status) || !fs::is_regular_file(status))
        return std::unexpected(failure(ErrorCode::NonRegularFile, std::move(label)));
    const auto canonical = fs::canonical(path, error);
    if (error) return std::unexpected(failure(ErrorCode::FileUnavailable, std::move(label)));
    return canonical;
}

std::expected<fs::path, Error> resolve_regular_file(const fs::path& root,
                                                    const std::string& relative_path) {
    const auto logical = root / fs::path{relative_path};
    std::error_code error;
    const auto logical_status = fs::symlink_status(logical, error);
    if (error || !fs::exists(logical_status))
        return std::unexpected(failure(ErrorCode::FileUnavailable, relative_path));
    if (fs::is_symlink(logical_status) || !fs::is_regular_file(logical_status))
        return std::unexpected(failure(ErrorCode::NonRegularFile, relative_path));
    const auto canonical = fs::canonical(logical, error);
    if (error || !path_within(root, canonical))
        return std::unexpected(failure(ErrorCode::NonRegularFile, relative_path));
    return canonical;
}

std::expected<std::string, Error> sha256_file(const fs::path& path, std::string subject) {
    std::error_code error;
    const auto size_before = fs::file_size(path, error);
    if (error) return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));
    const auto write_before = fs::last_write_time(path, error);
    if (error) return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));

    std::ifstream input{path, std::ios::binary};
    if (!input) return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));
    Sha256 sha256;
    std::array<char, 64 * 1024> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto count = input.gcount();
        if (count > 0 && !sha256.update({reinterpret_cast<const std::uint8_t*>(buffer.data()),
                                         static_cast<std::size_t>(count)}))
            return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));
    }
    if (input.bad() || (!input.eof() && input.fail()))
        return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));

    const auto size_after = fs::file_size(path, error);
    if (error) return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));
    const auto write_after = fs::last_write_time(path, error);
    if (error || size_before != size_after || write_before != write_after)
        return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));

    std::array<std::uint8_t, 32> digest{};
    if (!sha256.finish(digest))
        return std::unexpected(failure(ErrorCode::IoFailure, std::move(subject)));
    static constexpr char hex[] = "0123456789abcdef";
    std::string encoded(64, '0');
    for (std::size_t index = 0; index < digest.size(); ++index) {
        encoded[index * 2] = hex[digest[index] >> 4U];
        encoded[index * 2 + 1] = hex[digest[index] & 0x0fU];
    }
    return encoded;
}

MaxValidationObservation observation_from_record(const MaxValidationRecord& record) {
    MaxValidationObservation observation;
    observation.sunny_version = record.sunny_version;
    observation.package_version = record.package_version;
    observation.source_revision = record.source_revision;
    observation.observed_at_utc = record.observed_at_utc;
    observation.harness = record.harness;
    observation.environment = record.environment;
    for (const auto& artifact : record.artifacts) {
        observation.artifacts.push_back({.object_name = artifact.object_name,
                                         .package_relative_path = artifact.package_relative_path,
                                         .discovered = artifact.discovered,
                                         .instantiated = artifact.instantiated});
    }
    for (const auto& check : record.checks) {
        observation.checks.push_back({.check = check.check,
                                      .outcome = check.outcome,
                                      .summary = check.summary,
                                      .evidence_relative_path = check.evidence_relative_path});
    }
    return observation;
}

} // namespace

MaxValidationFileDigestResult max_validation_regular_file_sha256(const fs::path& path,
                                                                 std::string subject) {
    auto prepared = prepare_direct_file(path, subject);
    if (!prepared) return std::unexpected(prepared.error());
    return sha256_file(*prepared, std::move(subject));
}

MaxValidationFileDigestResult
max_validation_evidence_file_sha256(const fs::path& evidence_root,
                                    const std::string& evidence_relative_path) {
    auto root = prepare_root(evidence_root, "evidence_root");
    if (!root) return std::unexpected(root.error());
    auto path = resolve_regular_file(*root, evidence_relative_path);
    if (!path) return std::unexpected(path.error());
    return sha256_file(*path, evidence_relative_path);
}

MaxValidationMaterializationResult
materialize_max_validation_record(const MaxValidationObservation& supplied_observation,
                                  const MaxValidationMaterializationInputs& inputs) {
    const auto parsed_observation = max_validation_observation_from_json(
        max_validation_observation_to_json(supplied_observation));
    if (!parsed_observation)
        return std::unexpected(failure(ErrorCode::InvalidObservation, "observation"));
    const auto& observation = *parsed_observation;

    auto package_root = prepare_root(inputs.package_root, "package_root");
    if (!package_root) return std::unexpected(package_root.error());
    auto evidence_root = prepare_root(inputs.evidence_root, "evidence_root");
    if (!evidence_root) return std::unexpected(evidence_root.error());
    auto package_archive = prepare_direct_file(inputs.package_archive, "package_archive");
    if (!package_archive) return std::unexpected(package_archive.error());

    MaxValidationRecord record;
    record.sunny_version = observation.sunny_version;
    record.package_version = observation.package_version;
    record.source_revision = observation.source_revision;
    auto archive_hash = sha256_file(*package_archive, "package_archive");
    if (!archive_hash) return std::unexpected(archive_hash.error());
    record.package_archive_sha256 = std::move(*archive_hash);
    record.observed_at_utc = observation.observed_at_utc;
    record.harness = observation.harness;
    record.environment = observation.environment;

    for (const auto& artifact : observation.artifacts) {
        auto path = resolve_regular_file(*package_root, artifact.package_relative_path);
        if (!path) return std::unexpected(path.error());
        auto digest = sha256_file(*path, artifact.package_relative_path);
        if (!digest) return std::unexpected(digest.error());
        record.artifacts.push_back({.object_name = artifact.object_name,
                                    .package_relative_path = artifact.package_relative_path,
                                    .binary_sha256 = std::move(*digest),
                                    .discovered = artifact.discovered,
                                    .instantiated = artifact.instantiated});
    }

    std::unordered_map<std::string, std::string> evidence_digests;
    for (const auto& check : observation.checks) {
        MaxCheckObservation materialized{.check = check.check,
                                         .outcome = check.outcome,
                                         .summary = check.summary,
                                         .evidence_relative_path = check.evidence_relative_path,
                                         .evidence_sha256 = std::nullopt};
        if (check.evidence_relative_path) {
            const auto cached = evidence_digests.find(*check.evidence_relative_path);
            if (cached != evidence_digests.end()) {
                materialized.evidence_sha256 = cached->second;
            } else {
                auto path = resolve_regular_file(*evidence_root, *check.evidence_relative_path);
                if (!path) return std::unexpected(path.error());
                auto digest = sha256_file(*path, *check.evidence_relative_path);
                if (!digest) return std::unexpected(digest.error());
                materialized.evidence_sha256 = *digest;
                evidence_digests.emplace(*check.evidence_relative_path, std::move(*digest));
            }
        }
        record.checks.push_back(std::move(materialized));
    }

    auto parsed_record = max_validation_record_from_json(max_validation_record_to_json(record));
    if (!parsed_record)
        return std::unexpected(failure(ErrorCode::InvalidRecord, "materialized_record"));
    return std::move(*parsed_record);
}

MaxValidationVerificationResult
verify_max_validation_record_files(const MaxValidationRecord& supplied_record,
                                   const MaxValidationMaterializationInputs& inputs) {
    const auto parsed_record =
        max_validation_record_from_json(max_validation_record_to_json(supplied_record));
    if (!parsed_record) return std::unexpected(failure(ErrorCode::InvalidRecord, "record"));

    auto materialized =
        materialize_max_validation_record(observation_from_record(*parsed_record), inputs);
    if (!materialized) return std::unexpected(materialized.error());
    if (materialized->package_archive_sha256 != parsed_record->package_archive_sha256)
        return std::unexpected(failure(ErrorCode::DigestMismatch, "package_archive"));
    for (std::size_t index = 0; index < parsed_record->artifacts.size(); ++index) {
        if (materialized->artifacts[index].binary_sha256 !=
            parsed_record->artifacts[index].binary_sha256)
            return std::unexpected(failure(ErrorCode::DigestMismatch,
                                           parsed_record->artifacts[index].package_relative_path));
    }
    for (std::size_t index = 0; index < parsed_record->checks.size(); ++index) {
        if (materialized->checks[index].evidence_sha256 !=
            parsed_record->checks[index].evidence_sha256) {
            const auto subject = parsed_record->checks[index].evidence_relative_path.value_or(
                "check_without_evidence");
            return std::unexpected(failure(ErrorCode::DigestMismatch, subject));
        }
    }
    return {};
}

const char*
max_validation_materialization_error_name(MaxValidationMaterializationErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidObservation:
        return "invalid_observation";
    case ErrorCode::InvalidRecord:
        return "invalid_record";
    case ErrorCode::InputRootUnavailable:
        return "input_root_unavailable";
    case ErrorCode::FileUnavailable:
        return "file_unavailable";
    case ErrorCode::NonRegularFile:
        return "non_regular_file";
    case ErrorCode::IoFailure:
        return "io_failure";
    case ErrorCode::DigestMismatch:
        return "digest_mismatch";
    }
    return "unknown";
}

} // namespace sunny::infrastructure
