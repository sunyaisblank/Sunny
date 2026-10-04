"""Literal whole-project oracles through the registered actual MCP/TCP API.

The provider is external and deterministic. Success qualifies the finite native
control/readback contract, never actual Live DSP or saved Set persistence.
"""

from __future__ import annotations

import copy
import json

import pytest
import test_live_end_to_end as mcp_fixture
from test_mcp_native_effect_workflow import _assert_native_effect_values, _native_writes
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import _attempts
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_project_portfolio import author_complete_portfolio
from test_mcp_native_routing_workflow import _send
from test_mcp_native_routing_workflow import routing_workflow as routing_workflow
from test_mcp_project_portfolio import (
    COMBINED_NOTES,
    INITIAL_NOTES,
    add_then_remove_authored_part,
    assert_notes,
    author_pan,
    edit_notes_and_extent_together,
    source_for,
)
from test_mcp_realization_workflow import _revision
from test_mcp_workspace_workflow import _call as _ok


@pytest.fixture
def coordinator_workflow(request, monkeypatch):
    """Allow one bounded request to await all serialized durable family phases."""
    monkeypatch.setattr(mcp_fixture, "RESPONSE_TIMEOUT_SECONDS", 120.0)
    return request.getfixturevalue("routing_workflow")


def _selection(client, authored, *, initial=True, lane=True):
    one = {
        "part_id": 1,
        "source_selections": authored["sources"][1],
        "effect_selections": authored["effects"],
        "static_mixer": {
            "domains": ["volume", "pan", "mute", "solo"] if initial else ["volume", "mute", "solo"],
            "volume_tolerance_db": 0.0,
        },
    }
    if lane:
        one["pan_lane"] = {"lane_index": 0, "mode": "absent" if initial else "replace"}
    return {
        "score_id": 1,
        "expected_project_revision": _revision(client),
        "parts": [
            one,
            {
                "part_id": 2,
                "source_selections": authored["sources"][2],
                "static_mixer": {"domains": ["volume", "pan"], "volume_tolerance_db": 0.0},
            },
        ],
        "routing": [
            {"part_id": 1, "kind": "create_return", "aux_id": authored["aux"]},
            {
                "part_id": 1,
                "kind": "send_level",
                "aux_id": authored["aux"],
                "send_tolerance_db": 0.0,
            },
        ],
    }


def _plan(client, selection):
    return client.call("project_realization_plan", **selection)


def _apply(client, plan, **overrides):
    arguments = {
        "plan": plan,
        "explicit_plan_approval": True,
        "explicit_current_mixer_approval": True,
        "explicit_set_wide_audible_approval": True,
        "explicit_current_routing_approval": True,
        "explicit_selected_envelope_replacement": True,
        "allow_unsampled_selected_state_overwrite": True,
        "explicit_mute_retirement": True,
    }
    for name, value in overrides.items():
        if value is None:
            arguments.pop(name, None)
        else:
            arguments[name] = value
    return client.call("project_realization_apply", **arguments)


def _succeeded(workflow, result):
    if not result.get("success"):
        path = workflow.directory / "coordinator-failure.json"
        path.write_text(json.dumps(result, indent=2))
        summary = {
            "error": result.get("error"),
            "state": result.get("state"),
            "phase": result.get("effect_phase"),
            "failed_phases": [
                (phase["tool"], phase["part_id"], phase["result"].get("error"))
                for phase in result.get("phases", [])
                if phase["result"].get("success") is not True
            ],
            "full_evidence": str(path),
        }
        pytest.fail(str(summary))
    assert result["selected_contract_completed"] is True
    assert result["host_qualified"] is False and result["complete_project_realization"] is False
    assert result["final_current_cohorts_verified"] is True
    assert result["dsp_equivalence_qualified"] is False
    return result


def _snapshot(workflow):
    """Only external writes/fences; read-only formatter/observation calls may run."""
    tracks = tuple(workflow.live.song.tracks)
    return {
        "attempts": copy.deepcopy(_attempts(workflow)),
        "tracks": tracks,
        "returns": tuple(workflow.live.song.return_tracks),
        "insertions": list(workflow.native_device_insertions),
        "return_writes": list(workflow.native_return_calls),
        "route_writes": list(workflow.native_route_calls),
        "note_writes": list(workflow.native_apply_calls),
        "mixer_gates": list(workflow.native_mixer_gate_writes),
        "device_writes": [_native_writes(track) for track in tracks],
        "parameter_writes": [
            (
                list(getattr(track.mixer_device.volume, "writes", [])),
                list(getattr(track.mixer_device.panning, "writes", [])),
                [list(getattr(native, "writes", [])) for native in track.mixer_device.sends],
            )
            for track in tracks
        ],
    }


