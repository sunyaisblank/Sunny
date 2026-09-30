/**
 * @file validation_record.cpp
 * @brief Strict Max/Max for Live named-host validation-record implementation
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <nlohmann/json.hpp>
#include <string_view>
#include <sunny/infrastructure/max/validation_record.hpp>
#include <sunny/version.hpp>
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

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool valid_text(const std::string& value, std::size_t maximum) {
    if (value.empty() || value.size() > maximum) return false;
    return std::none_of(value.begin(), value.end(), [](const unsigned char character) {
        return character < 0x20 || character == 0x7f;
    });
}

bool lowercase_hex(const std::string& value, std::size_t length) {
    if (value.size() != length ||
        !std::all_of(value.begin(), value.end(), [](const unsigned char character) {
            return (character >= '0' && character <= '9') || (character >= 'a' && character <= 'f');
        }))
        return false;
    return std::any_of(
        value.begin(), value.end(), [](const char character) { return character != '0'; });
}

bool valid_source_revision(const std::string& value) {
    return lowercase_hex(value, 40) || lowercase_hex(value, 64);
}

bool valid_relative_path(const std::string& value) {
    if (!valid_text(value, 1024) || value.front() == '/' || value.front() == '\\' ||
        value.find('\\') != std::string::npos || value.find(':') != std::string::npos)
        return false;
    std::size_t begin = 0;
    while (begin <= value.size()) {
        const auto end = value.find('/', begin);
        const auto component = value.substr(begin, end == std::string::npos ? end : end - begin);
        if (component.empty() || component == "." || component == "..") return false;
        if (end == std::string::npos) break;
        begin = end + 1;
    }
    return true;
}

bool ascii_digit(const char value) {
    return value >= '0' && value <= '9';
}

unsigned decimal_pair(const std::string& value, std::size_t offset) {
    return static_cast<unsigned>(value[offset] - '0') * 10U +
           static_cast<unsigned>(value[offset + 1] - '0');
}

unsigned decimal_year(const std::string& value) {
    unsigned result = 0;
    for (std::size_t index = 0; index < 4; ++index)
        result = result * 10U + static_cast<unsigned>(value[index] - '0');
    return result;
}

bool valid_utc_timestamp(const std::string& value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != 'Z')
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

const char* host_kind_name(MaxHostKind value) {
    switch (value) {
    case MaxHostKind::StandaloneMax:
        return "standalone_max";
    case MaxHostKind::MaxForLive:
        return "max_for_live";
    }
    return "";
}

std::optional<MaxHostKind> host_kind_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "standalone_max") return MaxHostKind::StandaloneMax;
    if (text == "max_for_live") return MaxHostKind::MaxForLive;
    return std::nullopt;
}

const char* operating_system_name(MaxOperatingSystem value) {
    switch (value) {
    case MaxOperatingSystem::MacOS:
        return "macos";
    case MaxOperatingSystem::Windows:
        return "windows";
    }
    return "";
}

std::optional<MaxOperatingSystem> operating_system_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "macos") return MaxOperatingSystem::MacOS;
    if (text == "windows") return MaxOperatingSystem::Windows;
    return std::nullopt;
}

const char* architecture_name(MaxArchitecture value) {
    switch (value) {
    case MaxArchitecture::X86_64:
        return "x86_64";
    case MaxArchitecture::Arm64:
        return "arm64";
    }
    return "";
}

std::optional<MaxArchitecture> architecture_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "x86_64") return MaxArchitecture::X86_64;
    if (text == "arm64") return MaxArchitecture::Arm64;
    return std::nullopt;
}

const char* outcome_name(MaxValidationOutcome value) {
    switch (value) {
    case MaxValidationOutcome::Passed:
        return "passed";
    case MaxValidationOutcome::Failed:
        return "failed";
    case MaxValidationOutcome::NotRun:
        return "not_run";
    case MaxValidationOutcome::NotApplicable:
        return "not_applicable";
    }
    return "";
}

std::optional<MaxValidationOutcome> outcome_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "passed") return MaxValidationOutcome::Passed;
    if (text == "failed") return MaxValidationOutcome::Failed;
    if (text == "not_run") return MaxValidationOutcome::NotRun;
    if (text == "not_applicable") return MaxValidationOutcome::NotApplicable;
    return std::nullopt;
}

const char* check_name(MaxValidationCheck value) {
    switch (value) {
    case MaxValidationCheck::PackageDiscovery:
        return "package_discovery";
    case MaxValidationCheck::PublicClassSurface:
        return "public_class_surface";
    case MaxValidationCheck::SignalTopology:
        return "signal_topology";
    case MaxValidationCheck::DspSetup:
        return "dsp_setup";
    case MaxValidationCheck::PerformCallback:
        return "perform_callback";
    case MaxValidationCheck::FiniteSignalOutput:
        return "finite_signal_output";
    case MaxValidationCheck::DisconnectedProcessing:
        return "disconnected_processing";
    case MaxValidationCheck::ReconnectContinuity:
        return "reconnect_continuity";
    case MaxValidationCheck::ControlDispatch:
        return "control_dispatch";
    case MaxValidationCheck::ActiveDspTeardown:
        return "active_dsp_teardown";
    case MaxValidationCheck::ItmScheduleFire:
        return "itm_schedule_fire";
    case MaxValidationCheck::ItmEqualTickOrdering:
        return "itm_equal_tick_ordering";
    case MaxValidationCheck::ItmClearReassign:
        return "itm_clear_reassign";
    case MaxValidationCheck::ReleaseVelocityFormatting:
        return "release_velocity_formatting";
    case MaxValidationCheck::SchedulerTiming:
        return "scheduler_timing";
    case MaxValidationCheck::DownstreamMidiDelivery:
        return "downstream_midi_delivery";
    case MaxValidationCheck::RenderedAudio:
        return "rendered_audio";
    case MaxValidationCheck::LiveTransportDiscontinuities:
        return "live_transport_discontinuities";
    case MaxValidationCheck::StandaloneTransportIdentity:
        return "standalone_transport_identity";
    }
    return "";
}

std::optional<MaxValidationCheck> check_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    const auto found = std::find_if(VALIDATION_CHECKS.begin(),
                                    VALIDATION_CHECKS.end(),
                                    [&](auto check) { return text == check_name(check); });
    if (found == VALIDATION_CHECKS.end()) return std::nullopt;
    return *found;
}

bool check_applicable(MaxHostKind host, MaxValidationCheck check) {
    if (check == MaxValidationCheck::LiveTransportDiscontinuities)
        return host == MaxHostKind::MaxForLive;
    if (check == MaxValidationCheck::StandaloneTransportIdentity)
        return host == MaxHostKind::StandaloneMax;
    return true;
}

bool valid_environment(const MaxValidationEnvironment& environment) {
    const bool live_host = environment.host_kind == MaxHostKind::MaxForLive;
    if (std::string_view(host_kind_name(environment.host_kind)).empty() ||
        std::string_view(operating_system_name(environment.operating_system)).empty() ||
        std::string_view(architecture_name(environment.architecture)).empty() ||
        !valid_text(environment.max_version, 128) || !valid_text(environment.audio_driver, 256) ||
        !std::isfinite(environment.sample_rate) || environment.sample_rate <= 0.0 ||
        environment.sample_rate > 1'000'000.0 || environment.io_vector_size < 1 ||
        environment.io_vector_size > 1'048'576 || environment.signal_vector_size < 1 ||
        environment.signal_vector_size > 1'048'576 ||
        (environment.operating_system == MaxOperatingSystem::Windows &&
         environment.architecture != MaxArchitecture::X86_64))
        return false;
    if (!live_host) return !environment.live_version && !environment.max_for_live_version;
    return environment.live_version && environment.max_for_live_version &&
           valid_text(*environment.live_version, 128) &&
           valid_text(*environment.max_for_live_version, 128);
}

std::string expected_artifact_path(MaxOperatingSystem operating_system, std::string_view name) {
    if (operating_system == MaxOperatingSystem::Windows)
        return "externals/" + std::string(name) + ".mxe64";
    return "externals/" + std::string(name) + ".mxo/Contents/MacOS/" + std::string(name);
}

bool valid_artifacts(MaxOperatingSystem operating_system,
                     const std::vector<MaxArtifactObservation>& artifacts) {
    if (artifacts.size() != OBJECT_NAMES.size()) return false;
    for (std::size_t index = 0; index < artifacts.size(); ++index) {
        const auto& artifact = artifacts[index];
        if (artifact.object_name != OBJECT_NAMES[index] ||
            artifact.package_relative_path !=
                expected_artifact_path(operating_system, OBJECT_NAMES[index]) ||
            !valid_relative_path(artifact.package_relative_path) ||
            !lowercase_hex(artifact.binary_sha256, 64) ||
            (artifact.instantiated && !artifact.discovered))
            return false;
    }
    return true;
}

bool valid_checks(MaxHostKind host, const std::vector<MaxCheckObservation>& checks) {
    if (checks.size() != VALIDATION_CHECKS.size()) return false;
    for (std::size_t index = 0; index < checks.size(); ++index) {
        const auto& observation = checks[index];
        if (observation.check != VALIDATION_CHECKS[index] ||
            std::string_view(outcome_name(observation.outcome)).empty() ||
            !valid_text(observation.summary, 2048))
            return false;
        const bool applicable = check_applicable(host, observation.check);
        if ((observation.outcome == MaxValidationOutcome::NotApplicable) != !applicable)
            return false;
        const bool has_evidence = observation.evidence_relative_path.has_value() &&
                                  observation.evidence_sha256.has_value();
        const bool observed = observation.outcome == MaxValidationOutcome::Passed ||
                              observation.outcome == MaxValidationOutcome::Failed;
        if (has_evidence != observed) return false;
        if (has_evidence && (!valid_relative_path(*observation.evidence_relative_path) ||
                             !lowercase_hex(*observation.evidence_sha256, 64)))
            return false;
        if (!has_evidence && (observation.evidence_relative_path || observation.evidence_sha256))
            return false;
    }
    return true;
}

bool valid_artifact_facts(MaxOperatingSystem operating_system,
                          const std::vector<MaxArtifactFact>& artifacts) {
    if (artifacts.size() != OBJECT_NAMES.size()) return false;
    for (std::size_t index = 0; index < artifacts.size(); ++index) {
        const auto& artifact = artifacts[index];
        if (artifact.object_name != OBJECT_NAMES[index] ||
            artifact.package_relative_path !=
                expected_artifact_path(operating_system, OBJECT_NAMES[index]) ||
            !valid_relative_path(artifact.package_relative_path) ||
            (artifact.instantiated && !artifact.discovered))
            return false;
    }
    return true;
}

bool valid_check_facts(MaxHostKind host, const std::vector<MaxCheckFact>& checks) {
    if (checks.size() != VALIDATION_CHECKS.size()) return false;
    for (std::size_t index = 0; index < checks.size(); ++index) {
        const auto& observation = checks[index];
        if (observation.check != VALIDATION_CHECKS[index] ||
            std::string_view(outcome_name(observation.outcome)).empty() ||
            !valid_text(observation.summary, 2048))
            return false;
        const bool applicable = check_applicable(host, observation.check);
        if ((observation.outcome == MaxValidationOutcome::NotApplicable) != !applicable)
            return false;
        const bool observed = observation.outcome == MaxValidationOutcome::Passed ||
                              observation.outcome == MaxValidationOutcome::Failed;
        if (observation.evidence_relative_path.has_value() != observed) return false;
        if (observation.evidence_relative_path &&
            !valid_relative_path(*observation.evidence_relative_path))
            return false;
    }
    return true;
}

bool observation_shape_valid(const MaxValidationObservation& observation) {
    return observation.schema_version == MAX_VALIDATION_OBSERVATION_SCHEMA_VERSION &&
           observation.sunny_version == sunny::SUNNY_VERSION &&
           observation.package_version == sunny::SUNNY_VERSION &&
           valid_source_revision(observation.source_revision) &&
           valid_utc_timestamp(observation.observed_at_utc) &&
           valid_text(observation.harness, 256) && valid_environment(observation.environment) &&
           valid_artifact_facts(observation.environment.operating_system, observation.artifacts) &&
           valid_check_facts(observation.environment.host_kind, observation.checks);
}

bool uniform_hex_placeholder(const std::string& value) {
    return !value.empty() && std::all_of(value.begin() + 1, value.end(), [&](const char character) {
        return character == value.front();
    });
}

bool has_example_provenance(const MaxValidationRecord& record) {
    if (record.harness.starts_with("example_") ||
        record.environment.max_version.starts_with("replace-with-") ||
        record.environment.audio_driver.starts_with("replace-with-") ||
        uniform_hex_placeholder(record.source_revision) ||
        uniform_hex_placeholder(record.package_archive_sha256) ||
        std::any_of(record.artifacts.begin(), record.artifacts.end(), [](const auto& artifact) {
            return uniform_hex_placeholder(artifact.binary_sha256);
        }))
        return true;
    return std::any_of(record.checks.begin(), record.checks.end(), [](const auto& check) {
        return check.evidence_sha256 && uniform_hex_placeholder(*check.evidence_sha256);
    });
}

bool record_shape_valid(const MaxValidationRecord& record) {
    return record.schema_version == MAX_VALIDATION_RECORD_SCHEMA_VERSION &&
           record.sunny_version == sunny::SUNNY_VERSION &&
           record.package_version == sunny::SUNNY_VERSION &&
           valid_source_revision(record.source_revision) &&
           lowercase_hex(record.package_archive_sha256, 64) &&
           valid_utc_timestamp(record.observed_at_utc) && valid_text(record.harness, 256) &&
           valid_environment(record.environment) &&
           valid_artifacts(record.environment.operating_system, record.artifacts) &&
           valid_checks(record.environment.host_kind, record.checks);
}

bool record_complete(const MaxValidationRecord& record) {
    if (!record_shape_valid(record) || has_example_provenance(record) ||
        !std::all_of(record.artifacts.begin(), record.artifacts.end(), [](const auto& artifact) {
            return artifact.discovered && artifact.instantiated;
        }))
        return false;
    return std::all_of(record.checks.begin(), record.checks.end(), [&](const auto& observation) {
        return check_applicable(record.environment.host_kind, observation.check)
                   ? observation.outcome == MaxValidationOutcome::Passed
                   : observation.outcome == MaxValidationOutcome::NotApplicable;
    });
}

json optional_string_json(const std::optional<std::string>& value) {
    return value ? json(*value) : json(nullptr);
}

std::optional<std::optional<std::string>> optional_string_from_json(const json& value) {
    if (value.is_null()) return std::optional<std::string>{};
    if (!value.is_string()) return std::nullopt;
    return std::optional<std::string>{value.get<std::string>()};
}

} // namespace

const char* max_validation_check_name(MaxValidationCheck check) noexcept {
    return check_name(check);
}

const char* max_host_kind_name(MaxHostKind value) noexcept {
    return host_kind_name(value);
}

const char* max_operating_system_name(MaxOperatingSystem value) noexcept {
    return operating_system_name(value);
}

const char* max_architecture_name(MaxArchitecture value) noexcept {
    return architecture_name(value);
}

bool MaxValidationRecord::complete() const {
    return record_complete(*this);
}

json max_validation_record_to_json(const MaxValidationRecord& record) {
    json artifacts = json::array();
    for (const auto& artifact : record.artifacts) {
        artifacts.push_back({{"object_name", artifact.object_name},
                             {"package_relative_path", artifact.package_relative_path},
                             {"binary_sha256", artifact.binary_sha256},
                             {"discovered", artifact.discovered},
                             {"instantiated", artifact.instantiated}});
    }
    json checks = json::array();
    for (const auto& observation : record.checks) {
        checks.push_back(
            {{"check", check_name(observation.check)},
             {"outcome", outcome_name(observation.outcome)},
             {"summary", observation.summary},
             {"evidence_relative_path", optional_string_json(observation.evidence_relative_path)},
             {"evidence_sha256", optional_string_json(observation.evidence_sha256)}});
    }
    const auto& environment = record.environment;
    return {{"schema_version", record.schema_version},
            {"sunny_version", record.sunny_version},
            {"package_version", record.package_version},
            {"source_revision", record.source_revision},
            {"package_archive_sha256", record.package_archive_sha256},
            {"observed_at_utc", record.observed_at_utc},
            {"harness", record.harness},
            {"environment",
             {{"host_kind", host_kind_name(environment.host_kind)},
              {"max_version", environment.max_version},
              {"live_version", optional_string_json(environment.live_version)},
              {"max_for_live_version", optional_string_json(environment.max_for_live_version)},
              {"operating_system", operating_system_name(environment.operating_system)},
              {"architecture", architecture_name(environment.architecture)},
              {"audio_driver", environment.audio_driver},
              {"sample_rate", environment.sample_rate},
              {"io_vector_size", environment.io_vector_size},
              {"signal_vector_size", environment.signal_vector_size},
              {"overdrive", environment.overdrive},
              {"scheduler_in_audio_interrupt", environment.scheduler_in_audio_interrupt}}},
            {"artifacts", std::move(artifacts)},
            {"checks", std::move(checks)},
            {"complete", record.complete()}};
}

json max_validation_observation_to_json(const MaxValidationObservation& observation) {
    json artifacts = json::array();
    for (const auto& artifact : observation.artifacts) {
        artifacts.push_back({{"object_name", artifact.object_name},
                             {"package_relative_path", artifact.package_relative_path},
                             {"discovered", artifact.discovered},
                             {"instantiated", artifact.instantiated}});
    }
    json checks = json::array();
    for (const auto& check : observation.checks) {
        checks.push_back(
            {{"check", check_name(check.check)},
             {"outcome", outcome_name(check.outcome)},
             {"summary", check.summary},
             {"evidence_relative_path", optional_string_json(check.evidence_relative_path)}});
    }
    const auto& environment = observation.environment;
    return {
        {"schema_version", observation.schema_version},
        {"sunny_version", observation.sunny_version},
        {"package_version", observation.package_version},
        {"source_revision", observation.source_revision},
        {"observed_at_utc", observation.observed_at_utc},
        {"harness", observation.harness},
        {"environment",
         {{"host_kind", host_kind_name(environment.host_kind)},
          {"max_version", environment.max_version},
          {"live_version", optional_string_json(environment.live_version)},
          {"max_for_live_version", optional_string_json(environment.max_for_live_version)},
          {"operating_system", operating_system_name(environment.operating_system)},
          {"architecture", architecture_name(environment.architecture)},
          {"audio_driver", environment.audio_driver},
          {"sample_rate", environment.sample_rate},
          {"io_vector_size", environment.io_vector_size},
          {"signal_vector_size", environment.signal_vector_size},
          {"overdrive", environment.overdrive},
          {"scheduler_in_audio_interrupt", environment.scheduler_in_audio_interrupt}}},
        {"artifacts", std::move(artifacts)},
        {"checks", std::move(checks)},
    };
}

sunny::core::Result<MaxValidationObservation>
max_validation_observation_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "sunny_version",
                           "package_version",
                           "source_revision",
                           "observed_at_utc",
                           "harness",
                           "environment",
                           "artifacts",
                           "checks"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            !value.at("sunny_version").is_string() || !value.at("package_version").is_string() ||
            !value.at("source_revision").is_string() || !value.at("observed_at_utc").is_string() ||
            !value.at("harness").is_string() || !value.at("artifacts").is_array() ||
            !value.at("checks").is_array())
            return std::unexpected(ErrorCode::FormatError);

        const auto& encoded_environment = value.at("environment");
        if (!exact_fields(encoded_environment,
                          {"host_kind",
                           "max_version",
                           "live_version",
                           "max_for_live_version",
                           "operating_system",
                           "architecture",
                           "audio_driver",
                           "sample_rate",
                           "io_vector_size",
                           "signal_vector_size",
                           "overdrive",
                           "scheduler_in_audio_interrupt"}) ||
            !encoded_environment.at("max_version").is_string() ||
            !encoded_environment.at("audio_driver").is_string() ||
            !encoded_environment.at("sample_rate").is_number_float() ||
            !encoded_environment.at("io_vector_size").is_number_unsigned() ||
            !encoded_environment.at("signal_vector_size").is_number_unsigned() ||
            !encoded_environment.at("overdrive").is_boolean() ||
            !encoded_environment.at("scheduler_in_audio_interrupt").is_boolean())
            return std::unexpected(ErrorCode::FormatError);

        auto host_kind = host_kind_from_json(encoded_environment.at("host_kind"));
        auto operating_system =
            operating_system_from_json(encoded_environment.at("operating_system"));
        auto architecture = architecture_from_json(encoded_environment.at("architecture"));
        auto live_version = optional_string_from_json(encoded_environment.at("live_version"));
        auto max_for_live_version =
            optional_string_from_json(encoded_environment.at("max_for_live_version"));
        if (!host_kind || !operating_system || !architecture || !live_version ||
            !max_for_live_version)
            return std::unexpected(ErrorCode::FormatError);

        const auto schema_version = value.at("schema_version").get<std::uint64_t>();
        const auto io_vector_size = encoded_environment.at("io_vector_size").get<std::uint64_t>();
        const auto signal_vector_size =
            encoded_environment.at("signal_vector_size").get<std::uint64_t>();
        if (schema_version > std::numeric_limits<std::uint32_t>::max() ||
            io_vector_size > std::numeric_limits<std::uint32_t>::max() ||
            signal_vector_size > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::FormatError);

        MaxValidationObservation observation;
        observation.schema_version = static_cast<std::uint32_t>(schema_version);
        observation.sunny_version = value.at("sunny_version").get<std::string>();
        observation.package_version = value.at("package_version").get<std::string>();
        observation.source_revision = value.at("source_revision").get<std::string>();
        observation.observed_at_utc = value.at("observed_at_utc").get<std::string>();
        observation.harness = value.at("harness").get<std::string>();
        observation.environment = {
            .host_kind = *host_kind,
            .max_version = encoded_environment.at("max_version").get<std::string>(),
            .live_version = std::move(*live_version),
            .max_for_live_version = std::move(*max_for_live_version),
            .operating_system = *operating_system,
            .architecture = *architecture,
            .audio_driver = encoded_environment.at("audio_driver").get<std::string>(),
            .sample_rate = encoded_environment.at("sample_rate").get<double>(),
            .io_vector_size = static_cast<std::uint32_t>(io_vector_size),
            .signal_vector_size = static_cast<std::uint32_t>(signal_vector_size),
            .overdrive = encoded_environment.at("overdrive").get<bool>(),
            .scheduler_in_audio_interrupt =
                encoded_environment.at("scheduler_in_audio_interrupt").get<bool>()};

        for (const auto& encoded : value.at("artifacts")) {
            if (!exact_fields(
                    encoded,
                    {"object_name", "package_relative_path", "discovered", "instantiated"}) ||
                !encoded.at("object_name").is_string() ||
                !encoded.at("package_relative_path").is_string() ||
                !encoded.at("discovered").is_boolean() || !encoded.at("instantiated").is_boolean())
                return std::unexpected(ErrorCode::FormatError);
            observation.artifacts.push_back(
                {.object_name = encoded.at("object_name").get<std::string>(),
                 .package_relative_path = encoded.at("package_relative_path").get<std::string>(),
                 .discovered = encoded.at("discovered").get<bool>(),
                 .instantiated = encoded.at("instantiated").get<bool>()});
        }

        for (const auto& encoded : value.at("checks")) {
            if (!exact_fields(encoded, {"check", "outcome", "summary", "evidence_relative_path"}) ||
                !encoded.at("summary").is_string())
                return std::unexpected(ErrorCode::FormatError);
            auto check = check_from_json(encoded.at("check"));
            auto outcome = outcome_from_json(encoded.at("outcome"));
            auto evidence_path = optional_string_from_json(encoded.at("evidence_relative_path"));
            if (!check || !outcome || !evidence_path)
                return std::unexpected(ErrorCode::FormatError);
            observation.checks.push_back({.check = *check,
                                          .outcome = *outcome,
                                          .summary = encoded.at("summary").get<std::string>(),
                                          .evidence_relative_path = std::move(*evidence_path)});
        }

        if (!observation_shape_valid(observation)) return std::unexpected(ErrorCode::FormatError);
        return observation;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

sunny::core::Result<MaxValidationRecord> max_validation_record_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "sunny_version",
                           "package_version",
                           "source_revision",
                           "package_archive_sha256",
                           "observed_at_utc",
                           "harness",
                           "environment",
                           "artifacts",
                           "checks",
                           "complete"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            !value.at("sunny_version").is_string() || !value.at("package_version").is_string() ||
            !value.at("source_revision").is_string() ||
            !value.at("package_archive_sha256").is_string() ||
            !value.at("observed_at_utc").is_string() || !value.at("harness").is_string() ||
            !value.at("artifacts").is_array() || !value.at("checks").is_array() ||
            !value.at("complete").is_boolean())
            return std::unexpected(ErrorCode::FormatError);

        const auto& encoded_environment = value.at("environment");
        if (!exact_fields(encoded_environment,
                          {"host_kind",
                           "max_version",
                           "live_version",
                           "max_for_live_version",
                           "operating_system",
                           "architecture",
                           "audio_driver",
                           "sample_rate",
                           "io_vector_size",
                           "signal_vector_size",
                           "overdrive",
                           "scheduler_in_audio_interrupt"}) ||
            !encoded_environment.at("max_version").is_string() ||
            !encoded_environment.at("audio_driver").is_string() ||
            !encoded_environment.at("sample_rate").is_number_float() ||
            !encoded_environment.at("io_vector_size").is_number_unsigned() ||
            !encoded_environment.at("signal_vector_size").is_number_unsigned() ||
            !encoded_environment.at("overdrive").is_boolean() ||
            !encoded_environment.at("scheduler_in_audio_interrupt").is_boolean())
            return std::unexpected(ErrorCode::FormatError);

        auto host_kind = host_kind_from_json(encoded_environment.at("host_kind"));
        auto operating_system =
            operating_system_from_json(encoded_environment.at("operating_system"));
        auto architecture = architecture_from_json(encoded_environment.at("architecture"));
        auto live_version = optional_string_from_json(encoded_environment.at("live_version"));
        auto max_for_live_version =
            optional_string_from_json(encoded_environment.at("max_for_live_version"));
        if (!host_kind || !operating_system || !architecture || !live_version ||
            !max_for_live_version)
            return std::unexpected(ErrorCode::FormatError);

        const auto io_vector_size = encoded_environment.at("io_vector_size").get<std::uint64_t>();
        const auto signal_vector_size =
            encoded_environment.at("signal_vector_size").get<std::uint64_t>();
        if (io_vector_size > std::numeric_limits<std::uint32_t>::max() ||
            signal_vector_size > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::FormatError);

        MaxValidationRecord record;
        const auto schema_version = value.at("schema_version").get<std::uint64_t>();
        if (schema_version > std::numeric_limits<std::uint32_t>::max())
            return std::unexpected(ErrorCode::FormatError);
        record.schema_version = static_cast<std::uint32_t>(schema_version);
        record.sunny_version = value.at("sunny_version").get<std::string>();
        record.package_version = value.at("package_version").get<std::string>();
        record.source_revision = value.at("source_revision").get<std::string>();
        record.package_archive_sha256 = value.at("package_archive_sha256").get<std::string>();
        record.observed_at_utc = value.at("observed_at_utc").get<std::string>();
        record.harness = value.at("harness").get<std::string>();
        record.environment = {
            .host_kind = *host_kind,
            .max_version = encoded_environment.at("max_version").get<std::string>(),
            .live_version = std::move(*live_version),
            .max_for_live_version = std::move(*max_for_live_version),
            .operating_system = *operating_system,
            .architecture = *architecture,
            .audio_driver = encoded_environment.at("audio_driver").get<std::string>(),
            .sample_rate = encoded_environment.at("sample_rate").get<double>(),
            .io_vector_size = static_cast<std::uint32_t>(io_vector_size),
            .signal_vector_size = static_cast<std::uint32_t>(signal_vector_size),
            .overdrive = encoded_environment.at("overdrive").get<bool>(),
            .scheduler_in_audio_interrupt =
                encoded_environment.at("scheduler_in_audio_interrupt").get<bool>()};

        for (const auto& encoded : value.at("artifacts")) {
            if (!exact_fields(encoded,
                              {"object_name",
                               "package_relative_path",
                               "binary_sha256",
                               "discovered",
                               "instantiated"}) ||
                !encoded.at("object_name").is_string() ||
                !encoded.at("package_relative_path").is_string() ||
                !encoded.at("binary_sha256").is_string() ||
                !encoded.at("discovered").is_boolean() || !encoded.at("instantiated").is_boolean())
                return std::unexpected(ErrorCode::FormatError);
            record.artifacts.push_back(
                {.object_name = encoded.at("object_name").get<std::string>(),
                 .package_relative_path = encoded.at("package_relative_path").get<std::string>(),
                 .binary_sha256 = encoded.at("binary_sha256").get<std::string>(),
                 .discovered = encoded.at("discovered").get<bool>(),
                 .instantiated = encoded.at("instantiated").get<bool>()});
        }

        for (const auto& encoded : value.at("checks")) {
            if (!exact_fields(
                    encoded,
                    {"check", "outcome", "summary", "evidence_relative_path", "evidence_sha256"}) ||
                !encoded.at("summary").is_string())
                return std::unexpected(ErrorCode::FormatError);
            auto check = check_from_json(encoded.at("check"));
            auto outcome = outcome_from_json(encoded.at("outcome"));
            auto evidence_path = optional_string_from_json(encoded.at("evidence_relative_path"));
            auto evidence_sha256 = optional_string_from_json(encoded.at("evidence_sha256"));
            if (!check || !outcome || !evidence_path || !evidence_sha256)
                return std::unexpected(ErrorCode::FormatError);
            record.checks.push_back({.check = *check,
                                     .outcome = *outcome,
                                     .summary = encoded.at("summary").get<std::string>(),
                                     .evidence_relative_path = std::move(*evidence_path),
                                     .evidence_sha256 = std::move(*evidence_sha256)});
        }

        if (!record_shape_valid(record) || record.complete() != value.at("complete").get<bool>())
            return std::unexpected(ErrorCode::FormatError);
        return record;
    } catch (const json::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

} // namespace sunny::infrastructure
