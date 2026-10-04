/**
 * @file managed_realization.hpp
 * @brief Tokened managed Session MIDI realization and serializable receipts.
 *
 * This finite bridge foundation has no MCP admission or host qualification.
 * Persist intent and a durable may-have-sent fence BEFORE sending. A disk-restored
 * Prepared receipt alone cannot establish that a previous process never sent it.
 * Reconciliation only queries the
 * retained operation journal: it never retries or infers absence across epochs.
 */
#pragma once

#include <sunny/infrastructure/ableton/transport.hpp>

namespace sunny::infrastructure {

inline constexpr int SUNNY_MANAGED_RECEIPT_SCHEMA_VERSION = 1;

struct ManagedBridgeContext {
    std::string bridge_instance;
    std::string document_token;
};

/// Existing compiler projection, in Live quarter-note beats; no timing rewrite.
struct ManagedClipProjection {
    double clip_end = 0.0;
    int signature_numerator = 4;
    int signature_denominator = 4;
    nlohmann::json notes = nlohmann::json::array();
    /// Requests outside the selected Clip remain explicitly unapplied.
    std::vector<LomRequest> outside_clip_requests;
};

/// Extract exactly one already-recorded Score Clip. Reject unrepresented Clip
/// semantics rather than quietly substituting a second musical compiler.
[[nodiscard]] sunny::core::Result<ManagedClipProjection>
managed_clip_projection(const CommandBuffer& recording, int planned_track_index);

[[nodiscard]] sunny::core::Result<ManagedBridgeContext>
managed_bridge_context(LomTransport& transport);

[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_clip_request(const ManagedBridgeContext& context,
                          const std::string& operation_id,
                          const std::string& project_key,
                          const std::string& binding_key,
                          const ManagedClipProjection& projection,
                          std::optional<std::string> expected_content_fingerprint = std::nullopt);

/// Prepared is not yet sent. NotSent proves safe explicit retry of the same
/// token within an unfenced caller attempt. Durable dispatch fencing belongs to
/// the product store, not this value. Indeterminate/Unknown require reconciliation;
/// no function auto-retries.
enum class ManagedOperationOutcome {
    Prepared,
    NotSent,
    Acknowledged,
    Declined,
    Indeterminate,
    UnknownOperation,
    UnknownEpoch,
};

struct ManagedOperationReceipt {
    ManagedBridgeContext context;
    LomRequest request;
    LomDeliveryState delivery = LomDeliveryState::NotSent;
    ManagedOperationOutcome outcome = ManagedOperationOutcome::Prepared;
    std::optional<nlohmann::json> journal;
    std::optional<std::string> error;

    [[nodiscard]] bool explicit_retry_safe() const noexcept {
        return delivery == LomDeliveryState::NotSent && !journal &&
               (outcome == ManagedOperationOutcome::Prepared ||
                outcome == ManagedOperationOutcome::NotSent);
    }
};

[[nodiscard]] sunny::core::Result<ManagedOperationReceipt>
prepare_managed_operation(const ManagedBridgeContext& context, const LomRequest& request);

/// Send exactly once. Explicit repeated calls are allowed only for Prepared or
/// NotSent receipts; an uncertain or partial outcome cannot be retried here.
[[nodiscard]] sunny::core::Result<ManagedOperationReceipt>
execute_managed_operation(const ManagedOperationReceipt& prepared, LomTransport& transport);

/// Query only, preserving the original request and delivery evidence. A missing
/// journal or changed epoch is unknown, never evidence that creation did not run.
[[nodiscard]] sunny::core::Result<ManagedOperationReceipt>
reconcile_managed_operation(const ManagedOperationReceipt& receipt, LomTransport& transport);

[[nodiscard]] nlohmann::json managed_receipt_to_json(const ManagedOperationReceipt& receipt);
[[nodiscard]] sunny::core::Result<ManagedOperationReceipt>
managed_receipt_from_json(const nlohmann::json& value);

/// Persistable logical tags + complete actual content, never native pointers or
/// authorization by index. Incomplete observations can be saved but not rebound.
struct ManagedBindingReceipt {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    nlohmann::json observation;
};

[[nodiscard]] sunny::core::Result<ManagedBindingReceipt>
managed_binding_receipt(const ManagedOperationReceipt& acknowledgement);
[[nodiscard]] nlohmann::json managed_binding_to_json(const ManagedBindingReceipt& binding);
[[nodiscard]] sunny::core::Result<ManagedBindingReceipt>
managed_binding_from_json(const nlohmann::json& value);

[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_rebind_request(const ManagedBridgeContext& new_context,
                            const std::string& operation_id,
                            const ManagedBindingReceipt& persisted);
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_envelope_request(const ManagedBridgeContext& context,
                              const std::string& operation_id,
                              const ManagedBindingReceipt& binding,
                              const nlohmann::json& lane);

} // namespace sunny::infrastructure
