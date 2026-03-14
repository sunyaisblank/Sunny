/**
 * @file MCPT004A.h
 * @brief MCP Tool Registration — Corpus IR
 *
 * Component: MCPT004A
 * Domain: IN (Infrastructure) | Category: MC (MCP)
 *
 * Registers Corpus IR workflow functions (CIWF001A) as MCP tools.
 * Manages an in-process CorpusDatabase keyed by auto-incrementing
 * identifiers, so that tool calls can build and query a corpus
 * across a session.
 *
 * Tools registered (§6.1):
 *
 * Profile management:
 * - create_composer_profile, create_ingested_work
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

#include "MCPS001A.h"

namespace Sunny::Infrastructure {

/**
 * @brief Register Corpus IR tools with an MCP server
 *
 * The tools share an internal CorpusDatabase that persists for
 * the lifetime of the server. Composer and work IDs reference
 * entities across tool calls.
 */
void register_corpus_tools(McpServer& server);

}  // namespace Sunny::Infrastructure
