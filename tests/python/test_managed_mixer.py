"""Literal static Mixer ownership/setter/readback oracles, no actual Live qualification."""

from __future__ import annotations

import copy
from itertools import count
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import Device, DeviceType, Track
from Sunny.handler import BRIDGE_PROTOCOL_VERSION
from Sunny.managed_mixer import ADOPT_METHOD, DOMAINS, PREVIEW_METHOD, UPDATE_METHOD, valid_request
from test_managed_devices import Parameter
from test_managed_recovery import adopt
from test_managed_recovery import current as current_fixture
from test_managed_recovery import request as recovery_request


@pytest.fixture
def state(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Use actual Registry and real retained Clip-adoption seam, no Mixer grant yet."""
    current = current_fixture.__wrapped__(monkeypatch)
    adopt(current, current.recovery.preview(recovery_request()))
    record = current.registry._bindings[("project_a", "part_a")]
    current.record = record
    tokens = count(1)
    monkeypatch.setattr(
        "Sunny.managed_mixer.uuid",
        SimpleNamespace(uuid4=lambda: SimpleNamespace(hex=f"{next(tokens):032x}")),
    )
    for track in (*current.song.tracks, *current.song.return_tracks):
        track.canonical_parent = current.song
    mixer = current.track.mixer_device
    volume = Parameter("Track Volume", lambda v: f"{-40 + 272 * v**3:.2f} dB", value=0.375)
    volume._canonical_parent = mixer
    mixer._volume = volume
    current.volume = volume
    # Literal native mute coupling missing from the generic offline model.
    original_mute = Track.mute

    def mute(track: Any, value: Any) -> None:
        original_mute.__set__(track, value)
        track.mixer_device.track_activator._value = 0.0 if value else 1.0

    monkeypatch.setattr(Track, "mute", property(lambda track: original_mute.__get__(track), mute))
    current.track._devices = (
        Device("User native instrument", "UnknownInstrument", DeviceType.instrument),
    )
    current.track._devices[0]._canonical_parent = current.track
    record["adopted_device_cohort"] = current.track._devices
    current.registry._seal(record)
    return current


def preview_request(state: Any, desired: Any = None, purpose: str = "adopt") -> dict[str, Any]:
    """The real authored targets are literal; no expected parser reuse."""
    desired = (
        {"volume": {"target": -6.0, "tolerance": 0.0}, "pan": -0.25, "mute": True, "solo": True}
        if desired is None
        else desired
    )
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": state.record["content_fingerprint"],
        "expected_note_identity_fingerprint": state.record["note_identity_fingerprint"],
        "purpose": purpose,
        "selected_domains": [n for n in DOMAINS if n in desired],
        "desired": desired,
    }


def approve(preview: Any, operation: str) -> dict[str, Any]:
    """Exact current preview plus deliberately explicit Set-wide solo approval."""
    body = preview["preview"]
    result = {
        k: copy.deepcopy(body[k])
        for k in ("project_key", "binding_key", "purpose", "selected_domains", "desired")
    }
    result.update(
        document_token=body["context"]["document_token"],
        operation_id=operation,
        expected_content_fingerprint=body["before"]["binding_observation"]["content_fingerprint"],
        expected_note_identity_fingerprint=body["before"]["binding_observation"][
            "note_identity_fingerprint"
        ],
        preview_token=preview["preview_token"],
        preview_fingerprint=preview["preview_fingerprint"],
        approved_preview=copy.deepcopy(body),
        explicit_current_mixer_approval=True,
        explicit_set_wide_audible_approval=True,
    )
    return result


def grant(state: Any, desired: Any = None) -> dict[str, Any]:
    """Use actual Registry dispatch journal for current Mixer adoption."""
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, desired)])
    result = state.registry.dispatch(ADOPT_METHOD, [approve(preview, "adopt_mixer")])
    assert result["outcome"] == "acknowledged", result
    assert result["native_mutation_started"] is False
    return result


def update(state: Any, desired: Any = None, operation: str = "mix_write") -> dict[str, Any]:
    """Actual current preview→sole pending journal→helper dispatch path."""
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, desired, "update")])
    return state.registry.dispatch(UPDATE_METHOD, [approve(preview, operation)])


def test_preview_and_clip_note_device_metadata_do_not_grant_mixer(state: Any) -> None:
    """A full imported Clip ACK and a candidate never create static-Mixer authority."""
    before = copy.deepcopy(state.registry._capture(state.record))
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state)])
    assert preview["preview"]["current_authority_domains"] == []
    assert preview["preview"]["candidates"]["volume"]["internal_value"] == 0.5
    assert preview["preview"]["candidates"]["volume"]["display"] == "-6.00 dB"
    assert state.volume.writes == []
    assert state.registry._capture(state.record) == before
    state.registry._seal(state.record)
    with pytest.raises(RuntimeError, match="explicit current selected-domain adoption"):
        state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])


def test_explicit_current_grant_then_four_literal_native_targets_preserves_ids(state: Any) -> None:
    """-6dB resolves0.5, pan-.25, mute true mirrors0, solo preserves foreign flags."""
    before = state.registry._capture(state.record)
    grant(state)
    result = update(state)
    assert result["outcome"] == "acknowledged", result
    assert result["native_mutation_started"] is True
    assert state.volume.value == 0.5 and state.volume.writes == [0.5]
    assert state.track.mixer_device.panning.value == -0.25
    assert state.track.mute is True and state.track.mixer_device.track_activator.value == 0.0
    assert state.track.solo is True
    actual = result["result"]
    assert actual["note_identity"] == before["note_identity"]
    assert actual["mixer_update"]["readback"]["volume"]["display"] == "-6.00 dB"
    assert actual["mixer_update"]["set_wide_audible_effect"] is True
    assert actual["mixer_update"]["returned_fields"] == ["volume", "pan", "mute", "solo"]
    assert state.registry._operations["mix_write"] == result
    # Exact repeated token queries never repeat a native setter.
    assert state.registry.dispatch(UPDATE_METHOD, [result["request"]]) == result
    assert state.volume.writes == [0.5]


def test_closed_selected_domain_adoption_cannot_escalate(state: Any) -> None:
    """Volume-only approval grants no later pan/mute/solo rights through ordinary ACK."""
    selected = {"volume": {"target": -6.0, "tolerance": 0.0}}
    grant(state, selected)
    update(state, selected)
    state.registry._seal(state.record)
    with pytest.raises(RuntimeError, match="selected-domain adoption"):
        state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, {"solo": True}, "update")])
    assert state.record["_managed_mixer"]["domains"] == ["volume"]


def test_current_noop_ack_has_no_native_mutation(state: Any) -> None:
    """No-op truthful ACK retains current interpreted value and no false started flag."""
    selected = {"pan": 0.0, "mute": False, "solo": False}
    grant(state, selected)
    result = update(state, selected)
    assert result["outcome"] == "acknowledged", result
    assert result["native_mutation_started"] is False
    assert result["result"]["mixer_update"]["returned_fields"] == []
    assert state.volume.writes == []


@pytest.mark.parametrize(
    "fault", ("mixer", "parameter", "send", "foreign_solo", "note", "version", "formatter")
)
def test_approval_rechecks_exact_current_handles_and_state_before_setters(
    state: Any, fault: str
) -> None:
    """Identical replacement values or legitimate context changes cannot inherit preview."""
    grant(state)
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])
    if fault == "mixer":
        old = state.track.mixer_device
        new = copy.copy(old)
        new._canonical_parent = state.track
        state.track._mixer = new
    elif fault == "parameter":
        new = Parameter("Track Volume", state.volume.oracle, value=state.volume.value)
        new._canonical_parent = state.track.mixer_device
        state.track.mixer_device._volume = new
    elif fault == "send":
        state.song.create_return_track()
    elif fault == "foreign_solo":
        state.song.return_tracks[0].solo = True
    elif fault == "note":
        state.clip._notes[41].velocity = 73.0
    elif fault == "version":
        state.registry._handler._get_application = lambda: SimpleNamespace(
            get_major_version=lambda: 12, get_minor_version=lambda: 5, get_bugfix_version=lambda: 0
        )
    else:
        state.volume.oracle = lambda _: "-5.00 dB"
    result = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "bad_current")])
    assert result["outcome"] == "declined", result
    assert result["native_mutation_started"] is False and state.volume.writes == []


@pytest.mark.parametrize("fault", ("envelope", "automation", "split_pan", "grouped", "audio_role"))
def test_native_selected_preconditions_decline_without_setters(state: Any, fault: str) -> None:
    """Existing target automation/role/mode failures never clear or normalize anything."""
    if fault == "envelope":
        state.clip.create_automation_envelope(state.volume)
    elif fault == "automation":
        state.volume._automation_state = 1
    elif fault == "split_pan":
        state.track.mixer_device._panning_mode = 1
    elif fault == "grouped":
        state.track._injected = {"is_grouped": True}
    else:
        state.track._devices = ()
        state.record["adopted_device_cohort"] = ()
    # Deliberate external state not imported into baseline by inspect.
    with pytest.raises(RuntimeError):
        state.registry.dispatch(PREVIEW_METHOD, [preview_request(state)])
    assert state.volume.writes == []


@pytest.mark.parametrize("fault", ("clamp", "throw", "foreign_change", "unwritten_pan"))
def test_partial_native_outcomes_stop_later_setters_without_replay(state: Any, fault: str) -> None:
    """First volume call may mutate/throw; no later pan/mute/solo writes on mismatch."""
    grant(state)
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])
    if fault == "clamp":
        state.volume.clamp = True
    elif fault == "throw":
        state.volume.fail = True
    else:
        original = state.volume._write

        def write(value: float) -> None:
            original(value)
            if fault == "foreign_change":
                state.song.return_tracks[0].mute = True
            else:
                state.track.mixer_device.panning._value = 0.75

        state.volume._write = write
    result = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "partial")])
    assert result["outcome"] == "indeterminate", result
    assert result["native_mutation_started"] is True
    assert result["mixer_progress"]["started_fields"] == ["volume"]
    assert state.track.mute is False and state.track.solo is False
    assert state.volume.writes == [0.5]
    assert state.registry.dispatch(UPDATE_METHOD, [result["request"]]) == result
    assert state.volume.writes == [0.5]


def test_route_owner_can_explicitly_invalidate_then_fresh_adopt(state: Any) -> None:
    """Collection-changing ownership hook revokes grants and previews, never refreshes silently."""
    grant(state)
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])
    state.registry._mixer.invalidate_retained_cohort(state.record)
    result = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "revoked")])
    assert result["outcome"] == "declined" and state.volume.writes == []
    assert "_managed_mixer" not in state.record
    # New explicit current adoption can grant again with a fresh token.
    adopted = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state)])
    result = state.registry.dispatch(ADOPT_METHOD, [approve(adopted, "new_grant")])
    assert result["outcome"] == "acknowledged", result


@pytest.mark.parametrize(
    "change",
    (
        {"selected_domains": ["solo", "volume"]},
        {"selected_domains": ["volume", "volume"]},
        {"desired": {"send": -6.0}},
        {"explicit_set_wide_audible_approval": False},
    ),
)
def test_closed_mask_and_setwide_approval_types_reject(state: Any, change: Any) -> None:
    """Future roles, duplicates, noncanonical order and omitted solo scope cannot be approved."""
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state)])
    body = approve(preview, "invalid")
    body.update(change)
    assert valid_request(ADOPT_METHOD, body) is False


def test_throw_after_actual_volume_mutation_retains_observed_partial(state: Any) -> None:
    """Native may change .value then throw; retained after evidence must show that change."""
    grant(state)
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])

    def write(value: float) -> None:
        state.volume._value = value
        state.volume.writes.append(value)
        raise RuntimeError("literal post-write native failure")

    state.volume._write = write
    result = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "throw_after")])
    assert result["outcome"] == "indeterminate"
    assert result["mixer_partial"]["observed_after_available"] is True
    assert (
        result["mixer_partial"]["after"]["binding_observation"]["manifest"]["mixer"]["volume"][
            "value"
        ]
        == 0.5
    )
    assert result["mixer_progress"]["returned_fields"] == []
    assert state.track.mixer_device.panning.value == 0.0


def test_capacity_refusal_precedes_setter_and_has_no_grant_escalation(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Actual response budget failure stops before first static native mutation."""
    grant(state)
    preview = state.registry.dispatch(PREVIEW_METHOD, [preview_request(state, purpose="update")])

    def fail(*_: Any) -> None:
        raise RuntimeError("ReplyCapacityUnavailable: literal measured frame")

    monkeypatch.setattr("Sunny.managed_mixer.guard_managed_response_capacity", fail)
    result = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "capacity")])
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert state.volume.writes == []


