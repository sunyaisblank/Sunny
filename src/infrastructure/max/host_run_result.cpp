/**
 * @file host_run_result.cpp
 * @brief Fail-closed residual Max host evidence evaluation
 */

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <limits>
#include <map>
#include <nlohmann/json.hpp>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <sunny/infrastructure/max/host_run_result.hpp>
#include <sunny/infrastructure/max/max_test_result.hpp>
#include <utility>
#include <vector>

namespace sunny::infrastructure {

namespace {

using json = nlohmann::json;
using Error = MaxHostRunApplicationError;
using ErrorCode = MaxHostRunApplicationErrorCode;

constexpr std::array<std::string_view, 5> OBJECT_NAMES{
    "sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~", "sunny.events"};
constexpr std::array<std::string_view, 12> FILE_ROLES{
    "teardown_counts",
    "teardown_console",
    "teardown_memory_diagnostic",
    "scheduler_settings",
    "scheduler_offsets",
    "scheduler_audio",
    "midi_ports",
    "midi_received",
    "midi_receiver_transcript",
    "render_settings",
    "render_audio",
    "render_comparison_transcript",
};
constexpr std::array<std::string_view, 12> FILE_PATHS{
    "active-dsp-teardown/cycle-counts.json",
    "active-dsp-teardown/host-console.txt",
    "active-dsp-teardown/memory-diagnostic.txt",
    "scheduler-timing/scenario-settings.json",
    "scheduler-timing/impulse-offsets.json",
    "scheduler-timing/timing-capture.wav",
    "downstream-midi/ports.json",
    "downstream-midi/received-midi.json",
    "downstream-midi/receiver-transcript.txt",
    "rendered-audio/render-settings.json",
    "rendered-audio/sunny-render.wav",
    "rendered-audio/comparison.json",
};
constexpr std::array<std::string_view, 5> LIVE_FILE_ROLES{"live_saved_device",
                                                          "live_assertion_contract",
                                                          "live_shared_assertions",
                                                          "live_scenario_results",
                                                          "live_console"};
constexpr std::array<std::string_view, 5> LIVE_FILE_PATHS{
    "live-transport-discontinuities/sunny-validation-device.amxd",
    "live-transport-discontinuities/max-test-harness.json",
    "live-transport-discontinuities/shared-assertions.json",
    "live-transport-discontinuities/scenario-results.json",
    "live-transport-discontinuities/host-console.txt",
};
constexpr std::array<MaxValidationCheck, 13> LIVE_SHARED_CHECKS{
    MaxValidationCheck::PackageDiscovery,
    MaxValidationCheck::PublicClassSurface,
    MaxValidationCheck::SignalTopology,
    MaxValidationCheck::DspSetup,
    MaxValidationCheck::PerformCallback,
    MaxValidationCheck::FiniteSignalOutput,
    MaxValidationCheck::DisconnectedProcessing,
    MaxValidationCheck::ReconnectContinuity,
    MaxValidationCheck::ControlDispatch,
    MaxValidationCheck::ItmScheduleFire,
    MaxValidationCheck::ItmEqualTickOrdering,
    MaxValidationCheck::ItmClearReassign,
    MaxValidationCheck::ReleaseVelocityFormatting,
};
constexpr std::array<std::string_view, 13> LIVE_SHARED_CHECK_NAMES{
    "package_discovery",
    "public_class_surface",
    "signal_topology",
    "dsp_setup",
    "perform_callback",
    "finite_signal_output",
    "disconnected_processing",
    "reconnect_continuity",
    "control_dispatch",
    "itm_schedule_fire",
    "itm_equal_tick_ordering",
    "itm_clear_reassign",
    "release_velocity_formatting",
};
constexpr std::array<std::string_view, 19> VALIDATION_CHECK_NAMES{
    "package_discovery",
    "public_class_surface",
    "signal_topology",
    "dsp_setup",
    "perform_callback",
    "finite_signal_output",
    "disconnected_processing",
    "reconnect_continuity",
    "control_dispatch",
    "active_dsp_teardown",
    "itm_schedule_fire",
    "itm_equal_tick_ordering",
    "itm_clear_reassign",
    "release_velocity_formatting",
    "scheduler_timing",
    "downstream_midi_delivery",
    "rendered_audio",
    "live_transport_discontinuities",
    "standalone_transport_identity",
};
constexpr std::array<std::string_view, 5> SCHEDULER_SCENARIOS{
    "supported_enabled_timely",
    "supported_without_siai",
    "supported_without_overdrive",
    "late_absolute_target",
    "ordinary_message_consumer",
};
constexpr std::array<std::string_view, 6> LIVE_SCENARIOS{
    "stop_start",
    "seek_forward_and_backward",
    "arrangement_loop",
    "tempo_change",
    "preview_enter_exit",
    "scheduler_settings",
};
constexpr std::array<std::string_view, 4> RENDER_OBJECTS{
    "sunny.lfo~", "sunny.adsr~", "sunny.hold~", "sunny.clock~"};
constexpr std::array<std::string_view, 4> RENDER_CONFIGURATIONS{
    "frequency 0, saw, reset",
    "attack 0, decay 0, sustain 0.375, release 0, reset, gate 1",
    "value -0.25",
    "tempo 120, position 0, play",
};

Error failure(ErrorCode code, std::string subject) {
    return {.code = code, .subject = std::move(subject)};
}

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool lowercase_hex(std::string_view value, std::size_t length) {
    return value.size() == length &&
           std::all_of(value.begin(),
                       value.end(),
                       [](const unsigned char character) {
                           return (character >= '0' && character <= '9') ||
                                  (character >= 'a' && character <= 'f');
                       }) &&
           std::any_of(
               value.begin(), value.end(), [](const char character) { return character != '0'; });
}

bool valid_text(const json& value, std::size_t maximum = 4096) {
    if (!value.is_string()) return false;
    const auto& text = value.get_ref<const std::string&>();
    return !text.empty() && text.size() <= maximum &&
           std::none_of(text.begin(), text.end(), [](const unsigned char character) {
               return character < 0x20 || character == 0x7f;
           });
}

bool valid_protocol_identifier(const json& value) {
    if (!valid_text(value, 128)) return false;
    const auto& text = value.get_ref<const std::string&>();
    return std::all_of(text.begin(), text.end(), [](const unsigned char character) {
        return (character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') ||
               (character >= '0' && character <= '9') || character == '.' || character == '_' ||
               character == ':' || character == '-';
    });
}

bool finite_number(const json& value) {
    return value.is_number() && std::isfinite(value.get<double>());
}

bool ascii_digit(char value) {
    return value >= '0' && value <= '9';
}

unsigned decimal_pair(std::string_view value, std::size_t offset) {
    return static_cast<unsigned>(value[offset] - '0') * 10U +
           static_cast<unsigned>(value[offset + 1] - '0');
}

bool valid_utc_timestamp(std::string_view value) {
    if (value.size() != 20 || value[4] != '-' || value[7] != '-' || value[10] != 'T' ||
        value[13] != ':' || value[16] != ':' || value[19] != 'Z')
        return false;
    constexpr std::array<std::size_t, 14> digits{0, 1, 2, 3, 5, 6, 8, 9, 11, 12, 14, 15, 17, 18};
    if (!std::all_of(digits.begin(), digits.end(), [&](const auto offset) {
            return ascii_digit(value[offset]);
        }))
        return false;
    unsigned year = 0;
    for (std::size_t index = 0; index < 4; ++index)
        year = year * 10U + static_cast<unsigned>(value[index] - '0');
    const auto month = decimal_pair(value, 5);
    const auto day = decimal_pair(value, 8);
    const auto hour = decimal_pair(value, 11);
    const auto minute = decimal_pair(value, 14);
    const auto second = decimal_pair(value, 17);
    if (year < 1970 || month < 1 || month > 12 || hour > 23 || minute > 59 || second > 60)
        return false;
    constexpr std::array<unsigned, 12> month_days{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    auto maximum_day = month_days[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) maximum_day = 29;
    return day >= 1 && day <= maximum_day;
}

bool environment_shape_valid(const json& environment) {
    if (!exact_fields(environment,
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
        !valid_text(environment.at("max_version")) || !valid_text(environment.at("audio_driver")) ||
        !finite_number(environment.at("sample_rate")) ||
        environment.at("sample_rate").get<double>() <= 0.0 ||
        environment.at("sample_rate").get<double>() > 1'000'000.0 ||
        !environment.at("io_vector_size").is_number_unsigned() ||
        environment.at("io_vector_size").get<std::uint64_t>() == 0 ||
        environment.at("io_vector_size").get<std::uint64_t>() > 1'048'576 ||
        !environment.at("signal_vector_size").is_number_unsigned() ||
        environment.at("signal_vector_size").get<std::uint64_t>() == 0 ||
        environment.at("signal_vector_size").get<std::uint64_t>() > 1'048'576 ||
        !environment.at("overdrive").is_boolean() ||
        !environment.at("scheduler_in_audio_interrupt").is_boolean())
        return false;
    const bool standalone = environment.at("host_kind") == "standalone_max";
    const bool live = environment.at("host_kind") == "max_for_live";
    if ((!standalone && !live) ||
        (environment.at("operating_system") != "macos" &&
         environment.at("operating_system") != "windows") ||
        (environment.at("architecture") != "x86_64" && environment.at("architecture") != "arm64"))
        return false;
    if (environment.at("operating_system") == "windows" &&
        environment.at("architecture") != "x86_64")
        return false;
    if (standalone)
        return environment.at("live_version").is_null() &&
               environment.at("max_for_live_version").is_null();
    return valid_text(environment.at("live_version")) &&
           valid_text(environment.at("max_for_live_version"));
}

bool plan_contract_valid(const json& plan, std::string_view digest) {
    if (digest != MAX_HOST_RUN_PLAN_SHA256 || !plan.is_object() ||
        plan.value("schema_version", 0) != 1 ||
        plan.value("plan_name", std::string{}) != "sunny-max-named-host-run" ||
        plan.value("sunny_version", std::string{}) != "0.4.0" ||
        !plan.contains("max_for_live_assertions") || !plan.contains("normalized_residual_result") ||
        !plan.contains("residual_checks"))
        return false;
    const auto& live_assertions = plan.at("max_for_live_assertions");
    if (!exact_fields(live_assertions,
                      {"device_kind",
                       "source_harness",
                       "source_harness_sha256",
                       "source_generator",
                       "console_exporter",
                       "pinned_console_source_sha256",
                       "protocol",
                       "assertion_count",
                       "shared_checks"}) ||
        live_assertions.at("device_kind") != "max_audio_effect" ||
        live_assertions.at("source_harness") != "max-test-harness.json" ||
        live_assertions.at("source_harness_sha256") != MAX_TEST_HARNESS_MANIFEST_SHA256 ||
        live_assertions.at("source_generator") != "prepare-m4l-device-source.py" ||
        live_assertions.at("console_exporter") != "export-m4l-assertions.py" ||
        live_assertions.at("pinned_console_source_sha256") !=
            "fe8084df5a1350380f18d9299714865d6a68d2e28bb3cfbe5fbf1398f2df3028" ||
        live_assertions.at("assertion_count") != 64 ||
        !live_assertions.at("shared_checks").is_array() ||
        live_assertions.at("shared_checks").size() != LIVE_SHARED_CHECK_NAMES.size())
        return false;
    const auto& protocol = live_assertions.at("protocol");
    if (!exact_fields(protocol, {"begin", "assertion", "end"}) ||
        protocol.at("begin") != "SUNNY_M4L_ASSERTIONS_BEGIN|<run-id>" ||
        protocol.at("assertion") != "SUNNY_M4L_ASSERTION|<run-id>|<name>|Pass-or-Fail" ||
        protocol.at("end") != "SUNNY_M4L_ASSERTIONS_END|<run-id>|64")
        return false;
    for (std::size_t index = 0; index < LIVE_SHARED_CHECK_NAMES.size(); ++index) {
        if (live_assertions.at("shared_checks").at(index) != LIVE_SHARED_CHECK_NAMES[index])
            return false;
    }
    const auto& contract = plan.at("normalized_residual_result");
    if (!exact_fields(contract,
                      {"schema_version",
                       "indexer",
                       "evidence_relative_path",
                       "file_roles",
                       "max_for_live_file_roles"}) ||
        contract.at("schema_version") != 1 || contract.at("indexer") != "index-max-host-run.py" ||
        contract.at("evidence_relative_path") != "host-run-result.json" ||
        !contract.at("file_roles").is_array() ||
        contract.at("file_roles").size() != FILE_ROLES.size() ||
        !contract.at("max_for_live_file_roles").is_array() ||
        contract.at("max_for_live_file_roles").size() != LIVE_FILE_ROLES.size())
        return false;
    for (std::size_t index = 0; index < FILE_ROLES.size(); ++index) {
        const auto& entry = contract.at("file_roles").at(index);
        if (!exact_fields(entry, {"role", "relative_path"}) ||
            entry.at("role") != FILE_ROLES[index] || entry.at("relative_path") != FILE_PATHS[index])
            return false;
    }
    for (std::size_t index = 0; index < LIVE_FILE_ROLES.size(); ++index) {
        const auto& entry = contract.at("max_for_live_file_roles").at(index);
        if (!exact_fields(entry, {"role", "relative_path"}) ||
            entry.at("role") != LIVE_FILE_ROLES[index] ||
            entry.at("relative_path") != LIVE_FILE_PATHS[index])
            return false;
    }
    const auto& checks = plan.at("residual_checks");
    return checks.is_array() && checks.size() == 4 &&
           checks.at(0).value("check", std::string{}) == "active_dsp_teardown" &&
           checks.at(0).value("minimum_cycles_per_object", 0) == 1000 &&
           checks.at(1).value("check", std::string{}) == "scheduler_timing" &&
           checks.at(1).value("tempo_bpm", 0.0) == 120.0 &&
           checks.at(1).value("ticks_per_quarter", 0) == 480 &&
           checks.at(1).value("event_count", 0) == 32 &&
           checks.at(1).value("tick_interval", 0) == 48 &&
           checks.at(1).value("expected_sample_interval_at_baseline", 0) == 2400 &&
           checks.at(2).value("check", std::string{}) == "downstream_midi_delivery" &&
           checks.at(2).value("channel", 0) == 1 &&
           checks.at(3).value("check", std::string{}) == "rendered_audio" &&
           checks.at(3).value("minimum_frames", 0) == 4096;
}

struct IndexedFile {
    std::string role;
    std::string relative_path;
    std::string sha256;
};

struct ResultIndex {
    std::string plan_sha256;
    std::string sunny_version;
    std::string package_version;
    std::string source_revision;
    std::string observed_at_utc;
    json environment;
    std::vector<IndexedFile> files;
};

std::optional<ResultIndex> result_index_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "plan_sha256",
                           "sunny_version",
                           "package_version",
                           "source_revision",
                           "observed_at_utc",
                           "environment",
                           "files"}) ||
            value.at("schema_version") != MAX_HOST_RUN_RESULT_SCHEMA_VERSION ||
            !value.at("plan_sha256").is_string() ||
            !lowercase_hex(value.at("plan_sha256").get_ref<const std::string&>(), 64) ||
            value.at("sunny_version") != "0.4.0" || value.at("package_version") != "0.4.0" ||
            !value.at("source_revision").is_string() ||
            (!lowercase_hex(value.at("source_revision").get_ref<const std::string&>(), 40) &&
             !lowercase_hex(value.at("source_revision").get_ref<const std::string&>(), 64)) ||
            !value.at("observed_at_utc").is_string() ||
            !valid_utc_timestamp(value.at("observed_at_utc").get_ref<const std::string&>()) ||
            !environment_shape_valid(value.at("environment")) || !value.at("files").is_array())
            return std::nullopt;
        const bool live = value.at("environment").at("host_kind") == "max_for_live";
        const auto expected_size = FILE_ROLES.size() + (live ? LIVE_FILE_ROLES.size() : 0);
        if (value.at("files").size() != expected_size) return std::nullopt;
        ResultIndex result{.plan_sha256 = value.at("plan_sha256").get<std::string>(),
                           .sunny_version = value.at("sunny_version").get<std::string>(),
                           .package_version = value.at("package_version").get<std::string>(),
                           .source_revision = value.at("source_revision").get<std::string>(),
                           .observed_at_utc = value.at("observed_at_utc").get<std::string>(),
                           .environment = value.at("environment"),
                           .files = {}};
        result.files.reserve(expected_size);
        for (std::size_t index = 0; index < expected_size; ++index) {
            const auto& encoded = value.at("files").at(index);
            const auto expected_role = index < FILE_ROLES.size()
                                           ? FILE_ROLES[index]
                                           : LIVE_FILE_ROLES[index - FILE_ROLES.size()];
            const auto expected_path = index < FILE_PATHS.size()
                                           ? FILE_PATHS[index]
                                           : LIVE_FILE_PATHS[index - FILE_PATHS.size()];
            if (!exact_fields(encoded, {"role", "relative_path", "sha256"}) ||
                encoded.at("role") != expected_role ||
                encoded.at("relative_path") != expected_path || !encoded.at("sha256").is_string() ||
                !lowercase_hex(encoded.at("sha256").get_ref<const std::string&>(), 64))
                return std::nullopt;
            result.files.push_back({.role = encoded.at("role").get<std::string>(),
                                    .relative_path = encoded.at("relative_path").get<std::string>(),
                                    .sha256 = encoded.at("sha256").get<std::string>()});
        }
        return result;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

std::expected<json, Error> read_evidence_json(const std::filesystem::path& evidence_root,
                                              const IndexedFile& file) {
    std::ifstream input{evidence_root / file.relative_path, std::ios::binary};
    if (!input) return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
    try {
        auto value = json::parse(input);
        const auto digest = max_validation_evidence_file_sha256(evidence_root, file.relative_path);
        if (!digest)
            return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
        if (*digest != file.sha256)
            return std::unexpected(failure(ErrorCode::EvidenceDigestMismatch, file.relative_path));
        return value;
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::MeasurementContractMismatch, file.relative_path));
    }
}

