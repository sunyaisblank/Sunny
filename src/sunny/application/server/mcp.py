"""FastMCP Server Instance.

Component: SVMC001A
Domain: SV (Server) | Category: MC (MCP)

Creates and configures the FastMCP server instance.
All tool modules import and register with this instance.
"""

from __future__ import annotations

from mcp.server.fastmcp import FastMCP

from sunny.application.server.lifespan import lifespan

# Create the FastMCP server instance
mcp = FastMCP(
    "Sunny",
    lifespan=lifespan,
)
