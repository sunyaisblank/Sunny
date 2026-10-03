"""Tests for Sunny's Remote Script: TCP framing, the control surface, and the LOM handler.

The server tests pin the wire protocol used by the C++ ``TcpTransport``:
4-byte big-endian length prefix followed by UTF-8 JSON, request/response
over a single connection. The handler tests run against ``live_model``, the
shared offline model of Live's Python API, so that no Ableton instance is
required and no test can accept a host shape Live itself would reject.
"""

from __future__ import annotations

import json
import math
import socket
import struct
import threading
import time
from types import SimpleNamespace

import pytest
from live_model import (
    ArgumentError,
    AutomationState,
    ClipSlotPlayingStatus,
    CrossfadeAssignment,
    DeviceParameter,
    Groove,
    IntVector,
    LaunchMode,
    LiveSet,
    PanningMode,
    ParameterState,
    Quantization,
    RoutingChannel,
    RoutingType,
    RoutingTypeCategory,
    StringVector,
    Vector,
    inject,
    native_device,
    restore,
)
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


NOTE_FIELDS = [
    "note_id",
    "pitch",
    "start_time",
    "duration",
    "velocity",
    "mute",
    "probability",
    "velocity_deviation",
    "release_velocity",
]


@pytest.fixture
def live(monkeypatch):
    """A default Live 12.3.5 Set whose ``Live`` module is importable by the handler."""
    return LiveSet().install(monkeypatch)


def _request(handler, request_type, path, name, *args):
    return handler.handle(
        _versioned({"type": request_type, "path": path, "name": name, "args": list(args)})
    )


def _call(handler, path, name, *args):
    return _request(handler, "call", path, name, *args)


def _set(handler, path, name, value):
    return _request(handler, "set", path, name, value)


def _wire_note(pitch, start_time, duration, velocity=100, release_velocity=64.0):
    return {
        "pitch": pitch,
        "start_time": start_time,
        "duration": duration,
        "velocity": velocity,
        "mute": False,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": release_velocity,
    }


def _type_route(display_name, category):
    return {"display_name": display_name, "identifier": f"{int(category)}:{display_name}"}


def _channel_route(display_name):
    return {"display_name": display_name, "identifier": display_name}


def _midi_track_with_clip(live, length=4.0, instrument=None):
    track = live.song.create_midi_track(-1)
    if instrument is not None:
        track.insert_device(instrument)
    track.clip_slots[0].create_clip(length)
    return track, track.clip_slots[0].clip


def test_live_model_rejects_max_dictionary_shapes():
    """The model enforces Live's Python types, so handler regressions to Max shapes fail."""
    live = LiveSet()
    track, clip = _midi_track_with_clip(live, instrument="Operator")

    with pytest.raises(ArgumentError, match="expected MidiNoteSpecification, got str 'notes'"):
        clip.add_new_notes({"notes": [_wire_note(60, 0.0, 1.0)]})
    with pytest.raises(ArgumentError):
        clip.add_new_notes([_wire_note(60, 0.0, 1.0)])
    with pytest.raises(ArgumentError):
        clip.get_notes_by_id({"note_ids": [1], "return": NOTE_FIELDS})
    with pytest.raises(TypeError):
        clip.get_all_notes_extended({"return": NOTE_FIELDS})
    with pytest.raises(ArgumentError):
        track.output_routing_type = {"display_name": "Master", "identifier": "master"}
    with pytest.raises(RuntimeError, match="not empty"):
        track.clip_slots[0].create_clip(4.0)

    for value in (
        clip.launch_mode,
        clip.launch_quantization,
        track.mixer_device.crossfade_assign,
        track.mixer_device.panning_mode,
        track.clip_slots[0].playing_status,
        track.devices[0].type,
        track.devices[0].parameters[0].state,
        track.devices[0].parameters[0].automation_state,
    ):
        assert isinstance(value, int) and type(value) is not int
    assert type(track.output_routing_type) is RoutingType
    assert track.output_routing_type is not track.output_routing_type
    assert track.output_routing_type == track.output_routing_type
    assert type(track.devices[0].parameters[0].value_items).__name__ == "StringVector"


def test_add_new_notes_passes_midi_note_specifications_and_reads_back_midi_notes(live):
    """The wire note dictionary becomes MidiNoteSpecification objects; MidiNotes become dicts."""
    _, clip = _midi_track_with_clip(live)
    handler = LomHandler(live.surface)
    clip_path = "song/tracks/0/clip_slots/0/clip"
    notes = [
        _wire_note(60, 0.0, 1.0),
        _wire_note(64, 1.0 / 3.0, 2.0 / 3.0, velocity=90, release_velocity=23.0),
    ]

    response = _call(handler, clip_path, "add_new_notes", {"notes": notes})
    assert response == {"success": True, "value": [1, 2]}
    assert all(type(note_id) is int for note_id in response["value"])
    assert [
        (note.pitch, note.start_time, note.duration, note.velocity, note.release_velocity)
        for note in clip.get_all_notes_extended()
    ] == [(60, 0.0, 1.0, 100.0, 64.0), (64, 1.0 / 3.0, 2.0 / 3.0, 90.0, 23.0)]

    expected = [
        {
            "note_id": 1,
            "pitch": 60,
            "start_time": 0.0,
            "duration": 1.0,
            "velocity": 100.0,
            "mute": False,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 64.0,
        },
        {
            "note_id": 2,
            "pitch": 64,
            "start_time": 1.0 / 3.0,
            "duration": 2.0 / 3.0,
            "velocity": 90.0,
            "mute": False,
            "probability": 1.0,
            "velocity_deviation": 0.0,
            "release_velocity": 23.0,
        },
    ]
    by_id = _call(
        handler, clip_path, "get_notes_by_id", {"note_ids": [2, 1], "return": NOTE_FIELDS}
    )
    assert by_id == {"success": True, "value": {"notes": [expected[1], expected[0]]}}
    all_notes = _call(handler, clip_path, "get_all_notes_extended", {"return": NOTE_FIELDS})
    assert all_notes == {"success": True, "value": {"notes": expected}}
    assert type(all_notes["value"]["notes"][0]["mute"]) is bool


