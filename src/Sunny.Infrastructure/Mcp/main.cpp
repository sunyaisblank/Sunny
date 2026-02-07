/**
 * @file main.cpp
 * @brief sunny-mcp server binary entry point
 *
 * Standalone executable that:
 * 1. Creates Orchestrator
 * 2. Registers MCP tools
 * 3. Runs MCP server loop on stdio
 *
 * Usage:
 *   ./sunny-mcp                   # Start server on stdio
 *   echo '{"jsonrpc":"2.0","method":"initialize","params":{},"id":1}' | ./sunny-mcp
 */

#include "MCPS001A.h"
#include "MCPT001A.h"
#include "Application/INOR001A.h"

int main() {
    Sunny::Infrastructure::Orchestrator orchestrator;
    Sunny::Infrastructure::McpServer server;

    Sunny::Infrastructure::register_sunny_tools(server, orchestrator);

    server.run();

    return 0;
}
