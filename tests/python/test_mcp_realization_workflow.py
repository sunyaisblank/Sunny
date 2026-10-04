"""Owning-project realization through the actual stdio and TCP implementation.

Only the external Live provider is modelled. Existing-note snapshots and apply
are supplied here rather than expanding the permissive global fake contract.
These workflows do not qualify a real Live host or saved/reopened native MPE.

SUNNY_NATIVE_MCP_COMMAND selects the normal container stdio process with
{workspace_dir} mounted at /data. SUNNY_TEST_BRIDGE_BIND_HOST/HOST select the
listener and outbound address; no port is published for the stdio MCP process.
"""

from __future__ import annotations

import copy
import json
import os
import shlex
import threading
import time
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import Clip, LiveSet, MidiNoteVector
from Sunny import surface as surface_module
from Sunny.build_identity import BRIDGE_SOURCE_SHA256
from Sunny.surface import SunnyControlSurface
from test_live_end_to_end import _LiveMainThread, _McpClient, _sunny_mcp_binary


class NativeWorkflow:
    """Keep one real bridge while restarting the durable product process."""

    def __init__(self, directory: Path, monkeypatch: pytest.MonkeyPatch) -> None:
        """Install only native provider boundaries and launch the real socket surface."""
        self.directory = directory
        self.workspace = directory / "workspace.json"
        configured = os.environ.get("SUNNY_NATIVE_MCP_COMMAND")
        self.command = (
            [token.format(workspace_dir=str(directory)) for token in shlex.split(configured)]
            if configured
            else None
        )
        self.server_workspace = "/data/workspace.json" if configured else str(self.workspace)
        self.client_host = os.environ.get("SUNNY_TEST_BRIDGE_HOST", "127.0.0.1")
        bind_host = os.environ.get("SUNNY_TEST_BRIDGE_BIND_HOST", "127.0.0.1")
        if configured:
            directory.chmod(0o777)  # The image's non-root user must create durable files.
        monkeypatch.setenv("SUNNY_WORKSPACE_PATH", self.server_workspace)
        monkeypatch.delenv("SUNNY_WORKSPACE_RECOVERY", raising=False)
        self.live = LiveSet((12, 4, 0), midi_tracks=1, python_envelope_api=True).install(
            monkeypatch
        )
        self.native_apply_calls: list[tuple[int, ...]] = []
        self.managed_calls: list[str] = []
        self.drop_next_reply: str | None = None
        self.main_thread = _LiveMainThread()

        def select(clip: Any, note_ids: Any) -> MidiNoteVector:
            assert threading.get_ident() == self.main_thread._thread.ident
            return MidiNoteVector(
                copy.deepcopy(clip._notes[i]) for i in note_ids if i in clip._notes
            )

        def apply(clip: Any, notes: Any) -> None:
            assert threading.get_ident() == self.main_thread._thread.ident
            assert isinstance(notes, MidiNoteVector)
            self.native_apply_calls.append(tuple(note.note_id for note in notes))
            for note in notes:
                native = clip._notes[note.note_id]
                for field in (
                    "pitch",
                    "start_time",
                    "duration",
                    "velocity",
                    "mute",
                    "probability",
                    "velocity_deviation",
                    "release_velocity",
                ):
                    setattr(native, field, getattr(note, field))

        monkeypatch.setattr(Clip, "get_notes_by_id", select)
        monkeypatch.setattr(Clip, "apply_note_modifications", apply, raising=False)
        monkeypatch.setattr(surface_module, "_server_configuration", lambda: (bind_host, 0))

        def song(surface: Any) -> Any:
            assert threading.get_ident() == self.main_thread._thread.ident
            # Pinned _Framework/ControlSurface.py:119-120 delegates to the
            # native c_instance. Supply that external boundary only here.
            return surface._c_instance.song()

        monkeypatch.setattr(SunnyControlSurface.__mro__[1], "song", song, raising=False)
        self.surface = SunnyControlSurface(SimpleNamespace(song=lambda: self.live.song))
        self.surface.schedule_message = self.main_thread.schedule_message
        assert self.surface._server._ready.wait(5), "actual TCP listener did not start"
        dispatch = self.surface._managed_registry.dispatch

        def managed(name: str, args: list[Any]) -> dict[str, Any]:
            assert threading.get_ident() == self.main_thread._thread.ident
            self.managed_calls.append(name)
            if name in {
                "sunny_managed_create_clip",
                "sunny_managed_update_notes",
                "sunny_managed_author_envelope",
            }:
                records = json.loads(self.history_ledger().read_text())["attempts"]
                retained = [
                    record
                    for record in records
                    if record["intent"]["attempt_id"] == args[0]["operation_id"]
                ]
                assert len(retained) == 1
                assert retained[0]["dispatch_state"] == "may_have_sent"
                assert retained[0]["evidence"] == [] and retained[0]["bindings"] == []
                assert retained[0]["intent"]["prepared"]["outcome"] == "prepared"
                assert retained[0]["intent"]["prepared"]["request"]["name"] == name
                assert retained[0]["intent"]["prepared"]["request"]["args"] == args
            return dispatch(name, args)

        monkeypatch.setattr(self.surface._managed_registry, "dispatch", managed)
        send = self.surface._server._send_frame

        def send_frame(sock: Any, payload: str) -> None:
            response = json.loads(payload)
            value = response.get("value")
            if (
                self.drop_next_reply is not None
                and isinstance(value, dict)
                and value.get("name") == self.drop_next_reply
            ):
                self.drop_next_reply = None
                # The actual handler has completed native mutation and retained
                # its journal. Lose only its response at the external socket.
                self.surface._server._close_socket(sock)
                return
            send(sock, payload)

        monkeypatch.setattr(self.surface._server, "_send_frame", send_frame)

    @contextmanager
    def process(self):
        """Restart the actual stdio application against the same workspace and bridge."""
        client = _McpClient(
            None if self.command else _sunny_mcp_binary(),
            self.surface._server.bound_port,
            host=self.client_host,
            command=self.command,
        )
        try:
            # Docker/WSL may make the route visible after listener startup.
            # Establish readiness only through bounded read-only product calls;
            # no native mutation is sent or retried while the route is pending.
            deadline = time.monotonic() + 10.0
            while True:
                readiness = client.call("get_ableton_session_state")
                if readiness.get("success") is True:
                    break
                assert time.monotonic() < deadline, readiness
                threading.Event().wait(0.1)
            yield client
        finally:
            client.close()

    def host_path(self, server_path: str | Path) -> Path:
        """Translate this fixture's container paths without altering product metadata."""
        path = Path(server_path)
        return self.directory / path.relative_to("/data") if self.command else path

    def history_ledger(self) -> Path:
        """Return the actual host ledger backing the immutable native receipt namespace."""
        metadata = json.loads(self.workspace.read_text())["native_realization"]
        return (
            self.host_path(metadata["history_base_directory"])
            / metadata["workspace_namespace"]
            / "ledger.json"
        )

    def close(self) -> None:
        """Release the actual listener and designated host scheduler."""
        self.surface.disconnect()
        self.main_thread.stop()


