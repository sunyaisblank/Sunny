/**
 * @file validation_record.cpp
 * @brief Strict Ableton deployment validation-record implementation
 */

#include <algorithm>
#include <exception>
#include <initializer_list>
#include <limits>
#include <string_view>
#include <sunny/core/mix/serialization.hpp>
#include <sunny/core/score/serialization.hpp>
#include <sunny/core/timbre/serialization.hpp>
#include <sunny/infrastructure/ableton/lom_protocol.hpp>
#include <sunny/infrastructure/ableton/validation_record.hpp>
#include <sunny/version.hpp>

namespace sunny::infrastructure {

namespace {

using json = nlohmann::json;
using formats::AbletonProjectDeploymentStatus;
using sunny::core::ErrorCode;

bool exact_fields(const json& value, std::initializer_list<std::string_view> fields) {
    if (!value.is_object() || value.size() != fields.size()) return false;
    return std::all_of(
        fields.begin(), fields.end(), [&](const auto field) { return value.contains(field); });
}

bool valid_operator_text(const std::string& value, std::size_t maximum = 256) {
    if (value.empty() || value.size() > maximum) return false;
    return std::none_of(value.begin(), value.end(), [](const unsigned char character) {
        return character < 0x20 || character == 0x7f;
    });
}

const char* phase_name(AbletonDeploymentPhase phase) {
    switch (phase) {
    case AbletonDeploymentPhase::Score:
        return "score";
    case AbletonDeploymentPhase::Timbre:
        return "timbre";
    case AbletonDeploymentPhase::Mix:
        return "mix";
    }
    return "score";
}

std::optional<AbletonDeploymentPhase> phase_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "score") return AbletonDeploymentPhase::Score;
    if (text == "timbre") return AbletonDeploymentPhase::Timbre;
    if (text == "mix") return AbletonDeploymentPhase::Mix;
    return std::nullopt;
}

const char* outcome_name(AbletonMutationOutcome outcome) {
    switch (outcome) {
    case AbletonMutationOutcome::RecordedOnly:
        return "recorded_only";
    case AbletonMutationOutcome::Acknowledged:
        return "acknowledged";
    case AbletonMutationOutcome::DeclinedBeforeSend:
        return "declined_before_send";
    case AbletonMutationOutcome::Indeterminate:
        return "indeterminate";
    }
    return "indeterminate";
}

std::optional<AbletonMutationOutcome> outcome_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "recorded_only") return AbletonMutationOutcome::RecordedOnly;
    if (text == "acknowledged") return AbletonMutationOutcome::Acknowledged;
    if (text == "declined_before_send") return AbletonMutationOutcome::DeclinedBeforeSend;
    if (text == "indeterminate") return AbletonMutationOutcome::Indeterminate;
    return std::nullopt;
}

const char* status_name(AbletonProjectDeploymentStatus status) {
    switch (status) {
    case AbletonProjectDeploymentStatus::Completed:
        return "completed";
    case AbletonProjectDeploymentStatus::ProjectChanged:
        return "project_changed";
    case AbletonProjectDeploymentStatus::TargetChanged:
        return "target_changed";
    case AbletonProjectDeploymentStatus::PlanDiverged:
        return "plan_diverged";
    case AbletonProjectDeploymentStatus::PlanConsumed:
        return "plan_consumed";
    case AbletonProjectDeploymentStatus::ApplyFailed:
        return "apply_failed";
    case AbletonProjectDeploymentStatus::PostSnapshotUnavailable:
        return "post_snapshot_unavailable";
    }
    return "apply_failed";
}

