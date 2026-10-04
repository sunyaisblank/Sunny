"""Compact final proof preserves unknown non-owned Device coverage honestly.

This reuses the existing public recovery and coordinator providers. It tests
current handles and finite native evidence, not actual Live Set persistence.
"""

from __future__ import annotations

import copy

from test_managed_devices import device
from test_mcp_native_device_workflow import _source, _timbre
from test_mcp_native_effect_workflow import _effect_selections, _effects, _native_writes
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_native_routing_workflow import routing_workflow as routing_workflow
from test_mcp_note_population_workflow import _events
from test_mcp_project_realization_coordinator import _apply, _plan, _snapshot, _succeeded
from test_mcp_project_realization_coordinator import coordinator_workflow as coordinator_workflow
from test_mcp_realization_workflow import _author, _literal_notes, _realize, _revision
from test_mcp_workspace_workflow import _call as _ok


def test_score_only_completion_preserves_nonowned_chain_without_device_authority(request):
    """An existing User Utility stays concrete and unowned after adopted note revision."""
    workflow = request.getfixturevalue("coordinator_workflow")
    with workflow.process() as client:
        _author(client, workflow)
        source = _source(client)
        effects, _, _ = _effect_selections(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        assert _realize(client, "project_realization_create")["success"]
        old_track = workflow.live.song.tracks[1]
        user_device = device("Utility")
        user_device._canonical_parent = old_track
        user_device.name = "User Utility"
        # External user state, never a managed insert or logical Device-key grant.
        old_track._devices.append(user_device)
        _ok(client, "workspace_save", path=workflow.server_workspace)

    track, clip = _replace_provider_document(workflow)
    native = track._devices[0]
    assert native is not user_device and native.name == "User Utility"
    cohort = tuple(track._devices)
    writes = copy.deepcopy(_native_writes(track))
    with workflow.process() as client:
        preview = _preview(client)
        assert preview["success"] and preview["eligible_for_explicit_adoption"], preview
        adopted = _adopt(client, preview["preview"])
        assert adopted["success"], adopted
        binding = adopted["receipt"]["journal"]["result"]
        assert binding["manifest"]["devices_empty"] is False
        assert "device_identity" not in binding
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        _ok(client, "score_modify_note", score_id=1, event_id=_events(client)[0]["id"], velocity=90)
        selected = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [{"part_id": 1}],
            "routing": [],
        }
        planned = _plan(client, selected)
        assert planned["success"], planned
        completed = _succeeded(workflow, _apply(client, planned["plan"]))
        proof = completed["final_native_observations"][0]
        assert proof["selected_notes_and_geometry_verified"] is True
        assert proof["binding_guard"]["devices_empty"] is False
        assert proof["binding_guard"]["device_count"] is None
        assert "device_identity_fingerprint" not in proof["binding_guard"]
        assert "device_inspection" not in proof
        assert _literal_notes(clip) == [
            (101, 60, 0.0, 1.0, 90.0),
            (102, 67, 2.0, 0.5, 72.0),
        ]
        assert tuple(track._devices) == cohort and _native_writes(track) == writes
        assert not workflow.native_device_insertions
        before = _snapshot(workflow)
        for result in (_timbre(client, source), _effects(client, effects)):
            assert result["success"] is False, result
            assert "adopt" in result["error"].lower(), result
            assert _snapshot(workflow) == before
        assert tuple(track._devices) == cohort and _native_writes(track) == writes
