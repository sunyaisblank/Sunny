"""Literal current-native admission failures before any project mutation."""

from __future__ import annotations

import pytest
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_project_portfolio import author_complete_portfolio
from test_mcp_native_routing_workflow import routing_workflow as routing_workflow
from test_mcp_project_portfolio import author_two_parts
from test_mcp_project_realization_coordinator import (
    _apply,
    _plan,
    _refused,
    _selection,
    _snapshot,
    _succeeded,
)
from test_mcp_project_realization_coordinator import coordinator_workflow as coordinator_workflow
from test_mcp_realization_workflow import _revision
from test_mcp_workspace_workflow import _call as _ok


@pytest.fixture
def native_admission_workflow(request):
    """Use the matched coordinator provider and its bounded serialized timeout."""
    return request.getfixturevalue("coordinator_workflow")


@pytest.mark.parametrize("domain", ["volume", "pan"])
def test_new_audio_mixer_control_requires_selected_source_before_first_fence(
    native_admission_workflow, domain
):
    """A known empty new MIDI chain cannot expose an audio fader or stereo pan."""
    workflow = native_admission_workflow
    with workflow.process() as client:
        author_two_parts(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        mixer = {"domains": [domain]}
        if domain == "volume":
            mixer["volume_tolerance_db"] = 0.0
        selection = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [{"part_id": 1, "static_mixer": mixer}],
            "routing": [],
        }
        before = _snapshot(workflow)
        _refused(workflow, _plan(client, selection), before, "source")


@pytest.mark.parametrize("enabled", [True, False])
def test_existing_active_effect_target_outside_native_domain_refuses_before_note_fence(
    native_admission_workflow, enabled
):
    """Current active neutral Eq8 targets can be formatter-admitted before notes."""
    workflow = native_admission_workflow
    with workflow.process() as client:
        authored = author_complete_portfolio(client, workflow)
        selection = _selection(client, authored, lane=False)
        initial = _plan(client, selection)
        assert initial["success"], initial
        _succeeded(workflow, _apply(client, initial["plan"]))
        eq = workflow.live.song.tracks[1]._devices[-1]
        frequency = next(value for value in eq.parameters if value.original_name == "1 Frequency A")
        assert eq.class_name == "Eq8" and eq.is_active is True
        assert frequency.str_for_value(1.0) == "4200.00 Hz"
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1, "parts": [1]},
            interval={"chromatic": 2, "diatonic": 1},
        )
        _ok(
            client,
            "replace_mix_effect",
            graph_id=1,
            effect_id=authored["eq"],
            configuration={
                "effect_type": "eq",
                "enabled": enabled,
                "linear_phase": False,
                "bands": [{"frequency": 5000.0, "gain": -5.0, "q": 1.75, "type": 0}],
            },
        )
        selection["expected_project_revision"] = _revision(client)
        before = _snapshot(workflow)
        _refused(workflow, _plan(client, selection), before, "effect")
