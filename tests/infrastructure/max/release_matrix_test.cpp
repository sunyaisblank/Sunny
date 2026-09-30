/**
 * @file release_matrix_test.cpp
 * @brief Closed Max release-matrix aggregation and adversarial tests
 */

#include <array>
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <string>
#include <sunny/infrastructure/max/release_matrix.hpp>
#include <sunny/version.hpp>

using nlohmann::json;
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

constexpr std::array<const char*, 6> RECORD_PATHS{
    "records/macos-x86_64-standalone-max.json",
    "records/macos-x86_64-max-for-live.json",
    "records/macos-arm64-standalone-max.json",
    "records/macos-arm64-max-for-live.json",
    "records/windows-x86_64-standalone-max.json",
    "records/windows-x86_64-max-for-live.json",
};

std::filesystem::path repository_root() {
    return std::filesystem::path{__FILE__}.parent_path().parent_path().parent_path().parent_path();
}

std::string hash(char digit) {
    return std::string(63, digit) + (digit == 'f' ? "e" : "f");
}

MaxValidationRecord complete_record(MaxOperatingSystem operating_system,
                                    MaxArchitecture architecture,
                                    MaxHostKind host_kind,
                                    std::size_t target_index) {
    MaxValidationRecord record;
    record.sunny_version = sunny::SUNNY_VERSION;
    record.package_version = sunny::SUNNY_VERSION;
    record.source_revision = "0123456789abcdef0123456789abcdef01234567";
    record.package_archive_sha256 = hash(static_cast<char>('2' + target_index));
    record.observed_at_utc = "2026-08-31T10:00:00Z";
    record.harness = "Sunny release validation harness 1";
    record.environment.host_kind = host_kind;
    record.environment.max_version = "9.0.5";
    record.environment.operating_system = operating_system;
    record.environment.architecture = architecture;
    record.environment.audio_driver =
        operating_system == MaxOperatingSystem::MacOS ? "Core Audio" : "ASIO fixture";
    record.environment.sample_rate = 48'000.0;
    record.environment.io_vector_size = 512;
    record.environment.signal_vector_size = 64;
    record.environment.overdrive = true;
    record.environment.scheduler_in_audio_interrupt = true;
    if (host_kind == MaxHostKind::MaxForLive) {
        record.environment.live_version = "12.3.1";
        record.environment.max_for_live_version = "9.0.5";
    }

    constexpr std::array<const char*, 5> objects{
        "sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"};
    for (std::size_t index = 0; index < objects.size(); ++index) {
        const auto name = std::string{objects[index]};
        const auto path = operating_system == MaxOperatingSystem::MacOS
                              ? "externals/" + name + ".mxo/Contents/MacOS/" + name
                              : "externals/" + name + ".mxe64";
        record.artifacts.push_back(
            {.object_name = name,
             .package_relative_path = path,
             .binary_sha256 = hash(static_cast<char>('a' + (target_index * 5 + index) % 6)),
             .discovered = true,
             .instantiated = true});
    }

    for (std::size_t index = 0; index < CHECKS.size(); ++index) {
        const auto check = CHECKS[index];
        const bool applicable = (check != MaxValidationCheck::LiveTransportDiscontinuities ||
                                 host_kind == MaxHostKind::MaxForLive) &&
                                (check != MaxValidationCheck::StandaloneTransportIdentity ||
                                 host_kind == MaxHostKind::StandaloneMax);
        MaxCheckObservation observation;
        observation.check = check;
        observation.outcome =
            applicable ? MaxValidationOutcome::Passed : MaxValidationOutcome::NotApplicable;
        observation.summary =
            applicable ? "Observed expected host behavior" : "Check belongs to the other host kind";
        if (applicable) {
            observation.evidence_relative_path =
                "evidence/check-" + std::to_string(index) + ".json";
            observation.evidence_sha256 = hash(static_cast<char>('a' + index % 6));
        }
        record.checks.push_back(std::move(observation));
    }
    REQUIRE(record.complete());
    return record;
}

