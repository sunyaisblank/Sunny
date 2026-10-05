/** Durable admission of native mutations derived from owning project documents. */
#pragma once

#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

void register_project_realization_tools(McpServer& server,
                                        const McpSession& session,
                                        LomTransport* transport);

} // namespace sunny::infrastructure
