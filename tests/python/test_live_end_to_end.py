"""End-to-end tests: MCP client, ``sunny-mcp``, TCP bridge, real Remote Script, Live model.

Each test starts the real Remote Script control surface in-process against the
offline Live model, with a dedicated thread standing in for Live's main thread
so that scheduled LOM work runs off the socket thread as it does in Live. It
then launches the built ``sunny-mcp`` binary, speaks MCP JSON-RPC to it over
stdio, and asserts the state of the modelled Live Set. Nothing in the path
between the MCP request and the model is a test double.
"""

from __future__ import annotations

import json
import os
import queue
import subprocess
import threading
from fractions import Fraction
from pathlib import Path
from typing import Any

import pytest
from live_model import LiveSet, RoutingTypeCategory
from Sunny import surface as surface_module
from Sunny.surface import SunnyControlSurface

PROJECT_ROOT = Path(__file__).resolve().parents[2]
RESPONSE_TIMEOUT_SECONDS = 30.0


def _sunny_mcp_binary() -> Path:
    configured = os.environ.get("SUNNY_MCP_BINARY")
    candidates = [Path(configured)] if configured else []
    candidates += [PROJECT_ROOT / ".bin" / "sunny-mcp", PROJECT_ROOT / ".bin-python" / "sunny-mcp"]
    for candidate in candidates:
        if candidate.is_file() and os.access(candidate, os.X_OK):
            return candidate
    pytest.skip(
        "sunny-mcp is not built: set SUNNY_MCP_BINARY or build the sunny-mcp target into .bin"
    )


class _LiveMainThread:
    """Run callbacks scheduled with ``schedule_message`` serially on one thread."""

    def __init__(self) -> None:
        self._callbacks: queue.Queue[Any] = queue.Queue()
        self._thread = threading.Thread(target=self._run, name="LiveMainThread", daemon=True)
        self._thread.start()

    def schedule_message(self, delay: int, callback: Any) -> None:
        self._callbacks.put(callback)

    def _run(self) -> None:
        while (callback := self._callbacks.get()) is not None:
            callback()

    def stop(self) -> None:
        self._callbacks.put(None)
        self._thread.join(timeout=5)


