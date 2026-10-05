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


from .configuration import DEFAULT_BIND_HOST as DEFAULT_BIND_HOST
from .configuration import DEFAULT_PORT as DEFAULT_PORT
from .configuration import load_native_configuration
from .diagnostics import RemoteLog
from .handler import LomHandler
from .managed import ManagedRegistry
from .server import TcpServer

if TYPE_CHECKING:
    pass

logger = logging.getLogger("sunny.remote_script")

# Scheduling deadline: a request Live's main thread has not begun by then is
# cancelled. Mirrored by SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE in the native
# transport, whose response deadline must exceed it (test_bridge_lifecycle.py).
LOM_REQUEST_TIMEOUT_SECONDS = 10.0


def _server_configuration() -> tuple[str, int]:
    """Read and validate the Remote Script's bridge configuration."""
    host, port, _ = load_native_configuration()
    return host, port


class SunnyControlSurface(ControlSurface):
    """Ableton Control Surface that hosts a TCP command server."""

    def __init__(self, c_instance):
        # Validate every configured role before Live, logging, durable state or sockets.
        host, port = _server_configuration()
        super().__init__(c_instance)
        self._initialise_request_lifecycle()
        # Every "sunny.*" record from this script is retained for clients that
        # cannot see Live's Log.txt (sunny_get_remote_log).
        self._remote_log = RemoteLog()
        sunny_logger = logging.getLogger("sunny")
        sunny_logger.setLevel(logging.INFO)
        sunny_logger.addHandler(self._remote_log)
        self._managed_registry = ManagedRegistry(self)
        self._handler = LomHandler(
            self,
            self._remote_log,
            envelope_authorizer=self._managed_registry.authorize_envelope,
            managed_registry=self._managed_registry,
        )
        self._managed_registry.attach_handler(self._handler)
        self._server = TcpServer(
            host=host,
            port=port,
            handler=self._process_request,
        )
        self._server_thread: threading.Thread | None = None
        self._start_server()
        contract = "versioned_json" if "SUNNY_CONFIG_PATH" in os.environ else "legacy_environment"
        self.log_message(f"Sunny configuration contract: {contract}")
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
        if LomHandler.is_remote_log_request(request):
            # Reads only the script's own log: no Live object, no main thread.
            return self._handler.handle(request)
        response = self._run_on_main_thread(request)
        self._log_outcome(request, response)
        return response

    @staticmethod
    def _log_outcome(request: dict, response: dict) -> None:
        """Record one line per request so remote clients can follow activity."""
        if isinstance(request, dict):
            operation = "{} {} {}".format(
                request.get("type"), request.get("path"), request.get("name")
            )
        else:
            operation = "malformed request"
        value = response.get("value")
        ordinary_outcome = (
            value.get("outcome")
            if isinstance(request, dict)
            and request.get("name")
            in {"sunny_ordinary_prepare", "sunny_ordinary_execute", "sunny_ordinary_operation"}
            and isinstance(value, dict)
            else None
        )
        if response.get("success") and ordinary_outcome in {"declined", "partial", "unknown_epoch"}:
            logger.warning("%s: %s: %s", operation, ordinary_outcome, value.get("error"))
        elif response.get("success"):
            logger.info("%s: ok", operation)
        else:
            logger.warning("%s: %s", operation, response.get("error"))

    def _run_on_main_thread(self, request: dict) -> dict:
        """Queue one request for Live's main thread and wait for its outcome."""
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
        remote_log = getattr(self, "_remote_log", None)
        if remote_log is not None:
            logging.getLogger("sunny").removeHandler(remote_log)
        super().disconnect()