def _refused(workflow, result, before, diagnostic):
    assert result.get("success") is not True, result
    assert diagnostic.lower() in result["error"].lower(), result
    assert _snapshot(workflow) == before


def test_one_owning_project_plan_initial_joint_revision_and_retired_part(coordinator_workflow):
    """One authored revision preserves tied/triplet notes, objects and selected controls."""
    workflow = coordinator_workflow
    with workflow.process() as client:
        authored = author_complete_portfolio(client, workflow)
        author_pan(client)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        selection = _selection(client, authored)
        before = _snapshot(workflow)
        planned = _plan(client, selection)
        assert planned["success"] and planned["mutation_dispatched"] is False, planned
        assert _snapshot(workflow) == before
        assert planned["plan"]["selection"] == selection
        initial = _succeeded(workflow, _apply(client, planned["plan"]))
        assert initial["project_revision"] == selection["expected_project_revision"]
        assert _revision(client) == selection["expected_project_revision"]
        assert len(set(initial["phase_attempt_ids"])) == len(initial["phase_attempt_ids"])
        assert set(initial["phase_attempt_ids"]) == {
            item["intent"]["attempt_id"] for item in _attempts(workflow)
        }
        assert all(
            item["intent"]["project_revision"] == selection["expected_project_revision"]
            for item in _attempts(workflow)
        )
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 2
        assert workflow.native_return_calls == ["create_return_track"]
        assert len(workflow.live.song.return_tracks) == 2
        tracks = {part: workflow.live.song.tracks[part] for part in (1, 2)}
        clips = {part: tracks[part].clip_slots[0].clip for part in (1, 2)}
        ids = {part: tuple(clips[part]._notes) for part in (1, 2)}
        devices = {part: tuple(tracks[part]._devices) for part in (1, 2)}
        source_and_effect_writes = {
            part: copy.deepcopy(_native_writes(tracks[part])) for part in (1, 2)
        }
        _assert_native_effect_values(tracks[1])
        assert [native.class_name for native in tracks[2]._devices] == ["Drift"]
        for part in (1, 2):
            assert_notes(clips[part], INITIAL_NOTES[part])
            assert clips[part].end_marker == 8.0
            assert (clips[part].signature_numerator, clips[part].signature_denominator) == (4, 4)
            assert tracks[part].mixer_device.volume.value == 0.5
            assert tracks[part].mixer_device.sends[0].value == 0.375
            source_controls = {
                parameter.original_name: parameter
                for parameter in tracks[part]._devices[0].parameters
            }
            assert [
                source_controls[name].value
                for name in ("LP Freq", "Env 1 Attack", "Env 1 Decay", "Env 1 Release")
            ] == [0.5, 0.5, 0.5, 0.5]
            clips[part]._user_mpe_expression = {ids[part][0]: {"pressure": [0.1, 0.8]}}
            clips[part]._user_follow_actions = {"next": True}
        assert tracks[1].mixer_device.panning.value == -0.25
        assert tracks[2].mixer_device.panning.value == 0.25
        assert tracks[1].solo is True and tracks[2].muted_via_solo is True
        assert tracks[2].solo is False and tracks[2].mute is False
        assert tracks[1].mixer_device.sends[1].value == 0.25
        envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert [envelope.value_at_time(time) for time in (0, 1, 2, 7)] == [-0.5, -0.5, 0.5, 0.5]
        # Static pan is forbidden once the selected pan lane exists.
        unchanged = _snapshot(workflow)
        _refused(workflow, _plan(client, selection), unchanged, "pan")

        edit_notes_and_extent_together(client)
        _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[2], denominator=2)
        author_pan(client, revised=True)
        _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-35.75)
        _ok(client, "set_channel_flags", graph_id=1, channel_id=1, mute=True, solo=False)
        revised_selection = _selection(client, authored, initial=False)
        revised_plan = _plan(client, revised_selection)
        assert revised_plan["success"], revised_plan
        assert [item["clip_phase_count"] for item in revised_plan["plan"]["parts"]] == [
            3,
            1,
        ]
        revised = _succeeded(workflow, _apply(client, revised_plan["plan"]))
        revised_records = [
            item
            for item in _attempts(workflow)
            if item["intent"]["attempt_id"] in revised["phase_attempt_ids"]
        ]
        assert len(revised_records) == len(revised["phase_attempt_ids"])
        assert all(
            item["intent"]["project_revision"] == revised_selection["expected_project_revision"]
            for item in revised_records
        )
        assert _revision(client) == revised_selection["expected_project_revision"]
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 2
        assert workflow.native_return_calls == ["create_return_track"]
        for part in (1, 2):
            assert_notes(clips[part], COMBINED_NOTES[part])
            assert clips[part].end_marker == 12.0
            assert (clips[part].signature_numerator, clips[part].signature_denominator) == (2, 2)
            assert tuple(tracks[part]._devices) == devices[part]
            assert _native_writes(tracks[part]) == source_and_effect_writes[part]
            assert clips[part]._user_mpe_expression == {ids[part][0]: {"pressure": [0.1, 0.8]}}
            assert clips[part]._user_follow_actions == {"next": True}
        assert tuple(clips[1]._notes)[:3] == ids[1]
        assert tuple(clips[2]._notes) == ids[2]
        assert tracks[1].mixer_device.volume.value == 0.25
        assert tracks[1].mute is True and tracks[1].mixer_device.track_activator.value == 0.0
        assert tracks[1].solo is False and tracks[2].muted_via_solo is False
        revised_envelope = clips[1].automation_envelope(tracks[1].mixer_device.panning)
        assert revised_envelope is not None and revised_envelope is not envelope
        assert [revised_envelope.value_at_time(time) for time in (0, 0.5, 1, 8, 11)] == [
            -0.25,
            -0.25,
            0.25,
            0.25,
            0.25,
        ]
        assert revised["final_native_observations"][0]["pan_lane_interval_end"] == 12.0

        lifecycle = add_then_remove_authored_part(client)
        assert lifecycle["retired_part_id"] == 2
        sources3 = source_for(client, 3)
        third_selection = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [{"part_id": 1}, {"part_id": 3, "source_selections": sources3}],
            "routing": [],
            "retire_part_ids": [2],
        }
        volume_calls = list(tracks[2].mixer_device.volume.calls)
        retired_plan = _plan(client, third_selection)
        assert retired_plan["success"], retired_plan
        _succeeded(workflow, _apply(client, retired_plan["plan"]))
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 3
        assert len(workflow.live.song.tracks) == 4
        assert_notes(workflow.live.song.tracks[3].clip_slots[0].clip, [(65, 0.0, 1.0, 78.0)])
        assert tracks[2].mute is True and tracks[2].mixer_device.track_activator.value == 0.0
        assert tracks[2].mixer_device.volume.calls == volume_calls
        assert_notes(clips[2], [(55, 0.0, 5.0, 72.0)])
        assert clips[2].end_marker == 12.0 and tuple(clips[2]._notes) == ids[2]
        assert tuple(tracks[2]._devices) == devices[2]
        assert _revision(client) == third_selection["expected_project_revision"]
        _ok(client, "workspace_save", path=workflow.server_workspace)