class MatrixFixture final {
  public:
    MatrixFixture() {
        static std::atomic_uint64_t sequence{};
        const auto tick = std::chrono::steady_clock::now().time_since_epoch().count();
        root =
            std::filesystem::temp_directory_path() / ("sunny-max-matrix-" + std::to_string(tick) +
                                                      "-" + std::to_string(sequence.fetch_add(1)));
        std::filesystem::create_directories(root / "records");

        write_record(
            0,
            complete_record(
                MaxOperatingSystem::MacOS, MaxArchitecture::X86_64, MaxHostKind::StandaloneMax, 0));
        write_record(
            1,
            complete_record(
                MaxOperatingSystem::MacOS, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 0));
        write_record(
            2,
            complete_record(
                MaxOperatingSystem::MacOS, MaxArchitecture::Arm64, MaxHostKind::StandaloneMax, 1));
        write_record(
            3,
            complete_record(
                MaxOperatingSystem::MacOS, MaxArchitecture::Arm64, MaxHostKind::MaxForLive, 1));
        write_record(4,
                     complete_record(MaxOperatingSystem::Windows,
                                     MaxArchitecture::X86_64,
                                     MaxHostKind::StandaloneMax,
                                     2));
        write_record(
            5,
            complete_record(
                MaxOperatingSystem::Windows, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 2));
    }

    ~MatrixFixture() {
        std::error_code ignored;
        std::filesystem::remove_all(root, ignored);
    }

    MatrixFixture(const MatrixFixture&) = delete;
    MatrixFixture& operator=(const MatrixFixture&) = delete;

    void write_record(std::size_t index, const MaxValidationRecord& record, int indent = 2) const {
        std::ofstream output{root / RECORD_PATHS[index], std::ios::binary};
        output << max_validation_record_to_json(record).dump(indent) << '\n';
        REQUIRE(output.good());
    }

    [[nodiscard]] json manifest() const {
        std::ifstream input{repository_root() /
                            "max-package/misc/validation/max-release-matrix.json"};
        REQUIRE(input.good());
        return json::parse(input);
    }

    [[nodiscard]] MaxReleaseMatrix materialize() const {
        auto matrix =
            materialize_max_release_matrix(manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, root);
        REQUIRE(matrix);
        return std::move(*matrix);
    }

    std::filesystem::path root;
};

void require_error(const MaxReleaseMatrixResult& result, MaxReleaseMatrixErrorCode code) {
    REQUIRE_FALSE(result);
    CHECK(result.error().code == code);
}

} // namespace

TEST_CASE("the Max release matrix closes all six supported host cells",
          "[max][release-matrix][roundtrip]") {
    MatrixFixture fixture;
    const auto matrix = fixture.materialize();
    REQUIRE(matrix.complete());
    REQUIRE(matrix.cells.size() == 6);
    CHECK(matrix.cells[0].record.environment.host_kind == MaxHostKind::StandaloneMax);
    CHECK(matrix.cells[1].record.environment.host_kind == MaxHostKind::MaxForLive);
    CHECK(matrix.cells[2].record.environment.architecture == MaxArchitecture::Arm64);
    CHECK(matrix.cells[4].record.environment.operating_system == MaxOperatingSystem::Windows);

    const auto encoded = max_release_matrix_to_json(matrix);
    CHECK(encoded.at("complete") == true);
    const auto parsed = max_release_matrix_from_json(encoded);
    REQUIRE(parsed);
    CHECK(max_release_matrix_to_json(*parsed) == encoded);
    CHECK(verify_max_release_matrix_files(
        *parsed, fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root));
}

