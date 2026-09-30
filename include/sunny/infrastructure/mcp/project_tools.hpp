/**
 * @file project_tools.hpp
 * @brief MCP tools for cross-IR validation and Ableton deployment
 */

#pragma once

#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

class LomTransport;

/**
 * @brief Register validation, guarded plan/apply, and convenience deployment tools
 *
 * The supplied session must also be passed to the Score, Timbre, and Mix
 * registration groups so all tools resolve the same identity-stable objects.
 */
void register_project_tools(McpServer& server,
                            const McpSession& session,
                            LomTransport* transport = nullptr);

} // namespace sunny::infrastructure