std::expected<void, Error> require_nonempty_evidence(const std::filesystem::path& evidence_root,
                                                     const IndexedFile& file) {
    std::ifstream input{evidence_root / file.relative_path, std::ios::binary};
    char byte = 0;
    if (!input || !input.get(byte))
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":empty"));
    const auto digest = max_validation_evidence_file_sha256(evidence_root, file.relative_path);
    if (!digest)
        return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
    if (*digest != file.sha256)
        return std::unexpected(failure(ErrorCode::EvidenceDigestMismatch, file.relative_path));
    return {};
}

std::uint16_t little_u16(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint16_t>(bytes[offset]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1]) << 8U);
}

std::uint32_t little_u32(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
}

std::uint64_t little_u64(std::span<const std::uint8_t> bytes, std::size_t offset) {
    return static_cast<std::uint64_t>(little_u32(bytes, offset)) |
           (static_cast<std::uint64_t>(little_u32(bytes, offset + 4)) << 32U);
}

bool fourcc(std::span<const std::uint8_t> bytes, std::size_t offset, std::string_view value) {
    return value.size() == 4 && offset <= bytes.size() && bytes.size() - offset >= 4 &&
           std::equal(
               value.begin(), value.end(), bytes.begin() + static_cast<std::ptrdiff_t>(offset));
}

