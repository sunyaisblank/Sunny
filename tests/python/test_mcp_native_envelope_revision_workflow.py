"""Explicit selected Pan replacement through real MCP/TCP and durable token history.

The external provider supplies finite native objects and interval readbacks. These
cases do not qualify a Live host, complete breakpoint capture or Set persistence.
"""

from __future__ import annotations

import json
import threading
from types import SimpleNamespace

import pytest
from live_model import AutomationEnvelope, Clip, panning_parameter
from test_mcp_mix_realization_workflow import _apply, _lane
from test_mcp_realization_workflow import (
    NativeWorkflow,
    _author,
    _literal_notes,
    _ok,
    _realize,
    _revision,
)


@pytest.fixture
def replacement_workflow(tmp_path, monkeypatch):
    """Count actual native envelope callbacks only on the designated Live thread."""
    workflow = NativeWorkflow(tmp_path, monkeypatch)
    workflow.live.song.create_return_track()
    native_calls = []
    provider_create = Clip.create_automation_envelope
    provider_insert = AutomationEnvelope.insert_step
    for name in ("clear_envelope", "create_automation_envelope"):
        original = getattr(Clip, name)

        def callback(clip, parameter, name=name, original=original):
            assert threading.get_ident() == workflow.main_thread._thread.ident
            native_calls.append((name, clip, parameter))
            return original(clip, parameter)

        monkeypatch.setattr(Clip, name, callback)

    def insert(envelope, start, duration, value):
        assert threading.get_ident() == workflow.main_thread._thread.ident
        native_calls.append(("insert_step", envelope, start, duration, value))
        return provider_insert(envelope, start, duration, value)

    monkeypatch.setattr(AutomationEnvelope, "insert_step", insert)
    boundary = SimpleNamespace(
        workflow=workflow,
        native_calls=native_calls,
        provider_create=provider_create,
        provider_insert=provider_insert,
    )
    try:
        yield boundary
    finally:
        workflow.close()


def _prepare(client, boundary):
    workflow = boundary.workflow
    _author(client, workflow)
    assert _realize(client, "project_realization_create")["success"]
    _ok(
        client,
        "add_mix_automation",
        graph_id=1,
        target="channels[1].spatial.pan",
        interpolation=0,
        breakpoints=[{"bar": 1, "beat_num": 0, "beat_den": 1, "value": 0.0}],
    )
    original = _apply(client)
    assert original["success"] and original["history_saved"], original
    track = workflow.live.song.tracks[1]
    slot, clip = track.clip_slots[0], track.clip_slots[0].clip
    pan = track.mixer_device.panning
    envelope = clip.automation_envelope(pan)
    assert envelope is not None and envelope.value_at_time(1.0) == 0.0
    _ok(client, "remove_mix_automation", graph_id=1, index=0)
    _lane(client)
    boundary.native_calls.clear()
    return SimpleNamespace(track=track, slot=slot, clip=clip, pan=pan, envelope=envelope)


def _preview(client):
    return client.call(
        "project_realization_preview_mix_lane_replacement",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        lane_index=0,
    )


def _replace(client, preview, **approvals):
    return client.call(
        "project_realization_replace_mix_lane",
        score_id=1,
        part_id=1,
        expected_project_revision=_revision(client),
        lane_index=0,
        preview=preview,
        **approvals,
    )


def _approved(client, preview):
    return _replace(
        client,
        preview,
        explicit_selected_envelope_replacement=True,
        allow_unsampled_selected_state_overwrite=True,
    )


def _assert_replacement_calls(boundary, native):
    actual = native.clip.automation_envelope(native.pan)
    assert actual is not None and actual is not native.envelope
    assert boundary.native_calls == [
        ("clear_envelope", native.clip, native.pan),
        ("create_automation_envelope", native.clip, native.pan),
        ("insert_step", actual, 0.0, 2.0, -0.5),
        ("insert_step", actual, 2.0, 2.0, 0.5),
    ]
    return actual


