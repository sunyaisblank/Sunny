/** Owning native projections, retained history and durable dispatch for MCP tools.
 *
 * This private interface keeps authored intent separate from current native
 * authority. Reading a receipt never grants a dispatch permit; dispatch fences
 * the original operation before sending and retains its observed outcome.
 */
#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <sunny/infrastructure/ableton/managed_clip_revision.hpp>
#include <sunny/infrastructure/ableton/realization_store.hpp>
#include <sunny/infrastructure/mcp/session.hpp>
#include <vector>

namespace sunny::infrastructure::mcp_realization {
using nlohmann::json;

// Stable logical keys and process-held native history.
std::string hexadecimal(std::uint64_t value);
std::string project_key(const McpSession& session, std::uint64_t score);
std::string binding_key(std::uint64_t part);
std::shared_ptr<RealizationStore> history(const McpSession& session, bool mutation);
const RealizationStoredAttempt*
latest(const RealizationStore& store, std::uint64_t score, std::uint64_t part);
const ManagedOperationReceipt& last_receipt(const RealizationStoredAttempt& attempt);

// These pointers borrow from the session's retained store. Explicit adoption
// establishes a new authority boundary while earlier uncertainty stays queryable.
struct BindingHistory {
    const RealizationStoredAttempt* acknowledged = nullptr;
    const RealizationStoredAttempt* unresolved = nullptr;
};
BindingHistory
binding_history(const RealizationStore& store, std::uint64_t score, std::uint64_t part);
ManagedBindingReceipt current_binding(const RealizationStore& store,
                                      const RealizationStoredAttempt& prior);
const RealizationStoredAttempt* global_settings_blocker(const RealizationStore& store,
                                                        const ManagedBridgeContext& context);

// An admitted owning revision, optionally projected from verified history.
// A projection describes music; it does not authorize native object access.
struct DesiredPart {
    std::uint64_t score_id = 0;
    std::uint64_t part_id = 0;
    std::uint64_t revision = 0;
    ManagedClipProjection projection;
    std::vector<std::string> keys;
    json encoded;
    std::string identity;
    json warnings = json::array();
    std::optional<std::string> historical_projection_attempt;
};
DesiredPart owning_project_revision(const McpSession& session, const json& arguments);
DesiredPart owning_part(const McpSession& session, const json& arguments);
AbletonTargetProfile reviewed_native_target(LomTransport& transport);
DesiredPart desired_part(const McpSession& session,
                         const json& arguments,
                         DesiredPart desired,
                         LomTransport& transport);
bool active_part(const McpSession& session, const DesiredPart& desired);
ManagedClipProjection retained_projection(const RealizationStoredAttempt& prior);
DesiredPart stage_part(const DesiredPart& final, const ManagedClipRevisionStage& stage);
DesiredPart historical_part(DesiredPart desired, const RealizationStoredAttempt& prior);
DesiredPart
recovery_part(const McpSession& session, const json& arguments, LomTransport& transport);
void recovery_provenance(json& output, const McpSession& session, const DesiredPart& desired);

// Exact attack associations and permitted whole-Event population changes.
std::map<std::string, json> native_note_associations(const std::vector<std::string>& keys,
                                                     const json& projection,
                                                     const json& observation);
bool projection_matches(const json& desired, const json& observation);
struct NotePopulationChanges {
    json changes = json::array();
    json deletions = json::array();
    json additions = json::array();
    bool empty() const { return changes.empty() && deletions.empty() && additions.empty(); }
};
NotePopulationChanges note_population_changes(const RealizationStoredAttempt& previous,
                                              const DesiredPart& desired,
                                              const ManagedBindingReceipt& binding);
json note_changes(const RealizationStoredAttempt& previous,
                  const DesiredPart& desired,
                  const ManagedBindingReceipt& binding);

// Acknowledgement and successful persistence alone do not establish completion.
bool verified_outcome(const ManagedOperationReceipt& receipt, const DesiredPart& desired);
json dispatch(const std::shared_ptr<RealizationStore>& store,
              const DesiredPart& desired,
              const ManagedBridgeContext& context,
              const LomRequest& request,
              LomTransport& transport);
json attempt_summary(const RealizationStoredAttempt& attempt);
json decline(const std::string& message, const std::string& state = "declined");
class ReconciliationRequired final : public std::runtime_error {
  public:
    json response;
    explicit ReconciliationRequired(const RealizationStoredAttempt& attempt);
};
json workflow_failure(const std::exception& failure);

} // namespace sunny::infrastructure::mcp_realization