struct WaveCapture {
    bool ieee_float = false;
    std::uint16_t channels = 0;
    std::uint32_t sample_rate = 0;
    std::uint16_t bits_per_sample = 0;
    std::size_t frame_count = 0;
    std::vector<double> samples;
};

std::expected<WaveCapture, Error> read_wave_capture(const std::filesystem::path& evidence_root,
                                                    const IndexedFile& file) {
    const auto path = evidence_root / file.relative_path;
    std::ifstream input{path, std::ios::binary | std::ios::ate};
    if (!input) return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
    const auto end = input.tellg();
    if (end < 0 || static_cast<std::uint64_t>(end) > 512ULL * 1024ULL * 1024ULL)
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":size"));
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    if (!bytes.empty() && !input.read(reinterpret_cast<char*>(bytes.data()),
                                      static_cast<std::streamsize>(bytes.size())))
        return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
    const auto digest = max_validation_evidence_file_sha256(evidence_root, file.relative_path);
    if (!digest)
        return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
    if (*digest != file.sha256)
        return std::unexpected(failure(ErrorCode::EvidenceDigestMismatch, file.relative_path));

    const std::span<const std::uint8_t> data{bytes};
    if (data.size() < 12 || !fourcc(data, 0, "RIFF") || !fourcc(data, 8, "WAVE") ||
        static_cast<std::uint64_t>(little_u32(data, 4)) + 8ULL != data.size())
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":riff"));
    std::optional<std::size_t> format_offset;
    std::optional<std::size_t> format_size;
    std::optional<std::size_t> audio_offset;
    std::optional<std::size_t> audio_size;
    std::size_t cursor = 12;
    while (cursor <= data.size() && data.size() - cursor >= 8) {
        const auto chunk_size = static_cast<std::size_t>(little_u32(data, cursor + 4));
        const auto payload = cursor + 8;
        if (payload > data.size() || chunk_size > data.size() - payload)
            return std::unexpected(
                failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":chunk"));
        if (fourcc(data, cursor, "fmt ") && !format_offset) {
            format_offset = payload;
            format_size = chunk_size;
        } else if (fourcc(data, cursor, "data") && !audio_offset) {
            audio_offset = payload;
            audio_size = chunk_size;
        }
        const auto padded = chunk_size + (chunk_size & 1U);
        if (padded > data.size() - payload) break;
        cursor = payload + padded;
    }
    if (!format_offset || !format_size || *format_size < 16 || !audio_offset || !audio_size)
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":chunks"));
    const auto base = *format_offset;
    const auto format_tag = little_u16(data, base);
    const auto channels = little_u16(data, base + 2);
    const auto sample_rate = little_u32(data, base + 4);
    const auto block_align = little_u16(data, base + 12);
    const auto bits = little_u16(data, base + 14);
    bool ieee_float = format_tag == 3;
    if (format_tag == 0xfffe && *format_size >= 40) {
        constexpr std::array<std::uint8_t, 12> ieee_guid_tail{
            0x00, 0x00, 0x10, 0x00, 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71};
        ieee_float = little_u32(data, base + 24) == 3 &&
                     std::equal(ieee_guid_tail.begin(),
                                ieee_guid_tail.end(),
                                data.begin() + static_cast<std::ptrdiff_t>(base + 28));
    }
    if (channels == 0 || sample_rate == 0 || block_align == 0 || *audio_size % block_align != 0)
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, file.relative_path + ":format"));

    WaveCapture result{.ieee_float = ieee_float,
                       .channels = channels,
                       .sample_rate = sample_rate,
                       .bits_per_sample = bits,
                       .frame_count = *audio_size / block_align,
                       .samples = {}};
    if (!ieee_float || (bits != 32 && bits != 64) ||
        block_align != channels * static_cast<std::uint16_t>(bits / 8U))
        return result;
    const auto sample_count = result.frame_count * channels;
    result.samples.reserve(sample_count);
    for (std::size_t index = 0; index < sample_count; ++index) {
        const auto offset = *audio_offset + index * static_cast<std::size_t>(bits / 8U);
        if (bits == 32) {
            result.samples.push_back(
                static_cast<double>(std::bit_cast<float>(little_u32(data, offset))));
        } else {
            result.samples.push_back(std::bit_cast<double>(little_u64(data, offset)));
        }
    }
    return result;
}

struct DerivedResiduals {
    std::array<bool, 5> passed{};
    std::array<std::string, 5> summaries;
    struct LiveSharedAssertions {
        std::array<bool, OBJECT_NAMES.size()> artifacts{};
        std::array<bool, LIVE_SHARED_CHECKS.size()> checks{};
        std::array<std::string, LIVE_SHARED_CHECKS.size()> summaries;
        std::string run_id;
    };
    std::optional<LiveSharedAssertions> live_shared_assertions;
};

