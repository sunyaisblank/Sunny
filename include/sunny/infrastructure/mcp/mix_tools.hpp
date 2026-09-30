/**
 * @file mix_tools.hpp
 * @brief MCP Tool Registration — Mix IR
 *
 *
 * Registers Mix IR workflow functions  as MCP tools.
 * Manages an in-process MixGraph store keyed by graph id, so that
 * tool calls can reference and mutate graphs across a session.
 *
 * Tools registered (§12.1):
 *
 * Configuration:
 * - create_mix_graph, create_group_bus, create_aux_bus
 * - assign_channel_to_group, set_channel_send
 * - apply_seating_template, set_output_format
 *
 * Processing:
 * - add_channel_effect, add_bus_effect, add_aux_effect, add_master_effect
 * - map_mix_effect_parameter
 * - set_channel_level, set_channel_relative_level, resolve_mix_fader_levels
 * - set_channel_pan, set_channel_depth
 * - set_loudness_target
 *
 * Automation:
 * - add_mix_automation
 *
 * Analysis / Reference:
 * - create_reference_profile, compare_to_reference
 *
 * Intent:
 * - set_channel_intent, set_group_intent
 *
 * Validation / Serialisation:
 * - validate_mix, get_mix_json
 *
 * Compilation:
 * - compile_mix
 */

#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

class LomTransport;

/**
 * @brief Register Mix IR tools with an MCP server
 *
 * The tools share an internal graph store that persists for
 * the lifetime of the server. Graph IDs reference graphs
 * across tool calls.
 */
void register_mix_tools(McpServer& server,
                        LomTransport* transport = nullptr,
                        std::shared_ptr<MixSession> session = {});

} // namespace sunny::infrastructure
