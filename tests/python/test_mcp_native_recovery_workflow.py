"""Explicit current-object recovery through actual product, TCP, epoch change and fences.

Provider document replacement models the genuine external reopen boundary. It
cannot qualify Live's actual Set persistence or private Python identity behavior.
"""

from __future__ import annotations

import copy
import json

from live_model import Clip
from test_mcp_native_device_workflow import (
    _source,
    _timbre,
)
from test_mcp_native_device_workflow import (
    device_workflow as device_workflow,
)
from test_mcp_note_population_workflow import (
    _events,
    _insert,
)
from test_mcp_note_population_workflow import (
    population_workflow as population_workflow,
)
from test_mcp_realization_workflow import _author, _literal_notes, _ok, _realize, _revision


def _replace_provider_document(workflow):
    # Preinitialize external Clip copies: the source-contract model's version
    # gates run on every attribute read, including deepcopy's uninitialized
    # __dict__ lookup. This copies provider state; it invokes no bridge write
    # and supplies no evidence about actual Live Set serialization.
    memo = {}
    clips = []
    for track in workflow.live.song.tracks:
        for slot in track.clip_slots:
            if slot.clip is None:
                continue
            original = slot.clip
            replacement = Clip(
                original.end_marker,
                original._live_version,
                python_envelope_api=original._python_envelope_api,
            )
            memo[id(original)] = replacement
            clips.append((original, replacement))
    reopened = copy.deepcopy(workflow.live.song, memo)
    for original, replacement in clips:
        vars(replacement).update(copy.deepcopy(vars(original), memo))
    workflow.live.song = reopened
    workflow.live.application._document = reopened
    current = reopened.tracks[1].clip_slots[0].clip
    # The independent reopened provider assigns different current native IDs;
    # neither saved receipts nor semantic note equality can restore old IDs.
    current._notes = {old + 100: note for old, note in current._notes.items()}
    for note_id, note in current._notes.items():
        note.note_id = note_id
    current._next_note_id += 100
    return reopened.tracks[1], current


def _preview(client):
    return client.call(
        "project_realization_preview_adoption",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        selector={"track_index": 1, "slot_index": 0},
    )


def _adopt(client, preview, explicit=True):
    return client.call(
        "project_realization_adopt",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        preview=preview,
        explicit_adoption=explicit,
    )


