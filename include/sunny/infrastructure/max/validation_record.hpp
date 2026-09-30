/**
 * @file validation_record.hpp
 * @brief Strict evidence envelope for one named Max or Max for Live host run
 */

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <string>
#include <sunny/core/types/music_types.hpp>
#include <vector>

namespace sunny::infrastructure {

inline constexpr std::uint32_t MAX_VALIDATION_RECORD_SCHEMA_VERSION = 1;
inline constexpr std::uint32_t MAX_VALIDATION_OBSERVATION_SCHEMA_VERSION = 1;

enum class MaxHostKind : std::uint8_t { StandaloneMax, MaxForLive };
enum class MaxOperatingSystem : std::uint8_t { MacOS, Windows };
enum class MaxArchitecture : std::uint8_t { X86_64, Arm64 };
enum class MaxValidationOutcome : std::uint8_t { Passed, Failed, NotRun, NotApplicable };

/** Stable schema spellings shared by validation records and release coverage. */
[[nodiscard]] const char* max_host_kind_name(MaxHostKind value) noexcept;
[[nodiscard]] const char* max_operating_system_name(MaxOperatingSystem value) noexcept;
[[nodiscard]] const char* max_architecture_name(MaxArchitecture value) noexcept;

enum class MaxValidationCheck : std::uint8_t {
    PackageDiscovery,
    PublicClassSurface,
    SignalTopology,
    DspSetup,
    PerformCallback,
    FiniteSignalOutput,
    DisconnectedProcessing,
    ReconnectContinuity,
    ControlDispatch,
    ActiveDspTeardown,
    ItmScheduleFire,
    ItmEqualTickOrdering,
    ItmClearReassign,
    ReleaseVelocityFormatting,
    SchedulerTiming,
    DownstreamMidiDelivery,
    RenderedAudio,
    LiveTransportDiscontinuities,
    StandaloneTransportIdentity,
};

/** Stable schema spelling for one closed Max host check. */
[[nodiscard]] const char* max_validation_check_name(MaxValidationCheck check) noexcept;

/** Exact operator/test-harness facts for one host configuration. */
struct MaxValidationEnvironment {
    MaxHostKind host_kind = MaxHostKind::StandaloneMax;
    std::string max_version;
    std::optional<std::string> live_version;
    std::optional<std::string> max_for_live_version;
    MaxOperatingSystem operating_system = MaxOperatingSystem::MacOS;
    MaxArchitecture architecture = MaxArchitecture::X86_64;
    std::string audio_driver;
    double sample_rate = 48'000.0;
    std::uint32_t io_vector_size = 512;
    std::uint32_t signal_vector_size = 64;
    bool overdrive = false;
    bool scheduler_in_audio_interrupt = false;
};

/** One exact platform binary intended to provide a maintained Sunny Max object. */
struct MaxArtifactObservation {
    std::string object_name;
    std::string package_relative_path;
    std::string binary_sha256;
    bool discovered = false;
    bool instantiated = false;
};

/** One closed validation check and its content-addressed external evidence. */
struct MaxCheckObservation {
    MaxValidationCheck check = MaxValidationCheck::PackageDiscovery;
    MaxValidationOutcome outcome = MaxValidationOutcome::NotRun;
    std::string summary;
    std::optional<std::string> evidence_relative_path;
    std::optional<std::string> evidence_sha256;
};

/** One binary's host observations before its package bytes are content-addressed. */
struct MaxArtifactFact {
    std::string object_name;
    std::string package_relative_path;
    bool discovered = false;
    bool instantiated = false;
};

/** One host-check observation before its evidence bytes are content-addressed. */
struct MaxCheckFact {
    MaxValidationCheck check = MaxValidationCheck::PackageDiscovery;
    MaxValidationOutcome outcome = MaxValidationOutcome::NotRun;
    std::string summary;
    std::optional<std::string> evidence_relative_path;
};

/**
 * Closed operator/harness observations for one run. Unlike MaxValidationRecord, this object makes
 * no content-addressing claim: materialization must read every referenced file and derive hashes.
 */
struct MaxValidationObservation {
    std::uint32_t schema_version = MAX_VALIDATION_OBSERVATION_SCHEMA_VERSION;
    std::string sunny_version;
    std::string package_version;
    std::string source_revision;
    std::string observed_at_utc;
    std::string harness;
    MaxValidationEnvironment environment;
    std::vector<MaxArtifactFact> artifacts;
    std::vector<MaxCheckFact> checks;
};

/**
 * One immutable, revision-bound host-run record. `complete()` establishes only that example
 * sentinels are absent, every applicable check reports passed, and every binary was discovered and
 * instantiated. It does not authenticate operator statements or prove another host/configuration.
 */
struct MaxValidationRecord {
    std::uint32_t schema_version = MAX_VALIDATION_RECORD_SCHEMA_VERSION;
    std::string sunny_version;
    std::string package_version;
    std::string source_revision;
    std::string package_archive_sha256;
    std::string observed_at_utc;
    std::string harness;
    MaxValidationEnvironment environment;
    std::vector<MaxArtifactObservation> artifacts;
    std::vector<MaxCheckObservation> checks;

