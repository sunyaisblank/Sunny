"""Loopback tests for Sunny's Remote Script TCP server.

Pins the wire protocol used by the C++ ``TcpTransport``:
4-byte big-endian length prefix followed by UTF-8 JSON, request/response
over a single connection. These tests run the real TcpServer with a stub
handler; no Ableton instance is required.
"""

from __future__ import annotations

import json
import math
import socket
import struct
import sys
import threading
import time
from types import SimpleNamespace

import pytest
from Sunny import surface as surface_module
from Sunny.handler import (
    BRIDGE_PROTOCOL_VERSION,
    TARGET_SNAPSHOT_SCHEMA_VERSION,
    LomHandler,
    _request_allowed,
)
from Sunny.server import TcpServer
from Sunny.surface import (
    DEFAULT_BIND_HOST,
    DEFAULT_PORT,
    SunnyControlSurface,
    _server_configuration,
)


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


def _versioned(request: dict) -> dict:
    request["bridge_protocol_version"] = BRIDGE_PROTOCOL_VERSION
    return request


class _RunningServer:
    """Start a TcpServer on an ephemeral port in a background thread."""

    def __init__(self, handler):
        self.server = TcpServer(host="127.0.0.1", port=0, handler=handler)
        self.thread = threading.Thread(target=self.server.serve_forever, daemon=True)
        self.thread.start()
        # Wait for the listening socket to bind
        for _ in range(100):
            if self.server.is_running:
                break
            time.sleep(0.01)
        self.port = self.server.bound_port

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

    assert response == {
        "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
        "success": True,
        "value": 120.0,
    }
    assert received == [{"type": "get", "path": "song", "name": "tempo"}]


@pytest.mark.parametrize("partial_frame", [b"", b"\x00\x00", struct.pack(">I", 20) + b"{"])
def test_server_shutdown_wakes_idle_and_partial_frame_clients(partial_frame):
    """Disconnect cannot leave a receive worker alive or dispatch later input."""
    receiving = threading.Event()
    calls = []
    with _RunningServer(lambda request: calls.append(request) or {"success": True}) as running:
        original = running.server._recv_frame

        def receive(client):
            receiving.set()
            return original(client)

        running.server._recv_frame = receive
        with socket.create_connection(("127.0.0.1", running.port), timeout=1) as sock:
            sock.sendall(partial_frame)
            assert receiving.wait(1)
            running.server.shutdown()
            running.thread.join(timeout=0.5)
            assert not running.thread.is_alive()
            assert not running.server.is_running
            assert calls == []


def test_server_shutdown_before_worker_start_is_terminal():
    """A late worker cannot reopen a server whose surface already disconnected."""
    server = TcpServer(port=0)
    server.shutdown()
    worker = threading.Thread(target=server.serve_forever, daemon=True)
    worker.start()
    try:
        worker.join(timeout=0.5)
        assert not worker.is_alive()
        assert not server.is_running
    finally:
        server.shutdown()
        worker.join(timeout=2)


def test_server_shutdown_declines_a_frame_received_before_dispatch():
    """A complete buffered frame must still cross the stop check before dispatch."""
    received = threading.Event()
    release = threading.Event()
    calls = []
    with _RunningServer(lambda request: calls.append(request) or {"success": True}) as running:
        original = running.server._recv_frame

        def receive(client):
            frame = original(client)
            received.set()
            assert release.wait(2)
            return frame

        running.server._recv_frame = receive
        with socket.create_connection(("127.0.0.1", running.port), timeout=1) as sock:
            _send_frame(sock, {})
            assert received.wait(1)
            running.server.shutdown()
            release.set()
            running.thread.join(timeout=0.5)
            assert not running.thread.is_alive()
            assert calls == []


def test_server_shutdown_preserves_started_response_and_allows_replacement():
    """An accepted request finishes before a new server takes over its port."""
    begun = threading.Event()
    release = threading.Event()

    def handle(request):
        begun.set()
        assert release.wait(2)
        return {"success": True, "value": "finished"}

    with _RunningServer(handle) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=2) as sock:
            _send_frame(sock, {})
            assert begun.wait(1)
            running.server.shutdown()
            release.set()
            assert _recv_frame(sock)["value"] == "finished"
            running.thread.join(timeout=0.5)
            assert not running.thread.is_alive()
        replacement = TcpServer(port=running.port, handler=lambda request: {"success": True})
        worker = threading.Thread(target=replacement.serve_forever, daemon=True)
        worker.start()
        try:
            assert replacement._ready.wait(1)
            with socket.create_connection(("127.0.0.1", running.port), timeout=1) as sock:
                _send_frame(sock, {})
                assert _recv_frame(sock)["success"] is True
        finally:
            replacement.shutdown()
            worker.join(timeout=2)
        assert not worker.is_alive()


def test_sequential_requests_on_one_connection():
    """The server answers multiple framed requests on a single connection."""

    def handler(request: dict) -> dict:
        return {"success": True, "value": request["name"]}

    with _RunningServer(handler) as running:
        with socket.create_connection(("127.0.0.1", running.port), timeout=5) as sock:
            for name in ("tempo", "is_playing", "metronome"):
                _send_frame(sock, {"type": "get", "path": "song", "name": name})
                response = _recv_frame(sock)
                assert response["value"] == name


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


def test_live_11_note_dictionary_reaches_clip_api():
    """The wire shape matches Clip.add_new_notes, including field names."""

    class Clip:
        def __init__(self):
            self.received = None
            self.query = None

        def add_new_notes(self, note_dictionary):
            self.received = note_dictionary
            return [101]

        def get_notes_by_id(self, query):
            self.query = query
            return {
                "notes": [
                    {
                        "note_id": 101,
                        "pitch": 60,
                        "start_time": 0.0,
                        "duration": 1.0,
                        "velocity": 100.0,
                        "mute": False,
                        "probability": 1.0,
                        "velocity_deviation": 0.0,
                        "release_velocity": 23.0,
                    }
                ]
            }

        def get_all_notes_extended(self, query):
            self.all_query = query
            return self.get_notes_by_id({"note_ids": [101], "return": query["return"]})

    class ClipSlot:
        def __init__(self, clip):
            self.clip = clip

    class Track:
        def __init__(self, clip):
            self.clip_slots = [ClipSlot(clip)]

    class Song:
        def __init__(self, clip):
            self.tracks = [Track(clip)]

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    clip = Clip()
    handler = LomHandler(Surface(Song(clip)))
    notes = {
        "notes": [
            {
                "pitch": 60,
                "start_time": 0.0,
                "duration": 1.0,
                "velocity": 100,
                "mute": False,
                "probability": 1.0,
                "velocity_deviation": 0.0,
                "release_velocity": 23.0,
            }
        ]
    }

    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/clip_slots/0/clip",
                "name": "add_new_notes",
                "args": [notes],
            }
        )
    )

    assert response == {"success": True, "value": [101]}
    assert clip.received == notes

    query = {
        "note_ids": [101],
        "return": [
            "note_id",
            "pitch",
            "start_time",
            "duration",
            "velocity",
            "mute",
            "probability",
            "velocity_deviation",
            "release_velocity",
        ],
    }
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/clip_slots/0/clip",
                "name": "get_notes_by_id",
                "args": [query],
            }
        )
    )
    assert response["success"] is True
    assert response["value"]["notes"][0]["note_id"] == 101
    assert clip.query == query

    all_query = {"return": query["return"]}
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/clip_slots/0/clip",
                "name": "get_all_notes_extended",
                "args": [all_query],
            }
        )
    )
    assert response["success"] is True
    assert response["value"]["notes"][0]["note_id"] == 101
    assert clip.all_query == all_query


def test_handler_enforces_get_set_call_algebra_without_silent_noops():
    """Malformed or type-confused requests fail instead of reporting false success."""

    class Scene:
        tempo_enabled = True
        time_signature_enabled = True

    class Song:
        tempo = 120.0
        scenes = [Scene()]

        def stop_playing(self):
            return None

    class Surface:
        def song(self):
            return Song()

    surface = Surface()
    handler = LomHandler(surface)

    missing = handler.handle(_versioned({"type": "get", "path": "song", "name": "not_a_property"}))
    assert missing["success"] is False
    assert "outside Sunny bridge protocol" in missing["error"]

    confused = handler.handle(_versioned({"type": "get", "path": "song", "name": "stop_playing"}))
    assert confused["success"] is False
    assert "outside Sunny bridge protocol" in confused["error"]

    no_value = handler.handle(_versioned({"type": "set", "path": "song", "name": "tempo"}))
    assert no_value["success"] is False
    assert "outside Sunny bridge protocol" in no_value["error"]

    set_value = handler.handle(
        _versioned({"type": "set", "path": "song", "name": "tempo", "args": [137.5]})
    )
    assert set_value == {
        "success": True,
        "value": {"property": "tempo", "requested": 137.5, "observed": 137.5},
    }

    scene_value = handler.handle(
        _versioned(
            {
                "type": "set",
                "path": "song/scenes/0",
                "name": "time_signature_enabled",
                "args": [False],
            }
        )
    )
    assert scene_value == {
        "success": True,
        "value": {
            "property": "time_signature_enabled",
            "requested": False,
            "observed": False,
        },
    }

    wrong_args = handler.handle(
        _versioned({"type": "call", "path": "song", "name": "stop_playing", "args": {}})
    )
    assert wrong_args == {"success": False, "error": "Request args must be an array"}


def test_mixer_crossfade_assignment_uses_exact_integer_readback():
    """A generated Track is removed from both crossfader sides with closed evidence."""

    class Mixer:
        crossfade_assign = 0
        panning_mode = 1

    class Track:
        mixer_device = Mixer()
        arm = True
        implicit_arm = True

    class Song:
        tracks = (Track(),)

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "set",
                "path": "song/tracks/0/mixer_device",
                "name": "crossfade_assign",
                "args": [1],
            }
        )
    )

    assert response == {
        "success": True,
        "value": {"property": "crossfade_assign", "requested": 1, "observed": 1},
    }
    assert Song.tracks[0].mixer_device.crossfade_assign == 1

    response = handler.handle(
        _versioned(
            {
                "type": "set",
                "path": "song/tracks/0/mixer_device",
                "name": "panning_mode",
                "args": [0],
            }
        )
    )
    assert response == {
        "success": True,
        "value": {"property": "panning_mode", "requested": 0, "observed": 0},
    }
    assert Song.tracks[0].mixer_device.panning_mode == 0

    for property_name in ("arm", "implicit_arm"):
        response = handler.handle(
            _versioned(
                {
                    "type": "set",
                    "path": "song/tracks/0",
                    "name": property_name,
                    "args": [False],
                }
            )
        )
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": False, "observed": False},
        }
        assert getattr(Song.tracks[0], property_name) is False


