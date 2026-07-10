/**
 * @file MCPT001A.h
 * @brief MCP Tool Registration
 *
 * Component: MCPT001A
 * Domain: IN (Infrastructure) | Category: MC (MCP)
 *
 * Registers Sunny operations as MCP tools, mapping tool calls
 * to Orchestrator and Core function invocations.
 */

#pragma once

#include "MCPS001A.h"
#include "Application/INOR001A.h"
#include "Bridge/INBR002A.h"

namespace Sunny::Infrastructure {

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
void register_sunny_tools(McpServer& server, Orchestrator& orchestrator,
                          BridgeDispatcher& dispatcher);

}  // namespace Sunny::Infrastructure
