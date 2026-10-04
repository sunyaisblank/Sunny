"""Literal owning Mix Step lanes through actual MCP/TCP and retained Live identities."""

from __future__ import annotations

import pytest
from test_mcp_realization_workflow import NativeWorkflow, _author, _ok, _realize, _revision


@pytest.fixture
def mix_workflow(tmp_path, monkeypatch):
    """Keep one isolated native provider and durable owning workspace."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    try:
        yield workflow
    finally:
        workflow.close()


def _lane(client, interpolation=0, target="channels[1].spatial.pan"):
    return _ok(
        client,
        "add_mix_automation",
        graph_id=1,
        target=target,
        interpolation=interpolation,
        breakpoints=[
            {"bar": 1, "beat_num": 0, "beat_den": 1, "value": -0.5},
            {"bar": 1, "beat_num": 1, "beat_den": 2, "value": 0.5},
        ],
    )


def _apply(client, lane_index=0):
    return client.call(
        "project_realization_author_mix_lane",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        lane_index=lane_index,
    )


def test_owning_step_pan_lane_uses_quarter_beats_retained_ids_and_durable_fence(mix_workflow):
    """Half a Sunny whole note is two Live quarter beats; native values are sampled."""
    workflow = mix_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        track = workflow.live.song.tracks[1]
        clip = track.clip_slots[0].clip
        identities = set(clip._notes)
        _lane(client)
        outcome = _apply(client)
        assert outcome["success"], outcome
        assert outcome["dispatch_fenced"] and outcome["history_saved"]
        assert outcome["native_calls_acknowledged"] and outcome["native_lane_samples_verified"]
        assert outcome["complete_envelope_population_observed"] is False
        assert outcome["complete_project_realization"] is False
        assert outcome["native_lane_observation"]["envelope"]["samples"] == [
            {"time": 0.0, "value": -0.5},
            {"time": 1.0, "value": -0.5},
            {"time": 2.0, "value": 0.5},
            {"time": 3.0, "value": 0.5},
        ]
        envelope = clip.automation_envelope(track.mixer_device.panning)
        assert envelope is not None
        assert [envelope.value_at_time(time) for time in (0.0, 1.0, 2.0, 3.0)] == [
            -0.5,
            -0.5,
            0.5,
            0.5,
        ]
        assert track.clip_slots[0].clip is clip and set(clip._notes) == identities
        envelope.insert_step(0.25, 0.5, -0.25)  # Actual user edit beyond finite sampled evidence.
        repeated = _apply(client)
        assert repeated["success"] is False and repeated["state"] == "existing_envelope_preserved"
        assert envelope.value_at_time(0.5) == -0.25
        assert track.clip_slots[0].clip is clip and set(clip._notes) == identities


def test_unsupported_interpolation_declines_before_envelope_or_history_mutation(mix_workflow):
    """Linear authoring is not silently rendered as Step."""
    workflow = mix_workflow
    with workflow.process() as client:
        _author(client, workflow)
        assert _realize(client, "project_realization_create")["success"]
        _lane(client, interpolation=1)
        ledger = workflow.history_ledger()
        before = ledger.read_bytes()
        result = _apply(client)
        assert result["success"] is False and "Step panning" in result["error"]
        track = workflow.live.song.tracks[1]
        assert track.clip_slots[0].clip.automation_envelope(track.mixer_device.panning) is None
        assert ledger.read_bytes() == before