def test_generated_return_gate_uses_exact_boolean_and_stereo_pan_readback():
    """Aux returns clear independent suppressors and apply the modeled scalar pan."""

    class Panning:
        value = -0.5

    class Activator:
        value = 0.0

    class Mixer:
        crossfade_assign = 0
        panning_mode = 1
        track_activator = Activator()
        panning = Panning()

    class ReturnTrack:
        mute = True
        solo = True
        mixer_device = Mixer()

    class Song:
        return_tracks = (ReturnTrack(),)

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    operations = (
        ("song/return_tracks/0", "mute", False),
        ("song/return_tracks/0", "solo", False),
        ("song/return_tracks/0/mixer_device", "crossfade_assign", 1),
        ("song/return_tracks/0/mixer_device", "panning_mode", 0),
        ("song/return_tracks/0/mixer_device/track_activator", "value", 1.0),
        ("song/return_tracks/0/mixer_device/panning", "value", 0.25),
    )
    for path, property_name, value in operations:
        response = handler.handle(
            _versioned({"type": "set", "path": path, "name": property_name, "args": [value]})
        )
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }

    assert Song.return_tracks[0].mute is False
    assert Song.return_tracks[0].solo is False
    assert Song.return_tracks[0].mixer_device.crossfade_assign == 1
    assert Song.return_tracks[0].mixer_device.panning_mode == 0
    assert Song.return_tracks[0].mixer_device.track_activator.value == 1.0
    assert Song.return_tracks[0].mixer_device.panning.value == 0.25


def test_generated_main_gate_uses_exact_activator_and_centered_stereo_pan_readback():
    """The project-wide Main stage cannot inherit a muted or panned target state."""

    class Parameter:
        def __init__(self, value):
            self.value = value

    class Mixer:
        track_activator = Parameter(0.0)
        panning_mode = 1
        panning = Parameter(-0.5)

    class Master:
        mixer_device = Mixer()

    class Song:
        master_track = Master()

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    operations = (
        ("song/master_track/mixer_device/track_activator", "value", 1.0),
        ("song/master_track/mixer_device", "panning_mode", 0),
        ("song/master_track/mixer_device/panning", "value", 0.0),
    )
    for path, property_name, value in operations:
        response = handler.handle(
            _versioned({"type": "set", "path": path, "name": property_name, "args": [value]})
        )
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }

    assert Song.master_track.mixer_device.track_activator.value == 1.0
    assert Song.master_track.mixer_device.panning_mode == 0
    assert Song.master_track.mixer_device.panning.value == 0.0


def test_clip_groove_clear_uses_null_object_assignment_and_exact_readback():
    """A Sunny clip cannot inherit a non-destructive playback groove."""

    class Clip:
        groove = object()

    class Slot:
        clip = Clip()

    class Track:
        clip_slots = (Slot(),)

    class Song:
        tracks = (Track(),)

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "set",
                "path": "song/tracks/0/clip_slots/0/clip",
                "name": "groove",
                "args": [None],
            }
        )
    )

    assert response == {
        "success": True,
        "value": {"property": "groove", "requested": None, "observed": None},
    }
    assert Song.tracks[0].clip_slots[0].clip.groove is None


def test_clip_launch_tuple_is_assigned_with_exact_scalar_readback():
    """The wrapper retains the four bounded Live-11+ launch scalar categories."""

    class Clip:
        launch_mode = 2
        launch_quantization = 0
        legato = True
        velocity_amount = 1.0

    class Slot:
        clip = Clip()

    class Track:
        clip_slots = (Slot(),)

    class Song:
        tracks = (Track(),)

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    path = "song/tracks/0/clip_slots/0/clip"
    for property_name, value in (
        ("launch_mode", 0),
        ("launch_quantization", 1),
        ("legato", False),
        ("velocity_amount", 0.0),
    ):
        response = handler.handle(
            _versioned({"type": "set", "path": path, "name": property_name, "args": [value]})
        )
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }
        assert getattr(Song.tracks[0].clip_slots[0].clip, property_name) == value


def test_clip_envelope_clear_returns_exact_absence_evidence():
    """The adapter maps the public destructive call to one closed Boolean observation."""

    class Clip:
        has_envelopes = True

        def clear_all_envelopes(self):
            self.has_envelopes = False

    class Slot:
        clip = Clip()

    class Track:
        clip_slots = (Slot(),)

    class Song:
        tracks = (Track(),)

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/clip_slots/0/clip",
                "name": "sunny_clear_all_envelopes",
                "args": [],
            }
        )
    )

    assert response == {"success": True, "value": {"has_envelopes": False}}
    assert Song.tracks[0].clip_slots[0].clip.has_envelopes is False


def test_handler_requires_current_protocol_envelope_and_rejects_reflection_before_lom_access():
    """Only the canonical, versioned Sunny algebra can reach a Live object."""

    class Song:
        def __init__(self):
            self.stop_calls = 0

        def stop_playing(self):
            self.stop_calls += 1

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    song = Song()
    handler = LomHandler(Surface(song))
    unversioned = handler.handle({"type": "call", "path": "song", "name": "stop_playing"})
    assert unversioned["success"] is False
    assert "Unsupported bridge protocol version" in unversioned["error"]

    for request in (
        _versioned({"type": "call", "path": "song", "name": "stop_playing"}),
        _versioned({"type": "get", "path": "song", "name": "metronome"}),
        _versioned({"type": "set", "path": "tracks/0", "name": "mute", "args": [True]}),
        _versioned({"type": "get", "path": "song/__class__", "name": "__dict__"}),
        _versioned({"type": "get", "path": "/song", "name": "tempo"}),
    ):
        response = handler.handle(request)
        assert response["success"] is False
        assert "outside Sunny bridge protocol" in response["error"]
    assert song.stop_calls == 0

    wrong_version = _versioned({"type": "get", "path": "song", "name": "tempo"})
    wrong_version["bridge_protocol_version"] -= 1
    assert handler.handle(wrong_version)["success"] is False
    with_extra_field = _versioned({"type": "get", "path": "song", "name": "tempo", "x": 1})
    assert handler.handle(with_extra_field) == {
        "success": False,
        "error": "Request contains unknown fields",
    }


