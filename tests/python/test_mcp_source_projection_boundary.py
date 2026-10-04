"""Source no-op and setters require the complete current owning Clip projection.

The product and bridge are real; only external native objects are modelled.
This regression qualifies no actual Live persistence, edition, or DSP behavior.
"""

from __future__ import annotations

import copy
import json

import pytest
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_device_workflow import device_workflow as device_workflow
from test_mcp_native_effect_workflow import _native_writes
from test_mcp_realization_workflow import _author, _literal_notes, _ok, _realize, _revision


def _attempts(workflow):
    return json.loads(workflow.history_ledger().read_text())["attempts"]


@pytest.mark.parametrize("changed_source", [False, True])
def test_source_requires_current_clip_geometry_before_noop_or_any_native_setter(
    request, changed_source
):
    """An empty-bar edit cannot bypass Clip geometry through matching Source notes."""
    workflow = request.getfixturevalue("device_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        assert _realize(client, "project_realization_create")["success"]
        authored = _timbre(client, source)
        assert authored["success"], authored
        track = workflow.live.song.tracks[1]
        slot, clip = track.clip_slots[0], track.clip_slots[0].clip
        native = track._devices[0]
        notes = dict(clip._notes)
        old_notes = [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        assert _literal_notes(clip) == old_notes and clip.end_marker == 4.0
        assert native.parameters[1].value == 0.5

        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        if changed_source:
            _ok(client, "set_parameter", profile_id=1, path="source.filter.cutoff", value=2450.0)
        revision = _revision(client)
        documents = copy.deepcopy(_ok(client, "get_project_json", score_id=1)["documents"])
        before, writes = copy.deepcopy(_attempts(workflow)), copy.deepcopy(_native_writes(track))
        calls = list(workflow.managed_calls)
        rejected = _timbre(client, source)
        assert rejected.get("success") is not True and rejected.get("error"), rejected
        assert workflow.managed_calls == calls
        assert _attempts(workflow) == before and _native_writes(track) == writes
        assert track.clip_slots[0] is slot and slot.clip is clip
        assert track._devices == [native] and native.parameters[1].value == 0.5
        assert _literal_notes(clip) == old_notes and clip.end_marker == 4.0
        assert all(clip._notes[note_id] is note for note_id, note in notes.items())

        geometry = _realize(client, "project_realization_update")
        assert geometry["success"] and geometry["selected_clip_phases_completed"], geometry
        assert len(geometry["phase_attempt_ids"]) == 1
        assert clip.end_marker == 8.0 and _literal_notes(clip) == old_notes
        assert _native_writes(track) == writes and not workflow.native_apply_calls
        if changed_source:
            applied = _timbre(client, source)
            assert applied["success"] and applied["mutation_dispatched"], applied
            assert native.parameters[1].value == 0.75
        else:
            assert native.parameters[1].value == 0.5

        before, writes = copy.deepcopy(_attempts(workflow)), copy.deepcopy(_native_writes(track))
        unchanged = _timbre(client, source)
        assert unchanged["success"] and unchanged["project_revision"] == revision, unchanged
        assert unchanged["state"] == "selected_device_intent_already_observed"
        assert unchanged["mutation_dispatched"] is False
        assert unchanged["source_insertion_requested"] is False
        observed = unchanged["native_inspection"]["inspection"]["binding_observation"]
        assert observed["manifest"]["clip"]["end_marker"] == 8.0
        assert _attempts(workflow) == before and _native_writes(track) == writes
        assert _revision(client) == revision
        assert _ok(client, "get_project_json", score_id=1)["documents"] == documents
        assert track._devices == [native] and slot.clip is clip
        assert all(clip._notes[note_id] is note for note_id, note in notes.items())
    assert len(workflow.native_device_insertions) == 1
