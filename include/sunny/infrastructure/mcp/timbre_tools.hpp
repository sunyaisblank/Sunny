/**
 * @file timbre_tools.hpp
 * @brief MCP Tool Registration — Timbre IR
 *
 *
 * Registers Timbre IR workflow functions  as MCP tools.
 * Manages an in-process map of TimbreProfile objects keyed by
 * profile id, so that tool calls can reference and mutate profiles
 * across a session.
 *
 * Tools registered:
 * - create_timbre_profile
 * - set_sound_source
 * - add_effect, remove_effect, reorder_effects
 * - set_parameter, get_parameter
 * - create_modulation_lfo, create_modulation_envelope, create_step_sequencer
 * - create_macro, set_macro
 * - add_modulation
 * - add_automation
 * - set_semantic_descriptors
 * - analyze_timbre
 * - search_presets, load_preset, save_preset
 * - morph_presets
 * - validate_timbre
 */

#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

/**
 * @brief Register Timbre IR tools with an MCP server
 *
 * The tools share an internal profile store that persists for
 * the lifetime of the server. Profile IDs are used to reference
 * profiles across tool calls.
 */
void register_timbre_tools(McpServer& server, std::shared_ptr<TimbreSession> session = {});

} // namespace sunny::infrastructure
