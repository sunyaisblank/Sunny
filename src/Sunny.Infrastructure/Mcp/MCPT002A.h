/**
 * @file MCPT002A.h
 * @brief MCP Tool Registration — Timbre IR
 *
 * Component: MCPT002A
 * Domain: IN (Infrastructure) | Category: MC (MCP)
 *
 * Registers Timbre IR workflow functions (TIWF001A) as MCP tools.
 * Manages an in-process map of TimbreProfile objects keyed by
 * profile id, so that tool calls can reference and mutate profiles
 * across a session.
 *
 * Tools registered:
 * - create_timbre_profile
 * - set_sound_source
 * - add_effect, remove_effect, reorder_effects
 * - set_parameter, get_parameter
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

#include "MCPS001A.h"

namespace Sunny::Infrastructure {

/**
 * @brief Register Timbre IR tools with an MCP server
 *
 * The tools share an internal profile store that persists for
 * the lifetime of the server. Profile IDs are used to reference
 * profiles across tool calls.
 */
void register_timbre_tools(McpServer& server);

}  // namespace Sunny::Infrastructure
