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
 * Handles: server/discover, initialize, ping, tools/list, tools/call
 */

#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>

namespace sunny::infrastructure {

inline constexpr std::size_t MCP_MAX_INPUT_BYTES = std::size_t{4} * 1024 * 1024;

/// Tool definition for MCP registration
struct McpToolDef {
    std::string name;
    std::string description;
    nlohmann::json input_schema;
};

/// MCP tool handler callback
using McpToolHandler = std::function<nlohmann::json(const nlohmann::json& params)>;

/// Authoring domain routed through the request-atomic project boundary.
/// Live transport/deployment tools retain None and never enter local rollback.
enum class McpDocumentDomain : std::uint8_t { None, Score, Timbre, Mix };
using McpToolExecutor = std::function<nlohmann::json(
    McpDocumentDomain, const std::string&, const nlohmann::json&, const McpToolHandler&)>;

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

    class RegistrationScope {
      public:
        RegistrationScope(McpServer& server, McpDocumentDomain domain)
            : server_(server), previous_(server.registration_domain_) {
            server_.registration_domain_ = domain;
        }
        ~RegistrationScope() { server_.registration_domain_ = previous_; }
        RegistrationScope(const RegistrationScope&) = delete;
        RegistrationScope& operator=(const RegistrationScope&) = delete;

      private:
        McpServer& server_;
        McpDocumentDomain previous_;
    };

    /** Tag every tool in one IR registration group; explicitly clear query tags. */
    [[nodiscard]] RegistrationScope registration_scope(McpDocumentDomain domain) {
        return RegistrationScope(*this, domain);
    }
    void set_tool_document_domain(const std::string& name, McpDocumentDomain domain);
    /** Install before requests; the executor runs under the existing request mutex. */
    void set_tool_executor(McpToolExecutor executor);

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
     * @brief Run the server on the process's stdio (blocking)
     *
     * Equivalent to run(std::cin, std::cout).
     */
    void run();

    /**
     * @brief Run the server over newline-delimited streams (blocking)
     *
     * Reads one JSON-RPC message per line, dispatches it, and writes each
     * response as one line. Returns when input ends or stop() is called.
     */
    void run(std::istream& input, std::ostream& output);

    /**
     * @brief Signal shutdown
     */
    void stop();

    /**
     * @brief Process one parsed JSON-RPC message
     *
     * This is the same dispatcher used by run(). Keeping the parsed-message
     * seam public lets embedders and tests exercise the real protocol path
     * without redirecting process-wide stdin/stdout. Calls are serialized on
     * one server instance, including handler execution; tool registration must
     * be complete before the first request.
     *
     * Pre:  message is any parsed JSON value
     * Post: returns the response to write, or null when JSON-RPC forbids a
     *       reply (notifications and client responses); a message that is not
     *       a valid Request yields -32600 with a null id when none is readable
     */
    [[nodiscard]] nlohmann::json process_request(const nlohmann::json& message);

  private:
    struct ToolEntry {
        McpToolDef definition;
        McpToolHandler handler;
        McpDocumentDomain document_domain = McpDocumentDomain::None;
    };

    std::map<std::string, ToolEntry> tools_;
    std::atomic<bool> running_{false};
    std::mutex request_mutex_;
    McpDocumentDomain registration_domain_ = McpDocumentDomain::None;
    McpToolExecutor tool_executor_;

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