@pytest.mark.parametrize(
    "counterexample",
    ["later_part", "linear_phase", "linear_pan", "tap_conflict", "unknown_retired"],
)
def test_all_known_unsupported_late_selections_refuse_before_first_fence(
    coordinator_workflow, counterexample
):
    """Every known authored defect is checked before even the earliest Part create."""
    workflow = coordinator_workflow
    with workflow.process() as client:
        authored = author_complete_portfolio(client, workflow)
        author_pan(client)
        selected = _selection(client, authored)
        if counterexample == "later_part":
            selected["parts"][1]["source_selections"][0]["capability_id"] = "utility.gain"
            expected = "source/control"
        elif counterexample == "linear_phase":
            _ok(
                client,
                "replace_mix_effect",
                graph_id=1,
                effect_id=authored["eq"],
                configuration={
                    "effect_type": "eq",
                    "linear_phase": True,
                    "bands": [{"frequency": 733.0, "gain": -5.0, "q": 1.75, "type": 0}],
                },
            )
            selected["parts"].reverse()
            expected = "Linear"
        elif counterexample == "linear_pan":
            _ok(
                client,
                "add_mix_automation",
                graph_id=1,
                target="channels[1].spatial.pan",
                interpolation=1,
                breakpoints=[
                    {"bar": 1, "beat_num": 0, "beat_den": 1, "value": -0.5},
                    {"bar": 1, "beat_num": 1, "beat_den": 2, "value": 0.5},
                ],
            )
            selected["parts"][0]["pan_lane"]["lane_index"] = 1
            expected = "Step"
        elif counterexample == "tap_conflict":
            _send(client, authored["aux"], channel=2, pre=True, level=-24.0)
            expected = "Pre/Post"
        else:
            selected["retire_part_ids"] = [900]
            expected = "histor"
        selected["expected_project_revision"] = _revision(client)
        before = _snapshot(workflow)
        _refused(workflow, _plan(client, selected), before, expected)
        assert _attempts(workflow) == []
        assert workflow.native_device_insertions == [] and workflow.native_apply_calls == []
        assert len(workflow.live.song.tracks) == 1 and len(workflow.live.song.return_tracks) == 1


