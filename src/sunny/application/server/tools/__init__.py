"""Tool Modules for Sunny MCP Server.

Component: SVTL001A
Domain: SV (Server) | Category: TL (Tools)

Organized tool modules by functional category:
- session: Session info and transport control (SVTS001A)
- track: Track creation and management (SVTT001A)
- clip: Clip creation and note manipulation (SVTC001A)
- theory: Music theory computations (SVTH001A)
- mixer: Volume, pan, mute, solo controls (SVTM001A)
- device: Device and parameter management (SVTD001A)
- arrangement: Timeline and locator operations (SVTA001A)
- automation: Automation envelope control (SVAU001A)
- browser: Browser navigation and loading (SVTB001A)
- snapshot: Project snapshot management (SVSS001A)
- harmony: Advanced harmony and voice leading (SVHY001A)
- orchestration: Instrumentation guidance (SVOR001A)

Each module registers its tools with the shared mcp instance via decorator.
"""

from __future__ import annotations

# Import all tool modules to register them with mcp
# These imports have side effects (tool registration via @mcp.tool() decorators)
from sunny.application.server.tools import (
    session,
    track,
    clip,
    theory,
    mixer,
    device,
    arrangement,
    automation,
    browser,
    snapshot,
    harmony,
    orchestration,
)

__all__ = [
    "session",
    "track",
    "clip",
    "theory",
    "mixer",
    "device",
    "arrangement",
    "automation",
    "browser",
    "snapshot",
    "harmony",
    "orchestration",
]
