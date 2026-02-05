"""MCP Server Application.

Component: SVAP001A
Domain: SV (Server) | Category: AP (Application)

The MCP server application including:
- FastMCP instance configuration
- Lifecycle management
- Tool module registration

Note: Imports are lazy to avoid requiring the mcp package
for non-server use cases (e.g., just using core theory).
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from mcp.server.fastmcp import FastMCP
    from typing import AsyncIterator, Any

__all__ = [
    "mcp",
    "lifespan",
]


def __getattr__(name: str):
    """Lazy import for mcp and lifespan."""
    if name == "mcp":
        from sunny.application.server.mcp import mcp
        return mcp
    elif name == "lifespan":
        from sunny.application.server.lifespan import lifespan
        return lifespan
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
