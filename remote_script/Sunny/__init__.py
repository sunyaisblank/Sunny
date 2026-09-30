"""
Sunny Remote Script — Ableton Live control surface

Provides a TCP server that accepts JSON-RPC commands from the Sunny
C++ orchestrator (via ``TcpTransport``) and translates them
to Ableton's Live Object Model (LOM) API calls.

Installation:
    Copy or symlink this directory into the ``Remote Scripts`` folder in
    Ableton's configured User Library. The exact User Library location is
    selected in Live's Library preferences and is not tied to a Live-version
    Preferences directory.

    Then select "Sunny" as a Control Surface in
    Ableton Live > Preferences > Link, Tempo & MIDI.

Wire protocol (TCP):
    Each message is framed as:
      [4 bytes big-endian uint32: payload length] [UTF-8 JSON payload]

    Request JSON (using the version in ``bridge_contract.json``):
      {"bridge_protocol_version": <version>, "type": "get"|"set"|"call",
       "path": "song/...", "name": "...", "args": [...]}

    Response JSON:
      {"bridge_protocol_version": <version>, "success": true|false,
       "value": ..., "error": "..."}
"""

from .surface import SunnyControlSurface


def create_instance(c_instance):
    """Entry point called by Ableton Live."""
    return SunnyControlSurface(c_instance)
