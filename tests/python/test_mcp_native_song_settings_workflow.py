"""Set-wide approvals, scalar phases and uncertainty across real MCP/TCP joins.

Only native provider objects are modelled. Provider document replacement is
an external epoch boundary, not evidence of actual Live Set serialization.
"""

from __future__ import annotations

import json

import pytest
from live_model import DeviceParameter
from test_managed_song_settings import watch_setters
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_note_population_workflow import _events
from test_mcp_realization_workflow import (
    NativeWorkflow,
    _author,
    _literal_notes,
    _ok,
    _realize,
    _revision,
)


@pytest.fixture
def song_workflow(tmp_path, monkeypatch):
    """Expose the actual Main tempo parameter/loop roles required by production."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    song = workflow.live.song
    song.loop_start = 0.0
    song.loop_length = 8.0
    parameter = DeviceParameter("Song Tempo", minimum=20.0, maximum=999.0, value=120.0)
    parameter._canonical_parent = song.master_track.mixer_device
    song.master_track.mixer_device.song_tempo = parameter
    song.scenes[0].enable_launch_overrides(150.0, 7, 8)
    song.current_song_time = 4.0
    song.set_or_delete_cue()
    song.cue_points[0].name = "User cue"
    song.current_song_time = 0.0
    workflow.scalar_calls = watch_setters(monkeypatch)
    try:
        yield workflow
    finally:
        workflow.close()


def _small_meter_author(client, workflow):
    _author(client, workflow)
    _ok(client, "score_delete_event", score_id=1, event_id=_events(client)[1]["id"])
    _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[3], denominator=8)
    _tempo(client, 185, 2)
    _ok(client, "workspace_save", path=workflow.server_workspace)


def _tempo(client, numerator, denominator=1):
    _ok(
        client,
        "score_set_tempo_map",
        score_id=1,
        entries=[
            {
                "position": {"bar": 1, "beat_n": 0, "beat_d": 1},
                "bpm": {"n": numerator, "d": denominator},
                "beat_unit": "quarter",
                "transition": "immediate",
            }
        ],
    )


def _song_preview(client):
    result = _realize(client, "project_realization_preview_song_settings")
    assert result["success"], result
    assert result["all_tracks_affected"] and not result["authority_granted"]
    assert not result["mutation_dispatched"]
    return result["preview"]


def _song_apply(client, preview):
    return client.call(
        "project_realization_apply_song_settings",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        preview=preview,
        explicit_set_wide_approval=True,
    )


def _second_project(client):
    score = _ok(
        client,
        "score_create",
        title="Other owning project",
        total_bars=1,
        parts=[{"name": "Piano", "instrument_type": 47}],
    )
    assert score["score_id"] == 2
    _ok(client, "create_project", score_id=2)


def _second_create(client):
    owner = _ok(client, "get_project_json", score_id=2)["project"]
    return client.call(
        "project_realization_create",
        score_id=2,
        part_id=1,
        expected_project_revision=owner["revision"],
    )


def test_set_settings_preserve_native_part_foreign_state_and_noop_after_restart(song_workflow):
    """Literal92.5BPM/3/8 writes num→den→tempo; fresh strict noop starts no native write."""
    workflow = song_workflow
    foreign = workflow.live.song.tracks[0]
    foreign.clip_slots[0].create_clip(8.0)
    foreign_clip = foreign.clip_slots[0].clip
    with workflow.process() as client:
        _small_meter_author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        notes = tuple(clip._notes.values())
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0)]
        assert clip.end_marker == 1.5 and clip.signature_denominator == 8
        preview = _song_preview(client)
        assert not workflow.scalar_calls
        applied = _song_apply(client, preview)
        assert applied["success"] and applied["history_saved"], applied
        assert workflow.scalar_calls == [
            ("signature_numerator", 3),
            ("signature_denominator", 8),
            ("tempo", 92.5),
        ]
        assert applied["receipt"]["journal"]["native_mutation_started"]
        song = workflow.live.song
        assert (song.tempo, song.signature_numerator, song.signature_denominator) == (92.5, 3, 8)
        assert song.current_song_time == 0.0 and song.loop_start == 0.0 and song.loop_length == 8.0
        assert song.scenes[0].tempo == 150.0 and song.scenes[0].time_signature_numerator == 7
        assert song.scenes[0].time_signature_denominator == 8
        assert [(cue.name, cue.time) for cue in song.cue_points] == [("User cue", 4.0)]
        assert foreign.clip_slots[0].clip is foreign_clip
        assert workflow.live.song.tracks[1].clip_slots[0].clip is clip
        assert tuple(clip._notes.values()) == notes and not workflow.native_apply_calls
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        noop = _song_apply(restarted, _song_preview(restarted))
        assert noop["success"] and noop["history_saved"], noop
        assert noop["receipt"]["journal"]["native_mutation_started"] is False
        assert noop["receipt"]["journal"]["result"]["song_settings"]["started_fields"] == []
        assert len(workflow.scalar_calls) == 3
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0)]
    assert workflow.managed_calls.count("sunny_managed_apply_song_settings") == 2


def test_lost_set_reply_blocks_other_project_until_original_query_ack(song_workflow):
    """A Set-wide unknown fences all owning projects; read-only query never retries scalars."""
    workflow = song_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        _second_project(client)
        _tempo(client, 185, 2)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_apply_song_settings"
        uncertain = _song_apply(client, _song_preview(client))
        assert not uncertain["success"] and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        assert workflow.live.song.tempo == 92.5 and workflow.scalar_calls == [("tempo", 92.5)]
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        blocked = _second_create(client)
        assert blocked["state"] == "set_wide_reconciliation_required", blocked
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert not blocked["mutation_dispatched"]
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
    with workflow.process() as restarted:
        actual = _ok(restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"])
        assert actual["query_succeeded"] and actual["receipt"]["outcome"] == "acknowledged"
        assert not actual["mutation_retried"]
        created = _second_create(restarted)
        assert created["success"], created
        assert len(workflow.live.song.tracks) == 3
        assert workflow.scalar_calls == [("tempo", 92.5)]
    assert workflow.managed_calls.count("sunny_managed_apply_song_settings") == 1


def test_new_epoch_adoption_does_not_clear_set_barrier_fresh_approval_does(song_workflow):
    """Zero-setter Clip adoption permits fresh Set approval without erasing old uncertainty."""
    workflow = song_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        _second_project(client)
        _tempo(client, 185, 2)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_apply_song_settings"
        old = _song_apply(client, _song_preview(client))
        assert old["receipt"]["outcome"] == "indeterminate"
    current_track, current_clip = _replace_provider_document(workflow)
    with workflow.process() as restarted:
        unknown = _ok(restarted, "project_realization_reconcile", attempt_id=old["attempt_id"])
        assert unknown["receipt"]["outcome"] == "unknown_epoch"
        adopted = _adopt(restarted, _preview(restarted)["preview"])
        assert adopted["success"] and not adopted["receipt"]["journal"]["native_mutation_started"]
        assert _second_create(restarted)["state"] == "set_wide_reconciliation_required"
        approved = _song_apply(restarted, _song_preview(restarted))
        assert approved["success"], approved
        assert not approved["receipt"]["journal"]["native_mutation_started"]
        assert _second_create(restarted)["success"]
        retained = _ok(restarted, "project_realization_inspect", attempt_id=old["attempt_id"])
        assert retained["receipt"]["outcome"] == "unknown_epoch"
        assert workflow.live.song.tracks[1] is current_track
        assert _literal_notes(current_clip) == [
            (101, 60, 0.0, 1.0, 80.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        _tempo(restarted, 90)
        workflow.drop_next_reply = "sunny_managed_apply_song_settings"
        later = _song_apply(restarted, _song_preview(restarted))
        assert later["receipt"]["outcome"] == "indeterminate"
        _ok(restarted, "score_insert_measures", score_id=1, after_bar=1, count=1)
        blocked = _realize(restarted, "project_realization_update_geometry")
        assert blocked["state"] == "set_wide_reconciliation_required", blocked
        assert blocked["attempt_id"] == later["attempt_id"]
        assert current_clip.end_marker == 4.0
    assert workflow.scalar_calls == [("tempo", 92.5), ("tempo", 90.0)]
    assert workflow.managed_calls.count("sunny_managed_apply_song_settings") == 3


def test_lost_set_reply_refuses_workspace_namespace_switch_until_original_query_ack(song_workflow):
    """Opening another saved namespace cannot discard this process's Set-wide uncertainty."""
    workflow = song_workflow
    other_server_path = workflow.server_workspace.replace("workspace.json", "other-workspace.json")
    # Save an independent namespace without creating the fixture's default workspace.
    with workflow.process() as other:
        _ok(other, "workspace_save", path=other_server_path)
    other_metadata = json.loads(workflow.host_path(other_server_path).read_text())[
        "native_realization"
    ]
    assert not workflow.workspace.exists()
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        native_track = workflow.live.song.tracks[1]
        native_clip = native_track.clip_slots[0].clip
        _tempo(client, 185, 2)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        active_metadata = json.loads(workflow.workspace.read_text())["native_realization"]
        assert active_metadata["workspace_namespace"] != other_metadata["workspace_namespace"]
        workflow.drop_next_reply = "sunny_managed_apply_song_settings"
        uncertain = _song_apply(client, _song_preview(client))
        assert uncertain["receipt"]["outcome"] == "indeterminate", uncertain
        assert workflow.scalar_calls == [("tempo", 92.5)]
        history = workflow.history_ledger()
        before_ledger = history.read_bytes()
        before_project = _ok(client, "get_project_json", score_id=1)["project"]
        blocked = client.call("workspace_open", path=other_server_path)
        assert blocked.get("success") is False, blocked
        assert "Set-wide reconciliation required" in blocked["error"]
        assert uncertain["attempt_id"] in blocked["error"]
        assert history.read_bytes() == before_ledger
        assert _ok(client, "get_project_json", score_id=1)["project"] == before_project
        # Same-namespace authoring reopen preserves the original query namespace.
        _ok(client, "workspace_open", path=workflow.server_workspace)
        actual = _ok(client, "project_realization_reconcile", attempt_id=uncertain["attempt_id"])
        assert actual["query_succeeded"] and actual["receipt"]["outcome"] == "acknowledged"
        assert not actual["mutation_retried"]
        assert workflow.scalar_calls == [("tempo", 92.5)]
        original_tokens = json.loads(history.read_text())["attempts"]
        _ok(client, "workspace_open", path=other_server_path)
        assert json.loads(history.read_text())["attempts"] == original_tokens
        assert workflow.live.song.tracks[1] is native_track
        assert native_track.clip_slots[0].clip is native_clip
        assert _literal_notes(native_clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
    assert workflow.managed_calls.count("sunny_managed_apply_song_settings") == 1
