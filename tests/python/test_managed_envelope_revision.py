"""Explicit selected-lane overwrite oracles, without host persistence claims."""

from __future__ import annotations

from typing import Any

import pytest
from live_model import AutomationEnvelope, Clip
from Sunny.managed_envelope_revision import PREVIEW_METHOD, REPLACE_METHOD, valid_revision_request
from test_managed_realization import call, intent, observe
from test_managed_realization import target as managed_target


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Create only through the actual managed registry and strict native boundary."""
    target = managed_target.__wrapped__(monkeypatch)
    created = call(target, "sunny_managed_create_clip", intent(target))
    assert created["outcome"] == "acknowledged"
    target.record = target.registry._bindings[("project_a", "part_a")]
    target.clip = target.record["clip"]
    target.pan = target.record["track"].mixer_device.panning
    target.send = target.record["track"].mixer_device.sends[0]
    target.pan_env = target.clip.create_automation_envelope(target.pan)
    target.pan_env.insert_step(0.0, 4.0, 0.0)
    target.send_env = target.clip.create_automation_envelope(target.send)
    target.send_env.insert_step(0.0, 4.0, 0.67)
    target.clip._user_mpe_expression = {1: {"pressure": [(0.1, 0.7)]}}
    target.clip._user_follow_actions = {"next": True}
    target.registry._seal(target.record)
    target.calls = []
    target.actions = {}
    for name in ("clear_envelope", "create_automation_envelope"):
        original = getattr(Clip, name)

        def wrapped(clip: Any, parameter: Any, name: str = name, original: Any = original) -> Any:
            if clip is not target.clip:
                return original(clip, parameter)
            target.calls.append((name, parameter))
            action = target.actions.get(name)
            if action == "raise_before":
                raise RuntimeError("native failed before effect")
            if action == "ignore":
                return None
            value = original(clip, parameter)
            if callable(action):
                action(clip)
            if action == "raise_after":
                raise RuntimeError("native failed after effect")
            return value

        monkeypatch.setattr(Clip, name, wrapped)
    original_insert = AutomationEnvelope.insert_step

    def insert(envelope: Any, start: float, duration: float, value: float) -> None:
        if envelope._parameter is not target.pan:
            original_insert(envelope, start, duration, value)
            return
        target.calls.append(("insert_step", start, duration, value))
        action = target.actions.get("insert_step")
        if action == "raise_before":
            raise RuntimeError("native failed before effect")
        if action == "ignore":
            return
        original_insert(envelope, start, duration, value)
        if callable(action):
            action(target.clip)
        if action == "raise_after":
            raise RuntimeError("native failed after effect")

    monkeypatch.setattr(AutomationEnvelope, "insert_step", insert)
    return target


def preview_request(target: Any) -> dict[str, Any]:
    """Use the current finite content guard and a literal two-interval Pan lane."""
    return {
        "document_token": target.context["document_token"],
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": observe(target)["observation"]["content_fingerprint"],
        "lane": {
            "parameter": {"kind": "panning"},
            "interpolation": "step",
            "clip_end": 4.0,
            "points": [{"time": 0.0, "value": -0.5}, {"time": 2.0, "value": 0.5}],
        },
    }


def approval(target: Any, preview: Any) -> dict[str, Any]:
    """Approve whole selected state, including its unobserved unsampled population."""
    return {
        "document_token": target.context["document_token"],
        "operation_id": "replace_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "preview_token": preview["preview_token"],
        "preview_fingerprint": preview["preview_fingerprint"],
        "explicit_selected_envelope_replacement": True,
        "allow_unsampled_selected_state_overwrite": True,
    }


def test_explicit_unsampled_spike_overwrite_preserves_other_domains(target: Any) -> None:
    """Identical finite samples never prove absence of an unsampled user curve."""
    target.pan_env.insert_step(0.4, 0.1, 0.9)
    target.calls.clear()
    old_notes = dict(target.clip._notes)
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    assert target.calls == [] and target.registry._author_context is None
    assert preview["selected_envelope"]["samples"] == [
        {"time": 1.0, "value": 0.0},
        {"time": 3.0, "value": 0.0},
    ]
    assert preview["scope"]["breakpoint_population_observed"] is False
    assert preview["scope"]["unsampled_state_preservation_proven"] is False
    assert preview["scope"]["same_parameter_modulation_preservation_proven"] is False
    assert preview["native_step_call_budget"] == 64
    # Additional unsampled drift after the preview remains explicitly overwriteable.
    target.pan_env.insert_step(0.6, 0.1, -0.8)
    target.calls.clear()
    request = approval(target, preview)
    result = call(target, REPLACE_METHOD, request)
    assert result["outcome"] == "acknowledged", result
    assert result["native_mutation_started"] is True
    assert target.calls == [
        ("clear_envelope", target.pan),
        ("create_automation_envelope", target.pan),
        ("insert_step", 0.0, 2.0, -0.5),
        ("insert_step", 2.0, 2.0, 0.5),
    ]
    actual = target.clip.automation_envelope(target.pan)
    assert actual is not target.pan_env
    assert actual.value_at_time(0.45) == -0.5 and actual.value_at_time(0.65) == -0.5
    assert actual.value_at_time(1.0) == -0.5 and actual.value_at_time(3.0) == 0.5
    assert target.clip.automation_envelope(target.send) is target.send_env
    assert target.send_env.value_at_time(0.5) == 0.67
    assert all(target.clip._notes[k] is n for k, n in old_notes.items())
    assert target.clip._user_mpe_expression == {1: {"pressure": [(0.1, 0.7)]}}
    assert target.clip._user_follow_actions == {"next": True}
    update = result["result"]["envelope_replacement"]
    assert update["actual_samples"] == [{"time": 1.0, "value": -0.5}, {"time": 3.0, "value": 0.5}]
    assert all(
        update[k]
        for k in (
            "observed_step_samples_match_request",
            "note_ids_and_values_preserved",
            "other_finite_properties_preserved",
            "device_identity_preserved",
        )
    )
    prior_calls = list(target.calls)
    assert call(target, REPLACE_METHOD, request) == result
    assert target.calls == prior_calls
    query = {"document_token": target.context["document_token"], "operation_id": "replace_a"}
    assert call(target, "sunny_managed_operation", query) == result
    assert target.calls == prior_calls


@pytest.mark.parametrize(
    "field", ["explicit_selected_envelope_replacement", "allow_unsampled_selected_state_overwrite"]
)
def test_both_literal_overwrite_approvals_are_required(target: Any, field: str) -> None:
    """A preview never authorizes destructive selected-lane writes by itself."""
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    request = approval(target, preview)
    request[field] = False
    assert not valid_revision_request(REPLACE_METHOD, [request])
    assert target.calls == [] and target.clip.automation_envelope(target.pan) is target.pan_env


@pytest.mark.parametrize("drift", ["sample", "envelope", "note", "armed", "parameter", "override"])
def test_current_sampled_or_identity_drift_declines_before_clear(target: Any, drift: str) -> None:
    """Every observed touched-domain guard is rechecked against native handles."""
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    if drift == "sample":
        target.pan_env.insert_step(0.9, 0.2, 0.8)
    elif drift == "envelope":
        target.clip._automation_envelopes[target.pan] = AutomationEnvelope(target.pan)
    elif drift == "note":
        target.clip._notes[1].velocity = 42.0
    elif drift == "armed":
        target.record["track"].arm = True
    elif drift == "parameter":
        target.pan._is_enabled = False
    else:
        target.pan._automation_state = 2
    target.calls.clear()
    result = call(target, REPLACE_METHOD, approval(target, preview))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert target.calls == []


@pytest.mark.parametrize("name", ["clear_envelope", "create_automation_envelope", "insert_step"])
@pytest.mark.parametrize("failure", ["raise_before", "raise_after", "ignore"])
def test_phase_faults_stop_without_replay_or_compensation(
    target: Any, name: str, failure: str
) -> None:
    """Partial actual lane handles and token evidence survive every native phase fault."""
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    target.actions[name] = failure
    request = approval(target, preview)
    result = call(target, REPLACE_METHOD, request)
    assert result["outcome"] == "indeterminate", result
    assert result["native_mutation_started"] is True
    names = [call[0] for call in target.calls]
    assert (
        names
        == {
            "clear_envelope": ["clear_envelope"],
            "create_automation_envelope": ["clear_envelope", "create_automation_envelope"],
            "insert_step": ["clear_envelope", "create_automation_envelope", "insert_step"],
        }[name]
    )
    assert target.clip.automation_envelope(target.send) is target.send_env
    prior_calls = list(target.calls)
    assert call(target, REPLACE_METHOD, request) == result
    assert target.calls == prior_calls


def test_absent_selected_lane_and_same_parameter_unknown_modulation(target: Any) -> None:
    """Absence skips clear; unknown same-parameter modulation has no preservation claim."""
    target.clip.clear_envelope(target.pan)
    target.registry._seal(target.record)
    target.calls.clear()
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    assert preview["selected_envelope"]["has_envelope"] is False
    assert preview["selected_envelope"]["samples"] == []
    result = call(target, REPLACE_METHOD, approval(target, preview))
    assert result["outcome"] == "acknowledged", result
    assert [call[0] for call in target.calls] == [
        "create_automation_envelope",
        "insert_step",
        "insert_step",
    ]
    assert result["result"]["envelope_replacement"]["actual_samples"] == [
        {"time": 1.0, "value": -0.5},
        {"time": 3.0, "value": 0.5},
    ]


def test_native_step_budget_and_exact_sample_interior_bound(target: Any) -> None:
    """Reject large call budgets and unrepresentable interior times before preview publication."""
    request = preview_request(target)
    request["lane"]["points"] = [{"time": i / 64, "value": 0.0} for i in range(65)]
    assert not valid_revision_request(PREVIEW_METHOD, [request])
    assert target.calls == []
    request = preview_request(target)
    request["lane"]["points"] = [
        {"time": 0.0, "value": 0.0},
        {"time": 3.9999999999999996, "value": 0.0},
    ]
    with pytest.raises(AssertionError, match="no exact finite interior"):
        call(target, PREVIEW_METHOD, request)
    assert target.calls == [] and target.registry._envelope_revision._previews == {}


@pytest.mark.parametrize("phase", ["clear_envelope", "create_automation_envelope", "insert_step"])
def test_unexpected_native_note_change_stops_remaining_phases(target: Any, phase: str) -> None:
    """Native phase guards compare retained ID-to-value data rather than a request echo."""
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    target.actions[phase] = lambda clip: setattr(clip._notes[1], "velocity", 42.0)
    result = call(target, REPLACE_METHOD, approval(target, preview))
    assert result["outcome"] == "indeterminate" and result["native_mutation_started"] is True
    assert [call[0] for call in target.calls] == {
        "clear_envelope": ["clear_envelope"],
        "create_automation_envelope": ["clear_envelope", "create_automation_envelope"],
        "insert_step": ["clear_envelope", "create_automation_envelope", "insert_step"],
    }[phase]
    assert result["progress"]["last_observation"]["note_identity"]["notes"][0]["velocity"] == 42.0


def test_same_parameter_modulation_preservation_is_deliberately_unproven(target: Any) -> None:
    """A source-boundary clear may affect an inaccessible modulation domain under approval."""
    target.clip._same_parameter_modulation = {"unobserved": [(0.25, 0.75)]}
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    target.actions["clear_envelope"] = lambda clip: setattr(
        clip, "_same_parameter_modulation", None
    )
    result = call(target, REPLACE_METHOD, approval(target, preview))
    assert result["outcome"] == "acknowledged", result
    assert target.clip._same_parameter_modulation is None
    assert (
        result["result"]["envelope_replacement"]["preview_metadata"]["scope"][
            "same_parameter_modulation_preservation_proven"
        ]
        is False
    )
    assert target.clip.automation_envelope(target.send) is target.send_env


def test_track_reorder_resolves_retained_native_handles_without_index_authority(
    target: Any,
) -> None:
    """Location changes preserve the approved actual object and immutable preview evidence."""
    preview = call(target, PREVIEW_METHOD, preview_request(target))
    assert preview["observation"]["track_index"] == 1
    target.live.song._tracks.reverse()
    result = call(target, REPLACE_METHOD, approval(target, preview))
    assert result["outcome"] == "acknowledged", result
    assert result["result"]["track_index"] == 0
    assert result["result"]["envelope_replacement"]["before_observation"]["track_index"] == 1
    assert target.live.song.tracks[0] is target.record["track"]
    assert target.clip.automation_envelope(target.send) is target.send_env
