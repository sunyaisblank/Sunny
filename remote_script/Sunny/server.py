"""
TCP server using length-prefixed JSON framing.

Wire protocol:
    [4 bytes big-endian uint32: payload length] [UTF-8 JSON payload]

The server accepts one client at a time (the Sunny C++ orchestrator).
Each request is dispatched to the handler callback and the response
is sent back with the same framing. The connection persists between
requests for as long as the client keeps it open; only a frame left
incomplete is subject to a timeout.
"""

from __future__ import annotations

import json
import logging
import math
import socket
import struct
import threading
from collections.abc import Callable

from .handler import BRIDGE_PROTOCOL_VERSION

logger = logging.getLogger("sunny.remote_script.server")

HEADER_SIZE = 4  # bytes for uint32 big-endian length prefix
# Must match SUNNY_BRIDGE_MAX_WIRE_PAYLOAD in the native transport.
MAX_PAYLOAD = 16 * 1024 * 1024
# A client that stops sending part-way through a frame has desynchronised the
# stream, so an incomplete frame is abandoned after this long without
# progress. Waiting between frames is the normal state of the persistent link
# (the user may pause for any length of time) and has no deadline.
PARTIAL_FRAME_TIMEOUT_SECONDS = 30.0
# An idle receive wakes at this interval to re-check shutdown. Shutting the
# socket down from another thread wakes it at once where the platform allows;
# the interval bounds the wait where it does not.
IDLE_WAKE_SECONDS = 1.0
# With no idle deadline, TCP keepalive is what eventually releases a client
# that vanished without closing (a half-open connection), so that the
# single-client server can accept its replacement.
_KEEPALIVE_OPTIONS = (
    ("TCP_KEEPIDLE", 60),  # Linux, Windows
    ("TCP_KEEPALIVE", 60),  # macOS spelling of the idle interval
    ("TCP_KEEPINTVL", 10),
    ("TCP_KEEPCNT", 6),
)


def _versioned_response(response: object) -> dict:
    """Validate and version the exact response envelope emitted on the wire."""
    if not isinstance(response, dict):
        response = {"success": False, "error": "Handler response must be an object"}
    elif set(response) - {"success", "value", "error"}:
        response = {"success": False, "error": "Handler response contains unknown fields"}
    elif not isinstance(response.get("success"), bool):
        response = {"success": False, "error": "Handler response requires boolean success"}
    elif response["success"] and "error" in response:
        response = {"success": False, "error": "Successful handler response contained an error"}
    elif not response["success"] and (
        not isinstance(response.get("error"), str) or "value" in response
    ):
        response = {"success": False, "error": "Failed handler response requires only an error"}
    result = dict(response)
    result["bridge_protocol_version"] = BRIDGE_PROTOCOL_VERSION
    return result


