"""Offline active-Part edits recover old current music before one final revision.

The existing MCP/TCP provider replacement is a new-epoch source-contract model,
not actual Live Set persistence or historical native identity proof.
"""

from __future__ import annotations

import copy

import pytest
from test_mcp_native_effect_workflow import _native_writes
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import _attempts, _create_source
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_recovery_workflow import _replace_provider_document
from test_mcp_note_population_workflow import _events
from test_mcp_realization_workflow import _literal_notes, _realize, _revision
from test_mcp_workspace_workflow import _call as _ok

HISTORIC = "retained_verified_realization"
OLD_NOTES = [(101, 60, 0.0, 1.0, 80.0), (102, 67, 2.0, 0.5, 72.0)]
FINAL_NOTES = [
    (101, 60, 0.0, 1.0, 90.0),
    (102, 67, 2.0, 0.5, 72.0),
    (103, 64, 4.0, 1.0, 76.0),
]


def _preview(client, *, historical=False):
    arguments = {
        "score_id": 1,
        "part_id": 1,
        "expected_project_revision": _revision(client),
        "selector": {"track_index": 1, "slot_index": 0},
    }
    if historical:
        arguments["projection_source"] = HISTORIC
    return client.call("project_realization_preview_adoption", **arguments)


def _adopt(client, preview, **overrides):
    arguments = {
        "score_id": 1,
        "part_id": 1,
        "expected_project_revision": _revision(client),
        "preview": preview["preview"],
        "projection_source": HISTORIC,
        "historical_projection_attempt": preview["historical_projection_attempt"],
        "explicit_adoption": True,
    }
    arguments.update(overrides)
    return client.call("project_realization_adopt", **arguments)


def _assert_terminal_ack(receipt, original):
    # Reconciliation may add a current query diagnostic to error; the terminal
    # request, journal, context, outcome and delivery are immutable evidence.
    assert {key: value for key, value in receipt.items() if key != "error"} == {
        key: value for key, value in original.items() if key != "error"
    }
    assert receipt["outcome"] == "acknowledged"


def _authored(client):
    return _ok(client, "get_project_json", score_id=1)["documents"]


def _initial_pan(client):
    _ok(
        client,
        "add_mix_automation",
        graph_id=1,
        target="channels[1].spatial.pan",
        interpolation=0,
        breakpoints=[
            {"bar": 1, "beat_num": 0, "beat_den": 1, "value": -0.5},
            {"bar": 1, "beat_num": 1, "beat_den": 2, "value": 0.5},
        ],
    )
    result = client.call(
        "project_realization_author_mix_lane",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        lane_index=0,
    )
    assert result["success"] and result["native_lane_samples_verified"], result
    assert result["receipt"]["request"]["name"] == "sunny_managed_author_envelope"
    return result


