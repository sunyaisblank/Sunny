"""Literal wire and scratch-authority checks for the maintained host kit.

Live itself is the external boundary. These checks establish kit behavior, not
native Python ABI, audio, saved-Set persistence or any real-host acceptance.
"""

from __future__ import annotations

import importlib.util
import json
import subprocess
import sys
import threading
from pathlib import Path
from types import ModuleType, SimpleNamespace

import pytest
from live_model import LiveSet
from Sunny.handler import LomHandler
from Sunny.server import TcpServer

KIT = Path(__file__).resolve().parents[2] / "tools" / "live_qualification"
GROUP_IDS = (
    "runtime_containers",
    "display_value",
    "scene_flags",
    "groove_none",
    "schedule_zero",
    "insertion_return",
    "note_float_precision",
    "chorus_phaser_names",
    "running_cue",
    "large_set",
    "legacy_11_range",
    "modern_note_branch",
    "host_matrix",
    "complete_owning_authoring",
    "restart_lost_reply",
    "native_envelope_persistence",
    "take_lane_versions",
    "fader_send_floor",
    "routing_names",
    "routing_category",
    "track_devices_mixer",
    "dry_wet_table",
    "routing_and_modes_persistence",
)


def load_module(name, path):
    """Load a standalone maintained helper without executing its command line."""
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


@pytest.fixture
def common():
    """Load the actual transport helper without invoking a host command."""
    return load_module("qualification_common_test", KIT / "common.py")


@pytest.fixture
def running_bridge(monkeypatch):
    """Run real framed TCP and handler around the external Live boundary model."""
    provider = LiveSet(midi_tracks=1).install(monkeypatch)
    provider.song.scenes[0].name = "Literal scene witness"
    handler = LomHandler(provider.surface)
    received = []

    def dispatch(request):
        received.append(request)
        return handler.handle(request)

    server = TcpServer(host="127.0.0.1", port=0, handler=dispatch)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    assert server._ready.wait(3)
    yield server, provider, received
    server.shutdown()
    thread.join(timeout=3)
    assert not thread.is_alive()


def configuration(server):
    """Select the literal protocol expected by the production wire witness."""
    return {
        "bridge_host": "127.0.0.1",
        "bridge_port": server.bound_port,
        "expected_bridge_protocol_version": 46,
    }


def test_all_original_host_groups_keep_their_evidence_and_pending_state():
    """The literal original obligations stay intact; local checks never mark Live passed."""
    data = json.loads((KIT / "obligations.json").read_text())
    rows = data["every_obligation"]
    assert tuple(row["id"] for row in rows) == GROUP_IDS
    for row in rows:
        assert row["required_evidence"] and row["probe"] and row["boundary"]
        assert row["status"] == "untested"
        assert row["actual_version"] is None
        assert row["log"] is None and row["manual_evidence"] is None


def test_shipped_read_requests_reach_the_production_handler(common, running_bridge, tmp_path):
    """A profile and literal Scene getter must reach their operations, not version refusal."""
    server, _, received = running_bridge
    config = configuration(server)
    log = tmp_path / "wire.jsonl"
    profile = json.loads((KIT / "requests" / "profile.json").read_text())
    response = common.bridge_rpc(config, profile, log)
    assert response["success"] is True
    assert response["value"]["live"]["version"] == {
        "major": 12,
        "minor": 3,
        "bugfix": 5,
        "string": "12.3.5",
    }
    scene = json.loads((KIT / "requests" / "scene0-read.json").read_text())
    response = common.bridge_rpc(config, scene, log)
    assert response["bridge_protocol_version"] == 46 and response["success"] is True
    assert response["value"]["song"]["scenes"][0]["name"] == "Literal scene witness"
    assert response["value"]["song"]["scenes"][0]["tempo_enabled"] is False
    assert received == [profile, scene]


def test_missing_wrong_and_rejected_requests_cannot_be_successful_observations(
    common, running_bridge, tmp_path
):
    """Validate literal missing/wrong protocol and native refusal without retry."""
    server, _, received = running_bridge
    config = configuration(server)
    log = tmp_path / "wire.jsonl"
    request = {"type": "get", "path": "song/scenes/0", "name": "name"}
    for version in (None, 45):
        invalid = dict(request)
        if version is not None:
            invalid["bridge_protocol_version"] = version
        with pytest.raises(ValueError, match="protocol"):
            common.bridge_rpc(config, invalid, log)
        response = common.rpc(config["bridge_host"], config["bridge_port"], invalid, log)
        assert response["success"] is False
        assert "Unsupported bridge protocol version" in response["error"]
    assert len(received) == 2
    rejected = {"bridge_protocol_version": 46, "type": "get", "path": "song", "name": "nonexistent"}
    with pytest.raises(RuntimeError, match="Preserve evidence"):
        common.bridge_rpc(config, rejected, log)
    assert len(received) == 3


