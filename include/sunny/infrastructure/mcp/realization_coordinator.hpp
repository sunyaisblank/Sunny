/** Compose existing fenced native tools under the owning serialized request. */
#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>

namespace sunny::infrastructure {
using NativeAuthoringTools = std::map<std::string, McpToolHandler>;

// Preparation validates all selected authored domains before any native fence.
// Verification is a fresh read of the resulting retained native cohorts.
void register_project_realization_coordinator_tools(
    McpServer& server,
    std::shared_ptr<const NativeAuthoringTools> tools,
    McpToolHandler prepare,
    McpToolHandler verify,
    nlohmann::json source_selection_schema,
    nlohmann::json effect_selection_schema,
    nlohmann::json mixer_selection_schema,
    nlohmann::json routing_selection_schema);
} // namespace sunny::infrastructure