std::optional<AbletonProjectDeploymentStatus> status_from_json(const json& value) {
    if (!value.is_string()) return std::nullopt;
    const auto& text = value.get_ref<const std::string&>();
    if (text == "completed") return AbletonProjectDeploymentStatus::Completed;
    if (text == "project_changed") return AbletonProjectDeploymentStatus::ProjectChanged;
    if (text == "target_changed") return AbletonProjectDeploymentStatus::TargetChanged;
    if (text == "plan_diverged") return AbletonProjectDeploymentStatus::PlanDiverged;
    if (text == "plan_consumed") return AbletonProjectDeploymentStatus::PlanConsumed;
    if (text == "apply_failed") return AbletonProjectDeploymentStatus::ApplyFailed;
    if (text == "post_snapshot_unavailable")
        return AbletonProjectDeploymentStatus::PostSnapshotUnavailable;
    return std::nullopt;
}

json request_json(const LomRequest& request) {
    return json::parse(LomProtocol::serialize_request(request));
}

bool equivalent_request(const LomRequest& left, const LomRequest& right) {
    return LomProtocol::serialize_request(left) == LomProtocol::serialize_request(right);
}

json lom_value_json(const std::optional<LomValue>& value) {
    if (!value) return nullptr;
    return std::visit([](const auto& item) { return json(item); }, *value);
}

bool valid_project_state(const json& value) {
    if (!exact_fields(value, {"score", "timbre", "mix"}) || !value.at("score").is_object() ||
        !value.at("timbre").is_array() || !value.at("mix").is_object())
        return false;
    try {
        auto score = sunny::core::score_from_json(value.at("score"));
        auto mix = sunny::core::mix_from_json(value.at("mix"));
        if (!score || !mix) return false;

        std::vector<sunny::core::TimbreProfile> profiles;
        profiles.reserve(value.at("timbre").size());
        for (const auto& encoded : value.at("timbre")) {
            auto profile = sunny::core::timbre_from_json(encoded);
            if (!profile) return false;
            profiles.push_back(std::move(*profile));
        }
        std::sort(profiles.begin(), profiles.end(), [](const auto& lhs, const auto& rhs) {
            if (lhs.part_id != rhs.part_id) return lhs.part_id < rhs.part_id;
            return lhs.id < rhs.id;
        });
        json timbre = json::array();
        for (const auto& profile : profiles)
            timbre.push_back(sunny::core::timbre_to_json(profile));
        return value == json{{"score", sunny::core::score_to_json(*score)},
                             {"timbre", std::move(timbre)},
                             {"mix", sunny::core::mix_to_json(*mix)}};
    } catch (const std::exception&) {
        return false;
    }
}

bool valid_environment(const AbletonOperatorEnvironment& environment) {
    if (!valid_operator_text(environment.live_edition) ||
        !valid_operator_text(environment.operating_system) ||
        !valid_operator_text(environment.architecture) ||
        !valid_operator_text(environment.remote_script_revision))
        return false;
    return !environment.max || (valid_operator_text(environment.max->max_version) &&
                                valid_operator_text(environment.max->max_for_live_version) &&
                                valid_operator_text(environment.max->license_state));
}

bool valid_compilation_evidence(const json& evidence, const AbletonTargetSnapshot& plan_target) {
    if (!exact_fields(evidence,
                      {"success",
                       "connected",
                       "complete",
                       "target_profile",
                       "part_tracks",
                       "score",
                       "timbre",
                       "mix",
                       "postconditions",
                       "diagnostics",
                       "warnings"}) ||
        !evidence.at("success").is_boolean() || !evidence.at("success").get<bool>() ||
        !evidence.at("connected").is_boolean() || !evidence.at("connected").get<bool>() ||
        !evidence.at("complete").is_boolean() || !evidence.at("part_tracks").is_array() ||
        !evidence.at("score").is_object() || !evidence.at("timbre").is_array() ||
        !evidence.at("mix").is_object() || !evidence.at("postconditions").is_object() ||
        !evidence.at("diagnostics").is_array() || !evidence.at("warnings").is_array())
        return false;
    auto profile = target_profile_from_json(evidence.at("target_profile"));
    return profile &&
           target_profile_to_json(*profile) == target_profile_to_json(plan_target.target_profile);
}

