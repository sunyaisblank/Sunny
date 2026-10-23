"""Literal busy admission, single-client ownership and finite native framing."""

from __future__ import annotations

import errno
import json
import socket
import struct
import threading
import time
from contextlib import contextmanager

import pytest
from Sunny.server import TcpServer


def send(client, value):
    """Write one independent literal length-prefixed request."""
    payload = json.dumps(value).encode("utf-8")
    client.sendall(struct.pack(">I", len(payload)) + payload)


def receive(client):
    """Read one complete frame without sharing native framing helpers."""

    def exact(size):
        result = b""
        while len(result) < size:
            part = client.recv(size - len(result))
            assert part, "Incomplete literal frame"
            result += part
        return result

    return json.loads(exact(struct.unpack(">I", exact(4))[0]))


@contextmanager
def running(handler, timeout=0.15):
    """Start and completely join the listener and its exclusively owned worker."""
    server = TcpServer(host="127.0.0.1", port=0, handler=handler, partial_frame_timeout=timeout)
    thread = threading.Thread(target=server.serve_forever)
    thread.start()
    assert server._ready.wait(1)
    try:
        yield server
    finally:
        server.shutdown()
        thread.join(2)
        assert not thread.is_alive()
        if server._client_thread is not None:
            assert not server._client_thread.is_alive()


def released(server):
    """Wait finitely for the actual receiver to release its client slot."""
    deadline = time.monotonic() + 1
    while server._client_socket is not None:
        assert time.monotonic() < deadline, "Dead client retained native admission"
        time.sleep(0.005)


@pytest.mark.parametrize("in_progress", [False, True])
def test_second_clients_receive_literal_busy_without_dispatch_or_later_queue(in_progress):
    """Busy clients receive a definite rejection while the owner's request remains intact."""
    calls, begun, release = [], threading.Event(), threading.Event()

    def handler(request):
        calls.append(request)
        begun.set()
        if in_progress and request == {"owner": 1}:
            assert release.wait(2)
        return {"success": True, "value": request}

    with running(handler) as server:
        try:
            with socket.create_connection(("127.0.0.1", server.bound_port), 1) as owner:
                send(owner, {"owner": 1})
                assert begun.wait(1)
                if not in_progress:
                    assert receive(owner)["value"] == {"owner": 1}
                for index in range(3):
                    with socket.create_connection(("127.0.0.1", server.bound_port), 1) as extra:
                        send(extra, {"forbidden": index})
                        started = time.monotonic()
                        assert receive(extra) == {
                            "bridge_protocol_version": 47,
                            "success": False,
                            "error": (
                                "bridge_busy: Sunny accepts one active client; "
                                "close the existing client and retry"
                            ),
                        }
                        assert time.monotonic() - started < 0.5
                        try:
                            assert extra.recv(1) == b""
                        except ConnectionResetError:
                            pass
                release.set()
                if in_progress:
                    assert receive(owner)["value"] == {"owner": 1}
                send(owner, {"owner": 2})
                assert receive(owner)["value"] == {"owner": 2}
            released(server)
            with socket.create_connection(("127.0.0.1", server.bound_port), 1) as replacement:
                send(replacement, {"replacement": 1})
                assert receive(replacement)["value"] == {"replacement": 1}
            assert calls == [{"owner": 1}, {"owner": 2}, {"replacement": 1}]
        finally:
            release.set()


def test_abrupt_dead_client_releases_admission_for_explicit_replacement():
    """A real TCP reset releases ownership without a timed idle eviction."""
    calls = []
    with running(lambda request: calls.append(request) or {"success": True}) as server:
        owner = socket.create_connection(("127.0.0.1", server.bound_port), 1)
        send(owner, {"owner": 1})
        assert receive(owner)["success"]
        owner.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
        owner.close()
        released(server)
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as replacement:
            send(replacement, {"replacement": 1})
            assert receive(replacement)["success"]
        assert calls == [{"owner": 1}, {"replacement": 1}]


@pytest.mark.parametrize("prefix", [b"\x00", struct.pack(">I", 100) + b"{"])
def test_byte_trickle_cannot_extend_whole_partial_frame_deadline(prefix):
    """Header and payload trickles share one deadline and never reach the handler."""
    calls = []
    with running(lambda request: calls.append(request) or {"success": True}, 0.12) as server:
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as owner:
            started = time.monotonic()
            owner.sendall(prefix)
            time.sleep(0.07)
            owner.sendall(b"\x00")
            assert owner.recv(1) == b""
            assert time.monotonic() - started < 0.22
        released(server)
        assert calls == []
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as replacement:
            send(replacement, {"replacement": 1})
            assert receive(replacement)["success"]


def test_idle_owner_remains_usable_past_partial_frame_deadline_and_rejects_extra_client():
    """Normal user pauses preserve the owner while newly connecting clients are declined."""
    with running(lambda request: {"success": True, "value": request}, 0.05) as server:
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as owner:
            send(owner, {"first": 1})
            assert receive(owner)["value"] == {"first": 1}
            time.sleep(0.15)
            with socket.create_connection(("127.0.0.1", server.bound_port), 1) as extra:
                assert receive(extra)["success"] is False
            send(owner, {"second": 2})
            assert receive(owner)["value"] == {"second": 2}


def test_listener_failure_closes_idle_worker_without_leaving_a_receiver_alive():
    """An actual listener failure releases the idle worker even without explicit shutdown."""
    with running(lambda request: {"success": True}) as server:
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as owner:
            send(owner, {})
            assert receive(owner)["success"]
            server._server_socket.shutdown(socket.SHUT_RDWR)
            worker = server._client_thread
            worker.join(1)
            assert not worker.is_alive()
            assert owner.recv(1) == b""


@pytest.mark.parametrize("kernel_failure", [False, True])
def test_idle_wake_timeout_is_distinct_from_kernel_dead_peer_timeout(kernel_failure):
    """The same Python exception class represents an idle wake and a terminal kernel timeout."""
    server = TcpServer(port=0)

    class SocketBoundary:
        calls = 0

        def settimeout(self, timeout):
            assert timeout == 1.0

        def recv(self, size):
            assert size == 1
            self.calls += 1
            if self.calls == 1:
                if kernel_failure:
                    raise TimeoutError(errno.ETIMEDOUT, "Connection timed out")
                raise TimeoutError("timed out")
            return b"x"

    client = SocketBoundary()
    assert server._await_frame_start(client) == (None if kernel_failure else b"x")
    assert client.calls == (1 if kernel_failure else 2)
