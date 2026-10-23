"""Finite after-repair oracle for historical removed-Part current-object recovery.

Use existing public Clip/Mixer preview and explicit adoption APIs. Provider document
replacement models a new native epoch; it is not actual Live Set persistence proof.
"""

from __future__ import annotations

import copy

import pytest
from test_mcp_native_effect_workflow import _effect_selections, _effects, _native_writes
from test_mcp_native_effect_workflow import effect_workflow as effect_workflow
from test_mcp_native_mixer_workflow import (
    _attempts,
    _create_source,
    _mixer_apply,
    _mixer_preview,
    _update_evidence,
)
from test_mcp_native_mixer_workflow import mixer_workflow as mixer_workflow
from test_mcp_native_recovery_workflow import _adopt, _preview, _replace_provider_document
from test_mcp_note_population_workflow import _events
from test_mcp_realization_workflow import _literal_notes, _realize, _revision
from test_mcp_workspace_workflow import _call as _ok


def _retire(client, explicit=True):
    return client.call(
        "project_realization_retire_part",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        explicit_mute_retirement=explicit,
    )


def _authored_documents(client):
    return _ok(client, "get_project_json", score_id=1)["documents"]


@pytest.mark.parametrize("with_initial_pan", [False, True])
def test_removed_part_new_epoch_clip_and_mute_adoption_never_resurrects_authoring(
    request, with_initial_pan
):
    """Latest verified velocity90 maps to new101/102 IDs; fresh approval grants mute only."""
    workflow = request.getfixturevalue("mixer_workflow")
    with workflow.process() as client:
        track, clip = _create_source(client, workflow)
        selections, _, _ = _effect_selections(client)
        assert _effects(client, selections)["success"]
        _ok(client, "score_modify_note", score_id=1, event_id=_events(client)[0]["id"], velocity=90)
        revised = _realize(client, "project_realization_update")
        assert revised["success"], revised
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 90.0), (2, 67, 2.0, 0.5, 72.0)]
        if with_initial_pan:
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
            initial = client.call(
                "project_realization_author_mix_lane",
                score_id=1,
                part_id=1,
                expected_project_revision=_revision(client),
                lane_index=0,
            )
            assert initial["success"] and initial["native_lane_samples_verified"], initial
            assert initial["receipt"]["request"]["name"] == "sunny_managed_author_envelope"
            old_envelope = clip.automation_envelope(track.mixer_device.panning)
            assert [old_envelope.value_at_time(time) for time in (0, 1, 2, 3)] == [
                -0.5,
                -0.5,
                0.5,
                0.5,
            ]
            # Remove owning references before Part deletion. Native envelope
            # state remains preserved; retirement selects only mute.
            _ok(client, "remove_mix_automation", graph_id=1, index=0)
        added = _ok(
            client, "score_add_part", score_id=1, name="Remaining Piano", instrument_type=47
        )
        assert added["part_id"] == 2
        _ok(client, "score_remove_part", score_id=1, part_id=1)
        authored = copy.deepcopy(_authored_documents(client))
        assert [part["id"] for part in authored["score"]["parts"]] == [2]
        assert [channel["part_id"] for channel in authored["mix"]["channels"]] == [2]
        _ok(client, "workspace_save", path=workflow.server_workspace)
        retired = _retire(client)
        journal, evidence = _update_evidence(retired)
        assert journal["native_mutation_started"] is True
        assert evidence["returned_fields"] == ["mute"]
        assert track.mute is True and track.mixer_device.track_activator.value == 0.0
        retired_token = retired["attempt_id"]
        terminal_journal = copy.deepcopy(retired["receipt"]["journal"])
        terminal_request = copy.deepcopy(retired["receipt"]["request"])
        old_track, old_clip = track, clip
    track, clip = _replace_provider_document(workflow)
    assert track is not old_track and clip is not old_clip
    # Genuine current native user state is part of the preview. Recovery may
    # grant selected controls on it only after explicit approval; no saved
    # logical token, tag or old value silently restores historical ownership.
    track._mute = False
    track.mixer_device.track_activator._value = 1.0
    clip._user_mpe_expression = {101: {"pressure": [0.2, 0.9]}}
    clip._user_follow_actions = {"next": True}
    volume = track.mixer_device.volume
    volume_calls, volume_writes = list(volume.calls), list(volume.writes)
    device_cohort, device_writes = tuple(track._devices), copy.deepcopy(_native_writes(track))
    gate_writes = list(workflow.native_mixer_gate_writes)
    native_count = len(workflow.live.song.tracks)

    def forbidden_formatter(_):
        raise AssertionError("historical mute recovery must not format native volume")

    volume.oracle = forbidden_formatter
    current_envelope = clip.automation_envelope(track.mixer_device.panning)
    if with_initial_pan:
        assert current_envelope is not old_envelope
        assert [current_envelope.value_at_time(time) for time in (0, 1, 2, 3)] == [
            -0.5,
            -0.5,
            0.5,
            0.5,
        ]
    with workflow.process() as reopened:
        inspected = _ok(reopened, "project_realization_inspect", attempt_id=retired_token)
        assert inspected["receipt"]["outcome"] == "acknowledged"
        historical = _ok(reopened, "project_realization_reconcile", attempt_id=retired_token)
        # Historical terminal evidence and current query evidence are two
        # independent domains. An actual unknown epoch never erases the ACK,
        # changes its request/journal or authorizes a mutation replay.
        assert historical["receipt"]["outcome"] == "acknowledged"
        assert historical["receipt"]["journal"] == terminal_journal
        assert historical["receipt"]["request"] == terminal_request
        assert historical["actual_receipt"]["outcome"] == "unknown_epoch"
        current_journal = historical["actual_receipt"]["journal"]
        assert set(current_journal) == {"outcome", "document_token"}
        assert current_journal["outcome"] == "unknown_epoch"
        assert (
            isinstance(current_journal["document_token"], str) and current_journal["document_token"]
        )
        assert current_journal["document_token"] != retired["receipt"]["context"]["document_token"]
        assert historical["actual_receipt"]["request"] == terminal_request
        assert historical["actual_receipt"]["context"] == retired["receipt"]["context"]
        assert historical["attempt_id"] == retired_token
        assert historical["query_succeeded"] and historical["history_saved"]
        assert historical["mutation_retried"] is False
        before = _attempts(workflow)
        unavailable = _retire(reopened)
        assert unavailable["success"] is False
        assert _attempts(workflow) == before
        preview = _preview(reopened)
        assert preview["success"] and preview["eligible_for_explicit_adoption"], preview
        assert current_journal == {
            "outcome": "unknown_epoch",
            "document_token": preview["preview"]["context"]["document_token"],
        }
        assert preview["authority_granted"] is False and preview["mutation_dispatched"] is False
        assert preview["historical_native_identity_restored"] is False
        assert preview["current_score_projection_matches"] is None
        assert preview["projection_source"] == "retained_verified_removed_part"
        assert preview["historical_projection_attempt"] == retired_token
        assert preview["historical_projection_matches"] is True
        assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 90.0), (102, 67, 2.0, 0.5, 72.0)]
        unapproved = _adopt(reopened, preview["preview"], explicit=False)
        assert unapproved["success"] is False
        assert _attempts(workflow) == before
        assert workflow.native_mixer_gate_writes == gate_writes
        assert _authored_documents(reopened) == authored
        adopted = _adopt(reopened, preview["preview"])
        assert adopted["success"] and adopted["history_saved"], adopted
        assert adopted["receipt"]["journal"]["native_mutation_started"] is False
        assert adopted["historical_native_identity_restored"] is False
        assert adopted["earlier_attempt_history_retained"] is True
        assert adopted["projection_source"] == "retained_verified_removed_part"
        assert adopted["historical_projection_attempt"] == retired_token
        after_clip = _attempts(workflow)
        # Clip adoption preserves devices and current gate state but confers
        # no static Mixer control. The owning author IR still has no Part1.
        unavailable = _retire(reopened)
        assert unavailable["success"] is False
        assert _attempts(workflow) == after_clip
        unavailable_update = _mixer_preview(reopened, ["mute"], purpose="update")
        assert unavailable_update["success"] is False
        assert _attempts(workflow) == after_clip
        mixer = _mixer_preview(reopened, ["mute"], purpose="adopt")
        assert mixer["success"] and mixer["authority_granted"] is False, mixer
        assert mixer["plan"]["desired"] == {"mute": True}
        assert mixer["plan"]["projection_source"] == "retained_verified_removed_part"
        assert mixer["plan"]["current_mix_channel"] is None
        assert mixer["plan"]["historical_projection_attempt"] == adopted["attempt_id"]
        assert mixer["preview"]["preview"]["current_authority_domains"] == []
        assert _attempts(workflow) == after_clip
        grant = _mixer_apply(reopened, mixer["preview"], ["mute"], adoption=True)
        assert grant["success"] and grant["history_saved"], grant
        grant_journal = grant["receipt"]["journal"]
        assert grant_journal["native_mutation_started"] is False
        assert grant_journal["result"]["mixer_adoption"]["granted_domains"] == ["mute"]
        assert track.mute is False and track.mixer_device.track_activator.value == 1.0
        assert volume.calls == volume_calls and volume.writes == volume_writes
        assert workflow.native_mixer_gate_writes == gate_writes
        current_retirement = _retire(reopened)
        current_journal, current_evidence = _update_evidence(current_retirement)
        assert current_journal["native_mutation_started"] is True
        assert current_evidence["returned_fields"] == ["mute"]
        assert track.mute is True and track.mixer_device.track_activator.value == 0.0
        repeat = _retire(reopened)
        noop_journal, noop_evidence = _update_evidence(repeat)
        assert noop_journal["native_mutation_started"] is False
        assert noop_evidence["returned_fields"] == []
        assert workflow.native_mixer_gate_writes == [*gate_writes, (track, "mute", True)]
        assert _authored_documents(reopened) == authored
        assert _literal_notes(clip) == [(101, 60, 0.0, 1.0, 90.0), (102, 67, 2.0, 0.5, 72.0)]
        assert tuple(track._devices) == device_cohort and _native_writes(track) == device_writes
        assert volume.calls == volume_calls and volume.writes == volume_writes
        assert len(workflow.live.song.tracks) == native_count
        assert clip.automation_envelope(track.mixer_device.panning) is current_envelope
        if with_initial_pan:
            assert [current_envelope.value_at_time(time) for time in (0, 1, 2, 3)] == [
                -0.5,
                -0.5,
                0.5,
                0.5,
            ]
        assert clip._user_mpe_expression == {101: {"pressure": [0.2, 0.9]}}
        assert clip._user_follow_actions == {"next": True}
        # No deleted authoring Channel means no volume/pan/solo realization.
        before_escalation = _attempts(workflow)
        for domains in (["pan"], ["solo"], ["mute", "pan"]):
            declined = _mixer_preview(reopened, domains, purpose="adopt")
            assert declined["success"] is False, declined
            assert _attempts(workflow) == before_escalation
        absent = reopened.call(
            "project_realization_preview_adoption",
            score_id=1,
            part_id=900,
            expected_project_revision=_revision(reopened),
            selector={"track_index": 1, "slot_index": 0},
        )
        assert absent["success"] is False
        assert _attempts(workflow) == before_escalation
        retained = _ok(reopened, "project_realization_inspect", attempt_id=retired_token)
        assert retained["receipt"]["outcome"] == "acknowledged"
        assert retained["receipt"]["journal"] == terminal_journal
        assert retained["receipt"]["request"] == terminal_request
    assert workflow.managed_calls.count("sunny_managed_create_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_clip") == 1
    assert workflow.managed_calls.count("sunny_managed_adopt_static_mixer") == 1