def test_current_protocol_operation_algebra_covers_compilers_without_open_ended_lom_access():
    """Every compiler primitive is admitted by shape; adjacent reflection is declined."""
    note_dictionary = {
        "notes": [
            {
                "pitch": 60,
                "start_time": 0.0,
                "duration": 1.0,
                "velocity": 100,
                "mute": False,
                "probability": 1.0,
                "velocity_deviation": 0.0,
                "release_velocity": 64.0,
            }
        ]
    }
    note_query = {
        "note_ids": [101],
        "return": [
            "note_id",
            "pitch",
            "start_time",
            "duration",
            "velocity",
            "mute",
            "probability",
            "velocity_deviation",
            "release_velocity",
        ],
    }
    all_notes_query = {"return": note_query["return"]}
    allowed = (
        ("get", "song", "tempo", []),
        ("call", "song", "sunny_get_scene_count", []),
        ("call", "song", "sunny_get_return_track_count", []),
        ("set", "song", "tempo", [120.0]),
        ("set", "song", "signature_numerator", [4]),
        ("set", "song", "signature_denominator", [4]),
        ("set", "song/scenes/0", "tempo_enabled", [False]),
        ("set", "song/scenes/0", "time_signature_enabled", [False]),
        ("set", "song/scenes/0", "name", ["Full Score"]),
        ("call", "song", "create_scene", [0]),
        ("call", "song", "create_midi_track", [0]),
        ("call", "song", "create_return_track", []),
        ("call", "song", "sunny_get_target_profile", []),
        ("call", "song", "sunny_get_target_snapshot", []),
        ("call", "song", "sunny_set_cue", [4.0, "Verse"]),
        ("set", "song/tracks/0", "name", ["Part"]),
        ("set", "song/tracks/0", "mute", [False]),
        ("set", "song/tracks/0", "arm", [False]),
        ("set", "song/tracks/0", "implicit_arm", [False]),
        ("set", "song/tracks/0/mixer_device", "crossfade_assign", [1]),
        ("set", "song/return_tracks/0/mixer_device", "crossfade_assign", [1]),
        ("set", "song/return_tracks/0", "mute", [False]),
        ("set", "song/return_tracks/0", "solo", [False]),
        ("set", "song/tracks/0/mixer_device", "panning_mode", [0]),
        ("set", "song/return_tracks/0/mixer_device", "panning_mode", [0]),
        ("set", "song/master_track/mixer_device", "panning_mode", [0]),
        ("set", "song/tracks/0/mixer_device/track_activator", "value", [0.0]),
        ("set", "song/return_tracks/0/mixer_device/track_activator", "value", [1.0]),
        ("set", "song/master_track/mixer_device/track_activator", "value", [1.0]),
        ("set", "song/master_track/mixer_device/panning", "value", [0.0]),
        ("call", "song/tracks/0", "insert_device", ["Operator", 0]),
        ("call", "song/return_tracks/0", "sunny_get_device_count", []),
        ("call", "song/master_track", "insert_device", ["Limiter"]),
        ("call", "song/tracks/0/clip_slots/0", "create_clip", [4.0]),
        ("call", "song/tracks/0/clip_slots/0", "delete_clip", []),
        ("set", "song/tracks/0/clip_slots/0/clip", "name", ["Verse"]),
        ("set", "song/tracks/0/clip_slots/0/clip", "signature_numerator", [7]),
        ("set", "song/tracks/0/clip_slots/0/clip", "signature_denominator", [8]),
        ("set", "song/tracks/0/clip_slots/0/clip", "start_marker", [0.0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "end_marker", [4.0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "looping", [False]),
        ("set", "song/tracks/0/clip_slots/0/clip", "muted", [False]),
        ("set", "song/tracks/0/clip_slots/0/clip", "launch_mode", [0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "launch_quantization", [1]),
        ("set", "song/tracks/0/clip_slots/0/clip", "legato", [False]),
        ("set", "song/tracks/0/clip_slots/0/clip", "velocity_amount", [0.0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "groove", [None]),
        ("call", "song/tracks/0/clip_slots/0/clip", "sunny_clear_all_envelopes", []),
        ("call", "song/tracks/0/clip_slots/0/clip", "add_new_notes", [note_dictionary]),
        ("call", "song/tracks/0/clip_slots/0/clip", "get_notes_by_id", [note_query]),
        (
            "call",
            "song/tracks/0/clip_slots/0/clip",
            "get_all_notes_extended",
            [all_notes_query],
        ),
        ("set", "song/tracks/0/mixer_device/volume", "display_value", [-6.0]),
        ("set", "song/tracks/0/mixer_device/panning", "value", [0.25]),
        ("set", "song/return_tracks/0/mixer_device/panning", "value", [0.25]),
        ("set", "song/tracks/0/mixer_device/sends/0", "display_value", [-12.0]),
        (
            "call",
            "song/tracks/0/devices/0",
            "sunny_get_device_parameter",
            ["Dry/Wet", "value"],
        ),
        (
            "call",
            "song/tracks/0/devices/0",
            "sunny_set_device_parameter",
            ["Dry/Wet", 0.5, "value", 0.0, 1.0],
        ),
    )
    for operation in allowed:
        assert _request_allowed(*operation), operation

    for release_velocity in (0.0, 23.0, 127.0):
        expressive = {
            "notes": [{**note_dictionary["notes"][0], "release_velocity": release_velocity}]
        }
        assert _request_allowed(
            "call", "song/tracks/0/clip_slots/0/clip", "add_new_notes", [expressive]
        )
    for release_velocity in (23, 23.5, -1.0, 128.0, float("nan")):
        malformed = {
            "notes": [{**note_dictionary["notes"][0], "release_velocity": release_velocity}]
        }
        assert not _request_allowed(
            "call", "song/tracks/0/clip_slots/0/clip", "add_new_notes", [malformed]
        )

    declined = (
        ("call", "song", "stop_playing", []),
        ("get", "song", "metronome", []),
        ("get", "song", "scenes", []),
        ("get", "song", "tracks", []),
        ("get", "song", "return_tracks", []),
        ("call", "song", "sunny_get_scene_count", [0]),
        ("get", "tracks/0", "mute", []),
        ("get", "song/tracks/00", "mute", []),
        ("get", "song/tracks/١", "mute", []),
        ("get", "song/tracks/2147483648", "mute", []),
        ("get", "song/tracks/0/__class__", "__dict__", []),
        ("set", "song", "tempo", [float("nan")]),
        ("set", "song", "signature_numerator", [100]),
        ("set", "song", "signature_denominator", [32]),
        ("set", "song/scenes/0", "tempo_enabled", [0]),
        ("set", "song/scenes/0", "tempo", [120.0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "signature_numerator", [100]),
        ("set", "song/tracks/0/clip_slots/0/clip", "signature_denominator", [3]),
        ("set", "song/tracks/0/clip_slots/0/clip", "start_marker", [-1.0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "end_marker", [float("inf")]),
        ("set", "song/tracks/0/clip_slots/0/clip", "looping", [0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "muted", [0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "launch_mode", [1]),
        ("set", "song/tracks/0/clip_slots/0/clip", "launch_mode", [False]),
        ("set", "song/tracks/0/clip_slots/0/clip", "launch_quantization", [0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "legato", [True]),
        ("set", "song/tracks/0/clip_slots/0/clip", "velocity_amount", [0]),
        ("set", "song/tracks/0/clip_slots/0/clip", "velocity_amount", [0.5]),
        ("set", "song/tracks/0/clip_slots/0/clip", "groove", [False]),
        ("set", "song/tracks/0/mixer_device", "crossfade_assign", [0]),
        ("set", "song/tracks/0/mixer_device", "crossfade_assign", [2]),
        ("set", "song/tracks/0/mixer_device", "crossfade_assign", [True]),
        ("set", "song/master_track/mixer_device", "crossfade_assign", [1]),
        ("set", "song/tracks/0/mixer_device", "panning_mode", [1]),
        ("set", "song/tracks/0/mixer_device", "panning_mode", [False]),
        ("set", "song/tracks/0/mixer_device/track_activator", "value", [1]),
        ("set", "song/tracks/0/mixer_device/track_activator", "value", [0.5]),
        ("set", "song/return_tracks/0/mixer_device/track_activator", "value", [0.0]),
        ("set", "song/return_tracks/0/mixer_device/track_activator", "value", [1]),
        ("set", "song/master_track/mixer_device/track_activator", "value", [0.0]),
        ("set", "song/master_track/mixer_device/track_activator", "value", [1]),
        ("set", "song/master_track/mixer_device/panning", "value", [0.25]),
        ("set", "song/master_track/mixer_device/panning", "value", [0]),
        ("set", "song/tracks/0", "arm", [True]),
        ("set", "song/tracks/0", "implicit_arm", [True]),
        ("set", "song/tracks/0", "arm", [0]),
        ("set", "song/return_tracks/0", "arm", [False]),
        ("set", "song/return_tracks/0", "mute", [0]),
        ("set", "song/return_tracks/0", "mute", [True]),
        (
            "call",
            "song/tracks/0/clip_slots/0/clip",
            "sunny_clear_all_envelopes",
            [False],
        ),
        ("call", "song", "create_scene", [2147483648]),
        ("call", "song/tracks/0", "insert_device", ["Operator", 2147483648]),
        ("call", "song/tracks/0/clip_slots/0", "create_clip", [0.0]),
        ("call", "song/tracks/0/clip_slots/0/clip", "add_new_notes", [{"notes": []}]),
        (
            "call",
            "song/tracks/0/clip_slots/0/clip",
            "get_notes_by_id",
            [{"note_ids": [101, 101], "return": note_query["return"]}],
        ),
        (
            "call",
            "song/tracks/0/clip_slots/0/clip",
            "get_notes_by_id",
            [{"note_ids": [2147483648], "return": note_query["return"]}],
        ),
        (
            "call",
            "song/tracks/0/clip_slots/0/clip",
            "get_all_notes_extended",
            [{"return": note_query["return"], "note_ids": [101]}],
        ),
        (
            "call",
            "song/tracks/0/devices/0",
            "sunny_get_device_parameter",
            ["Dry/Wet", "unknown"],
        ),
        (
            "call",
            "song/tracks/0/devices/0",
            "sunny_set_device_parameter",
            ["Dry/Wet", 0.5, "value", 1.0, 0.0],
        ),
    )
    for operation in declined:
        assert not _request_allowed(*operation), operation


def test_song_collection_counts_are_closed_scalars_not_stringified_object_lists():
    """Cardinality adapters do not export private Live object labels as pseudo-values."""
    song = SimpleNamespace(
        scenes=(object(), object(), object()),
        return_tracks=[object(), object()],
    )
    handler = LomHandler(SimpleNamespace(song=lambda: song))

    for method, expected in (
        ("sunny_get_scene_count", 3),
        ("sunny_get_return_track_count", 2),
    ):
        response = handler.handle(
            _versioned({"type": "call", "path": "song", "name": method, "args": []})
        )
        assert response == {"success": True, "value": expected}

    song.scenes = iter([object()])
    malformed = handler.handle(
        _versioned({"type": "call", "path": "song", "name": "sunny_get_scene_count", "args": []})
    )
    assert malformed["success"] is False
    assert "invalid scenes collection" in malformed["error"]


def test_response_serialisation_rejects_private_or_non_json_host_values():
    """Unsupported host wrappers cannot masquerade as strings or permissive JSON."""

    class AbletonVector:
        def __iter__(self):
            return iter((1, 2.0, "three"))

    assert LomHandler._serialise(AbletonVector()) == [1, 2.0, "three"]

    invalid_values = (
        object(),
        float("nan"),
        float("inf"),
        1 << 65,
        {1: "coerced key"},
        {"nested": object()},
    )
    for value in invalid_values:
        with pytest.raises(RuntimeError):
            LomHandler._serialise(value)

    song = SimpleNamespace(tempo=object())
    handler = LomHandler(SimpleNamespace(song=lambda: song))
    response = handler.handle(
        _versioned({"type": "get", "path": "song", "name": "tempo", "args": []})
    )
    assert response["success"] is False
    assert "unsupported private object" in response["error"]


def test_sunny_set_cue_creates_named_cue_and_restores_playhead():
    """The bridge adapter gives Song's toggle-only cue API safe set semantics."""

    class Cue:
        def __init__(self, time):
            self.time = time
            self.name = ""

    class Song:
        def __init__(self):
            self.current_song_time = 9.0
            self.cue_points = []

        def set_or_delete_cue(self):
            self.cue_points.append(Cue(self.current_song_time))

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    song = Song()
    handler = LomHandler(Surface(song))

    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [16.0, "Verse"],
            }
        )
    )

    assert response == {
        "success": True,
        "value": {
            "action": "created",
            "requested_time": 16.0,
            "observed_time": 16.0,
            "requested_name": "Verse",
            "observed_name": "Verse",
        },
    }
    assert song.current_song_time == 9.0
    assert [(cue.time, cue.name) for cue in song.cue_points] == [(16.0, "Verse")]

    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song",
                "name": "sunny_set_cue",
                "args": [16.0, "Chorus"],
            }
        )
    )
    assert response == {
        "success": True,
        "value": {
            "action": "updated",
            "requested_time": 16.0,
            "observed_time": 16.0,
            "requested_name": "Chorus",
            "observed_name": "Chorus",
        },
    }
    assert [(cue.time, cue.name) for cue in song.cue_points] == [(16.0, "Chorus")]


def test_sunny_set_device_parameter_requires_exact_enabled_parameter():
    """Device parameter adaptation searches names and declines bad targets."""

    class Parameter:
        def __init__(self, name, enabled=True):
            self.name = name
            self.original_name = name
            self.is_enabled = enabled
            self.value = 0.0
            self.display_value = 0.0
            self.min = 0.0
            self.max = 1.0
            self.is_quantized = False
            self.default_value = 0.0
            self.state = 0
            self.automation_state = 0

    class Device:
        def __init__(self):
            self.parameters = [Parameter("Dry/Wet")]

    class Track:
        def __init__(self):
            self.mixer_device = object()
            self.devices = [self.mixer_device, Device()]

    class Song:
        def __init__(self):
            self.tracks = [Track()]

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    song = Song()
    handler = LomHandler(Surface(song))
    request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0/devices/0",
            "name": "sunny_set_device_parameter",
            "args": ["Dry/Wet", 0.25, "value", 0.0, 1.0],
        }
    )

    assert handler.handle(request) == {
        "success": True,
        "value": {
            "matched_name": "Dry/Wet",
            "original_name": "Dry/Wet",
            "property": "value",
            "requested": 0.25,
            "observed": 0.25,
            "minimum": 0.0,
            "maximum": 1.0,
            "is_quantized": False,
            "default_value": 0.0,
            "value_items": None,
            "is_enabled": True,
            "state": 0,
            "automation_state": 0,
        },
    }
    parameter = song.tracks[0].devices[1].parameters[0]
    assert parameter.value == 0.25
    observation_request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0/devices/0",
            "name": "sunny_get_device_parameter",
            "args": ["Dry/Wet", "value"],
        }
    )
    assert handler.handle(observation_request) == {
        "success": True,
        "value": {
            "matched_name": "Dry/Wet",
            "original_name": "Dry/Wet",
            "property": "value",
            "observed": 0.25,
            "minimum": 0.0,
            "maximum": 1.0,
            "is_quantized": False,
            "default_value": 0.0,
            "value_items": None,
            "is_enabled": True,
            "state": 0,
            "automation_state": 0,
        },
    }
    parameter.is_enabled = False
    parameter.state = 2
    disabled = handler.handle(observation_request)
    assert disabled["success"] is True
    assert disabled["value"]["is_enabled"] is False
    assert disabled["value"]["state"] == 2
    assert parameter.value == 0.25
    parameter.is_enabled = True
    parameter.state = 0
    parameter.is_enabled = 1
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "invalid Boolean" in malformed["error"]
    assert parameter.value == 0.25
    parameter.is_enabled = True
    parameter.value = float("nan")
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "non-finite" in malformed["error"]
    parameter.value = 0.25
    parameter.max = -1.0
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "inverted range" in malformed["error"]
    parameter.max = 1.0
    parameter.default_value = 0
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "default_value" in malformed["error"]
    parameter.default_value = 2.0
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "outside its reported range" in malformed["error"]
    parameter.is_quantized = True
    parameter.value_items = ("Dry", "Wet")
    quantized = handler.handle(observation_request)
    assert quantized["success"] is True
    assert quantized["value"]["default_value"] is None
    assert quantized["value"]["value_items"] == ["Dry", "Wet"]
    parameter.value_items = ["Dry", 1]
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "value_items entry" in malformed["error"]
    parameter.is_quantized = False
    parameter.default_value = 0.0
    request["args"] = ["Dry/Wet", 37.5, "display_value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is True
    assert response["value"]["property"] == "display_value"
    assert response["value"]["observed"] == 37.5
    assert parameter.display_value == 37.5

    parameter.state = 1
    parameter.automation_state = 2
    request["args"] = ["Dry/Wet", 0.3, "value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is True
    assert response["value"]["state"] == 1
    assert response["value"]["automation_state"] == 2

    parameter.state = 2
    request["args"] = ["Dry/Wet", 0.6, "value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "cannot be changed" in response["error"]
    assert parameter.value == 0.3

    parameter.state = 0
    parameter.automation_state = 0

    request["args"] = ["Dry/Wet", 0.75, "value", 0.0, 2.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "does not match expected" in response["error"]
    assert parameter.value == 0.3

    request["args"] = ["Unknown", 0.5, "value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "not found" in response["error"]

    request["args"] = ["Dry/Wet", 0.5, "not_a_lom_property", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_insert_device_returns_exact_identity_type_and_activity_evidence():
    """The adapter distinguishes insertion acceptance from an active instrument."""

    class Device:
        def __init__(self, name):
            self.name = name
            self.class_display_name = name
            self.class_name = name.replace(" ", "")
            self.type = 1
            self.is_active = True
            self.can_have_chains = False
            self.latency_in_samples = 64
            self.latency_in_ms = 1.5

    class Track:
        def __init__(self):
            self.mixer_device = object()
            self.devices = [self.mixer_device]
            self.has_audio_output = False
            self.has_midi_output = True

        def insert_device(self, name, index):
            self.devices.insert(index + 1, Device(name))
            self.has_audio_output = True
            self.has_midi_output = False

    class Song:
        def __init__(self):
            self.tracks = [Track()]

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    handler = LomHandler(Surface(Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0",
                "name": "insert_device",
                "args": ["Operator", 0],
            }
        )
    )

    assert response == {
        "success": True,
        "value": {
            "requested_name": "Operator",
            "requested_index": 0,
            "before_count": 0,
            "after_count": 1,
            "device_index": 0,
            "name": "Operator",
            "class_display_name": "Operator",
            "class_name": "Operator",
            "type": 1,
            "is_active": True,
            "can_have_chains": False,
            "latency_in_samples": 64,
            "latency_in_ms": 1.5,
            "track_has_audio_output": True,
            "track_has_midi_output": False,
        },
    }


def test_device_snapshot_rejects_non_boolean_rack_and_activity_facts():
    """The private peer must not coerce category-confused Device facts into evidence."""
    device = SimpleNamespace(
        name="Operator",
        class_display_name="Operator",
        class_name="Operator",
        type=1,
        is_active=True,
        can_have_chains=False,
        latency_in_samples=64,
        latency_in_ms=1.5,
    )

    assert LomHandler._device_snapshot(device)["can_have_chains"] is False

    device.can_have_chains = 0
    with pytest.raises(RuntimeError, match="invalid can_have_chains"):
        LomHandler._device_snapshot(device)

    device.can_have_chains = False
    device.is_active = 1
    with pytest.raises(RuntimeError, match="invalid is_active"):
        LomHandler._device_snapshot(device)

    device.is_active = True
    device.type = True
    with pytest.raises(RuntimeError, match="invalid type"):
        LomHandler._device_snapshot(device)


def test_sunny_set_device_parameter_rejects_ambiguous_name_without_mutation():
    """A duplicate public/original name cannot select a parameter by list accident."""

    class Parameter:
        def __init__(self, name, original_name):
            self.name = name
            self.original_name = original_name
            self.is_enabled = True
            self.value = 0.0
            self.display_value = 0.0
            self.min = 0.0
            self.max = 1.0
            self.is_quantized = False
            self.default_value = 0.0
            self.state = 0
            self.automation_state = 0

    class Device:
        def __init__(self):
            self.parameters = [
                Parameter("Cutoff", "Filter Frequency"),
                Parameter("Filter Frequency", "Frequency"),
            ]

    class Track:
        def __init__(self):
            self.devices = [Device()]

    class Song:
        def __init__(self):
            self.tracks = [Track()]

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    song = Song()
    handler = LomHandler(Surface(song))
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/devices/0",
                "name": "sunny_set_device_parameter",
                "args": ["Filter Frequency", 0.5, "value", 0.0, 1.0],
            }
        )
    )

    assert response["success"] is False
    assert "ambiguous" in response["error"]
    assert [parameter.value for parameter in song.tracks[0].devices[0].parameters] == [0.0, 0.0]

    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song/tracks/0/devices/0",
                "name": "sunny_get_device_parameter",
                "args": ["Filter Frequency", "value"],
            }
        )
    )
    assert response["success"] is False
    assert "ambiguous" in response["error"]


