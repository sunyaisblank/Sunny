"""Owning clip geometry phases through the real fenced MCP/TCP/native boundary."""

from __future__ import annotations

import json

import pytest
from live_model import Clip
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_realization_workflow import NativeWorkflow, _author, _literal_notes, _ok, _realize


@pytest.fixture
def geometry_workflow(tmp_path, monkeypatch):
    """Retain one actual bridge while restarting the owning product process."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    try:
        yield workflow
    finally:
        workflow.close()


def test_extent_population_phases_preserve_ids_and_reopen(geometry_workflow):
    """Extend4→8, add D at4, delete D, shrink8→4 without retiming retained C/G."""
    workflow = geometry_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        track = workflow.live.song.tracks[1]
        clip = track.clip_slots[0].clip
        original_notes = tuple(clip._notes.values())
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        extended = _realize(client, "project_realization_update_geometry")
        assert extended["success"] and extended["history_saved"], extended
        assert clip.end_marker == 8.0 and clip.loop_end == 4.0
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        _ok(
            client,
            "score_insert_note",
            score_id=1,
            part_id=1,
            bar=2,
            offset={"n": 0, "d": 1},
            pitch={"letter": "D", "accidental": 0, "octave": 4},
            duration={"n": 1, "d": 4},
            velocity=76,
        )
        added = _realize(client, "project_realization_update")
        assert added["success"], added
        assert _literal_notes(clip) == [
            (1, 60, 0.0, 1.0, 80.0),
            (2, 67, 2.0, 0.5, 72.0),
            (3, 62, 4.0, 1.0, 76.0),
        ]
        score = _ok(client, "score_get_json", score_id=1)
        d_event = next(
            event
            for event in score["parts"][0]["measures"][1]["voices"][0]["events"]
            if event["type"] == "note_group"
        )
        _ok(client, "score_delete_event", score_id=1, event_id=d_event["id"])
        deleted = _realize(client, "project_realization_update")
        assert deleted["success"], deleted
        _ok(client, "score_delete_measures", score_id=1, bar=2, count=1)
        shrunk = _realize(client, "project_realization_update_geometry")
        assert shrunk["success"] and shrunk["history_saved"], shrunk
        assert track.clip_slots[0].clip is clip and clip.end_marker == 4.0
        assert tuple(clip._notes.values()) == original_notes
        assert all(clip._notes[note.note_id] is note for note in original_notes)
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
        assert not workflow.native_apply_calls
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        unchanged = _realize(restarted, "project_realization_update_geometry")
        assert unchanged["success"] and unchanged["state"] == "unchanged_desired_geometry"
        assert not unchanged["mutation_dispatched"]
        retained = _ok(restarted, "project_realization_inspect", attempt_id=shrunk["attempt_id"])
        assert retained["receipt"] == shrunk["receipt"]
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
    assert workflow.managed_calls.count("sunny_managed_update_clip_geometry") == 2
    assert workflow.managed_calls.count("sunny_managed_revise_note_population") == 2


def test_lost_geometry_reply_queries_original_after_restart(geometry_workflow):
    """An actual completed extension remains fenced and is never repeated after loss."""
    workflow = geometry_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_update_clip_geometry"
        uncertain = _realize(client, "project_realization_update_geometry")
        assert not uncertain["success"] and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate" and clip.end_marker == 8.0
        blocked = _realize(client, "project_realization_update_geometry")
        assert blocked["state"] == "reconciliation_required"
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert not blocked["mutation_dispatched"]
    with workflow.process() as restarted:
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert not reconciled["mutation_retried"]
        unchanged = _realize(restarted, "project_realization_update_geometry")
        assert unchanged["success"] and not unchanged["mutation_dispatched"]
        assert workflow.live.song.tracks[1].clip_slots[0].clip is clip
        assert clip.end_marker == 8.0
    assert workflow.managed_calls.count("sunny_managed_update_clip_geometry") == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_lost_geometry_reply_can_query_immediately_on_same_mcp_session(geometry_workflow):
    """The original-token query reconnects before any other request can reconnect it."""
    workflow = geometry_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        workflow.drop_next_reply = "sunny_managed_update_clip_geometry"
        uncertain = _realize(client, "project_realization_update_geometry")
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        actual = _ok(client, "project_realization_reconcile", attempt_id=uncertain["attempt_id"])
        assert actual["query_succeeded"] and actual["history_saved"]
        assert actual["actual_receipt"]["outcome"] == "acknowledged"
        assert not actual["mutation_retried"] and clip.end_marker == 8.0
        assert workflow.managed_calls.count("sunny_managed_update_clip_geometry") == 1
        assert workflow.managed_calls.count("sunny_managed_operation") == 1


def test_combined_note_and_geometry_edit_declines_before_fence(geometry_workflow):
    """Changing notes and extent together cannot silently create hidden intermediate intent."""
    workflow = geometry_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        rejected = _realize(client, "project_realization_update_geometry")
        assert not rejected["success"] and "GeometryOnlyRevisionRequired" in rejected["error"]
        assert len(json.loads(workflow.history_ledger().read_text())["attempts"]) == 1
        assert "sunny_managed_update_clip_geometry" not in workflow.managed_calls
        assert clip.end_marker == 4.0
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]


def test_provider_document_replacement_requires_four_domain_adoption_before_extension(
    geometry_workflow, monkeypatch
):
    """External provider replacement models a new epoch, without proving Live persistence."""
    workflow = geometry_workflow
    with workflow.process() as client:
        _author(client, workflow)
        created = _realize(client, "project_realization_create")
        assert created["success"] and created["history_saved"], created
        previous_track = workflow.live.song.tracks[1]
        previous_clip = previous_track.clip_slots[0].clip
        _ok(client, "workspace_save", path=workflow.server_workspace)

    current_track, current_clip = _replace_provider_document(workflow)
    assert current_track is not previous_track and current_clip is not previous_clip
    assert _literal_notes(current_clip) == [
        (101, 60, 0.0, 1.0, 80.0),
        (102, 67, 2.0, 0.5, 72.0),
    ]
    current_notes = dict(current_clip._notes)
    current_clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    current_clip._user_follow_actions = {"next": True}
    setters = []
    for name in ("end_marker", "signature_numerator", "signature_denominator"):
        native_property = Clip.__dict__[name]

        def setter(clip, value, name=name, native_property=native_property):
            setters.append((clip, name, value))
            native_property.__set__(clip, value)

        monkeypatch.setattr(
            Clip,
            name,
            property(
                lambda clip, native_property=native_property: native_property.__get__(clip, Clip),
                setter,
            ),
        )

    with workflow.process() as restarted:
        attempts_before = json.loads(workflow.history_ledger().read_text())["attempts"]
        refused = _realize(restarted, "project_realization_update_geometry")
        assert refused["success"] is False and refused["mutation_dispatched"] is False, refused
        assert refused["state"] == "native_drift_or_observation_unavailable"
        assert setters == [] and current_clip.end_marker == 4.0
        assert "sunny_managed_update_clip_geometry" not in workflow.managed_calls
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == attempts_before

        preview = _preview(restarted)
        assert preview["success"] and preview["eligible_for_explicit_adoption"], preview
        assert preview["authority_granted"] is False and preview["mutation_dispatched"] is False
        approved = preview["preview"]
        domains = [
            "existing_note_updates",
            "note_population_updates",
            "clip_geometry_updates",
            "absent_mixer_step_lanes",
        ]
        assert approved["allowed_domains"] == domains
        assert (
            approved["context"]["document_token"] != created["receipt"]["context"]["document_token"]
        )
        assert [note["note_id"] for note in approved["observation"]["note_identity"]["notes"]] == [
            101,
            102,
        ]
        assert setters == []

        adopted = _adopt(restarted, approved)
        assert adopted["success"] and adopted["history_saved"], adopted
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        adoption = adopted["receipt"]["journal"]["result"]["adoption"]
        assert adoption["allowed_domains"] == domains
        assert adoption["approved_note_ids"] == [101, 102]
        assert adopted["historical_native_identity_restored"] is False
        assert adopted["earlier_attempt_history_retained"] is True
        assert setters == [] and not workflow.native_apply_calls

        _ok(restarted, "score_insert_measures", score_id=1, after_bar=1, count=1)
        extended = _realize(restarted, "project_realization_update_geometry")
        assert extended["success"] and extended["history_saved"], extended
        assert setters == [(current_clip, "end_marker", 8.0)]
        assert current_clip.end_marker == 8.0 and current_clip.loop_end == 4.0
        assert current_clip._notes == current_notes
        assert all(current_clip._notes[note_id] is note for note_id, note in current_notes.items())
        assert _literal_notes(current_clip) == [
            (101, 60, 0.0, 1.0, 80.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        assert previous_clip.end_marker == 4.0
        assert _literal_notes(previous_clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        _ok(restarted, "workspace_save", path=workflow.server_workspace)

    with workflow.process() as reopened_product:
        retained_create = _ok(
            reopened_product, "project_realization_inspect", attempt_id=created["attempt_id"]
        )
        assert retained_create["receipt"] == created["receipt"]
        assert retained_create["current_native_state_observed"] is False
        retained_adoption = _ok(
            reopened_product, "project_realization_inspect", attempt_id=adopted["attempt_id"]
        )
        assert retained_adoption["receipt"] == adopted["receipt"]
        assert retained_adoption["receipt"]["journal"]["result"]["adoption"]["allowed_domains"] == (
            domains
        )
        retained_extension = _ok(
            reopened_product, "project_realization_inspect", attempt_id=extended["attempt_id"]
        )
        assert retained_extension["receipt"] == extended["receipt"]
        unchanged = _realize(reopened_product, "project_realization_update_geometry")
        assert unchanged["success"] and unchanged["state"] == "unchanged_desired_geometry"
        assert unchanged["mutation_dispatched"] is False
        attempts = json.loads(workflow.history_ledger().read_text())["attempts"]
        assert {attempt["intent"]["attempt_id"] for attempt in attempts} == {
            created["attempt_id"],
            adopted["attempt_id"],
            extended["attempt_id"],
        }

    assert workflow.live.song.tracks[1] is current_track
    assert current_track.clip_slots[0].clip is current_clip
    assert current_clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
    assert current_clip._user_follow_actions == {"next": True}
    assert setters == [(current_clip, "end_marker", 8.0)] and not workflow.native_apply_calls
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_update_clip_geometry") == 1
