/** Closed original-document authority for the existing musical command algebra. */
#pragma once
#include <sunny/infrastructure/ableton/lom_protocol.hpp>

namespace sunny::infrastructure {
class LomTransport;
struct LegacyPlanningAuthority {
    nlohmann::json scope;
    std::uint32_t graph_revision = 0;
    std::string workspace_namespace;
};
struct LegacyWorkflowRecipe {
    std::string kind;
    std::string intent_fingerprint;
    LegacyPlanningAuthority authority;
    // Exact {command:Command,phase:0..2} entries, mutations only.
    nlohmann::json commands = nlohmann::json::array();
};
enum class LegacyOperationOutcome {
    Prepared,
    NativePrepared,
    NotSent,
    Acknowledged,
    Declined,
    Partial,
    Indeterminate,
    UnknownEpoch,
    UnknownOperation
};
enum class LegacyOperationStage { Unsent, Prepare, Execute, Query };
struct LegacyOperationReceipt {
    nlohmann::json intent;
    LomDeliveryState delivery = LomDeliveryState::NotSent;
    LegacyOperationOutcome outcome = LegacyOperationOutcome::Prepared;
    std::optional<nlohmann::json> journal = std::nullopt;
    std::optional<std::string> error = std::nullopt;
    LegacyOperationStage stage = LegacyOperationStage::Unsent;
};
[[nodiscard]] nlohmann::json legacy_command(const LomRequest& request);
[[nodiscard]] sunny::core::Result<LomRequest> legacy_command_from_json(const nlohmann::json&);
[[nodiscard]] bool legacy_request_valid(const std::string&, const nlohmann::json&) noexcept;
[[nodiscard]] nlohmann::json legacy_authority_to_json(const LegacyPlanningAuthority&);
[[nodiscard]] sunny::core::Result<LegacyPlanningAuthority>
legacy_authority_from_json(const nlohmann::json&);
[[nodiscard]] nlohmann::json legacy_recipe_to_json(const LegacyWorkflowRecipe&);
[[nodiscard]] sunny::core::Result<LegacyWorkflowRecipe>
legacy_recipe_from_json(const nlohmann::json&);
[[nodiscard]] nlohmann::json legacy_token(const LegacyOperationReceipt&);
[[nodiscard]] sunny::core::Result<LegacyOperationReceipt>
prepare_legacy_operation(const LegacyPlanningAuthority&,
                         const std::string& workflow,
                         const std::string& operation,
                         std::uint32_t ordinal,
                         const LomRequest&);
[[nodiscard]] sunny::core::Result<LegacyOperationReceipt>
execute_legacy_operation(const LegacyOperationReceipt&, LomTransport&);
[[nodiscard]] sunny::core::Result<LegacyOperationReceipt>
reconcile_legacy_operation(const LegacyOperationReceipt&, LomTransport&);
[[nodiscard]] nlohmann::json legacy_receipt_to_json(const LegacyOperationReceipt&);
[[nodiscard]] sunny::core::Result<LegacyOperationReceipt>
legacy_receipt_from_json(const nlohmann::json&);
} // namespace sunny::infrastructure