std::expected<std::pair<bool, std::string>, Error> evaluate_teardown(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "objects",
                           "host_exit_code",
                           "crash_count",
                           "hang_count",
                           "invalid_access_count",
                           "definitely_lost_bytes"}) ||
            value.at("schema_version") != 1 || !value.at("objects").is_array() ||
            value.at("objects").size() != OBJECT_NAMES.size() ||
            !value.at("host_exit_code").is_number_integer())
            return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                           "active-dsp-teardown/cycle-counts.json"));
        bool passed = value.at("host_exit_code").get<std::int64_t>() == 0;
        constexpr std::array<std::string_view, 4> counts{
            "attempted_cycles", "constructed_cycles", "dsp_active_cycles", "destroyed_cycles"};
        std::uint64_t minimum_observed = std::numeric_limits<std::uint64_t>::max();
        for (std::size_t index = 0; index < OBJECT_NAMES.size(); ++index) {
            const auto& object = value.at("objects").at(index);
            if (!exact_fields(object,
                              {"object_name",
                               "attempted_cycles",
                               "constructed_cycles",
                               "dsp_active_cycles",
                               "destroyed_cycles"}) ||
                object.at("object_name") != OBJECT_NAMES[index])
                return std::unexpected(
                    failure(ErrorCode::MeasurementContractMismatch, "active-dsp-teardown/objects"));
            std::array<std::uint64_t, 4> observed{};
            for (std::size_t count = 0; count < counts.size(); ++count) {
                if (!object.at(counts[count]).is_number_unsigned())
                    return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                                   "active-dsp-teardown/count"));
                observed[count] = object.at(counts[count]).get<std::uint64_t>();
                if (observed[count] > 10'000'000)
                    return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                                   "active-dsp-teardown/count_bound"));
                minimum_observed = std::min(minimum_observed, observed[count]);
            }
            passed = passed && observed[0] >= 1000 &&
                     std::all_of(observed.begin() + 1, observed.end(), [&](const auto count) {
                         return count == observed[0];
                     });
        }
        for (const auto field :
             {"crash_count", "hang_count", "invalid_access_count", "definitely_lost_bytes"}) {
            if (!value.at(field).is_number_unsigned())
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "active-dsp-teardown/diagnostic_count"));
            passed = passed && value.at(field).get<std::uint64_t>() == 0;
        }
        return std::pair{passed,
                         std::string{"native teardown evaluation: minimum lifecycle count="} +
                             std::to_string(minimum_observed) +
                             (passed ? "; all exit/crash/hang/memory criteria passed"
                                     : "; one or more lifecycle or diagnostic criteria failed")};
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                       "active-dsp-teardown/cycle-counts.json"));
    }
}

std::optional<std::vector<std::uint64_t>> unsigned_vector(const json& value) {
    if (!value.is_array() || value.size() > 4096) return std::nullopt;
    std::vector<std::uint64_t> result;
    result.reserve(value.size());
    for (const auto& item : value) {
        if (!item.is_number_unsigned()) return std::nullopt;
        result.push_back(item.get<std::uint64_t>());
    }
    return result;
}

std::optional<std::vector<double>> finite_vector(const json& value) {
    if (!value.is_array() || value.size() > 4096) return std::nullopt;
    std::vector<double> result;
    result.reserve(value.size());
    for (const auto& item : value) {
        if (!finite_number(item)) return std::nullopt;
        result.push_back(item.get<double>());
    }
    return result;
}

bool fixed_spacing(const std::vector<std::uint64_t>& values, std::uint64_t spacing) {
    return values.size() >= 2 &&
           std::adjacent_find(values.begin(), values.end(), [&](const auto left, const auto right) {
               return right <= left || right - left != spacing;
           }) == values.end();
}

std::vector<std::uint64_t> nonzero_offsets(const WaveCapture& wave, std::size_t channel) {
    std::vector<std::uint64_t> offsets;
    if (wave.samples.size() != wave.frame_count * wave.channels || channel >= wave.channels)
        return offsets;
    for (std::size_t frame = 0; frame < wave.frame_count; ++frame) {
        if (wave.samples[frame * wave.channels + channel] != 0.0)
            offsets.push_back(static_cast<std::uint64_t>(frame));
    }
    return offsets;
}

std::expected<std::pair<bool, std::string>, Error> evaluate_scheduler(const json& settings,
                                                                      const json& offsets,
                                                                      const WaveCapture& wave,
                                                                      const IndexedFile& audio,
                                                                      const json& environment) {
    try {
        if (!exact_fields(settings,
                          {"schema_version",
                           "tempo_bpm",
                           "ticks_per_quarter",
                           "event_count",
                           "tick_interval",
                           "scenarios"}) ||
            settings.at("schema_version") != 1 || !finite_number(settings.at("tempo_bpm")) ||
            !settings.at("ticks_per_quarter").is_number_unsigned() ||
            !settings.at("event_count").is_number_unsigned() ||
            !settings.at("tick_interval").is_number_unsigned() ||
            !settings.at("scenarios").is_array() ||
            settings.at("scenarios").size() != SCHEDULER_SCENARIOS.size() ||
            !exact_fields(offsets, {"schema_version", "audio_sha256", "channels"}) ||
            offsets.at("schema_version") != 1 || !offsets.at("audio_sha256").is_string() ||
            offsets.at("audio_sha256").get_ref<const std::string&>() != audio.sha256 ||
            !offsets.at("channels").is_array() || offsets.at("channels").size() != 4)
            return std::unexpected(
                failure(ErrorCode::MeasurementContractMismatch, "scheduler-timing/schema"));

        bool passed = settings.at("tempo_bpm").get<double>() == 120.0 &&
                      settings.at("ticks_per_quarter").get<std::uint64_t>() == 480 &&
                      settings.at("event_count").get<std::uint64_t>() == 32 &&
                      settings.at("tick_interval").get<std::uint64_t>() == 48 &&
                      environment.at("sample_rate").get<double>() == 48'000.0 &&
                      environment.at("overdrive").get<bool>() &&
                      environment.at("scheduler_in_audio_interrupt").get<bool>() &&
                      wave.ieee_float &&
                      (wave.bits_per_sample == 32 || wave.bits_per_sample == 64) &&
                      wave.channels == 4 && wave.sample_rate == 48'000 &&
                      std::all_of(wave.samples.begin(), wave.samples.end(), [](const auto sample) {
                          return std::isfinite(sample);
                      });

        std::array<std::vector<std::uint64_t>, 4> claimed_offsets;
        for (std::size_t index = 0; index < claimed_offsets.size(); ++index) {
            const auto& channel = offsets.at("channels").at(index);
            if (!exact_fields(channel, {"scenario", "channel_index", "sample_offsets"}) ||
                channel.at("scenario") != SCHEDULER_SCENARIOS[index] ||
                !channel.at("channel_index").is_number_unsigned() ||
                channel.at("channel_index").get<std::uint64_t>() != index)
                return std::unexpected(
                    failure(ErrorCode::MeasurementContractMismatch, "scheduler-timing/channels"));
            auto parsed = unsigned_vector(channel.at("sample_offsets"));
            if (!parsed)
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "scheduler-timing/sample_offsets"));
            claimed_offsets[index] = std::move(*parsed);
            if (wave.ieee_float && wave.channels == 4)
                passed = passed && claimed_offsets[index] == nonzero_offsets(wave, index);
        }

        constexpr std::array<bool, 5> expected_overdrive{true, true, false, true, true};
        constexpr std::array<bool, 5> expected_siai{true, false, false, true, true};
        constexpr std::array<std::string_view, 5> expected_downstream{
            "click~", "click~", "click~", "click~", "timer"};
        for (std::size_t index = 0; index < SCHEDULER_SCENARIOS.size(); ++index) {
            const auto& scenario = settings.at("scenarios").at(index);
            if (!exact_fields(scenario,
                              {"name",
                               "overdrive",
                               "scheduler_in_audio_interrupt",
                               "downstream",
                               "target_ticks",
                               "callback_count",
                               "target_rejected",
                               "message_times_ms"}) ||
                scenario.at("name") != SCHEDULER_SCENARIOS[index] ||
                !scenario.at("overdrive").is_boolean() ||
                !scenario.at("scheduler_in_audio_interrupt").is_boolean() ||
                !scenario.at("downstream").is_string() ||
                !scenario.at("callback_count").is_number_unsigned() ||
                !scenario.at("target_rejected").is_boolean())
                return std::unexpected(
                    failure(ErrorCode::MeasurementContractMismatch, "scheduler-timing/scenario"));
            auto ticks = unsigned_vector(scenario.at("target_ticks"));
            auto messages = finite_vector(scenario.at("message_times_ms"));
            if (!ticks || !messages)
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "scheduler-timing/scenario_vectors"));
            passed =
                passed && scenario.at("overdrive").get<bool>() == expected_overdrive[index] &&
                scenario.at("scheduler_in_audio_interrupt").get<bool>() == expected_siai[index] &&
                scenario.at("downstream") == expected_downstream[index];
            if (index == 3) {
                passed = passed && ticks->size() == 1 &&
                         scenario.at("callback_count").get<std::uint64_t>() == 0 &&
                         scenario.at("target_rejected").get<bool>() && messages->empty() &&
                         claimed_offsets[index].empty();
            } else {
                passed = passed && ticks->size() == 32 && fixed_spacing(*ticks, 48) &&
                         scenario.at("callback_count").get<std::uint64_t>() == 32 &&
                         !scenario.at("target_rejected").get<bool>();
                if (index == 4) {
                    passed = passed && claimed_offsets[0].size() == 32 && messages->size() == 32 &&
                             std::is_sorted(messages->begin(), messages->end());
                } else {
                    passed = passed && messages->empty() && claimed_offsets[index].size() == 32;
                }
            }
        }
        passed = passed && fixed_spacing(claimed_offsets[0], 2400);
        const auto enabled_count = claimed_offsets[0].size();
        return std::pair{
            passed,
            std::string{"native scheduler evaluation: enabled impulses="} +
                std::to_string(enabled_count) +
                (passed ? "; exact 2400-sample spacing and all controls passed"
                        : "; capture, setup, spacing, rejection, or control criteria failed")};
    } catch (const json::exception&) {
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, "scheduler-timing/schema"));
    }
}