    [[nodiscard]] bool complete() const;
};

/** Strict schema-v1 parser; extensions, reordered/omitted closed sets, and contradictions fail. */
[[nodiscard]] sunny::core::Result<MaxValidationRecord>
max_validation_record_from_json(const nlohmann::json& value);

/** Canonical JSON encoding with a checked derived `complete` member. */
[[nodiscard]] nlohmann::json max_validation_record_to_json(const MaxValidationRecord& record);

/** Strict schema-v1 parser for unhashed named-host observations. */
[[nodiscard]] sunny::core::Result<MaxValidationObservation>
max_validation_observation_from_json(const nlohmann::json& value);

/** Canonical JSON encoding of one unhashed named-host observation. */
[[nodiscard]] nlohmann::json
max_validation_observation_to_json(const MaxValidationObservation& observation);

/** Files whose bytes bind a run observation to one distributable package and evidence tree. */
struct MaxValidationMaterializationInputs {
    std::filesystem::path package_archive;
    std::filesystem::path package_root;
    std::filesystem::path evidence_root;
};

enum class MaxValidationMaterializationErrorCode : std::uint8_t {
    InvalidObservation,
    InvalidRecord,
    InputRootUnavailable,
    FileUnavailable,
    NonRegularFile,
    IoFailure,
    DigestMismatch,
};

struct MaxValidationMaterializationError {
    MaxValidationMaterializationErrorCode code =
        MaxValidationMaterializationErrorCode::InvalidObservation;
    std::string subject;
};

using MaxValidationMaterializationResult =
    std::expected<MaxValidationRecord, MaxValidationMaterializationError>;
using MaxValidationVerificationResult = std::expected<void, MaxValidationMaterializationError>;
using MaxValidationFileDigestResult = std::expected<std::string, MaxValidationMaterializationError>;

/** Hash one direct non-symlink regular file with stable size/write-time checks. */
[[nodiscard]] MaxValidationFileDigestResult
max_validation_regular_file_sha256(const std::filesystem::path& path, std::string subject);

/**
 * Hash one regular evidence file below a canonical root using the same confinement and stable-read
 * policy as record materialization.
 */
[[nodiscard]] MaxValidationFileDigestResult
max_validation_evidence_file_sha256(const std::filesystem::path& evidence_root,
                                    const std::string& evidence_relative_path);

/**
 * Read and SHA-256-address the exact archive, platform binaries, and observed evidence files.
 * Symlink file leaves, non-regular files, escaped roots, missing files, and I/O failures decline.
 */
[[nodiscard]] MaxValidationMaterializationResult
materialize_max_validation_record(const MaxValidationObservation& observation,
                                  const MaxValidationMaterializationInputs& inputs);

/** Re-read every bound file and require byte-for-byte digest agreement with a strict record. */
[[nodiscard]] MaxValidationVerificationResult
verify_max_validation_record_files(const MaxValidationRecord& record,
                                   const MaxValidationMaterializationInputs& inputs);

[[nodiscard]] const char*
max_validation_materialization_error_name(MaxValidationMaterializationErrorCode code) noexcept;

} // namespace sunny::infrastructure
