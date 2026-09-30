/**
 * @file corpus_tools.hpp
 * @brief MCP Tool Registration — Corpus IR
 *
 *
 * Registers Corpus IR workflow functions  as MCP tools.
 * Manages an in-process CorpusDatabase keyed by auto-incrementing
 * identifiers, so that tool calls can build and query a corpus
 * across a session.
 *
 * Tools registered (§6.1):
 *
 * Profile management:
 * - create_composer_profile, create_ingested_work, remove_ingested_work
 * - assign_work_to_composer, assign_work_to_period
 * - add_period_profile, set_work_metadata
 *
 * Analysis:
 * - analyze_work, rebuild_style_profile, detect_signature_patterns
 *
 * Comparison:
 * - compare_composers, analyze_evolution
 *
 * Queries:
 * - query_style_profile, find_examples
 * - get_progression_examples, get_formal_template
 * - query_how_would_x_handle
 *
 * Corpus-level:
 * - validate_corpus, get_corpus_json
 */

#pragma once

#include <memory>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/session.hpp>

namespace sunny::infrastructure {

/**
 * @brief Register Corpus IR tools with an MCP server
 *
 * The tools share an internal CorpusDatabase that persists for
 * the lifetime of the server. Composer and work IDs reference
 * entities across tool calls.
 */
void register_corpus_tools(McpServer& server, std::shared_ptr<CorpusSession> session = {});

} // namespace sunny::infrastructure