def test_sunny_set_device_parameter_reports_post_write_host_state():
    """Activity/automation evidence is sampled after the write that can change it."""

    class Parameter:
        name = "Cutoff"
        original_name = "Cutoff"
        is_enabled = True
        display_value = 0.0
        min = 0.0
        max = 1.0
        is_quantized = False
        default_value = 0.0
        state = 0
        automation_state = 1

        def __init__(self):
            self._value = 0.0

        @property
        def value(self):
            return self._value

        @value.setter
        def value(self, requested):
            self._value = requested
            self.state = 1
            self.automation_state = 2

    parameter = Parameter()
    evidence = LomHandler._set_device_parameter(
        SimpleNamespace(parameters=[parameter]), "Cutoff", 0.5, "value", 0.0, 1.0
    )

    assert evidence["observed"] == 0.5
    assert evidence["state"] == 1
    assert evidence["automation_state"] == 2

    parameter._value = 1
    with pytest.raises(RuntimeError, match="Device parameter 'Cutoff' value"):
        LomHandler._get_device_parameter(SimpleNamespace(parameters=[parameter]), "Cutoff", "value")

    parameter._value = 0.5
    parameter.name = object()
    with pytest.raises(RuntimeError, match="Device parameter 'Cutoff' name"):
        LomHandler._get_device_parameter(SimpleNamespace(parameters=[parameter]), "Cutoff", "value")


def test_sunny_get_device_count_reports_exact_track_state_and_rejects_args():
    """Inserted-device addressing is based on observed track state, not a guessed index."""

    class Track:
        def __init__(self):
            self.mixer_device = object()
            self.devices = [object(), self.mixer_device, object(), object()]

    class Song:
        def __init__(self):
            self.tracks = [Track()]

    class Surface:
        def __init__(self, song):
            self._song = song

        def song(self):
            return self._song

    handler = LomHandler(Surface(Song()))
    request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0",
            "name": "sunny_get_device_count",
            "args": [],
        }
    )
    assert handler.handle(request) == {"success": True, "value": 3}

    request["args"] = ["unexpected"]
    response = handler.handle(request)
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_output_routing_mutation_is_advertised_two_stage_and_fail_closed():
    """Route mutation re-enumerates target choices and rejects incoherent stages."""
    master = {"display_name": "Master", "identifier": "master"}
    group = {"display_name": "Group 1", "identifier": "group_1"}
    stereo = {"display_name": "1/2", "identifier": "stereo_1_2"}
    group_stereo = {"display_name": "Group", "identifier": "group_stereo"}

    class Track:
        def __init__(self):
            self._output_routing_type = master.copy()
            self.output_routing_channel = stereo.copy()
            self.available_output_routing_types = {
                "available_output_routing_types": [master.copy(), group.copy()]
            }
            self.available_output_routing_channels = {
                "available_output_routing_channels": [stereo.copy()]
            }

        @property
        def output_routing_type(self):
            return self._output_routing_type

        @output_routing_type.setter
        def output_routing_type(self, value):
            self._output_routing_type = value
            if value == group:
                self.output_routing_channel = group_stereo.copy()
                self.available_output_routing_channels = {
                    "available_output_routing_channels": [group_stereo.copy()]
                }
            else:
                self.output_routing_channel = stereo.copy()
                self.available_output_routing_channels = {
                    "available_output_routing_channels": [stereo.copy()]
                }

    class Song:
        def __init__(self):
            self.tracks = [Track()]
            self.return_tracks = [Track()]

    class Surface:
        def __init__(self):
            self._song = Song()

        def song(self):
            return self._song

    surface = Surface()
    handler = LomHandler(surface)
    type_request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0",
            "name": "sunny_set_output_routing_type",
            "args": [group.copy()],
        }
    )
    type_response = handler.handle(type_request)
    assert type_response["success"] is True
    assert type_response["value"] == {
        "requested_type": group,
        "available_output_routing_types_before": {
            "available_output_routing_types": [master, group]
        },
        "output_routing_type": group,
        "output_routing_channel": group_stereo,
        "available_output_routing_types": {"available_output_routing_types": [master, group]},
        "available_output_routing_channels": {"available_output_routing_channels": [group_stereo]},
    }

    channel_request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0",
            "name": "sunny_set_output_routing_channel",
            "args": [group.copy(), group_stereo.copy()],
        }
    )
    channel_response = handler.handle(channel_request)
    assert channel_response["success"] is True
    assert channel_response["value"]["output_routing_type_before"] == group
    assert channel_response["value"]["requested_channel"] == group_stereo
    assert channel_response["value"]["output_routing_channel"] == group_stereo
    assert channel_response["value"]["available_output_routing_channels_before"] == {
        "available_output_routing_channels": [group_stereo]
    }

    unavailable_type = handler.handle(
        _versioned(
            {
                **type_request,
                "args": [{"display_name": "External", "identifier": "external"}],
            }
        )
    )
    assert unavailable_type["success"] is False
    assert "not in available_output_routing_types" in unavailable_type["error"]
    assert surface._song.tracks[0].output_routing_type == group

    surface._song.tracks[0].output_routing_type = master.copy()
    changed_type = handler.handle(channel_request)
    assert changed_type["success"] is False
    assert "changed before channel mutation" in changed_type["error"]
    assert surface._song.tracks[0].output_routing_channel == stereo

    malformed = handler.handle(
        _versioned(
            {
                **type_request,
                "args": [{"display_name": "Master", "identifier": "master", "extra": 1}],
            }
        )
    )
    assert malformed["success"] is False
    assert "outside Sunny bridge protocol" in malformed["error"]
    assert not _request_allowed(
        "call", "song/master_track", "sunny_set_output_routing_type", [master]
    )


