/**
 * @file validation_record_test.cpp
 * @brief Strict Max/Max for Live host-evidence record tests
 */

#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <sunny/version.hpp>

using nlohmann::json;
using sunny::core::ErrorCode;
using namespace sunny::infrastructure;

namespace {

constexpr std::array<MaxValidationCheck, 19> CHECKS{
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

std::string hash(char digit) {
    return std::string(63, digit) + (digit == 'f' ? "e" : "f");
}

MaxValidationRecord complete_record(MaxHostKind host = MaxHostKind::StandaloneMax) {
    MaxValidationRecord record;
    record.sunny_version = sunny::SUNNY_VERSION;
    record.package_version = sunny::SUNNY_VERSION;
    record.source_revision = "0123456789abcdef0123456789abcdef01234567";
    record.package_archive_sha256 = hash('2');
    record.observed_at_utc = "2026-08-31T10:00:00Z";
    record.harness = "Sunny named-host harness 1";
    record.environment.host_kind = host;
    record.environment.max_version = "9.0.5";
    record.environment.operating_system = MaxOperatingSystem::MacOS;
    record.environment.architecture = MaxArchitecture::Arm64;
    record.environment.audio_driver = "Core Audio";
    record.environment.sample_rate = 48'000.0;
    record.environment.io_vector_size = 512;
    record.environment.signal_vector_size = 64;
    record.environment.overdrive = true;
    record.environment.scheduler_in_audio_interrupt = true;
    if (host == MaxHostKind::MaxForLive) {
        record.environment.live_version = "12.3.1";
        record.environment.max_for_live_version = "9.0.5";
    }

    constexpr std::array<const char*, 5> objects{
        "sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"};
    for (std::size_t index = 0; index < objects.size(); ++index) {
        record.artifacts.push_back(
            {.object_name = objects[index],
             .package_relative_path = "externals/" + std::string(objects[index]) +
                                      ".mxo/Contents/MacOS/" + objects[index],
             .binary_sha256 = hash(static_cast<char>('3' + index)),
             .discovered = true,
             .instantiated = true});
    }

    for (std::size_t index = 0; index < CHECKS.size(); ++index) {
        const auto check = CHECKS[index];
        const bool applicable = (check != MaxValidationCheck::LiveTransportDiscontinuities ||
                                 host == MaxHostKind::MaxForLive) &&
                                (check != MaxValidationCheck::StandaloneTransportIdentity ||
                                 host == MaxHostKind::StandaloneMax);
        MaxCheckObservation observation;
        observation.check = check;
        observation.outcome =
            applicable ? MaxValidationOutcome::Passed : MaxValidationOutcome::NotApplicable;
        observation.summary =
            applicable ? "Observed expected host behavior" : "Check belongs to the other host kind";
        if (applicable) {
            observation.evidence_relative_path =
                "evidence/check-" + std::to_string(index) + ".json";
            observation.evidence_sha256 = hash(static_cast<char>('a' + (index % 6)));
        }
        record.checks.push_back(std::move(observation));
    }
    return record;
}

MaxValidationObservation observation_from(const MaxValidationRecord& record) {
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

class TemporaryValidationFiles final {
  public:
    TemporaryValidationFiles() {
        static std::atomic_uint64_t sequence{};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
               ("sunny-max-validation-" + std::to_string(tick) + "-" +
                std::to_string(sequence.fetch_add(1)));
        package_root = root / "package";
        evidence_root = root / "evidence";
        package_archive = root / "Sunny.amxd-package.zip";
        std::filesystem::create_directories(package_root);
        std::filesystem::create_directories(evidence_root);
    }

    ~TemporaryValidationFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    TemporaryValidationFiles(const TemporaryValidationFiles&) = delete;
    TemporaryValidationFiles& operator=(const TemporaryValidationFiles&) = delete;

    bool write(const std::filesystem::path& path, std::string_view bytes = "abc") const {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream output{path, std::ios::binary};
        output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        return output.good();
    }

    bool populate(const MaxValidationObservation& observation) const {
        if (!write(package_archive)) return false;
        for (const auto& artifact : observation.artifacts) {
            if (!write(package_root / artifact.package_relative_path)) return false;
        }
        for (const auto& check : observation.checks) {
            if (check.evidence_relative_path &&
                !write(evidence_root / *check.evidence_relative_path))
                return false;
        }
        return true;
    }

    [[nodiscard]] MaxValidationMaterializationInputs inputs() const {
        return {.package_archive = package_archive,
                .package_root = package_root,
                .evidence_root = evidence_root};
    }

    std::filesystem::path root;
    std::filesystem::path package_root;
    std::filesystem::path evidence_root;
    std::filesystem::path package_archive;
};

void reject(json value) {
    const auto parsed = max_validation_record_from_json(value);
    REQUIRE_FALSE(parsed);
    CHECK(parsed.error() == ErrorCode::FormatError);
}

} // namespace

TEST_CASE("complete standalone Max validation evidence round trips canonically",
          "[max][validation-record][roundtrip]") {
    const auto record = complete_record();
    REQUIRE(record.complete());
    const auto encoded = max_validation_record_to_json(record);
    CHECK(encoded.at("complete") == true);
    CHECK(encoded.at("environment").at("live_version").is_null());
    CHECK(encoded.at("checks").back().at("outcome") == "passed");
    CHECK(encoded.at("checks").at(17).at("outcome") == "not_applicable");

    const auto parsed = max_validation_record_from_json(encoded);
    REQUIRE(parsed);
    CHECK(parsed->complete());
    CHECK(max_validation_record_to_json(*parsed) == encoded);
}

TEST_CASE("Max for Live evidence requires its host versions and opposite conditional check",
          "[max][validation-record][environment]") {
    const auto record = complete_record(MaxHostKind::MaxForLive);
    REQUIRE(record.complete());
    const auto encoded = max_validation_record_to_json(record);
    CHECK(encoded.at("checks").at(17).at("outcome") == "passed");
    CHECK(encoded.at("checks").at(18).at("outcome") == "not_applicable");
    CHECK(max_validation_record_from_json(encoded));

    auto value = encoded;
    value["environment"]["live_version"] = nullptr;
    reject(value);
    value = encoded;
    value["environment"]["max_for_live_version"] = "";
    reject(value);
    value = encoded;
    value["environment"]["host_kind"] = "standalone_max";
    reject(value);

    auto windows = complete_record();
    windows.environment.operating_system = MaxOperatingSystem::Windows;
    windows.environment.architecture = MaxArchitecture::X86_64;
    for (auto& artifact : windows.artifacts)
        artifact.package_relative_path = "externals/" + artifact.object_name + ".mxe64";
    CHECK(windows.complete());
    CHECK(max_validation_record_from_json(max_validation_record_to_json(windows)));
}

TEST_CASE("Max validation completeness is derived from artifacts and applicable checks",
          "[max][validation-record][complete]") {
    const auto canonical = max_validation_record_to_json(complete_record());

    auto value = canonical;
    value["artifacts"][0]["instantiated"] = false;
    value["complete"] = false;
    auto parsed = max_validation_record_from_json(value);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->complete());

    value = canonical;
    value["checks"][0]["outcome"] = "not_run";
    value["checks"][0]["evidence_relative_path"] = nullptr;
    value["checks"][0]["evidence_sha256"] = nullptr;
    value["complete"] = false;
    parsed = max_validation_record_from_json(value);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->complete());