def test_note_readback_rejects_malformed_host_notes(live, monkeypatch):
    """Converted MidiNote fields keep their categories; host drift fails closed."""
    _, clip = _midi_track_with_clip(live)
    handler = LomHandler(live.surface)
    clip_path = "song/tracks/0/clip_slots/0/clip"
    assert _call(handler, clip_path, "add_new_notes", {"notes": [_wire_note(60, 0.0, 1.0)]})[
        "success"
    ]
    note = clip.get_all_notes_extended()[0]

    for field, malformed in (
        ("mute", 0),
        ("pitch", 60.0),
        ("start_time", float("nan")),
        ("note_id", True),
    ):
        original = getattr(note, field)
        setattr(note, field, malformed)
        response = _call(handler, clip_path, "get_all_notes_extended", {"return": NOTE_FIELDS})
        assert response["success"] is False, field
        assert "MidiNote" in response["error"]
        setattr(note, field, original)

    monkeypatch.setattr(clip, "add_new_notes", lambda specifications: None)
    response = _call(handler, clip_path, "add_new_notes", {"notes": [_wire_note(62, 1.0, 1.0)]})
    assert response["success"] is False
    assert "note IDs" in response["error"]


def test_handler_enforces_get_set_call_algebra_without_silent_noops(live):
    """Malformed or type-confused requests fail instead of reporting false success."""
    handler = LomHandler(live.surface)

    missing = handler.handle(_versioned({"type": "get", "path": "song", "name": "not_a_property"}))
    assert missing["success"] is False
    assert "outside Sunny bridge protocol" in missing["error"]

    confused = handler.handle(_versioned({"type": "get", "path": "song", "name": "stop_playing"}))
    assert confused["success"] is False
    assert "outside Sunny bridge protocol" in confused["error"]

    no_value = handler.handle(_versioned({"type": "set", "path": "song", "name": "tempo"}))
    assert no_value["success"] is False
    assert "outside Sunny bridge protocol" in no_value["error"]

    assert _set(handler, "song", "tempo", 137.5) == {
        "success": True,
        "value": {"property": "tempo", "requested": 137.5, "observed": 137.5},
    }
    assert live.song.tempo == 137.5

    live.song.scenes[0].enable_launch_overrides(128.0, 7, 8)
    assert _set(handler, "song/scenes/0", "time_signature_enabled", False) == {
        "success": True,
        "value": {"property": "time_signature_enabled", "requested": False, "observed": False},
    }
    assert live.song.scenes[0].time_signature_enabled is False

    wrong_args = handler.handle(
        _versioned({"type": "call", "path": "song", "name": "stop_playing", "args": {}})
    )
    assert wrong_args == {"success": False, "error": "Request args must be an array"}


def test_structural_calls_return_no_private_host_object(live):
    """Live returns the created Track or Scene; the adapter reports success, not the object."""
    handler = LomHandler(live.surface)

    assert _call(handler, "song", "create_midi_track", -1) == {"success": True, "value": None}
    assert _call(handler, "song", "create_midi_track", 0) == {"success": True, "value": None}
    assert _call(handler, "song", "create_return_track") == {"success": True, "value": None}
    assert _call(handler, "song", "create_scene", 0) == {"success": True, "value": None}
    assert [track.name for track in live.song.tracks] == ["1-MIDI", "1-MIDI"]
    assert len(live.song.scenes) == 2
    assert all(len(track.clip_slots) == 2 for track in live.song.tracks)
    assert all(len(track.mixer_device.sends) == 1 for track in live.song.tracks)

    assert _call(handler, "song/tracks/0/clip_slots/0", "create_clip", 4.0) == {
        "success": True,
        "value": None,
    }
    occupied = _call(handler, "song/tracks/0/clip_slots/0", "create_clip", 4.0)
    assert occupied["success"] is False
    assert "not empty" in occupied["error"]
    assert _call(handler, "song/tracks/0/clip_slots/0", "delete_clip") == {
        "success": True,
        "value": None,
    }
    assert live.song.tracks[0].clip_slots[0].has_clip is False


def test_mixer_enum_assignment_reads_back_as_plain_integers(live):
    """Enum-valued mixer properties are int subclasses; evidence carries plain ints."""
    track = live.song.create_midi_track(-1)
    track.mixer_device.crossfade_assign = 0
    track.mixer_device.panning_mode = 1
    track.arm = True
    track.implicit_arm = True
    handler = LomHandler(live.surface)

    for path, property_name, value in (
        ("song/tracks/0/mixer_device", "crossfade_assign", 1),
        ("song/tracks/0/mixer_device", "panning_mode", 0),
        ("song/tracks/0", "arm", False),
        ("song/tracks/0", "implicit_arm", False),
    ):
        response = _set(handler, path, property_name, value)
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }
        assert type(response["value"]["observed"]) is type(value)
    assert track.mixer_device.crossfade_assign == CrossfadeAssignment.NONE
    assert track.mixer_device.panning_mode == PanningMode.stereo
    assert track.arm is False
    assert track.implicit_arm is False


