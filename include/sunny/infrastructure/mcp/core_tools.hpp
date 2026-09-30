/**
 * @file core_tools.hpp
 * @brief MCP Tool Registration
 *
 *
 * Registers Sunny operations as MCP tools, mapping tool calls
 * to Orchestrator and Core function invocations.
 */

#pragma once

#include <sunny/infrastructure/ableton/dispatcher.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/orchestrator.hpp>

namespace sunny::infrastructure {

/**
 * @brief Register all Sunny tools with an MCP server
 *
 * Tools registered:
 * - create_progression_clip
 * - apply_euclidean_rhythm
 * - apply_arpeggio
 * - get_scale_notes
 * - analyze_harmony
 * - generate_negative_harmony
 * - voice_lead
 * - get_ableton_session_state
 * - undo_ableton_operation
 * - redo_ableton_operation
 *
 * The three Ableton-mutating tools decline before computing when the
 * dispatcher is offline, and deliver the orchestrator's queued bridge
 * messages through it when online; pure theory tools never touch the
 * transport.
 *
 * @param server MCP server to register tools with
 * @param orchestrator Orchestrator instance for stateful operations
 * @param dispatcher Bridge dispatcher (offline dispatcher declines)
 */
void register_sunny_tools(McpServer& server,
                          Orchestrator& orchestrator,
                          BridgeDispatcher& dispatcher);

} // namespace sunny::infrastructure