std::expected<std::pair<bool, std::string>, Error> evaluate_midi(const json& ports,
                                                                 const json& received) {
    try {
        if (!exact_fields(ports,
                          {"schema_version",
                           "output_port",
                           "input_port",
                           "receiver_name",
                           "receiver_version",
                           "channel"}) ||
            ports.at("schema_version") != 1 || !valid_text(ports.at("output_port")) ||
            !valid_text(ports.at("input_port")) || !valid_text(ports.at("receiver_name")) ||
            !valid_text(ports.at("receiver_version")) ||
            !ports.at("channel").is_number_unsigned() ||
            !exact_fields(received,
                          {"schema_version", "source_callback_lists", "received_bytes"}) ||
            received.at("schema_version") != 1 ||
            !received.at("source_callback_lists").is_array() ||
            !received.at("received_bytes").is_array() ||
            received.at("source_callback_lists").size() > 4096 ||
            received.at("received_bytes").size() > 4096)
            return std::unexpected(
                failure(ErrorCode::MeasurementContractMismatch, "downstream-midi/schema"));

        constexpr std::array<std::array<std::uint8_t, 3>, 4> expected_callbacks{
            {{60, 100, 23}, {60, 0, 23}, {64, 101, 73}, {64, 0, 73}}};
        constexpr std::array<std::uint8_t, 12> expected_bytes{
            144, 60, 100, 128, 60, 23, 144, 64, 101, 128, 64, 73};
        bool passed = ports.at("channel").get<std::uint64_t>() == 1 &&
                      received.at("source_callback_lists").size() == expected_callbacks.size() &&
                      received.at("received_bytes").size() == expected_bytes.size();
        for (std::size_t index = 0; index < received.at("source_callback_lists").size(); ++index) {
            const auto& callback = received.at("source_callback_lists").at(index);
            if (!callback.is_array() || callback.size() != 3)
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "downstream-midi/source_callback_lists"));
            for (std::size_t byte = 0; byte < callback.size(); ++byte) {
                if (!callback.at(byte).is_number_unsigned() ||
                    callback.at(byte).get<std::uint64_t>() > 255)
                    return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                                   "downstream-midi/source_callback_byte"));
                if (index < expected_callbacks.size())
                    passed = passed && callback.at(byte).get<std::uint64_t>() ==
                                           expected_callbacks[index][byte];
            }
        }
        for (std::size_t index = 0; index < received.at("received_bytes").size(); ++index) {
            const auto& byte = received.at("received_bytes").at(index);
            if (!byte.is_number_unsigned() || byte.get<std::uint64_t>() > 255)
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "downstream-midi/received_byte"));
            if (index < expected_bytes.size())
                passed = passed && byte.get<std::uint64_t>() == expected_bytes[index];
        }
        return std::pair{passed,
                         std::string{"native MIDI evaluation: received bytes="} +
                             std::to_string(received.at("received_bytes").size()) +
                             (passed ? "; exact independent channel-1 receipt passed"
                                     : "; callback or receiving-port byte criteria failed")};
    } catch (const json::exception&) {
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, "downstream-midi/schema"));
    }
}

std::expected<std::pair<bool, std::string>, Error> evaluate_render(const json& settings,
                                                                   const json& comparison,
                                                                   const WaveCapture& wave,
                                                                   const IndexedFile& audio) {
    try {
        if (!exact_fields(settings,
                          {"schema_version",
                           "format",
                           "sample_rate",
                           "minimum_frames",
                           "settled_start_frame",
                           "settled_frame_count",
                           "channels"}) ||
            settings.at("schema_version") != 1 || !settings.at("format").is_string() ||
            !finite_number(settings.at("sample_rate")) ||
            !settings.at("minimum_frames").is_number_unsigned() ||
            !settings.at("settled_start_frame").is_number_unsigned() ||
            !settings.at("settled_frame_count").is_number_unsigned() ||
            !settings.at("channels").is_array() || settings.at("channels").size() != 4 ||
            !exact_fields(comparison,
                          {"schema_version",
                           "comparator_name",
                           "comparator_version",
                           "command",
                           "exit_code",
                           "audio_sha256"}) ||
            comparison.at("schema_version") != 1 || !valid_text(comparison.at("comparator_name")) ||
            !valid_text(comparison.at("comparator_version")) ||
            !comparison.at("command").is_array() || comparison.at("command").empty() ||
            comparison.at("command").size() > 256 ||
            !comparison.at("exit_code").is_number_integer() ||
            !comparison.at("audio_sha256").is_string() ||
            comparison.at("audio_sha256").get_ref<const std::string&>() != audio.sha256)
            return std::unexpected(
                failure(ErrorCode::MeasurementContractMismatch, "rendered-audio/schema"));
        for (const auto& argument : comparison.at("command")) {
            if (!valid_text(argument))
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "rendered-audio/comparator_command"));
        }

        bool passed = settings.at("format") == "wave_ieee_float" &&
                      settings.at("sample_rate").get<double>() == 48'000.0 &&
                      settings.at("minimum_frames").get<std::uint64_t>() == 4096 &&
                      comparison.at("exit_code").get<std::int64_t>() == 0 && wave.ieee_float &&
                      (wave.bits_per_sample == 32 || wave.bits_per_sample == 64) &&
                      wave.channels == 4 && wave.sample_rate == 48'000 &&
                      wave.samples.size() == wave.frame_count * wave.channels;
        for (std::size_t index = 0; index < RENDER_OBJECTS.size(); ++index) {
            const auto& channel = settings.at("channels").at(index);
            if (!exact_fields(channel, {"object_name", "channel_index", "configuration"}) ||
                channel.at("object_name") != RENDER_OBJECTS[index] ||
                !channel.at("channel_index").is_number_unsigned() ||
                channel.at("channel_index").get<std::uint64_t>() != index ||
                channel.at("configuration") != RENDER_CONFIGURATIONS[index])
                passed = false;
        }
        const auto start = settings.at("settled_start_frame").get<std::uint64_t>();
        const auto count = settings.at("settled_frame_count").get<std::uint64_t>();
        passed = passed && count >= 4096 && start <= wave.frame_count &&
                 count <= wave.frame_count - std::min<std::uint64_t>(start, wave.frame_count);
        std::array<double, 4> maximum_errors{};
        if (passed) {
            for (std::uint64_t frame = start; frame < start + count; ++frame) {
                const std::array<double, 3> expected{-1.0, 0.375, -0.25};
                for (std::size_t channel = 0; channel < expected.size(); ++channel) {
                    const auto sample = wave.samples[frame * wave.channels + channel];
                    if (!std::isfinite(sample)) {
                        passed = false;
                    } else {
                        maximum_errors[channel] =
                            std::max(maximum_errors[channel], std::abs(sample - expected[channel]));
                    }
                }
                const auto clock = wave.samples[frame * wave.channels + 3];
                if (!std::isfinite(clock)) passed = false;
                if (frame > start) {
                    const auto previous = wave.samples[(frame - 1) * wave.channels + 3];
                    if (!std::isfinite(previous)) {
                        passed = false;
                    } else {
                        maximum_errors[3] = std::max(maximum_errors[3],
                                                     std::abs((clock - previous) - 1.0 / 24'000.0));
                    }
                }
            }
            passed = passed && maximum_errors[0] <= 1e-7 && maximum_errors[1] <= 1e-7 &&
                     maximum_errors[2] <= 1e-7 && maximum_errors[3] <= 1e-10;
        }
        return std::pair{
            passed,
            std::string{"native float-WAVE evaluation: frames="} + std::to_string(count) +
                "; max errors=" + std::to_string(maximum_errors[0]) + "," +
                std::to_string(maximum_errors[1]) + "," + std::to_string(maximum_errors[2]) + "," +
                std::to_string(maximum_errors[3]) +
                (passed ? "; all render criteria passed"
                        : "; format, region, configuration, or sample criteria failed")};
    } catch (const json::exception&) {
        return std::unexpected(
            failure(ErrorCode::MeasurementContractMismatch, "rendered-audio/schema"));
    }
}

