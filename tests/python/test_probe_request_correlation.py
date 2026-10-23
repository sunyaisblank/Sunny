"""Finite socket/scheduler witnesses around the real native-probe dispatch.

No socket, worker, Live host or native file is created. These checks establish
request ownership and one-shot callback delivery, not real-host scheduling.
"""

from __future__ import annotations

import builtins
import importlib.util
import json
import struct
import sys
import threading
from pathlib import Path
from types import ModuleType, SimpleNamespace

import pytest

PROBE = (
    Path(__file__).resolve().parents[2]
    / "tools"
    / "live_qualification"
    / "SunnyHostProbe"
    / "probe.py"
)


class FramedConnection:
    """Retain independent requests and replies with ordinary wire framing."""

    def __init__(self, request):
        """Encode one complete request without opening a socket."""
        self.request = json.loads(json.dumps(request))
        body = json.dumps(self.request).encode("utf-8")
        self.incoming = bytearray(struct.pack(">I", len(body)) + body)
        self.replies = []
        self.timeouts = []
        self.closed = False

    def __enter__(self):
        """Represent acceptance of this connection."""
        return self

    def __exit__(self, *_):
        """Record closure of this connection only."""
        self.closed = True

    def settimeout(self, timeout):
        """Retain the production connection's finite timeout."""
        self.timeouts.append(timeout)

    def recv(self, size):
        """Supply this connection's actual encoded request bytes."""
        result = bytes(self.incoming[:size])
        del self.incoming[:size]
        return result

    def sendall(self, frame):
        """Decode and retain only a complete production reply."""
        size = struct.unpack(">I", frame[:4])[0]
        assert len(frame) == size + 4
        self.replies.append(json.loads(frame[4:]))


class FiniteServer:
    """Accept exactly the two supplied connections, then stop the serve loop."""

    def __init__(self, connections):
        """Own the isolated finite connection sequence."""
        self.connections = list(connections)
        self.accepted = []

    def accept(self):
        """End with OSError after the second accepted request."""
        if not self.connections:
            raise OSError("Finite witness exhausted")
        connection = self.connections.pop(0)
        self.accepted.append(connection)
        return connection, ("in-memory", len(self.accepted))


class ControlledScheduler:
    """Deliver deferred callbacks in a finite order on the model UI thread."""

    def __init__(self, constructor_thread):
        """Keep socket and callback thread observations distinct without a worker."""
        self.constructor_thread = constructor_thread
        self.current_thread = -103
        self.delivery = None
        self.callbacks = []
        self.deliveries = []

    def schedule(self, delay, callback):
        """Retain the production callback without replacing its composition."""
        assert delay == 0
        self.callbacks.append(callback)

    def deliver(self, index, label):
        """Run that exact callback, restoring the synthetic socket context."""
        self.delivery = label
        self.deliveries.append(label)
        self.current_thread = self.constructor_thread
        try:
            self.callbacks[index]()
        finally:
            self.current_thread = -103
            self.delivery = None


@pytest.fixture
def correlation_witness(monkeypatch):
    """Replace only external Live, framing and callback-wait boundaries."""
    monkeypatch.setattr(sys, "dont_write_bytecode", True)
    framework = ModuleType("_Framework")
    controls = ModuleType("_Framework.ControlSurface")
    controls.ControlSurface = object
    monkeypatch.setitem(sys.modules, "_Framework", framework)
    monkeypatch.setitem(sys.modules, "_Framework.ControlSurface", controls)
    live = ModuleType("Live")
    live.Track = SimpleNamespace(Track=SimpleNamespace(monitoring_states=SimpleNamespace(OFF=71)))
    monkeypatch.setitem(sys.modules, "Live", live)
    monkeypatch.delattr(builtins, "_sunny_primary_qualification_registrations_v1", raising=False)

    spec = importlib.util.spec_from_file_location("probe_correlation_witness", PROBE)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    probe = module.HostProbe.__new__(module.HostProbe)
    probe._constructor_thread = threading.get_ident()
    probe._document_song = None
    probe._document_token = None
    probe._mutations = []
    probe._socket_errors = []
    probe._socket_errors_lock = threading.Lock()
    probe._stop = threading.Event()
    scheduler = ControlledScheduler(probe._constructor_thread)
    effects = []

    class Clip:
        is_midi_clip = True
        has_groove = True

        @property
        def groove(self):
            return "original-groove"

        @groove.setter
        def groove(self, value):
            effects.append((scheduler.delivery, "groove", value))

    class Scene:
        name = "SUNNY_HOST_PROBE_SCENE"

        @property
        def tempo_enabled(self):
            return False

        @tempo_enabled.setter
        def tempo_enabled(self, value):
            effects.append((scheduler.delivery, "tempo_enabled", value))

        @property
        def time_signature_enabled(self):
            return False

        @time_signature_enabled.setter
        def time_signature_enabled(self, value):
            effects.append((scheduler.delivery, "time_signature_enabled", value))

    song = SimpleNamespace(
        name="SUNNY_HOST_QUALIFICATION_CORRELATION",
        is_playing=False,
        session_record=False,
        record_mode=False,
        tracks=[
            SimpleNamespace(
                name="SUNNY_HOST_PROBE_GROOVE",
                current_monitoring_state=71,
                clip_slots=[SimpleNamespace(clip=Clip())],
            )
        ],
        scenes=[Scene()],
    )
    probe.song = lambda: song
    from Sunny.managed import ManagedRegistry
    from Sunny.native_qualification import observe_primary_context, register_primary

    registry = ManagedRegistry(SimpleNamespace(song=lambda: song))
    assert register_primary(registry)
    primary = observe_primary_context(song)
    approval = {
        "scratch_approved": True,
        "expected_set_name": song.name,
        "expected_document_token": probe._observe_document(song),
        "expected_bridge_instance": primary["bridge_instance"],
        "expected_native_document_token": primary["document_token"],
    }
    return SimpleNamespace(
        module=module,
        probe=probe,
        scheduler=scheduler,
        effects=effects,
        approval=approval,
        registry=registry,  # Retain the actual weakly registered primary for both callbacks.
    )


