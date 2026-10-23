/** MCP file persistence; transport and Live remain outside authored workspace state. */
#pragma once

#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

void register_workspace_tools(McpServer& server, const McpSession& session);

} // namespace sunny::infrastructure