struct LiveAssertionMapping {
    std::string subject;
    std::vector<std::string> required_assertions;
};

struct LiveAssertionContract {
    std::array<LiveAssertionMapping, OBJECT_NAMES.size()> artifacts;
    std::array<LiveAssertionMapping, LIVE_SHARED_CHECKS.size()> checks;
    std::set<std::string> expected_assertions;
};

std::optional<std::vector<std::string>> mapped_assertion_names(const json& value) {
    if (!value.is_array() || value.empty() || value.size() > 256) return std::nullopt;
    std::vector<std::string> result;
    std::set<std::string> unique;
    result.reserve(value.size());
    for (const auto& encoded : value) {
        if (!valid_text(encoded, 512)) return std::nullopt;
        auto name = encoded.get<std::string>();
        if (name.find('|') != std::string::npos || !unique.emplace(name).second)
            return std::nullopt;
        result.push_back(std::move(name));
    }
    return result;
}

std::optional<LiveAssertionContract>
live_assertion_contract_from_json(const json& value, const IndexedFile& contract_file) {
    try {
        if (contract_file.sha256 != MAX_TEST_HARNESS_MANIFEST_SHA256 ||
            !exact_fields(value,
                          {"schema_version",
                           "harness",
                           "patchers",
                           "test_runs",
                           "artifact_assertions",
                           "check_assertions"}) ||
            value.at("schema_version") != 1)
            return std::nullopt;
        const auto& harness = value.at("harness");
        if (!harness.is_object() || harness.value("name", std::string{}) != "cycling74-max-test" ||
            harness.value("repository", std::string{}) !=
                "https://github.com/Cycling74/max-test.git" ||
            harness.value("revision", std::string{}) != MAX_TEST_HARNESS_REVISION ||
            harness.value("package_version", std::string{}) != "1.2.1" ||
            harness.value("minimum_max_version", std::string{}) != "8.2" ||
            harness.value("host_kind", std::string{}) != "standalone_max")
            return std::nullopt;

        const auto& artifacts = value.at("artifact_assertions");
        const auto& checks = value.at("check_assertions");
        if (!artifacts.is_array() || artifacts.size() != OBJECT_NAMES.size() ||
            !checks.is_array() || checks.size() != VALIDATION_CHECK_NAMES.size())
            return std::nullopt;
        LiveAssertionContract contract;
        for (std::size_t index = 0; index < OBJECT_NAMES.size(); ++index) {
            const auto& mapping = artifacts.at(index);
            if (!exact_fields(mapping, {"object_name", "required_assertions"}) ||
                mapping.at("object_name") != OBJECT_NAMES[index])
                return std::nullopt;
            auto names = mapped_assertion_names(mapping.at("required_assertions"));
            if (!names) return std::nullopt;
            contract.expected_assertions.insert(names->begin(), names->end());
            contract.artifacts[index] = {.subject = std::string{OBJECT_NAMES[index]},
                                         .required_assertions = std::move(*names)};
        }
        std::size_t shared_index = 0;
        for (std::size_t index = 0; index < VALIDATION_CHECK_NAMES.size(); ++index) {
            const auto& mapping = checks.at(index);
            if (!exact_fields(mapping, {"check", "automation", "required_assertions"}) ||
                mapping.at("check") != VALIDATION_CHECK_NAMES[index])
                return std::nullopt;
            if (shared_index >= LIVE_SHARED_CHECK_NAMES.size() ||
                mapping.at("check") != LIVE_SHARED_CHECK_NAMES[shared_index])
                continue;
            if (mapping.at("automation") != "max_test") return std::nullopt;
            auto names = mapped_assertion_names(mapping.at("required_assertions"));
            if (!names) return std::nullopt;
            contract.expected_assertions.insert(names->begin(), names->end());
            contract.checks[shared_index] = {
                .subject = std::string{LIVE_SHARED_CHECK_NAMES[shared_index]},
                .required_assertions = std::move(*names),
            };
            ++shared_index;
        }
        if (shared_index != LIVE_SHARED_CHECK_NAMES.size() ||
            contract.expected_assertions.size() != 64)
            return std::nullopt;
        return contract;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

bool live_mapping_passed(const LiveAssertionMapping& mapping,
                         const std::map<std::string, bool>& assertions) {
    return std::all_of(mapping.required_assertions.begin(),
                       mapping.required_assertions.end(),
                       [&](const auto& name) {
                           const auto found = assertions.find(name);
                           return found != assertions.end() && found->second;
                       });
}

std::expected<DerivedResiduals::LiveSharedAssertions, Error>
evaluate_live_shared_assertions(const json& manifest,
                                const IndexedFile& contract_file,
                                const json& run,
                                const IndexedFile& saved_device,
                                const IndexedFile& console) {
    auto contract = live_assertion_contract_from_json(manifest, contract_file);
    if (!contract)
        return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                       "live-transport-discontinuities/assertion_contract"));
    try {
        if (!exact_fields(run,
                          {"schema_version",
                           "harness_manifest_sha256",
                           "device_sha256",
                           "console_sha256",
                           "run_id",
                           "assertions"}) ||
            run.at("schema_version") != 1 ||
            run.at("harness_manifest_sha256") != contract_file.sha256 ||
            run.at("device_sha256") != saved_device.sha256 ||
            run.at("console_sha256") != console.sha256 ||
            !valid_protocol_identifier(run.at("run_id")) || !run.at("assertions").is_array() ||
            run.at("assertions").size() != contract->expected_assertions.size())
            return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                           "live-transport-discontinuities/shared_assertions"));
        std::map<std::string, bool> assertions;
        auto expected = contract->expected_assertions.begin();
        for (const auto& encoded : run.at("assertions")) {
            if (!exact_fields(encoded, {"name", "outcome"}) ||
                !valid_text(encoded.at("name"), 512) ||
                expected == contract->expected_assertions.end() ||
                encoded.at("name") != *expected ||
                (encoded.at("outcome") != "Pass" && encoded.at("outcome") != "Fail"))
                return std::unexpected(
                    failure(ErrorCode::MeasurementContractMismatch,
                            "live-transport-discontinuities/shared_assertion_order"));
            assertions.emplace(*expected, encoded.at("outcome") == "Pass");
            ++expected;
        }
        if (expected != contract->expected_assertions.end())
            return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                           "live-transport-discontinuities/shared_assertion_set"));

        DerivedResiduals::LiveSharedAssertions result;
        result.run_id = run.at("run_id").get<std::string>();
        for (std::size_t index = 0; index < contract->artifacts.size(); ++index)
            result.artifacts[index] = live_mapping_passed(contract->artifacts[index], assertions);
        for (std::size_t index = 0; index < contract->checks.size(); ++index) {
            const auto& mapping = contract->checks[index];
            std::vector<std::string> failed;
            for (const auto& name : mapping.required_assertions) {
                const auto found = assertions.find(name);
                if (found == assertions.end() || !found->second) failed.push_back(name);
            }
            result.checks[index] = failed.empty();
            result.summaries[index] =
                "native M4L shared assertion evaluation run_id=" + result.run_id + ": ";
            if (failed.empty()) {
                result.summaries[index] += std::to_string(mapping.required_assertions.size()) +
                                           "/" +
                                           std::to_string(mapping.required_assertions.size()) +
                                           " required assertions passed";
            } else {
                result.summaries[index] += "failed assertions: ";
                for (std::size_t failed_index = 0; failed_index < failed.size(); ++failed_index) {
                    if (failed_index != 0) result.summaries[index] += ", ";
                    result.summaries[index] += failed[failed_index];
                }
            }
        }
        return result;
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                       "live-transport-discontinuities/shared_assertions"));
    }
}