    value["complete"] = true;
    reject(value);

    value = canonical;
    value["harness"] = "example_observation_not_evidence";
    value["complete"] = false;
    parsed = max_validation_record_from_json(value);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->complete());
    value["complete"] = true;
    reject(value);

    value = canonical;
    value["source_revision"] = std::string(40, '1');
    value["complete"] = false;
    parsed = max_validation_record_from_json(value);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->complete());
}

TEST_CASE("Max validation record closes provenance and supported-platform shape",
          "[max][validation-record][provenance]") {
    const auto canonical = max_validation_record_to_json(complete_record());
    auto value = canonical;
    value["extra"] = true;
    reject(value);
    value = canonical;
    value["sunny_version"] = "999.0.0";
    reject(value);
    value = canonical;
    value["source_revision"] = std::string(40, '0');
    reject(value);
    value = canonical;
    value["package_archive_sha256"] = std::string(64, 'A');
    reject(value);
    value = canonical;
    value["observed_at_utc"] = "2026-08-31 10:00:00";
    reject(value);
    value = canonical;
    value["observed_at_utc"] = "2026-02-31T10:00:00Z";
    reject(value);
    value = canonical;
    value["environment"]["sample_rate"] = 48'000;
    reject(value);
    value = canonical;
    value["environment"]["io_vector_size"] = true;
    reject(value);
    value = canonical;
    value["environment"]["operating_system"] = "windows";
    value["environment"]["architecture"] = "arm64";
    reject(value);
}