@pytest.fixture
def native_workflow(tmp_path, monkeypatch):
    """Supply one isolated native provider and exact matching bridge source bundle."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    try:
        yield workflow
    finally:
        workflow.close()


def _ok(client, name, **arguments):
    result = client.call(name, **arguments)
    assert "error" not in result and result.get("success") is not False, (name, result)
    return result


def _revision(client):
    return _ok(client, "get_project_json", score_id=1)["project"]["revision"]


def _realize(client, name):
    return client.call(name, score_id=1, part_id=1, expected_project_revision=_revision(client))


def _author(client, workflow):
    _ok(
        client,
        "score_create",
        title="Native owned phrase",
        total_bars=1,
        parts=[{"name": "Piano", "instrument_type": 47}],
    )
    _ok(client, "create_project", score_id=1)
    for letter, offset, duration, velocity in (
        ("C", {"n": 0, "d": 1}, {"n": 1, "d": 4}, 80),
        ("G", {"n": 1, "d": 2}, {"n": 1, "d": 8}, 72),
    ):
        _ok(
            client,
            "score_insert_note",
            score_id=1,
            part_id=1,
            bar=1,
            offset=offset,
            pitch={"letter": letter, "accidental": 0, "octave": 4},
            duration=duration,
            velocity=velocity,
        )
    state = _ok(client, "get_ableton_session_state")
    # The actual transport handshake verifies this exact bundle independently.
    assert state["target_profile"]["adapter"]["source_sha256"] == BRIDGE_SOURCE_SHA256
    assert state["success"] is True
    saved = _ok(client, "workspace_save", path=workflow.server_workspace)
    assert saved["durability_confirmed"] and saved["native_history_available"]


def _literal_notes(clip):
    return sorted(
        (note.note_id, note.pitch, note.start_time, note.duration, note.velocity)
        for note in clip._notes.values()
    )


def test_owned_native_revision_restart_undo_and_backup_preserve_fences(native_workflow):
    """Actual MCP authoring revisions retain native objects, IDs and durable history."""
    workflow = native_workflow
    original = workflow.live.song.tracks[0]
    original.name = "Unrelated user track"
    original.clip_slots[0].create_clip(8.0)
    with workflow.process() as client:
        _author(client, workflow)
        created = _realize(client, "project_realization_create")
        assert created["success"] and created["history_saved"], created
        create_id = created["attempt_id"]
        assert created["dispatch_fenced"] and created["retry_authorized"] is False
        owned = workflow.live.song.tracks[1]
        slot, clip = owned.clip_slots[0], owned.clip_slots[0].clip
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        repeated = _realize(client, "project_realization_create")
        assert repeated["success"] is False and repeated["state"] == "existing_attempt"
        assert len(workflow.live.song.tracks) == 2
        inspected = _ok(client, "project_realization_inspect", score_id=1, part_id=1)
        assert inspected["current_native_state_observed"] is True
        assert (
            inspected["native_observation"]["observation"]["note_identity"]["notes"][0]["note_id"]
            == 1
        )
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        _ok(
            client, "score_set_dynamics", score_id=1, region={"start_bar": 1, "end_bar": 1}, level=6
        )
        updated = _realize(client, "project_realization_update")
        assert updated["success"] and updated["history_saved"], updated
        update_id = updated["attempt_id"]
        assert update_id != create_id
        assert _literal_notes(clip) == [(1, 62, 0.0, 1.0, 104.0), (2, 69, 2.0, 0.5, 104.0)]
        assert workflow.native_apply_calls == [(1, 2)]
        assert owned.clip_slots[0] is slot and slot.clip is clip
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
        _ok(client, "workspace_save", path=workflow.server_workspace)
        _ok(client, "score_undo", score_id=1)
        undo_history = _ok(client, "project_realization_inspect", attempt_id=update_id)
        assert undo_history["attempt_id"] == update_id and undo_history["dispatch_fenced"]
        assert _literal_notes(clip)[0][1] == 62  # Authoring undo never mutates Live.
    with workflow.process() as restarted:
        durable = _ok(restarted, "project_realization_inspect", attempt_id=update_id)
        assert durable["current_native_state_observed"] is True
        assert durable["receipt"] == updated["receipt"]
        _ok(restarted, "project_realization_inspect", attempt_id=create_id)
        _ok(restarted, "workspace_recover_backup", path=workflow.server_workspace, apply=True)
        retained = _ok(restarted, "project_realization_inspect", attempt_id=update_id)
        assert retained["attempt_id"] == update_id and retained["dispatch_fenced"]
        repeated = _realize(restarted, "project_realization_create")
        assert repeated["success"] is False and repeated["state"] == "update_required"
        assert len(workflow.live.song.tracks) == 2 and slot.clip is clip
    assert original.name == "Unrelated user track" and original.clip_slots[0].clip.end_marker == 8.0
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_update_notes") == 1


def test_lost_native_creation_reply_reconciles_retained_token_without_recreation(native_workflow):
    """Restarted MCP reads the durable may-have-sent fence and only queries its journal."""
    workflow = native_workflow
    with workflow.process() as client:
        _author(client, workflow)
        workflow.drop_next_reply = "sunny_managed_create_clip"
        uncertain = _realize(client, "project_realization_create")
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        assert uncertain["receipt"]["delivery"] == "sent_without_valid_response"
        attempt_id = uncertain["attempt_id"]
        assert len(workflow.live.song.tracks) == 2
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        repeated = _realize(client, "project_realization_create")
        assert repeated["success"] is False and repeated.get("attempt_id") == attempt_id, repeated
    with workflow.process() as restarted:
        before = _ok(restarted, "project_realization_inspect", attempt_id=attempt_id)
        assert before["receipt"]["outcome"] == "indeterminate"
        reconciled = _ok(restarted, "project_realization_reconcile", attempt_id=attempt_id)
        assert reconciled["mutation_retried"] is False and reconciled["history_saved"]
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert reconciled["attempt_id"] == attempt_id
        assert workflow.live.song.tracks[1].clip_slots[0].clip is clip
        repeated = _realize(restarted, "project_realization_create")
        assert repeated["success"] is False and repeated["state"] == "existing_attempt"
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_native_user_drift_is_preserved_and_declined_history_does_not_erase_binding(
    native_workflow,
):
    """An explicit later candidate uses retained ownership after a proven no-effect decline."""
    workflow = native_workflow
    with workflow.process() as client:
        _author(client, workflow)
        created = _realize(client, "project_realization_create")
        assert created["success"], created
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        clip._notes[1].velocity = 64.0  # Independent user/provider edit.
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        declined = _realize(client, "project_realization_update")
        assert declined["success"] is False and declined["history_saved"], declined
        assert declined["receipt"]["outcome"] == "declined"
        assert declined["receipt"]["journal"]["native_mutation_started"] is False
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 64.0), (2, 67, 2.0, 0.5, 72.0)]
        inspected = _ok(client, "project_realization_inspect", attempt_id=declined["attempt_id"])
        assert (
            inspected["native_observation"]["observation"]["manifest"]["notes"][0]["velocity"]
            == 64.0
        )
        assert not workflow.native_apply_calls
        clip._notes[1].velocity = 80.0  # Restore the exact guarded boundary explicitly.
        updated = _realize(client, "project_realization_update")
        assert updated["success"] and updated["attempt_id"] != declined["attempt_id"], updated
        assert _literal_notes(clip) == [(1, 62, 0.0, 1.0, 80.0), (2, 69, 2.0, 0.5, 72.0)]
        assert workflow.native_apply_calls == [(1, 2)]
        retained = _ok(client, "project_realization_inspect", attempt_id=declined["attempt_id"])
        assert retained["receipt"]["outcome"] == "declined"
        _ok(client, "project_realization_inspect", attempt_id=created["attempt_id"])
        assert len(workflow.live.song.tracks) == 2