std::expected<std::pair<bool, std::string>, Error> evaluate_live_transport(
    const json& value, const IndexedFile& saved_device, std::string_view assertion_run_id) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "live_set_name",
                           "device_sha256",
                           "assertion_run_id",
                           "scenarios"}) ||
            value.at("schema_version") != 1 || !valid_text(value.at("live_set_name")) ||
            !value.at("device_sha256").is_string() ||
            value.at("device_sha256").get_ref<const std::string&>() != saved_device.sha256 ||
            value.at("assertion_run_id") != assertion_run_id || !value.at("scenarios").is_array() ||
            value.at("scenarios").size() != LIVE_SCENARIOS.size())
            return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                           "live-transport-discontinuities/schema"));
        bool passed = true;
        std::uint64_t total_scheduled = 0;
        std::uint64_t total_observed = 0;
        for (std::size_t index = 0; index < LIVE_SCENARIOS.size(); ++index) {
            const auto& scenario = value.at("scenarios").at(index);
            if (!exact_fields(scenario,
                              {"name",
                               "completed",
                               "scheduled_event_count",
                               "expected_fire_count",
                               "observed_fire_count",
                               "duplicate_fire_count",
                               "stale_output_count",
                               "callback_healthy",
                               "final_reserved_slots",
                               "final_retained_events",
                               "observation"}) ||
                scenario.at("name") != LIVE_SCENARIOS[index] ||
                !scenario.at("completed").is_boolean() ||
                !scenario.at("callback_healthy").is_boolean() ||
                !valid_text(scenario.at("observation")))
                return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                               "live-transport-discontinuities/scenario"));
            constexpr std::array<std::string_view, 7> count_fields{
                "scheduled_event_count",
                "expected_fire_count",
                "observed_fire_count",
                "duplicate_fire_count",
                "stale_output_count",
                "final_reserved_slots",
                "final_retained_events",
            };
            std::array<std::uint64_t, count_fields.size()> counts{};
            for (std::size_t count = 0; count < count_fields.size(); ++count) {
                if (!scenario.at(count_fields[count]).is_number_unsigned() ||
                    scenario.at(count_fields[count]).get<std::uint64_t>() > 1'000'000)
                    return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                                   "live-transport-discontinuities/count"));
                counts[count] = scenario.at(count_fields[count]).get<std::uint64_t>();
            }
            total_scheduled += counts[0];
            total_observed += counts[2];
            passed = passed && scenario.at("completed").get<bool>() && counts[0] > 0 &&
                     counts[1] <= counts[0] && counts[2] == counts[1] && counts[3] == 0 &&
                     counts[4] == 0 && scenario.at("callback_healthy").get<bool>() &&
                     counts[5] == 0 && counts[6] == 0;
        }
        return std::pair{
            passed,
            std::string{"native M4L discontinuity evaluation: scheduled="} +
                std::to_string(total_scheduled) +
                "; observed expected fires=" + std::to_string(total_observed) +
                (passed ? "; all six scenarios and final gauges passed"
                        : "; completion, fire-count, callback, stale, or gauge criteria failed")};
    } catch (const json::exception&) {
        return std::unexpected(failure(ErrorCode::MeasurementContractMismatch,
                                       "live-transport-discontinuities/schema"));
    }
}

std::expected<DerivedResiduals, Error> evaluate(const json& host_plan,
                                                std::string_view host_plan_sha256,
                                                const json& indexed_result,
                                                std::string_view indexed_result_sha256,
                                                const std::filesystem::path& evidence_root,
                                                std::string_view result_evidence_relative_path) {
    if (!plan_contract_valid(host_plan, host_plan_sha256))
        return std::unexpected(failure(ErrorCode::InvalidPlan, "host_plan"));
    if (!lowercase_hex(indexed_result_sha256, 64))
        return std::unexpected(failure(ErrorCode::InvalidResult, "result_sha256"));
    auto index = result_index_from_json(indexed_result);
    if (!index) return std::unexpected(failure(ErrorCode::InvalidResult, "indexed_result"));
    if (index->plan_sha256 != host_plan_sha256)
        return std::unexpected(failure(ErrorCode::InvalidResult, "plan_sha256"));
    if (result_evidence_relative_path != "host-run-result.json")
        return std::unexpected(failure(ErrorCode::InvalidResult, "result_evidence_relative_path"));
    const auto result_digest = max_validation_evidence_file_sha256(
        evidence_root, std::string{result_evidence_relative_path});
    if (!result_digest)
        return std::unexpected(
            failure(ErrorCode::EvidenceUnavailable, std::string{result_evidence_relative_path}));
    if (*result_digest != indexed_result_sha256)
        return std::unexpected(
            failure(ErrorCode::EvidenceDigestMismatch, std::string{result_evidence_relative_path}));
    for (const auto& file : index->files) {
        const auto digest = max_validation_evidence_file_sha256(evidence_root, file.relative_path);
        if (!digest)
            return std::unexpected(failure(ErrorCode::EvidenceUnavailable, file.relative_path));
        if (*digest != file.sha256)
            return std::unexpected(failure(ErrorCode::EvidenceDigestMismatch, file.relative_path));
    }
    constexpr std::array<std::size_t, 3> shared_nonempty{1, 2, 8};
    for (const auto file_index : shared_nonempty) {
        const auto nonempty = require_nonempty_evidence(evidence_root, index->files[file_index]);
        if (!nonempty) return std::unexpected(nonempty.error());
    }
    if (index->environment.at("host_kind") == "max_for_live") {
        for (const auto file_index : {std::size_t{12}, std::size_t{16}}) {
            const auto nonempty =
                require_nonempty_evidence(evidence_root, index->files[file_index]);
            if (!nonempty) return std::unexpected(nonempty.error());
        }
    }

    auto teardown = read_evidence_json(evidence_root, index->files[0]);
    auto scheduler_settings = read_evidence_json(evidence_root, index->files[3]);
    auto scheduler_offsets = read_evidence_json(evidence_root, index->files[4]);
    auto scheduler_wave = read_wave_capture(evidence_root, index->files[5]);
    auto midi_ports = read_evidence_json(evidence_root, index->files[6]);
    auto midi_received = read_evidence_json(evidence_root, index->files[7]);
    auto render_settings = read_evidence_json(evidence_root, index->files[9]);
    auto render_wave = read_wave_capture(evidence_root, index->files[10]);
    auto render_comparison = read_evidence_json(evidence_root, index->files[11]);
    std::optional<json> live_assertion_contract;
    std::optional<json> live_assertions;
    std::optional<json> live_scenarios;
    if (index->environment.at("host_kind") == "max_for_live") {
        auto encoded_contract = read_evidence_json(evidence_root, index->files[13]);
        auto encoded_assertions = read_evidence_json(evidence_root, index->files[14]);
        auto encoded_scenarios = read_evidence_json(evidence_root, index->files[15]);
        if (!encoded_contract) return std::unexpected(encoded_contract.error());
        if (!encoded_assertions) return std::unexpected(encoded_assertions.error());
        if (!encoded_scenarios) return std::unexpected(encoded_scenarios.error());
        live_assertion_contract = std::move(*encoded_contract);
        live_assertions = std::move(*encoded_assertions);
        live_scenarios = std::move(*encoded_scenarios);
    }
    if (!teardown) return std::unexpected(teardown.error());
    if (!scheduler_settings) return std::unexpected(scheduler_settings.error());
    if (!scheduler_offsets) return std::unexpected(scheduler_offsets.error());
    if (!scheduler_wave) return std::unexpected(scheduler_wave.error());
    if (!midi_ports) return std::unexpected(midi_ports.error());
    if (!midi_received) return std::unexpected(midi_received.error());
    if (!render_settings) return std::unexpected(render_settings.error());
    if (!render_wave) return std::unexpected(render_wave.error());
    if (!render_comparison) return std::unexpected(render_comparison.error());

    auto teardown_result = evaluate_teardown(*teardown);
    auto scheduler_result = evaluate_scheduler(*scheduler_settings,
                                               *scheduler_offsets,
                                               *scheduler_wave,
                                               index->files[5],
                                               index->environment);
    auto midi_result = evaluate_midi(*midi_ports, *midi_received);
    auto render_result =
        evaluate_render(*render_settings, *render_comparison, *render_wave, index->files[10]);
    if (!teardown_result) return std::unexpected(teardown_result.error());
    if (!scheduler_result) return std::unexpected(scheduler_result.error());
    if (!midi_result) return std::unexpected(midi_result.error());
    if (!render_result) return std::unexpected(render_result.error());
    std::optional<DerivedResiduals::LiveSharedAssertions> live_shared_result;
    std::optional<std::pair<bool, std::string>> live_result;
    if (live_assertion_contract && live_assertions && live_scenarios) {
        auto evaluated_shared = evaluate_live_shared_assertions(*live_assertion_contract,
                                                                index->files[13],
                                                                *live_assertions,
                                                                index->files[12],
                                                                index->files[16]);
        if (!evaluated_shared) return std::unexpected(evaluated_shared.error());
        auto evaluated_live =
            evaluate_live_transport(*live_scenarios, index->files[12], evaluated_shared->run_id);
        if (!evaluated_live) return std::unexpected(evaluated_live.error());
        live_shared_result = std::move(*evaluated_shared);
        live_result = std::move(*evaluated_live);
    }
    DerivedResiduals result;
    const std::array evaluated{*teardown_result, *scheduler_result, *midi_result, *render_result};
    for (std::size_t index_value = 0; index_value < evaluated.size(); ++index_value) {
        result.passed[index_value] = evaluated[index_value].first;
        result.summaries[index_value] = evaluated[index_value].second + "; residual index sha256=" +
                                        std::string{indexed_result_sha256};
    }
    if (live_result) {
        result.passed[4] = live_result->first;
        result.summaries[4] =
            live_result->second + "; residual index sha256=" + std::string{indexed_result_sha256};
    }
    if (live_shared_result) {
        for (auto& summary : live_shared_result->summaries)
            summary += "; residual index sha256=" + std::string{indexed_result_sha256};
        result.live_shared_assertions = std::move(*live_shared_result);
    }
    return result;
}

} // namespace