def test_target_profile_uses_documented_application_version_and_conservative_gaps(monkeypatch):
    """The handshake reports observed Live facts without inventing Max availability."""

    class Application:
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

    live_module = SimpleNamespace(
        Application=SimpleNamespace(get_application=lambda: Application())
    )
    monkeypatch.setitem(sys.modules, "Live", live_module)
    handler = LomHandler(object())

    response = handler.handle(
        _versioned({"type": "call", "path": "song", "name": "sunny_get_target_profile"})
    )

    assert response["success"] is True
    profile = response["value"]
    assert profile["bridge_protocol_version"] == BRIDGE_PROTOCOL_VERSION
    assert profile["live"]["version"]["string"] == "12.2.1"
    assert profile["capabilities"]["clip_add_new_notes"] == "available"
    assert profile["capabilities"]["track_insert_device_native"] == "unavailable"
    assert profile["capabilities"]["max_for_live"] == "unknown"
    assert profile["capabilities"]["structural_snapshot"] == "available"
    assert profile["adapter"]["contract"] == "version_coupled_private"


def test_target_profile_rejects_scalar_category_coercion(monkeypatch):
    """Private host values cannot acquire documented profile categories via constructors."""
    application = SimpleNamespace(
        get_major_version=lambda: 12,
        get_minor_version=lambda: 3,
        get_bugfix_version=lambda: 5,
        get_version_string=lambda: "12.3.5",
    )
    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(Application=SimpleNamespace(get_application=lambda: application)),
    )
    handler = LomHandler(object())
    request = _versioned({"type": "call", "path": "song", "name": "sunny_get_target_profile"})

    for method_name, malformed_value, expected_error in (
        ("get_major_version", "12", "Application major version"),
        ("get_minor_version", 3.0, "Application minor version"),
        ("get_bugfix_version", True, "Application bugfix version"),
        ("get_version_string", object(), "Application version string"),
    ):
        original = getattr(application, method_name)
        setattr(application, method_name, lambda value=malformed_value: value)
        response = handler.handle(request)
        assert response["success"] is False
        assert expected_error in response["error"]
        setattr(application, method_name, original)


