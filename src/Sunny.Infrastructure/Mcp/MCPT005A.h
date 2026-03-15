/**
 * @file MCPT005A.h
 * @brief MCP Tool Registration — Score IR
 *
 * Component: MCPT005A
 * Domain: IN (Infrastructure) | Category: MC (MCP)
 *
 * Registers Score IR workflow (SIWF001A), query (SIQR001A), and
 * compilation (INWF001A) functions as MCP tools. Manages an
 * in-process map of Score objects keyed by auto-incrementing
 * identifiers, so that tool calls can create and manipulate
 * scores across a session.
 *
 * Tools registered (25, all prefixed score_):
 *
 * Composition:
 * - score_create, score_set_formal_plan, score_add_part,
 *   score_set_section_harmony
 *
 * Arrangement:
 * - score_write_melody, score_write_harmony, score_reorchestrate,
 *   score_double_part, score_set_dynamics, score_set_articulation
 *
 * Detail:
 * - score_insert_note, score_modify_note, score_delete_event,
 *   score_transpose
 *
 * Analysis:
 * - score_analyze_harmony, score_get_orchestration,
 *   score_get_reduction, score_validate, score_get_form_summary
 *
 * Serialisation:
 * - score_get_json
 *
 * Compilation:
 * - score_compile_to_midi, score_compile_to_musicxml,
 *   score_compile_to_lilypond
 *
 * Query:
 * - score_query_harmony_at, score_find_motif
 */

#pragma once

#include "MCPS001A.h"

namespace Sunny::Infrastructure {

/**
 * @brief Register Score IR tools with an MCP server
 *
 * The tools share an internal score store that persists for
 * the lifetime of the server. Score IDs reference entities
 * across tool calls.
 */
void register_score_tools(McpServer& server);

}  // namespace Sunny::Infrastructure
