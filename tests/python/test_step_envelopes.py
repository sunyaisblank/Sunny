"""Native Python envelope adapter tests, independent of a running Live host.

The fixture opts into the pinned Python source contract. It does not establish
an API version floor, device unit calibration, or saved/reopened persistence.
"""

from __future__ import annotations

import copy
import math
import threading
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import (
    ABSENT,
    ArgumentError,
    AutomationEnvelope,
    AutomationState,
    DeviceParameter,
    LiveSet,
    ParameterState,
    inject,
)
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler, _request_allowed

CLIP_PATH = "song/tracks/0/clip_slots/0/clip"
DEVICE_PARAMETER = {"kind": "device", "device_index": 0, "parameter_name": "Dry/Wet"}


def _request(handler: LomHandler, name: str, payload: dict[str, Any]) -> dict[str, Any]:
    return handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": CLIP_PATH,
            "name": name,
            "args": [payload],
        }
    )


def _lane(selector: dict[str, Any] | None = None) -> dict[str, Any]:
    return {
        "parameter": copy.deepcopy(DEVICE_PARAMETER if selector is None else selector),
        "interpolation": "step",
        "clip_end": 4.0,
        "points": [
            {"time": 0.0, "value": 0.25},
            {"time": 1.0, "value": 0.8},
            {"time": 2.5, "value": 0.15},
        ],
    }


def _query(times: list[float], selector: dict[str, Any] | None = None) -> dict[str, Any]:
    return {
        "parameter": copy.deepcopy(DEVICE_PARAMETER if selector is None else selector),
        "sample_times": times,
    }


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    """Create explicit source-contract objects and an identity-only authorizer."""
    live = LiveSet(midi_tracks=1, return_tracks=1, python_envelope_api=True).install(monkeypatch)
    track = live.song.tracks[0]
    track.insert_device("Compressor")
    slot = track.clip_slots[0]
    slot.create_clip(4.0)
    clip = slot.clip
    clip.looping = False
    parameter = track.devices[0].parameters[-1]
    calls = []

    def authorize(resolved_track: Any, resolved_clip: Any, resolved_parameter: Any) -> bool:
        calls.append((resolved_track, resolved_clip, resolved_parameter, threading.get_ident()))
        return resolved_track is track and resolved_clip is clip and resolved_parameter is parameter

    return SimpleNamespace(
        live=live,
        track=track,
        clip=clip,
        parameter=parameter,
        authorizations=calls,
        handler=LomHandler(live.surface, envelope_authorizer=authorize),
    )