TEST_CASE("Max validation artifacts are an exact ordered and content-addressed set",
          "[max][validation-record][artifacts]") {
    const auto canonical = max_validation_record_to_json(complete_record());
    auto value = canonical;
    std::swap(value["artifacts"][0], value["artifacts"][1]);
    reject(value);
    value = canonical;
    value["artifacts"].push_back(value["artifacts"][0]);
    reject(value);
    value = canonical;
    value["artifacts"][0]["package_relative_path"] = "../outside";
    reject(value);
    value = canonical;
    value["artifacts"][0]["package_relative_path"] = "C:/outside";
    reject(value);
    value = canonical;
    value["artifacts"][0]["binary_sha256"] = std::string(64, '0');
    reject(value);
    value = canonical;
    value["artifacts"][0]["discovered"] = false;
    value["complete"] = false;
    reject(value);
}

TEST_CASE("Max validation checks close order applicability and evidence pairing",
          "[max][validation-record][checks]") {
    const auto canonical = max_validation_record_to_json(complete_record());
    auto value = canonical;
    std::swap(value["checks"][0], value["checks"][1]);
    reject(value);
    value = canonical;
    value["checks"][0]["check"] = "unknown";
    reject(value);
    value = canonical;
    value["checks"][0]["outcome"] = "not_applicable";
    value["checks"][0]["evidence_relative_path"] = nullptr;
    value["checks"][0]["evidence_sha256"] = nullptr;
    value["complete"] = false;
    reject(value);
    value = canonical;
    value["checks"][0]["evidence_sha256"] = nullptr;
    value["complete"] = false;
    reject(value);
    value = canonical;
    value["checks"][0]["evidence_relative_path"] = "/absolute/log.json";
    value["complete"] = false;
    reject(value);
    value = canonical;
    value["checks"][0]["evidence_sha256"] = std::string(64, '0');
    value["complete"] = false;
    reject(value);
}

TEST_CASE("the staged Max validation example is canonical but cannot claim completion",
          "[max][validation-record][example]") {
    const auto repository =
        std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path().parent_path();
    std::ifstream input{repository /
                        "max-package/misc/validation/max-validation-record.example.json"};
    REQUIRE(input.good());
    const auto encoded = json::parse(input);
    const auto parsed = max_validation_record_from_json(encoded);
    REQUIRE(parsed);
    CHECK_FALSE(parsed->complete());
    CHECK(max_validation_record_to_json(*parsed) == encoded);

    std::ifstream observation_input{
        repository / "max-package/misc/validation/max-validation-observation.example.json"};
    REQUIRE(observation_input.good());
    const auto observation_encoded = json::parse(observation_input);
    const auto observation = max_validation_observation_from_json(observation_encoded);
    REQUIRE(observation);
    CHECK(max_validation_observation_to_json(*observation) == observation_encoded);
}

TEST_CASE("Max run observations are a strict unhashed closed contract",
          "[max][validation-record][observation]") {
    const auto observation = observation_from(complete_record());
    const auto canonical = max_validation_observation_to_json(observation);
    auto parsed = max_validation_observation_from_json(canonical);
    REQUIRE(parsed);
    CHECK(max_validation_observation_to_json(*parsed) == canonical);

    auto value = canonical;
    value["complete"] = true;
    CHECK_FALSE(max_validation_observation_from_json(value));
    value = canonical;
    value["artifacts"][0]["binary_sha256"] = hash('a');
    CHECK_FALSE(max_validation_observation_from_json(value));
    value = canonical;
    value["checks"][0]["evidence_relative_path"] = nullptr;
    CHECK_FALSE(max_validation_observation_from_json(value));
    value = canonical;
    value["checks"][0]["evidence_relative_path"] = "../escape.json";
    CHECK_FALSE(max_validation_observation_from_json(value));
}