def test_generated_return_gate_uses_exact_boolean_and_stereo_pan_readback(live):
    """Aux returns clear independent suppressors and apply the modeled scalar pan."""
    returned = live.song.create_return_track()
    returned.mute = True
    returned.solo = True
    returned.mixer_device.crossfade_assign = 0
    returned.mixer_device.panning_mode = 1
    returned.mixer_device.track_activator.value = 0.0
    returned.mixer_device.panning.value = -0.5
    handler = LomHandler(live.surface)

    for path, property_name, value in (
        ("song/return_tracks/0", "mute", False),
        ("song/return_tracks/0", "solo", False),
        ("song/return_tracks/0/mixer_device", "crossfade_assign", 1),
        ("song/return_tracks/0/mixer_device", "panning_mode", 0),
        ("song/return_tracks/0/mixer_device/track_activator", "value", 1.0),
        ("song/return_tracks/0/mixer_device/panning", "value", 0.25),
    ):
        assert _set(handler, path, property_name, value) == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }

    assert returned.mute is False and returned.solo is False
    assert returned.mixer_device.crossfade_assign == 1
    assert returned.mixer_device.panning_mode == 0
    assert returned.mixer_device.track_activator.value == 1.0
    assert returned.mixer_device.panning.value == 0.25


def test_generated_main_gate_uses_exact_activator_and_centered_stereo_pan_readback(live):
    """The project-wide Main stage cannot inherit a muted or panned target state."""
    mixer = live.song.master_track.mixer_device
    mixer.track_activator.value = 0.0
    mixer.panning_mode = 1
    mixer.panning.value = -0.5
    handler = LomHandler(live.surface)

    for path, property_name, value in (
        ("song/master_track/mixer_device/track_activator", "value", 1.0),
        ("song/master_track/mixer_device", "panning_mode", 0),
        ("song/master_track/mixer_device/panning", "value", 0.0),
    ):
        assert _set(handler, path, property_name, value) == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }
    assert mixer.track_activator.value == 1.0
    assert mixer.panning_mode == 0
    assert mixer.panning.value == 0.0


def test_mixer_display_value_writes_fader_and_send_levels(live):
    """Faders and sends are written in dB through display_value and read back exactly."""
    live.song.create_return_track()
    track = live.song.create_midi_track(-1)
    handler = LomHandler(live.surface)

    assert _set(handler, "song/tracks/0/mixer_device/volume", "display_value", -6.0) == {
        "success": True,
        "value": {"property": "display_value", "requested": -6.0, "observed": -6.0},
    }
    assert _set(handler, "song/tracks/0/mixer_device/sends/0", "display_value", -12.0) == {
        "success": True,
        "value": {"property": "display_value", "requested": -12.0, "observed": -12.0},
    }
    assert 0.0 < track.mixer_device.volume.value < 0.85
    assert 0.0 < track.mixer_device.sends[0].value < 1.0


def test_clip_groove_clear_uses_null_object_assignment_and_exact_readback(live):
    """A Sunny clip cannot inherit a non-destructive playback groove."""
    _, clip = _midi_track_with_clip(live)
    clip.groove = Groove()
    handler = LomHandler(live.surface)

    assert _set(handler, "song/tracks/0/clip_slots/0/clip", "groove", None) == {
        "success": True,
        "value": {"property": "groove", "requested": None, "observed": None},
    }
    assert clip.groove is None
    assert clip.has_groove is False


def test_clip_launch_tuple_is_assigned_with_exact_scalar_readback(live):
    """The wrapper retains the four bounded Live-11+ launch scalar categories."""
    _, clip = _midi_track_with_clip(live)
    clip.launch_mode = 2
    clip.launch_quantization = 0
    clip.legato = True
    clip.velocity_amount = 1.0
    handler = LomHandler(live.surface)

    for property_name, value in (
        ("launch_mode", 0),
        ("launch_quantization", 1),
        ("legato", False),
        ("velocity_amount", 0.0),
    ):
        response = _set(handler, "song/tracks/0/clip_slots/0/clip", property_name, value)
        assert response == {
            "success": True,
            "value": {"property": property_name, "requested": value, "observed": value},
        }
        assert type(response["value"]["observed"]) is type(value)
        assert getattr(clip, property_name) == value
    assert clip.launch_mode == LaunchMode.trigger
    assert clip.launch_quantization == Quantization.q_no_q


def test_clip_envelope_clear_returns_exact_absence_evidence(live):
    """The adapter maps the public destructive call to one closed Boolean observation."""
    _, clip = _midi_track_with_clip(live)
    clip.add_envelope("Track Volume")
    handler = LomHandler(live.surface)

    response = _call(handler, "song/tracks/0/clip_slots/0/clip", "sunny_clear_all_envelopes")
    assert response == {"success": True, "value": {"has_envelopes": False}}
    assert clip.has_envelopes is False


