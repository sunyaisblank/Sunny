/**
 * @file project_session.hpp
 * @brief Request-atomic authoring over the canonical Score/Timbre/Mix stores
 */
#pragma once

#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

/**
 * Register binding, default sibling construction, inspection, and project
 * history. Install the execution boundary for every registered IR mutation.
 * The server serializes the entire boundary under its request mutex.
 */
void register_project_authoring_tools(McpServer& server, const McpSession& session);

/** Capture unchanged child schemas and the preset library for review/history. */
[[nodiscard]] sunny::core::Result<ProjectSnapshot> snapshot_project(const McpSession& session,
                                                                    const ProjectRecord& project);

} // namespace sunny::infrastructure
