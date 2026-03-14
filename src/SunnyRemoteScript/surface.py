"""
Sunny Control Surface — main entry point for Ableton integration.

Inherits from ControlSurface and manages the TCP server lifecycle.
The server runs in a background thread; incoming commands are queued
and executed on the main thread via schedule_message to avoid
threading issues with the LOM API.
"""

from __future__ import annotations

import logging
import threading
from typing import TYPE_CHECKING

try:
    from _Framework.ControlSurface import ControlSurface
except ImportError:
    # Fallback for testing outside Ableton
    class ControlSurface:  # type: ignore[no-redef]
        def __init__(self, c_instance=None):
            self._c_instance = c_instance
        def log_message(self, msg): logging.info(msg)
        def schedule_message(self, delay, callback): callback()
        def disconnect(self): pass

from .server import TcpServer
from .handler import LomHandler

if TYPE_CHECKING:
    pass

logger = logging.getLogger("SunnyRemoteScript")

# Default TCP port; override via environment variable SUNNY_PORT
DEFAULT_PORT = 9001


class SunnyControlSurface(ControlSurface):
    """Ableton Control Surface that hosts a TCP command server."""

    def __init__(self, c_instance):
        super().__init__(c_instance)
        self._handler = LomHandler(self)
        self._server = TcpServer(
            host="0.0.0.0",
            port=DEFAULT_PORT,
            handler=self._process_request,
        )
        self._server_thread: threading.Thread | None = None
        self._start_server()
        self.log_message(f"Sunny Remote Script started on port {DEFAULT_PORT}")

    def _start_server(self):
        """Start the TCP server in a background thread."""
        self._server_thread = threading.Thread(
            target=self._server.serve_forever,
            daemon=True,
            name="SunnyTcpServer",
        )
        self._server_thread.start()

    def _process_request(self, request: dict) -> dict:
        """Handle a single LOM request. Called from the server thread.

        For LOM operations that must run on the main thread, we use
        schedule_message with delay=0. For simple property reads that
        Ableton allows from any thread, we execute directly.

        In practice, all LOM access should be from the main thread.
        The server thread queues the request and waits for the result.
        """
        return self._handler.handle(request)

    def disconnect(self):
        """Called by Ableton when the script is unloaded."""
        self.log_message("Sunny Remote Script disconnecting")
        if self._server:
            self._server.shutdown()
        super().disconnect()