def test_fresh_current_adoption_retains_unknown_epoch_history_and_revises_new_ids(
    request,
):
    """A lost old create reply remains unknown; explicit current adoption grants101/102 only."""
    workflow = request.getfixturevalue("population_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        workflow.drop_next_reply = "sunny_managed_create_clip"
        uncertain = _realize(client, "project_realization_create")
        assert uncertain["receipt"]["outcome"] == "indeterminate"
    current_track, current_clip = _replace_provider_document(workflow)
    current_clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    with workflow.process() as restarted:
        unknown = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert unknown["receipt"]["outcome"] == "unknown_epoch"
        blocked = _realize(restarted, "project_realization_update")
        assert blocked["state"] == "reconciliation_required"
        preview = _preview(restarted)
        assert preview["success"] and preview["eligible_for_explicit_adoption"], preview
        assert preview["authority_granted"] is False and preview["mutation_dispatched"] is False
        assert preview["historical_native_identity_restored"] is False
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        unapproved = _adopt(restarted, preview["preview"], explicit=False)
        assert unapproved["success"] is False
        forged = copy.deepcopy(preview["preview"])
        forged["observation"]["note_identity"]["notes"][0]["note_id"] = 500
        assert _adopt(restarted, forged)["success"] is False
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        adopted = _adopt(restarted, preview["preview"])
        assert adopted["success"] and adopted["history_saved"], adopted
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert adopted["historical_native_identity_restored"] is False
        assert adopted["earlier_attempt_history_retained"] is True
        assert _literal_notes(current_clip) == [
            (101, 60, 0.0, 1.0, 80.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        _ok(
            restarted,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1},
            interval={"chromatic": 2, "diatonic": 1},
        )
        revised = _realize(restarted, "project_realization_update")
        assert revised["success"], revised
        assert _literal_notes(current_clip) == [
            (101, 62, 0.0, 1.0, 80.0),
            (102, 69, 2.0, 0.5, 72.0),
        ]
        g_event = _events(restarted)[1]
        _ok(restarted, "score_delete_event", score_id=1, event_id=g_event["id"])
        _insert(restarted, "E", {"n": 1, "d": 4}, {"n": 1, "d": 8})
        population = _realize(restarted, "project_realization_update")
        assert population["success"], population
        assert _literal_notes(current_clip) == [
            (101, 62, 0.0, 1.0, 80.0),
            (103, 64, 1.0, 0.5, 76.0),
        ]
        retained = _ok(restarted, "project_realization_inspect", attempt_id=uncertain["attempt_id"])
        assert retained["receipt"]["outcome"] == "unknown_epoch"
    assert workflow.native_apply_calls == [(101, 102)]
    assert workflow.live.song.tracks[1] is current_track
    assert current_clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_clip") == 1


def test_same_epoch_native_note_drift_needs_authored_match_and_explicit_refresh(
    request,
):
    """Accepting native C1 velocity64 requires matching Score and explicit approval."""
    workflow = request.getfixturevalue("population_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        created = _realize(client, "project_realization_create")
        assert created["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        clip._notes[1].velocity = 64.0
        mismatch = _preview(client)
        assert mismatch["success"] and mismatch["eligible_for_explicit_adoption"] is False
        before = json.loads(workflow.history_ledger().read_text())["attempts"]
        declined = _adopt(client, mismatch["preview"])
        assert declined["success"] is False and "authored Score" in declined["error"]
        assert json.loads(workflow.history_ledger().read_text())["attempts"] == before
        c_event = _events(client)[0]
        _ok(client, "score_modify_note", score_id=1, event_id=c_event["id"], velocity=64)
        matched = _preview(client)
        assert matched["eligible_for_explicit_adoption"]
        adopted = _adopt(client, matched["preview"])
        assert (
            adopted["success"] and adopted["receipt"]["journal"]["native_mutation_started"] is False
        )
        _ok(client, "score_modify_note", score_id=1, event_id=c_event["id"], velocity=90)
        assert _realize(client, "project_realization_update")["success"]
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 90.0), (2, 67, 2.0, 0.5, 72.0)]
    assert workflow.native_apply_calls == [(1,)]
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1


def test_current_state_change_after_preview_declines_adoption_without_refresh(request):
    """A preview is an exact proposed boundary; later native edits cannot slip through approval."""
    workflow = request.getfixturevalue("population_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        clip = workflow.live.song.tracks[1].clip_slots[0].clip
        preview = _preview(client)
        assert preview["eligible_for_explicit_adoption"]
        clip._notes[1].velocity = 63.0
        rejected = _adopt(client, preview["preview"])
        assert rejected["success"] is False and rejected["receipt"]["outcome"] == "declined", (
            rejected
        )
        assert rejected["receipt"]["journal"]["native_mutation_started"] is False
        assert clip._notes[1].velocity == 63.0
        assert not workflow.native_apply_calls
        assert _realize(client, "project_realization_update")["success"] is False


def test_epoch_reopen_adopts_clip_then_current_drift_controls_without_native_writes(
    request,
):
    """Fresh native objects/IDs require separate exact Clip and source-device approval."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        selections = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        assert _timbre(client, selections)["success"]
        _ok(client, "workspace_save", path=workflow.server_workspace)
    current_track, clip = _replace_provider_document(workflow)
    native = current_track._devices[0]
    writes = [list(parameter.writes) for parameter in native.parameters]
    with workflow.process() as restarted:
        preview = _preview(restarted)
        assert preview["eligible_for_explicit_adoption"], preview
        adopted = _adopt(restarted, preview["preview"])
        assert adopted["success"], adopted
        preserved = _timbre(restarted, selections)
        assert preserved["success"] is False and "adopt" in preserved["error"]
        device_preview = _timbre(
            restarted, selections, "project_realization_preview_device_adoption"
        )
        assert device_preview["success"] and device_preview["authority_granted"] is False, (
            device_preview
        )
        device_adopted = _timbre(
            restarted,
            selections,
            "project_realization_adopt_devices",
            preview=device_preview["preview"],
            explicit_adoption=True,
        )
        assert device_adopted["success"] and device_adopted["history_saved"], device_adopted
        assert device_adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert [list(parameter.writes) for parameter in native.parameters] == writes
        _ok(restarted, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2450.0)
        revised = _timbre(restarted, selections)
        assert revised["success"] and revised["source_insertion_requested"] is False, revised
        assert native.parameters[1].value == 0.75
        assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 80.0), (102, 67, 2.0, 0.5, 72.0)]
    assert current_track._devices[0] is native and len(workflow.native_device_insertions) == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_devices") == 1


def test_empty_clip_adoption_requires_separate_device_grant_even_after_note_ack(request):
    """Empty capture metadata cannot grant source insertion rights after Clip adoption."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        selections = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        _ok(client, "workspace_save", path=workflow.server_workspace)
    track, clip = _replace_provider_document(workflow)
    with workflow.process() as restarted:
        preview = _preview(restarted)
        assert preview["eligible_for_explicit_adoption"], preview
        assert _adopt(restarted, preview["preview"])["success"]
        declined = _timbre(restarted, selections)
        assert declined["success"] is False and declined["receipt"]["outcome"] == "declined", (
            declined
        )
        assert not track._devices and not workflow.native_device_insertions
        c_event = _events(restarted)[0]
        _ok(restarted, "score_modify_note", score_id=1, event_id=c_event["id"], velocity=90)
        assert _realize(restarted, "project_realization_update")["success"]
        declined_after_ack = _timbre(restarted, selections)
        assert declined_after_ack["success"] is False
        assert declined_after_ack["receipt"]["outcome"] == "declined", declined_after_ack
        assert not track._devices and not workflow.native_device_insertions
        device_preview = _timbre(
            restarted, selections, "project_realization_preview_device_adoption"
        )
        assert (
            device_preview["success"] and device_preview["preview"]["preview"]["devices"] == []
        ), device_preview
        granted = _timbre(
            restarted,
            selections,
            "project_realization_adopt_devices",
            preview=device_preview["preview"],
            explicit_adoption=True,
        )
        assert (
            granted["success"] and granted["receipt"]["journal"]["native_mutation_started"] is False
        ), granted
        assert not track._devices
        inserted = _timbre(restarted, selections)
        assert inserted["success"] and inserted["source_insertion_requested"] is True, inserted
        assert len(track._devices) == 1 and track._devices[0].parameters[1].value == 0.5
        assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 90.0), (102, 67, 2.0, 0.5, 72.0)]
    assert len(workflow.native_device_insertions) == 1
