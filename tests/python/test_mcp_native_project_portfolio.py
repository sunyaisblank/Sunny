"""Executable literal portfolio using only registered per-family public APIs.

Root binds the final coordinator schema later. These external-provider workflows
prove the actual MCP/TCP and immutable-history joins, not Live DSP/Set persistence.
"""

from __future__ import annotations

import copy

import pytest
from test_mcp_native_effect_workflow import (
    _assert_native_effect_values,
    _effect_selections,
    _effects,
    _native_writes,
)
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_envelope_revision_workflow import _approved
from test_mcp_native_envelope_revision_workflow import _preview as _lane_preview
from test_mcp_native_mixer_workflow import _attempts
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_routing_workflow import _aux, _send
from test_mcp_native_routing_workflow import routing_workflow as routing_workflow
from test_mcp_project_portfolio import (
    COMBINED_NOTES,
    INITIAL_NOTES,
    add_then_remove_authored_part,
    assert_notes,
    author_pan,
    author_two_parts,
    edit_notes_and_extent_together,
    native_part,
    source_for,
)
from test_mcp_realization_workflow import _revision
from test_mcp_workspace_workflow import _call as _ok


@pytest.fixture
def whole_public_workflow(request):
    """Compose existing genuine Mixer/control and Return/source provider boundaries."""
    return request.getfixturevalue("routing_workflow")


def _route(client, part, tool, kind, **extra):
    return native_part(client, part, f"project_realization_{tool}_routing", kind=kind, **extra)


def _route_apply(client, part, kind, **extra):
    preview = _route(client, part, "preview", kind, **extra)
    assert preview["success"] and preview["mutation_dispatched"] is False, preview
    result = _route(
        client,
        part,
        "apply",
        kind,
        preview=preview["preview"],
        explicit_current_routing_approval=True,
        **extra,
    )
    assert result["success"] and result["history_saved"], result
    return result


def _mixer(client, part, tool, domains, **extra):
    if "volume" in domains:
        extra["volume_tolerance_db"] = 0.0
    return native_part(client, part, tool, domains=domains, **extra)


def _mixer_apply(client, part, domains, *, adoption=False):
    preview = _mixer(
        client,
        part,
        "project_realization_preview_static_mixer",
        domains,
        purpose="adopt" if adoption else "update",
    )
    assert preview["success"] and preview["mutation_dispatched"] is False, preview
    result = _mixer(
        client,
        part,
        "project_realization_adopt_static_mixer"
        if adoption
        else "project_realization_apply_static_mixer",
        domains,
        preview=preview["preview"],
        explicit_current_mixer_approval=True,
        explicit_set_wide_audible_approval=True,
    )
    assert result["success"] and result["history_saved"], result
    if adoption:
        assert result["receipt"]["journal"]["native_mutation_started"] is False
    return result


def author_complete_portfolio(client, workflow):
    """Return stable owning IDs/selectors; derive no expected native note values."""
    author_two_parts(client)
    sources = {part: source_for(client, part) for part in (1, 2)}
    effects, width, eq = _effect_selections(client)
    aux = _aux(client)
    _send(client, aux, channel=1, pre=False, level=-24.0)
    _ok(client, "set_channel_flags", graph_id=1, channel_id=1, mute=False, solo=True)
    _ok(client, "workspace_save", path=workflow.server_workspace)
    return {"sources": sources, "effects": effects, "width": width, "eq": eq, "aux": aux}


