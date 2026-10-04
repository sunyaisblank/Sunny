"""Final selected envelope reads cannot discard their actual Part drift evidence.

Uses the real public MCP/TCP coordinator and independent external native provider;
this is not a Live host or complete envelope population qualification.
"""

from __future__ import annotations

import copy
import threading

import pytest
from test_mcp_native_device_workflow import _source
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_routing_workflow import routing_workflow as routing_workflow
from test_mcp_project_portfolio import author_pan
from test_mcp_project_realization_coordinator import _apply, _plan, _snapshot
from test_mcp_project_realization_coordinator import coordinator_workflow as coordinator_workflow
from test_mcp_realization_workflow import _author, _literal_notes, _revision
from test_mcp_workspace_workflow import _call as _ok


@pytest.fixture
def final_workflow(request):
    """Register inherited public provider fixtures explicitly for standalone collection."""
    return request.getfixturevalue("coordinator_workflow")


def test_final_pan_samples_refuse_actual_note_drift_without_new_history_or_writes(
    final_workflow, monkeypatch
):
    """Correct Step values cannot certify stale music after a truthful actual read."""
    workflow = final_workflow
    with workflow.process() as client:
        _author(client, workflow)
        sources = _source(client)
        author_pan(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        selection = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [
                {
                    "part_id": 1,
                    "source_selections": sources,
                    "pan_lane": {"lane_index": 0, "mode": "absent"},
                }
            ],
            "routing": [],
        }
        planned = _plan(client, selection)
        assert planned["success"], planned
        initial = _apply(client, planned["plan"])
        assert initial["success"] and initial["selected_contract_completed"], initial
        track = workflow.live.song.tracks[1]
        clip = track.clip_slots[0].clip
        pan = track.mixer_device.panning
        ids = tuple(clip._notes)
        assert _literal_notes(clip) == [
            (ids[0], 60, 0.0, 1.0, 80.0),
            (ids[1], 67, 2.0, 0.5, 72.0),
        ]
        envelope = clip.automation_envelope(pan)
        assert [envelope.value_at_time(t) for t in (0, 1, 2, 3)] == [-0.5, -0.5, 0.5, 0.5]
        original_attempts = copy.deepcopy(_snapshot(workflow)["attempts"])
        selection["parts"] = [{"part_id": 1, "pan_lane": {"lane_index": 0, "mode": "replace"}}]
        planned = _plan(client, selection)
        assert planned["success"], planned

        # Inject the user change inside the ACTUAL final native envelope read.
        # Preflight samples only0; final boundary/midpoint reads have multiple times.
        handler = workflow.surface._handler
        native_read = handler._get_step_envelope
        dispatch = workflow.surface._managed_registry.dispatch
        evidence = {}
        in_final_read = False

        def observed_dispatch(name, args):
            nonlocal in_final_read
            previous = in_final_read
            in_final_read = (
                name == "sunny_managed_sample_envelope" and len(args[0]["sample_times"]) > 1
            )
            try:
                return dispatch(name, args)
            finally:
                in_final_read = previous

        def actual_read(path, actual_clip, parameters):
            assert threading.get_ident() == workflow.main_thread._thread.ident
            result = native_read(path, actual_clip, parameters)
            if in_final_read:
                assert not evidence, "Final selected Pan sampling unexpectedly ran twice"
                evidence["before"] = _snapshot(workflow)
                evidence["ledger"] = workflow.history_ledger().read_bytes()
                evidence["samples"] = copy.deepcopy(result)
                assert actual_clip is clip
                # The native external edit occurs after sampling, before the helper's
                # second actual _observe. No production mutation helper is called.
                actual_clip._notes[ids[0]].velocity = 17.0
            return result

        monkeypatch.setattr(workflow.surface._managed_registry, "dispatch", observed_dispatch)
        monkeypatch.setattr(handler, "_get_step_envelope", actual_read)
        completed = _apply(client, planned["plan"])
        assert evidence, "Coordinator did not freshly sample the selected final Pan lane"
        assert _snapshot(workflow) == evidence["before"]
        assert workflow.history_ledger().read_bytes() == evidence["ledger"]
        retained = {item["intent"]["attempt_id"]: item for item in _snapshot(workflow)["attempts"]}
        for original in original_attempts:
            assert retained[original["intent"]["attempt_id"]] == original
        assert tuple(clip._notes) == ids
        assert clip._notes[ids[0]].velocity == 17.0
        assert clip._notes[ids[1]].velocity == 72.0
        current_envelope = clip.automation_envelope(pan)
        assert [current_envelope.value_at_time(t) for t in (0, 1, 2, 3)] == [-0.5, -0.5, 0.5, 0.5]
        assert evidence["samples"]["has_envelope"] is True
        if completed.get("success") is True:
            # The old implementation explicitly returns contradictory actual evidence.
            proof = completed["final_native_observations"][0]
            before_notes = proof["current_native_binding"]["observation"]["note_identity"]["notes"]
            observed_notes = proof["pan_lane_observation"]["observation"]["note_identity"]["notes"]
            assert next(n for n in before_notes if n["note_id"] == ids[0])["velocity"] == 80.0
            assert next(n for n in observed_notes if n["note_id"] == ids[0])["velocity"] == 17.0
        assert completed.get("success") is not True, completed
        assert completed.get("selected_contract_completed") is not True
        assert "cohort" in completed["error"].lower() or "binding" in completed["error"].lower()
