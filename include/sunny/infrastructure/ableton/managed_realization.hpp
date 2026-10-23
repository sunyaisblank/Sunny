/**
 * @file managed_realization.hpp
 * @brief Tokened managed Session MIDI realization and serializable receipts.
 *
 * The owning-project MCP subset uses this finite bridge contract. Host
 * qualification remains pending.
 * Persist intent and a durable may-have-sent fence BEFORE sending. A disk-restored
 * Prepared receipt alone cannot establish that a previous process never sent it.
 * Reconciliation only queries the
 * retained operation journal: it never retries or infers absence across epochs.
 */
#pragma once

#include <sunny/infrastructure/ableton/transport.hpp>

namespace sunny::infrastructure {

inline constexpr int SUNNY_MANAGED_RECEIPT_SCHEMA_VERSION = 1;
// Bounds native main-thread insert_step calls independently of wire capacity.
inline constexpr std::size_t SUNNY_MANAGED_ENVELOPE_MAX_STEPS = 64;

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
reconcile_managed_operation(const ManagedOperationReceipt& receipt,
                            LomTransport& transport,
                            std::optional<ManagedOperationReceipt>* query_evidence = nullptr);

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

enum class ManagedObservationOutcome {
    Observed,
    PartialBinding,
    RecoveryUnavailable,
    UnknownEpoch
};

/// Exact closed read-only response, including actual native note IDs and logical
/// context. Inspection never refreshes the bridge's retained mutation guard.
struct ManagedBindingObservation {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    ManagedObservationOutcome outcome = ManagedObservationOutcome::RecoveryUnavailable;
    nlohmann::json evidence;
};

[[nodiscard]] sunny::core::Result<ManagedBindingObservation>
observe_managed_binding(const ManagedBridgeContext& expected_context,
                        const std::string& project_key,
                        const std::string& binding_key,
                        LomTransport& transport);
[[nodiscard]] sunny::core::Result<ManagedBindingReceipt>
managed_observed_binding(const ManagedBindingObservation& observation);

/// Read-only samples resolved through retained native binding identities.
/// Evidence is the exact response wrapper. Observed responses contain envelope
/// {has_envelope, parameter, samples}; other outcomes contain no sample evidence.
/// Samples do not observe complete breakpoint populations or refresh guards.
struct ManagedEnvelopeObservation {
    ManagedBindingObservation binding;
    nlohmann::json evidence;
};

[[nodiscard]] sunny::core::Result<ManagedEnvelopeObservation>
sample_managed_envelope(const ManagedBridgeContext& expected_context,
                        const std::string& project_key,
                        const std::string& binding_key,
                        const nlohmann::json& parameter,
                        const std::vector<double>& sample_times,
                        LomTransport& transport);

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

/// In-place existing-ID revisions only. Expected is the eight observed native
/// fields; updates admits pitch/start_time/duration/velocity/mute/release_velocity.
/// Full Live11.1+ note population and collision-free proposed geometry are
/// required. Unknown MPE/FollowActions remain host-owned and unobserved.
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_note_update_request(const ManagedBridgeContext& context,
                                 const std::string& operation_id,
                                 const ManagedBindingReceipt& binding,
                                 const nlohmann::json& changes);

/// Whole Event additions/deletions and existing-ID changes, without recreation
/// of retained notes. Addition note_key labels carry association, not authority.
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_note_population_request(const ManagedBridgeContext& context,
                                     const std::string& operation_id,
                                     const ManagedBindingReceipt& binding,
                                     const nlohmann::json& changes,
                                     const nlohmann::json& deletions,
                                     const nlohmann::json& additions);

/// Same native Clip extent/meter, preserving all current note IDs/values.
/// Full population and contained note endpoints are necessary admission;
/// actual retained-object authority remains enforced by the bridge.
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_clip_geometry_request(const ManagedBridgeContext& context,
                                   const std::string& operation_id,
                                   const ManagedBindingReceipt& binding,
                                   double end_marker,
                                   int signature_numerator,
                                   int signature_denominator);

} // namespace sunny::infrastructure
