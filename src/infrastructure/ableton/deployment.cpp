/**
 * @file deployment.cpp
 * @brief Guarded/journalled Ableton transport implementation
 */

#include <sunny/infrastructure/ableton/deployment.hpp>
#include <utility>

namespace sunny::infrastructure {

namespace {

bool is_read_only_request(const LomRequest& request) {
    if (request.type == LomRequestType::GetProperty) return true;
    if (request.type != LomRequestType::CallMethod) return false;
    return request.property_or_method == "get_notes_by_id" ||
           request.property_or_method == "get_notes_extended" ||
           request.property_or_method == "get_all_notes_extended" ||
           request.property_or_method == "sunny_get_target_profile" ||
           request.property_or_method == "sunny_get_target_snapshot" ||
           request.property_or_method == "sunny_get_device_parameter" ||
           request.property_or_method == "sunny_get_step_envelope" ||
           request.property_or_method == "sunny_managed_context" ||
           request.property_or_method == "sunny_managed_operation" ||
           request.property_or_method == "sunny_managed_observe" ||
           request.property_or_method == "sunny_managed_sample_envelope" ||
           request.property_or_method == "sunny_get_device_count";
}

} // namespace

JournaledLomTransport::JournaledLomTransport(LomTransport& underlying,
                                             std::optional<AbletonTargetProfile> fixed_profile,
                                             std::span<const AbletonPlannedMutation> expected,
                                             bool enforce_plan)
    : underlying_(underlying), fixed_profile_(std::move(fixed_profile)),
      expected_(expected.begin(), expected.end()), enforce_plan_(enforce_plan) {}

bool JournaledLomTransport::matches_expected(const LomRequest& request) const {
    if (!enforce_plan_) return true;
    if (expected_index_ >= expected_.size()) return false;
    const auto& expected = expected_[expected_index_];
    return expected.phase == phase_ && LomProtocol::serialize_request(expected.request) ==
                                           LomProtocol::serialize_request(request);
}

void JournaledLomTransport::record(const LomRequest& request,
                                   const LomResponse& response,
                                   bool sent) {
    AbletonMutationJournalEntry entry;
    entry.sequence = static_cast<std::uint64_t>(journal_.size());
    entry.phase = phase_;
    entry.request = request;
    entry.response_value = response.value;
    entry.response_error = response.error;
    if (!sent || response.delivery == LomDeliveryState::NotSent) {
        entry.outcome = AbletonMutationOutcome::DeclinedBeforeSend;
    } else if (underlying_.records_without_execution() && response.success) {
        entry.outcome = AbletonMutationOutcome::RecordedOnly;
    } else if (response.success) {
        entry.outcome = AbletonMutationOutcome::Acknowledged;
    } else {
        // A failed mutation response cannot prove that the target remained
        // unchanged: execution may have happened before readback/ack failure.
        entry.outcome = AbletonMutationOutcome::Indeterminate;
    }
    journal_.push_back(std::move(entry));
}

LomResponse JournaledLomTransport::send(const LomRequest& request) {
    const bool read_only = is_read_only_request(request);
    if (!LomProtocol::validate_request(request)) {
        LomResponse response{false,
                             std::nullopt,
                             std::string{"request outside Sunny bridge protocol v"} +
                                 std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                             LomDeliveryState::NotSent};
        if (!read_only) record(request, response, false);
        return response;
    }
    // Read-only observations may depend on acknowledgements produced by the
    // executing target (for example returned note IDs). They are validated and
    // forwarded, but are neither mutations nor members of the dry-run mutation
    // sequence.
    if (read_only) return underlying_.send(request);
    if (!matches_expected(request)) {
        plan_diverged_ = true;
        LomResponse response{false,
                             std::nullopt,
                             std::string{"deployment plan diverged"},
                             LomDeliveryState::NotSent};
        record(request, response, false);
        return response;
    }
    auto response = underlying_.send(request);
    record(request, response, true);
    if (enforce_plan_) ++expected_index_;
    return response;
}

LomResponse JournaledLomTransport::send_notes(const LomPath& clip_path,
                                              const std::vector<LomNoteData>& notes) {
    const auto request = LomProtocol::add_new_notes(clip_path, notes);
    if (!LomProtocol::validate_request(request)) {
        LomResponse response{false,
                             std::nullopt,
                             std::string{"request outside Sunny bridge protocol v"} +
                                 std::to_string(SUNNY_BRIDGE_PROTOCOL_VERSION),
                             LomDeliveryState::NotSent};
        record(request, response, false);
        return response;
    }
    if (!matches_expected(request)) {
        plan_diverged_ = true;
        LomResponse response{false,
                             std::nullopt,
                             std::string{"deployment plan diverged"},
                             LomDeliveryState::NotSent};
        record(request, response, false);
        return response;
    }
    auto response = underlying_.send_notes(clip_path, notes);
    record(request, response, true);
    if (enforce_plan_) ++expected_index_;
    return response;
}

bool JournaledLomTransport::is_connected() const {
    return underlying_.is_connected();
}

bool JournaledLomTransport::records_without_execution() const {
    return underlying_.records_without_execution();
}

bool JournaledLomTransport::ensure_connected() {
    return underlying_.ensure_connected();
}

sunny::core::Result<std::optional<AbletonTargetProfile>> JournaledLomTransport::target_profile() {
    if (fixed_profile_) return fixed_profile_;
    return underlying_.target_profile();
}

sunny::core::Result<std::optional<AbletonTargetSnapshot>> JournaledLomTransport::target_snapshot() {
    return underlying_.target_snapshot();
}

sunny::core::Result<std::optional<std::uint32_t>> JournaledLomTransport::scene_count() {
    return underlying_.scene_count();
}

sunny::core::Result<std::optional<std::uint32_t>> JournaledLomTransport::return_track_count() {
    return underlying_.return_track_count();
}

sunny::core::Result<std::optional<std::uint32_t>>
JournaledLomTransport::device_count(const LomPath& track_path) {
    return underlying_.device_count(track_path);
}

std::vector<AbletonPlannedMutation>
planned_mutations_from_journal(std::span<const AbletonMutationJournalEntry> journal) {
    std::vector<AbletonPlannedMutation> result;
    result.reserve(journal.size());
    for (const auto& entry : journal)
        result.push_back({entry.phase, entry.request});
    return result;
}

} // namespace sunny::infrastructure
