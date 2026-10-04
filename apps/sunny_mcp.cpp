/**
 * @file sunny_mcp.cpp
 * @brief sunny-mcp server binary entry point
 *
 * Standalone executable that:
 * 1. Creates Orchestrator
 * 2. Connects to Ableton via TcpTransport when SUNNY_ABLETON_HOST is set
 * 3. Registers MCP tools (Ableton-mutating tools decline while offline)
 * 4. Runs MCP server loop on stdio
 *
 * Configuration (environment):
 *   SUNNY_ABLETON_HOST  Remote Script host; unset means offline mode
 *   SUNNY_TCP_PORT      Remote Script port (default 9001)
 *   SUNNY_WORKSPACE_PATH  Restore an explicitly saved workspace at startup
 *   SUNNY_WORKSPACE_RECOVERY  'backup' explicitly restores .bak without repairing the main file
 *
 * Usage:
 *   ./sunny-mcp                   # Start server on stdio (offline mode)
 *   SUNNY_ABLETON_HOST=127.0.0.1 ./sunny-mcp   # Connect to Ableton
 *
 * Diagnostics go to stderr; stdout carries only the MCP protocol.
 */

#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sunny/infrastructure/ableton/dispatcher.hpp>
#include <sunny/infrastructure/ableton/transport.hpp>
#include <sunny/infrastructure/mcp/core_tools.hpp>
#include <sunny/infrastructure/mcp/corpus_tools.hpp>
#include <sunny/infrastructure/mcp/mix_tools.hpp>
#include <sunny/infrastructure/mcp/project_tools.hpp>
#include <sunny/infrastructure/mcp/score_tools.hpp>
#include <sunny/infrastructure/mcp/server.hpp>
#include <sunny/infrastructure/mcp/timbre_tools.hpp>
#include <sunny/infrastructure/mcp/workspace_state.hpp>
#include <sunny/infrastructure/orchestrator.hpp>

namespace {

std::optional<std::uint16_t> parse_port(std::string_view text) noexcept {
    unsigned int parsed = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), parsed);
    if (error != std::errc{} || end != text.data() + text.size() || parsed == 0 ||
        parsed > std::numeric_limits<std::uint16_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(parsed);
}

} // namespace

int run_server() {
    using namespace sunny::infrastructure;

    Orchestrator orchestrator;
    McpServer server;
    McpSession session;

    if (const char* workspace = std::getenv("SUNNY_WORKSPACE_PATH")) {
        if (*workspace == '\0') throw std::runtime_error("SUNNY_WORKSPACE_PATH must not be empty");
        const std::filesystem::path path(workspace);
        session.realization->workspace_path = std::filesystem::absolute(path).string();
        const char* recovery = std::getenv("SUNNY_WORKSPACE_RECOVERY");
        if (recovery && std::string_view(recovery) != "backup")
            throw std::runtime_error("SUNNY_WORKSPACE_RECOVERY must be 'backup' or unset");
        if (recovery) {
            const auto restored = recover_workspace_backup(session, path, true);
            if (!restored)
                throw std::runtime_error("Workspace backup recovery failed: " +
                                         restored.error().message);
            std::cerr << "sunny-mcp: restored explicit backup for " << path
                      << "; main file remains unchanged until workspace_save\n";
        } else if (std::filesystem::exists(path)) {
            const auto opened = open_workspace(session, path);
            if (!opened)
                throw std::runtime_error(
                    "Workspace startup open failed: " + opened.error().message +
                    "; inspect the file or explicitly set "
                    "SUNNY_WORKSPACE_RECOVERY=backup");
            std::cerr << "sunny-mcp: restored authored workspace from " << path << "\n";
        } else {
            std::cerr << "sunny-mcp: new workspace; use workspace_save with path " << path
                      << " to retain authored state\n";
        }
    } else if (std::getenv("SUNNY_WORKSPACE_RECOVERY")) {
        throw std::runtime_error("SUNNY_WORKSPACE_RECOVERY requires SUNNY_WORKSPACE_PATH");
    }

    // Ableton connection is opt-in: without SUNNY_ABLETON_HOST the server
    // runs offline and Ableton-mutating tools decline loudly.
    std::unique_ptr<TcpTransport> transport;
    if (const char* host = std::getenv("SUNNY_ABLETON_HOST")) {
        TcpConfig config;
        config.host = host;
        if (const char* port = std::getenv("SUNNY_TCP_PORT")) {
            if (const auto parsed_port = parse_port(port)) {
                config.port = *parsed_port;
            } else {
                std::cerr << "sunny-mcp: ignoring invalid SUNNY_TCP_PORT; using " << config.port
                          << "\n";
            }
        }

        transport = std::make_unique<TcpTransport>(config);
        if (transport->connect()) {
            std::cerr << "sunny-mcp: connected to Ableton at " << config.host << ":" << config.port
                      << "\n";
        } else {
            std::cerr << "sunny-mcp: could not connect to Ableton at " << config.host << ":"
                      << config.port;
            if (const auto failure = transport->last_connect_failure())
                std::cerr << " (" << describe(*failure) << ")";
            std::cerr << "; each Ableton tool call will try to connect again\n";
        }
    } else {
        std::cerr << "sunny-mcp: SUNNY_ABLETON_HOST not set; running offline "
                  << "(theory and IR tools available, Ableton tools decline)\n";
    }

    // The TCP overload lets offline declines name the actual connection failure.
    BridgeDispatcher dispatcher = transport ? BridgeDispatcher(*transport) : BridgeDispatcher();

    register_sunny_tools(server, orchestrator, dispatcher);
    register_timbre_tools(server, session.timbre);
    register_mix_tools(server, session.mix);
    register_corpus_tools(server, session.corpus);
    register_score_tools(server, session.score);
    register_project_tools(server, session, transport.get());

    server.run();

    return 0;
}

int main() noexcept {
    try {
        return run_server();
    } catch (const std::exception& error) {
        std::cerr << "sunny-mcp: fatal error: " << error.what() << "\n";
    } catch (...) {
        std::cerr << "sunny-mcp: fatal unknown error\n";
    }
    return 1;
}
