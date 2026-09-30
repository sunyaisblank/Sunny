"""Connection-lifecycle tests for the Ableton bridge.

The bridge is a persistent request/response link: ``sunny-mcp`` holds one TCP
connection to the Remote Script and reuses it for every tool call. These tests
pin the lifecycle contract on both peers. A user pause of any length must not
fail the next operation; a Remote Script reload must be absorbed by the native
client before it sends; and the native response deadline must outlast the
Remote Script's scheduling deadline, so that a request the native side stops
waiting for cannot still begin in Live afterwards.

The end-to-end cases run the real ``TcpServer`` and ``SunnyControlSurface``
request path in-process over a minimal fake song, with the built ``sunny-mcp``
binary as the client. Timeouts are injected so that no test waits 30 s.
"""

from __future__ import annotations

import json
import os
import queue
import re
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path
from types import SimpleNamespace

import pytest
from Sunny import surface as surface_module
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.server import TcpServer
from Sunny.surface import SunnyControlSurface

PROJECT_ROOT = Path(__file__).resolve().parents[2]
TRANSPORT_HEADER = PROJECT_ROOT / "include/sunny/infrastructure/ableton/transport.hpp"

# Short enough to keep the suite fast, long enough that a loaded CI host still
# delivers a complete frame well inside it.
SHORT_PARTIAL_FRAME_TIMEOUT = 0.3
IDLE_GAP_SECONDS = 1.0


def _send_frame(sock: socket.socket, payload: dict) -> None:
    data = json.dumps(payload).encode("utf-8")
    sock.sendall(struct.pack(">I", len(data)) + data)


def _recv_frame(sock: socket.socket) -> dict:
    buffer = b""
    while len(buffer) < 4:
        chunk = sock.recv(4 - len(buffer))
        assert chunk, "server closed connection mid-header"
        buffer += chunk
    (length,) = struct.unpack(">I", buffer)
    payload = b""
    while len(payload) < length:
        chunk = sock.recv(length - len(payload))
        assert chunk, "server closed connection mid-payload"
        payload += chunk
    return json.loads(payload.decode("utf-8"))


class _Song:
    """The song facts read by ``get_ableton_session_state``."""

    tempo = 120.0
    signature_numerator = 4
    signature_denominator = 4
    is_playing = False
    current_song_time = 0.0


class _Application:
    @staticmethod
    def get_major_version():
        return 12

    @staticmethod
    def get_minor_version():
        return 2

    @staticmethod
    def get_bugfix_version():
        return 1

    @staticmethod
    def get_version_string():
        return "12.2.1"


@pytest.fixture
def live_application(monkeypatch):
    """Provide the documented Application object the handshake reads."""
    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(Application=SimpleNamespace(get_application=lambda: _Application())),
    )


class _RemoteScript:
    """The real server and surface request path over a fake song."""

    def __init__(self, port: int = 0, handler=None) -> None:
        surface = SunnyControlSurface.__new__(SunnyControlSurface)
        surface._initialise_request_lifecycle()
        surface._handler = LomHandler(SimpleNamespace(song=lambda: _Song()))
        # The fake host runs scheduled work immediately, as Live's main thread
        # would when idle.
        surface.schedule_message = lambda delay, callback: callback()
        self.server = TcpServer(
            host="127.0.0.1",
            port=port,
            handler=handler or surface._process_request,
            partial_frame_timeout=SHORT_PARTIAL_FRAME_TIMEOUT,
        )
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        assert self.server._ready.wait(2), "Remote Script server did not start listening"
        self.port = self.server.bound_port

    def close(self) -> None:
        self.server.shutdown()
        self.thread.join(timeout=3)
        assert not self.thread.is_alive(), "Remote Script server thread did not stop"

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()


def _sunny_mcp_binary() -> Path:
    override = os.environ.get("SUNNY_MCP_BINARY")
    candidate = Path(override) if override else PROJECT_ROOT / ".bin" / "sunny-mcp"
    if not candidate.is_file():
        pytest.skip(f"sunny-mcp not built at {candidate}")
    return candidate