@pytest.mark.parametrize("duplicate_delivery", [False, True], ids=["late-A", "duplicate-B"])
def test_cancelled_request_cannot_capture_next_request_or_replay_it(
    correlation_witness, duplicate_delivery
):
    """A late cancelled callback owns A; B's real native phases execute once."""
    witness = correlation_witness
    first = FramedConnection(
        {"op": "clear_groove", "track_name": "SUNNY_HOST_PROBE_GROOVE", **witness.approval}
    )
    second = FramedConnection({"op": "scene_flags_same_value_set", **witness.approval})
    original_requests = [dict(first.request), dict(second.request)]
    server = FiniteServer([first, second])
    witness.probe._server = server
    witness.probe.schedule_message = witness.scheduler.schedule
    events = []

    class Completion:
        def __init__(self):
            self.index = len(events)
            self.completed = False
            self.set_count = 0
            self.waits = []
            events.append(self)

        def set(self):
            self.completed = True
            self.set_count += 1

        def wait(self, timeout=None):
            self.waits.append(timeout)
            assert timeout == 30, "The witness must never enter an unbounded wait"
            if self.index == 0:
                return False  # A's UI callback has not started at the finite deadline.
            assert self.index == 1 and len(witness.scheduler.callbacks) == 2
            witness.scheduler.deliver(0, "late-A")
            witness.scheduler.deliver(1, "B")
            if duplicate_delivery:
                witness.scheduler.deliver(1, "duplicate-B")
            assert self.completed
            return True

    witness.module.threading = SimpleNamespace(
        Event=Completion,
        Lock=threading.Lock,
        get_ident=lambda: witness.scheduler.current_thread,
    )
    witness.probe._serve()

    assert server.accepted == [first, second]
    assert first.closed and second.closed
    assert first.timeouts == second.timeouts == [35]
    assert not first.incoming and not second.incoming
    assert [first.request, second.request] == original_requests
    assert len(first.replies) == len(second.replies) == 1
    assert first.replies[0] == {
        "success": False,
        "error": "Diagnostic callback cancelled before start",
    }
    second_reply = second.replies[0]
    assert second_reply["success"] is True
    assert set(second_reply["result"]) == {"tempo_enabled", "time_signature_enabled"}
    assert all(
        row[edge]["value"] is False
        for row in second_reply["result"].values()
        for edge in ("before", "after")
    )
    assert second_reply["dispatch"]["socket_thread"] == -103
    assert (
        second_reply["dispatch"]["callback_thread"]
        == second_reply["dispatch"]["constructor_thread"]
        == witness.probe._constructor_thread
    )
    expected_effects = [("B", "tempo_enabled", False), ("B", "time_signature_enabled", False)]
    assert witness.effects == expected_effects
    assert [event.waits for event in events] == [[30], [30]]
    assert [event.set_count for event in events] == [0, 1]
    assert witness.probe._socket_errors == []
    assert [row["phase"] for row in witness.probe._mutations] == [
        "tempo_enabled",
        "time_signature_enabled",
    ]
    for row in witness.probe._mutations:
        assert row["op"] == second.request["op"]
        assert row["started"] is True and row["returned"] is True
        assert "error" not in row
        for evidence, original in (
            ("document_token", "expected_document_token"),
            ("bridge_instance", "expected_bridge_instance"),
            ("native_document_token", "expected_native_document_token"),
        ):
            assert row[evidence] == second.request[original]

    # Cancellation and completion remain sticky even after replies and socket close.
    retained_evidence = json.dumps(witness.probe._mutations, sort_keys=True)
    retained_replies = json.dumps([first.replies, second.replies], sort_keys=True)
    witness.scheduler.deliver(0, "after-close-A")
    if duplicate_delivery:
        witness.scheduler.deliver(1, "after-close-B")
    assert json.dumps(witness.probe._mutations, sort_keys=True) == retained_evidence
    assert witness.effects == expected_effects
    assert [event.set_count for event in events] == [0, 1]
    assert json.dumps([first.replies, second.replies], sort_keys=True) == retained_replies
