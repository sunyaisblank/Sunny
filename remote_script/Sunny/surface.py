"""
Sunny control surface — main entry point for Ableton integration.

Inherits from ControlSurface and manages the TCP server lifecycle.
The server runs in a background thread; incoming commands are queued
and executed on the main thread via schedule_message to avoid
threading issues with the LOM API.
"""

from __future__ import annotations

import logging
import os
import threading
from collections.abc import Callable
from typing import TYPE_CHECKING

try:
    from _Framework.ControlSurface import ControlSurface
except ImportError as framework_error:
    try:
        import Live as _live_runtime  # noqa: F401
    except ImportError:
        # Test-only fallback. If the Live module exists, an incompatible
        # Control Surface framework must fail instead of running LOM work on
        # the socket thread through this synchronous scheduler.
        class ControlSurface:  # type: ignore[no-redef]
            def __init__(self, c_instance=None):
                self._c_instance = c_instance

            def log_message(self, msg):
                logging.info(msg)

            def schedule_message(self, delay, callback):
                callback()

            def disconnect(self):
                pass

    else:
        raise RuntimeError(
            "Sunny requires a Live-compatible _Framework.ControlSurface host"
        ) from framework_error


from .handler import LomHandler
from .server import TcpServer

if TYPE_CHECKING:
    pass

logger = logging.getLogger("sunny.remote_script")

# The bridge is loopback-only unless the user deliberately exposes it.
DEFAULT_BIND_HOST = "127.0.0.1"
DEFAULT_PORT = 9001
# Scheduling deadline: a request Live's main thread has not begun by then is
# cancelled. Mirrored by SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE in the native
# transport, whose response deadline must exceed it (test_bridge_lifecycle.py).
LOM_REQUEST_TIMEOUT_SECONDS = 10.0


def _server_configuration() -> tuple[str, int]:
    """Read and validate the Remote Script's bridge configuration."""
    host = os.environ.get("SUNNY_BIND_HOST", DEFAULT_BIND_HOST)
    raw_port = os.environ.get("SUNNY_TCP_PORT", str(DEFAULT_PORT))
    try:
        port = int(raw_port)
    except ValueError:
        logger.warning("Invalid SUNNY_TCP_PORT=%r; using %d", raw_port, DEFAULT_PORT)
        port = DEFAULT_PORT
    if not 1 <= port <= 65_535:
        logger.warning("Out-of-range SUNNY_TCP_PORT=%r; using %d", raw_port, DEFAULT_PORT)
        port = DEFAULT_PORT
    return host, port


class SunnyControlSurface(ControlSurface):
    """Ableton Control Surface that hosts a TCP command server."""

    def __init__(self, c_instance):
        super().__init__(c_instance)
        self._initialise_request_lifecycle()
        self._handler = LomHandler(self)
        host, port = _server_configuration()
        self._server = TcpServer(
            host=host,
            port=port,
            handler=self._process_request,
        )
        self._server_thread: threading.Thread | None = None
        self._start_server()
        self.log_message(f"Sunny Remote Script started on {host}:{port}")

    def _initialise_request_lifecycle(self) -> None:
        self._request_lock = threading.Lock()
        self._disconnected = False
        self._pending_requests: set[Callable[[str], None]] = set()

    def _start_server(self) -> None:
        """Start the TCP server in a background thread."""
        self._server_thread = threading.Thread(
            target=self._server.serve_forever,
            daemon=True,
            name="SunnyTcpServer",
        )
        self._server_thread.start()

    def _process_request(self, request: dict) -> dict:
        """Handle a single LOM request. Called from the server thread.

        All LOM access runs on the main thread. The server thread queues
        the request and waits for its result. A request that has not begun
        by the scheduling deadline is cancelled before it can touch Live.
        Once a request has begun, wait for its definite outcome: returning
        a timeout while a mutating LOM call remained in flight would make a
        retry unsafe and could invalidate the deployment journal.
        """
        completed = threading.Event()
        outcome: dict[str, dict] = {}
        state = {"phase": "queued"}

        def cancel(reason: str) -> None:
            # All phase transitions and pending-set access hold _request_lock.
            state["phase"] = "cancelled"
            outcome["response"] = {"success": False, "error": reason}
            self._pending_requests.discard(cancel)
            completed.set()

        with self._request_lock:
            if self._disconnected:
                return {"success": False, "error": "Sunny Remote Script disconnected"}
            self._pending_requests.add(cancel)

        def handle_on_main_thread() -> None:
            with self._request_lock:
                if state["phase"] == "cancelled":
                    return
                state["phase"] = "started"
                self._pending_requests.discard(cancel)
            try:
                outcome["response"] = self._handler.handle(request)
            except Exception as exc:
                logger.error("Scheduled LOM request failed: %s", exc, exc_info=True)
                outcome["response"] = {"success": False, "error": str(exc)}
            finally:
                with self._request_lock:
                    state["phase"] = "completed"
                completed.set()

        try:
            self.schedule_message(0, handle_on_main_thread)
        except Exception as exc:
            logger.error("Could not schedule LOM request: %s", exc, exc_info=True)
            with self._request_lock:
                if state["phase"] == "queued":
                    cancel(f"Could not schedule LOM request: {exc}")

        if not completed.wait(LOM_REQUEST_TIMEOUT_SECONDS):
            with self._request_lock:
                if state["phase"] == "queued":
                    cancel(
                        "Timed out before Ableton's main thread began the request; "
                        "request cancelled"
                    )

            # If the request started, there is no public Live cancellation
            # primitive; a second timeout would manufacture an ambiguous
            # outcome. Cancelled requests have already signalled completion.
            completed.wait()
        return outcome["response"]

    def disconnect(self) -> None:
        """Called by Ableton when the script is unloaded."""
        self.log_message("Sunny Remote Script disconnecting")
        with self._request_lock:
            self._disconnected = True
            for cancel in tuple(self._pending_requests):
                cancel("Sunny Remote Script disconnected; queued request cancelled")
        if self._server:
            self._server.shutdown()
        super().disconnect()