class _McpClient:
    """Drive the built ``sunny-mcp`` over its stdio JSON-RPC transport."""

    RESPONSE_TIMEOUT_SECONDS = 30.0

    def __init__(self, host: str, port: int, stderr_path: Path) -> None:
        env = dict(os.environ, SUNNY_ABLETON_HOST=host, SUNNY_TCP_PORT=str(port))
        self._stderr = stderr_path.open("w")
        self.process = subprocess.Popen(
            [str(_sunny_mcp_binary())],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self._stderr,
            text=True,
            env=env,
        )
        self._lines: queue.Queue[str] = queue.Queue()
        self._reader = threading.Thread(target=self._read_stdout, daemon=True)
        self._reader.start()
        self._next_id = 0
        self._request(
            "initialize",
            {
                "protocolVersion": "2024-11-05",
                "capabilities": {},
                "clientInfo": {"name": "bridge-lifecycle-test", "version": "0"},
            },
        )
        self._write({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _read_stdout(self) -> None:
        assert self.process.stdout is not None
        for line in self.process.stdout:
            self._lines.put(line)

    def _write(self, message: dict) -> None:
        assert self.process.stdin is not None
        self.process.stdin.write(json.dumps(message) + "\n")
        self.process.stdin.flush()

    def _request(self, method: str, params: dict) -> dict:
        self._next_id += 1
        self._write({"jsonrpc": "2.0", "id": self._next_id, "method": method, "params": params})
        line = self._lines.get(timeout=self.RESPONSE_TIMEOUT_SECONDS)
        response = json.loads(line)
        assert response["id"] == self._next_id
        assert "result" in response, response
        return response["result"]

    def call_tool(self, name: str) -> dict:
        return self._request("tools/call", {"name": name, "arguments": {}})["structuredContent"]

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        try:
            self.process.wait(timeout=10)
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait()
            self._stderr.close()


@pytest.fixture
def mcp_client(tmp_path):
    """Start sunny-mcp clients against a loopback port and stop them afterwards."""
    clients: list[_McpClient] = []

    def connect(port: int, host: str = "127.0.0.1") -> _McpClient:
        client = _McpClient(host, port, tmp_path / f"sunny-mcp-{len(clients)}.stderr")
        clients.append(client)
        return client

    yield connect
    for client in clients:
        client.close()


def _assert_live_session_state(state: dict) -> None:
    assert state["connected"] is True
    assert state["tempo"] == 120.0
    assert state["target_profile"]["live"]["version"]["string"] == "12.2.1"


# =============================================================================
# Remote Script: idle connections persist; stalled frames do not
# =============================================================================


def test_idle_connection_outlives_the_partial_frame_timeout():
    """A pause between frames is the normal state of the link, not a stall."""
    with _RemoteScript(handler=lambda request: {"success": True, "value": request["n"]}) as peer:
        with socket.create_connection(("127.0.0.1", peer.port), timeout=5) as sock:
            _send_frame(sock, {"n": 1})
            assert _recv_frame(sock)["value"] == 1
            time.sleep(IDLE_GAP_SECONDS)
            _send_frame(sock, {"n": 2})
            assert _recv_frame(sock) == {
                "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
                "success": True,
                "value": 2,
            }


def test_stalled_partial_frame_is_dropped_and_the_next_client_is_served():
    """A frame left incomplete past the timeout desynchronises the stream."""
    calls = []
    with _RemoteScript(handler=lambda request: calls.append(request) or {"success": True}) as peer:
        with socket.create_connection(("127.0.0.1", peer.port), timeout=5) as stalled:
            stalled.sendall(b"\x00\x00")
            started = time.monotonic()
            assert stalled.recv(1) == b""
            assert time.monotonic() - started < 5
        with socket.create_connection(("127.0.0.1", peer.port), timeout=5) as sock:
            _send_frame(sock, {"n": 1})
            assert _recv_frame(sock)["success"] is True
    assert calls == [{"n": 1}]


def test_partial_frame_timeout_must_be_positive():
    """A zero or negative timeout would disable or invert stall detection."""
    for invalid in (0.0, -1.0):
        with pytest.raises(ValueError):
            TcpServer(port=0, partial_frame_timeout=invalid)


# =============================================================================
# Deadline ordering between the peers
# =============================================================================


def _header_milliseconds(name: str) -> int:
    match = re.search(
        rf"{name}\s*\{{\s*(\d+)\s*\}}",
        TRANSPORT_HEADER.read_text(encoding="utf-8"),
    )
    assert match, f"{name} is not declared in {TRANSPORT_HEADER}"
    return int(match.group(1))


def test_native_response_deadline_exceeds_remote_script_scheduling_deadline():
    """A request the native side stops waiting for has already begun or been cancelled."""
    scheduling_ms = _header_milliseconds("SUNNY_REMOTE_SCRIPT_SCHEDULING_DEADLINE")
    response_ms = _header_milliseconds("SUNNY_BRIDGE_RESPONSE_TIMEOUT")
    assert scheduling_ms == round(surface_module.LOM_REQUEST_TIMEOUT_SECONDS * 1000)
    assert response_ms > scheduling_ms


# =============================================================================
# End to end: sunny-mcp against the real Remote Script server
# =============================================================================


def test_tool_calls_separated_by_an_idle_gap_both_succeed(live_application, mcp_client):
    """Issue #4 acceptance: the call after a pause succeeds on the first attempt."""
    with _RemoteScript() as peer:
        client = mcp_client(peer.port)
        first = client.call_tool("get_ableton_session_state")
        _assert_live_session_state(first)
        time.sleep(IDLE_GAP_SECONDS)
        second = client.call_tool("get_ableton_session_state")
        assert second == first


def test_first_tool_call_after_a_remote_script_reload_succeeds(live_application, mcp_client):
    """The native client notices the closed idle socket and reconnects before sending."""
    original = _RemoteScript()
    port = original.port
    try:
        client = mcp_client(port)
        first = client.call_tool("get_ableton_session_state")
        _assert_live_session_state(first)
    finally:
        original.close()
    with _RemoteScript(port=port):
        assert client.call_tool("get_ableton_session_state") == first


def test_host_name_is_resolved_rather_than_parsed_as_a_literal(live_application, mcp_client):
    """Issue #5: SUNNY_ABLETON_HOST=localhost reaches a loopback Remote Script."""
    with _RemoteScript() as peer:
        client = mcp_client(peer.port, host="localhost")
        _assert_live_session_state(client.call_tool("get_ableton_session_state"))
