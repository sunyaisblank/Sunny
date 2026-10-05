"""Actual TCP generation revocation with one serialized modeled Live thread."""

from __future__ import annotations

import queue
import socket
import threading
import time
from types import SimpleNamespace

from live_model import LiveSet
from Sunny.diagnostics import RemoteLog
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.managed import ManagedRegistry, _digest
from Sunny.server import TcpServer
from Sunny.surface import SunnyControlSurface
from test_tcp_admission import receive, released, running, send


def request(name, value=None, *, kind="call"):
    """Build an explicit independently framed native request."""
    return {
        "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
        "type": kind,
        "path": "song",
        "name": name,
        "args": [] if value is None else [value],
    }


def exchange(client, name, value=None):
    """Require an outer ACK without conflating it with inner journal outcome."""
    send(client, request(name, value))
    response = receive(client)
    assert response["success"], response
    return response["value"]


def wait_until(predicate):
    """Bound every fixture wait independently of the production scheduler."""
    deadline = time.monotonic() + 2
    while not predicate():
        assert time.monotonic() < deadline
        time.sleep(0.005)


def test_disconnected_queued_callback_never_enters_native_handler():
    """A queued callback checks its original socket generation at main-thread entry."""
    queued, calls = [], []
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface._handler = SimpleNamespace(
        handle=lambda value: calls.append(value) or {"success": True}
    )
    surface.schedule_message = lambda delay, callback: queued.append(callback)
    with running(surface._process_request) as server:
        client = socket.create_connection(("127.0.0.1", server.bound_port), 1)
        send(client, request("tempo", kind="get"))
        wait_until(lambda: len(queued) == 1)
        client.close()
        released(server)
        queued[0]()
        assert calls == []
        assert surface._pending_requests == set()


def test_started_disconnect_releases_socket_and_preserves_original_journal(monkeypatch):
    """One returned setter settles; logs bypass blocked Live and replacement work declines."""
    live = LiveSet(midi_tracks=1).install(monkeypatch)
    callbacks, ready = queue.Queue(), threading.Event()
    entered, finish = threading.Event(), threading.Event()
    state = SimpleNamespace(count=0)
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface.schedule_message = lambda delay, callback: callbacks.put(callback)
    log = RemoteLog()

    def main_thread():
        registry = ManagedRegistry(live.surface)
        surface._managed_registry = registry
        surface._handler = LomHandler(live.surface, log, managed_registry=registry)
        registry.attach_handler(surface._handler)
        ready.set()
        while (callback := callbacks.get()) is not None:
            callback()
        registry._legacy.close()

    native = threading.Thread(target=main_thread)
    native.start()
    assert ready.wait(1)
    descriptor = type(live.song).tempo
    original_setter = descriptor._setter

    def blocked_setter(song, value):
        state.count += 1
        original_setter(song, value)
        entered.set()
        assert finish.wait(3), "Modeled Live setter was not released"

    monkeypatch.setattr(descriptor, "_setter", blocked_setter)
    server = TcpServer(host="127.0.0.1", port=0, handler=surface._process_request)
    listener = threading.Thread(target=server.serve_forever)
    listener.start()
    assert server._ready.wait(1)
    owner = socket.create_connection(("127.0.0.1", server.bound_port), 1)
    try:
        context = exchange(owner, "sunny_managed_context")
        captured = exchange(owner, "sunny_legacy_scope", {**context, "scope_id": None})
        scope = {
            key: captured[key]
            for key in ("schema_version", "bridge_instance", "document_token", "scope_id")
        }
        intent = {
            **scope,
            "workflow_id": "c" * 32,
            "operation_id": "e" * 32,
            "ordinal": 1,
            "graph_revision": 0,
            "command": {"type": "set", "path": "song", "name": "tempo", "args": [140.0]},
        }
        token = {
            **{key: value for key, value in intent.items() if key != "command"},
            "fingerprint": _digest(intent),
        }
        assert exchange(owner, "sunny_legacy_prepare", intent)["outcome"] == "prepared"
        send(owner, request("sunny_legacy_execute", token))
        assert entered.wait(1)
        owner.close()
        released(server)
        # This reply arrives while the one original native setter remains on
        # the modeled Live thread. No replacement executor is created.
        replacement = socket.create_connection(("127.0.0.1", server.bound_port), 1)
        log_reply = exchange(replacement, "sunny_get_remote_log", 0)
        assert log_reply["stream_id"] and not finish.is_set()
        second = {**intent, "operation_id": "f" * 32, "ordinal": 2}
        send(replacement, request("sunny_legacy_prepare", second))
        wait_until(lambda: callbacks.qsize() == 1)
        replacement.close()
        released(server)
        finish.set()
        wait_until(lambda: callbacks.empty())
        with socket.create_connection(("127.0.0.1", server.bound_port), 1) as query_peer:
            journal = exchange(query_peer, "sunny_legacy_operation", token)
            assert journal["outcome"] == "acknowledged", journal
            assert journal["started_calls"] == journal["returned_calls"] == 1
            assert journal["result"]["value"] == {
                "property": "tempo",
                "requested": 140.0,
                "observed": 140.0,
            }
            assert exchange(query_peer, "sunny_legacy_execute", token) == journal
            # A new peer can inspect original evidence, never prepare the
            # next old workflow operation under replacement generation.
            declined = exchange(query_peer, "sunny_legacy_prepare", second)
            assert declined["outcome"] == "declined"
            assert declined["started_calls"] == 0
        assert state.count == 1 and live.song.tempo == 140.0
    finally:
        finish.set()
        owner.close()
        server.shutdown()
        listener.join(2)
        callbacks.put(None)
        native.join(2)
        log.close()
        assert not listener.is_alive() and not native.is_alive()