MaxHostRunVerificationResult
verify_max_host_run_result(const json& host_plan,
                           std::string_view host_plan_sha256,
                           const json& indexed_result,
                           std::string_view indexed_result_sha256,
                           const std::filesystem::path& evidence_root,
                           std::string_view result_evidence_relative_path) {
    auto evaluated = evaluate(host_plan,
                              host_plan_sha256,
                              indexed_result,
                              indexed_result_sha256,
                              evidence_root,
                              result_evidence_relative_path);
    if (!evaluated) return std::unexpected(evaluated.error());
    return {};
}

MaxHostRunApplicationResult
apply_max_host_run_result(const MaxValidationObservation& supplied_observation,
                          const json& host_plan,
                          std::string_view host_plan_sha256,
                          const json& indexed_result,
                          std::string_view indexed_result_sha256,
                          const std::filesystem::path& evidence_root,
                          std::string result_evidence_relative_path) {
    auto observation = max_validation_observation_from_json(
        max_validation_observation_to_json(supplied_observation));
    if (!observation) return std::unexpected(failure(ErrorCode::InvalidObservation, "observation"));
    auto index = result_index_from_json(indexed_result);
    if (!index) return std::unexpected(failure(ErrorCode::InvalidResult, "indexed_result"));
    const auto observation_json = max_validation_observation_to_json(*observation);
    if (index->sunny_version != observation->sunny_version ||
        index->package_version != observation->package_version ||
        index->source_revision != observation->source_revision ||
        index->observed_at_utc != observation->observed_at_utc ||
        index->environment != observation_json.at("environment"))
        return std::unexpected(failure(ErrorCode::ProvenanceMismatch, "observation"));
    auto evaluated = evaluate(host_plan,
                              host_plan_sha256,
                              indexed_result,
                              indexed_result_sha256,
                              evidence_root,
                              result_evidence_relative_path);
    if (!evaluated) return std::unexpected(evaluated.error());

    if (observation->environment.host_kind == MaxHostKind::MaxForLive) {
        if (!evaluated->live_shared_assertions ||
            observation->artifacts.size() != OBJECT_NAMES.size())
            return std::unexpected(failure(ErrorCode::InvalidObservation, "live_shared_sets"));
        const auto& shared = *evaluated->live_shared_assertions;
        for (std::size_t index = 0; index < OBJECT_NAMES.size(); ++index) {
            if (observation->artifacts[index].object_name != OBJECT_NAMES[index])
                return std::unexpected(
                    failure(ErrorCode::InvalidObservation, "live_artifact_order"));
            observation->artifacts[index].discovered = shared.artifacts[index];
            observation->artifacts[index].instantiated = shared.artifacts[index];
        }
        for (std::size_t index = 0; index < LIVE_SHARED_CHECKS.size(); ++index) {
            const auto found = std::find_if(
                observation->checks.begin(), observation->checks.end(), [&](const auto& fact) {
                    return fact.check == LIVE_SHARED_CHECKS[index];
                });
            if (found == observation->checks.end())
                return std::unexpected(
                    failure(ErrorCode::InvalidObservation, "live_shared_checks"));
            found->outcome =
                shared.checks[index] ? MaxValidationOutcome::Passed : MaxValidationOutcome::Failed;
            found->summary = shared.summaries[index];
            found->evidence_relative_path = result_evidence_relative_path;
        }
    }

    constexpr std::array<MaxValidationCheck, 4> checks{
        MaxValidationCheck::ActiveDspTeardown,
        MaxValidationCheck::SchedulerTiming,
        MaxValidationCheck::DownstreamMidiDelivery,
        MaxValidationCheck::RenderedAudio,
    };
    for (std::size_t index_value = 0; index_value < checks.size(); ++index_value) {
        const auto found =
            std::find_if(observation->checks.begin(),
                         observation->checks.end(),
                         [&](const auto& fact) { return fact.check == checks[index_value]; });
        if (found == observation->checks.end())
            return std::unexpected(failure(ErrorCode::InvalidObservation, "closed_checks"));
        found->outcome = evaluated->passed[index_value] ? MaxValidationOutcome::Passed
                                                        : MaxValidationOutcome::Failed;
        found->summary = evaluated->summaries[index_value];
        found->evidence_relative_path = result_evidence_relative_path;
    }
    if (observation->environment.host_kind == MaxHostKind::MaxForLive) {
        const auto found = std::find_if(
            observation->checks.begin(), observation->checks.end(), [](const auto& fact) {
                return fact.check == MaxValidationCheck::LiveTransportDiscontinuities;
            });
        if (found == observation->checks.end())
            return std::unexpected(failure(ErrorCode::InvalidObservation, "closed_checks"));
        found->outcome =
            evaluated->passed[4] ? MaxValidationOutcome::Passed : MaxValidationOutcome::Failed;
        found->summary = evaluated->summaries[4];
        found->evidence_relative_path = result_evidence_relative_path;
    }
    auto validated =
        max_validation_observation_from_json(max_validation_observation_to_json(*observation));
    if (!validated)
        return std::unexpected(failure(ErrorCode::InvalidObservation, "derived_observation"));
    return std::move(*validated);
}

const char* max_host_run_application_error_name(MaxHostRunApplicationErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::InvalidPlan:
        return "invalid_plan";
    case ErrorCode::InvalidResult:
        return "invalid_result";
    case ErrorCode::InvalidObservation:
        return "invalid_observation";
    case ErrorCode::ProvenanceMismatch:
        return "provenance_mismatch";
    case ErrorCode::EvidenceUnavailable:
        return "evidence_unavailable";
    case ErrorCode::EvidenceDigestMismatch:
        return "evidence_digest_mismatch";
    case ErrorCode::MeasurementContractMismatch:
        return "measurement_contract_mismatch";
    }
    return "unknown";
}

} // namespace sunny::infrastructure