def test_target_snapshot_is_single_call_structural_plan_evidence(monkeypatch, sunny_native_module):
    """Protocol v43 reports bounded topology, input/output options, mixer, and latency."""

    class Application:
        get_major_version = staticmethod(lambda: 12)
        get_minor_version = staticmethod(lambda: 3)
        get_bugfix_version = staticmethod(lambda: 5)
        get_version_string = staticmethod(lambda: "12.3.5")

    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(Application=SimpleNamespace(get_application=lambda: Application())),
    )

    class Device:
        name = "Operator"
        class_display_name = "Operator"
        class_name = "Operator"
        type = 1
        is_active = True
        can_have_chains = False
        latency_in_samples = 128
        latency_in_ms = 2.9

    class Parameter:
        value = 0.5
        display_value = 0.0
        min = 0.0
        max = 1.0
        is_quantized = False
        default_value = 0.0
        state = 0
        automation_state = 0
        is_enabled = True

    class ActivatorParameter(Parameter):
        value = 1.0
        display_value = 1.0
        is_quantized = True
        value_items = ("Off", "On")

    class PanParameter(Parameter):
        min = -1.0

    class Mixer:
        volume = Parameter()
        track_activator = ActivatorParameter()
        panning = PanParameter()
        crossfade_assign = 1
        panning_mode = 0

        def __init__(self, send_count=0):
            self.sends = tuple(Parameter() for _ in range(send_count))

    class Clip:
        name = "Verse"
        is_audio_clip = False
        is_midi_clip = True
        is_arrangement_clip = False
        is_session_clip = True
        is_take_lane_clip = False
        length = 4.0
        signature_numerator = 4
        signature_denominator = 4
        start_marker = 0.0
        end_marker = 4.0
        end_time = 4.0
        looping = False
        muted = False
        has_envelopes = False
        is_playing = False
        is_recording = False
        is_overdubbing = False
        is_triggered = False
        will_record_on_start = False
        launch_mode = 0
        launch_quantization = 1
        legato = False
        velocity_amount = 0.0
        has_groove = False

    class Scene:
        def __init__(self, name, tempo_enabled=False, signature_enabled=False):
            self.name = name
            self.is_triggered = False
            self.tempo_enabled = tempo_enabled
            self.tempo = 128.0 if tempo_enabled else -1.0
            self.time_signature_enabled = signature_enabled
            self.time_signature_numerator = 7 if signature_enabled else -1
            self.time_signature_denominator = 8 if signature_enabled else -1

    class Slot:
        def __init__(self, clip=None):
            self.clip = clip
            self.has_clip = clip is not None
            self.has_stop_button = clip is None
            self.is_group_slot = False
            self.controls_other_clips = False
            self.is_playing = False
            self.is_recording = False
            self.is_triggered = False
            self.playing_status = 0
            self.will_record_on_start = False

    class Track:
        def __init__(self, name, clips=True, send_count=0):
            self.name = name
            self.mixer_device = Mixer(send_count)
            self.devices = [self.mixer_device, Device()]
            self.clip_slots = [Slot(Clip()), Slot()] if clips else []
            self.arrangement_clips = ()
            self.take_lanes = ()
            self.group_track = None
            self.is_grouped = False
            self.input_routing_type = {"display_name": "All Ins", "identifier": "all_ins"}
            self.input_routing_channel = {
                "display_name": "All Channels",
                "identifier": "all_channels",
            }
            self.available_input_routing_types = {
                "available_input_routing_types": (
                    {"display_name": "No Input", "identifier": "no_input"},
                    {"display_name": "All Ins", "identifier": "all_ins"},
                )
            }
            self.available_input_routing_channels = {
                "available_input_routing_channels": [
                    {"display_name": "All Channels", "identifier": "all_channels"},
                    {"display_name": "Ch. 1", "identifier": "channel_1"},
                ]
            }
            self.output_routing_type = {"display_name": "Master", "identifier": "master"}
            self.output_routing_channel = {"display_name": "1/2", "identifier": "stereo_1_2"}
            self.available_output_routing_types = {
                "available_output_routing_types": (
                    {"display_name": "Master", "identifier": "master"},
                    {"display_name": "Group 1", "identifier": "group_1"},
                )
            }
            self.available_output_routing_channels = {
                "available_output_routing_channels": [
                    {"display_name": "1/2", "identifier": "stereo_1_2"},
                    {"display_name": "1", "identifier": "mono_1"},
                ]
            }
            self.has_audio_input = False
            self.has_midi_input = True
            self.input_meter_level = 0.0
            self.output_meter_level = 0.0
            self.has_audio_output = clips
            self.input_meter_left = 0.0
            self.input_meter_right = 0.0
            self.output_meter_left = 0.0
            self.output_meter_right = 0.0
            self.has_midi_output = False
            self.is_frozen = False
            self.arm = False
            self.implicit_arm = False
            self.back_to_arranger = False
            self.fired_slot_index = -1
            self.playing_slot_index = -1
            self.mute = False
            self.solo = False
            self.muted_via_solo = False

    class Cue:
        name = "Start"
        time = 0.0

    class TuningSystem:
        name = "12-TET"
        pseudo_octave_in_cents = 1200.0
        lowest_note = {"opaque_fixture": "lowest"}
        highest_note = {"opaque_fixture": "highest"}
        reference_pitch = {"opaque_fixture": "reference"}
        note_tunings = {
            "opaque_fixture": [
                0.0,
                100.0,
                200.0,
                300.0,
                400.0,
                500.0,
                600.0,
                700.0,
                800.0,
                900.0,
                1000.0,
                1100.0,
            ]
        }

    class Song:
        tempo = 120.0
        signature_numerator = 4
        signature_denominator = 4
        is_playing = False
        is_counting_in = False
        arrangement_overdub = False
        overdub = False
        record_mode = False
        session_record = False
        session_automation_record = False
        is_ableton_link_enabled = False
        is_ableton_link_start_stop_sync_enabled = False
        tempo_follower_enabled = False
        nudge_down = False
        nudge_up = False
        back_to_arranger = False
        re_enable_automation_enabled = False
        loop = False
        metronome = False
        root_note = 0
        scale_name = "Major"
        scale_intervals = (0, 2, 4, 5, 7, 9, 11)
        scale_mode = True
        tuning_system = TuningSystem()

        def __init__(self):
            self.scenes = (Scene("Scene 1"), Scene("Scene 2", True, True))
            self.tracks = (Track("Part", send_count=1),)
            self.return_tracks = (Track("Return A", clips=False),)
            del self.return_tracks[0].input_routing_type
            del self.return_tracks[0].input_routing_channel
            del self.return_tracks[0].available_input_routing_types
            del self.return_tracks[0].available_input_routing_channels
            del self.return_tracks[0].has_audio_input
            del self.return_tracks[0].has_midi_input
            del self.return_tracks[0].input_meter_level
            del self.return_tracks[0].output_meter_level
            self.master_track = Track("Master", clips=False)
            del self.master_track.input_routing_type
            del self.master_track.input_routing_channel
            del self.master_track.available_input_routing_types
            del self.master_track.available_input_routing_channels
            del self.master_track.has_audio_input
            del self.master_track.has_midi_input
            del self.master_track.input_meter_level
            del self.master_track.output_meter_level
            del self.master_track.output_routing_type
            del self.master_track.output_routing_channel
            del self.master_track.available_output_routing_types
            del self.master_track.available_output_routing_channels
            self.cue_points = (Cue(),)

    class Surface:
        def __init__(self):
            self._song = Song()

        def song(self):
            return self._song

    surface = Surface()
    handler = LomHandler(surface)
    request = _versioned(
        {
            "type": "call",
            "path": "song",
            "name": "sunny_get_target_snapshot",
            "args": [],
        }
    )
    response = handler.handle(request)

    assert response["success"] is True
    snapshot = response["value"]
    assert snapshot["schema_version"] == TARGET_SNAPSHOT_SCHEMA_VERSION
    assert snapshot["target_profile"]["bridge_protocol_version"] == BRIDGE_PROTOCOL_VERSION
    assert sunny_native_module.ABLETON_BRIDGE_PROTOCOL_VERSION == BRIDGE_PROTOCOL_VERSION
    assert (
        sunny_native_module.ABLETON_TARGET_SNAPSHOT_SCHEMA_VERSION == TARGET_SNAPSHOT_SCHEMA_VERSION
    )
    assert snapshot["song"]["scale"] == {
        "root_note": 0,
        "name": "Major",
        "intervals": [0, 2, 4, 5, 7, 9, 11],
        "mode": True,
    }
    assert snapshot["song"]["tuning_system"] == {
        "name": "12-TET",
        "pseudo_octave_in_cents": 1200.0,
        "lowest_note": {"opaque_fixture": "lowest"},
        "highest_note": {"opaque_fixture": "highest"},
        "reference_pitch": {"opaque_fixture": "reference"},
        "note_tunings": {
            "opaque_fixture": [
                0.0,
                100.0,
                200.0,
                300.0,
                400.0,
                500.0,
                600.0,
                700.0,
                800.0,
                900.0,
                1000.0,
                1100.0,
            ]
        },
    }
    assert sunny_native_module.validate_ableton_target_snapshot_json(json.dumps(snapshot))
    assert snapshot["song"]["scene_count"] == 2
    assert {
        name: snapshot["song"][name]
        for name in (
            "is_playing",
            "is_counting_in",
            "arrangement_overdub",
            "overdub",
            "record_mode",
            "session_record",
            "session_automation_record",
            "is_ableton_link_enabled",
            "is_ableton_link_start_stop_sync_enabled",
            "tempo_follower_enabled",
            "nudge_down",
            "nudge_up",
            "back_to_arranger",
            "re_enable_automation_enabled",
            "loop",
            "metronome",
        )
    } == {
        "is_playing": False,
        "is_counting_in": False,
        "arrangement_overdub": False,
        "overdub": False,
        "record_mode": False,
        "session_record": False,
        "session_automation_record": False,
        "is_ableton_link_enabled": False,
        "is_ableton_link_start_stop_sync_enabled": False,
        "tempo_follower_enabled": False,
        "nudge_down": False,
        "nudge_up": False,
        "back_to_arranger": False,
        "re_enable_automation_enabled": False,
        "loop": False,
        "metronome": False,
    }
    assert snapshot["song"]["scenes"] == [
        {
            "name": "Scene 1",
            "is_triggered": False,
            "tempo_enabled": False,
            "tempo": -1.0,
            "time_signature_enabled": False,
            "time_signature_numerator": -1,
            "time_signature_denominator": -1,
        },
        {
            "name": "Scene 2",
            "is_triggered": False,
            "tempo_enabled": True,
            "tempo": 128.0,
            "time_signature_enabled": True,
            "time_signature_numerator": 7,
            "time_signature_denominator": 8,
        },
    ]
    assert snapshot["song"]["tracks"][0] == {
        "name": "Part",
        "devices": [
            {
                "name": "Operator",
                "class_display_name": "Operator",
                "class_name": "Operator",
                "type": 1,
                "is_active": True,
                "can_have_chains": False,
                "latency_in_samples": 128,
                "latency_in_ms": 2.9,
            }
        ],
        "mixer": {
            "volume": {
                "value": 0.5,
                "display_value": 0.0,
                "minimum": 0.0,
                "maximum": 1.0,
                "is_quantized": False,
                "default_value": 0.0,
                "value_items": None,
                "state": 0,
                "automation_state": 0,
                "is_enabled": True,
            },
            "track_activator": {
                "value": 1.0,
                "display_value": 1.0,
                "minimum": 0.0,
                "maximum": 1.0,
                "is_quantized": True,
                "default_value": None,
                "value_items": ["Off", "On"],
                "state": 0,
                "automation_state": 0,
                "is_enabled": True,
            },
            "panning": {
                "value": 0.5,
                "display_value": 0.0,
                "minimum": -1.0,
                "maximum": 1.0,
                "is_quantized": False,
                "default_value": 0.0,
                "value_items": None,
                "state": 0,
                "automation_state": 0,
                "is_enabled": True,
            },
            "sends": [
                {
                    "value": 0.5,
                    "display_value": 0.0,
                    "minimum": 0.0,
                    "maximum": 1.0,
                    "is_quantized": False,
                    "default_value": 0.0,
                    "value_items": None,
                    "state": 0,
                    "automation_state": 0,
                    "is_enabled": True,
                }
            ],
            "crossfade_assign": 1,
            "panning_mode": 0,
        },
        "clip_slot_count": 2,
        "arrangement_clip_count": 0,
        "take_lane_count": 0,
        "clip_slots": [
            {
                "slot": 0,
                "has_clip": True,
                "has_stop_button": False,
                "is_group_slot": False,
                "controls_other_clips": False,
                "is_playing": False,
                "is_recording": False,
                "is_triggered": False,
                "playing_status": 0,
                "will_record_on_start": False,
            },
            {
                "slot": 1,
                "has_clip": False,
                "has_stop_button": True,
                "is_group_slot": False,
                "controls_other_clips": False,
                "is_playing": False,
                "is_recording": False,
                "is_triggered": False,
                "playing_status": 0,
                "will_record_on_start": False,
            },
        ],
        "clips": [
            {
                "slot": 0,
                "name": "Verse",
                "is_audio_clip": False,
                "is_midi_clip": True,
                "is_arrangement_clip": False,
                "is_session_clip": True,
                "is_take_lane_clip": False,
                "length": 4.0,
                "signature_numerator": 4,
                "signature_denominator": 4,
                "start_marker": 0.0,
                "end_marker": 4.0,
                "end_time": 4.0,
                "looping": False,
                "muted": False,
                "has_envelopes": False,
                "is_playing": False,
                "is_recording": False,
                "is_overdubbing": False,
                "is_triggered": False,
                "will_record_on_start": False,
                "launch_mode": 0,
                "launch_quantization": 1,
                "legato": False,
                "velocity_amount": 0.0,
                "has_groove": False,
            }
        ],
        "group_track_index": None,
        "input_routing_type": {"display_name": "All Ins", "identifier": "all_ins"},
        "input_routing_channel": {
            "display_name": "All Channels",
            "identifier": "all_channels",
        },
        "available_input_routing_types": {
            "available_input_routing_types": [
                {"display_name": "No Input", "identifier": "no_input"},
                {"display_name": "All Ins", "identifier": "all_ins"},
            ]
        },
        "available_input_routing_channels": {
            "available_input_routing_channels": [
                {"display_name": "All Channels", "identifier": "all_channels"},
                {"display_name": "Ch. 1", "identifier": "channel_1"},
            ]
        },
        "output_routing_type": {"display_name": "Master", "identifier": "master"},
        "output_routing_channel": {"display_name": "1/2", "identifier": "stereo_1_2"},
        "available_output_routing_types": {
            "available_output_routing_types": [
                {"display_name": "Master", "identifier": "master"},
                {"display_name": "Group 1", "identifier": "group_1"},
            ]
        },
        "available_output_routing_channels": {
            "available_output_routing_channels": [
                {"display_name": "1/2", "identifier": "stereo_1_2"},
                {"display_name": "1", "identifier": "mono_1"},
            ]
        },
        "has_audio_input": False,
        "has_midi_input": True,
        "input_meter_level": 0.0,
        "output_meter_level": 0.0,
        "input_meter_left": 0.0,
        "input_meter_right": 0.0,
        "output_meter_left": 0.0,
        "output_meter_right": 0.0,
        "has_audio_output": True,
        "has_midi_output": False,
        "is_frozen": False,
        "arm": False,
        "implicit_arm": False,
        "back_to_arranger": False,
        "fired_slot_index": -1,
        "playing_slot_index": -1,
        "mute": False,
        "solo": False,
        "muted_via_solo": False,
    }
    assert {
        name: snapshot["song"]["return_tracks"][0][name]
        for name in (
            "name",
            "output_routing_type",
            "output_routing_channel",
            "available_output_routing_types",
            "available_output_routing_channels",
            "mute",
            "solo",
            "muted_via_solo",
        )
    } == {
        "name": "Return A",
        "output_routing_type": {"display_name": "Master", "identifier": "master"},
        "output_routing_channel": {"display_name": "1/2", "identifier": "stereo_1_2"},
        "available_output_routing_types": {
            "available_output_routing_types": [
                {"display_name": "Master", "identifier": "master"},
                {"display_name": "Group 1", "identifier": "group_1"},
            ]
        },
        "available_output_routing_channels": {
            "available_output_routing_channels": [
                {"display_name": "1/2", "identifier": "stereo_1_2"},
                {"display_name": "1", "identifier": "mono_1"},
            ]
        },
        "mute": False,
        "solo": False,
        "muted_via_solo": False,
    }
    assert snapshot["song"]["cue_points"] == [{"name": "Start", "time": 0.0}]

    surface._song.tracks[0].arrangement_clips = [Clip()]
    arrangement_content = handler.handle(request)
    assert arrangement_content["success"] is True
    assert arrangement_content["value"]["song"]["tracks"][0]["arrangement_clip_count"] == 1
    surface._song.tracks[0].arrangement_clips = ()

    surface._song.tracks[0].arrangement_clips = object()
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid arrangement_clips collection" in malformed["error"]
    surface._song.tracks[0].arrangement_clips = ()

    surface._song.tracks[0].take_lanes = [object()]
    take_lanes = handler.handle(request)
    assert take_lanes["success"] is True
    assert take_lanes["value"]["song"]["tracks"][0]["take_lane_count"] == 1
    surface._song.tracks[0].take_lanes = ()

    surface._song.tracks[0].take_lanes = object()
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid take_lanes collection" in malformed["error"]
    surface._song.tracks[0].take_lanes = ()

    surface._song.tracks[0].is_frozen = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid is_frozen" in malformed["error"]
    surface._song.tracks[0].is_frozen = False

    for property_name in (
        "is_playing",
        "is_counting_in",
        "arrangement_overdub",
        "overdub",
        "record_mode",
        "session_record",
        "session_automation_record",
        "is_ableton_link_enabled",
        "is_ableton_link_start_stop_sync_enabled",
        "tempo_follower_enabled",
        "nudge_down",
        "nudge_up",
        "back_to_arranger",
        "re_enable_automation_enabled",
        "loop",
        "metronome",
    ):
        setattr(surface._song, property_name, 0)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(surface._song, property_name, False)

    for property_name in ("is_triggered", "tempo_enabled", "time_signature_enabled"):
        scene = surface._song.scenes[0]
        original = getattr(scene, property_name)
        setattr(scene, property_name, int(original))
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(scene, property_name, original)

    slot = surface._song.tracks[0].clip_slots[0]
    for property_name in (
        "has_clip",
        "has_stop_button",
        "is_group_slot",
        "controls_other_clips",
        "is_playing",
        "is_recording",
        "is_triggered",
        "will_record_on_start",
    ):
        original = getattr(slot, property_name)
        setattr(slot, property_name, int(original))
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(slot, property_name, original)

    slot.playing_status = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid playing_status" in malformed["error"]
    slot.playing_status = 0

    slot.playing_status = 1
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "incoherent is_playing" in malformed["error"]
    slot.playing_status = 0

    surface._song.tracks[0].arm = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid arm" in malformed["error"]
    surface._song.tracks[0].arm = False

    surface._song.tracks[0].implicit_arm = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid implicit_arm" in malformed["error"]
    surface._song.tracks[0].implicit_arm = False

    surface._song.tracks[0].back_to_arranger = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid back_to_arranger" in malformed["error"]
    surface._song.tracks[0].back_to_arranger = False

    for property_name, malformed_value in (
        ("fired_slot_index", True),
        ("fired_slot_index", -3),
        ("playing_slot_index", 2),
    ):
        setattr(surface._song.tracks[0], property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(surface._song.tracks[0], property_name, -1)

    surface._song.tracks[0].clip_slots[0].has_clip = 1
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid has_clip" in malformed["error"]
    surface._song.tracks[0].clip_slots[0].has_clip = True

    for property_name in (
        "has_audio_input",
        "has_midi_input",
        "has_audio_output",
        "has_midi_output",
    ):
        original = getattr(surface._song.tracks[0], property_name)
        setattr(surface._song.tracks[0], property_name, int(original))
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(surface._song.tracks[0], property_name, original)

    for property_name, malformed_value in (
        ("input_meter_level", 0),
        ("input_meter_level", -0.1),
        ("output_meter_level", 1.1),
        ("output_meter_level", math.inf),
        ("input_meter_left", 0),
        ("input_meter_right", -0.1),
        ("output_meter_left", 1.1),
        ("output_meter_right", math.inf),
    ):
        original = getattr(surface._song.tracks[0], property_name)
        setattr(surface._song.tracks[0], property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(surface._song.tracks[0], property_name, original)

    track = surface._song.tracks[0]
    track.has_midi_input = False
    no_input = handler.handle(request)
    assert no_input["success"] is True
    assert {
        property_name: no_input["value"]["song"]["tracks"][0][property_name]
        for property_name in (
            "input_routing_type",
            "input_routing_channel",
            "available_input_routing_types",
            "available_input_routing_channels",
            "input_meter_level",
            "output_meter_level",
        )
    } == {
        "input_routing_type": None,
        "input_routing_channel": None,
        "available_input_routing_types": None,
        "available_input_routing_channels": None,
        "input_meter_level": None,
        "output_meter_level": None,
    }
    track.has_midi_input = True

    track.has_audio_output = False
    no_audio_output = handler.handle(request)
    assert no_audio_output["success"] is True
    assert {
        property_name: no_audio_output["value"]["song"]["tracks"][0][property_name]
        for property_name in (
            "input_meter_left",
            "input_meter_right",
            "output_meter_left",
            "output_meter_right",
        )
    } == {
        "input_meter_left": None,
        "input_meter_right": None,
        "output_meter_left": None,
        "output_meter_right": None,
    }
    track.has_audio_output = True

    for property_name, malformed_value in (
        ("input_routing_type", {"display_name": "All Ins"}),
        ("input_routing_channel", {"display_name": "All Channels", "identifier": 1}),
    ):
        original = getattr(track, property_name)
        setattr(track, property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name} dictionary" in malformed["error"]
        setattr(track, property_name, original)

    for property_name, malformed_value, expected_error in (
        (
            "available_input_routing_types",
            {"available_input_routing_types": "All Ins"},
            "invalid available_input_routing_types list",
        ),
        (
            "available_input_routing_channels",
            {
                "available_input_routing_channels": [
                    {"display_name": "All Channels", "identifier": "all_channels", "extra": 1}
                ]
            },
            "invalid available_input_routing_channels dictionary",
        ),
    ):
        original = getattr(track, property_name)
        setattr(track, property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert expected_error in malformed["error"]
        setattr(track, property_name, original)

    original_selected_input = track.input_routing_type
    track.input_routing_type = {"display_name": "Ext. In", "identifier": "external"}
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "not in available_input_routing_types" in malformed["error"]
    track.input_routing_type = original_selected_input

    for property_name, malformed_value in (
        ("output_routing_type", {"display_name": "Master"}),
        (
            "output_routing_type",
            {"display_name": "Master", "identifier": "master", "unexpected": True},
        ),
        ("output_routing_channel", {"display_name": "1/2", "identifier": 1}),
        ("output_routing_channel", ("1/2", "stereo_1_2")),
    ):
        original = getattr(track, property_name)
        setattr(track, property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name} dictionary" in malformed["error"]
        setattr(track, property_name, original)

    return_track = surface._song.return_tracks[0]
    original_return_route = return_track.output_routing_type
    return_track.output_routing_type = {"display_name": "Master", "identifier": False}
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid output_routing_type dictionary" in malformed["error"]
    return_track.output_routing_type = original_return_route

    for property_name, malformed_value, expected_error in (
        ("available_output_routing_types", [], "invalid available_output_routing_types dictionary"),
        (
            "available_output_routing_types",
            {"available_output_routing_types": "Master"},
            "invalid available_output_routing_types list",
        ),
        (
            "available_output_routing_channels",
            {
                "available_output_routing_channels": [
                    {"display_name": "1/2", "identifier": "stereo_1_2", "unexpected": True}
                ]
            },
            "invalid available_output_routing_channels dictionary",
        ),
    ):
        original = getattr(track, property_name)
        setattr(track, property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert expected_error in malformed["error"]
        setattr(track, property_name, original)

    original_selected_type = track.output_routing_type
    track.output_routing_type = {"display_name": "External", "identifier": "external"}
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "not in available_output_routing_types" in malformed["error"]
    track.output_routing_type = original_selected_type

    for property_name in ("mute", "solo", "muted_via_solo"):
        setattr(surface._song.return_tracks[0], property_name, 0)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(surface._song.return_tracks[0], property_name, False)

    surface._song.tracks[0].mixer_device.crossfade_assign = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid crossfade_assign" in malformed["error"]
    surface._song.tracks[0].mixer_device.crossfade_assign = 3
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid crossfade_assign" in malformed["error"]
    surface._song.tracks[0].mixer_device.crossfade_assign = 1

    surface._song.tracks[0].mixer_device.panning_mode = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid panning_mode" in malformed["error"]
    surface._song.tracks[0].mixer_device.panning_mode = 2
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid panning_mode" in malformed["error"]
    surface._song.tracks[0].mixer_device.panning_mode = 0

    Parameter.value = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid value" in malformed["error"]
    Parameter.value = 0.5

    Parameter.display_value = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid display_value" in malformed["error"]
    Parameter.display_value = 0.0

    Parameter.min = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid minimum" in malformed["error"]
    Parameter.min = 0.0

    Parameter.max = float("inf")
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid maximum" in malformed["error"]
    Parameter.max = 1.0

    Parameter.min = 2.0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid maximum" in malformed["error"]
    Parameter.min = 0.0

    Parameter.value = 2.0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "outside its reported range" in malformed["error"]
    Parameter.value = 0.5

    Parameter.is_quantized = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid is_quantized" in malformed["error"]
    Parameter.is_quantized = False

    Parameter.default_value = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "default_value" in malformed["error"]
    Parameter.default_value = 2.0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "outside its reported range" in malformed["error"]
    Parameter.default_value = 0.0

    ActivatorParameter.is_quantized = 1
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid is_quantized" in malformed["error"]
    ActivatorParameter.is_quantized = True

    ActivatorParameter.value_items = ["Off", 1]
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "value_items entry" in malformed["error"]
    ActivatorParameter.value_items = ("Off", "On")

    Clip.has_groove = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid has_groove" in malformed["error"]
    Clip.has_groove = False

    Clip.has_envelopes = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid has_envelopes" in malformed["error"]
    Clip.has_envelopes = False

    for property_name in (
        "is_audio_clip",
        "is_midi_clip",
        "is_arrangement_clip",
        "looping",
        "muted",
    ):
        original = getattr(Clip, property_name)
        setattr(Clip, property_name, int(original))
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(Clip, property_name, original)

    Clip.is_audio_clip = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "incoherent audio/MIDI identity" in malformed["error"]
    Clip.is_audio_clip = False

    Clip.is_arrangement_clip = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "Arrangement Clip" in malformed["error"]
    Clip.is_arrangement_clip = False

    for property_name in ("is_session_clip", "is_take_lane_clip"):
        original = getattr(Clip, property_name)
        setattr(Clip, property_name, int(original))
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert "invalid Live 11+ location identity" in malformed["error"]
        setattr(Clip, property_name, original)

    Clip.is_session_clip = False
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "non-Session or Take Lane Clip" in malformed["error"]
    Clip.is_session_clip = True

    Clip.is_take_lane_clip = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "non-Session or Take Lane Clip" in malformed["error"]
    Clip.is_take_lane_clip = False

    Clip.end_time = 4
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid Clip end_time" in malformed["error"]
    Clip.end_time = 4.0

    Clip.end_time = 3.0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "incoherent unlooped end_time" in malformed["error"]
    Clip.end_time = 4.0

    for property_name in (
        "is_playing",
        "is_recording",
        "is_overdubbing",
        "is_triggered",
        "will_record_on_start",
    ):
        setattr(Clip, property_name, 0)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert f"invalid {property_name}" in malformed["error"]
        setattr(Clip, property_name, False)

    Clip.launch_mode = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid launch_mode" in malformed["error"]
    Clip.launch_mode = 0

    Clip.launch_quantization = 15
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid launch_quantization" in malformed["error"]
    Clip.launch_quantization = 1

    Clip.legato = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid legato" in malformed["error"]
    Clip.legato = False

    Clip.velocity_amount = 0
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid velocity_amount" in malformed["error"]
    Clip.velocity_amount = 0.0

    Device.latency_in_samples = True
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid latency_in_samples" in malformed["error"]
    Device.latency_in_samples = 128

    Device.latency_in_ms = 3
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid latency_in_ms" in malformed["error"]

    Device.latency_in_ms = -0.1
    malformed = handler.handle(request)
    assert malformed["success"] is False
    assert "invalid latency_in_ms" in malformed["error"]
    Device.latency_in_ms = 2.9

    for owner, property_name, malformed_value, expected_error in (
        (surface._song, "tempo", 120, "Song tempo"),
        (surface._song, "signature_numerator", 4.0, "Song signature_numerator"),
        (surface._song.scenes[0], "name", object(), "Scene name"),
        (surface._song.cue_points[0], "time", 0, "CuePoint time"),
        (Clip, "name", object(), "Clip name"),
        (Clip, "length", 4, "Clip length"),
        (Device, "name", object(), "Device name"),
    ):
        original = getattr(owner, property_name)
        setattr(owner, property_name, malformed_value)
        malformed = handler.handle(request)
        assert malformed["success"] is False
        assert expected_error in malformed["error"]
        setattr(owner, property_name, original)

    request["args"] = ["unexpected"]
    response = handler.handle(request)
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_target_snapshot_pitch_context_is_version_coupled(monkeypatch):
    """Unsupported older Live versions emit explicit null context instead of probing it."""

    class Application:
        get_major_version = staticmethod(lambda: 11)
        get_minor_version = staticmethod(lambda: 3)
        get_bugfix_version = staticmethod(lambda: 0)
        get_version_string = staticmethod(lambda: "11.3.0")

    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(Application=SimpleNamespace(get_application=lambda: Application())),
    )

    class Parameter:
        value = 0.5
        display_value = 0.0
        min = 0.0
        max = 1.0
        is_quantized = False
        default_value = 0.0
        state = 0
        automation_state = 0
        is_enabled = True

    class ActivatorParameter(Parameter):
        value = 1.0
        display_value = 1.0
        is_quantized = True
        value_items = ("Off", "On")

    class PanParameter(Parameter):
        min = -1.0

    class Mixer:
        volume = Parameter()
        track_activator = ActivatorParameter()
        panning = PanParameter()
        sends = ()
        panning_mode = 0

    class Master:
        name = "Master"
        devices = ()
        mixer_device = Mixer()

    class Song:
        tempo = 120.0
        signature_numerator = 4
        signature_denominator = 4
        is_playing = False
        is_counting_in = False
        arrangement_overdub = False
        overdub = False
        record_mode = False
        session_record = False
        session_automation_record = False
        is_ableton_link_enabled = False
        is_ableton_link_start_stop_sync_enabled = False
        tempo_follower_enabled = False
        nudge_down = False
        nudge_up = False
        back_to_arranger = False
        re_enable_automation_enabled = False
        loop = False
        metronome = False
        scenes = ()
        tracks = ()
        return_tracks = ()
        master_track = Master()
        cue_points = ()

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song",
                "name": "sunny_get_target_snapshot",
                "args": [],
            }
        )
    )

    assert response["success"] is True
    assert response["value"]["song"]["scale"] is None
    assert response["value"]["song"]["tuning_system"] is None