def test_two_part_return_static_lane_combined_revision_and_retirement(whole_public_workflow):
    """Real signal order and captured IDs remain stable across every selected domain."""
    workflow = whole_public_workflow
    with workflow.process() as client:
        selected = author_complete_portfolio(client, workflow)
        for part in (1, 2):
            assert native_part(client, part)["success"]
        tracks = {part: workflow.live.song.tracks[part] for part in (1, 2)}
        clips = {part: tracks[part].clip_slots[0].clip for part in (1, 2)}
        original_ids = {part: tuple(clips[part]._notes) for part in (1, 2)}
        old_return = workflow.live.song.return_tracks[0]
        old_sends = {part: tracks[part].mixer_device.sends[0] for part in (1, 2)}

        # Return BEFORE fresh static Mixer grants, and before source role changes.
        returned = _route_apply(client, 1, "create_return", aux_id=selected["aux"])
        assert returned["receipt"]["journal"]["native_mutation_started"] is True
        assert workflow.native_return_calls == ["create_return_track"]
        assert len(workflow.live.song.return_tracks) == 2
        assert workflow.live.song.return_tracks[0] is old_return
        owned_return = workflow.live.song.return_tracks[1]
        for part in (1, 2):
            assert tracks[part].mixer_device.sends[0] is old_sends[part]
            assert old_sends[part].value == 0.375
            source = native_part(
                client,
                part,
                "project_realization_author_timbre",
                selections=selected["sources"][part],
            )
            assert source["success"], source
            assert_notes(clips[part], INITIAL_NOTES[part])
            assert tuple(clips[part]._notes) == original_ids[part]
            assert clips[part].end_marker == 8.0
        assert _effects(client, selected["effects"])["success"]
        _assert_native_effect_values(tracks[1])
        sent = _route_apply(client, 1, "send_level", aux_id=selected["aux"], send_tolerance_db=0.0)
        send_result = sent["receipt"]["journal"]["result"]["routing"]
        assert send_result["send_readback"]["display"] == "-24.00 dB"
        assert send_result["tap_policy_observed"] is False
        assert send_result["logical_send_complete"] is False
        selected_send = tracks[1].mixer_device.sends[1]
        assert selected_send.value == 0.25 and selected_send.writes == [0.25]

        for part, domains in ((1, ["volume", "pan", "mute", "solo"]), (2, ["volume", "pan"])):
            unavailable = _mixer(
                client, part, "project_realization_preview_static_mixer", domains, purpose="update"
            )
            assert unavailable["success"] is False, unavailable
            _mixer_apply(client, part, domains, adoption=True)
            _mixer_apply(client, part, domains)
        assert tracks[1].mixer_device.volume.value == 0.5
        assert tracks[2].mixer_device.volume.value == 0.5
        assert tracks[1].mixer_device.panning.value == -0.25
        assert tracks[2].mixer_device.panning.value == 0.25
        assert tracks[1].mute is False and tracks[1].mixer_device.track_activator.value == 1.0
        assert tracks[1].solo is True and tracks[2].solo is False
        assert tracks[2].muted_via_solo is True
        assert tracks[2].mute is False
        author_pan(client)
        initial_lane = native_part(client, 1, "project_realization_author_mix_lane", lane_index=0)
        assert initial_lane["success"] and initial_lane["native_lane_samples_verified"], (
            initial_lane
        )
        envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert [envelope.value_at_time(time) for time in (0, 1, 2, 7)] == [-0.5, -0.5, 0.5, 0.5]
        device_cohorts = {part: tuple(tracks[part]._devices) for part in (1, 2)}
        device_writes = {part: copy.deepcopy(_native_writes(tracks[part])) for part in (1, 2)}
        for part in (1, 2):
            clips[part]._user_mpe_expression = {original_ids[part][0]: {"pressure": [0.1, 0.8]}}
            clips[part]._user_follow_actions = {"next": True}

        edit_notes_and_extent_together(client)
        _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[2], denominator=2)
        author_pan(client, revised=True)
        _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-35.75)
        _ok(client, "set_channel_flags", graph_id=1, channel_id=1, mute=True, solo=False)
        final_revision = _revision(client)
        for part in (1, 2):
            revised = native_part(client, part, "project_realization_update")
            assert revised["success"] and revised["selected_clip_phases_completed"], revised
            assert len(revised["phase_attempt_ids"]) == (3 if part == 1 else 1)
            assert_notes(clips[part], COMBINED_NOTES[part])
            assert clips[part].end_marker == 12.0
            assert clips[part].signature_numerator == 2 and clips[part].signature_denominator == 2
            assert _revision(client) == final_revision
            assert tuple(tracks[part]._devices) == device_cohorts[part]
            assert _native_writes(tracks[part]) == device_writes[part]
        assert tuple(clips[1]._notes)[:3] == original_ids[1]
        assert tuple(clips[2]._notes) == original_ids[2]
        _mixer_apply(client, 1, ["volume", "mute", "solo"])
        assert tracks[1].mixer_device.volume.value == 0.25
        assert tracks[1].mute is True and tracks[1].mixer_device.track_activator.value == 0.0
        assert tracks[1].solo is False and tracks[2].muted_via_solo is False
        preview = _lane_preview(client)
        assert preview["success"], preview
        replacement = _approved(client, preview["preview"])
        assert replacement["success"], replacement
        envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert [envelope.value_at_time(time) for time in (0, 0.5, 1, 8, 11)] == [
            -0.25,
            -0.25,
            0.25,
            0.25,
            0.25,
        ]
        assert _revision(client) == final_revision
        _ok(client, "workspace_save", path=workflow.server_workspace)
        lifecycle = add_then_remove_authored_part(client)
        assert lifecycle["retired_part_id"] == 2
        assert native_part(client, 3)["success"]
        new_track = workflow.live.song.tracks[3]
        assert_notes(new_track.clip_slots[0].clip, [(65, 0.0, 1.0, 78.0)])
        # An earlier volume/pan grant cannot authorize retirement mute.
        # Review/adopt exact CURRENT mute-only authority on the historic Part;
        # this changes bridge authority only and never formats native volume.
        before_retirement = copy.deepcopy(_attempts(workflow))
        unavailable = native_part(
            client, 2, "project_realization_retire_part", explicit_mute_retirement=True
        )
        assert unavailable["success"] is False
        assert _attempts(workflow) == before_retirement
        assert tracks[2].mute is False and tracks[2].mixer_device.track_activator.value == 1.0
        volume_calls = list(tracks[2].mixer_device.volume.calls)
        _mixer_apply(client, 2, ["mute"], adoption=True)
        assert tracks[2].mute is False
        retired = native_part(
            client, 2, "project_realization_retire_part", explicit_mute_retirement=True
        )
        assert retired["success"] and retired["history_saved"], retired
        assert tracks[2].mute is True and tracks[2].mixer_device.track_activator.value == 0.0
        assert_notes(clips[2], [(55, 0.0, 5.0, 72.0)])
        assert clips[2].end_marker == 12.0 and tuple(clips[2]._notes) == original_ids[2]
        repeat = native_part(
            client, 2, "project_realization_retire_part", explicit_mute_retirement=True
        )
        assert (
            repeat["success"] and repeat["receipt"]["journal"]["native_mutation_started"] is False
        )
        assert workflow.live.song.return_tracks[1] is owned_return
        assert selected_send.value == 0.25 and selected_send.writes == [0.25]
        assert old_sends[1].value == 0.375 and old_sends[2].value == 0.375
        for part in (1, 2):
            assert workflow.live.song.tracks[part] is tracks[part]
            assert tracks[part].clip_slots[0].clip is clips[part]
            assert tuple(tracks[part]._devices) == device_cohorts[part]
            assert _native_writes(tracks[part]) == device_writes[part]
            assert clips[part]._user_mpe_expression == {
                original_ids[part][0]: {"pressure": [0.1, 0.8]}
            }
            assert clips[part]._user_follow_actions == {"next": True}
        assert tracks[2].mixer_device.volume.calls == volume_calls
        assert workflow.native_return_calls == ["create_return_track"]
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 3
        _ok(client, "workspace_save", path=workflow.server_workspace)


