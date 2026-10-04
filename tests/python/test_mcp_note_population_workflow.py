"""Literal attack addition/deletion through owning documents, disk fences and native APIs."""

from __future__ import annotations

import json

import pytest
from test_mcp_realization_workflow import NativeWorkflow, _author, _literal_notes, _ok, _realize


@pytest.fixture
def population_workflow(tmp_path, monkeypatch):
    """Run the actual MCP/TCP stack with the existing source-contract native provider."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    try:
        yield workflow
    finally:
        workflow.close()


def _events(client):
    score = _ok(client, "score_get_json", score_id=1)
    return [
        event
        for event in score["parts"][0]["measures"][0]["voices"][0]["events"]
        if event["type"] == "note_group"
    ]


def _insert(client, letter, offset, duration, velocity=76):
    return _ok(
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


def test_population_phases_keep_retained_ids_and_restart_associations(population_workflow):
    """Delete G2, add E3, then delete E3/change C1/add A4; unchanged C1 keeps opaque state."""
    workflow = population_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        track = workflow.live.song.tracks[1]
        slot, clip = track.clip_slots[0], track.clip_slots[0].clip
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        c_event, g_event = _events(client)
        _ok(client, "score_delete_event", score_id=1, event_id=g_event["id"])
        _insert(client, "E", {"n": 1, "d": 4}, {"n": 1, "d": 8})
        first = _realize(client, "project_realization_update")
        assert first["success"] and first["history_saved"], first
        assert first["receipt"]["request"]["name"] == "sunny_managed_revise_note_population"
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (3, 64, 1.0, 0.5, 76.0)]
        assert not workflow.native_apply_calls
        e_event = next(event for event in _events(client) if event["id"] != c_event["id"])
        _ok(client, "score_delete_event", score_id=1, event_id=e_event["id"])
        _ok(client, "score_modify_note", score_id=1, event_id=c_event["id"], velocity=90)
        _insert(client, "A", {"n": 3, "d": 4}, {"n": 1, "d": 8}, 88)
        second = _realize(client, "project_realization_update")
        assert second["success"] and second["history_saved"], second
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 90.0), (4, 69, 3.0, 0.5, 88.0)]
        assert workflow.native_apply_calls == [(1,)]
        supplement = second["receipt"]["journal"]["result"]["note_population_update"]
        assert supplement["returned_added_note_ids"] == [4]
        assert [entry["note_id"] for entry in supplement["addition_associations"]] == [4]
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        unchanged = _realize(restarted, "project_realization_update")
        assert unchanged["success"] and unchanged["state"] == "unchanged_desired_notes", unchanged
        assert unchanged["mutation_dispatched"] is False
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 90.0), (4, 69, 3.0, 0.5, 88.0)]
        retained = _ok(restarted, "project_realization_inspect", attempt_id=second["attempt_id"])
        assert retained["receipt"] == second["receipt"]
    assert track.clip_slots[0] is slot and slot.clip is clip
    assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
    assert clip._user_follow_actions == {"next": True}
    assert workflow.managed_calls.count("sunny_managed_revise_note_population") == 2
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1


def test_population_lost_reply_reconciles_original_token_without_addition_replay(
    population_workflow,
):
    """The external socket loses the returned mutation reply; restart only queries its journal."""
    workflow = population_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        _insert(client, "E", {"n": 1, "d": 4}, {"n": 1, "d": 8})
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_revise_note_population"
        uncertain = _realize(client, "project_realization_update")
        assert uncertain["success"] is False and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        assert _literal_notes(clip) == [
            (1, 60, 0.0, 1.0, 80.0),
            (2, 67, 2.0, 0.5, 72.0),
            (3, 64, 1.0, 0.5, 76.0),
        ]
        blocked = _realize(client, "project_realization_update")
        assert blocked["state"] == "reconciliation_required"
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert blocked["mutation_dispatched"] is False
    with workflow.process() as restarted:
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["mutation_retried"] is False
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert (
            _realize(restarted, "project_realization_update")["state"] == "unchanged_desired_notes"
        )
    assert workflow.managed_calls.count("sunny_managed_revise_note_population") == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1
    assert len(clip._notes) == 3 and len(workflow.live.song.tracks) == 2


def test_population_intermediate_mismatch_stops_before_later_phases(
    population_workflow,
    monkeypatch,
):
    """A native removal that returns but preserves G2 stops before changing C1 or adding E3."""
    workflow = population_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        c_event, g_event = _events(client)
        _ok(client, "score_delete_event", score_id=1, event_id=g_event["id"])
        _ok(client, "score_modify_note", score_id=1, event_id=c_event["id"], velocity=90)
        _insert(client, "E", {"n": 1, "d": 4}, {"n": 1, "d": 8})
        removed = []
        monkeypatch.setattr(clip, "remove_notes_by_id", lambda ids: removed.append(tuple(ids)))
        partial = _realize(client, "project_realization_update")
        assert partial["success"] is False and partial["history_saved"], partial
        assert partial["receipt"]["outcome"] == "indeterminate"
        assert removed == [(2,)] and not workflow.native_apply_calls
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        blocked = _realize(client, "project_realization_update")
        assert (
            blocked["attempt_id"] == partial["attempt_id"]
            and blocked["mutation_dispatched"] is False
        )
        assert removed == [(2,)]


def test_existing_chord_cardinality_change_is_declined_before_dispatch(population_workflow):
    """Appending a chord member cannot transfer a retained ordinal's unknown expression."""
    workflow = population_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        _insert(client, "E", {"n": 0, "d": 1}, {"n": 1, "d": 4})
        declined = _realize(client, "project_realization_update")
        assert (
            declined["success"] is False
            and "UnsupportedChordCardinalityRevision" in declined["error"]
        )
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
    assert workflow.managed_calls.count("sunny_managed_revise_note_population") == 0


@pytest.mark.parametrize(
    ("version", "admitted"),
    [
        ((12, 3, 0), True),
        ((12, 3, 99), True),
        ((12, 4, 0), True),
        ((12, 4, 99), True),
        ((12, 5, 0), False),
        ((13, 0, 0), False),
        ((11, 3, 0), False),
    ],
)
def test_managed_product_version_matrix_rejects_unreviewed_versions_before_fence(
    population_workflow,
    version,
    admitted,
):
    """A permissive provider's available methods cannot qualify an unknown future version."""
    workflow = population_workflow
    workflow.live.application._version = version
    with workflow.process() as client:
        _author(client, workflow)
        result = _realize(client, "project_realization_create")
        if admitted:
            assert result["success"], result
            assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
            assert len(workflow.live.song.tracks) == 2
        else:
            assert result["success"] is False and "reviewed Live" in result["error"], result
            assert json.loads(workflow.history_ledger().read_text())["attempts"] == []
            assert workflow.managed_calls.count("sunny_managed_create_clip") == 0
            assert len(workflow.live.song.tracks) == 1
