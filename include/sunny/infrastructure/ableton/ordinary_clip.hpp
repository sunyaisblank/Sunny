/** Finite ordinary Clip authority. Restored receipts only permit queries. */
#pragma once

#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {

enum class OrdinaryClipOutcome {
    Prepared,
    NativePrepared,
    NotSent,
    Acknowledged,
    Declined,
    Partial,
    Indeterminate,
    UnknownOperation,
    UnknownEpoch
};

struct OrdinaryClipReceipt {
    // Exact original sunny_ordinary_prepare payload, including shared epoch and
    // immutable action. No native pointer or current-index undo authority.
    nlohmann::json intent;
    LomDeliveryState delivery = LomDeliveryState::NotSent;
    OrdinaryClipOutcome outcome = OrdinaryClipOutcome::Prepared;
    std::optional<nlohmann::json> journal = std::nullopt;
    std::optional<std::string> error = std::nullopt;
};

[[nodiscard]] bool ordinary_request_valid(const std::string& name,
                                          const nlohmann::json& payload) noexcept;
[[nodiscard]] sunny::core::Result<OrdinaryClipReceipt>
prepare_ordinary_clip(const ManagedBridgeContext& context,
                      const std::string& operation_id,
                      const std::string& action,
                      const nlohmann::json& payload);
// Caller must consume a fresh durable OrdinaryDispatchPermit. Sends preparation
// and execution at most once each, with no compensation or implicit retry.
[[nodiscard]] sunny::core::Result<OrdinaryClipReceipt>
execute_ordinary_clip(const OrdinaryClipReceipt& prepared, LomTransport& transport);
[[nodiscard]] sunny::core::Result<OrdinaryClipReceipt>
reconcile_ordinary_clip(const OrdinaryClipReceipt& original, LomTransport& transport);
[[nodiscard]] nlohmann::json ordinary_receipt_to_json(const OrdinaryClipReceipt& receipt);
[[nodiscard]] sunny::core::Result<OrdinaryClipReceipt>
ordinary_receipt_from_json(const nlohmann::json& value);

} // namespace sunny::infrastructure