def test_formatter_search_joins_known_parameter_guards_on_every_call(state: Any) -> None:
    """A formatter changing current notes fails immediately, not merely after all searches."""

    def mutate(_: float) -> None:
        state.clip._notes[41].velocity = 74.0

    state.volume.callback = mutate
    with pytest.raises(RuntimeError, match="context changed"):
        state.registry.dispatch(PREVIEW_METHOD, [preview_request(state)])
    assert len(state.volume.calls) == 1 and state.volume.writes == []


def test_solo_setter_preserves_foreign_stored_gates_but_records_audible_coupling(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Native exclusive preference remains irrelevant; foreign muted_via_solo may change."""
    foreign = state.song.return_tracks[0]
    foreign.solo = True
    state.song.exclusive_solo = True
    state.registry._seal(state.record)
    original_solo = Track.solo

    def solo(track: Any, value: Any) -> None:
        original_solo.__set__(track, value)
        foreign._literal_muted_via_solo = value

    monkeypatch.setattr(Track, "solo", property(lambda track: original_solo.__get__(track), solo))
    monkeypatch.setattr(
        Track,
        "muted_via_solo",
        property(lambda track: getattr(track, "_literal_muted_via_solo", False)),
    )
    grant(state, {"solo": True})
    result = update(state, {"solo": True})
    assert result["outcome"] == "acknowledged", result
    assert foreign.solo is True and foreign.mute is False
    assert foreign.muted_via_solo is True and state.song.exclusive_solo is True
    evidence = result["result"]["mixer_update"]
    assert evidence["before"]["solo_cohort"][-1]["muted_via_solo"] is False
    assert evidence["after"]["solo_cohort"][-1]["muted_via_solo"] is True


def test_same_epoch_clean_refresh_preserves_exact_grant_dirty_handle_revokes(state: Any) -> None:
    """Recovery consumes a read-only decision; identical new control gains no inherited rights."""
    grant(state, {"pan": 0.0})
    candidate = {**state.record}
    copied = state.registry._mixer.authority_for_refresh(state.record, candidate)
    assert copied["domains"] == ["pan"]
    candidate["clip"] = object()
    assert state.registry._mixer.authority_for_refresh(state.record, candidate) is None


def test_current_control_drift_revokes_clean_refresh_disposition(state: Any) -> None:
    """Exact same Mixer handles plus changed selected pan must require explicit fresh adoption."""
    grant(state, {"pan": 0.0})
    state.track.mixer_device.panning._value = 0.5
    assert state.registry._mixer.authority_for_refresh(state.record, {**state.record}) is None
    with pytest.raises(RuntimeError, match="guarded controls"):
        state.registry._seal(state.record)
    assert state.record["_managed_mixer"]["guard"]["pan"] == 0.0


def test_revised_note_ack_preserves_static_mixer_grant_without_enlarging_mask(state: Any) -> None:
    """Existing note update advances its content guard while native pan grant remains exact."""
    grant(state, {"pan": -0.25})
    note = state.registry._capture(state.record)["note_identity"]["notes"][0]
    request = {
        "document_token": "document_a",
        "operation_id": "notes",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": state.record["content_fingerprint"],
        "changes": [
            {
                "note_id": 41,
                "expected": {k: v for k, v in note.items() if k != "note_id"},
                "updates": {"velocity": 84.0},
            }
        ],
    }
    from live_model import MidiNoteVector

    state.clip.get_notes_by_id = lambda ids: MidiNoteVector(state.clip._notes[i] for i in ids)

    def apply_notes(notes: Any) -> None:
        for revised in notes:
            state.clip._notes[revised.note_id].velocity = revised.velocity

    state.clip.apply_note_modifications = apply_notes
    result = state.registry.dispatch("sunny_managed_update_notes", [request])
    assert result["outcome"] == "acknowledged", result
    assert state.record["_managed_mixer"]["domains"] == ["pan"]
    revised = update(state, {"pan": -0.25})
    assert revised["outcome"] == "acknowledged", revised
    assert state.clip._notes[41].velocity == 84.0


@pytest.mark.parametrize("formatter", ("missing", "forbidden_getter"))
def test_mute_only_adoption_does_not_require_unselected_volume_formatter(
    state: Any, monkeypatch: pytest.MonkeyPatch, formatter: str
) -> None:
    """A retired-Part mute uses current native authority, no active Part or dB query."""
    if formatter == "missing":
        state.volume.str_for_value = None
    else:

        def forbidden(_: Any) -> Any:
            raise AssertionError("Unselected volume formatter getter must never be accessed")

        monkeypatch.setattr(type(state.volume), "str_for_value", property(forbidden))
    grant(state, {"mute": True})
    result = update(state, {"mute": True})
    assert result["outcome"] == "acknowledged", result
    assert state.track.mute is True and state.track.mixer_device.track_activator.value == 0.0
    assert state.volume.calls == [] and state.volume.writes == []
    assert state.record["_managed_mixer"]["domains"] == ["mute"]


def test_successful_original_track_creation_grants_real_mixer_without_clip_adoption(
    state: Any,
) -> None:
    """The sole existing create journal grants controls on the actual newborn Track."""
    result = state.registry.dispatch(
        "sunny_managed_create_clip",
        [
            {
                "document_token": "document_a",
                "operation_id": "born",
                "project_key": "project_a",
                "binding_key": "part_b",
                "clip_end": 4.0,
                "signature_numerator": 4,
                "signature_denominator": 4,
                "notes": [
                    {
                        "pitch": 62,
                        "start_time": 0.0,
                        "duration": 1.0,
                        "velocity": 80,
                        "mute": False,
                        "probability": 1.0,
                        "velocity_deviation": 0.0,
                        "release_velocity": 64.0,
                    }
                ],
            }
        ],
    )
    assert result["outcome"] == "acknowledged", result
    record = state.registry._bindings[("project_a", "part_b")]
    assert record["_managed_mixer"]["domains"] == list(DOMAINS)
    assert record["_managed_mixer"]["origin"] == "managed_track_creation"
    track = record["track"]
    track.canonical_parent = state.song  # Explicit offline parent fixture.
    request = preview_request(SimpleNamespace(record=record), {"mute": True}, "update")
    request["binding_key"] = "part_b"
    preview = state.registry.dispatch(PREVIEW_METHOD, [request])
    ack = state.registry.dispatch(UPDATE_METHOD, [approve(preview, "born_mute")])
    assert ack["outcome"] == "acknowledged", ack
    assert track.mute is True and track.mixer_device.track_activator.value == 0.0
    assert state.track.mute is False


def test_readonly_static_mixer_inspection_observes_actual_controls_without_tokens(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Current nonlinear display is independent of the selected desired candidate."""
    payload = preview_request(state, purpose="inspect")
    helper = state.registry._mixer
    helper._previews = {f"{index:032x}": {} for index in range(256)}
    before = state.registry._capture(state.record)
    count = len(state.registry._operations)
    monkeypatch.setattr(
        "Sunny.managed_mixer.uuid",
        SimpleNamespace(uuid4=lambda: pytest.fail("inspection allocated token")),
    )
    for _ in range(3):
        wire = state.registry._handler.handle(
            {
                "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
                "type": "call",
                "path": "song",
                "name": "sunny_managed_inspect_static_mixer",
                "args": [payload],
            }
        )
        assert wire["success"], wire
        result = wire["value"]
        assert set(result) == {"schema_version", "outcome", "inspection"}
        assert result["outcome"] == "observed"
        body = result["inspection"]
        assert "preview_token" not in body
        assert body["purpose"] == "inspect"
        assert body["authority_origin"] == "none"
        assert body["native_mutation_started"] is False
        candidate = body["candidates"]["volume"]
        assert candidate["internal_value"] == 0.5
        assert candidate["display"] == "-6.00 dB"
        assert candidate["current_display"]["display"] == "-25.66 dB"
        assert body["before"]["track_context"]["mute"] is False
        assert body["before"]["track_context"]["solo"] is False
    assert len(helper._previews) == 256
    assert "_managed_mixer" not in state.record
    assert len(state.registry._operations) == count
    assert state.volume.writes == []
    assert state.registry._capture(state.record) == before


def test_readonly_static_mixer_inspection_has_fresh_after_write_evidence(state: Any) -> None:
    """Final read sees genuine nonlinear target/mute/solo, with no new journal."""
    grant(state)
    result = update(state)
    assert result["outcome"] == "acknowledged"
    payload = preview_request(state, purpose="inspect")
    grant_identity = state.record["_managed_mixer"]
    before_grant = dict(grant_identity)
    count = len(state.registry._operations)
    previews = len(state.registry._mixer._previews)
    response = state.registry.dispatch("sunny_managed_inspect_static_mixer", [payload])
    body = response["inspection"]
    assert body["candidates"]["volume"]["current_display"]["display"] == "-6.00 dB"
    assert body["before"]["track_context"]["mute"] is True
    assert body["before"]["track_context"]["solo"] is True
    assert body["before"]["mixer_capture"]["parameters"][1]["descriptor"]["value"] == -0.25
    assert state.record["_managed_mixer"] is grant_identity
    assert state.record["_managed_mixer"] == before_grant
    assert len(state.registry._operations) == count
    assert len(state.registry._mixer._previews) == previews
    assert state.volume.writes == [0.5]


def test_readonly_static_mixer_inspection_preserves_selected_existing_envelope(state: Any) -> None:
    """Write prerequisites still apply; this read cannot approve clearing native automation."""
    state.track._clip_slots[0]._clip.automation_envelope = (
        lambda parameter: object() if parameter is state.track.mixer_device.panning else None
    )
    payload = preview_request(state, {"pan": -0.25}, purpose="inspect")
    count = len(state.registry._operations)
    with pytest.raises(RuntimeError, match="existing selected envelope preserved"):
        state.registry.dispatch("sunny_managed_inspect_static_mixer", [payload])
    assert state.volume.writes == []
    assert len(state.registry._operations) == count
    assert not state.registry._mixer._previews


def test_readonly_static_mixer_inspection_rejects_cohort_drift_during_formatter(state: Any) -> None:
    """A foreign Solo change during capture is observed drift, never adopted state."""
    foreign = state.song.return_tracks[0]
    state.volume.callback = lambda _: setattr(foreign, "solo", True)
    payload = preview_request(state, purpose="inspect")
    count = len(state.registry._operations)
    with pytest.raises(RuntimeError, match="context changed during formatter"):
        state.registry.dispatch("sunny_managed_inspect_static_mixer", [payload])
    assert state.volume.writes == []
    assert len(state.registry._operations) == count
    assert "_managed_mixer" not in state.record
    assert not state.registry._mixer._previews