@pytest.mark.parametrize(
    "counterexample",
    ["tap_conflict", "linear_phase", "missing_earlier_trim", "wrong_source_capability"],
)
def test_known_mandatory_unsupported_selection_refuses_before_any_native_fence(
    whole_public_workflow, counterexample
):
    """Expose Root-ready selectors through real owning tools; no coordinator schema assumed."""
    workflow = whole_public_workflow
    with workflow.process() as client:
        readiness_calls = list(workflow.managed_calls)
        selected = author_complete_portfolio(client, workflow)
        if counterexample == "tap_conflict":
            _send(client, selected["aux"], channel=2, pre=True, level=-24.0)
            result = _route(client, 1, "plan", "create_return", aux_id=selected["aux"])
            expected = "Pre/Post"
        elif counterexample == "linear_phase":
            _ok(
                client,
                "replace_mix_effect",
                graph_id=1,
                effect_id=selected["eq"],
                configuration={
                    "effect_type": "eq",
                    "linear_phase": True,
                    "bands": [{"frequency": 733.0, "gain": -5.0, "q": 1.75, "type": 0}],
                },
            )
            result = _effects(client, selected["effects"], "project_realization_plan_effects")
            expected = "Linear"
        elif counterexample == "missing_earlier_trim":
            incomplete = [item for item in selected["effects"] if item["kind"] != "mix_input_trim"]
            result = _effects(client, incomplete, "project_realization_plan_effects")
            expected = "trim"
        else:
            invalid = copy.deepcopy(selected["sources"][1])
            invalid[0]["capability_id"] = "utility.gain"
            result = native_part(client, 1, "project_realization_plan_timbre", selections=invalid)
            expected = "source/control"
        assert result.get("success") is not True and expected.lower() in result["error"].lower(), (
            result
        )
        assert _attempts(workflow) == []
        assert workflow.managed_calls == readiness_calls
        assert workflow.native_return_calls == [] and workflow.native_route_calls == []
        assert workflow.native_device_insertions == [] and workflow.native_apply_calls == []
        assert len(workflow.live.song.tracks) == 1 and len(workflow.live.song.return_tracks) == 1
