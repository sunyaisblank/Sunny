"""Literal Set-wide scalar oracles; explicit source fixtures, no running host."""

from __future__ import annotations

import copy
from collections.abc import Callable
from itertools import count
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import AutomationState, DeviceParameter, ParameterState, Scene, Song, Track
from Sunny.managed import _digest
from Sunny.managed_song_settings import (
    APPLY_METHOD,
    PREVIEW_METHOD,
    ManagedSongSettings,
    valid_request,
)
from test_managed_recovery import adopt, request
from test_managed_recovery import current as selected_part_fixture


@pytest.fixture
def state(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Approve real current Part IDs; expose explicit Main tempo/loop fixture seams."""
    current = selected_part_fixture.__wrapped__(monkeypatch)
    adopt(current, current.recovery.preview(request()))
    song = current.song
    song.loop_start = 0.0
    song.loop_length = 8.0
    tempo = DeviceParameter("Song Tempo", minimum=20.0, maximum=999.0, value=120.0)
    tempo._canonical_parent = song.master_track.mixer_device
    song.master_track.mixer_device.song_tempo = tempo
    song.create_midi_track(-1)
    for track in song.tracks:
        track.canonical_parent = song
    foreign = song.tracks[1]
    foreign.name = "User strings"
    foreign.clip_slots[0].create_clip(8.0)
    foreign.clip_slots[0].clip.name = "Foreign audio idea"
    foreign.clip_slots[0].clip._opaque_user_state = {"unchanged": object()}
    scene = song.scenes[0]
    scene.name = "User scene"
    scene.enable_launch_overrides(150.0, 7, 8)
    song.current_song_time = 4.0
    song.set_or_delete_cue()
    song.cue_points[0].name = "User cue"
    song.current_song_time = 0.0
    tokens = count(1)
    monkeypatch.setattr(
        "Sunny.managed_song_settings.uuid.uuid4",
        lambda: SimpleNamespace(hex=f"{next(tokens):032x}"),
    )
    current.helper = ManagedSongSettings(current.registry)
    current.tempo_parameter = tempo
    current.foreign = foreign
    return current


def preview_request(state: Any, desired: Any = None) -> dict[str, Any]:
    """Supply current retained context and literal desired scalar intent."""
    record = state.registry._bindings[("project_a", "part_a")]
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": record["content_fingerprint"],
        "expected_note_identity_fingerprint": record["note_identity_fingerprint"],
        "desired": {"tempo": 92.5, "signature_numerator": 3, "signature_denominator": 8}
        if desired is None
        else desired,
    }


def approve(preview: Any) -> dict[str, Any]:
    """Approve exact current preview, expressly including its Set-wide effects."""
    body = preview["preview"]
    return {
        "document_token": "document_a",
        "operation_id": "song_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": body["binding_guard"]["content_fingerprint"],
        "expected_note_identity_fingerprint": body["binding_guard"]["note_identity_fingerprint"],
        "desired": copy.deepcopy(body["desired"]),
        "preview_token": preview["preview_token"],
        "preview_fingerprint": preview["preview_fingerprint"],
        "approved_preview": copy.deepcopy(body),
        "explicit_set_wide_approval": True,
    }


def pending(payload: Any) -> dict[str, Any]:
    """Use the existing registry's immutable pending operation shape."""
    return {
        "document_token": "document_a",
        "operation_id": payload["operation_id"],
        "name": APPLY_METHOD,
        "request": copy.deepcopy(payload),
        "request_fingerprint": _digest({"name": APPLY_METHOD, "request": payload}),
        "outcome": "pending",
        "native_mutation_started": False,
    }


def apply(state: Any, preview: Any) -> tuple[Any, Any]:
    """Invoke only the draft helper; shared dispatcher integration is a later gate."""
    payload = approve(preview)
    operation = pending(payload)
    result = state.helper.apply(("project_a", "part_a"), payload, operation)
    return result, operation


def watch_setters(
    monkeypatch: pytest.MonkeyPatch,
    fail: str | None = None,
    clamp: bool = False,
    after: Callable[[Any, str], None] | None = None,
) -> list[Any]:
    """Observe literal native scalar calls independently of returned flags."""
    calls: list[Any] = []
    for name in ("tempo", "signature_numerator", "signature_denominator"):
        original = getattr(Song, name)

        def write(song: Any, value: Any, native: Any = original, field: str = name) -> None:
            calls.append((field, value))
            if field == fail:
                raise RuntimeError("native field failure")
            native.__set__(song, 90.0 if field == "tempo" and clamp else value)
            if after is not None:
                after(song, field)

        def read(song: Any, native: Any = original) -> Any:
            return native.__get__(song, Song)

        monkeypatch.setattr(Song, name, property(read, write))
    return calls


def test_current_global_scalars_change_only_explicit_fields_and_preserve_user_objects(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Actual92.5/3/8 readbacks and native call list determine the outcome."""
    before_tracks = state.song.tracks
    before_scene = state.song.scenes[0]
    before_cue = state.song.cue_points[0]
    before_clip = state.foreign.clip_slots[0].clip
    before_notes = copy.deepcopy(state.clip._notes)
    calls = watch_setters(monkeypatch)
    preview = state.helper.preview(preview_request(state))
    assert calls == []
    assert preview["preview"]["scope"]["all_tracks_affected"] is True
    assert [track["name"] for track in preview["preview"]["before"]["tracks"]] == [
        "Sunny|project_a|part_a|track",
        "User strings",
    ]
    result, operation = apply(state, preview)
    assert calls == [("signature_numerator", 3), ("signature_denominator", 8), ("tempo", 92.5)]
    assert (state.song.tempo, state.song.signature_numerator, state.song.signature_denominator) == (
        92.5,
        3,
        8,
    )
    assert operation["native_mutation_started"] is True
    evidence = result["song_settings"]
    assert (
        evidence["desired_settings_match"] is True
        and evidence["observed_untouched_state_preserved"] is True
    )
    assert evidence["clip_and_note_ids_preserved"] is True
    assert (
        state.song.tracks == before_tracks
        and state.song.scenes[0] is before_scene
        and state.song.cue_points[0] is before_cue
    )
    assert state.foreign.clip_slots[0].clip is before_clip
    assert (
        before_scene.tempo == 150.0
        and before_scene.time_signature_numerator == 7
        and before_scene.time_signature_denominator == 8
    )
    assert before_cue.name == "User cue" and before_cue.time == 4.0
    assert {key: (note.pitch, note.velocity) for key, note in state.clip._notes.items()} == {
        key: (note.pitch, note.velocity) for key, note in before_notes.items()
    }
    assert state.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}
    assert state.clip._user_follow_actions == {"next": True}
    assert state.song.current_song_time == 0.0


@pytest.mark.parametrize(
    "field,expected,returned",
    [
        ("signature_numerator", (120.0, 4, 4), []),
        ("signature_denominator", (120.0, 3, 4), ["signature_numerator"]),
        ("tempo", (120.0, 3, 8), ["signature_numerator", "signature_denominator"]),
    ],
)
def test_native_partial_setter_failure_has_actual_readback_without_compensation(
    state: Any, monkeypatch: pytest.MonkeyPatch, field: str, expected: Any, returned: Any
) -> None:
    """A later throw leaves earlier native scalars changed, and records exactly that."""
    preview = state.helper.preview(preview_request(state))
    payload = approve(preview)
    operation = pending(payload)
    calls = watch_setters(monkeypatch, fail=field)
    with pytest.raises(RuntimeError, match="native field failure"):
        state.helper.apply(("project_a", "part_a"), payload, operation)
    assert (
        state.song.tempo,
        state.song.signature_numerator,
        state.song.signature_denominator,
    ) == expected
    assert operation["native_mutation_started"] is True
    assert operation["song_settings_progress"]["returned_fields"] == returned
    assert operation["song_settings_progress"]["started_fields"][-1] == field
    assert operation["song_settings_partial"]["after"]["settings"] == {
        "tempo": expected[0],
        "signature_numerator": expected[1],
        "signature_denominator": expected[2],
    }
    assert len(calls) == len(returned) + 1


def test_current_settings_noop_truthfully_has_no_native_start(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Exact current approval reads state but invokes no setter, even with integerBPM."""
    desired = {"tempo": 120, "signature_numerator": 4, "signature_denominator": 4}
    preview = state.helper.preview(preview_request(state, desired))
    calls = watch_setters(monkeypatch)
    result, operation = apply(state, preview)
    assert calls == [] and operation["native_mutation_started"] is False
    assert result["song_settings"]["started_fields"] == []
    assert result["song_settings"]["desired_settings_match"] is True


def test_native_clamp_is_truthful_completed_mismatch(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Native90BPM cannot become requested92.5 success through echoed flags."""
    preview = state.helper.preview(preview_request(state))
    calls = watch_setters(monkeypatch, clamp=True)
    result, operation = apply(state, preview)
    assert calls[-1] == ("tempo", 92.5) and state.song.tempo == 90.0
    assert operation["native_mutation_started"] is True
    assert result["song_settings"]["after"]["settings"]["tempo"] == 90.0
    assert result["song_settings"]["desired_settings_match"] is False


@pytest.mark.parametrize("mode", ["automated", "overridden", "disabled", "inactive", "missing"])
def test_changed_tempo_requires_actual_enabled_active_unautomated_parameter(
    state: Any, monkeypatch: pytest.MonkeyPatch, mode: str
) -> None:
    """Observed native state, rather than targetversion or guessed getter, admits tempo."""
    if mode == "automated":
        state.tempo_parameter._automation_state = AutomationState.playing
    if mode == "overridden":
        state.tempo_parameter._automation_state = AutomationState.overridden
    if mode == "disabled":
        state.tempo_parameter._is_enabled = False
    if mode == "inactive":
        state.tempo_parameter._state = ParameterState.irrelevant
    if mode == "missing":
        del state.song.master_track.mixer_device.song_tempo
    calls = watch_setters(monkeypatch)
    with pytest.raises(RuntimeError, match="TempoParameterUnavailable"):
        state.helper.preview(preview_request(state))
    assert calls == [] and state.helper._previews == {}


@pytest.mark.parametrize(
    "drift",
    [
        "tempo",
        "scene",
        "cue",
        "parameter",
        "same_name_scene",
        "same_name_track",
        "time",
        "native_note",
    ],
)
def test_current_baseline_or_handles_change_after_preview_declines_before_setters(
    state: Any, monkeypatch: pytest.MonkeyPatch, drift: str
) -> None:
    """A same-name replacement never inherits exact current-object approval."""
    preview = state.helper.preview(preview_request(state))
    if drift == "tempo":
        state.song.tempo = 100.0
    if drift == "scene":
        state.song.scenes[0]._tempo = 160.0
    if drift == "cue":
        state.song.cue_points[0].name = "Changed cue"
    if drift == "parameter":
        replacement = DeviceParameter("Song Tempo", minimum=20.0, maximum=999.0, value=120.0)
        replacement._canonical_parent = state.song.master_track.mixer_device
        state.song.master_track.mixer_device.song_tempo = replacement
    if drift == "same_name_scene":
        replacement_scene = Scene()
        replacement_scene.name = state.song.scenes[0].name
        replacement_scene.enable_launch_overrides(150.0, 7, 8)
        state.song._scenes[0] = replacement_scene
    if drift == "same_name_track":
        replacement_track = Track(state.song, "User strings", "midi")
        replacement_track.canonical_parent = state.song
        replacement_track._clip_slots = state.foreign._clip_slots
        state.song._tracks[1] = replacement_track
    if drift == "time":
        state.song.current_song_time = 2.0
    if drift == "native_note":
        state.clip._notes[41].velocity = 12.0
    calls = watch_setters(monkeypatch)
    with pytest.raises(RuntimeError):
        apply(state, preview)
    assert calls == [] and state.song.signature_numerator == 4


@pytest.mark.parametrize("version", [(11, 1, 0), (12, 2, 9), (12, 5, 0), (13, 0, 0)])
def test_unproved_live_versions_are_not_future_compatibility_claims(
    state: Any, version: Any
) -> None:
    """Finite12.3.x/12.4.x source coverage never expands through >= comparisons."""
    state.live.application._version = version
    with pytest.raises(RuntimeError, match="candidate coverage"):
        state.helper.preview(preview_request(state))


@pytest.mark.parametrize(
    "settings",
    [
        {"tempo": 19.99, "signature_numerator": 4, "signature_denominator": 4},
        {"tempo": 1000, "signature_numerator": 4, "signature_denominator": 4},
        {"tempo": True, "signature_numerator": 4, "signature_denominator": 4},
        {"tempo": 120.0, "signature_numerator": True, "signature_denominator": 4},
        {"tempo": 120.0, "signature_numerator": 100, "signature_denominator": 4},
        {"tempo": 120.0, "signature_numerator": 4, "signature_denominator": 3},
        {"tempo": float("nan"), "signature_numerator": 4, "signature_denominator": 4},
    ],
)
def test_invalid_scalar_domains_have_independent_boundary_expectations(
    state: Any, settings: Any
) -> None:
    """Declared native scalar limits are checked before preview allocation/setters."""
    assert not valid_request(PREVIEW_METHOD, preview_request(state, settings))
    with pytest.raises(RuntimeError, match="malformed preview"):
        state.helper.preview(preview_request(state, settings))


@pytest.mark.parametrize(
    "field",
    [
        "explicit_set_wide_approval",
        "project_key",
        "desired",
        "preview_fingerprint",
        "approved_preview",
    ],
)
def test_forged_or_unapproved_current_settings_cannot_invoke_any_setter(
    state: Any, monkeypatch: pytest.MonkeyPatch, field: str
) -> None:
    """A matching token alone cannot authorize altered approval semantics."""
    preview = state.helper.preview(preview_request(state))
    payload = approve(preview)
    if field == "explicit_set_wide_approval":
        payload[field] = False
    if field == "project_key":
        payload[field] = "foreign"
    if field == "desired":
        payload[field]["tempo"] = 101.0
    if field == "preview_fingerprint":
        payload[field] = "f" * 64
    if field == "approved_preview":
        payload[field]["scope"]["all_tracks_affected"] = False
    calls = watch_setters(monkeypatch)
    with pytest.raises(RuntimeError):
        state.helper.apply((payload["project_key"], "part_a"), payload, pending(payload))
    assert calls == [] and state.song.tempo == 120.0


def test_missing_actual_tempo_state_reports_precise_unavailability(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Missing actual automation access never silently becomes an unautomated state."""
    from live_model import ABSENT, inject

    inject(state.tempo_parameter, "automation_state", ABSENT)
    calls = watch_setters(monkeypatch)
    with pytest.raises(RuntimeError, match="TempoParameterUnavailable.*state access"):
        state.helper.preview(preview_request(state))
    assert calls == [] and state.helper._previews == {}


def test_complete_ack_capacity_is_checked_before_any_native_scalar_setter(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Actual wide native names fit preview but overflow complete journal before writes."""
    import json

    from Sunny.managed_capacity import MAX_MANAGED_RESPONSE_BYTES

    for _ in range(1450):
        scene = Scene()
        scene.name = "\U0001f600" * 250  # 1000 UTF8 bytes;3000 JSON escaped bytes.
        state.song._scenes.append(scene)
    preview = state.helper.preview(preview_request(state))
    assert (
        len(json.dumps({"success": True, "value": preview}).encode()) < MAX_MANAGED_RESPONSE_BYTES
    )
    payload = approve(preview)
    op = pending(payload)
    calls = watch_setters(monkeypatch)
    with pytest.raises(RuntimeError, match="complete managed response exceeds"):
        state.helper.apply(("project_a", "part_a"), payload, op)
    assert calls == [] and op["native_mutation_started"] is False
    assert state.song.tempo == 120.0 and state.song.signature_numerator == 4


def test_consumed_approval_cannot_repeat_after_a_lost_local_reply(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Only the outer immutable token journal may reconcile, never a second helper call."""
    preview = state.helper.preview(preview_request(state))
    calls = watch_setters(monkeypatch)
    payload = approve(preview)
    op = pending(payload)
    state.helper.apply(("project_a", "part_a"), payload, op)
    initial = list(calls)
    with pytest.raises(RuntimeError, match="absent or consumed"):
        state.helper.apply(("project_a", "part_a"), payload, pending(payload))
    assert calls == initial and len(calls) == 3


@pytest.mark.parametrize(
    "drift",
    [
        "written_scalar",
        "unwritten_scalar",
        "scene",
        "cue",
        "same_name_scene",
        "same_name_track",
        "note",
        "device",
        "automation",
        "transport",
    ],
)
def test_returned_first_setter_is_observed_before_any_later_native_write(
    state: Any, monkeypatch: pytest.MonkeyPatch, drift: str
) -> None:
    """Literal clamps/foreign state changes stop at the first completed native setter."""
    preview = state.helper.preview(preview_request(state))
    payload = approve(preview)
    operation = pending(payload)

    def alter(song: Any, field: str) -> None:
        if field != "signature_numerator":
            return
        if drift == "written_scalar":
            song._numerator = 2
        if drift == "unwritten_scalar":
            song._tempo = 110.0
        if drift == "scene":
            song.scenes[0]._tempo = 160.0
        if drift == "cue":
            song.cue_points[0].name = "Native cue drift"
        if drift == "same_name_scene":
            replacement = Scene()
            replacement.name = song.scenes[0].name
            replacement.enable_launch_overrides(150.0, 7, 8)
            song._scenes[0] = replacement
        if drift == "same_name_track":
            replacement = Track(song, state.foreign.name, "midi")
            replacement.canonical_parent = song
            replacement._clip_slots = state.foreign._clip_slots
            song._tracks[1] = replacement
        if drift == "note":
            state.clip._notes[41].velocity = 12.0
        if drift == "device":
            from live_model import Device, DeviceType

            new_device = Device("Foreign Utility", "Utility", DeviceType.audio_effect)
            new_device._canonical_parent = state.track
            state.track._devices.append(new_device)
        if drift == "automation":
            state.tempo_parameter._automation_state = AutomationState.playing
        if drift == "transport":
            from live_model import inject

            inject(song, "is_playing", True)

    calls = watch_setters(monkeypatch, after=alter)
    with pytest.raises(RuntimeError):
        state.helper.apply(("project_a", "part_a"), payload, operation)
    assert calls == [("signature_numerator", 3)]
    assert operation["native_mutation_started"] is True
    assert operation["song_settings_progress"] == {
        "started_fields": ["signature_numerator"],
        "returned_fields": ["signature_numerator"],
    }
    assert state.song.signature_denominator == 4
    assert operation["song_settings_partial"]["after"]["settings"]["tempo"] == (
        110.0 if drift == "unwritten_scalar" else 120.0
    )
    assert operation["song_settings_partial"]["after"]["settings"]["signature_numerator"] == (
        2 if drift == "written_scalar" else 3
    )


def test_final_setter_may_not_silently_change_an_already_verified_scalar(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The final-field clamp exception never excuses drift in an earlier meter write."""
    preview = state.helper.preview(preview_request(state))
    payload = approve(preview)
    operation = pending(payload)

    def alter(song: Any, field: str) -> None:
        if field == "tempo":
            song._numerator = 2

    calls = watch_setters(monkeypatch, after=alter)
    with pytest.raises(RuntimeError, match="completed scalar differs"):
        state.helper.apply(("project_a", "part_a"), payload, operation)
    assert calls == [("signature_numerator", 3), ("signature_denominator", 8), ("tempo", 92.5)]
    assert operation["native_mutation_started"] is True
    assert operation["song_settings_partial"]["after"]["settings"] == {
        "tempo": 92.5,
        "signature_numerator": 2,
        "signature_denominator": 8,
    }


@pytest.mark.parametrize("change", ["value", "parameter_handle", "device_handle"])
@pytest.mark.parametrize("timing", ["before", "after_first"])
def test_valid_device_guard_renewal_after_song_preview_cannot_transfer_its_approval(
    state: Any, monkeypatch: pytest.MonkeyPatch, change: str, timing: str
) -> None:
    """Modeled explicit Device renewal preserves Clip hashes but cannot replace preview handles."""
    import copy

    from test_managed_recovery import owned_drift

    device = owned_drift(state, monkeypatch)
    record = state.registry._bindings[("project_a", "part_a")]
    # Explicit fixture seam: a previously acknowledged native chain supplies
    # the current normal Part baseline before this independent Song preview.
    record["content_fingerprint"] = state.registry._capture(record)["content_fingerprint"]
    preview = state.helper.preview(preview_request(state))
    approved_hash = preview["preview"]["binding_guard"]["device_identity_fingerprint"]
    owned = record["_managed_devices"]

    def renew() -> None:
        if change == "value":
            device.parameters[1].value = 0.5
        if change == "parameter_handle":
            replacement = copy.copy(device.parameters[1])
            device._parameters = (device.parameters[0], replacement) + tuple(device.parameters[2:])
            owned["entries"][0]["parameter_handles"] = tuple(device.parameters)
        if change == "device_handle":
            replacement = copy.copy(device)
            replacement._parameters = [copy.copy(parameter) for parameter in device.parameters]
            for parameter in replacement._parameters:
                parameter._canonical_parent = replacement
            state.track._devices[0] = replacement
            owned["handles"] = (replacement,)
            owned["entries"][0]["handle"] = replacement
            owned["entries"][0]["parameter_handles"] = tuple(replacement.parameters)
        # Model separate explicit Device authority renewal: current private
        # handles/baseline are valid, rather than merely supplying stale guards.
        observed = state.registry._devices.capture(record)
        owned["baseline"] = observed["device_identity_fingerprint"]
        state.registry._devices.verify_retained_chain(record)
        assert (
            state.registry._capture(record)["content_fingerprint"] == record["content_fingerprint"]
        )
        if change != "value":
            assert observed["device_identity_fingerprint"] == approved_hash

    def after(song: Any, field: str) -> None:
        if field == "signature_numerator":
            renew()

    if timing == "before":
        renew()
    calls = watch_setters(monkeypatch, after=after if timing == "after_first" else None)
    operation = pending(approve(preview))
    with pytest.raises(RuntimeError, match="device"):
        state.helper.apply(("project_a", "part_a"), approve(preview), operation)
    if timing == "before":
        assert calls == [] and operation["native_mutation_started"] is False
        assert "song_settings_progress" not in operation
    else:
        assert (
            calls == [("signature_numerator", 3)] and operation["native_mutation_started"] is True
        )
        assert operation["song_settings_partial"]["after"]["settings"] == {
            "tempo": 120.0,
            "signature_numerator": 3,
            "signature_denominator": 4,
        }


def test_song_inspection_preserves_tokens_grants_and_reads_current_global_scalars(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A fresh actual120/4/4 mismatch remains usable preflight evidence, no setters."""
    from Sunny.managed_song_settings import INSPECTION_METHOD, MAX_PREVIEWS

    payload = preview_request(state)
    state.helper._current(payload)
    state.helper._previews = {f"{i:032x}": object() for i in range(MAX_PREVIEWS)}
    before_previews = dict(state.helper._previews)
    record = state.registry._bindings[("project_a", "part_a")]
    guard = record["content_fingerprint"]
    before_notes = dict(state.clip._notes)
    state.registry._song_settings = state.helper
    calls = watch_setters(monkeypatch)
    monkeypatch.setattr(
        "Sunny.managed_song_settings.uuid.uuid4",
        lambda: pytest.fail("Song inspection allocated a preview token"),
    )
    value = state.registry.dispatch(INSPECTION_METHOD, [payload])
    body = value["inspection"]
    assert set(value) == {"schema_version", "outcome", "inspection", "observation"}
    assert value["outcome"] == "observed"
    assert body["before"]["settings"] == {
        "tempo": 120.0,
        "signature_numerator": 4,
        "signature_denominator": 4,
    }
    assert body["desired"] == {
        "tempo": 92.5,
        "signature_numerator": 3,
        "signature_denominator": 8,
    }
    assert body["authority_origin"] == "none"
    assert body["native_mutation_started"] is False
    assert "preview_token" not in body
    assert body["scope"]["all_tracks_affected"] is True
    assert body["before"]["scenes"][0]["tempo"] == 150.0
    assert body["before"]["cue_points"][0]["time"] == 4.0
    assert calls == []
    assert state.clip._notes == before_notes
    assert state.helper._previews == before_previews
    assert record["content_fingerprint"] == guard
    assert state.registry._operations == {}


def test_song_inspection_keeps_actual_tempo_eligibility_and_current_cohort_checks(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """No token or new authority hides automated tempo or replaced current Scenes."""
    payload = preview_request(state)
    calls = watch_setters(monkeypatch)
    state.tempo_parameter._automation_state = AutomationState.playing
    with pytest.raises(RuntimeError, match="actual Main song_tempo"):
        state.helper.inspect(payload)
    state.tempo_parameter._automation_state = AutomationState.none
    original = state.helper._capture
    captures = 0

    def replaced(song: Any) -> Any:
        nonlocal captures
        captures += 1
        if captures == 2:
            scene = Scene(state.song.scenes[0].name)
            scene.enable_launch_overrides(150.0, 7, 8)
            state.song._scenes = (scene,)
        return original(song)

    monkeypatch.setattr(state.helper, "_capture", replaced)
    with pytest.raises(RuntimeError, match="inspection native state changed"):
        state.helper.inspect(payload)
    assert calls == []
    assert state.helper._previews == {}
