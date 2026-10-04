"""Independent native cohort/setter oracles; no running host qualification."""

from __future__ import annotations

import copy
import math
from typing import Any

import pytest
from live_model import Device, DeviceType, Song, Track
from Sunny.managed import _digest
from Sunny.managed_routing import APPLY_METHOD, ManagedRouting, valid_intent
from test_managed_devices import Parameter
from test_managed_recovery import adopt, request
from test_managed_recovery import current as selected_part_fixture


@pytest.fixture
def state(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Supply genuine Track-parent/Return-send source seams explicitly."""
    current = selected_part_fixture.__wrapped__(monkeypatch)
    song = current.song
    song.loop_start, song.loop_length = 0.0, 8.0
    for track in (*song.tracks, *song.return_tracks, song.master_track):
        track.canonical_parent = song
    # The model's Return construction only updates player sends. Official Mixer
    # contract says one send per Return; native fixture fills existing Returns.
    for track in song.return_tracks:
        while len(track.mixer_device.sends) < len(song.return_tracks):
            track._mixer._add_send("Existing return send")
    current.track._devices.append(
        Device("Opaque user synth", "PluginInstrument", DeviceType.instrument)
    )
    adopt(current, current.recovery.preview(request()))
    current.helper = ManagedRouting(current.registry)
    current.calls = []
    original = Song.create_return_track

    def native_create(song: Any) -> Any:
        current.calls.append("create_return_track")
        created = original(song)
        created.canonical_parent = song
        for track in song.return_tracks:
            while len(track.mixer_device.sends) < len(song.return_tracks):
                track._mixer._add_send("New return send")
        return created

    monkeypatch.setattr(Song, "create_return_track", native_create)
    return current


def preview_request(state: Any, intent: Any) -> Any:
    """Use actual current retained Part fingerprints as the context."""
    record = state.registry._bindings[("project_a", "part_a")]
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": record["content_fingerprint"],
        "expected_note_identity_fingerprint": record["note_identity_fingerprint"],
        "intent": intent,
    }


def approved(preview: Any) -> Any:
    """Explicit immutable exact current-object approval, not a native write."""
    body = preview["preview"]
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "route_a",
        "expected_content_fingerprint": body["binding_guard"]["content_fingerprint"],
        "expected_note_identity_fingerprint": body["binding_guard"]["note_identity_fingerprint"],
        "intent": copy.deepcopy(body["intent"]),
        "preview_token": preview["preview_token"],
        "preview_fingerprint": preview["preview_fingerprint"],
        "approved_preview": copy.deepcopy(body),
        "explicit_current_routing_approval": True,
    }


def apply(state: Any, preview: Any) -> Any:
    """Exercise helper after a literal existing-journal pending reservation."""
    payload = approved(preview)
    operation = {
        "document_token": "document_a",
        "operation_id": "route_a",
        "name": APPLY_METHOD,
        "request": payload,
        "request_fingerprint": _digest({"name": APPLY_METHOD, "request": payload}),
        "outcome": "pending",
        "native_mutation_started": False,
    }
    return state.helper.apply(("project_a", "part_a"), payload, operation), operation


def test_return_appends_one_preserves_notes_and_original_send_and_revokes_mixer(state: Any) -> None:
    """Exact new count/handles/native call list prove append and current grant."""
    old = state.song.return_tracks[0]
    old_send = state.track.mixer_device.sends[0]
    old_send._value = 0.375
    state.registry._seal(state.registry._bindings[("project_a", "part_a")])
    notes = dict(state.clip._notes)
    invalidated = []
    invalidate = state.registry._mixer.invalidate_retained_cohort

    def invalidate_observed(record: Any) -> None:
        invalidated.append(record)
        invalidate(record)

    state.registry._mixer.invalidate_retained_cohort = invalidate_observed
    preview = state.helper.preview(
        preview_request(state, {"kind": "create_return", "aux_key": "aux_room"})
    )
    assert state.calls == [] and len(state.song.return_tracks) == 1
    result, operation = apply(state, preview)
    assert state.calls == ["create_return_track"] and len(state.song.return_tracks) == 2
    assert state.song.return_tracks[0] is old
    assert state.track.mixer_device.sends[0] is old_send and old_send.value == 0.375
    assert [
        len(track.mixer_device.sends) for track in (*state.song.tracks, *state.song.return_tracks)
    ] == [2, 2, 2]
    assert state.song.return_tracks[1].name == "Sunny|project_a|aux_room|return"
    assert state.clip._notes == notes
    assert state.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}
    assert invalidated == [state.registry._bindings[("project_a", "part_a")]]
    assert (
        result["routing"]["desired_match"] is True and operation["native_mutation_started"] is True
    )
    assert result["routing"]["progress"] == {
        "started": ["create_return_track", "return_name"],
        "returned": ["create_return_track", "return_name"],
    }
    with pytest.raises(RuntimeError, match="retained|capacity|consumed"):
        apply(state, preview)
    assert state.calls == ["create_return_track"]


def test_current_return_adoption_is_zero_setters_and_same_name_replacement_is_declined(
    state: Any,
) -> None:
    """Names/index select the actual current handle but cannot replace authority."""
    intent = {"kind": "adopt_return", "aux_key": "aux_room", "return_index": 0}
    preview = state.helper.preview(preview_request(state, intent))
    selected = state.song.return_tracks[0]
    _, operation = apply(state, preview)
    assert state.calls == [] and operation["native_mutation_started"] is False
    replacement = Track(state.song, selected.name, "return")
    replacement.canonical_parent = state.song
    replacement._mixer._add_send("Existing return send")
    state.song._return_tracks[0] = replacement
    with pytest.raises(RuntimeError, match="replacement|replaced|drift"):
        state.helper.preview(preview_request(state, intent))
    assert state.calls == []


def test_return_creation_cohort_drift_stops_before_any_native_append(state: Any) -> None:
    """Equal-valued replacement send has a different native handle."""
    preview = state.helper.preview(
        preview_request(state, {"kind": "create_return", "aux_key": "aux_room"})
    )
    replacement = copy.copy(state.track.mixer_device.sends[0])
    state.track._mixer._sends = (replacement,)
    with pytest.raises(RuntimeError, match="objects|baseline"):
        apply(state, preview)
    assert state.calls == [] and len(state.song.return_tracks) == 1


def test_output_main_requires_native_attached_object_and_preserves_note_ids(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Pinned Push2/routing.py522 uses attached_object, never the route name."""
    # Put current output on the user Return explicitly, before any preview.
    route = next(
        item
        for item in state.track.available_output_routing_types
        if item.attached_object is state.song.return_tracks[0]
    )
    state.track.output_routing_type = route
    state.registry._seal(state.registry._bindings[("project_a", "part_a")])
    main = next(
        item
        for item in state.track.available_output_routing_types
        if item.attached_object is state.song.master_track
    )
    intent = {
        "kind": "output_type",
        "destination": "main",
        "group_key": None,
        "route_identifier": str(hash(main)),
    }
    preview = state.helper.preview(preview_request(state, intent))
    calls = []
    native = Track.output_routing_type

    def write(track: Any, value: Any) -> None:
        calls.append(value.attached_object)
        native.__set__(track, value)

    monkeypatch.setattr(
        Track, "output_routing_type", property(lambda track: native.__get__(track, Track), write)
    )
    result, operation = apply(state, preview)
    assert calls == [state.song.master_track]
    assert state.track.output_routing_type.attached_object is state.song.master_track
    assert list(state.clip._notes) == [41, 99]
    assert result["routing"]["desired_match"] is True
    assert operation["routing_progress"]["returned"] == ["output_routing_type"]


def test_identical_master_name_with_foreign_attached_target_cannot_authorize_route(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Actual opaque target association defeats a same-name/category fake."""
    native = Track._output_types

    def options(track: Any) -> Any:
        return [
            (key, name, category, state.song.return_tracks[0] if name == "Master" else attached)
            for key, name, category, attached in native(track)
        ]

    monkeypatch.setattr(Track, "_output_types", options)
    master = next(
        item for item in state.track.available_output_routing_types if item.display_name == "Master"
    )
    with pytest.raises(RuntimeError, match="TargetAssociationUnavailable"):
        state.helper.preview(
            preview_request(
                state,
                {
                    "kind": "output_type",
                    "destination": "main",
                    "group_key": None,
                    "route_identifier": str(hash(master)),
                },
            )
        )
    assert state.calls == []


def adopt_return(state: Any) -> None:
    """Approve current Return handle explicitly before selecting its send."""
    apply(
        state,
        state.helper.preview(
            preview_request(
                state, {"kind": "adopt_return", "aux_key": "aux_room", "return_index": 0}
            )
        ),
    )


def send_parameter(state: Any) -> Any:
    """Independent nonlinear native send: -48+48*sqrt(value), not a fader formula."""
    parameter = Parameter(
        "Send A",
        lambda value: f"{-48 + 48 * math.sqrt(value):.12f} dB",
        minimum=0.0,
        maximum=1.0,
        value=0.5,
    )
    parameter._canonical_parent = state.track.mixer_device
    state.track._mixer._sends = (parameter,)
    state.registry._seal(state.registry._bindings[("project_a", "part_a")])
    return parameter


def test_native_send_dB_search_and_readback_are_independent_of_level_normalization(
    state: Any,
) -> None:
    """Literal target-24 resolves0.25 and changes only this actual parameter."""
    adopt_return(state)
    parameter = send_parameter(state)
    before_notes = dict(state.clip._notes)
    intent = {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": False,
    }
    preview = state.helper.preview(preview_request(state, intent))
    assert parameter.writes == []
    assert preview["preview"]["selected"]["candidate"]["internal_value"] == 0.25
    result, operation = apply(state, preview)
    assert parameter.writes == [0.25] and parameter.value == 0.25
    assert state.clip._notes == before_notes
    assert result["routing"]["desired_match"] is True
    assert result["routing"]["logical_send_complete"] is False
    assert result["routing"]["tap_policy_observed"] is False
    assert operation["routing_progress"]["returned"] == ["send_value"]


def test_send_selected_existing_envelope_and_automation_are_preserved(state: Any) -> None:
    """An unknown selected native envelope never becomes a static knob grant."""
    adopt_return(state)
    parameter = send_parameter(state)
    state.clip.create_automation_envelope(parameter).insert_step(0.0, 4.0, 0.7)
    state.registry._seal(state.registry._bindings[("project_a", "part_a")])
    intent = {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": True,
    }
    with pytest.raises(RuntimeError, match="preserve existing selected send envelope"):
        state.helper.preview(preview_request(state, intent))
    assert parameter.writes == []


def test_return_native_append_drift_stops_before_name_set_and_records_partial(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Native append side effect to an old send prevents every later writer."""
    preview = state.helper.preview(
        preview_request(state, {"kind": "create_return", "aux_key": "aux_room"})
    )
    native = Song.create_return_track

    def drift(song: Any) -> Any:
        result = native(song)
        state.track.mixer_device.sends[0]._value = 0.8
        return result

    monkeypatch.setattr(Song, "create_return_track", drift)
    payload = approved(preview)
    operation = {"request": payload, "name": APPLY_METHOD, "native_mutation_started": False}
    with pytest.raises(RuntimeError, match="append changed untouched"):
        state.helper.apply(("project_a", "part_a"), payload, operation)
    assert state.calls == ["create_return_track"]
    assert state.song.return_tracks[-1].name == "B-Return"
    assert operation["routing_progress"] == {
        "started": ["create_return_track"],
        "returned": ["create_return_track"],
    }
    assert "routing_partial" in operation and operation["native_mutation_started"] is True
    assert ("project_a", "aux_room") not in state.helper._returns


@pytest.mark.parametrize("mutate", ["scene", "notes", "input_route"])
def test_native_output_writer_drift_stops_without_authority_refresh(
    state: Any, monkeypatch: pytest.MonkeyPatch, mutate: str
) -> None:
    """Input/notes/user Scene drift is not authorized by an output-route choice."""
    route = next(
        item
        for item in state.track.available_output_routing_types
        if item.attached_object is state.song.return_tracks[0]
    )
    state.track.output_routing_type = route
    state.registry._seal(state.registry._bindings[("project_a", "part_a")])
    master = next(
        item
        for item in state.track.available_output_routing_types
        if item.attached_object is state.song.master_track
    )
    preview = state.helper.preview(
        preview_request(
            state,
            {
                "kind": "output_type",
                "destination": "main",
                "group_key": None,
                "route_identifier": str(hash(master)),
            },
        )
    )
    native = Track.output_routing_type

    def write(track: Any, value: Any) -> None:
        native.__set__(track, value)
        if mutate == "scene":
            state.song.scenes[0].name = "User new scene name"
        elif mutate == "notes":
            state.clip._notes[41].velocity = 12.0
        else:
            track.input_routing_type = next(
                item
                for item in track.available_input_routing_types
                if item.display_name == "No Input"
            )

    monkeypatch.setattr(
        Track, "output_routing_type", property(lambda track: native.__get__(track, Track), write)
    )
    payload = approved(preview)
    operation = {"request": payload, "name": APPLY_METHOD, "native_mutation_started": False}
    with pytest.raises(RuntimeError, match="untouched|notes/device"):
        state.helper.apply(("project_a", "part_a"), payload, operation)
    assert operation["native_mutation_started"] is True


def current_group(state: Any, monkeypatch: pytest.MonkeyPatch) -> Any:
    """Official read-only Group flags/topology, explicitly supplied native seams."""
    group = Track(state.song, "Current strings", "group")
    group.canonical_parent = state.song
    group._mixer._add_send("Existing return send")
    state.song._tracks.append(group)
    state.track._group_actual = group
    monkeypatch.setattr(
        Track, "is_foldable", property(lambda track: track._kind == "group"), raising=False
    )
    monkeypatch.setattr(
        Track,
        "is_grouped",
        property(lambda track: getattr(track, "_group_actual", None) is not None),
    )
    monkeypatch.setattr(
        Track, "group_track", property(lambda track: getattr(track, "_group_actual", None))
    )
    # A reopened/current grouped Clip has no historical registry binding grant.
    state.registry._bindings.clear()
    return group


def group_request() -> Any:
    """Actual current selection is a locator; explicit approval supplies authority."""
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "intent": {
            "kind": "adopt_group",
            "group_key": "group_strings",
            "track_index": 1,
            "member_binding_keys": ["part_a"],
        },
        "selector": {"track_index": 0, "slot_index": 0},
    }


def test_current_group_bootstrap_grants_only_exact_hierarchy_and_no_part_binding(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Saved grouped objects can be approved before separate current Clip adoption."""
    group = current_group(state, monkeypatch)
    track, clip = state.track, state.clip
    preview = state.helper.preview_group(group_request())
    assert not state.helper.verify_group_selection("project_a", "part_a", track)
    result, operation = apply(state, preview)
    assert set(result) == {"group_adoption"}
    assert result["group_adoption"]["part_authority_granted"] is False
    assert result["group_adoption"]["device_authority_granted"] is False
    assert state.registry._bindings == {} and state.calls == []
    assert operation["native_mutation_started"] is False
    assert state.helper.verify_group_selection("project_a", "part_a", track)
    assert not state.helper.verify_group_selection("foreign_project", "part_a", track)
    assert state.track is track and state.clip is clip and state.track.group_track is group


def test_current_group_same_name_replacement_cannot_inherit_approved_members(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Concrete native Group identity defeats an equal-valued renamed substitute."""
    group = current_group(state, monkeypatch)
    apply(state, state.helper.preview_group(group_request()))
    replacement = Track(state.song, group.name, "group")
    replacement.canonical_parent = state.song
    replacement._mixer._add_send("Existing return send")
    state.song._tracks[1] = replacement
    state.track._group_actual = replacement
    assert not state.helper.verify_group_selection("project_a", "part_a", state.track)
    assert state.helper.capture_group_authority("project_a", "part_a", state.track) == {}
    # Same-key replacement still cannot be explicitly approved in this epoch.
    with pytest.raises(RuntimeError, match="replacement"):
        state.helper.preview_group(group_request())
    assert state.registry._bindings == {} and state.calls == []


def test_current_group_after_preview_member_geometry_drift_cannot_publish_grant(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Current anchor evidence is rechecked before even zero-setter authority."""
    current_group(state, monkeypatch)
    preview = state.helper.preview_group(group_request())
    state.clip._notes[41].velocity = 1.0
    with pytest.raises(RuntimeError, match="Group/anchor objects changed"):
        apply(state, preview)
    assert state.helper._groups == {} and state.registry._bindings == {}


@pytest.mark.parametrize(
    "extra", [{"unexpected": True}, {"kind": "delete_return"}, {"pre_fader": True}]
)
def test_closed_creation_request_refuses_unsupported_mutations(extra: Any) -> None:
    """Intent grammar is finite and cannot select an arbitrary native setter."""
    assert not valid_intent({"kind": "create_return", "aux_key": "aux_room", **extra})


def test_group_inspection_reports_only_previously_explicit_current_grant(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Typed proof comes from the original grant, never names/current class alone."""
    current_group(state, monkeypatch)
    preview = state.helper.preview_group(group_request())
    assert state.helper.capture_group_authority("project_a", "part_a", state.track) == {}
    apply(state, preview)
    evidence = state.helper.capture_group_authority("project_a", "part_a", state.track)
    body = evidence["group_authority"]
    assert body["approved_preview_fingerprint"] == preview["preview_fingerprint"]
    assert body["member_binding_keys"] == ["part_a"]
    assert body["member_track_indices"] == [0] and body["group_track_index"] == 1
    assert body["selected_binding_key"] == "part_a" and body["selected_track_index"] == 0
    assert body["context"] == {"bridge_instance": "bridge_a", "document_token": "document_a"}
    assert evidence["group_authority_fingerprint"] == _digest(body)
    assert state.registry._bindings == {} and state.calls == []
    state.registry._document_token = "document_b"
    assert state.helper.capture_group_authority("project_a", "part_a", state.track) == {}
    assert state.helper._groups == {}


def test_send_readback_pins_shared_decimal_rounding(state: Any) -> None:
    """Ambient Decimal rounding cannot change the producer/consumer contract."""
    from decimal import ROUND_UP, localcontext

    parameter = state.track.mixer_device.sends[0]
    parameter.str_for_value = lambda value: "100.00 dB"
    body = {"intent": {"level_db": -1e-300, "tolerance_db": 100.0}}
    with localcontext() as context:
        context.rounding = ROUND_UP
        result = state.helper._send_readback(body, parameter)
    assert result["display_value"] == 100.0
    assert result["absolute_display_error"] == 100.0
    assert result["matches_intent"] is True


def test_return_reply_capacity_uses_actual_second_anchor_before_native_append(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A tiny first binding cannot budget a much larger selected native Part."""
    from live_model import MidiNote, MidiNoteSpecification
    from test_managed_recovery import approval, pending

    second = Track(state.song, "Sunny|project_b|part_b|track", "midi")
    second.canonical_parent = state.song
    state.song._tracks.append(second)
    second.clip_slots[0].create_clip(4.0)
    clip = second.clip_slots[0].clip
    clip.name = "Sunny|project_b|part_b|clip"
    clip.looping = False
    clip._notes = {
        1000 + index: MidiNote(
            1000 + index,
            MidiNoteSpecification(60 + index % 12, (index % 512) / 128.0, 0.01, 64.0),
        )
        for index in range(8000)
    }
    read = {
        "document_token": "document_a",
        "project_key": "project_b",
        "binding_key": "part_b",
        "selector": {"track_index": 1, "slot_index": 0},
    }
    from types import SimpleNamespace

    monkeypatch.setattr("Sunny.managed_recovery.uuid.uuid4", lambda: SimpleNamespace(hex="b" * 32))
    selected = state.recovery.preview(read)
    adopt_payload = approval(selected)
    adopt_payload.update(project_key="project_b", binding_key="part_b")
    state.recovery.adopt(("project_b", "part_b"), adopt_payload, pending(adopt_payload))
    record = state.registry._bindings[("project_b", "part_b")]
    payload = {
        "document_token": "document_a",
        "project_key": "project_b",
        "binding_key": "part_b",
        "expected_content_fingerprint": record["content_fingerprint"],
        "expected_note_identity_fingerprint": record["note_identity_fingerprint"],
        "intent": {"kind": "create_return", "aux_key": "aux_second"},
    }
    preview = state.helper.preview(payload)
    approved_payload = approved(preview)
    approved_payload.update(project_key="project_b", binding_key="part_b")
    operation = {
        "document_token": "document_a",
        "operation_id": "route_a",
        "name": APPLY_METHOD,
        "request": approved_payload,
        "request_fingerprint": _digest({"name": APPLY_METHOD, "request": approved_payload}),
        "outcome": "pending",
        "native_mutation_started": False,
    }
    with pytest.raises(RuntimeError, match="ReplyCapacityUnavailable"):
        state.helper.apply(("project_b", "part_b"), approved_payload, operation)
    assert state.calls == [] and len(state.song.return_tracks) == 1
    assert operation["native_mutation_started"] is False
    assert list(clip._notes) == list(range(1000, 9000))


def test_send_silent_final_clamp_is_truthful_acknowledgement(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Final actual physical mismatch does not forge success or compensate the write."""
    from live_model import DeviceParameter

    adopt_return(state)
    parameter = state.track.mixer_device.sends[0]
    parameter.str_for_value = lambda value: f"{-48.0 + 48.0 * value**0.5:.12f} dB"
    preview = state.helper.preview(
        preview_request(
            state,
            {
                "kind": "send_level",
                "aux_key": "aux_room",
                "level_db": -24.0,
                "tolerance_db": 0.0,
                "requested_pre_fader": False,
            },
        )
    )
    native = DeviceParameter.value
    writes = []

    def clamp(actual: Any, value: Any) -> None:
        if actual is parameter:
            writes.append(value)
            value = 0.5
        native.__set__(actual, value)

    monkeypatch.setattr(
        DeviceParameter,
        "value",
        property(lambda actual: native.__get__(actual, DeviceParameter), clamp),
    )
    result, operation = apply(state, preview)
    assert writes == [0.25] and parameter.value == 0.5
    assert result["routing"]["desired_match"] is False
    assert result["routing"]["send_readback"]["matches_intent"] is False
    assert result["routing"]["send_readback"]["absolute_display_error"] > 9.0
    assert operation["native_mutation_started"] is True
    assert operation["routing_progress"] == {"started": ["send_value"], "returned": ["send_value"]}
    assert list(state.clip._notes) == [41, 99]


def candidate_request(state: Any) -> Any:
    """The public inspector requires retained Part guards, never a guessed hash."""
    payload = preview_request(state, {"kind": "create_return", "aux_key": "aux_room"})
    del payload["intent"]
    return payload


def test_candidate_inspector_exposes_actual_advertised_ids_without_grant(state: Any) -> None:
    """Native IDs are discoverable with actual target association and no preview/write."""
    previews, returns, groups = (
        dict(state.helper._previews),
        dict(state.helper._returns),
        dict(state.helper._groups),
    )
    result = state.helper.candidates(candidate_request(state))
    assert set(result) == {
        "context",
        "project_key",
        "binding_key",
        "observation",
        "frame",
        "native_mutation_started",
        "authority_origin",
    }
    assert result["context"] == {"bridge_instance": "bridge_a", "document_token": "document_a"}
    assert result["project_key"] == "project_a" and result["binding_key"] == "part_a"
    assert result["native_mutation_started"] is False and result["authority_origin"] == "none"
    row = result["frame"]["mixers"][result["observation"]["track_index"]]
    main = next(
        route
        for route in state.track.available_output_routing_types
        if route.attached_object is state.song.master_track
    )
    advertised = next(
        route
        for route in row["routing"]["available_types"]
        if route["attached_target"] == {"kind": "main", "index": None}
    )
    assert advertised["identifier"] == str(hash(main))
    assert (
        state.helper._previews == previews
        and state.helper._returns == returns
        and state.helper._groups == groups
    )
    assert state.calls == [] and set(state.clip._notes) == {41, 99}


def test_candidate_inspector_rejects_equal_valued_handle_drift(state: Any) -> None:
    """A read-only capture cannot offer selection IDs from mixed native cohorts."""
    frame = state.helper._frame
    count = 0
    original = state.track.mixer_device.sends[0]

    def drift(song: Any) -> Any:
        nonlocal count
        captured = frame(song)
        count += 1
        if count == 1:
            state.track._mixer._sends = (copy.copy(original),)
        return captured

    state.helper._frame = drift
    with pytest.raises(RuntimeError, match="changed|retained|drift"):
        state.helper.candidates(candidate_request(state))
    assert state.calls == [] and state.helper._previews == {} and state.helper._returns == {}


def test_send_inspection_reads_actual_formatter_without_preview_retention(
    state: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Nonlinear native .5 differs from target .25; reading grants/writes nothing."""
    from Sunny.managed_routing import MAX_PREVIEWS, SEND_INSPECTION_METHOD

    adopt_return(state)
    parameter = send_parameter(state)
    state.registry._routing = state.helper
    intent = {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": False,
    }
    payload = preview_request(state, intent)
    # Initialize the actual epoch before deliberately exhausting preview capacity.
    state.helper._current(payload)
    state.helper._previews = {f"{i:032x}": object() for i in range(MAX_PREVIEWS)}
    before_previews = dict(state.helper._previews)
    record = state.registry._bindings[("project_a", "part_a")]
    before_guard = record["content_fingerprint"]
    before_notes = dict(state.clip._notes)
    monkeypatch.setattr(
        "Sunny.managed_routing.uuid.uuid4",
        lambda: pytest.fail("read-only Send inspection allocated a token"),
    )
    result = state.registry.dispatch(SEND_INSPECTION_METHOD, [payload])
    body = result["inspection"]
    assert set(result) == {"outcome", "inspection", "observation"}
    assert result["outcome"] == "observed"
    assert "preview_token" not in body
    assert body["authority_origin"] == "none"
    assert body["native_mutation_started"] is False
    actual = body["send_readback"]
    assert actual["internal_value"] == 0.5
    assert actual["display"] == "-14.058874503046 dB"
    assert actual["matches_intent"] is False
    assert body["selected"]["candidate"]["internal_value"] == 0.25
    assert parameter.writes == []
    assert state.clip._notes == before_notes
    assert state.helper._previews == before_previews
    assert record["content_fingerprint"] == before_guard
    assert state.registry._operations == {}
    # Simulate a legitimate owned Send write separately; fresh actual readback changes.
    parameter.value = 0.25
    state.registry._seal(record)
    result = state.registry.dispatch(SEND_INSPECTION_METHOD, [preview_request(state, intent)])
    assert result["inspection"]["send_readback"]["display"] == "-24.000000000000 dB"
    assert result["inspection"]["send_readback"]["matches_intent"] is True
    assert parameter.writes == [0.25]


@pytest.mark.parametrize("drift", ["scene", "parameter"])
def test_send_inspection_rejects_native_formatter_and_handle_drift(
    state: Any, monkeypatch: pytest.MonkeyPatch, drift: str
) -> None:
    """Real formatter callback drift cannot be reported as a stable fresh read."""
    from Sunny.managed_routing import SEND_INSPECTION_METHOD, valid_request

    adopt_return(state)
    parameter = send_parameter(state)
    intent = {
        "kind": "send_level",
        "aux_key": "aux_room",
        "level_db": -24.0,
        "tolerance_db": 0.0,
        "requested_pre_fader": False,
    }
    payload = preview_request(state, intent)
    assert valid_request(SEND_INSPECTION_METHOD, payload)
    bad = copy.deepcopy(payload)
    bad["intent"] = {"kind": "create_return", "aux_key": "other"}
    assert not valid_request(SEND_INSPECTION_METHOD, bad)
    original = state.helper._send_readback

    def changed(body: Any, native: Any) -> Any:
        value = original(body, native)
        if drift == "scene":
            state.song.scenes[0].name = "Changed while reading actual Send"
        else:
            # Equal public values do not confer identity on another native control.
            state.track._mixer._sends = (copy.copy(parameter),)
        return value

    monkeypatch.setattr(state.helper, "_send_readback", changed)
    with pytest.raises(RuntimeError, match="inspection native state changed"):
        state.helper.inspect_send(payload)
    assert parameter.writes == []
    assert state.helper._previews == {}
