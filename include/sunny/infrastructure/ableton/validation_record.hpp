/**
 * @file validation_record.hpp
 * @brief Versioned, replayable evidence for one guarded Ableton deployment attempt
 */

#pragma once

#include <cstdint>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <sunny/core/types/music_types.hpp>
#include <sunny/infrastructure/formats/ableton_project.hpp>
#include <vector>

namespace sunny::infrastructure {

inline constexpr std::uint32_t ABLETON_VALIDATION_RECORD_SCHEMA_VERSION = 1;

/** Operator-supplied Max facts. Presence means every field was explicitly supplied. */
struct AbletonMaxOperatorEnvironment {
    std::string max_version;
    std::string max_for_live_version;
    std::string license_state;
};

/**
 * Facts that the public Live bridge does not observe. They remain explicitly operator-supplied;
 * the target snapshots independently retain bridge-observed Live version/capability evidence.
 */
struct AbletonOperatorEnvironment {
    std::string live_edition;
    std::string operating_system;
    std::string architecture;
    std::string remote_script_revision;
    std::optional<AbletonMaxOperatorEnvironment> max;
};

/**
 * A lossless evidence envelope around one exact project plan and its one-shot apply attempt.
 * `execution_trace_complete()` proves only the presence and coherence of the host execution trace;
 * it does not imply that compilation was complete, audio was rendered, or sound was equivalent.
 */
struct AbletonValidationRecord {
    std::uint32_t schema_version = ABLETON_VALIDATION_RECORD_SCHEMA_VERSION;
    std::string sunny_version;
    AbletonOperatorEnvironment operator_environment;
    int ppq = 480;
    nlohmann::json project_state;
    AbletonTargetSnapshot plan_target_snapshot;
    std::vector<AbletonPlannedMutation> planned_mutations;
    formats::AbletonProjectDeploymentStatus deployment_status =
        formats::AbletonProjectDeploymentStatus::ApplyFailed;
    std::optional<sunny::core::ErrorCode> error;
    std::optional<AbletonTargetSnapshot> apply_target_before;
    std::optional<AbletonTargetSnapshot> target_after;
    std::vector<AbletonMutationJournalEntry> mutation_journal;
    std::optional<nlohmann::json> compilation_evidence;
    std::vector<std::string> cleanup_steps;

    [[nodiscard]] bool target_may_be_partially_modified() const;
    [[nodiscard]] bool execution_trace_complete() const;
};

/** Construct a record from the actual authoritative plan and retained attempt objects. */
[[nodiscard]] sunny::core::Result<AbletonValidationRecord>
make_ableton_validation_record(const formats::AbletonProjectDeploymentPlan& plan,
                               const formats::AbletonProjectDeploymentAttempt& attempt,
                               AbletonOperatorEnvironment environment,
                               std::optional<nlohmann::json> compilation_evidence,
                               std::vector<std::string> cleanup_steps = {});

/** Strict schema-v1 parser; malformed, extended, or internally contradictory evidence fails. */
[[nodiscard]] sunny::core::Result<AbletonValidationRecord>
ableton_validation_record_from_json(const nlohmann::json& value);

/** Canonical JSON representation suitable for retention outside the running process. */
[[nodiscard]] nlohmann::json
ableton_validation_record_to_json(const AbletonValidationRecord& record);

} // namespace sunny::infrastructure