std::optional<ErrorCode> error_from_json(const json& value) {
    if (value.is_null()) return std::nullopt;
    if (!value.is_number_integer()) return std::nullopt;
    const auto encoded = value.get<std::int64_t>();
    if (encoded < std::numeric_limits<int>::min() || encoded > std::numeric_limits<int>::max())
        return std::nullopt;
    const auto error = static_cast<ErrorCode>(static_cast<int>(encoded));
    switch (error) {
    case ErrorCode::ArithmeticOverflow:
    case ErrorCode::InvalidMidiNote:
    case ErrorCode::InvalidVelocity:
    case ErrorCode::InvalidBeat:
    case ErrorCode::InvalidTimeSignature:
    case ErrorCode::ConnectionFailed:
    case ErrorCode::ConnectionLost:
    case ErrorCode::SendFailed:
    case ErrorCode::ReceiveFailed:
    case ErrorCode::ProtocolError:
    case ErrorCode::TargetAddressUnrepresentable:
    case ErrorCode::TargetValueUnrepresentable:
    case ErrorCode::FormatError:
    case ErrorCode::InvalidMidiPPQ:
    case ErrorCode::InvariantViolation:
    case ErrorCode::TimbreInvalidParameter:
    case ErrorCode::MixInvalidParameter:
    case ErrorCode::ProjectValidationFailed:
    case ErrorCode::ProjectMissingComponent:
    case ErrorCode::ProjectPlanProjectChanged:
    case ErrorCode::ProjectPlanTargetChanged:
    case ErrorCode::ProjectPlanDiverged:
    case ErrorCode::TargetSnapshotUnavailable:
    case ErrorCode::ProjectPlanConsumed:
        return error;
    default:
        return std::nullopt;
    }
}

bool status_error_consistent(const AbletonValidationRecord& record) {
    using Status = AbletonProjectDeploymentStatus;
    if (record.deployment_status == Status::Completed) return !record.error;
    if (!record.error) return false;
    if (record.deployment_status == Status::ProjectChanged)
        return *record.error == ErrorCode::ProjectPlanProjectChanged;
    if (record.deployment_status == Status::TargetChanged)
        return *record.error == ErrorCode::ProjectPlanTargetChanged;
    if (record.deployment_status == Status::PlanDiverged)
        return *record.error == ErrorCode::ProjectPlanDiverged;
    if (record.deployment_status == Status::PlanConsumed)
        return *record.error == ErrorCode::ProjectPlanConsumed;
    return *record.error != ErrorCode::Ok;
}

bool status_shape_consistent(const AbletonValidationRecord& record) {
    using Status = AbletonProjectDeploymentStatus;
    if (record.deployment_status == Status::ProjectChanged ||
        record.deployment_status == Status::PlanConsumed)
        return !record.apply_target_before && !record.target_after &&
               record.mutation_journal.empty() && !record.compilation_evidence;
    if (record.deployment_status == Status::TargetChanged)
        return record.apply_target_before && !record.target_after &&
               record.mutation_journal.empty() && !record.compilation_evidence;
    if (record.deployment_status == Status::Completed)
        return record.apply_target_before && record.target_after && record.compilation_evidence;
    if (record.deployment_status == Status::PostSnapshotUnavailable)
        return record.apply_target_before && !record.target_after;
    return true;
}

