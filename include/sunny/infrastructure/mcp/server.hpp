/**
 * @file server.hpp
 * @brief MCP Protocol Handler (JSON-RPC 2.0 over stdio)
 *
 *
 * Implements the MCP (Model Context Protocol) server subset needed
 * for tool serving. Communicates via stdin/stdout using newline-
 * delimited JSON-RPC 2.0 and supports both the legacy handshake era
 * and the stateless 2026-07-28 discovery era.
 *
 * Handles: server/discover, initialize, tools/list, tools/call
 */

#pragma once

#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace sunny::infrastructure {

/// Tool definition for MCP registration
struct McpToolDef {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

/// MCP tool handler callback
using McpToolHandler = std::function<nlohmann::json(const nlohmann::json& params)>;

/**
 * @brief MCP Server (JSON-RPC 2.0 over stdio)
 *
 * Protocol flow:
 * 1. Read JSON-RPC request from stdin (newline-delimited)
 * 2. Dispatch to registered tool handler
 * 3. Write JSON-RPC response to stdout
 */
class McpServer {
  public:
    McpServer() = default;

    /**
     * @brief Register a tool
     *
     * @param name Tool name (e.g., "create_progression_clip")
     * @param description Human-readable description
     * @param input_schema JSON Schema for tool parameters
     * @param handler Callback invoked on tools/call
     */
    void register_tool(std::string name,
                       std::string description,
                       nlohmann::json input_schema,
                       McpToolHandler handler);

    /**
     * @brief Run the server (blocking)
     *
     * Reads stdin line by line, dispatches JSON-RPC requests,
     * writes responses to stdout. Returns when stdin closes or
     * stop() is called.
     */
    void run();

    /**
     * @brief Signal shutdown
     */
    void stop();

    /**
     * @brief Process one parsed JSON-RPC request
     *
     * This is the same dispatcher used by run(). Keeping the parsed-request
     * seam public lets embedders and tests exercise the real protocol path
     * without redirecting process-wide stdin/stdout. Calls are serialized on
     * one server instance, including handler execution; tool registration must
     * be complete before the first request.
     */
    [[nodiscard]] nlohmann::json process_request(const nlohmann::json& request);

  private:
    struct ToolEntry {
        McpToolDef definition;
        McpToolHandler handler;
    };

    std::map<std::string, ToolEntry> tools_;
    std::atomic<bool> running_{false};
    std::mutex request_mutex_;

    nlohmann::json handle_discover(const nlohmann::json& id);
    nlohmann::json handle_initialize(const nlohmann::json& id, const nlohmann::json& params);
    nlohmann::json handle_tools_list(const nlohmann::json& id, bool modern);
    nlohmann::json
    handle_tools_call(const nlohmann::json& id, const nlohmann::json& params, bool modern);

    static nlohmann::json make_response(const nlohmann::json& id, const nlohmann::json& result);
    static nlohmann::json
    make_error(const nlohmann::json& id, int code, const std::string& message);
};

} // namespace sunny::infrastructure