class _McpClient:
    """A line-delimited JSON-RPC client for one ``sunny-mcp`` process."""

    def __init__(self, binary: Path, port: int) -> None:
        environment = dict(os.environ, SUNNY_ABLETON_HOST="127.0.0.1", SUNNY_TCP_PORT=str(port))
        self._process = subprocess.Popen(
            [str(binary)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            env=environment,
        )
        self._lines: queue.Queue[str | None] = queue.Queue()
        self._reader = threading.Thread(target=self._read, daemon=True)
        self._reader.start()
        self._next_id = 0
        initialised = self._rpc("initialize", {"protocolVersion": "2024-11-05"})
        assert "result" in initialised, initialised

    def _read(self) -> None:
        assert self._process.stdout is not None
        for line in self._process.stdout:
            self._lines.put(line)
        self._lines.put(None)

    def _rpc(self, method: str, params: dict[str, Any]) -> dict[str, Any]:
        self._next_id += 1
        request = {"jsonrpc": "2.0", "id": self._next_id, "method": method, "params": params}
        assert self._process.stdin is not None
        self._process.stdin.write(json.dumps(request) + "\n")
        self._process.stdin.flush()
        line = self._lines.get(timeout=RESPONSE_TIMEOUT_SECONDS)
        assert line is not None, "sunny-mcp exited"
        response = json.loads(line)
        assert response.get("id") == self._next_id, response
        return response

    def call(self, tool: str, **arguments: Any) -> dict[str, Any]:
        response = self._rpc("tools/call", {"name": tool, "arguments": arguments})
        assert "result" in response, response
        return json.loads(response["result"]["content"][0]["text"])

    def close(self) -> None:
        if self._process.stdin is not None:
            self._process.stdin.close()
        try:
            self._process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            self._process.kill()
            self._process.wait()


@pytest.fixture
def bridge(request, monkeypatch):
    """Yield ``(live, client)``: a modelled Live Set behind the real bridge and an MCP client."""
    binary = _sunny_mcp_binary()
    # Tests may request another Live version with indirect parametrisation.
    live = LiveSet(getattr(request, "param", (12, 3, 5))).install(monkeypatch)
    # Port 0 asks the OS for an ephemeral port; the surface's own parser
    # accepts only 1..65535, so the configuration is supplied directly.
    monkeypatch.setattr(surface_module, "_server_configuration", lambda: ("127.0.0.1", 0))
    main_thread = _LiveMainThread()
    surface = SunnyControlSurface(object())
    surface.schedule_message = main_thread.schedule_message
    client = None
    try:
        for _ in range(500):
            if surface._server.is_running:
                break
            threading.Event().wait(0.01)
        client = _McpClient(binary, surface._server.bound_port)
        yield live, client
    finally:
        if client is not None:
            client.close()
        surface.disconnect()
        main_thread.stop()


def _pitch(letter: str, octave: int, accidental: int = 0) -> dict[str, Any]:
    return {"letter": letter, "accidental": accidental, "octave": octave}


def _whole(n: int, d: int) -> dict[str, int]:
    """A Score IR duration or offset, measured in whole notes."""
    return {"n": n, "d": d}


def _beats(whole_notes: Fraction) -> float:
    """Live's clip time unit is the quarter-note beat."""
    return float(whole_notes * 4)


def _clip_notes(clip) -> list[tuple[int, float, float, float, bool, float]]:
    return sorted(
        (
            note.pitch,
            note.start_time,
            note.duration,
            note.velocity,
            note.mute,
            note.release_velocity,
        )
        for note in clip.get_all_notes_extended()
    )


def _event_id(client: _McpClient, score_id: int, part_index: int, bar: int, offset: Fraction):
    document = client.call("score_get_json", score_id=score_id)
    for event in document["parts"][part_index]["measures"][bar - 1]["voices"][0]["events"]:
        position = Fraction(event["offset"]["num"], event["offset"]["den"])
        if event["type"] == "note_group" and position == offset:
            return event["id"]
    raise AssertionError(f"no note at bar {bar} offset {offset}")


def test_score_compiles_to_exact_live_notes_with_a_tie_and_a_triplet(bridge):
    """A two-bar 4/4 score reaches Live as exactly the hand-derived notes."""
    live, client = bridge
    score = client.call(
        "score_create",
        title="Two Bars",
        total_bars=2,
        bpm=96,
        time_sig_num=4,
        time_sig_den=4,
        parts=[
            {"name": "Chords", "instrument_type": 0},
            {"name": "Melody", "instrument_type": 0},
        ],
    )
    score_id = score["score_id"]
    chords, melody = score["part_ids"]

    def insert(part_id, bar, offset, pitch, duration):
        result = client.call(
            "score_insert_note",
            score_id=score_id,
            part_id=part_id,
            bar=bar,
            offset=_whole(offset.numerator, offset.denominator),
            pitch=pitch,
            duration=_whole(duration.numerator, duration.denominator),
            velocity=100,
        )
        assert result.get("ok") is True, result

    # A C major triad held for bar 1; coincident notes join one chord.
    for pitch in (_pitch("C", 4), _pitch("E", 4), _pitch("G", 4)):
        insert(chords, 1, Fraction(0), pitch, Fraction(1))

    # Melody: E5 crotchet, a D5-E5-F5 quaver triplet, G5 minim tied over the
    # barline to a G5 crotchet, then C5 for the rest of bar 2.
    third = Fraction(1, 12)
    insert(melody, 1, Fraction(0), _pitch("E", 5), Fraction(1, 4))
    for index, pitch in enumerate((_pitch("D", 5), _pitch("E", 5), _pitch("F", 5))):
        insert(melody, 1, Fraction(1, 4) + index * third, pitch, third)
    insert(melody, 1, Fraction(1, 2), _pitch("G", 5), Fraction(1, 2))
    insert(melody, 2, Fraction(0), _pitch("G", 5), Fraction(1, 4))
    insert(melody, 2, Fraction(1, 4), _pitch("C", 5), Fraction(3, 4))
    tie_source = _event_id(client, score_id, 1, 1, Fraction(1, 2))
    tied = client.call("score_set_tie", score_id=score_id, event_id=tie_source, tied=True)
    assert tied.get("ok") is True, tied

    compiled = client.call("score_compile_to_ableton", score_id=score_id)
    assert compiled.get("success") is True, compiled

    song = live.song
    assert song.tempo == 96.0
    assert (song.signature_numerator, song.signature_denominator) == (4, 4)
    assert song.scenes[0].name == "Two Bars"
    assert [track.name for track in song.tracks] == ["Chords", "Melody"]

    chord_clip = song.tracks[0].clip_slots[0].clip
    melody_clip = song.tracks[1].clip_slots[0].clip
    for clip, name in ((chord_clip, "Chords"), (melody_clip, "Melody")):
        assert clip.name == name
        assert clip.looping is False
        assert (clip.start_marker, clip.end_marker) == (0.0, 8.0)
        assert clip.launch_quantization == 1

    whole_bar = Fraction(1)
    assert _clip_notes(chord_clip) == [
        (pitch, 0.0, _beats(whole_bar), 100.0, False, 64.0) for pitch in (60, 64, 67)
    ]
    expected_melody = [
        (76, Fraction(0), Fraction(1, 4)),
        (74, Fraction(1, 4), third),
        (76, Fraction(1, 4) + third, third),
        (77, Fraction(1, 4) + 2 * third, third),
        # The tie joins the bar-1 minim and the bar-2 crotchet into one note.
        (79, Fraction(1, 2), Fraction(3, 4)),
        (72, Fraction(5, 4), Fraction(3, 4)),
    ]
    assert _clip_notes(melody_clip) == sorted(
        (pitch, _beats(start), _beats(duration), 100.0, False, 64.0)
        for pitch, start, duration in expected_melody
    )


def test_project_plan_applies_score_timbre_and_mix_with_a_completed_journal(bridge):
    """A planned project deploys tracks, devices, mixer values and routes against Live."""
    live, client = bridge
    # A user track that is sounding: its meters move between plan and apply.
    existing = live.song.create_midi_track(-1)
    existing.name = "User Track"
    existing.insert_device("Operator")

    score = client.call(
        "score_create",
        title="Project",
        total_bars=1,
        time_sig_num=4,
        time_sig_den=4,
        parts=[{"name": "Lead", "instrument_type": 0}],
    )
    part_id = score["part_ids"][0]
    inserted = client.call(
        "score_insert_note",
        score_id=score["score_id"],
        part_id=part_id,
        bar=1,
        offset=_whole(0, 1),
        pitch=_pitch("C", 4),
        duration=_whole(1, 2),
        velocity=90,
    )
    assert inserted.get("ok") is True, inserted
    profile = client.call("create_timbre_profile", part_id=part_id, name="Lead")
    source = client.call(
        "set_sound_source", profile_id=profile["profile_id"], source_type="fm", operator_count=4
    )
    assert source.get("success") is True, source
    mix = client.call("create_mix_graph", part_ids=[part_id])
    channels = client.call("get_mix_json", graph_id=mix["graph_id"])["mix_ir"]["channels"]
    channel_id = next(channel["id"] for channel in channels if channel["part_id"] == part_id)
    for tool, arguments in (
        ("set_channel_level", {"level_db": -6.0}),
        ("set_channel_pan", {"pan": 0.25}),
    ):
        result = client.call(tool, graph_id=mix["graph_id"], channel_id=channel_id, **arguments)
        assert result.get("success") is True, (tool, result)

    master = {"display_name": "Master", "identifier": f"{int(RoutingTypeCategory.master)}:Master"}
    track_in = {"display_name": "Track In", "identifier": "Track In"}
    project = {
        "score_id": score["score_id"],
        "timbre_profile_ids": [profile["profile_id"]],
        "mix_graph_id": mix["graph_id"],
        "output_routing_bindings": {
            "part_tracks": [
                {
                    "part_id": part_id,
                    "type": master,
                    "channel": track_in,
                    "mapping_provenance": "offline Live model routing names",
                }
            ],
            "aux_returns": [],
        },
    }
    validation = client.call("project_validate", **project)
    assert validation["valid"] is True, validation

    plan = client.call("project_plan_to_ableton", **project)
    assert plan.get("success") is True, plan
    assert [track.name for track in live.song.tracks] == ["User Track"]

    existing.set_meters(0.8)
    applied = client.call("project_apply_ableton_plan", plan_id=plan["plan_id"])
    assert applied.get("success") is True, applied
    deployment = applied["deployment"]
    assert deployment["status"] == "completed"
    assert deployment["target_may_be_partially_modified"] is True
    assert len(deployment["mutation_journal"]) == len(plan["planned_mutations"])
    assert {entry["outcome"] for entry in deployment["mutation_journal"]} == {"acknowledged"}

    names = [track.name for track in live.song.tracks]
    # The plan inserts Part tracks at their Part index, ahead of existing tracks.
    assert names == ["Lead", "User Track"]
    lead = live.song.tracks[names.index("Lead")]
    chain = [device.name for device in lead.devices if device is not lead.mixer_device]
    assert chain[0] == "Operator"
    assert lead.mixer_device.volume.display_value == -6.0
    assert lead.mixer_device.panning.value == 0.25
    assert lead.output_routing_type.display_name == "Master"
    assert lead.output_routing_channel.display_name == "Track In"
    assert _clip_notes(lead.clip_slots[0].clip) == [(60, 0.0, 2.0, 90.0, False, 64.0)]
    user = live.song.tracks[names.index("User Track")]
    assert [device.name for device in user.devices if device is not user.mixer_device] == [
        "Operator"
    ]
    assert user.clip_slots[0].has_clip is False


def test_progression_clip_and_session_state_reach_live(bridge):
    """The orchestrator's progression clip lands in Live; session state reads its counts."""
    live, client = bridge
    live.song.create_return_track()
    live.song.create_midi_track(-1)

    created = client.call(
        "create_progression_clip",
        track_index=0,
        slot_index=0,
        root="C",
        scale="major",
        numerals=["I", "IV", "V", "I"],
        octave=4,
        duration_beats=4.0,
    )
    assert created.get("success") is True, created
    clip = live.song.tracks[0].clip_slots[0].clip
    assert clip is not None
    pitches_by_start: dict[float, list[int]] = {}
    for pitch, start, *_ in _clip_notes(clip):
        pitches_by_start.setdefault(start, []).append(pitch)
    assert sorted(pitches_by_start) == [0.0, 1.0, 2.0, 3.0]
    pitch_classes = [
        sorted({p % 12 for p in pitches}) for _, pitches in sorted(pitches_by_start.items())
    ]
    assert pitch_classes == [[0, 4, 7], [0, 5, 9], [2, 7, 11], [0, 4, 7]]

    state = client.call("get_ableton_session_state")
    assert state.get("success") is True, state
    assert state["track_count"] == 1
    assert state["return_track_count"] == 1
    assert state["tempo"] == 120.0
    assert state["target_profile"]["live"]["version"]["string"] == "12.3.5"


@pytest.mark.parametrize("bridge", [(11, 3, 0)], indirect=True)
def test_project_plan_applies_against_live_11_without_take_lanes(bridge):
    """A Live 11 Set has no take lanes; planning and applying a project still completes."""
    live, client = bridge
    existing = live.song.create_midi_track(-1)
    existing.name = "User Track"
    existing.clip_slots[0].create_clip(4.0)

    score = client.call(
        "score_create",
        title="Eleven",
        total_bars=1,
        time_sig_num=4,
        time_sig_den=4,
        parts=[{"name": "Lead", "instrument_type": 0}],
    )
    part_id = score["part_ids"][0]
    inserted = client.call(
        "score_insert_note",
        score_id=score["score_id"],
        part_id=part_id,
        bar=1,
        offset=_whole(0, 1),
        pitch=_pitch("C", 4),
        duration=_whole(1, 4),
        velocity=80,
    )
    assert inserted.get("ok") is True, inserted
    profile = client.call("create_timbre_profile", part_id=part_id, name="Lead")
    mix = client.call("create_mix_graph", part_ids=[part_id])
    project = {
        "score_id": score["score_id"],
        "timbre_profile_ids": [profile["profile_id"]],
        "mix_graph_id": mix["graph_id"],
    }

    plan = client.call("project_plan_to_ableton", **project)
    assert plan.get("success") is True, plan
    snapshot_tracks = plan["target_snapshot"]["song"]["tracks"]
    assert snapshot_tracks[0]["take_lane_count"] is None
    assert snapshot_tracks[0]["clips"][0]["is_take_lane_clip"] is None
    assert snapshot_tracks[0]["clips"][0]["is_session_clip"] is True

    applied = client.call("project_apply_ableton_plan", plan_id=plan["plan_id"])
    assert applied.get("success") is True, applied
    assert applied["deployment"]["status"] == "completed"
    assert {entry["outcome"] for entry in applied["deployment"]["mutation_journal"]} == {
        "acknowledged"
    }
    # Live 11 has no take lanes, so their absence and the clip's identity verify
    # without take-lane evidence.
    postconditions = applied["postconditions"]
    assert postconditions["track_gates"][0]["take_lane_topology_observed"] is False
    assert postconditions["track_gates"][0]["take_lanes_absent_verified"] is True
    assert postconditions["clips"][0]["observed_is_take_lane_clip"] is None
    assert postconditions["clips"][0]["clip_identity_verified"] is True
    assert [track.name for track in live.song.tracks] == ["Lead", "User Track"]
    lead = live.song.tracks[0]
    assert _clip_notes(lead.clip_slots[0].clip) == [(60, 0.0, 1.0, 80.0, False, 64.0)]