sunny::core::VoidResult validate_record(const AbletonValidationRecord& record) {
    if (record.schema_version != ABLETON_VALIDATION_RECORD_SCHEMA_VERSION ||
        record.sunny_version != sunny::SUNNY_VERSION ||
        !valid_environment(record.operator_environment) || record.ppq < 1 ||
        record.ppq > std::numeric_limits<std::uint16_t>::max() ||
        !valid_project_state(record.project_state) || !status_error_consistent(record) ||
        !status_shape_consistent(record))
        return std::unexpected(ErrorCode::FormatError);

    auto plan_target =
        target_snapshot_from_json(target_snapshot_to_json(record.plan_target_snapshot));
    if (!plan_target) return std::unexpected(ErrorCode::FormatError);
    const auto expected_profile =
        target_profile_to_json(record.plan_target_snapshot.target_profile);
    const auto validate_optional_snapshot = [&](const auto& snapshot) {
        if (!snapshot) return true;
        auto parsed = target_snapshot_from_json(target_snapshot_to_json(*snapshot));
        return parsed && target_profile_to_json(snapshot->target_profile) == expected_profile;
    };
    if (!validate_optional_snapshot(record.apply_target_before) ||
        !validate_optional_snapshot(record.target_after))
        return std::unexpected(ErrorCode::FormatError);

    for (const auto& mutation : record.planned_mutations)
        if (!LomProtocol::validate_request(mutation.request))
            return std::unexpected(ErrorCode::FormatError);

    for (std::size_t index = 0; index < record.mutation_journal.size(); ++index) {
        const auto& entry = record.mutation_journal[index];
        if (entry.sequence != index || !LomProtocol::validate_request(entry.request) ||
            ((entry.outcome == AbletonMutationOutcome::Acknowledged ||
              entry.outcome == AbletonMutationOutcome::RecordedOnly) &&
             entry.response_error) ||
            ((entry.outcome == AbletonMutationOutcome::DeclinedBeforeSend ||
              entry.outcome == AbletonMutationOutcome::Indeterminate) &&
             (!entry.response_error || !valid_operator_text(*entry.response_error, 2048))) ||
            (record.deployment_status != AbletonProjectDeploymentStatus::PlanDiverged &&
             (index >= record.planned_mutations.size() ||
              entry.phase != record.planned_mutations[index].phase ||
              !equivalent_request(entry.request, record.planned_mutations[index].request))))
            return std::unexpected(ErrorCode::FormatError);
    }

    if (record.compilation_evidence &&
        !valid_compilation_evidence(*record.compilation_evidence, record.plan_target_snapshot))
        return std::unexpected(ErrorCode::FormatError);
    for (const auto& step : record.cleanup_steps)
        if (!valid_operator_text(step, 2048)) return std::unexpected(ErrorCode::FormatError);
    return {};
}

json environment_json(const AbletonOperatorEnvironment& environment) {
    json max = nullptr;
    if (environment.max) {
        max = {{"max_version", environment.max->max_version},
               {"max_for_live_version", environment.max->max_for_live_version},
               {"license_state", environment.max->license_state}};
    }
    return {{"provenance", "operator_supplied"},
            {"live_edition", environment.live_edition},
            {"operating_system", environment.operating_system},
            {"architecture", environment.architecture},
            {"remote_script_revision", environment.remote_script_revision},
            {"max", std::move(max)}};
}

sunny::core::Result<AbletonOperatorEnvironment> environment_from_json(const json& value) {
    if (!exact_fields(value,
                      {"provenance",
                       "live_edition",
                       "operating_system",
                       "architecture",
                       "remote_script_revision",
                       "max"}) ||
        !value.at("provenance").is_string() || value.at("provenance") != "operator_supplied" ||
        !value.at("live_edition").is_string() || !value.at("operating_system").is_string() ||
        !value.at("architecture").is_string() || !value.at("remote_script_revision").is_string())
        return std::unexpected(ErrorCode::FormatError);

    AbletonOperatorEnvironment environment{value.at("live_edition").get<std::string>(),
                                           value.at("operating_system").get<std::string>(),
                                           value.at("architecture").get<std::string>(),
                                           value.at("remote_script_revision").get<std::string>(),
                                           std::nullopt};
    if (!value.at("max").is_null()) {
        if (!exact_fields(value.at("max"),
                          {"max_version", "max_for_live_version", "license_state"}) ||
            !value.at("max").at("max_version").is_string() ||
            !value.at("max").at("max_for_live_version").is_string() ||
            !value.at("max").at("license_state").is_string())
            return std::unexpected(ErrorCode::FormatError);
        environment.max = AbletonMaxOperatorEnvironment{
            value.at("max").at("max_version").get<std::string>(),
            value.at("max").at("max_for_live_version").get<std::string>(),
            value.at("max").at("license_state").get<std::string>()};
    }
    if (!valid_environment(environment)) return std::unexpected(ErrorCode::FormatError);
    return environment;
}

} // namespace