def test_handler_requires_current_protocol_envelope_and_rejects_reflection_before_lom_access(
    live, monkeypatch
):
    """Only the canonical, versioned Sunny algebra can reach a Live object."""
    reached = []
    monkeypatch.setattr(live.surface.__class__, "song", lambda self: reached.append(1))
    handler = LomHandler(live.surface)
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
    assert reached == []

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
    note_dictionary = {"notes": [_wire_note(60, 0.0, 1.0)]}
    note_query = {"note_ids": [101], "return": NOTE_FIELDS}
    all_notes_query = {"return": note_query["return"]}
    routing = {"display_name": "Master", "identifier": "2:Master"}
    allowed = (
        ("get", "song", "tempo", []),
        ("call", "song", "sunny_get_scene_count", []),
        ("call", "song", "sunny_get_track_count", []),
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
        ("call", "song/tracks/0", "sunny_set_output_routing_type", [routing]),
        ("call", "song/return_tracks/0", "sunny_set_output_routing_channel", [routing, routing]),
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
        ("call", "song", "sunny_get_track_count", [0]),
        ("call", "song/tracks/0", "sunny_get_track_count", []),
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
        ("call", "song/master_track", "sunny_set_output_routing_type", [routing]),
        (
            "call",
            "song/tracks/0",
            "sunny_set_output_routing_type",
            [{**routing, "extra": 1}],
        ),
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


def test_song_collection_counts_are_closed_scalars_not_stringified_object_lists(live):
    """Cardinality adapters report plain counts for tracks, returns and scenes."""
    song = live.song
    song.create_scene(-1)
    song.create_scene(-1)
    song.create_return_track()
    song.create_return_track()
    for _ in range(3):
        song.create_midi_track(-1)
    handler = LomHandler(live.surface)

    for method, expected in (
        ("sunny_get_scene_count", 3),
        ("sunny_get_track_count", 3),
        ("sunny_get_return_track_count", 2),
    ):
        assert _call(handler, "song", method) == {"success": True, "value": expected}

    # A Live.Base.Vector is a sized host container and counts like a tuple.
    inject(song, "scenes", Vector(song.scenes))
    assert _call(handler, "song", "sunny_get_scene_count") == {"success": True, "value": 3}
    inject(song, "scenes", iter(song.scenes))
    malformed = _call(handler, "song", "sunny_get_scene_count")
    assert malformed["success"] is False
    assert "invalid scenes collection" in malformed["error"]


def test_response_serialisation_rejects_private_or_non_json_host_values(live):
    """Unsupported host wrappers cannot masquerade as strings or permissive JSON."""
    assert LomHandler._serialise(IntVector((1, 2))) == [1, 2]
    assert LomHandler._serialise(StringVector(("Off", "On"))) == ["Off", "On"]
    for enum_value, expected in ((LaunchMode.gate, 1), (CrossfadeAssignment.B, 2)):
        assert LomHandler._serialise(enum_value) == expected
        assert type(LomHandler._serialise(enum_value)) is int
    assert LomHandler._serialise(True) is True

    for value in (
        object(),
        float("nan"),
        float("inf"),
        1 << 65,
        {1: "coerced key"},
        {"nested": object()},
        RoutingType(("master",), "Master", RoutingTypeCategory.master, None),
    ):
        with pytest.raises(RuntimeError):
            LomHandler._serialise(value)

    inject(live.song, "tempo", object())
    response = _request(LomHandler(live.surface), "get", "song", "tempo")
    assert response["success"] is False
    assert "unsupported private object" in response["error"]


def test_sunny_set_cue_creates_named_cue_and_restores_playhead(live):
    """The bridge adapter gives Song's toggle-only cue API safe set semantics."""
    song = live.song
    song.current_song_time = 9.0
    handler = LomHandler(live.surface)

    assert _call(handler, "song", "sunny_set_cue", 16.0, "Verse") == {
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

    assert _call(handler, "song", "sunny_set_cue", 16.0, "Chorus") == {
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


def test_sunny_set_device_parameter_requires_exact_enabled_parameter(live):
    """Device parameter adaptation searches names and declines bad targets."""
    track = live.song.create_midi_track(-1)
    track.insert_device("Operator")
    track.insert_device("Compressor")
    handler = LomHandler(live.surface)
    parameter = next(p for p in track.devices[1].parameters if p.name == "Dry/Wet")
    request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0/devices/1",
            "name": "sunny_set_device_parameter",
            "args": ["Dry/Wet", 0.25, "value", 0.0, 1.0],
        }
    )
    observation = {
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
    }

    assert handler.handle(request) == {"success": True, "value": {**observation, "requested": 0.25}}
    assert parameter.value == 0.25
    observation_request = _versioned(
        {
            "type": "call",
            "path": "song/tracks/0/devices/1",
            "name": "sunny_get_device_parameter",
            "args": ["Dry/Wet", "value"],
        }
    )
    assert handler.handle(observation_request) == {"success": True, "value": observation}

    # EQ Eight has no Dry/Wet control: an exact-name lookup must not guess one.
    track.insert_device("EQ Eight")
    missing = handler.handle(_versioned({**observation_request, "path": "song/tracks/0/devices/2"}))
    assert missing["success"] is False
    assert "not found" in missing["error"]

    for name, malformed, expected_error in (
        ("is_enabled", 1, "invalid Boolean"),
        ("value", float("nan"), "non-finite"),
        ("max", -1.0, "inverted range"),
        ("default_value", 0, "default_value"),
        ("default_value", 2.0, "outside its reported range"),
        ("state", True, "invalid state"),
    ):
        inject(parameter, name, malformed)
        response = handler.handle(observation_request)
        assert response["success"] is False, name
        assert expected_error in response["error"]
        restore(parameter, name)
    assert parameter.value == 0.25

    inject(parameter, "is_enabled", False)
    inject(parameter, "state", ParameterState.disabled)
    disabled = handler.handle(observation_request)
    assert disabled["success"] is True
    assert disabled["value"]["is_enabled"] is False
    assert disabled["value"]["state"] == 2
    refused = handler.handle(request)
    assert refused["success"] is False
    assert "disabled" in refused["error"]
    restore(parameter, "is_enabled")
    refused = handler.handle(request)
    assert refused["success"] is False
    assert "cannot be changed" in refused["error"]
    restore(parameter, "state")
    assert parameter.value == 0.25

    inject(parameter, "is_quantized", True)
    inject(parameter, "value_items", StringVector(("Dry", "Wet")))
    quantized = handler.handle(observation_request)
    assert quantized["success"] is True
    assert quantized["value"]["default_value"] is None
    assert quantized["value"]["value_items"] == ["Dry", "Wet"]
    inject(parameter, "value_items", ("Dry", 1))
    malformed = handler.handle(observation_request)
    assert malformed["success"] is False
    assert "value_items entry" in malformed["error"]
    restore(parameter, "value_items")
    restore(parameter, "is_quantized")

    request["args"] = ["Dry/Wet", 0.375, "display_value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is True
    assert response["value"]["property"] == "display_value"
    assert response["value"]["observed"] == 0.375
    assert parameter.display_value == 0.375

    request["args"] = ["Dry/Wet", 0.75, "value", 0.0, 2.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "does not match expected" in response["error"]
    assert parameter.value == 0.375

    request["args"] = ["Unknown", 0.5, "value", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "not found" in response["error"]

    request["args"] = ["Dry/Wet", 0.5, "not_a_lom_property", 0.0, 1.0]
    response = handler.handle(request)
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_sunny_set_device_parameter_reports_post_write_automation_override(live):
    """Writing an automated parameter overrides its automation; evidence is sampled after."""
    track = live.song.create_midi_track(-1)
    track.insert_device("Operator")
    parameter = next(p for p in track.devices[0].parameters if p.name == "Filter Freq")
    parameter.start_automation_playback()
    handler = LomHandler(live.surface)

    response = _call(
        handler,
        "song/tracks/0/devices/0",
        "sunny_set_device_parameter",
        "Filter Freq",
        0.5,
        "value",
        0.0,
        1.0,
    )
    assert response["success"] is True
    assert response["value"]["observed"] == 0.5
    assert response["value"]["automation_state"] == int(AutomationState.overridden)
    assert type(response["value"]["automation_state"]) is int


def test_insert_device_returns_exact_identity_type_and_activity_evidence(live):
    """The adapter distinguishes insertion acceptance from an active instrument."""
    track = live.song.create_midi_track(-1)
    handler = LomHandler(live.surface)

    response = _call(handler, "song/tracks/0", "insert_device", "Operator", 0)
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
            "latency_in_samples": 0,
            "latency_in_ms": 0.0,
            "track_has_audio_output": True,
            "track_has_midi_output": False,
        },
    }
    assert type(response["value"]["type"]) is int
    assert [device.name for device in LomHandler._device_chain(track)] == ["Operator"]

    unknown = _call(handler, "song/tracks/0", "insert_device", "Not A Device")
    assert unknown["success"] is False
    assert "Unknown" in unknown["error"]


def test_device_snapshot_rejects_non_boolean_rack_and_activity_facts():
    """The private peer must not coerce category-confused Device facts into evidence."""
    device = native_device("Operator")
    assert LomHandler._device_snapshot(device)["can_have_chains"] is False

    for name, malformed, expected_error in (
        ("can_have_chains", 0, "invalid can_have_chains"),
        ("is_active", 1, "invalid is_active"),
        ("type", True, "invalid type"),
        ("type", 3, "invalid type"),
    ):
        inject(device, name, malformed)
        with pytest.raises(RuntimeError, match=expected_error):
            LomHandler._device_snapshot(device)
        restore(device, name)


def test_sunny_set_device_parameter_rejects_ambiguous_name_without_mutation(live):
    """A duplicate public/original name cannot select a parameter by list accident."""
    track = live.song.create_midi_track(-1)
    track.insert_device("Operator")
    device = track.devices[0]
    parameters = (
        DeviceParameter("Cutoff", original_name="Filter Frequency"),
        DeviceParameter("Filter Frequency", original_name="Frequency"),
    )
    inject(device, "parameters", parameters)
    handler = LomHandler(live.surface)

    for name, args in (
        ("sunny_set_device_parameter", ("Filter Frequency", 0.5, "value", 0.0, 1.0)),
        ("sunny_get_device_parameter", ("Filter Frequency", "value")),
    ):
        response = _call(handler, "song/tracks/0/devices/0", name, *args)
        assert response["success"] is False
        assert "ambiguous" in response["error"]
    assert [parameter.value for parameter in parameters] == [0.0, 0.0]


def test_sunny_get_device_count_excludes_the_mixer_device_and_rejects_args(live):
    """Inserted-device addressing counts the insertable chain, not Track.devices."""
    track = live.song.create_midi_track(-1)
    for name in ("Operator", "Saturator", "Reverb"):
        track.insert_device(name)
    assert len(track.devices) == 4
    handler = LomHandler(live.surface)

    assert _call(handler, "song/tracks/0", "sunny_get_device_count") == {
        "success": True,
        "value": 3,
    }
    response = _call(handler, "song/tracks/0", "sunny_get_device_count", "unexpected")
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_output_routing_is_selected_from_advertised_objects_in_two_stages(live):
    """Routes are RoutingType/RoutingChannel objects chosen from the advertised tuples."""
    song = live.song
    song.create_return_track()
    song.create_return_track()
    track = song.create_midi_track(-1)
    track.insert_device("Operator")
    handler = LomHandler(live.surface)
    master = _type_route("Master", RoutingTypeCategory.master)
    external = _type_route("Ext. Out", RoutingTypeCategory.external)
    return_a = _type_route("A-Return", RoutingTypeCategory.track)
    return_b = _type_route("B-Return", RoutingTypeCategory.track)
    track_in = _channel_route("Track In")
    advertised_types = {"available_output_routing_types": [master, external, return_a, return_b]}

    response = _call(handler, "song/tracks/0", "sunny_set_output_routing_type", return_a)
    assert response == {
        "success": True,
        "value": {
            "requested_type": return_a,
            "available_output_routing_types_before": advertised_types,
            "output_routing_type": return_a,
            "output_routing_channel": track_in,
            "available_output_routing_types": advertised_types,
            "available_output_routing_channels": {"available_output_routing_channels": [track_in]},
        },
    }
    assert track.output_routing_type.attached_object is song.return_tracks[0]

    assert _call(handler, "song/tracks/0", "sunny_set_output_routing_type", external)["success"]
    channel = _call(
        handler,
        "song/tracks/0",
        "sunny_set_output_routing_channel",
        external,
        _channel_route("3/4"),
    )
    assert channel["success"] is True
    assert channel["value"]["output_routing_type_before"] == external
    assert channel["value"]["requested_channel"] == _channel_route("3/4")
    assert channel["value"]["output_routing_channel"] == _channel_route("3/4")
    assert channel["value"]["available_output_routing_channels_before"] == {
        "available_output_routing_channels": [
            _channel_route(name) for name in ("1/2", "3/4", "1", "2", "3", "4")
        ]
    }
    assert track.output_routing_channel.display_name == "3/4"

    # An identifier naming an advertised route with another category is not that route.
    impostor = {"display_name": "Master", "identifier": return_a["identifier"]}
    for requested in (_type_route("C-Return", RoutingTypeCategory.track), impostor):
        unavailable = _call(handler, "song/tracks/0", "sunny_set_output_routing_type", requested)
        assert unavailable["success"] is False
        assert "not in available_output_routing_types" in unavailable["error"]
    assert track.output_routing_type.display_name == "Ext. Out"

    track.output_routing_type = track.available_output_routing_types[0]
    changed = _call(
        handler, "song/tracks/0", "sunny_set_output_routing_channel", external, _channel_route("1")
    )
    assert changed["success"] is False
    assert "changed before channel mutation" in changed["error"]
    assert track.output_routing_channel.display_name == "Track In"

    returned = _call(handler, "song/return_tracks/1", "sunny_set_output_routing_type", return_a)
    assert returned["success"] is True
    assert song.return_tracks[1].output_routing_type.attached_object is song.return_tracks[0]


def test_output_routing_mutation_refuses_ambiguous_advertised_routes(live):
    """Two advertised routes with one identity cannot be told apart, so neither is chosen."""
    song = live.song
    first = song.create_return_track()
    second = song.create_return_track()
    first.name = "Bus"
    second.name = "Bus"
    track = song.create_midi_track(-1)
    track.insert_device("Operator")
    handler = LomHandler(live.surface)

    response = _call(
        handler,
        "song/tracks/0",
        "sunny_set_output_routing_type",
        _type_route("Bus", RoutingTypeCategory.track),
    )
    assert response["success"] is False
    assert "ambiguous" in response["error"]
    assert track.output_routing_type.display_name == "Master"


def test_target_profile_uses_documented_application_version_and_conservative_gaps(monkeypatch):
    """The handshake reports observed Live facts without inventing Max availability."""
    live = LiveSet((12, 2, 1)).install(monkeypatch)
    handler = LomHandler(live.surface)

    response = _call(handler, "song", "sunny_get_target_profile")
    assert response["success"] is True
    profile = response["value"]
    assert profile["bridge_protocol_version"] == BRIDGE_PROTOCOL_VERSION
    assert profile["live"]["version"] == {
        "major": 12,
        "minor": 2,
        "bugfix": 1,
        "string": "12.2.1",
    }
    assert profile["capabilities"]["clip_add_new_notes"] == "available"
    assert profile["capabilities"]["track_insert_device_native"] == "unavailable"
    assert profile["capabilities"]["max_for_live"] == "unknown"
    assert profile["capabilities"]["structural_snapshot"] == "available"
    assert profile["adapter"]["contract"] == "version_coupled_private"


def test_target_profile_rejects_scalar_category_coercion(live, monkeypatch):
    """Private host values cannot acquire documented profile categories via constructors."""
    application = live.application
    handler = LomHandler(live.surface)

    for method_name, malformed_value, expected_error in (
        ("get_major_version", "12", "Application major version"),
        ("get_minor_version", 3.0, "Application minor version"),
        ("get_bugfix_version", True, "Application bugfix version"),
        ("get_version_string", object(), "Application version string"),
    ):
        with monkeypatch.context() as patch:
            patch.setattr(application, method_name, lambda value=malformed_value: value)
            response = _call(handler, "song", "sunny_get_target_profile")
        assert response["success"] is False
        assert expected_error in response["error"]
    assert _call(handler, "song", "sunny_get_target_profile")["success"] is True


def _snapshot_fixture(live, instrument="Operator"):
    """A Set with one scored MIDI track, one return, two scenes and one cue."""
    song = live.song
    song.create_scene(-1)
    song.scenes[0].name = "Scene 1"
    song.scenes[1].name = "Scene 2"
    song.scenes[1].enable_launch_overrides(128.0, 7, 8)
    song.create_return_track()
    track, clip = _midi_track_with_clip(live, instrument=instrument)
    track.name = "Part"
    clip.name = "Verse"
    clip.looping = False
    song.current_song_time = 0.0
    song.set_or_delete_cue()
    song.cue_points[0].name = "Start"
    return track, clip


def test_target_snapshot_is_single_call_structural_plan_evidence(live, sunny_native_module):
    """The snapshot reads Live's Python types and satisfies the native snapshot validator."""
    track, clip = _snapshot_fixture(live)
    handler = LomHandler(live.surface)
    request = _versioned(
        {"type": "call", "path": "song", "name": "sunny_get_target_snapshot", "args": []}
    )

    response = handler.handle(request)
    assert response["success"] is True, response
    snapshot = response["value"]
    assert snapshot["schema_version"] == TARGET_SNAPSHOT_SCHEMA_VERSION
    assert snapshot["target_profile"]["bridge_protocol_version"] == BRIDGE_PROTOCOL_VERSION
    assert sunny_native_module.ABLETON_BRIDGE_PROTOCOL_VERSION == BRIDGE_PROTOCOL_VERSION
    assert (
        sunny_native_module.ABLETON_TARGET_SNAPSHOT_SCHEMA_VERSION == TARGET_SNAPSHOT_SCHEMA_VERSION
    )
    assert sunny_native_module.validate_ableton_target_snapshot_json(json.dumps(snapshot))

    song_state = snapshot["song"]
    assert song_state["scale"] == {
        "root_note": 0,
        "name": "Major",
        "intervals": [0, 2, 4, 5, 7, 9, 11],
        "mode": False,
    }
    assert song_state["tuning_system"]["name"] == "12-TET"
    assert song_state["tuning_system"]["note_tunings"] == {
        "note_tunings": [100.0 * n for n in range(12)]
    }
    assert song_state["scene_count"] == 2
    assert song_state["scenes"] == [
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
    assert song_state["cue_points"] == [{"name": "Start", "time": 0.0}]

    part = song_state["tracks"][0]
    assert part["name"] == "Part"
    assert part["devices"] == [
        {
            "name": "Operator",
            "class_display_name": "Operator",
            "class_name": "Operator",
            "type": 1,
            "is_active": True,
            "can_have_chains": False,
            "latency_in_samples": 0,
            "latency_in_ms": 0.0,
        }
    ]
    assert part["mixer"]["volume"] == {
        "value": 0.85,
        "display_value": 0.0,
        "minimum": 0.0,
        "maximum": 1.0,
        "is_quantized": False,
        "default_value": 0.85,
        "value_items": None,
        "state": 0,
        "automation_state": 0,
        "is_enabled": True,
    }
    assert part["mixer"]["track_activator"]["value_items"] == ["Off", "On"]
    assert part["mixer"]["track_activator"]["default_value"] is None
    assert len(part["mixer"]["sends"]) == 1
    assert part["mixer"]["crossfade_assign"] == 1
    assert part["mixer"]["panning_mode"] == 0
    assert part["clip_slot_count"] == 2
    assert part["arrangement_clip_count"] == 0
    assert part["take_lane_count"] == 0
    assert [slot["has_clip"] for slot in part["clip_slots"]] == [True, False]
    assert part["clip_slots"][0]["playing_status"] == 0
    assert part["clips"] == [
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
            "launch_quantization": 0,
            "legato": False,
            "velocity_amount": 0.0,
            "has_groove": False,
        }
    ]
    assert part["input_routing_type"] == _type_route("All Ins", RoutingTypeCategory.external)
    assert part["input_routing_channel"] == _channel_route("All Channels")
    assert part["output_routing_type"] == _type_route("Master", RoutingTypeCategory.master)
    assert part["output_routing_channel"] == _channel_route("Track In")
    assert part["available_output_routing_types"] == {
        "available_output_routing_types": [
            _type_route("Master", RoutingTypeCategory.master),
            _type_route("Ext. Out", RoutingTypeCategory.external),
            _type_route("A-Return", RoutingTypeCategory.track),
        ]
    }
    assert {name: part[name] for name in ("has_audio_output", "has_midi_output")} == {
        "has_audio_output": True,
        "has_midi_output": False,
    }
    assert part["input_meter_level"] == 0.0 and part["output_meter_right"] == 0.0
    assert song_state["return_tracks"][0]["name"] == "A-Return"
    assert song_state["return_tracks"][0]["output_routing_type"] == _type_route(
        "Master", RoutingTypeCategory.master
    )
    assert song_state["master_track"]["mixer"]["crossfade_assign"] is None

    def rejected(owner, name, malformed, expected_error):
        inject(owner, name, malformed)
        try:
            response = handler.handle(request)
        finally:
            restore(owner, name)
        assert response["success"] is False, (name, malformed)
        assert expected_error in response["error"], response["error"]

    song = live.song
    slot = track.clip_slots[0]
    mixer = track.mixer_device
    device = track.devices[0]
    returned = song.return_tracks[0]

    for flag in ("is_playing", "metronome", "loop", "session_record", "nudge_up"):
        rejected(song, flag, 0, f"invalid {flag}")
    for name in ("is_triggered", "tempo_enabled", "time_signature_enabled"):
        rejected(song.scenes[0], name, 0, f"invalid {name}")
    for name in ("has_clip", "has_stop_button", "is_group_slot", "is_playing", "is_triggered"):
        rejected(slot, name, int(getattr(slot, name)), f"invalid {name}")
    rejected(slot, "playing_status", True, "invalid playing_status")
    rejected(slot, "playing_status", ClipSlotPlayingStatus.playing, "incoherent is_playing")
    for name in ("arm", "implicit_arm", "back_to_arranger", "is_frozen", "has_midi_input"):
        rejected(track, name, int(getattr(track, name)), f"invalid {name}")
    for name, malformed in (
        ("fired_slot_index", True),
        ("fired_slot_index", -3),
        ("playing_slot_index", 2),
        ("input_meter_level", 0),
        ("output_meter_level", 1.1),
        ("output_meter_right", math.inf),
    ):
        rejected(track, name, malformed, f"invalid {name}")
    rejected(track, "arrangement_clips", object(), "invalid arrangement_clips collection")
    rejected(track, "take_lanes", iter(()), "invalid take_lanes collection")
    rejected(track, "output_routing_type", {"display_name": "Master"}, "output_routing_type")
    rejected(
        track,
        "available_output_routing_types",
        "Master",
        "invalid available_output_routing_types",
    )
    rejected(
        track,
        "output_routing_type",
        RoutingType(("elsewhere",), "Elsewhere", RoutingTypeCategory.external, None),
        "not in available_output_routing_types",
    )
    rejected(
        track,
        "input_routing_channel",
        RoutingChannel(("channel", "Ch. 99"), "Ch. 99"),
        "not in available_input_routing_channels",
    )
    for name in ("mute", "solo", "muted_via_solo"):
        rejected(returned, name, 0, f"invalid {name}")
    rejected(mixer, "crossfade_assign", True, "invalid crossfade_assign")
    rejected(mixer, "crossfade_assign", 3, "invalid crossfade_assign")
    rejected(mixer, "panning_mode", 2, "invalid panning_mode")
    for name, malformed, expected_error in (
        ("value", 0, "invalid value"),
        ("display_value", 0, "invalid display_value"),
        ("min", 0, "invalid minimum"),
        ("max", float("inf"), "invalid maximum"),
        ("value", 2.0, "outside its reported range"),
        ("is_quantized", 0, "invalid is_quantized"),
        ("default_value", 0, "default_value"),
        ("automation_state", 3, "invalid automation_state"),
    ):
        rejected(mixer.volume, name, malformed, expected_error)
    rejected(mixer.track_activator, "value_items", ("Off", 1), "value_items entry")
    for name in ("has_groove", "has_envelopes", "is_audio_clip", "looping", "is_playing"):
        rejected(clip, name, int(getattr(clip, name)), f"invalid {name}")
    rejected(clip, "is_audio_clip", True, "incoherent audio/MIDI identity")
    rejected(clip, "is_arrangement_clip", True, "Arrangement Clip")
    rejected(clip, "is_session_clip", 1, "invalid Live 11+ location identity")
    rejected(clip, "is_session_clip", False, "non-Session Clip")
    rejected(clip, "is_take_lane_clip", 0, "invalid Live 12+ take-lane identity")
    rejected(clip, "is_take_lane_clip", True, "returned a Take Lane Clip")
    rejected(clip, "end_time", 4, "invalid Clip end_time")
    rejected(clip, "end_time", 3.0, "incoherent unlooped end_time")
    rejected(clip, "launch_mode", True, "invalid launch_mode")
    rejected(clip, "launch_quantization", 15, "invalid launch_quantization")
    rejected(clip, "legato", 0, "invalid legato")
    rejected(clip, "velocity_amount", 0, "invalid velocity_amount")
    rejected(device, "latency_in_samples", True, "invalid latency_in_samples")
    rejected(device, "latency_in_ms", -0.1, "invalid latency_in_ms")
    rejected(song, "tempo", 120, "Song tempo")
    rejected(song, "signature_numerator", 4.0, "Song signature_numerator")
    rejected(song.scenes[0], "name", object(), "Scene name")
    rejected(song.cue_points[0], "time", 0, "CuePoint time")
    rejected(clip, "name", object(), "Clip name")
    rejected(clip, "length", 4, "Clip length")
    rejected(device, "name", object(), "Device name")

    inject(track, "has_midi_input", False)
    no_input = handler.handle(request)
    restore(track, "has_midi_input")
    assert no_input["success"] is True
    assert {
        name: no_input["value"]["song"]["tracks"][0][name]
        for name in (
            "input_routing_type",
            "available_input_routing_channels",
            "input_meter_level",
            "output_meter_level",
        )
    } == dict.fromkeys(
        (
            "input_routing_type",
            "available_input_routing_channels",
            "input_meter_level",
            "output_meter_level",
        )
    )

    request["args"] = ["unexpected"]
    response = handler.handle(request)
    assert response["success"] is False
    assert "outside Sunny bridge protocol" in response["error"]


def test_target_snapshot_accepts_live_container_types_for_documented_lists(live):
    """Live may return Base vectors where Cycling '74 documents lists (#22); both are read."""
    track, _ = _snapshot_fixture(live)
    handler = LomHandler(live.surface)
    inject(live.song, "scenes", Vector(live.song.scenes))
    inject(track, "arrangement_clips", Vector())
    inject(track.mixer_device.track_activator, "value_items", ("Off", "On"))
    inject(live.song, "scale_intervals", [0, 2, 4, 5, 7, 9, 11])

    response = _call(handler, "song", "sunny_get_target_snapshot")
    assert response["success"] is True, response
    assert response["value"]["song"]["scene_count"] == 2
    assert response["value"]["song"]["scale"]["intervals"] == [0, 2, 4, 5, 7, 9, 11]


def test_target_snapshot_pitch_context_is_version_coupled(monkeypatch):
    """Unsupported older Live versions emit explicit null context instead of probing it."""
    live = LiveSet((11, 3, 0)).install(monkeypatch)
    response = _call(LomHandler(live.surface), "song", "sunny_get_target_snapshot")

    assert response["success"] is True, response
    assert response["value"]["song"]["scale"] is None
    assert response["value"]["song"]["tuning_system"] is None


@pytest.mark.parametrize(
    ("version", "take_lanes_available"), [((11, 3, 0), False), ((12, 0, 5), True)]
)
def test_target_snapshot_take_lane_state_is_coupled_to_live_12(
    monkeypatch, sunny_native_module, version, take_lanes_available
):
    """A Live 11 Set has no take lanes; its snapshot reports none, and Live 12 still does."""
    live = LiveSet(version).install(monkeypatch)
    _snapshot_fixture(live, instrument=None)
    response = _call(LomHandler(live.surface), "song", "sunny_get_target_snapshot")

    assert response["success"] is True, response
    part = response["value"]["song"]["tracks"][0]
    assert part["arrangement_clip_count"] == 0
    assert part["clips"][0]["is_session_clip"] is True
    if take_lanes_available:
        assert part["take_lane_count"] == 0
        assert part["clips"][0]["is_take_lane_clip"] is False
    else:
        assert part["take_lane_count"] is None
        assert part["clips"][0]["is_take_lane_clip"] is None
    assert sunny_native_module.validate_ableton_target_snapshot_json(json.dumps(response["value"]))


@pytest.mark.parametrize(
    ("owner", "name", "malformed"),
    [
        ("song", "scale_intervals", [0, True]),
        ("song", "scale_intervals", "0,2,4"),
        ("song", "root_note", 12),
        ("song", "scale_mode", 1),
        ("tuning", "pseudo_octave_in_cents", float("inf")),
        ("tuning", "pseudo_octave_in_cents", 1200),
        ("tuning", "name", 12),
        ("tuning", "lowest_note", []),
        ("tuning", "highest_note", {"nested": float("nan")}),
        ("tuning", "note_tunings", {"first": [0.0], "second": [100.0]}),
        ("tuning", "note_tunings", {"only": [0.0, "100"]}),
    ],
)
def test_target_snapshot_rejects_malformed_host_pitch_context(live, owner, name, malformed):
    """Private host wrappers cannot smuggle malformed scale/tuning facts into evidence."""
    if owner == "song":
        inject(live.song, name, malformed)
    else:
        setattr(live.song.tuning_system, name, malformed)

    response = _call(LomHandler(live.surface), "song", "sunny_get_target_snapshot")
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