@pytest.mark.parametrize(
    ("scale_intervals", "pseudo_octave", "tuning_overrides"),
    [
        ([0, True], 1200.0, {}),
        ([0, 2, 4], float("inf"), {}),
        ([0, 2, 4], 1200, {}),
        ([0, 2, 4], 1200.0, {"name": 12}),
        ([0, 2, 4], 1200.0, {"lowest_note": []}),
        ([0, 2, 4], 1200.0, {"highest_note": {"nested": float("nan")}}),
        ([0, 2, 4], 1200.0, {"note_tunings": {"first": [0.0], "second": [100.0]}}),
        ([0, 2, 4], 1200.0, {"note_tunings": {"only": [0.0, "100"]}}),
    ],
)
def test_target_snapshot_rejects_malformed_host_pitch_context(
    monkeypatch, scale_intervals, pseudo_octave, tuning_overrides
):
    """Private host wrappers cannot smuggle malformed scale/tuning facts into evidence."""

    class Application:
        get_major_version = staticmethod(lambda: 12)
        get_minor_version = staticmethod(lambda: 3)
        get_bugfix_version = staticmethod(lambda: 5)
        get_version_string = staticmethod(lambda: "12.3.5")

    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(Application=SimpleNamespace(get_application=lambda: Application())),
    )

    class Parameter:
        value = 0.5
        display_value = 0.0
        min = 0.0
        max = 1.0
        is_quantized = False
        default_value = 0.0
        state = 0
        automation_state = 0
        is_enabled = True

    class ActivatorParameter(Parameter):
        value = 1.0
        display_value = 1.0
        is_quantized = True
        value_items = ("Off", "On")

    class PanParameter(Parameter):
        min = -1.0

    class Mixer:
        volume = Parameter()
        track_activator = ActivatorParameter()
        panning = PanParameter()
        sends = ()
        panning_mode = 0

    class Master:
        name = "Master"
        devices = ()
        mixer_device = Mixer()

    class Tuning:
        name = "Malformed"
        pseudo_octave_in_cents = pseudo_octave
        lowest_note = {"opaque_fixture": "lowest"}
        highest_note = {"opaque_fixture": "highest"}
        reference_pitch = {"opaque_fixture": "reference"}
        note_tunings = {"opaque_fixture": [0.0, 100.0]}

    for property_name, value in tuning_overrides.items():
        setattr(Tuning, property_name, value)

    class Song:
        tempo = 120.0
        signature_numerator = 4
        signature_denominator = 4
        is_playing = False
        is_counting_in = False
        arrangement_overdub = False
        overdub = False
        record_mode = False
        session_record = False
        session_automation_record = False
        is_ableton_link_enabled = False
        is_ableton_link_start_stop_sync_enabled = False
        tempo_follower_enabled = False
        nudge_down = False
        nudge_up = False
        back_to_arranger = False
        re_enable_automation_enabled = False
        loop = False
        metronome = False
        root_note = 0
        scale_name = "Major"
        scale_mode = True
        tuning_system = Tuning()
        scenes = ()
        tracks = ()
        return_tracks = ()
        master_track = Master()
        cue_points = ()

        def __init__(self):
            self.scale_intervals = scale_intervals

    handler = LomHandler(SimpleNamespace(song=lambda: Song()))
    response = handler.handle(
        _versioned(
            {
                "type": "call",
                "path": "song",
                "name": "sunny_get_target_snapshot",
                "args": [],
            }
        )
    )

    assert response["success"] is False
    assert "invalid" in response["error"].lower()


