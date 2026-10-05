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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iosfwd>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <sunny/infrastructure/request_control.hpp>

namespace sunny::infrastructure {

inline constexpr std::size_t MCP_MAX_INPUT_BYTES = std::size_t{4} * 1024 * 1024;
inline constexpr std::size_t MCP_MAX_OUTPUT_BYTES = std::size_t{64} * 1024 * 1024;
inline constexpr auto MCP_OUTPUT_TIMEOUT = std::chrono::seconds(5);

enum class McpReadResult { Byte, End, Stopped };

/** Owned adapters must check running at least every 50 ms and honor the write deadline. */
class McpIo {
  public:
    virtual ~McpIo() = default;
    virtual McpReadResult read(char& byte, const std::atomic<bool>& running) = 0;
    virtual bool write(std::string_view line,
                       std::chrono::steady_clock::time_point deadline,
                       const std::atomic<bool>& running) = 0;
};

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
     * Uses finite, interruptible POSIX pipe/file adapters. The supported Windows
     * client runs this Linux executable in Docker.
     */
    void run();

    /**
     * @brief Run the server over newline-delimited streams (blocking)
     *
     * Reads one JSON-RPC message per line, dispatches it, and writes each
     * response as one line. The caller must supply nonblocking streams or close
     * blocked stream operations after stop(). Arbitrary C++ streambuf operations
     * cannot be preempted; production uses the bounded McpIo adapter instead.
     */
    void run(std::istream& input, std::ostream& output);

    /** Run through an owned, interruptible adapter; output is bounded and serialized. */
    void run(McpIo& io);

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

    /** A live, successful doctor may publish a pin for future admissions only. */
    [[nodiscard]] bool publish_native_origin(const NativeOrigin& origin);

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
    std::mutex control_mutex_;
    std::map<std::string, std::shared_ptr<RequestControl>> controls_;
    bool session_stopped_ = false;
    bool input_revoked_ = false;
    std::optional<NativeOrigin> native_origin_;

    std::shared_ptr<RequestControl> admit_request(const nlohmann::json& message);
    void retire_request(const std::shared_ptr<RequestControl>& control);
    bool cancel_notification(const nlohmann::json& message);
    void revoke_native_input();
    static std::optional<nlohmann::json> preflight(const nlohmann::json& message);

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
