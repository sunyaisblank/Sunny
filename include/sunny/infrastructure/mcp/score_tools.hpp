/**
 * @file score_tools.hpp
 * @brief MCP Tool Registration — Score IR
 *
 *
 * Registers Score IR workflow, query, and compilation functions as MCP
 * tools. Manages an
 * in-process map of Score objects keyed by auto-incrementing
 * identifiers, so that tool calls can create and manipulate
 * scores across a session.
 *
 * Tools registered (29, all prefixed score_):
 *
 * Composition:
 * - score_create, score_set_tuning, score_set_formal_plan, score_add_part,
 *   score_set_section_harmony
 *
 * Arrangement:
 * - score_write_melody, score_write_harmony, score_reorchestrate,
 *   score_double_part, score_set_dynamics, score_set_articulation,
 *   score_set_articulation_mapping
 *
 * Detail:
 * - score_insert_note, score_insert_chord_symbol, score_modify_note, score_delete_event,
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
 *   score_compile_to_lilypond, score_compile_to_ableton
 *
 * Query:
 * - score_query_harmony_at, score_find_motif
 */

#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

class LomTransport;

/**
 * @brief Register Score IR tools with an MCP server
 *
 * The tools share an internal score store that persists for
 * the lifetime of the server. Score IDs reference entities
 * across tool calls.
 */
void register_score_tools(McpServer& server,
                          LomTransport* transport = nullptr,
                          std::shared_ptr<ScoreSession> session = {});

} // namespace sunny::infrastructure