def test_control_surface_schedules_lom_work():
    """TCP worker requests are marshalled through schedule_message."""
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface._handler = type(
        "Handler",
        (),
        {"handle": staticmethod(lambda request: {"success": True, "value": request})},
    )()
    scheduled = []

    def schedule(delay, callback):
        scheduled.append(delay)
        callback()

    surface.schedule_message = schedule
    request = {"type": "get", "path": "song", "name": "tempo"}

    assert surface._process_request(request) == {"success": True, "value": request}
    assert scheduled == [0]


def test_control_surface_cancels_a_queued_request_before_returning_timeout(monkeypatch):
    """A scheduling timeout cannot leave a later unjournalled Live mutation."""
    monkeypatch.setattr(surface_module, "LOM_REQUEST_TIMEOUT_SECONDS", 0.01)
    calls = []
    callbacks = []
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface._handler = type(
        "Handler",
        (),
        {"handle": staticmethod(lambda request: calls.append(request) or {"success": True})},
    )()
    surface.schedule_message = lambda delay, callback: callbacks.append(callback)

    response = surface._process_request(
        _versioned({"type": "set", "path": "song", "name": "tempo", "args": [121.0]})
    )

    assert response["success"] is False
    assert "request cancelled" in response["error"]
    assert calls == []
    assert len(callbacks) == 1

    # Live may still invoke the already-queued callback. It must be inert.
    callbacks[0]()
    assert calls == []


def test_control_surface_waits_for_an_already_started_lom_outcome(monkeypatch):
    """Once mutation begins, the wire result remains definite rather than timing out."""
    monkeypatch.setattr(surface_module, "LOM_REQUEST_TIMEOUT_SECONDS", 0.01)
    begun = threading.Event()
    release = threading.Event()
    workers = []
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()

    def handle(request: dict) -> dict:
        begun.set()
        assert release.wait(1.0)
        return {"success": True, "value": request["name"]}

    def schedule(delay, callback):
        worker = threading.Thread(target=callback)
        workers.append(worker)
        worker.start()
        assert begun.wait(1.0)

    surface._handler = type("Handler", (), {"handle": staticmethod(handle)})()
    surface.schedule_message = schedule
    release_timer = threading.Timer(0.05, release.set)
    release_timer.start()

    response = surface._process_request(
        _versioned({"type": "set", "path": "song", "name": "tempo", "args": [121.0]})
    )

    release_timer.join()
    for worker in workers:
        worker.join()
    assert response == {"success": True, "value": "tempo"}


def test_control_surface_disconnect_cancels_queued_and_declines_new_requests():
    """Unloading the surface makes pending callbacks inert immediately."""
    calls = []
    callbacks = []
    scheduled = threading.Event()
    results = []
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface._server = SimpleNamespace(shutdown=lambda: None)
    surface._handler = SimpleNamespace(
        handle=lambda request: calls.append(request) or {"success": True}
    )

    def schedule(delay, callback):
        callbacks.append(callback)
        scheduled.set()

    surface.schedule_message = schedule
    request = _versioned({"type": "set", "path": "song", "name": "tempo", "args": [121.0]})
    worker = threading.Thread(target=lambda: results.append(surface._process_request(request)))
    worker.start()
    assert scheduled.wait(1)
    surface.disconnect()
    callbacks[0]()
    worker.join(timeout=1)
    assert not worker.is_alive()
    assert calls == []
    assert results[0]["success"] is False
    assert "disconnect" in results[0]["error"].lower()
    assert surface._process_request(request)["success"] is False
    assert len(callbacks) == 1


def test_control_surface_disconnect_preserves_started_request_outcome():
    """Disconnect cancels queued work but never fabricates a started call's result."""
    begun = threading.Event()
    release = threading.Event()
    results = []
    callbacks = []
    surface = SunnyControlSurface.__new__(SunnyControlSurface)
    surface._initialise_request_lifecycle()
    surface._server = SimpleNamespace(shutdown=lambda: None)

    def handle(request):
        begun.set()
        assert release.wait(2)
        return {"success": True, "value": "finished"}

    def schedule(delay, callback):
        worker = threading.Thread(target=callback)
        callbacks.append(worker)
        worker.start()

    surface._handler = SimpleNamespace(handle=handle)
    surface.schedule_message = schedule
    worker = threading.Thread(target=lambda: results.append(surface._process_request({})))
    worker.start()
    try:
        assert begun.wait(1)
        surface.disconnect()
        assert results == []
    finally:
        release.set()
        worker.join(timeout=1)
        for callback in callbacks:
            callback.join(timeout=1)
    assert not worker.is_alive()
    assert results == [{"success": True, "value": "finished"}]


def test_remote_script_defaults_to_loopback(monkeypatch):
    """The Live bridge is not network-exposed unless explicitly configured."""
    monkeypatch.delenv("SUNNY_BIND_HOST", raising=False)
    monkeypatch.delenv("SUNNY_TCP_PORT", raising=False)

    assert _server_configuration() == (DEFAULT_BIND_HOST, DEFAULT_PORT)


def test_remote_script_rejects_invalid_port(monkeypatch):
    """Malformed environment configuration falls back to the safe default."""
    monkeypatch.setenv("SUNNY_TCP_PORT", "70000")

    assert _server_configuration() == (DEFAULT_BIND_HOST, DEFAULT_PORT)