def test_author_creates_native_steps_and_query_samples_independent_values(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Verify literal intervals and boundary values via separate native reads."""
    inserted = []
    implementation = AutomationEnvelope.insert_step

    def insert(envelope: AutomationEnvelope, start: float, duration: float, value: float) -> None:
        inserted.append((start, duration, value))
        implementation(envelope, start, duration, value)

    monkeypatch.setattr(AutomationEnvelope, "insert_step", insert)
    acknowledgement = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert acknowledgement["success"], acknowledgement
    assert acknowledgement["value"]["action"] == "created"
    assert acknowledgement["value"]["steps_inserted"] == 3
    assert set(acknowledgement["value"]) == {"action", "steps_inserted", "parameter"}
    assert inserted == [(0.0, 1.0, 0.25), (1.0, 1.5, 0.8), (2.5, 1.5, 0.15)]
    assert target.parameter.value == 0.0  # No temporary parameter control.
    assert len(target.authorizations) == 4  # Creation plus each returned insert call.
    assert all(
        call[:3] == (target.track, target.clip, target.parameter) for call in target.authorizations
    )
    assert all(call[3] == threading.main_thread().ident for call in target.authorizations)

    times = [0.0, 0.999, 1.0, 2.499, 2.5, 3.999]
    response = _request(target.handler, "sunny_get_step_envelope", _query(times))
    assert response["success"], response
    assert response["value"]["has_envelope"] is True
    assert response["value"]["parameter"]["unit"] == "internal"
    assert response["value"]["samples"] == [
        {"time": time, "value": value}
        for time, value in zip(times, [0.25, 0.25, 0.8, 0.8, 0.15, 0.15], strict=True)
    ]

    # Change the model after authoring: readback must report the changed native
    # envelope rather than cached points or the acknowledgement.
    target.clip.automation_envelope(target.parameter).insert_step(1.0, 1.5, 0.4)
    changed = _request(target.handler, "sunny_get_step_envelope", _query([1.25]))
    assert changed["value"]["samples"] == [{"time": 1.25, "value": 0.4}]
    assert len(target.authorizations) == 4  # Queries never invoke write authorization.


def test_query_missing_envelope_does_not_create_or_use_static_parameter_value(
    target: SimpleNamespace,
) -> None:
    """Keep absent envelope evidence separate from a parameter's base value."""
    target.parameter.value = 0.6
    response = _request(target.handler, "sunny_get_step_envelope", _query([0.25, 3.75]))
    assert response["success"], response
    assert response["value"]["has_envelope"] is False
    assert response["value"]["samples"] == []
    assert target.clip.has_envelopes is False
    assert target.parameter.value == 0.6
    assert not target.authorizations


def test_default_and_non_boolean_authorizers_deny_before_creation(
    target: SimpleNamespace,
) -> None:
    """Production defaults and truthy substitutes cannot establish ownership."""
    for authorizer in (None, lambda *_: False, lambda *_: 1, lambda *_: "owned"):
        handler = LomHandler(target.live.surface, envelope_authorizer=authorizer)
        response = _request(handler, "sunny_author_step_envelope", _lane())
        assert not response["success"]
        assert "not authorized" in response["error"]
        assert target.clip.has_envelopes is False


def test_existing_lane_is_updated_without_clearing_another_parameter(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Reuse valid lookup objects and preserve an unrelated native envelope."""
    own = target.clip.create_automation_envelope(target.parameter)
    own.insert_step(0.0, 4.0, 0.9)
    pan = target.track.mixer_device.panning
    other = target.clip.create_automation_envelope(pan)
    other.insert_step(0.0, 4.0, -0.6)

    def unexpected_create(_: Any) -> None:
        raise AssertionError("Lookup already returned a native envelope")

    monkeypatch.setattr(target.clip, "create_automation_envelope", unexpected_create)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert response["success"], response
    assert response["value"]["action"] == "updated"
    assert target.clip.automation_envelope(target.parameter) is own
    assert own.value_at_time(0.25) == 0.25
    assert target.clip.automation_envelope(pan) is other
    assert other.value_at_time(1.25) == -0.6
    assert len(target.authorizations) == 3


@pytest.mark.parametrize(
    "selector", [{"kind": "volume"}, {"kind": "panning"}, {"kind": "send", "send_index": 0}]
)
def test_mixer_bindings_use_same_track_objects_and_internal_domains(
    target: SimpleNamespace, selector: dict[str, Any]
) -> None:
    """Bind real mixer parameters without assuming display or normalized units."""
    mixer = target.track.mixer_device
    parameter = mixer.sends[0] if selector["kind"] == "send" else getattr(mixer, selector["kind"])
    handler = LomHandler(
        target.live.surface,
        envelope_authorizer=lambda track, clip, resolved: (
            track is target.track and clip is target.clip and resolved is parameter
        ),
    )
    lane = _lane(selector)
    lane["points"] = [{"time": 0.0, "value": parameter.min}, {"time": 2.0, "value": parameter.max}]
    original_display = parameter.display_value
    response = _request(handler, "sunny_author_step_envelope", lane)
    assert response["success"], response
    sampled = _request(handler, "sunny_get_step_envelope", _query([0.5, 2.5], selector))
    assert sampled["success"], sampled
    assert [sample["value"] for sample in sampled["value"]["samples"]] == [
        parameter.min,
        parameter.max,
    ]
    assert parameter.display_value == original_display


@pytest.mark.parametrize(
    "field,value",
    [
        ("is_playing", True),
        ("is_recording", True),
        ("is_overdubbing", True),
        ("is_triggered", True),
        ("will_record_on_start", True),
        ("is_playing", 0),
        ("is_session_clip", False),
        ("is_arrangement_clip", True),
        ("is_midi_clip", False),
        ("is_audio_clip", True),
        ("looping", True),
        ("start_marker", 0.5),
        ("end_marker", 3.5),
        ("end_marker", math.inf),
        ("end_marker", ABSENT),
    ],
)
def test_unsupported_or_non_idle_clip_declines_before_writes(
    target: SimpleNamespace, field: str, value: Any
) -> None:
    """Reject unsupported scope and uncertain runtime state without mutation."""
    inject(target.clip, field, value)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert target.clip.has_envelopes is False
    assert not target.authorizations


@pytest.mark.parametrize(
    "field,value",
    [
        ("is_enabled", False),
        ("is_enabled", 1),
        ("state", ParameterState.irrelevant),
        ("state", ParameterState.disabled),
        ("state", True),
        ("is_quantized", True),
        ("min", math.nan),
        ("min", 1.0),
        ("max", 0.0),
        ("value", math.inf),
        ("value", -0.1),
        ("automation_state", AutomationState.overridden),
    ],
)
def test_parameter_domain_and_state_decline_before_writes(
    target: SimpleNamespace, field: str, value: Any
) -> None:
    """Require a usable actual domain and preserve overridden automation."""
    inject(target.parameter, field, value)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert target.clip.has_envelopes is False
    assert not target.authorizations


def test_entire_lane_domain_is_validated_before_any_native_mutation(
    target: SimpleNamespace,
) -> None:
    """A later out-of-domain point cannot leave an earlier point authored."""
    lane = _lane()
    lane["points"][-1]["value"] = 1.01
    response = _request(target.handler, "sunny_author_step_envelope", lane)
    assert not response["success"]
    assert "actual parameter domain" in response["error"]
    assert target.clip.has_envelopes is False
    assert not target.authorizations


@pytest.mark.parametrize("foreign_parent", ["clip", "slot", "parameter", "device"])
def test_foreign_canonical_parent_is_rejected_before_authorization(
    target: SimpleNamespace, foreign_parent: str
) -> None:
    """Matching indices and names never authorize a foreign native identity."""
    target.live.song.create_midi_track(-1)
    foreign = target.live.song.tracks[1]
    if foreign_parent == "clip":
        inject(target.clip, "canonical_parent", foreign.clip_slots[0])
    elif foreign_parent == "slot":
        inject(target.track.clip_slots[0], "canonical_parent", foreign)
    elif foreign_parent == "parameter":
        inject(target.parameter, "canonical_parent", foreign.mixer_device)
    else:
        inject(target.track.devices[0], "canonical_parent", foreign)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert "target track" in response["error"]
    assert target.clip.has_envelopes is False
    assert not target.authorizations


def test_duck_typed_and_ambiguous_parameter_bindings_are_rejected(
    target: SimpleNamespace,
) -> None:
    """Resolve a single actual Live DeviceParameter rather than a lookalike."""
    device = target.track.devices[0]
    inject(device, "parameters", (SimpleNamespace(name="Dry/Wet", original_name="Dry/Wet"),))
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert not target.authorizations
    other = DeviceParameter("Other", original_name="Dry/Wet")
    other._canonical_parent = device
    inject(device, "parameters", (target.parameter, other))
    ambiguous = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not ambiguous["success"]
    assert "ambiguous" in ambiguous["error"]
    assert target.clip.has_envelopes is False


def test_authorization_is_rechecked_before_each_native_write(
    target: SimpleNamespace,
) -> None:
    """A changed ownership decision halts writes and retains actual partial state."""
    calls = []

    def authorize(track: Any, clip: Any, parameter: Any) -> bool:
        calls.append((track, clip, parameter))
        return len(calls) <= 2  # Permit creation and first interval only.

    handler = LomHandler(target.live.surface, envelope_authorizer=authorize)
    response = _request(handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert "not authorized" in response["error"]
    assert len(calls) == 3
    assert target.clip.has_envelopes is True
    sampled = _request(handler, "sunny_get_step_envelope", _query([0.25, 1.25]))
    assert sampled["value"]["samples"] == [
        {"time": 0.25, "value": 0.25},
        {"time": 1.25, "value": 0.0},
    ]


def test_native_failure_after_partial_insertion_is_not_claimed_as_acknowledged(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Preserve uncertain mutation semantics rather than rolling back or retrying."""
    implementation = AutomationEnvelope.insert_step
    attempted = []

    def fail(envelope: AutomationEnvelope, start: float, duration: float, value: float) -> None:
        attempted.append(start)
        implementation(envelope, start, duration, value)
        if len(attempted) == 2:
            raise RuntimeError("host failed after insertion")

    monkeypatch.setattr(AutomationEnvelope, "insert_step", fail)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert "host failed after insertion" in response["error"]
    assert "value" not in response
    assert attempted == [0.0, 1.0]
    assert target.clip.automation_envelope(target.parameter).value_at_time(1.25) == 0.8


def test_query_samples_while_playing_and_rejects_invalid_native_observations(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A read-only probe neither overrides playback nor fabricates valid values."""
    assert _request(target.handler, "sunny_author_step_envelope", _lane())["success"]
    inject(target.clip, "is_playing", True)
    inject(target.parameter, "automation_state", AutomationState.playing)
    sampled = _request(target.handler, "sunny_get_step_envelope", _query([1.25]))
    assert sampled["success"], sampled
    assert sampled["value"]["parameter"]["automation_state"] == 1
    assert sampled["value"]["samples"] == [{"time": 1.25, "value": 0.8}]
    envelope = target.clip.automation_envelope(target.parameter)
    for invalid in (math.nan, math.inf, True, 1.1, -0.1, "0.8"):
        monkeypatch.setattr(envelope, "value_at_time", lambda _, value=invalid: value)
        response = _request(target.handler, "sunny_get_step_envelope", _query([1.25]))
        assert not response["success"]
        assert "value" not in response
    outside = _request(target.handler, "sunny_get_step_envelope", _query([4.0]))
    assert not outside["success"]
    assert "inside the marker interval" in outside["error"]


def test_fixture_requires_explicit_python_api_and_real_parameter_arguments(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Source-observed API signatures do not prove any unselected host version."""
    for method in (
        target.clip.automation_envelope,
        target.clip.create_automation_envelope,
        target.clip.clear_envelope,
    ):
        with pytest.raises(ArgumentError, match="DeviceParameter object"):
            method("Dry/Wet")
    for version in ((11, 0, 0), (11, 3, 0), (12, 3, 5)):
        live = LiveSet(version=version, midi_tracks=1).install(monkeypatch)
        live.song.tracks[0].clip_slots[0].create_clip(4.0)
        clip = live.song.tracks[0].clip_slots[0].clip
        assert not hasattr(clip, "automation_envelope")
        assert not hasattr(clip, "create_automation_envelope")


def test_missing_or_invalid_native_envelope_api_fails_without_success_evidence(
    target: SimpleNamespace, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Runtime callability is required even for an admitted source contract."""
    monkeypatch.setattr(target.clip, "automation_envelope", None)
    response = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not response["success"]
    assert "unavailable" in response["error"]
    assert not target.authorizations
    assert target.clip.has_envelopes is False
    monkeypatch.setattr(target.clip, "automation_envelope", lambda _: None)
    monkeypatch.setattr(target.clip, "create_automation_envelope", lambda _: None)
    invalid = _request(target.handler, "sunny_author_step_envelope", _lane())
    assert not invalid["success"]
    assert "editable envelope" in invalid["error"]
    assert "value" not in invalid


@pytest.mark.parametrize(
    "selector",
    [
        {"kind": "device", "device_index": False, "parameter_name": "Dry/Wet"},
        {"kind": "device", "device_index": 0.0, "parameter_name": "Dry/Wet"},
        {"kind": "device", "device_index": 2147483648, "parameter_name": "Dry/Wet"},
        {"kind": "device", "device_index": 0, "parameter_name": ""},
        {"kind": "send", "send_index": -1},
        {"kind": "send", "send_index": True},
        {"kind": "volume", "path": "song/tracks/1"},
        {"kind": "unknown"},
    ],
)
def test_closed_parameter_selectors_reject_foreign_or_ambiguous_shapes(
    target: SimpleNamespace, selector: dict[str, Any]
) -> None:
    """Keep selectors relative to one resolved track with exact key sets."""
    for name, payload in (
        ("sunny_author_step_envelope", _lane(selector)),
        ("sunny_get_step_envelope", _query([0.5], selector)),
    ):
        assert not _request_allowed("call", CLIP_PATH, name, [payload])
        assert not _request(target.handler, name, payload)["success"]
    assert not target.authorizations
    assert target.clip.has_envelopes is False


@pytest.mark.parametrize(
    "points",
    [
        [],
        [{"time": 0.1, "value": 0.2}],
        [{"time": 0.0, "value": True}],
        [{"time": 0.0, "value": math.nan}],
        [{"time": 0.0, "value": math.inf}],
        [{"time": 0.0, "value": 10**400}],
        [{"time": -0.1, "value": 0.2}],
        [{"time": 0.0, "value": 0.2}, {"time": 4.0, "value": 0.3}],
        [{"time": 0.0, "value": 0.2}, {"time": 0.0, "value": 0.3}],
        [{"time": 0.0, "value": 0.2, "curve": "linear"}],
    ],
)
def test_closed_author_lane_rejects_invalid_points_before_writes(
    target: SimpleNamespace, points: list[dict[str, Any]]
) -> None:
    """Admit finite Step intent covering the interval from time zero only."""
    lane = _lane()
    lane["points"] = points
    assert not _request_allowed("call", CLIP_PATH, "sunny_author_step_envelope", [lane])
    assert not _request(target.handler, "sunny_author_step_envelope", lane)["success"]
    assert not target.authorizations
    assert target.clip.has_envelopes is False


@pytest.mark.parametrize(
    "field,value",
    [
        ("interpolation", "linear"),
        ("interpolation", "smooth"),
        ("clip_end", 0.0),
        ("clip_end", True),
        ("clip_end", math.inf),
        ("foreign_clip", "song/tracks/1/clip_slots/0/clip"),
    ],
)
def test_closed_author_lane_rejects_unsupported_controls(
    target: SimpleNamespace, field: str, value: Any
) -> None:
    """Reject curves, unknown fields and invalid finite interval controls."""
    lane = _lane()
    lane[field] = value
    assert not _request_allowed("call", CLIP_PATH, "sunny_author_step_envelope", [lane])
    assert not _request(target.handler, "sunny_author_step_envelope", lane)["success"]
    assert target.clip.has_envelopes is False


@pytest.mark.parametrize(
    "times", [[], [-0.1], [True], [math.inf], [math.nan], [0.5, 0.5], [1.5, 0.5], [10**400]]
)
def test_closed_query_rejects_invalid_sample_controls(
    target: SimpleNamespace, times: list[float]
) -> None:
    """Read requests require finite, ordered, distinct sample times."""
    payload = _query(times)
    assert not _request_allowed("call", CLIP_PATH, "sunny_get_step_envelope", [payload])
    assert not _request(target.handler, "sunny_get_step_envelope", payload)["success"]
    assert target.clip.has_envelopes is False


def test_float_conversion_cannot_collapse_positive_step_intervals(
    target: SimpleNamespace,
) -> None:
    """JSON integer ordering must remain strict in Live's floating time domain."""
    lane = _lane()
    lane["clip_end"] = 2**54
    lane["points"] = [
        {"time": 0, "value": 0.2},
        {"time": 2**53, "value": 0.3},
        {"time": 2**53 + 1, "value": 0.4},
    ]
    assert not _request_allowed("call", CLIP_PATH, "sunny_author_step_envelope", [lane])
    query = _query([2**53, 2**53 + 1])
    assert not _request_allowed("call", CLIP_PATH, "sunny_get_step_envelope", [query])


def test_model_clear_is_parameter_specific_and_all_clear_includes_native_envelopes(
    target: SimpleNamespace,
) -> None:
    """Exercise object-envelope storage rather than only legacy string markers."""
    own = target.clip.create_automation_envelope(target.parameter)
    own.insert_step(0.0, 4.0, 0.2)
    other = target.clip.create_automation_envelope(target.track.mixer_device.panning)
    other.insert_step(0.0, 4.0, -0.6)
    target.clip.clear_envelope(target.parameter)
    assert target.clip.automation_envelope(target.parameter) is None
    assert target.clip.automation_envelope(target.track.mixer_device.panning) is other
    assert target.clip.has_envelopes is True
    target.clip.clear_all_envelopes()
    assert target.clip.has_envelopes is False
