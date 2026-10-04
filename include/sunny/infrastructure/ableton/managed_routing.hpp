/** Finite current native Return/output/send candidates; not host qualification.
 */
#pragma once
#include <sunny/infrastructure/ableton/managed_realization.hpp>

namespace sunny::infrastructure {
enum class ManagedRoutingKind {
    CreateReturn,
    AdoptReturn,
    OutputType,
    OutputChannel,
    SendLevel,
    AdoptGroup
};
struct ManagedRoutingPreview {
    ManagedBridgeContext context;
    std::string project_key;
    std::string binding_key;
    std::string preview_token;
    std::string preview_fingerprint;
    nlohmann::json approved_preview;
    nlohmann::json observation;
};
/** Read-only advertised current native IDs/target descriptors; no authority. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_routing_candidates_request(const ManagedBridgeContext&, const ManagedBindingReceipt&);
[[nodiscard]] sunny::core::Result<nlohmann::json> parse_managed_routing_candidates(
    const LomRequest&, const ManagedBridgeContext&, const nlohmann::json& actual);
/** Closed finite intent. Product validates owning Mix/revision and affected
 * membership. */
[[nodiscard]] sunny::core::Result<LomRequest> make_managed_routing_preview_request(
    const ManagedBridgeContext&, const ManagedBindingReceipt&, const nlohmann::json& intent);
/** Read-only current Group bootstrap; no pre-existing grouped Part binding
 * required. */
[[nodiscard]] sunny::core::Result<LomRequest>
make_managed_group_preview_request(const ManagedBridgeContext&,
                                   const std::string& project,
                                   const std::string& anchor_binding,
                                   const nlohmann::json& current_anchor_selector,
                                   const nlohmann::json& group_intent);
[[nodiscard]] sunny::core::Result<ManagedRoutingPreview> parse_managed_routing_preview(
    const LomRequest&, const ManagedBridgeContext&, const nlohmann::json& actual);
/** Product fences immutable Prepared request before any call to this setter
 * family. */
[[nodiscard]] sunny::core::Result<LomRequest> make_managed_routing_request(
    const ManagedBridgeContext&, const std::string& operation, const ManagedRoutingPreview&);
/** Authorized affected-Part overlays, never foreign Store evidence appends.
 * Group-only acknowledgements return an empty vector and no Clip authority. */
[[nodiscard]] sunny::core::Result<std::vector<ManagedBindingReceipt>>
managed_routing_affected_bindings(const ManagedBridgeContext&,
                                  const nlohmann::json& immutable_request,
                                  const nlohmann::json& result);
namespace managed_routing_detail {
[[nodiscard]] bool request_valid(std::string_view method, const nlohmann::json&);
[[nodiscard]] bool preview_valid(const nlohmann::json&);
[[nodiscard]] bool result_matches_request(const nlohmann::json& request,
                                          const nlohmann::json& result);
[[nodiscard]] bool acknowledged_native_start_valid(const nlohmann::json& request,
                                                   const nlohmann::json& result,
                                                   bool native_started);
[[nodiscard]] bool partial_valid(const nlohmann::json& request, const nlohmann::json& journal);
} // namespace managed_routing_detail
} // namespace sunny::infrastructure
