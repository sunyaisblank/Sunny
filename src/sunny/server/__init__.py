"""Sunny - Server Module.

This module contains the FastMCP server, tools, and transport layer
for communicating with Ableton Live.

Note: The MCP server has been restructured into the application layer.
This module provides backward-compatible re-exports.
The mcp object and main function are lazily imported to avoid
requiring the mcp package for non-server use cases.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

if TYPE_CHECKING:
    from mcp.server.fastmcp import FastMCP

__all__ = ["mcp", "main"]


def __getattr__(name: str):
    """Lazy import for mcp and main."""
    if name == "mcp":
        from sunny.application.server.mcp import mcp
        return mcp
    elif name == "main":
        from sunny.host.main import main
        return main
    raise AttributeError(f"module {__name__!r} has no attribute {name!r}")