bool AbletonValidationRecord::target_may_be_partially_modified() const {
    return std::any_of(mutation_journal.begin(), mutation_journal.end(), [](const auto& entry) {
        return entry.target_may_have_mutated();
    });
}

bool AbletonValidationRecord::execution_trace_complete() const {
    if (deployment_status != formats::AbletonProjectDeploymentStatus::Completed || error ||
        !apply_target_before || !target_after || !compilation_evidence ||
        !equivalent_target_snapshot(plan_target_snapshot, *apply_target_before) ||
        mutation_journal.size() != planned_mutations.size())
        return false;
    for (std::size_t index = 0; index < mutation_journal.size(); ++index) {
        const auto& planned = planned_mutations[index];
        const auto& observed = mutation_journal[index];
        if (observed.sequence != index || observed.phase != planned.phase ||
            observed.outcome != AbletonMutationOutcome::Acknowledged ||
            !equivalent_request(observed.request, planned.request))
            return false;
    }
    return valid_compilation_evidence(*compilation_evidence, plan_target_snapshot);
}

sunny::core::Result<AbletonValidationRecord>
make_ableton_validation_record(const formats::AbletonProjectDeploymentPlan& plan,
                               const formats::AbletonProjectDeploymentAttempt& attempt,
                               AbletonOperatorEnvironment environment,
                               std::optional<json> compilation_evidence,
                               std::vector<std::string> cleanup_steps) {
    if (!plan.consumed ||
        static_cast<bool>(attempt.compilation) != static_cast<bool>(compilation_evidence) ||
        (attempt.compilation && target_profile_to_json(attempt.compilation->score.target_profile) !=
                                    target_profile_to_json(plan.target_snapshot.target_profile)))
        return std::unexpected(ErrorCode::FormatError);

    AbletonValidationRecord record;
    record.sunny_version = std::string(sunny::SUNNY_VERSION);
    record.operator_environment = std::move(environment);
    record.ppq = plan.ppq;
    record.project_state = plan.project_state;
    record.plan_target_snapshot = plan.target_snapshot;
    record.planned_mutations = plan.mutations;
    record.deployment_status = attempt.status;
    record.error = attempt.error;
    record.apply_target_before = attempt.target_before;
    record.target_after = attempt.target_after;
    record.mutation_journal = attempt.mutation_journal;
    record.compilation_evidence = std::move(compilation_evidence);
    record.cleanup_steps = std::move(cleanup_steps);
    auto valid = validate_record(record);
    if (!valid) return std::unexpected(valid.error());
    return record;
}

