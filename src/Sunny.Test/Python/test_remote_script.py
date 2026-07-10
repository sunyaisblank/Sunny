"""Loopback tests for the SunnyRemoteScript TCP server.

Pins the wire protocol the C++ TcpTransport (Bridge/INTP001A) speaks:
4-byte big-endian length prefix followed by UTF-8 JSON, request/response
over a single connection. These tests run the real TcpServer with a stub
handler; no Ableton instance is required.
"""

from __future__ import annotations

import json
import socket
import struct
import threading
import time

from SunnyRemoteScript.server import TcpServer


def _send_frame(sock: socket.socket, payload: dict) -> None:
    data = json.dumps(payload).encode("utf-8")
    sock.sendall(struct.pack(">I", len(data)) + data)


def _recv_frame(sock: socket.socket) -> dict:
    header = b""
    while len(header) < 4:
        chunk = sock.recv(4 - len(header))
        assert chunk, "server closed connection mid-header"
        header += chunk
    (length,) = struct.unpack(">I", header)
    payload = b""
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        assert chunk, "server closed connection mid-payload"
        payload += chunk
    return json.loads(payload.decode("utf-8"))


class _RunningServer:
    """Start a TcpServer on an ephemeral port in a background thread."""

    def __init__(self, handler):
        self.server = TcpServer(host="127.0.0.1", port=0, handler=handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        # Wait for the listening socket to bind
        for _ in range(100):
            if self.server._server_socket is not None and self.server._running:
                break
            time.sleep(0.01)
        self.port = self.server._server_socket.getsockname()[1]

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.server.shutdown()
        self.thread.join(timeout=2.0)


def test_request_response_round_trip():
    """A framed request is dispatched to the handler and answered in kind."""
    received = []

    def handler(request: dict) -> dict:
        received.append(request)
        return {"success": True, "value": 120.0}

    with _RunningServer(handler) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=5) as sock:
            _send_frame(sock, {"type": "get", "path": "song", "name": "tempo"})
            response = _recv_frame(sock)

    assert response == {"success": True, "value": 120.0}
    assert received == [{"type": "get", "path": "song", "name": "tempo"}]


def test_sequential_requests_on_one_connection():
    """The server answers multiple framed requests on a single connection."""

    def handler(request: dict) -> dict:
        return {"success": True, "echo": request["name"]}

    with _RunningServer(handler) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=5) as sock:
            for name in ("tempo", "is_playing", "metronome"):
                _send_frame(sock, {"type": "get", "path": "song", "name": name})
                response = _recv_frame(sock)
                assert response["echo"] == name


def test_invalid_json_is_declined_not_dropped():
    """Malformed payloads produce an error response, not silence."""
    with _RunningServer(lambda request: {"success": True}) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=5) as sock:
            bad = b"not json"
            sock.sendall(struct.pack(">I", len(bad)) + bad)
            response = _recv_frame(sock)

    assert response["success"] is False
    assert "Invalid JSON" in response["error"]


def test_handler_exception_is_reported():
    """A handler crash surfaces as an error response with the message."""

    def handler(request: dict) -> dict:
        raise RuntimeError("boom")

    with _RunningServer(handler) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=5) as sock:
            _send_frame(sock, {"type": "call", "path": "song", "name": "x"})
            response = _recv_frame(sock)

    assert response["success"] is False
    assert "boom" in response["error"]