def test_missing_selected_approval_tampered_and_stale_plan_are_zero_fence(coordinator_workflow):
    """Each approval and exact current plan is required before durable native dispatch."""
    workflow = coordinator_workflow
    with workflow.process() as client:
        authored = author_complete_portfolio(client, workflow)
        selected = _selection(client, authored, lane=False)
        planned = _plan(client, selected)
        assert planned["success"], planned
        before = _snapshot(workflow)
        for omitted in (
            "explicit_current_mixer_approval",
            "explicit_set_wide_audible_approval",
            "explicit_current_routing_approval",
        ):
            for unavailable_approval in (False, None):
                result = _apply(client, planned["plan"], **{omitted: unavailable_approval})
                _refused(workflow, result, before, omitted)
        tampered = copy.deepcopy(planned["plan"])
        tampered["parts"][0]["part_id"] = 900
        _refused(workflow, _apply(client, tampered), before, "changed")
        _ok(client, "set_channel_level", graph_id=1, channel_id=1, level_db=-35.75)
        _refused(workflow, _apply(client, planned["plan"]), before, "revision")


def test_lost_static_mixer_reply_stops_project_then_queries_original_without_replay(
    coordinator_workflow,
):
    """Durable per-operation fences survive application restart, with no new journal."""
    workflow = coordinator_workflow
    with workflow.process() as client:
        authored = author_complete_portfolio(client, workflow)
        selected = _selection(client, authored, lane=False)
        plan = _plan(client, selected)
        assert plan["success"], plan
        workflow.drop_next_reply = "sunny_managed_update_static_mixer"
        lost = _apply(client, plan["plan"])
        assert lost["success"] is False and lost["receipt"]["outcome"] == "indeterminate", lost
        original = lost["attempt_id"]
        assert original in lost["phase_attempt_ids"]
        assert lost["phases"][-1]["tool"] == "project_realization_apply_static_mixer"
        assert lost["phases"][-1]["part_id"] == 1
        assert workflow.managed_calls.count("sunny_managed_update_static_mixer") == 1
        one, two = workflow.live.song.tracks[1:3]
        assert one.mixer_device.volume.writes == [0.5]
        assert two.mixer_device.volume.writes == []
        ledger = _attempts(workflow)
        record = next(item for item in ledger if item["intent"]["attempt_id"] == original)
        assert record["dispatch_state"] == "may_have_sent"
        assert record["evidence"][-1]["outcome"] == "indeterminate"
        native_before = _snapshot(workflow)
        blocked = _plan(client, selected)
        assert blocked["success"] is False and blocked["attempt_id"] == original, blocked
        assert _snapshot(workflow) == native_before
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        resumed = _ok(restarted, "project_realization_reconcile", attempt_id=original)
        assert resumed["receipt"]["outcome"] == "acknowledged"
        assert resumed["actual_receipt"]["outcome"] == "acknowledged"
        assert resumed["query_succeeded"] and resumed["mutation_retried"] is False
        assert workflow.managed_calls.count("sunny_managed_update_static_mixer") == 1
        assert one.mixer_device.volume.writes == [0.5]
        revised_plan = _plan(restarted, selected)
        assert revised_plan["success"], revised_plan
        _succeeded(workflow, _apply(restarted, revised_plan["plan"]))
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 2
        assert workflow.native_return_calls == ["create_return_track"]
        assert one.mixer_device.volume.writes == [0.5]
        assert two.mixer_device.volume.writes == [0.5]
        final = _ok(restarted, "project_realization_inspect", attempt_id=original)
        assert final["receipt"]["request"] == resumed["receipt"]["request"]
        assert final["receipt"]["journal"] == resumed["receipt"]["journal"]