TEST_CASE("the release matrix rejects missing incomplete and misidentified cells",
          "[max][release-matrix][cells]") {
    MatrixFixture fixture;
    std::filesystem::remove(fixture.root / RECORD_PATHS[5]);
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::FileUnavailable);

    auto incomplete = complete_record(
        MaxOperatingSystem::Windows, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 2);
    incomplete.checks[0].outcome = MaxValidationOutcome::NotRun;
    incomplete.checks[0].evidence_relative_path.reset();
    incomplete.checks[0].evidence_sha256.reset();
    REQUIRE_FALSE(incomplete.complete());
    fixture.write_record(5, incomplete);
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::IncompleteRecord);

    fixture.write_record(
        5,
        complete_record(
            MaxOperatingSystem::Windows, MaxArchitecture::X86_64, MaxHostKind::StandaloneMax, 2));
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::CellMismatch);
}

TEST_CASE("one release matrix cannot combine revisions or packages across a native target",
          "[max][release-matrix][coherence]") {
    MatrixFixture fixture;
    auto live = complete_record(
        MaxOperatingSystem::MacOS, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 0);
    live.source_revision = "1123456789abcdef0123456789abcdef01234567";
    fixture.write_record(1, live);
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::RevisionMismatch);

    live = complete_record(
        MaxOperatingSystem::MacOS, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 0);
    live.package_archive_sha256 = hash('9');
    fixture.write_record(1, live);
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::PackageMismatch);

    live = complete_record(
        MaxOperatingSystem::MacOS, MaxArchitecture::X86_64, MaxHostKind::MaxForLive, 0);
    live.artifacts[2].binary_sha256 = hash('f');
    fixture.write_record(1, live);
    require_error(materialize_max_release_matrix(
                      fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
                  MaxReleaseMatrixErrorCode::PackageMismatch);
}

TEST_CASE("release matrix JSON rejects extensions reordering and forged derivations",
          "[max][release-matrix][schema]") {
    MatrixFixture fixture;
    const auto canonical = max_release_matrix_to_json(fixture.materialize());

    auto value = canonical;
    value["extra"] = true;
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::InvalidMatrix);
    value = canonical;
    std::swap(value["cells"][0], value["cells"][1]);
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::CellMismatch);
    value = canonical;
    value["cells"][0]["record_relative_path"] = "../outside.json";
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::CellMismatch);
    value = canonical;
    value["cells"][0]["record_sha256"] = std::string(64, '0');
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::InvalidMatrix);
    value = canonical;
    value["cells"][0]["record"]["complete"] = false;
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::InvalidRecord);
    value = canonical;
    value["complete"] = false;
    require_error(max_release_matrix_from_json(value), MaxReleaseMatrixErrorCode::InvalidMatrix);
}

TEST_CASE("release matrix verification binds authored manifest and exact record bytes",
          "[max][release-matrix][verification]") {
    MatrixFixture fixture;
    const auto matrix = fixture.materialize();

    require_error(
        materialize_max_release_matrix(fixture.manifest(), std::string(64, 'a'), fixture.root),
        MaxReleaseMatrixErrorCode::ManifestDigestMismatch);

    auto manifest = fixture.manifest();
    manifest["cells"][0]["host_kind"] = "max_for_live";
    require_error(
        materialize_max_release_matrix(manifest, MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root),
        MaxReleaseMatrixErrorCode::InvalidManifest);

    auto first = matrix.cells[0].record;
    fixture.write_record(0, first, 4);
    const auto verified = verify_max_release_matrix_files(
        matrix, fixture.manifest(), MAX_RELEASE_MATRIX_MANIFEST_SHA256, fixture.root);
    REQUIRE_FALSE(verified);
    CHECK(verified.error().code == MaxReleaseMatrixErrorCode::RecordDigestMismatch);
}

TEST_CASE("the authored release matrix manifest matches the compiled digest authority",
          "[max][release-matrix][manifest]") {
    const auto path = repository_root() / "max-package/misc/validation/max-release-matrix.json";
    const auto digest = max_validation_regular_file_sha256(path, "max-release-matrix.json");
    REQUIRE(digest);
    CHECK(*digest == MAX_RELEASE_MATRIX_MANIFEST_SHA256);

    MatrixFixture fixture;
    const auto matrix = materialize_max_release_matrix(fixture.manifest(), *digest, fixture.root);
    REQUIRE(matrix);
    CHECK(matrix->complete());
}