class TcpServer:
    """Single-client TCP server; a surface reload creates a fresh instance."""

    def __init__(
        self,
        host: str = "127.0.0.1",
        port: int = 9001,
        handler: Callable[[dict], dict] | None = None,
        partial_frame_timeout: float = PARTIAL_FRAME_TIMEOUT_SECONDS,
    ) -> None:
        if not math.isfinite(partial_frame_timeout) or partial_frame_timeout <= 0:
            raise ValueError("partial_frame_timeout must be a positive number of seconds")
        self._host = host
        self._partial_frame_timeout = partial_frame_timeout
        self._port = port
        self._handler = handler
        self._server_socket: socket.socket | None = None
        self._client_socket: socket.socket | None = None
        self._request_in_progress = False
        self._started = False
        self._lock = threading.Lock()
        self._ready = threading.Event()
        self._stop_requested = threading.Event()

    @property
    def is_running(self) -> bool:
        """Return whether the listening socket is ready."""
        return self._ready.is_set()

    @property
    def bound_port(self) -> int:
        """Return the bound port, including an OS-assigned ephemeral port."""
        with self._lock:
            if self._server_socket is None:
                raise RuntimeError("TCP server is not listening")
            return int(self._server_socket.getsockname()[1])

    def serve_forever(self) -> None:
        """Block and serve connections until shutdown() is called."""
        with self._lock:
            if self._started or self._stop_requested.is_set():
                return
            self._started = True
        try:
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as server_socket:
                server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
                server_socket.settimeout(1.0)  # Allow periodic shutdown checks
                server_socket.bind((self._host, self._port))
                server_socket.listen(1)
                with self._lock:
                    if self._stop_requested.is_set():
                        return
                    self._server_socket = server_socket
                    bound_port = int(server_socket.getsockname()[1])
                    self._ready.set()

                logger.info("Sunny TCP server listening on %s:%d", self._host, bound_port)

                while not self._stop_requested.is_set():
                    try:
                        client, addr = server_socket.accept()
                    except TimeoutError:
                        continue
                    except OSError:
                        break

                    logger.info("Client connected from %s", addr)
                    with client:
                        with self._lock:
                            if self._stop_requested.is_set():
                                break
                            self._client_socket = client
                        try:
                            self._handle_client(client)
                        except Exception:
                            if not self._stop_requested.is_set():
                                logger.exception("Client request failed")
                        finally:
                            with self._lock:
                                self._client_socket = None
                    logger.info("Client disconnected")
        finally:
            self._ready.clear()
            with self._lock:
                self._server_socket = None

    def shutdown(self) -> None:
        """Stop receiving work, allowing an accepted request to finish its response."""
        with self._lock:
            self._stop_requested.set()
            self._ready.clear()
            if self._server_socket:
                self._close_socket(self._server_socket)
            if self._client_socket and not self._request_in_progress:
                self._close_socket(self._client_socket)

    @staticmethod
    def _close_socket(sock: socket.socket) -> None:
        # close alone need not interrupt a receive blocked in another thread.
        try:
            sock.shutdown(socket.SHUT_RDWR)
        except OSError:
            pass
        sock.close()

    def _handle_client(self, client: socket.socket) -> None:
        """Process requests from a single client until disconnect."""
        self._enable_keepalive(client)

        while not self._stop_requested.is_set():
            # Read length-prefixed frame
            request_data = self._recv_frame(client)
            if request_data is None:
                break  # Client disconnected

            # Receiving and dispatching have distinct shutdown boundaries. A
            # complete frame can arrive while shutdown wakes the receiver.
            with self._lock:
                if self._stop_requested.is_set():
                    break
                self._request_in_progress = True

            try:
                self._dispatch_frame(client, request_data)
            finally:
                with self._lock:
                    self._request_in_progress = False

    def _dispatch_frame(self, client: socket.socket, request_data: str) -> None:
        """Dispatch an accepted frame and finish its definite response."""

        # Parse JSON
        try:
            request = json.loads(request_data)
        except json.JSONDecodeError as e:
            response = {
                "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
                "success": False,
                "error": f"Invalid JSON: {e}",
            }
            self._send_frame(client, json.dumps(response))
            return

        # Dispatch to handler
        if self._handler:
            try:
                response = self._handler(request)
            except Exception as exc:
                response = {"success": False, "error": str(exc)}
        else:
            response = {"success": False, "error": "No handler registered"}

        response = _versioned_response(response)

        # Send response
        self._send_frame(client, json.dumps(response))

    @staticmethod
    def _enable_keepalive(client: socket.socket) -> None:
        """Let the kernel detect a client that disappeared without closing."""
        try:
            client.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
        except OSError:
            return
        for name, value in _KEEPALIVE_OPTIONS:
            option = getattr(socket, name, None)
            if option is None:
                continue
            try:
                client.setsockopt(socket.IPPROTO_TCP, option, value)
            except OSError:
                # The platform default keepalive schedule still applies.
                pass

    def _await_frame_start(self, client: socket.socket) -> bytes | None:
        """Wait, without a deadline, for the first byte of the next frame.

        Returns None on disconnect or once shutdown has been requested.
        """
        client.settimeout(IDLE_WAKE_SECONDS)
        while not self._stop_requested.is_set():
            try:
                first = client.recv(1)
            except TimeoutError:
                continue
            except ConnectionResetError:
                return None
            return first or None
        return None

    def _recv_frame(self, client: socket.socket) -> str | None:
        """Read one length-prefixed frame. Returns None on disconnect."""
        first = self._await_frame_start(client)
        if first is None:
            return None
        # From the first byte until the response has been written, every
        # socket operation must make progress within the partial-frame timeout.
        client.settimeout(self._partial_frame_timeout)
        rest = self._recv_exact(client, HEADER_SIZE - 1)
        if rest is None:
            return None
        header = first + rest

        length = struct.unpack(">I", header)[0]
        if length > MAX_PAYLOAD:
            raise ValueError(f"Payload too large: {length}")

        payload = self._recv_exact(client, length)
        if payload is None:
            return None

        return payload.decode("utf-8")

    def _send_frame(self, client: socket.socket, data: str) -> None:
        """Send one length-prefixed frame."""
        payload = data.encode("utf-8")
        if len(payload) > MAX_PAYLOAD:
            raise ValueError(f"Response payload too large: {len(payload)}")
        header = struct.pack(">I", len(payload))
        client.sendall(header + payload)

    @staticmethod
    def _recv_exact(sock: socket.socket, n: int) -> bytes | None:
        """Read exactly n bytes. Returns None on disconnect."""
        buf = bytearray()
        while len(buf) < n:
            try:
                chunk = sock.recv(n - len(buf))
            except (TimeoutError, ConnectionResetError):
                return None
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)
