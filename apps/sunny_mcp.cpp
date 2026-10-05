/**
 * @file sunny_mcp.cpp
 * @brief sunny-mcp server binary entry point
 *
 * Standalone executable that:
 * 1. Creates Orchestrator
 * 2. Reads explicit production JSON, or legacy environment migration inputs
 * 3. Registers MCP tools (Ableton-mutating tools decline while offline)
 * 4. Runs MCP server loop on stdio
 *
 * Production configuration: SUNNY_CONFIG_PATH selects a strict schema1 JSON file.
 * Legacy configuration (explicit compatibility and migration input):
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

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
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
#include <sunny/infrastructure/runtime_configuration.hpp>

namespace {
std::filesystem::path utf8_path(std::string_view value) {
    return std::filesystem::path(std::u8string(value.begin(), value.end()));
}
} // namespace

int run_server(const sunny::infrastructure::ClientRuntimeConfiguration& configuration) {
    using namespace sunny::infrastructure;

    Orchestrator orchestrator;
    McpServer server;
    McpSession session;

    std::cerr << "sunny-mcp: configuration contract: "
              << (configuration.legacy_environment ? "legacy_environment"
                                                   : SUNNY_CONFIGURATION_CONTRACT)
              << "\n";
    if (configuration.workspace) {
        const auto admission = acquire_workspace_writer(session, configuration.workspace->path);
        if (!admission) throw std::runtime_error(admission.error().message);
        const auto& path = *admission;
        session.realization->workspace_path = std::filesystem::absolute(path).string();
        if (configuration.workspace->recover_backup) {
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
                    "; inspect the file or explicitly select backup recovery in the configuration");
            std::cerr << "sunny-mcp: restored authored workspace from " << path << "\n";
        } else {
            std::cerr << "sunny-mcp: new workspace; use workspace_save with path " << path
                      << " to retain authored state\n";
        }
    }

    std::unique_ptr<TcpTransport> transport;
    if (configuration.transport) {
        const auto& config = *configuration.transport;
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
        std::cerr << "sunny-mcp: running offline "
                  << "(theory and IR tools available, Ableton tools decline)\n";
    }

    // The TCP overload lets offline declines name the actual connection failure.
    BridgeDispatcher dispatcher = transport ? BridgeDispatcher(*transport) : BridgeDispatcher();
    dispatcher.set_ordinary_store_provider([&session](bool mutation) {
        auto& runtime = *session.realization;
        if (mutation && !runtime.namespace_saved_durably)
            throw std::runtime_error("Save this workspace durably before native authoring");
        if (!runtime.metadata.history_base_directory)
            throw std::runtime_error("Save this workspace durably through workspace_save; native "
                                     "history is unavailable");
        if (!runtime.store) {
            auto opened = RealizationStore::open(*runtime.metadata.history_base_directory,
                                                 runtime.metadata.workspace_namespace,
                                                 RealizationStoreMode::OpenExisting);
            if (!opened) {
                runtime.history_error = opened.error().message;
                throw std::runtime_error("Native history is unavailable: " +
                                         opened.error().message);
            }
            runtime.store = std::move(*opened);
            runtime.history_error.reset();
        }
        if (mutation && !runtime.store->native_writes_available())
            throw std::runtime_error(runtime.store->blocked_reason().value_or(
                "Native history cannot fence a write durably"));
        return runtime.store;
    });

    register_sunny_tools(server, orchestrator, dispatcher);
    register_timbre_tools(server, session.timbre);
    register_mix_tools(server, session.mix);
    register_corpus_tools(server, session.corpus);
    register_score_tools(server, session.score);
    register_project_tools(server, session, transport.get());

    server.run();

    return 0;
}

int main(int argc, char** argv) noexcept {
    try {
        using namespace sunny::infrastructure;
        if (argc == 2 && std::string_view(argv[1]) == "--configuration-contract") {
            std::cout << "{\"configuration_schema_version\":" << SUNNY_CONFIGURATION_SCHEMA_VERSION
                      << ",\"configuration_contract\":\"" << SUNNY_CONFIGURATION_CONTRACT
                      << "\"}\n";
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--validate-config") {
            // This explicit file validation never reads or merges process environment.
            std::cout << validate_client_configuration_file(utf8_path(argv[2])) << "\n";
            return 0;
        }
        if (argc == 3 && std::string_view(argv[1]) == "--migrate-config") {
            migrate_client_environment(runtime_environment(), utf8_path(argv[2]));
            std::cout << "{\"configuration_schema_version\":" << SUNNY_CONFIGURATION_SCHEMA_VERSION
                      << ",\"configuration_contract\":\"" << SUNNY_CONFIGURATION_CONTRACT
                      << "\"}\n";
            return 0;
        }
        if (argc != 1)
            throw std::runtime_error("Usage: sunny-mcp [--configuration-contract | "
                                     "--validate-config PATH | --migrate-config OUTPUT]");
        // Complete pure validation precedes every workspace, ledger or transport effect.
        const auto configuration = load_client_configuration(runtime_environment());
        return run_server(configuration);
    } catch (const std::exception& error) {
        std::cerr << "sunny-mcp: fatal error: " << error.what() << "\n";
    } catch (...) {
        std::cerr << "sunny-mcp: fatal unknown error\n";
    }
    return 1;
}