json ableton_validation_record_to_json(const AbletonValidationRecord& record) {
    json planned = json::array();
    for (std::size_t index = 0; index < record.planned_mutations.size(); ++index) {
        planned.push_back({{"sequence", index},
                           {"phase", phase_name(record.planned_mutations[index].phase)},
                           {"request", request_json(record.planned_mutations[index].request)}});
    }

    json journal = json::array();
    for (const auto& entry : record.mutation_journal) {
        journal.push_back(
            {{"sequence", entry.sequence},
             {"phase", phase_name(entry.phase)},
             {"request", request_json(entry.request)},
             {"outcome", outcome_name(entry.outcome)},
             {"response_value", lom_value_json(entry.response_value)},
             {"response_error", entry.response_error ? json(*entry.response_error) : json(nullptr)},
             {"target_may_have_mutated", entry.target_may_have_mutated()}});
    }

    return {
        {"schema_version", record.schema_version},
        {"sunny_version", record.sunny_version},
        {"operator_environment", environment_json(record.operator_environment)},
        {"ppq", record.ppq},
        {"project_state", record.project_state},
        {"plan_target_snapshot", target_snapshot_to_json(record.plan_target_snapshot)},
        {"planned_mutations", std::move(planned)},
        {"deployment",
         {{"status", status_name(record.deployment_status)},
          {"error_code", record.error ? json(static_cast<int>(*record.error)) : json(nullptr)},
          {"target_may_be_partially_modified", record.target_may_be_partially_modified()},
          {"apply_target_before",
           record.apply_target_before ? target_snapshot_to_json(*record.apply_target_before)
                                      : json(nullptr)},
          {"target_after",
           record.target_after ? target_snapshot_to_json(*record.target_after) : json(nullptr)},
          {"mutation_journal", std::move(journal)},
          {"compilation_evidence",
           record.compilation_evidence ? *record.compilation_evidence : json(nullptr)},
          {"execution_trace_complete", record.execution_trace_complete()}}},
        {"cleanup_steps", record.cleanup_steps},
    };
}

