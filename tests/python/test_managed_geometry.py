"""Literal same-object Clip geometry oracles, without a running Live host."""

from __future__ import annotations

from typing import Any

import pytest
from live_model import Clip, inject
from Sunny.managed_geometry import GEOMETRY_METHOD, valid_managed_geometry_request
from test_managed_realization import call, intent, observe
from test_managed_realization import target as managed_target


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Reuse the genuine managed native-object boundary fixture."""
    return managed_target.__wrapped__(monkeypatch)


@pytest.fixture
def geometry_target(target: Any, monkeypatch: pytest.MonkeyPatch) -> Any:
    """Wrap only actual native-property boundary effects, preserving strict guards."""
    created = call(target, "sunny_managed_create_clip", intent(target))
    assert created["outcome"] == "acknowledged"
    target.created = created
    record = target.registry._bindings[("project_a", "part_a")]
    target.record, target.clip = record, record["clip"]
    target.calls = []
    target.mode = "independent"
    target.actions = {}
    for name in ("end_marker", "signature_numerator", "signature_denominator"):
        original = Clip.__dict__[name]

        def setter(clip: Any, value: Any, name: str = name, original: Any = original) -> None:
            if clip is not target.clip:
                original.__set__(clip, value)
                return
            target.calls.append((name, value))
            action = target.actions.get(name)
            if action == "raise_before":
                raise RuntimeError("native setter failed before effect")
            if action == "ignore":
                return
            original.__set__(clip, value)
            if name == "end_marker" and target.mode == "alias":
                clip._loop_end = float(value)
            if callable(action):
                action(clip)
            if action == "raise_after":
                raise RuntimeError("native setter failed after effect")

        monkeypatch.setattr(
            Clip,
            name,
            property(lambda clip, original=original: original.__get__(clip, Clip), setter),
        )
    target.clip._user_mpe_expression = {1: {"pressure": [(0.125, 0.7)]}}
    target.clip._user_follow_actions = {"next": True}
    target.clip._envelopes = {"untouched user lane"}
    target.created["result"] = target.registry._seal(record)
    return target


def request(
    target: Any, end: float = 8.0, numerator: int = 4, denominator: int = 4
) -> dict[str, Any]:
    """Retain the exact logical binding and before-content fingerprint."""
    return {
        "document_token": target.context["document_token"],
        "operation_id": "geometry_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": target.created["result"]["content_fingerprint"],
        "geometry": {
            "end_marker": end,
            "signature_numerator": numerator,
            "signature_denominator": denominator,
        },
    }


@pytest.mark.parametrize("mode", ["independent", "alias"])
def test_geometry_extent_and_meter_preserve_literal_native_objects(
    geometry_target: Any, mode: str
) -> None:
    """Distinguish native inactive-boundary and alias behavior while retaining opaque data."""
    target = geometry_target
    target.mode = mode
    clip, track, note1, note2 = (
        target.clip,
        target.record["track"],
        target.clip._notes[1],
        target.clip._notes[2],
    )
    original_user_track = target.live.song.tracks[0]
    before = observe(target)["observation"]
    result = call(target, GEOMETRY_METHOD, request(target, 8.0, 3, 8))
    assert result["outcome"] == "acknowledged", result
    assert target.calls == [
        ("end_marker", 8.0),
        ("signature_numerator", 3),
        ("signature_denominator", 8),
    ]
    assert result["native_mutation_started"] is True
    update = result["result"]["clip_geometry_update"]
    assert (
        update["loop_end_relationship"]
        == {"independent": "unchanged_inactive_boundary", "alias": "followed_end_marker"}[mode]
    )
    assert (
        result["result"]["manifest"]["clip"]["loop_end"] == {"independent": 4.0, "alias": 8.0}[mode]
    )
    assert all(
        update[k] is True
        for k in (
            "observed_geometry_matches_request",
            "note_values_preserved",
            "note_ids_preserved",
            "note_cardinality_preserved",
            "other_finite_properties_preserved",
            "device_identity_preserved",
        )
    )
    assert result["result"]["note_identity"] == before["note_identity"]
    assert target.record["clip"] is clip and target.record["track"] is track
    assert target.clip._notes == {1: note1, 2: note2}
    assert target.live.song.tracks[0] is original_user_track
    assert target.clip._user_mpe_expression == {1: {"pressure": [(0.125, 0.7)]}}
    assert target.clip._user_follow_actions == {"next": True}
    assert target.clip._envelopes == {"untouched user lane"}
    assert result["result"]["content_boundary_complete"] is False
    assert call(target, GEOMETRY_METHOD, request(target, 8.0, 3, 8)) == result
    assert len(target.calls) == 3


def test_geometry_meter_only_never_assigns_end_or_retimes_notes(geometry_target: Any) -> None:
    """Change display meter through its two native setters without playback geometry writes."""
    target = geometry_target
    before = observe(target)["observation"]
    result = call(target, GEOMETRY_METHOD, request(target, 4.0, 3, 8))
    assert result["outcome"] == "acknowledged"
    assert target.calls == [("signature_numerator", 3), ("signature_denominator", 8)]
    assert result["result"]["manifest"]["clip"]["end_marker"] == 4.0
    assert result["result"]["note_identity"] == before["note_identity"]


@pytest.mark.parametrize("end", [2.0, 2.25])
def test_geometry_shrink_rejects_any_retained_start_or_tail(
    geometry_target: Any, end: float
) -> None:
    """Refuse newly hidden starts and clipped audible tails before any native setter."""
    target = geometry_target
    result = call(target, GEOMETRY_METHOD, request(target, end))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert "endpoint exceeds" in result["error"] and target.calls == []
    assert target.clip.end_marker == 4.0 and sorted(target.clip._notes) == [1, 2]


def test_geometry_shrink_after_selected_deletion_preserves_retained_id(
    geometry_target: Any,
) -> None:
    """Delete only the selected Event then shrink around the same retained native note."""
    target = geometry_target
    note = target.created["result"]["note_identity"]["notes"][1]
    population = {
        "document_token": target.context["document_token"],
        "operation_id": "delete_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": target.created["result"]["content_fingerprint"],
        "changes": [],
        "deletions": [
            {"note_id": 2, "expected": {k: v for k, v in note.items() if k != "note_id"}}
        ],
        "additions": [],
    }
    deleted = call(target, "sunny_managed_revise_note_population", population)
    assert deleted["outcome"] == "acknowledged"
    target.created = deleted
    kept = target.clip._notes[1]
    result = call(target, GEOMETRY_METHOD, request(target, 1.0))
    assert result["outcome"] == "acknowledged"
    assert target.calls == [("end_marker", 1.0)]
    assert target.clip._notes == {1: kept}


@pytest.mark.parametrize(
    "property_name", ["end_marker", "signature_numerator", "signature_denominator"]
)
@pytest.mark.parametrize("failure", ["raise_before", "raise_after", "ignore"])
def test_geometry_phase_failure_stops_without_replay_or_compensation(
    geometry_target: Any, property_name: str, failure: str
) -> None:
    """Retain partial property effects and reconcile without repeated setters."""
    target = geometry_target
    target.actions[property_name] = failure
    payload = request(target, 8.0, 3, 8)
    result = call(target, GEOMETRY_METHOD, payload)
    assert result["outcome"] == "indeterminate" and result["native_mutation_started"] is True
    index = ("end_marker", "signature_numerator", "signature_denominator").index(property_name)
    assert [name for name, _ in target.calls] == [
        "end_marker",
        "signature_numerator",
        "signature_denominator",
    ][: index + 1]
    assert result["progress"]["last_observation"]["note_identity"]["notes"][0]["note_id"] == 1
    if failure == "raise_after":
        assert getattr(target.clip, property_name) == payload["geometry"][property_name]
    assert call(target, GEOMETRY_METHOD, payload) == result
    queried = call(
        target,
        "sunny_managed_operation",
        {"document_token": target.context["document_token"], "operation_id": "geometry_a"},
    )
    assert queried == result and len(target.calls) == index + 1


@pytest.mark.parametrize("mutation", ["notes", "ids", "swapped", "loop_end", "loop_start", "mute"])
def test_geometry_detects_unexpected_native_phase_effects(
    geometry_target: Any, mutation: str
) -> None:
    """Stop remaining phases when native setters alter protected content."""
    target = geometry_target

    def mutate(clip: Any) -> None:
        if mutation == "notes":
            clip._notes[1].velocity = 45.0
        elif mutation == "ids":
            note = clip._notes.pop(1)
            note.note_id = 41
            clip._notes[41] = note
        elif mutation == "swapped":
            first, second = clip._notes[1], clip._notes[2]
            for name in (
                "pitch",
                "start_time",
                "duration",
                "velocity",
                "mute",
                "probability",
                "velocity_deviation",
                "release_velocity",
            ):
                old = getattr(first, name)
                setattr(first, name, getattr(second, name))
                setattr(second, name, old)
        elif mutation == "loop_end":
            clip._loop_end = 7.0
        elif mutation == "loop_start":
            clip._loop_start = 1.0
        else:
            clip._muted = True

    target.actions["end_marker"] = mutate
    result = call(target, GEOMETRY_METHOD, request(target, 8.0, 3, 8))
    assert result["outcome"] == "indeterminate"
    assert target.calls == [("end_marker", 8.0)]
    assert "PhaseMismatch" in result["error"]
    assert "last_observation" in result["progress"]


@pytest.mark.parametrize(
    "field,value",
    [
        ("end_marker", 0.0),
        ("end_marker", float("inf")),
        ("signature_numerator", True),
        ("signature_numerator", 100),
        ("signature_denominator", 3),
        ("signature_denominator", 4.0),
    ],
)
def test_geometry_closed_request_rejects_literal_bad_domain(
    geometry_target: Any, field: str, value: Any
) -> None:
    """Reject Boolean, fractional enum and nonfinite marker counterexamples."""
    payload = request(geometry_target)
    payload["geometry"][field] = value
    assert not valid_managed_geometry_request(GEOMETRY_METHOD, [payload])
    assert geometry_target.calls == []


def test_geometry_endpoint_overflow_and_inaccessible_population_are_before_setter(
    geometry_target: Any,
) -> None:
    """Finite scalar values do not prove a finite endpoint or complete legacy range."""
    target = geometry_target
    target.clip._notes[1].start_time = 1e308
    target.clip._notes[1].duration = 1e308
    target.created["result"] = target.registry._seal(target.record)
    result = call(target, GEOMETRY_METHOD, request(target, 1.7e308))
    assert result["outcome"] == "declined" and target.calls == []
    target.clip._notes[1].start_time, target.clip._notes[1].duration = 0.0, 1.0
    target.clip._live_version = (11, 0, 5)
    target.created["result"] = target.registry._seal(target.record)
    payload = request(target)
    payload["operation_id"] = "legacy_a"
    result = call(target, GEOMETRY_METHOD, payload)
    assert result["outcome"] == "declined" and target.calls == []
    assert "entire Clip note population" in result["error"]


def test_geometry_adopted_ids_do_not_implicitly_grant_extent_authority(
    geometry_target: Any,
) -> None:
    """Require the explicit named extent domain beyond current-ID ownership."""
    target = geometry_target
    target.record["authority_origin"] = "explicit_adoption"
    target.record["allowed_domains"] = (
        "existing_note_updates",
        "note_population_updates",
        "absent_mixer_step_lanes",
    )
    result = call(target, GEOMETRY_METHOD, request(target))
    assert result["outcome"] == "declined" and "AuthorityUnavailable" in result["error"]
    assert target.calls == []
    target.record["allowed_domains"] += ("clip_geometry_updates",)
    payload = request(target)
    payload["operation_id"] = "approved_a"
    assert call(target, GEOMETRY_METHOD, payload)["outcome"] == "acknowledged"


@pytest.mark.parametrize(
    "condition",
    ["arm", "is_frozen", "is_grouped", "is_playing", "looping", "unowned_id", "external_drift"],
)
def test_geometry_rejects_inactive_or_drifted_managed_boundary(
    geometry_target: Any, condition: str
) -> None:
    """Preserve user edits and inactive native state before any geometry mutation."""
    target = geometry_target
    if condition in ("arm", "is_frozen", "is_grouped"):
        inject(target.record["track"], condition, True)
        target.created["result"] = target.registry._seal(target.record)
    elif condition == "is_playing":
        inject(target.clip, condition, True)
    elif condition == "looping":
        target.clip._looping = True
    elif condition == "unowned_id":
        target.record["owned_note_ids"].remove(2)
    else:
        target.clip._notes[1].velocity = 45.0
    result = call(target, GEOMETRY_METHOD, request(target))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert target.calls == []


def test_geometry_same_values_are_readonly_comparison_not_false_native_ack(
    geometry_target: Any,
) -> None:
    """Avoid native writes and fictitious mutation-start evidence for already matching state."""
    result = call(geometry_target, GEOMETRY_METHOD, request(geometry_target, 4.0))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert "GeometryAlreadyMatches" in result["error"] and geometry_target.calls == []


def test_explicit_four_domain_adoption_geometry_preserves_opaque_device_cohort(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Fresh approval of current IDs/geometry retains a foreign rack and opaque native fields."""
    from live_model import Device, DeviceType
    from test_managed_recovery import approval
    from test_managed_recovery import current as recovery_current
    from test_managed_recovery import request as preview_request

    target = recovery_current.__wrapped__(monkeypatch)
    rack = Device(
        "User rack", "AudioEffectGroupDevice", DeviceType.audio_effect, can_have_chains=True
    )
    rack._canonical_parent = target.track
    rack._user_opaque_state = {"foreign_plugin": b"private"}
    target.track._devices.append(rack)
    target.clip._envelopes.add("other user lane")
    original_notes = dict(target.clip._notes)
    preview = call(target, "sunny_managed_preview_adoption", preview_request())
    assert preview["allowed_domains"] == [
        "existing_note_updates",
        "note_population_updates",
        "clip_geometry_updates",
        "absent_mixer_step_lanes",
    ]
    adopted = call(target, "sunny_managed_adopt_clip", approval(preview))
    assert adopted["outcome"] == "acknowledged" and adopted["native_mutation_started"] is False
    payload = {
        "document_token": "document_a",
        "operation_id": "geometry_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": adopted["result"]["content_fingerprint"],
        "geometry": {"end_marker": 8.0, "signature_numerator": 3, "signature_denominator": 8},
    }
    result = call(target, GEOMETRY_METHOD, payload)
    assert result["outcome"] == "acknowledged", result
    assert target.slot.clip is target.clip and target.handler._device_chain(target.track) == (rack,)
    assert all(target.clip._notes[k] is note for k, note in original_notes.items())
    assert target.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}
    assert target.clip._user_follow_actions == {"next": True}
    assert target.clip._envelopes == {"other user lane"}
    assert rack._user_opaque_state == {"foreign_plugin": b"private"}
    assert result["result"]["clip_geometry_update"]["device_identity_preserved"] is True
    assert "device_identity" not in result["result"]
    assert result["result"]["content_boundary_complete"] is False