def test_mute_only_retirement_preserves_solo_and_declares_remaining_set_effect(
    coordinator_workflow,
):
    """Mute is a scalar retirement policy; preserved Solo may still mute other Tracks."""
    workflow = coordinator_workflow
    from test_mcp_project_portfolio import author_two_parts

    with workflow.process() as client:
        author_two_parts(client)
        _ok(client, "set_channel_flags", graph_id=1, channel_id=1, mute=False, solo=False)
        _ok(client, "set_channel_flags", graph_id=1, channel_id=2, mute=False, solo=True)
        _ok(client, "workspace_save", path=workflow.server_workspace)
        selection = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [
                {"part_id": 1, "static_mixer": {"domains": ["mute", "solo"]}},
                {"part_id": 2, "static_mixer": {"domains": ["mute", "solo"]}},
            ],
            "routing": [],
        }
        planned = _plan(client, selection)
        assert planned["success"], planned
        _succeeded(workflow, _apply(client, planned["plan"]))
        one, retired = workflow.live.song.tracks[1:3]
        clip = retired.clip_slots[0].clip
        ids = tuple(clip._notes)
        assert one.solo is False and retired.solo is True
        assert one.muted_via_solo is True and one.mute is False
        _ok(client, "score_remove_part", score_id=1, part_id=2)
        selection = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [{"part_id": 1}],
            "routing": [],
            "retire_part_ids": [2],
        }
        planned = _plan(client, selection)
        assert planned["success"], planned
        # The coordinator's known policy is mute-only. Root adds these exact
        # declarations to the plan/final proof instead of claiming inaudibility.
        retirement = planned["plan"]["retirements"][0]
        assert retirement["retained_solo"] is True
        assert retirement["solo_policy"] == "preserve"
        result = _succeeded(workflow, _apply(client, planned["plan"]))
        final = next(item for item in result["final_native_observations"] if item["retirement"])
        assert final["retained_solo"] is True and final["solo_policy"] == "preserve"
        assert retired.mute is True and retired.mixer_device.track_activator.value == 0.0
        assert retired.solo is True and one.muted_via_solo is True
        assert_notes(clip, [(55, 0.0, 5.0, 72.0)])
        assert tuple(clip._notes) == ids and clip.end_marker == 8.0
        assert workflow.managed_calls.count("sunny_managed_create_clip") == 2


def test_existing_later_source_outside_native_display_domain_refuses_before_note_fence(
    coordinator_workflow,
):
    """A later current Source must admit its Hz target before earlier note revision."""
    from test_mcp_project_portfolio import author_two_parts

    workflow = coordinator_workflow
    with workflow.process() as client:
        author_two_parts(client)
        selections = {part: source_for(client, part) for part in (1, 2)}
        _ok(client, "workspace_save", path=workflow.server_workspace)
        selected = {
            "score_id": 1,
            "expected_project_revision": _revision(client),
            "parts": [{"part_id": part, "source_selections": selections[part]} for part in (1, 2)],
            "routing": [],
        }
        initial_plan = _plan(client, selected)
        assert initial_plan["success"], initial_plan
        _succeeded(workflow, _apply(client, initial_plan["plan"]))
        tracks = {part: workflow.live.song.tracks[part] for part in (1, 2)}
        clips = {part: tracks[part].clip_slots[0].clip for part in (1, 2)}
        ids = {part: tuple(clips[part]._notes) for part in (1, 2)}
        cutoff = next(
            value for value in tracks[2]._devices[0].parameters if value.original_name == "LP Freq"
        )
        # The independent provider is 200 + 4000*x*x Hz on [0,1].
        assert cutoff.min == 0.0 and cutoff.max == 1.0
        assert cutoff.str_for_value(1.0) == "4200.00 Hz"
        assert cutoff.value == 0.5
        _ok(
            client,
            "score_transpose",
            score_id=1,
            region={"start_bar": 1, "end_bar": 1, "parts": [1]},
            interval={"chromatic": 2, "diatonic": 1},
        )
        # 5000 Hz is valid owning IR, but cannot be expressed by this native descriptor.
        _ok(client, "set_parameter", profile_id=2, path="source.filter.cutoff", value=5000.0)
        selected["expected_project_revision"] = _revision(client)
        before = _snapshot(workflow)
        _refused(workflow, _plan(client, selected), before, "Source")
        for part in (1, 2):
            assert_notes(clips[part], INITIAL_NOTES[part])
            assert tuple(clips[part]._notes) == ids[part]
        assert cutoff.value == 0.5