sunny::core::Result<AbletonValidationRecord>
ableton_validation_record_from_json(const json& value) {
    try {
        if (!exact_fields(value,
                          {"schema_version",
                           "sunny_version",
                           "operator_environment",
                           "ppq",
                           "project_state",
                           "plan_target_snapshot",
                           "planned_mutations",
                           "deployment",
                           "cleanup_steps"}) ||
            !value.at("schema_version").is_number_unsigned() ||
            value.at("schema_version").get<std::uint64_t>() !=
                ABLETON_VALIDATION_RECORD_SCHEMA_VERSION ||
            !value.at("sunny_version").is_string() || !value.at("ppq").is_number_integer() ||
            !value.at("planned_mutations").is_array() || !value.at("cleanup_steps").is_array() ||
            !exact_fields(value.at("deployment"),
                          {"status",
                           "error_code",
                           "target_may_be_partially_modified",
                           "apply_target_before",
                           "target_after",
                           "mutation_journal",
                           "compilation_evidence",
                           "execution_trace_complete"}) ||
            !value.at("deployment").at("target_may_be_partially_modified").is_boolean() ||
            !value.at("deployment").at("mutation_journal").is_array() ||
            !value.at("deployment").at("execution_trace_complete").is_boolean())
            return std::unexpected(ErrorCode::FormatError);

        auto environment = environment_from_json(value.at("operator_environment"));
        auto plan_target = target_snapshot_from_json(value.at("plan_target_snapshot"));
        auto status = status_from_json(value.at("deployment").at("status"));
        if (!environment || !plan_target || !status) return std::unexpected(ErrorCode::FormatError);

        const auto ppq_value = value.at("ppq").get<std::int64_t>();
        if (ppq_value < 1 || ppq_value > std::numeric_limits<std::uint16_t>::max())
            return std::unexpected(ErrorCode::FormatError);

        AbletonValidationRecord record;
        record.schema_version = ABLETON_VALIDATION_RECORD_SCHEMA_VERSION;
        record.sunny_version = value.at("sunny_version").get<std::string>();
        record.operator_environment = std::move(*environment);
        record.ppq = static_cast<int>(ppq_value);
        record.project_state = value.at("project_state");
        record.plan_target_snapshot = std::move(*plan_target);
        record.deployment_status = *status;
        record.error = error_from_json(value.at("deployment").at("error_code"));
        if (!value.at("deployment").at("error_code").is_null() && !record.error)
            return std::unexpected(ErrorCode::FormatError);

        for (std::size_t index = 0; index < value.at("planned_mutations").size(); ++index) {
            const auto& encoded = value.at("planned_mutations")[index];
            if (!exact_fields(encoded, {"sequence", "phase", "request"}) ||
                !encoded.at("sequence").is_number_unsigned() ||
                encoded.at("sequence").get<std::uint64_t>() != index)
                return std::unexpected(ErrorCode::FormatError);
            auto phase = phase_from_json(encoded.at("phase"));
            auto request = LomProtocol::deserialize_request(encoded.at("request"));
            if (!phase || !request) return std::unexpected(ErrorCode::FormatError);
            record.planned_mutations.push_back({*phase, std::move(*request)});
        }

        const auto parse_snapshot =
            [](const json& encoded) -> sunny::core::Result<std::optional<AbletonTargetSnapshot>> {
            if (encoded.is_null()) return std::optional<AbletonTargetSnapshot>{};
            auto snapshot = target_snapshot_from_json(encoded);
            if (!snapshot) return std::unexpected(ErrorCode::FormatError);
            return std::optional<AbletonTargetSnapshot>{std::move(*snapshot)};
        };
        auto before = parse_snapshot(value.at("deployment").at("apply_target_before"));
        auto after = parse_snapshot(value.at("deployment").at("target_after"));
        if (!before || !after) return std::unexpected(ErrorCode::FormatError);
        record.apply_target_before = std::move(*before);
        record.target_after = std::move(*after);

        for (std::size_t index = 0; index < value.at("deployment").at("mutation_journal").size();
             ++index) {
            const auto& encoded = value.at("deployment").at("mutation_journal")[index];
            if (!exact_fields(encoded,
                              {"sequence",
                               "phase",
                               "request",
                               "outcome",
                               "response_value",
                               "response_error",
                               "target_may_have_mutated"}) ||
                !encoded.at("sequence").is_number_unsigned() ||
                encoded.at("sequence").get<std::uint64_t>() != index ||
                !encoded.at("target_may_have_mutated").is_boolean() ||
                (!encoded.at("response_error").is_null() &&
                 !encoded.at("response_error").is_string()))
                return std::unexpected(ErrorCode::FormatError);
            auto phase = phase_from_json(encoded.at("phase"));
            auto request = LomProtocol::deserialize_request(encoded.at("request"));
            auto outcome = outcome_from_json(encoded.at("outcome"));
            if (!phase || !request || !outcome) return std::unexpected(ErrorCode::FormatError);

            AbletonMutationJournalEntry entry;
            entry.sequence = index;
            entry.phase = *phase;
            entry.request = std::move(*request);
            entry.outcome = *outcome;
            if (!encoded.at("response_value").is_null())
                entry.response_value = LomValue{encoded.at("response_value")};
            if (!encoded.at("response_error").is_null())
                entry.response_error = encoded.at("response_error").get<std::string>();
            if (entry.target_may_have_mutated() !=
                encoded.at("target_may_have_mutated").get<bool>())
                return std::unexpected(ErrorCode::FormatError);
            record.mutation_journal.push_back(std::move(entry));
        }

        if (!value.at("deployment").at("compilation_evidence").is_null())
            record.compilation_evidence = value.at("deployment").at("compilation_evidence");
        for (const auto& step : value.at("cleanup_steps")) {
            if (!step.is_string()) return std::unexpected(ErrorCode::FormatError);
            record.cleanup_steps.push_back(step.get<std::string>());
        }

        auto valid = validate_record(record);
        if (!valid ||
            record.target_may_be_partially_modified() !=
                value.at("deployment").at("target_may_be_partially_modified").get<bool>() ||
            record.execution_trace_complete() !=
                value.at("deployment").at("execution_trace_complete").get<bool>())
            return std::unexpected(ErrorCode::FormatError);
        return record;
    } catch (const std::exception&) {
        return std::unexpected(ErrorCode::FormatError);
    }
}

} // namespace sunny::infrastructure