def test_explicit_whole_pan_overwrite_removes_unsampled_spikes_and_preserves_other_state(
    replacement_workflow,
):
    """Samples at1/3 cannot prove absence of user spikes at0.45/0.65."""
    boundary = replacement_workflow
    workflow = boundary.workflow
    user_track, user_scene, user_return = (
        workflow.live.song.tracks[0],
        workflow.live.song.scenes[0],
        workflow.live.song.return_tracks[0],
    )
    user_track.name = "Unrelated user track"
    user_track.clip_slots[0].create_clip(8.0)
    user_clip = user_track.clip_slots[0].clip
    with workflow.process() as client:
        native = _prepare(client, boundary)
        clip = native.clip
        old_notes = dict(clip._notes)
        other_parameter = native.track.mixer_device.sends[0]
        other_envelope = boundary.provider_create(clip, other_parameter)
        boundary.provider_insert(other_envelope, 0.0, 4.0, 0.67)
        boundary.provider_insert(native.envelope, 0.4, 0.1, 0.9)
        clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
        clip._user_follow_actions = {"next": True}
        assert native.envelope.value_at_time(0.45) == 0.9
        ledger_before = workflow.history_ledger().read_bytes()
        preview = _preview(client)
        assert preview["success"], preview
        assert workflow.history_ledger().read_bytes() == ledger_before
        assert boundary.native_calls == []
        raw = preview["preview"]
        assert raw["lane"] == {
            "parameter": {"kind": "panning"},
            "interpolation": "step",
            "clip_end": 4.0,
            "points": [{"time": 0.0, "value": -0.5}, {"time": 2.0, "value": 0.5}],
        }
        assert raw["selected_envelope"]["samples"] == [
            {"time": 1.0, "value": 0.0},
            {"time": 3.0, "value": 0.0},
        ]
        assert raw["scope"]["breakpoint_population_observed"] is False
        assert raw["scope"]["unsampled_state_preservation_proven"] is False
        assert raw["scope"]["same_parameter_modulation_preservation_proven"] is False
        # Unsampled selected state can change after preview under the exact explicit grant.
        boundary.provider_insert(native.envelope, 0.6, 0.1, -0.8)
        assert native.envelope.value_at_time(0.65) == -0.8
        replaced = _approved(client, raw)
        assert replaced["success"] and replaced["history_saved"], replaced
        assert replaced["dispatch_fenced"] and replaced["retry_authorized"] is False
        actual = _assert_replacement_calls(boundary, native)
        assert [actual.value_at_time(t) for t in (0.45, 0.65, 1.0, 3.0)] == [
            -0.5,
            -0.5,
            -0.5,
            0.5,
        ]
        update = replaced["receipt"]["journal"]["result"]["envelope_replacement"]
        assert update["actual_samples"] == [
            {"time": 1.0, "value": -0.5},
            {"time": 3.0, "value": 0.5},
        ]
        assert all(
            update[field]
            for field in (
                "observed_step_samples_match_request",
                "note_ids_and_values_preserved",
                "other_finite_properties_preserved",
                "device_identity_preserved",
            )
        )
        assert clip.automation_envelope(other_parameter) is other_envelope
        assert other_envelope.value_at_time(0.5) == 0.67
        assert all(clip._notes[note_id] is note for note_id, note in old_notes.items())
        assert _literal_notes(clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        assert workflow.live.song.tracks[1] is native.track and native.slot.clip is clip
        assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
        assert clip._user_follow_actions == {"next": True}
        assert workflow.native_apply_calls == []
        assert (
            workflow.live.song.tracks[0] is user_track
            and user_track.clip_slots[0].clip is user_clip
        )
        assert workflow.live.song.scenes[0] is user_scene
        assert workflow.live.song.return_tracks[0] is user_return
        assert user_track.name == "Unrelated user track" and user_clip.end_marker == 8.0
        _ok(client, "workspace_save", path=workflow.server_workspace)
    with workflow.process() as restarted:
        retained = _ok(restarted, "project_realization_inspect", attempt_id=replaced["attempt_id"])
        assert retained["receipt"] == replaced["receipt"]
        assert native.slot.clip is clip and clip.automation_envelope(native.pan) is actual
        assert clip.automation_envelope(other_parameter) is other_envelope
        _assert_replacement_calls(boundary, native)
    assert workflow.managed_calls.count("sunny_managed_replace_envelope") == 1


@pytest.mark.parametrize(
    "field", ["explicit_selected_envelope_replacement", "allow_unsampled_selected_state_overwrite"]
)
@pytest.mark.parametrize("mode", ["missing", "false"])
def test_each_selected_overwrite_approval_is_required_before_dispatch(
    replacement_workflow, field, mode
):
    """Missing or false approval changes neither native objects nor durable membership."""
    boundary = replacement_workflow
    workflow = boundary.workflow
    with workflow.process() as client:
        native = _prepare(client, boundary)
        preview = _preview(client)
        assert preview["success"], preview
        approvals = {
            "explicit_selected_envelope_replacement": True,
            "allow_unsampled_selected_state_overwrite": True,
        }
        if mode == "missing":
            del approvals[field]
        else:
            approvals[field] = False
        before = workflow.history_ledger().read_bytes()
        rejected = _replace(client, preview["preview"], **approvals)
        assert rejected.get("success") is not True and "error" in rejected, rejected
        assert workflow.history_ledger().read_bytes() == before
        assert "sunny_managed_replace_envelope" not in workflow.managed_calls
        assert boundary.native_calls == []
        assert native.clip.automation_envelope(native.pan) is native.envelope
        assert native.envelope.value_at_time(1.0) == 0.0
        assert _literal_notes(native.clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        accepted = _approved(client, preview["preview"])
        assert accepted["success"], accepted
        _assert_replacement_calls(boundary, native)


def test_lost_replacement_reply_reconciles_original_token_without_a_second_clear(
    replacement_workflow,
):
    """One native replacement completes before TCP loss; restart permits only the original query."""
    boundary = replacement_workflow
    workflow = boundary.workflow
    with workflow.process() as client:
        native = _prepare(client, boundary)
        preview = _preview(client)
        assert preview["success"], preview
        _ok(client, "workspace_save", path=workflow.server_workspace)
        workflow.drop_next_reply = "sunny_managed_replace_envelope"
        uncertain = _approved(client, preview["preview"])
        assert not uncertain["success"] and uncertain["history_saved"], uncertain
        assert uncertain["receipt"]["outcome"] == "indeterminate"
        assert uncertain["receipt"]["delivery"] == "sent_without_valid_response"
        actual = _assert_replacement_calls(boundary, native)
        assert actual.value_at_time(1.0) == -0.5 and actual.value_at_time(3.0) == 0.5
        ledger_before = workflow.history_ledger().read_bytes()
        blocked = _approved(client, preview["preview"])
        assert not blocked["success"] and blocked["state"] == "reconciliation_required", blocked
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        assert workflow.history_ledger().read_bytes() == ledger_before
        _assert_replacement_calls(boundary, native)
    with workflow.process() as restarted:
        blocked = _approved(restarted, preview["preview"])
        assert not blocked["success"] and blocked["state"] == "reconciliation_required", blocked
        assert blocked["attempt_id"] == uncertain["attempt_id"]
        reconciled = _ok(
            restarted, "project_realization_reconcile", attempt_id=uncertain["attempt_id"]
        )
        assert reconciled["query_succeeded"] and reconciled["history_saved"]
        assert reconciled["receipt"]["outcome"] == "acknowledged"
        assert reconciled["actual_receipt"]["outcome"] == "acknowledged"
        assert not reconciled["mutation_retried"] and reconciled["retry_authorized"] is False
        assert native.slot.clip is native.clip
        assert native.clip.automation_envelope(native.pan) is actual
        assert _literal_notes(native.clip) == [(1, 60, 0.0, 1.0, 80.0), (2, 67, 2.0, 0.5, 72.0)]
        _assert_replacement_calls(boundary, native)
        attempts = json.loads(workflow.history_ledger().read_text())["attempts"]
        assert len(attempts) == 3
        retained_attempt = next(
            attempt
            for attempt in attempts
            if attempt["intent"]["attempt_id"] == uncertain["attempt_id"]
        )
        assert retained_attempt["dispatch_ordinal"] == 3
    assert workflow.managed_calls.count("sunny_managed_replace_envelope") == 1
    assert workflow.managed_calls.count("sunny_managed_operation") == 1
    assert workflow.native_apply_calls == []


@pytest.mark.parametrize("drift", ["sample", "envelope", "parameter", "note"])
def test_sample_or_actual_identity_drift_after_preview_declines_before_any_native_clear(
    replacement_workflow, drift
):
    """Equal sample/descriptor bytes do not transfer an approved envelope or parameter handle."""
    boundary = replacement_workflow
    workflow = boundary.workflow
    with workflow.process() as client:
        native = _prepare(client, boundary)
        preview = _preview(client)
        assert preview["success"], preview
        if drift == "sample":
            boundary.provider_insert(native.envelope, 0.9, 0.2, 0.8)
            assert native.envelope.value_at_time(1.0) == 0.8
        elif drift == "envelope":
            replacement = AutomationEnvelope(native.pan)
            boundary.provider_insert(replacement, 0.0, 4.0, 0.0)
            native.clip._automation_envelopes[native.pan] = replacement
            assert replacement is not native.envelope
            assert replacement.value_at_time(1.0) == native.envelope.value_at_time(1.0)
        elif drift == "parameter":
            replacement = panning_parameter()
            replacement._canonical_parent = native.track.mixer_device
            native.track.mixer_device._panning = replacement
            native.clip._automation_envelopes[replacement] = native.envelope
            assert replacement is not native.pan and replacement.value == native.pan.value
        else:
            native.clip._notes[1].velocity = 42.0
        current_parameter = native.track.mixer_device.panning
        current_envelope = native.clip.automation_envelope(current_parameter)
        rejected = _approved(client, preview["preview"])
        assert not rejected["success"] and rejected["history_saved"], rejected
        assert rejected["receipt"]["outcome"] == "declined"
        assert rejected["receipt"]["journal"]["native_mutation_started"] is False
        assert boundary.native_calls == []
        assert native.clip.automation_envelope(current_parameter) is current_envelope
        assert native.slot.clip is native.clip
        assert native.clip._notes[1].velocity == (42.0 if drift == "note" else 80.0)
        assert workflow.managed_calls.count("sunny_managed_replace_envelope") == 1
        assert workflow.native_apply_calls == []
