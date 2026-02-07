/**
 * @file MCPS001A.h
 * @brief MCP Protocol Handler (JSON-RPC 2.0 over stdio)
 *
 * Component: MCPS001A
 * Domain: IN (Infrastructure) | Category: MC (MCP)
 *
 * Implements the MCP (Model Context Protocol) server subset needed
 * for tool serving. Communicates via stdin/stdout using newline-
 * delimited JSON-RPC 2.0.
 *
 * Handles: initialize, tools/list, tools/call
 */

#pragma once

#include <functional>
#include <map>
#include <string>
#include <nlohmann/json.hpp>

namespace Sunny::Infrastructure {

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
    void register_tool(std::string name, std::string description,
                       nlohmann::json input_schema, McpToolHandler handler);

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

private:
    struct ToolEntry {
        McpToolDef definition;
        McpToolHandler handler;
    };

    std::map<std::string, ToolEntry> tools_;
    bool running_{false};

    nlohmann::json handle_request(const nlohmann::json& request);
    nlohmann::json handle_initialize(const nlohmann::json& id);
    nlohmann::json handle_tools_list(const nlohmann::json& id);
    nlohmann::json handle_tools_call(const nlohmann::json& id,
                                     const nlohmann::json& params);

    static nlohmann::json make_response(const nlohmann::json& id,
                                        const nlohmann::json& result);
    static nlohmann::json make_error(const nlohmann::json& id,
                                     int code,
                                     const std::string& message);
};

}  // namespace Sunny::Infrastructure