@pytest.mark.parametrize("with_initial_pan", [False, True])
def test_offline_active_part_history_adoption_revises_notes_extent_and_meter(
    request, with_initial_pan
):
    """Current101/102 survive final8/2/2; old ACK is not replaced by UnknownEpoch."""
    workflow = request.getfixturevalue("mixer_workflow")
    with workflow.process() as client:
        old_track, old_clip = _create_source(client, workflow)
        if with_initial_pan:
            _initial_pan(client)
        source_record = copy.deepcopy(
            max(_attempts(workflow), key=lambda item: item["dispatch_ordinal"])
        )
        source_id = source_record["intent"]["attempt_id"]
        prior = _ok(client, "project_realization_inspect", attempt_id=source_id)
        assert prior["receipt"]["outcome"] == "acknowledged"
        terminal = copy.deepcopy(prior["receipt"])
        earlier_id = min(_attempts(workflow), key=lambda item: item["dispatch_ordinal"])["intent"][
            "attempt_id"
        ]
        assert earlier_id != source_id

        # Real offline authoring changes precede epoch replacement/adoption.
        # No temporary Score rollback or caller-supplied desired arrays.
        _ok(client, "score_modify_note", score_id=1, event_id=_events(client)[0]["id"], velocity=90)
        _ok(client, "score_insert_measures", score_id=1, after_bar=1, count=1)
        _ok(
            client,
            "score_insert_note",
            score_id=1,
            part_id=1,
            bar=2,
            offset={"n": 0, "d": 1},
            pitch={"letter": "E", "accidental": 0, "octave": 4},
            duration={"n": 1, "d": 4},
            velocity=76,
        )
        _ok(client, "score_set_time_signature", score_id=1, bar=1, groups=[2], denominator=2)
        final_revision = _revision(client)
        assert final_revision > source_record["intent"]["project_revision"]
        authored = copy.deepcopy(_authored(client))
        _ok(client, "workspace_save", path=workflow.server_workspace)
        assert _literal_notes(old_clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        assert old_clip.end_marker == 4.0

    track, clip = _replace_provider_document(workflow)
    assert track is not old_track and clip is not old_clip
    clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    clip._user_follow_actions = {"next": True}
    cohort = tuple(track._devices)
    device_writes = copy.deepcopy(_native_writes(track))
    gate_writes = list(workflow.native_mixer_gate_writes)
    native_count = len(workflow.live.song.tracks)
    with workflow.process() as client:
        assert _revision(client) == final_revision and _authored(client) == authored
        actual = _ok(client, "project_realization_reconcile", attempt_id=source_id)
        _assert_terminal_ack(actual["receipt"], terminal)
        assert actual["actual_receipt"]["outcome"] == "unknown_epoch"
        assert actual["actual_receipt"]["request"] == terminal["request"]
        assert actual["mutation_retried"] is False and actual["query_succeeded"]
        before = copy.deepcopy(_attempts(workflow))
        default = _preview(client)
        assert default["success"] and default["eligible_for_explicit_adoption"] is False, default
        assert default["current_score_projection_matches"] is False
        selected = _preview(client, historical=True)
        assert selected["success"] and selected["eligible_for_explicit_adoption"], selected
        assert selected["projection_source"] == HISTORIC
        assert selected["historical_projection_attempt"] == source_id
        assert selected["source_project_revision"] == source_record["intent"]["project_revision"]
        assert (
            selected["retained_projection_identity"]
            == source_record["intent"]["desired_note_identity"]
        )
        assert selected["project_revision"] == final_revision
        assert selected["historical_projection_matches"] is True
        assert selected["attack_associations_available"] is True
        # Historical eligibility never falsely qualifies the different final Score.
        assert selected["current_score_projection_matches"] is not True
        assert selected["authority_granted"] is False and selected["mutation_dispatched"] is False
        assert selected["historical_native_identity_restored"] is False
        assert _attempts(workflow) == before

        declined = [
            _adopt(client, selected, explicit_adoption=False),
            _adopt(client, selected, expected_project_revision=final_revision - 1),
            _adopt(client, selected, historical_projection_attempt=earlier_id),
            _adopt(client, selected, part_id=900),
            _adopt(client, selected, desired_projection={"clip_end": 8.0, "notes": []}),
            _adopt(client, selected, projection_source="caller_defined"),
        ]
        # Schema admission returns an error object before invoking the handler;
        # business admission returns success:false. Both must fail with a
        # concrete diagnostic and leave the ledger/native objects untouched.
        assert all(
            result.get("success") is not True and result.get("error") for result in declined
        ), declined
        assert "desired_projection" in declined[4]["error"]
        assert "projection_source" in declined[5]["error"]
        assert _attempts(workflow) == before
        assert _literal_notes(clip) == OLD_NOTES and clip.end_marker == 4.0
        assert clip.signature_numerator == 4 and clip.signature_denominator == 4
        assert tuple(track._devices) == cohort and _native_writes(track) == device_writes
        assert workflow.native_mixer_gate_writes == gate_writes
        assert _authored(client) == authored

        adopted = _adopt(client, selected)
        assert adopted["success"] and adopted["history_saved"], adopted
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert adopted["projection_source"] == HISTORIC
        assert adopted["historical_projection_attempt"] == source_id
        assert adopted["historical_native_identity_restored"] is False
        assert _literal_notes(clip) == OLD_NOTES and clip.end_marker == 4.0
        assert len(workflow.live.song.tracks) == native_count
        assert _authored(client) == authored and _revision(client) == final_revision

        revised = _realize(client, "project_realization_update")
        assert revised["success"] and revised["selected_clip_phases_completed"], revised
        phase_ids = revised["phase_attempt_ids"]
        assert len(phase_ids) == 3 and len(set(phase_ids)) == 3
        stages = [
            next(item for item in _attempts(workflow) if item["intent"]["attempt_id"] == token)
            for token in phase_ids
        ]
        assert [item["intent"]["prepared"]["request"]["name"] for item in stages] == [
            "sunny_managed_update_clip_geometry",
            "sunny_managed_revise_note_population",
            "sunny_managed_update_clip_geometry",
        ]
        assert all(item["intent"]["project_revision"] == final_revision for item in stages)
        projections = [item["intent"]["desired_projection"] for item in stages]
        assert [projection["clip_end"] for projection in projections] == [8.0, 8.0, 8.0]
        assert [(p["signature_numerator"], p["signature_denominator"]) for p in projections] == [
            (4, 4),
            (4, 4),
            (2, 2),
        ]
        assert [
            (note["pitch"], note["start_time"], note["duration"], note["velocity"])
            for note in projections[0]["notes"]
        ] == [(60, 0.0, 1.0, 80.0), (67, 2.0, 0.5, 72.0)]
        assert _literal_notes(clip) == FINAL_NOTES
        assert clip.end_marker == 8.0
        assert clip.signature_numerator == 2 and clip.signature_denominator == 2
        assert _revision(client) == final_revision and _authored(client) == authored
        assert tuple(track._devices) == cohort and _native_writes(track) == device_writes
        assert workflow.native_mixer_gate_writes == gate_writes
        assert clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
        assert clip._user_follow_actions == {"next": True}
        if with_initial_pan:
            envelope = clip.automation_envelope(track.mixer_device.panning)
            assert [envelope.value_at_time(time) for time in (0, 1, 2, 3)] == [-0.5, -0.5, 0.5, 0.5]
        retained = _ok(client, "project_realization_inspect", attempt_id=source_id)
        _assert_terminal_ack(retained["receipt"], terminal)
        source_after = next(
            item for item in _attempts(workflow) if item["intent"]["attempt_id"] == source_id
        )
        assert source_after["intent"] == source_record["intent"]
        assert (
            source_after["evidence"][: len(source_record["evidence"])] == source_record["evidence"]
        )
        assert (
            source_after["bindings"][: len(source_record["bindings"])] == source_record["bindings"]
        )
        assert len(workflow.live.song.tracks) == native_count
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_clip") == 1
