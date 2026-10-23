#pragma once
#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {
inline constexpr std::size_t SUNNY_MANAGED_ENVELOPE_REPLACEMENT_MAX_STEPS =
    SUNNY_MANAGED_ENVELOPE_MAX_STEPS;
/// Samples are incomplete; no saved/reopened or historical envelope identity.
struct ManagedEnvelopeReplacementPreview {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    std::string preview_token;
    std::string preview_fingerprint;
    nlohmann::json evidence;
};

[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_envelope_replacement_preview_request(const ManagedBridgeContext& context,
                                                  const ManagedBindingReceipt& binding,
                                                  const nlohmann::json& lane);
[[nodiscard]] sunny::core::Result<ManagedEnvelopeReplacementPreview>
preview_managed_envelope_replacement(const ManagedBridgeContext& context,
                                     const ManagedBindingReceipt& binding,
                                     const nlohmann::json& lane,
                                     LomTransport& transport);
[[nodiscard]] sunny::core::Result<ManagedEnvelopeReplacementPreview>
managed_envelope_replacement_preview_from_json(const nlohmann::json& value);
/// Explicit whole selected-parameter overwrite, including unknown unsampled
/// state. Persist the product dispatch fence before execution.
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_envelope_replacement_request(const ManagedBridgeContext& context,
                                          const std::string& operation_id,
                                          const ManagedEnvelopeReplacementPreview& preview);
} // namespace sunny::infrastructure
