/** @file score_notation_tools.hpp
 *  @brief Exact tempo, standalone tuplet and region authoring tools.
 */
#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

/** Register ten Score-domain tools using the supplied shared session/history. */
void register_score_notation_tools(McpServer& server, std::shared_ptr<ScoreSession> session);

} // namespace sunny::infrastructure