def test_snapshot_timing_cli_validates_a_successful_actual_wire_response(running_bridge, tmp_path):
    """The real CLI checks profile/source/schema rather than timing a rejection."""
    server, _, received = running_bridge
    config = configuration(server)
    from Sunny.build_identity import BRIDGE_SOURCE_SHA256

    config.update(
        expected_target_snapshot_schema_version=35,
        expected_bridge_source_sha256=BRIDGE_SOURCE_SHA256,
        evidence_directory=str(tmp_path / "evidence"),
    )
    path = tmp_path / "config.json"
    path.write_text(json.dumps(config))
    result = subprocess.run(
        [
            sys.executable,
            str(KIT / "host_runner.py"),
            "--config",
            str(path),
            "--repeat",
            "1",
            "snapshot-timing",
        ],
        capture_output=True,
        text=True,
        timeout=10,
    )
    assert result.returncode == 0, result.stderr
    assert len(received) == 1
    assert received[0]["bridge_protocol_version"] == 46
    evidence = list((tmp_path / "evidence").glob("*.jsonl"))
    rows = [json.loads(line) for line in evidence[0].read_text().splitlines()]
    response = next(row["response"] for row in rows if row["kind"] == "tcp_response")
    assert response["success"] is True and response["value"]["schema_version"] == 35


@pytest.fixture
def probe(monkeypatch):
    """Only replace Live and the ControlSurface base at their external boundary."""
    framework = ModuleType("_Framework")
    controls = ModuleType("_Framework.ControlSurface")
    controls.ControlSurface = object
    monkeypatch.setitem(sys.modules, "_Framework", framework)
    monkeypatch.setitem(sys.modules, "_Framework.ControlSurface", controls)
    live = ModuleType("Live")
    live.Track = SimpleNamespace(Track=SimpleNamespace(monitoring_states=SimpleNamespace(OFF=71)))
    monkeypatch.setitem(sys.modules, "Live", live)
    module = load_module("qualification_probe_test", KIT / "SunnyHostProbe" / "probe.py")
    instance = module.HostProbe.__new__(module.HostProbe)
    instance._constructor_thread = threading.get_ident()
    instance._document_song = None
    instance._document_token = None
    return instance


def scratch_song(name="SUNNY_HOST_QUALIFICATION_LITERAL"):
    """Create a document identity witness with stopped transport and recording."""

    class SongWitness:
        pass

    song = SongWitness()
    song.name = name
    song.is_playing = False
    song.session_record = False
    song.record_mode = False
    song.tracks = [SimpleNamespace(current_monitoring_state=71)]
    return song


def authorization(probe, song):
    """Bind explicit scratch approval to one observed document and exact name."""
    return {
        "scratch_approved": True,
        "expected_set_name": song.name,
        "expected_document_token": probe._observe_document(song),
    }


def test_mutation_gate_binds_exact_observed_document_even_if_name_is_reused(probe):
    """A replacement with the same/prefixed name cannot inherit the old approval."""
    original = scratch_song()
    current = [original]
    probe.song = lambda: current[0]
    request = authorization(probe, original)
    assert probe._scratch(request) is original
    for replacement in (scratch_song(), scratch_song("SUNNY_HOST_QUALIFICATION_OTHER")):
        current[0] = replacement
        with pytest.raises(RuntimeError, match="exact observed"):
            probe._scratch(request)
        assert not hasattr(replacement, "changed")
    current[0] = original
    with pytest.raises(RuntimeError, match="exact observed"):
        probe._scratch(request)


@pytest.mark.parametrize("flag", ["is_playing", "session_record", "record_mode"])
def test_mutation_gate_refuses_transport_or_recording(probe, flag):
    """Every native mutation is admitted only in the stopped scratch document."""
    song = scratch_song()
    probe.song = lambda: song
    request = authorization(probe, song)
    setattr(song, flag, True)
    with pytest.raises(RuntimeError, match="Stop transport"):
        probe._scratch(request)


def test_missing_approval_token_or_exact_name_is_refused(probe):
    """Prefix and a public approval boolean alone never suffice."""
    song = scratch_song()
    probe.song = lambda: song
    request = authorization(probe, song)
    for key in ("scratch_approved", "expected_set_name", "expected_document_token"):
        invalid = dict(request)
        del invalid[key]
        with pytest.raises(RuntimeError, match="exact observed"):
            probe._scratch(invalid)


def test_wrong_callback_thread_is_refused_before_any_live_access(probe):
    """A callback thread mismatch must not inspect even one native property."""
    probe._constructor_thread = -1

    def forbidden_song():
        raise AssertionError("Live was accessed on the wrong thread")

    probe.song = forbidden_song
    with pytest.raises(RuntimeError, match="Wrong callback thread"):
        probe._dispatch({"op": "note_shapes"})


@pytest.mark.parametrize("state", [0, 1, 2, None])
def test_monitoring_requires_the_actual_native_off_enum(probe, state):
    """No guessed numeric OFF value permits monitored or unobserved input."""
    song = scratch_song()
    probe.song = lambda: song
    request = authorization(probe, song)
    song.tracks[0].current_monitoring_state = state
    with pytest.raises(RuntimeError, match="input monitoring"):
        probe._scratch(request)


def test_missing_monitoring_observation_does_not_grant_mutation(probe):
    """A native getter error must remain evidence of an unmet precondition."""
    song = scratch_song()
    probe.song = lambda: song
    request = authorization(probe, song)
    del song.tracks[0].current_monitoring_state
    with pytest.raises(RuntimeError, match="input monitoring"):
        probe._scratch(request)
