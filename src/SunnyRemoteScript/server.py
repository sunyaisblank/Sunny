"""
TCP server with length-prefixed JSON framing.

Wire protocol:
    [4 bytes big-endian uint32: payload length] [UTF-8 JSON payload]

The server accepts one client at a time (the Sunny C++ orchestrator).
Each request is dispatched to the handler callback and the response
is sent back with the same framing.
"""

from __future__ import annotations

import json
import logging
import socket
import struct
import threading
from typing import Callable

logger = logging.getLogger("SunnyRemoteScript.server")

HEADER_SIZE = 4  # bytes for uint32 big-endian length prefix
MAX_PAYLOAD = 16 * 1024 * 1024  # 16 MB sanity limit


class TcpServer:
    """Single-client TCP server with length-prefixed JSON framing."""

    def __init__(
        self,
        host: str = "0.0.0.0",
        port: int = 9001,
        handler: Callable[[dict], dict] | None = None,
    ):
        self._host = host
        self._port = port
        self._handler = handler
        self._running = False
        self._server_socket: socket.socket | None = None
        self._lock = threading.Lock()

    def serve_forever(self):
        """Block and serve connections until shutdown() is called."""
        self._server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._server_socket.settimeout(1.0)  # Allow periodic shutdown checks
        self._server_socket.bind((self._host, self._port))
        self._server_socket.listen(1)
        self._running = True

        logger.info("Sunny TCP server listening on %s:%d", self._host, self._port)

        while self._running:
            try:
                client, addr = self._server_socket.accept()
            except socket.timeout:
                continue
            except OSError:
                break

            logger.info("Client connected from %s", addr)
            try:
                self._handle_client(client)
            except Exception as e:
                logger.error("Client error: %s", e)
            finally:
                client.close()
                logger.info("Client disconnected")

        self._server_socket.close()

    def shutdown(self):
        """Signal the server to stop accepting connections."""
        self._running = False
        with self._lock:
            if self._server_socket:
                try:
                    self._server_socket.close()
                except OSError:
                    pass

    def _handle_client(self, client: socket.socket):
        """Process requests from a single client until disconnect."""
        client.settimeout(30.0)

        while self._running:
            # Read length-prefixed frame
            request_data = self._recv_frame(client)
            if request_data is None:
                break  # Client disconnected

            # Parse JSON
            try:
                request = json.loads(request_data)
            except json.JSONDecodeError as e:
                response = {"success": False, "error": f"Invalid JSON: {e}"}
                self._send_frame(client, json.dumps(response))
                continue

            # Dispatch to handler
            if self._handler:
                try:
                    response = self._handler(request)
                except Exception as e:
                    response = {"success": False, "error": str(e)}
            else:
                response = {"success": False, "error": "No handler registered"}

            # Send response
            self._send_frame(client, json.dumps(response))

    def _recv_frame(self, client: socket.socket) -> str | None:
        """Read one length-prefixed frame. Returns None on disconnect."""
        header = self._recv_exact(client, HEADER_SIZE)
        if header is None:
            return None

        length = struct.unpack(">I", header)[0]
        if length > MAX_PAYLOAD:
            raise ValueError(f"Payload too large: {length}")

        payload = self._recv_exact(client, length)
        if payload is None:
            return None

        return payload.decode("utf-8")

    def _send_frame(self, client: socket.socket, data: str):
        """Send one length-prefixed frame."""
        payload = data.encode("utf-8")
        header = struct.pack(">I", len(payload))
        client.sendall(header + payload)

    @staticmethod
    def _recv_exact(sock: socket.socket, n: int) -> bytes | None:
        """Read exactly n bytes. Returns None on disconnect."""
        buf = bytearray()
        while len(buf) < n:
            try:
                chunk = sock.recv(n - len(buf))
            except (socket.timeout, ConnectionResetError):
                return None
            if not chunk:
                return None
            buf.extend(chunk)
        return bytes(buf)
