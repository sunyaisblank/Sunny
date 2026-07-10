/**
 * @file main.cpp
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
 *
 * Usage:
 *   ./sunny-mcp                   # Start server on stdio (offline mode)
 *   SUNNY_ABLETON_HOST=127.0.0.1 ./sunny-mcp   # Connect to Ableton
 *
 * Diagnostics go to stderr; stdout carries only the MCP protocol.
 */

#include "MCPS001A.h"
#include "MCPT001A.h"
#include "MCPT002A.h"
#include "MCPT003A.h"
#include "MCPT004A.h"
#include "MCPT005A.h"
#include "Application/INOR001A.h"
#include "Bridge/INBR002A.h"
#include "Bridge/INTP001A.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

int main() {
    using namespace Sunny::Infrastructure;

    Orchestrator orchestrator;
    McpServer server;

    // Ableton connection is opt-in: without SUNNY_ABLETON_HOST the server
    // runs offline and Ableton-mutating tools decline loudly.
    std::unique_ptr<TcpTransport> transport;
    if (const char* host = std::getenv("SUNNY_ABLETON_HOST")) {
        TcpConfig config;
        config.host = host;
        if (const char* port = std::getenv("SUNNY_TCP_PORT")) {
            config.port = static_cast<std::uint16_t>(std::stoi(port));
        }

        transport = std::make_unique<TcpTransport>(config);
        if (transport->connect()) {
            std::cerr << "sunny-mcp: connected to Ableton at "
                      << config.host << ":" << config.port << "\n";
        } else {
            std::cerr << "sunny-mcp: could not connect to Ableton at "
                      << config.host << ":" << config.port
                      << "; Ableton tools will decline\n";
            transport.reset();
        }
    } else {
        std::cerr << "sunny-mcp: SUNNY_ABLETON_HOST not set; running offline "
                  << "(theory and IR tools available, Ableton tools decline)\n";
    }

    BridgeDispatcher dispatcher(transport.get());

    register_sunny_tools(server, orchestrator, dispatcher);
    register_timbre_tools(server);
    register_mix_tools(server);
    register_corpus_tools(server);
    register_score_tools(server);

    server.run();

    return 0;
}