TEST_CASE("Max records derive all file digests and verify them against exact bytes",
          "[max][validation-record][materialization]") {
    const auto observation = observation_from(complete_record());
    TemporaryValidationFiles files;
    REQUIRE(files.populate(observation));

    const auto materialized = materialize_max_validation_record(observation, files.inputs());
    REQUIRE(materialized);
    constexpr std::string_view abc_sha256 =
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
    CHECK(materialized->complete());
    CHECK(materialized->package_archive_sha256 == abc_sha256);
    for (const auto& artifact : materialized->artifacts)
        CHECK(artifact.binary_sha256 == abc_sha256);
    for (const auto& check : materialized->checks) {
        if (check.evidence_sha256) CHECK(*check.evidence_sha256 == abc_sha256);
    }
    CHECK(verify_max_validation_record_files(*materialized, files.inputs()));

    const std::string thousand_a(1'000, 'a');
    REQUIRE(files.write(files.package_archive, thousand_a));
    const auto multi_block = materialize_max_validation_record(observation, files.inputs());
    REQUIRE(multi_block);
    CHECK(multi_block->package_archive_sha256 ==
          "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");
    REQUIRE(files.write(files.package_archive));

    const auto evidence = *observation.checks.front().evidence_relative_path;
    REQUIRE(files.write(files.evidence_root / evidence, "changed"));
    const auto mismatch = verify_max_validation_record_files(*materialized, files.inputs());
    REQUIRE_FALSE(mismatch);
    CHECK(mismatch.error().code == MaxValidationMaterializationErrorCode::DigestMismatch);
    CHECK(mismatch.error().subject == evidence);
}

TEST_CASE("Max record materialization rejects absent roots, missing files, and directories",
          "[max][validation-record][materialization][filesystem]") {
    const auto observation = observation_from(complete_record());
    TemporaryValidationFiles files;
    REQUIRE(files.populate(observation));

    auto inputs = files.inputs();
    inputs.evidence_root = files.root / "absent";
    auto result = materialize_max_validation_record(observation, inputs);
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxValidationMaterializationErrorCode::InputRootUnavailable);
    CHECK(result.error().subject == "evidence_root");

    const auto evidence = *observation.checks.front().evidence_relative_path;
    std::filesystem::remove(files.evidence_root / evidence);
    result = materialize_max_validation_record(observation, files.inputs());
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxValidationMaterializationErrorCode::FileUnavailable);
    CHECK(result.error().subject == evidence);

    std::filesystem::create_directories(files.evidence_root / evidence);
    result = materialize_max_validation_record(observation, files.inputs());
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxValidationMaterializationErrorCode::NonRegularFile);
    CHECK(result.error().subject == evidence);
    CHECK(std::string{max_validation_materialization_error_name(result.error().code)} ==
          "non_regular_file");
}

TEST_CASE("Max record materialization rejects symlink leaves and root escapes",
          "[max][validation-record][materialization][filesystem]") {
    const auto observation = observation_from(complete_record());
    TemporaryValidationFiles files;
    REQUIRE(files.populate(observation));
    const auto evidence = *observation.checks.front().evidence_relative_path;
    const auto evidence_path = files.evidence_root / evidence;
    const auto outside_file = files.root / "outside.json";
    REQUIRE(files.write(outside_file));

    std::filesystem::remove(evidence_path);
    std::error_code error;
    std::filesystem::create_symlink(outside_file, evidence_path, error);
    if (error) {
        WARN("platform did not permit symlink-leaf test: " << error.message());
        return;
    }
    auto result = materialize_max_validation_record(observation, files.inputs());
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxValidationMaterializationErrorCode::NonRegularFile);
    CHECK(result.error().subject == evidence);

    std::filesystem::remove(evidence_path);
    const auto first_component = evidence_path.parent_path();
    std::filesystem::remove_all(first_component);
    const auto outside_directory = files.root / "outside-evidence";
    REQUIRE(files.write(outside_directory / std::filesystem::path{evidence}.filename()));
    std::filesystem::create_directory_symlink(outside_directory, first_component, error);
    if (error) {
        WARN("platform did not permit root-escape test: " << error.message());
        return;
    }
    result = materialize_max_validation_record(observation, files.inputs());
    REQUIRE_FALSE(result);
    CHECK(result.error().code == MaxValidationMaterializationErrorCode::NonRegularFile);
    CHECK(result.error().subject == evidence);
}
