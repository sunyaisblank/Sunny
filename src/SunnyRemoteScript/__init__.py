"""
Sunny Remote Script — Ableton Live Control Surface

Provides a TCP server that accepts JSON-RPC commands from the Sunny
C++ orchestrator (via TcpTransport / INTP001A) and translates them
to Ableton's Live Object Model (LOM) API calls.

Installation:
    Copy or symlink this directory to:
    C:\\Users\\<user>\\AppData\\Roaming\\Ableton\\Live 12\\Preferences\\User Remote Scripts\\SunnyRemoteScript

    Then select "SunnyRemoteScript" as a Control Surface in
    Ableton Live > Preferences > Link, Tempo & MIDI.

Wire protocol (TCP):
    Each message is framed as:
      [4 bytes big-endian uint32: payload length] [UTF-8 JSON payload]

    Request JSON:
      {"type": "get"|"set"|"call", "path": "...", "name": "...", "args": [...]}

    Response JSON:
      {"success": true|false, "value": ..., "error": "..."}
"""

from .surface import SunnyControlSurface


def create_instance(c_instance):
    """Entry point called by Ableton Live."""
    return SunnyControlSurface(c_instance)
