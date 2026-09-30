/**
 * @file deployment.hpp
 * @brief Exact-command guards and mutation evidence for Ableton deployment
 */

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <sunny/infrastructure/ableton/target_snapshot.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <vector>

namespace sunny::infrastructure {

enum class AbletonDeploymentPhase : std::uint8_t { Score, Timbre, Mix };

enum class AbletonMutationOutcome : std::uint8_t {
    RecordedOnly,
    Acknowledged,
    DeclinedBeforeSend,
    Indeterminate,
};

struct AbletonPlannedMutation {
    AbletonDeploymentPhase phase = AbletonDeploymentPhase::Score;
    LomRequest request;
};

struct AbletonMutationJournalEntry {
    std::uint64_t sequence = 0;
    AbletonDeploymentPhase phase = AbletonDeploymentPhase::Score;
    LomRequest request;
    AbletonMutationOutcome outcome = AbletonMutationOutcome::Indeterminate;
    std::optional<LomValue> response_value;
    std::optional<std::string> response_error;

    [[nodiscard]] bool target_may_have_mutated() const {
        return outcome == AbletonMutationOutcome::Acknowledged ||
               outcome == AbletonMutationOutcome::Indeterminate;
    }
};

/**
 * Wrap a transport, guard each mutation against an optional exact plan, and
 * preserve every acknowledgement or uncertainty. Read-only target probes are
 * forwarded but are not mutations and therefore do not enter the journal.
 */
class JournaledLomTransport final : public LomTransport {
  public:
    explicit JournaledLomTransport(LomTransport& underlying,
                                   std::optional<AbletonTargetProfile> fixed_profile = std::nullopt,
                                   std::span<const AbletonPlannedMutation> expected = {},
                                   bool enforce_plan = false);

    LomResponse send(const LomRequest& request) override;
    LomResponse send_notes(const LomPath& clip_path,
                           const std::vector<LomNoteData>& notes) override;
    [[nodiscard]] bool is_connected() const override;
    [[nodiscard]] bool records_without_execution() const override;
    [[nodiscard]] bool ensure_connected() override;
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetProfile>>
    target_profile() override;
    [[nodiscard]] sunny::core::Result<std::optional<AbletonTargetSnapshot>>
    target_snapshot() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> scene_count() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>> return_track_count() override;
    [[nodiscard]] sunny::core::Result<std::optional<std::uint32_t>>
    device_count(const LomPath& track_path) override;

    void set_phase(AbletonDeploymentPhase phase) { phase_ = phase; }
    [[nodiscard]] const std::vector<AbletonMutationJournalEntry>& journal() const {
        return journal_;
    }
    [[nodiscard]] bool plan_diverged() const { return plan_diverged_; }
    [[nodiscard]] bool plan_consumed() const {
        return !enforce_plan_ || expected_index_ == expected_.size();
    }

  private:
    [[nodiscard]] bool matches_expected(const LomRequest& request) const;
    void record(const LomRequest& request, const LomResponse& response, bool sent);

    LomTransport& underlying_;
    std::optional<AbletonTargetProfile> fixed_profile_;
    std::vector<AbletonPlannedMutation> expected_;
    std::size_t expected_index_ = 0;
    AbletonDeploymentPhase phase_ = AbletonDeploymentPhase::Score;
    std::vector<AbletonMutationJournalEntry> journal_;
    bool enforce_plan_ = false;
    bool plan_diverged_ = false;
};

[[nodiscard]] std::vector<AbletonPlannedMutation>
planned_mutations_from_journal(std::span<const AbletonMutationJournalEntry> journal);

} // namespace sunny::infrastructure
