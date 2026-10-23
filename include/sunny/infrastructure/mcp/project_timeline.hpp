/** Measure splicing of ScoreTime anchored sibling controls inside a project transaction. */
#pragma once

#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

[[nodiscard]] nlohmann::json relocate_project_controls(const McpSession& session,
                                                       const ProjectRecord& project,
                                                       const std::string& tool_name,
                                                       const nlohmann::json& arguments);

} // namespace sunny::infrastructure
