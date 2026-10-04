"""Literal current-object adoption checks against the offline Python source contract.

No running Live host or saved/reopened native identity is qualified here. The
Track parent and note modification adapter are explicit finite fixture seams.
"""

from __future__ import annotations

import copy
import json
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import (
    Clip,
    Device,
    DeviceType,
    LiveSet,
    MidiNote,
    MidiNoteSpecification,
    MidiNoteVector,
    Song,
    Track,
    inject,
)
from Sunny.handler import LomHandler
from Sunny.managed import ManagedRegistry, _digest, valid_managed_request
from Sunny.managed_capacity import MAX_MANAGED_RESPONSE_BYTES, json_wire_bound
from Sunny.managed_recovery import (
    ManagedRecovery,
    valid_adoption_request,
    valid_preview_request,
)


@pytest.fixture
def current(monkeypatch: pytest.MonkeyPatch) -> Any:
    """Build selected current objects independently of Sunny creation receipts."""
    live = LiveSet(midi_tracks=1, return_tracks=1, python_envelope_api=True).install(monkeypatch)
    song = live.song
    track = song.tracks[0]
    track.canonical_parent = song  # Explicit Track parent fixture; no persistent identity API.
    track.name = "Sunny|project_a|part_a|track"
    slot = track.clip_slots[0]
    slot.create_clip(4.0)
    clip = slot.clip
    clip.name = "Sunny|project_a|part_a|clip"
    clip.looping = False
    clip._notes = {
        41: MidiNote(41, MidiNoteSpecification(60, 0.0, 1.0, 96.0)),
        99: MidiNote(99, MidiNoteSpecification(67, 2.0, 0.5, 72.0, True, release_velocity=32.0)),
    }
    clip._user_mpe_expression = {41: {"pressure": [0.1, 0.8]}}
    clip._user_follow_actions = {"next": True}
    registry = ManagedRegistry(live.surface)
    registry._bridge_instance = "bridge_a"
    registry._document_token = "document_a"
    handler = LomHandler(
        live.surface, managed_registry=registry, envelope_authorizer=registry.authorize_envelope
    )
    registry.attach_handler(handler)
    recovery = ManagedRecovery(registry)
    monkeypatch.setattr("Sunny.managed_recovery.uuid.uuid4", lambda: SimpleNamespace(hex="a" * 32))
    return SimpleNamespace(
        live=live,
        song=song,
        track=track,
        slot=slot,
        clip=clip,
        registry=registry,
        recovery=recovery,
        handler=handler,
    )


def request(selector: Any = None) -> dict[str, Any]:
    """Use explicit current indices, which supply selection rather than authority."""
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "selector": {"track_index": 0, "slot_index": 0} if selector is None else selector,
    }


def approval(preview: Any) -> dict[str, Any]:
    """Approve this exact newly captured preview with a fresh operation token."""
    return {
        "document_token": "document_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "operation_id": "adopt_a",
        "preview_token": preview["preview_token"],
        "preview_fingerprint": preview["preview_fingerprint"],
        "explicit_adoption": True,
    }


def pending(intent: Any) -> dict[str, Any]:
    """Represent the existing dispatcher's reservation before invoking the helper."""
    return {
        "document_token": intent["document_token"],
        "operation_id": intent["operation_id"],
        "request_fingerprint": _digest({"name": "sunny_managed_adopt_clip", "request": intent}),
        "request": copy.deepcopy(intent),
        "name": "sunny_managed_adopt_clip",
        "outcome": "pending",
        "native_mutation_started": False,
    }


def adopt(current: Any, preview: Any) -> dict[str, Any]:
    """Invoke only the helper, leaving production dispatcher integration for its gate."""
    intent = approval(preview)
    result: dict[str, Any] = current.recovery.adopt(
        ("project_a", "part_a"), intent, pending(intent)
    )
    return result


def test_preview_has_no_authority_and_literal_current_ids(current: Any) -> None:
    """Names locate the native objects; preview must preserve every native field."""
    preview = current.recovery.preview(request())
    assert preview["authority_origin"] == "none"
    assert preview["historical_identity_proven"] is False
    assert preview["set_info"] == {"file_path": None, "name": None}
    assert preview["allowed_domains"] == [
        "existing_note_updates",
        "note_population_updates",
        "absent_mixer_step_lanes",
    ]
    assert preview["preserved_unknown_domains"] == [
        "mpe",
        "follow_actions",
        "existing_envelopes",
        "devices",
    ]
    assert [note["note_id"] for note in preview["observation"]["note_identity"]["notes"]] == [
        41,
        99,
    ]
    assert current.registry._bindings == {} and current.registry._operations == {}
    assert current.slot.clip is current.clip and current.song.tracks == (current.track,)
    assert current.clip._notes[41].velocity == 96.0
    assert preview["observation"]["content_boundary_complete"] is False
    assert preview["observation"]["manifest"]["mpe_note_expression_state_observed"] is False
    assert preview["observation"]["manifest"]["follow_actions_state_observed"] is False
    # Independent fixed cross-language fixture (not recomputed from the helper).
    assert (
        preview["preview_fingerprint"]
        == "075133e8c2da6c6a9d3cc983d000e479ab3b1e39c791e03db9127d9892fdecc3"
    )


def test_adoption_grants_current_ids_and_reconstructible_preview(current: Any) -> None:
    """Adoption publishes only explicit finite authority, without any native write."""
    preview = current.recovery.preview(request())
    acknowledgement = adopt(current, preview)
    retained = current.registry._bindings[("project_a", "part_a")]
    assert retained["owned_note_ids"] == {41, 99}
    assert retained["track"] is current.track and retained["clip"] is current.clip
    assert retained["adopted_device_cohort"] == ()
    assert retained["authority_origin"] == "explicit_adoption"
    assert acknowledgement["adoption"]["approved_note_ids"] == [41, 99]
    assert acknowledgement["adoption"]["historical_identity_proven"] is False
    assert acknowledgement["adoption"]["devices_preserved"] is True
    actual = copy.deepcopy(acknowledgement)
    metadata = actual.pop("adoption")["preview_metadata"]
    assert _digest({**metadata, "observation": actual}) == preview["preview_fingerprint"]
    assert current.clip._notes[41].velocity == 96.0 and current.clip._notes[99].mute is True
    assert current.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}
    assert current.clip._user_follow_actions == {"next": True}
    with pytest.raises(RuntimeError, match="already consumed"):
        adopt(current, preview)


def test_unique_tags_locate_same_current_objects_without_automatic_authority(current: Any) -> None:
    """Unique Sunny labels resolve a preview while leaving ownership absent."""
    preview = current.recovery.preview(
        request({"track_tag": current.track.name, "clip_tag": current.clip.name})
    )
    assert preview["observation"]["track_index"] == 0
    assert current.registry._bindings == {}
    assert adopt(current, preview)["adoption"]["authority_origin"] == "explicit_adoption"


def test_unknown_devices_and_existing_envelopes_are_preserved_only(current: Any) -> None:
    """A rack/opaque state does not block finite Clip authority or become device authority."""
    rack = Device(
        "User rack", "AudioEffectGroupDevice", DeviceType.audio_effect, can_have_chains=True
    )
    rack._canonical_parent = current.track
    rack._user_opaque_state = {"plugin": b"foreign"}
    current.track._devices.append(rack)
    current.clip._envelopes.add("existing user envelope")
    preview = current.recovery.preview(request())
    assert preview["observation"]["structural_boundary_complete"] is False
    assert preview["observation"]["manifest"]["clip"]["has_envelopes"] is True
    assert preview["observation"]["manifest"]["devices_empty"] is False
    adopt(current, preview)
    retained = current.registry._bindings[("project_a", "part_a")]
    assert retained["adopted_device_cohort"] == (rack,)
    assert "device_identity" not in retained
    assert not any(
        name in retained["allowed_domains"] for name in ("device_parameters", "device_insertion")
    )
    assert current.track._devices == [rack]
    assert rack._user_opaque_state == {"plugin": b"foreign"}
    assert current.clip._envelopes == {"existing user envelope"}


@pytest.mark.parametrize(
    "kind",
    [
        "note",
        "note_id",
        "end_marker",
        "arm",
        "playing",
        "name",
        "slot",
        "track",
        "device",
        "document",
        "set_info",
    ],
)
def test_drift_or_same_named_replacement_after_preview_never_grants_authority(
    current: Any, kind: str
) -> None:
    """Content equality and identical labels cannot authorize replacement native handles."""
    if kind == "device":
        device = Device("User device", "PluginDevice", DeviceType.audio_effect)
        device._canonical_parent = current.track
        current.track._devices.append(device)
    preview = current.recovery.preview(request())
    if kind == "note":
        current.clip._notes[41].velocity = 12.0
    elif kind == "note_id":
        current.clip._notes[141] = current.clip._notes.pop(41)
        current.clip._notes[141].note_id = 141
    elif kind == "end_marker":
        current.clip.end_marker = 8.0
    elif kind == "arm":
        current.track.arm = True
    elif kind == "playing":
        inject(current.clip, "is_playing", True)
    elif kind == "name":
        current.clip.name += "-changed"
    elif kind == "slot":
        duplicate = Clip(4.0, python_envelope_api=True)
        duplicate.name = current.clip.name
        duplicate.looping = False
        duplicate._notes = copy.deepcopy(current.clip._notes)
        duplicate._canonical_parent = current.slot
        current.slot._clip = duplicate
    elif kind == "track":
        replacement = Track(current.song, current.track.name, "midi")
        replacement.canonical_parent = current.song
        replacement._clip_slots = current.track._clip_slots
        current.song._tracks[0] = replacement
    elif kind == "device":
        replacement = Device("User device", "PluginDevice", DeviceType.audio_effect)
        replacement._canonical_parent = current.track
        current.track._devices[0] = replacement
    elif kind == "document":
        current.registry._surface._song = Song(current.live.application)
    elif kind == "set_info":
        current.song.name = "Changed Set"
    with pytest.raises(RuntimeError):
        adopt(current, preview)
    assert current.registry._bindings == {}


@pytest.mark.parametrize(
    "mutation", ["fingerprint", "token", "explicit_false", "extra", "foreign_binding"]
)
def test_explicit_approval_is_closed_and_preview_bound(current: Any, mutation: str) -> None:
    """No fallback from malformed, foreign or unappproved intent grants authority."""
    preview = current.recovery.preview(request())
    intent = approval(preview)
    if mutation == "fingerprint":
        intent["preview_fingerprint"] = "b" * 64
    elif mutation == "token":
        intent["preview_token"] = "b" * 32
    elif mutation == "explicit_false":
        intent["explicit_adoption"] = False
    elif mutation == "extra":
        intent["retry_old_operation"] = True
    elif mutation == "foreign_binding":
        intent["binding_key"] = "other_part"
    with pytest.raises(RuntimeError):
        current.recovery.adopt(("project_a", intent["binding_key"]), intent, pending(intent))
    assert current.registry._bindings == {}


def test_absent_or_ambiguous_tags_and_foreign_names_decline(current: Any) -> None:
    """Current discovery remains explicit and never renames user objects."""
    current.song.create_midi_track(-1).name = current.track.name
    with pytest.raises(RuntimeError, match="ambiguous"):
        current.recovery.preview(
            request({"track_tag": current.track.name, "clip_tag": current.clip.name})
        )
    current.track.name = "User piano"
    with pytest.raises(RuntimeError, match="Sunny names"):
        current.recovery.preview(request())
    assert current.track.name == "User piano" and not current.recovery._previews


def test_retained_native_objects_cannot_be_adopted_by_second_binding(current: Any) -> None:
    """An already retained concrete Track/Clip remains exclusively owned."""
    preview = current.recovery.preview(request())
    current.registry._bindings[("other_project", "other_part")] = {
        "track": current.track,
        "clip": current.clip,
    }
    with pytest.raises(RuntimeError, match="another binding"):
        adopt(current, preview)
    assert list(current.registry._bindings) == [("other_project", "other_part")]


def test_observation_can_change_caller_copy_without_changing_retained_approval(
    current: Any,
) -> None:
    """Caller-visible evidence is not the bridge's immutable approved capture."""
    preview = current.recovery.preview(request())
    preview["observation"]["manifest"]["notes"][0]["velocity"] = 0.0
    acknowledgement = adopt(current, preview)
    assert acknowledgement["manifest"]["notes"][0]["velocity"] == 96.0


@pytest.mark.parametrize(
    "selector",
    [
        {},
        {"track_index": True, "slot_index": 0},
        {"track_index": -1, "slot_index": 0},
        {"track_index": 1 << 31, "slot_index": 0},
        {"track_tag": chr(0xD800), "clip_tag": "a"},
        {"track_tag": "", "clip_tag": "a"},
        {"track_index": 0, "slot_index": 0, "authority": True},
    ],
)
def test_selector_rejects_invalid_domains(selector: Any) -> None:
    """Selection fields have independent finite bounds and valid Unicode."""
    assert valid_preview_request(request(selector)) is False


def test_exhausted_preview_capacity_preserves_existing_tokens(current: Any) -> None:
    """A bounded no-eviction preview registry cannot silently transfer stale handles."""
    preview = current.recovery.preview(request())
    current.recovery._previews.update({f"{i:032x}": {"used": True} for i in range(255)})
    with pytest.raises(RuntimeError, match="capacity exhausted"):
        current.recovery.preview(request())
    assert current.recovery._previews["a" * 32]["used"] is False
    assert adopt(current, preview)["adoption"]["approved_note_ids"] == [41, 99]


def test_random_preview_token_collision_never_overwrites_prior_selection(current: Any) -> None:
    """Even a broken UUID provider cannot overwrite a retained preview."""
    first = current.recovery.preview(request())
    with pytest.raises(RuntimeError, match="collision bound exhausted"):
        current.recovery.preview(request())
    assert len(current.recovery._previews) == 1
    assert current.recovery._previews["a" * 32]["result"] == first


def test_initial_legacy_population_is_unavailable(current: Any) -> None:
    """A ranged Live11.0 query is insufficient to approve every current native ID."""
    current.clip._live_version = (11, 0, 0)
    with pytest.raises(RuntimeError, match="entire current Clip note population"):
        current.recovery.preview(request())
    assert not current.recovery._previews and not current.registry._bindings


def test_existing_native_id_velocity_update_after_adoption_preserves_opaque_state(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Approve current IDs, then exercise the existing in-place updater once."""
    preview = current.recovery.preview(request())
    acknowledgement = adopt(current, preview)
    calls: list[Any] = []
    selected: list[Any] = []

    def select(ids: Any) -> MidiNoteVector:
        snapshots = [copy.copy(current.clip._notes[i]) for i in ids]
        selected.extend(snapshots)
        return MidiNoteVector(snapshots)

    def apply(notes: Any) -> None:
        calls.append(tuple(note.note_id for note in notes))
        assert all(any(note is item for item in selected) for note in notes)
        for note in notes:
            current.clip._notes[note.note_id].velocity = note.velocity

    monkeypatch.setattr(current.clip, "get_notes_by_id", select)
    monkeypatch.setattr(current.clip, "apply_note_modifications", apply, raising=False)
    actual = acknowledgement["note_identity"]["notes"][0]
    intent = {
        "document_token": "document_a",
        "operation_id": "velocity_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": acknowledgement["content_fingerprint"],
        "changes": [
            {
                "note_id": 41,
                "expected": {name: value for name, value in actual.items() if name != "note_id"},
                "updates": {"velocity": 84.0},
            }
        ],
    }
    result = current.registry.dispatch("sunny_managed_update_notes", [intent])
    assert result["outcome"] == "acknowledged", result
    assert calls == [(41,)]
    assert sorted(current.clip._notes) == [41, 99]
    assert current.clip._notes[41].velocity == 84.0 and current.clip._notes[99].velocity == 72.0
    assert current.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}
    assert current.clip._user_follow_actions == {"next": True}
    assert (
        current.registry._bindings[("project_a", "part_a")]["authority_origin"]
        == "explicit_adoption"
    )
    assert current.registry._bindings[("project_a", "part_a")]["adopted_device_cohort"] == ()


def test_large_preview_has_explicit_bounded_unavailability_without_token(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A full valid current population beyond the actual TCP frame grants nothing."""
    actual = current.registry._capture(
        {
            "track": current.track,
            "slot": current.slot,
            "clip": current.clip,
            "track_tag": current.track.name,
            "clip_tag": current.clip.name,
        }
    )
    note = actual["note_identity"]["notes"][0]
    # A closed finite capture fixture with literal 51000 distinct native IDs;
    # both copies retain complete semantic fields, independently exceeding wire size.
    actual["note_identity"]["notes"] = [{**note, "note_id": i} for i in range(51000)]
    actual["manifest"]["notes"] = [
        {name: value for name, value in note.items() if name != "note_id"} for _ in range(51000)
    ]
    assert len(json.dumps({"success": True, "value": actual}).encode()) > MAX_MANAGED_RESPONSE_BYTES
    actual["content_fingerprint"] = _digest(actual["manifest"])
    actual["note_identity_fingerprint"] = _digest(actual["note_identity"])
    monkeypatch.setattr(current.registry, "_capture", lambda record: actual)
    with pytest.raises(RuntimeError, match="ReplyCapacityUnavailable"):
        current.recovery.preview(request())
    assert current.recovery._previews == {} and current.registry._bindings == {}


def test_adoption_journal_capacity_checked_before_authority_publication(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Exact request/context/journal overhead can make a preview-size reply unreportable."""
    preview = current.recovery.preview(request())
    intent = approval(preview)
    operation = pending(intent)
    # Independent framed-byte boundary exercises guard placement without allocating
    # giant native captures again. Preview fit is not a future journal guarantee.
    boundary = json_wire_bound({"success": True, "value": preview})
    monkeypatch.setattr("Sunny.managed_capacity.MAX_MANAGED_RESPONSE_BYTES", boundary)
    with pytest.raises(RuntimeError, match="ReplyCapacityUnavailable"):
        current.recovery.adopt(("project_a", "part_a"), intent, operation)
    assert current.registry._bindings == {}
    assert current.recovery._previews[preview["preview_token"]]["used"] is True
    assert operation["native_mutation_started"] is False


def test_valid_adoption_has_literal_explicit_boolean(current: Any) -> None:
    """Numeric truthiness or absent approval is never an adoption grant."""
    intent = approval(current.recovery.preview(request()))
    assert valid_adoption_request(intent)
    intent["explicit_adoption"] = 1
    assert not valid_adoption_request(intent)


def test_observed_set_path_name_are_context_not_historical_identity(current: Any) -> None:
    """A real available filename/name is reported without asserting persistent identity."""
    current.song.file_path = "/project/current.als"
    current.song.name = "Current Set"
    preview = current.recovery.preview(request())
    assert preview["set_info"] == {"file_path": "/project/current.als", "name": "Current Set"}
    assert preview["historical_identity_proven"] is False
    assert adopt(current, preview)["adoption"]["historical_identity_proven"] is False


def test_saved_preview_has_no_authority_in_new_bridge_epoch(current: Any) -> None:
    """Persisted content/token/hash alone cannot recreate private native handles."""
    preview = current.recovery.preview(request())
    restored = ManagedRegistry(current.registry._surface)
    restored._document_token = "document_a"
    handler = LomHandler(
        restored._surface,
        managed_registry=restored,
        envelope_authorizer=restored.authorize_envelope,
    )
    restored.attach_handler(handler)
    recovery = ManagedRecovery(restored)
    intent = approval(preview)
    with pytest.raises(RuntimeError, match="preview token is absent"):
        recovery.adopt(("project_a", "part_a"), intent, pending(intent))
    assert restored._bindings == {} and restored._operations == {}
    assert current.slot.clip is current.clip and sorted(current.clip._notes) == [41, 99]


def test_same_epoch_explicit_adoption_lost_reply_uses_query_only_retained_journal(
    current: Any,
) -> None:
    """The integration seam retains an actual non-native-write authority result once.

    This models the existing token journal's wrapper; production dispatcher
    registration and product durable fencing still require their own gate.
    """
    preview = current.recovery.preview(request())
    intent = approval(preview)
    operation = pending(intent)
    current.registry._operations["adopt_a"] = operation
    acknowledgement = current.recovery.adopt(("project_a", "part_a"), intent, operation)
    operation.update({"outcome": "acknowledged", "result": acknowledgement})
    recovered = current.registry.dispatch(
        "sunny_managed_operation", [{"document_token": "document_a", "operation_id": "adopt_a"}]
    )
    assert recovered["outcome"] == "acknowledged"
    assert recovered["native_mutation_started"] is False
    assert recovered["result"]["adoption"]["approved_note_ids"] == [41, 99]
    assert current.registry._bindings[("project_a", "part_a")]["clip"] is current.clip
    assert len(current.registry._bindings) == 1 and len(current.song.tracks) == 1
    fresh = ManagedRegistry(current.registry._surface)
    fresh._document_token = "document_a"
    assert (
        fresh.dispatch(
            "sunny_managed_operation", [{"document_token": "document_a", "operation_id": "adopt_a"}]
        )["outcome"]
        == "unknown_operation"
    )
    assert fresh._bindings == {}  # Missing old journal never transfers authority.


def fresh_tokens(monkeypatch: pytest.MonkeyPatch) -> None:
    """Supply distinct deterministic preview tokens without changing managed epoch IDs."""
    tokens = iter(("a" * 32, "b" * 32, "c" * 32, "d" * 32))
    monkeypatch.setattr(
        "Sunny.managed_recovery.uuid",
        SimpleNamespace(uuid4=lambda: SimpleNamespace(hex=next(tokens))),
    )


def test_same_handle_explicit_refresh_accepts_reviewed_current_drift(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A new approval repairs current authority; ordinary inspect cannot accept drift."""
    fresh_tokens(monkeypatch)
    initial = adopt(current, current.recovery.preview(request()))
    old_record = current.registry._bindings[("project_a", "part_a")]
    old_content = old_record["content_fingerprint"]
    current.clip._notes[41].velocity = 12.0
    current.clip.end_marker = 8.0
    current.clip._notes[777] = MidiNote(777, MidiNoteSpecification(72, 6.0, 0.5, 80.0))
    observed = current.registry.dispatch(
        "sunny_managed_observe",
        [{name: value for name, value in request().items() if name != "selector"}],
    )
    assert observed["outcome"] == "observed"
    assert old_record["content_fingerprint"] == old_content == initial["content_fingerprint"]
    preview = current.recovery.preview(request())
    refreshed = adopt(current, preview)
    retained = current.registry._bindings[("project_a", "part_a")]
    assert retained is not old_record
    assert (
        retained["track"] is old_record["track"]
        and retained["slot"] is old_record["slot"]
        and retained["clip"] is old_record["clip"]
    )
    assert retained["owned_note_ids"] == {41, 99, 777}
    assert refreshed["manifest"]["clip"]["end_marker"] == 8.0
    assert refreshed["manifest"]["notes"][0]["velocity"] == 12.0
    assert retained["content_fingerprint"] == refreshed["content_fingerprint"] != old_content
    assert refreshed["adoption"]["authority_origin"] == "explicit_adoption"
    assert current.clip._user_mpe_expression == {41: {"pressure": [0.1, 0.8]}}


def test_existing_binding_same_named_replacement_is_never_refreshed(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Explicit refresh of a retained object cannot transfer authority to a clone."""
    fresh_tokens(monkeypatch)
    adopt(current, current.recovery.preview(request()))
    old_record = current.registry._bindings[("project_a", "part_a")]
    duplicate = Clip(4.0, python_envelope_api=True)
    duplicate.name = current.clip.name
    duplicate.looping = False
    duplicate._notes = copy.deepcopy(current.clip._notes)
    duplicate._canonical_parent = current.slot
    current.slot._clip = duplicate
    with pytest.raises(RuntimeError, match="different native objects"):
        current.recovery.preview(request())
    assert current.registry._bindings[("project_a", "part_a")] is old_record
    assert old_record["clip"] is current.clip and current.slot.clip is duplicate


def owned_drift(current: Any, monkeypatch: pytest.MonkeyPatch) -> Any:
    """Install an explicit existing-authority fixture and use the actual device verifier."""
    from Sunny.managed_devices import ManagedDevices

    device = Device("Drift", "Drift", DeviceType.instrument, ("LP Freq",))
    device._canonical_parent = current.track
    device.voice_mode_index = 1
    device.voice_mode_list = ("Mono", "Poly", "Unison", "Stereo")
    device.voice_count_index = 2
    device.voice_count_list = ("1", "2", "8", "32")
    current.track._devices.append(device)
    retained = current.registry._bindings[("project_a", "part_a")]
    retained["_managed_devices"] = {
        "handles": (device,),
        "entries": [
            {
                "device_key": "source",
                "device": {
                    "browser_name": "Drift",
                    "class_name": "Drift",
                    "type": 1,
                    "role": "source",
                    "insertion_policy": "AppendOwnedChain",
                },
                "handle": device,
            }
        ],
        "baseline": "",
        "append_authority": "explicit_current_device_adoption",
        "authority_track": current.track,
    }
    helper = ManagedDevices(current.registry)
    retained["_managed_devices"]["baseline"] = helper.capture(retained)[
        "device_identity_fingerprint"
    ]
    original = current.registry._capture
    monkeypatch.setattr(
        current.registry, "_capture", lambda record: {**original(record), **helper.capture(record)}
    )
    current.recovery._device_authorizer = helper
    return device


def test_same_handle_refresh_preserves_unchanged_owned_device_keys(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Only the actual helper's unchanged private chain/parameter guard preserves keys."""
    fresh_tokens(monkeypatch)
    adopt(current, current.recovery.preview(request()))
    device = owned_drift(current, monkeypatch)
    old_record = current.registry._bindings[("project_a", "part_a")]
    old_state = old_record["_managed_devices"]
    current.clip._notes[41].velocity = 12.0
    preview = current.recovery.preview(request())
    assert preview["observation"]["device_identity"]["cohort"][0]["device_key"] == "source"
    acknowledgement = adopt(current, preview)
    retained = current.registry._bindings[("project_a", "part_a")]
    assert retained["_managed_devices"] is not old_state
    assert retained["_managed_devices"]["entries"][0] is not old_state["entries"][0]
    assert retained["_managed_devices"]["handles"] == (device,)
    assert retained["_managed_devices"]["entries"][0]["parameter_handles"] == tuple(
        device.parameters
    )
    assert retained["_managed_devices"]["entries"][0]["device_key"] == "source"
    assert acknowledgement["device_identity_fingerprint"] == old_state["baseline"]
    assert acknowledgement["adoption"]["devices_preserved"] is True
    assert retained["_managed_devices"]["append_authority"] == "explicit_current_device_adoption"
    assert retained["_managed_devices"]["authority_track"] is current.track


def test_same_handle_refresh_revokes_dirty_device_authority_but_preserves_cohort(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Explicit note approval never silently adopts a changed device parameter guard."""
    fresh_tokens(monkeypatch)
    adopt(current, current.recovery.preview(request()))
    device = owned_drift(current, monkeypatch)
    device.parameters[1].value = 0.5
    preview = current.recovery.preview(request())
    assert "device_identity" not in preview["observation"]
    acknowledgement = adopt(current, preview)
    retained = current.registry._bindings[("project_a", "part_a")]
    assert "_managed_devices" not in retained
    assert retained["adopted_device_cohort"] == (device,)
    assert "device_identity" not in acknowledgement
    assert device.parameters[1].value == 0.5


def test_clean_device_disposition_changing_after_preview_declines_refresh(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Approval does not authorize a clean-to-revoked authority transition afterward."""
    fresh_tokens(monkeypatch)
    adopt(current, current.recovery.preview(request()))
    device = owned_drift(current, monkeypatch)
    old_record = current.registry._bindings[("project_a", "part_a")]
    preview = current.recovery.preview(request())
    device.parameters[1].value = 0.5
    with pytest.raises(RuntimeError, match="disposition changed"):
        adopt(current, preview)
    assert current.registry._bindings[("project_a", "part_a")] is old_record
    assert old_record["_managed_devices"]["entries"][0]["device_key"] == "source"


def test_explicit_current_id_adoption_authorizes_named_population_add_delete(
    current: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Approve current native IDs, then selectively delete/add without replay."""
    registry = ManagedRegistry(current.registry._surface)
    registry._bridge_instance = "bridge_a"
    registry._document_token = "document_a"
    handler = LomHandler(
        registry._surface,
        managed_registry=registry,
        envelope_authorizer=registry.authorize_envelope,
    )
    registry.attach_handler(handler)
    current.registry = registry
    current.recovery = registry._recovery
    preview = current.recovery.preview(request())
    acknowledgement = adopt(current, preview)
    assert "note_population_updates" in acknowledgement["adoption"]["allowed_domains"]
    assert registry._bindings[("project_a", "part_a")]["owned_note_ids"] == {41, 99}
    existing_pressure = copy.deepcopy(current.clip._user_mpe_expression)
    current.clip._envelopes.add("existing user lane")
    # Review the current envelope state explicitly before approving this fresh authority.
    fresh_tokens(monkeypatch)
    current.recovery._previews.clear()
    acknowledgement = adopt(current, current.recovery.preview(request()))
    old = acknowledgement["note_identity"]["notes"][1]
    note = {
        "pitch": 72,
        "start_time": 3.0,
        "duration": 0.5,
        "velocity": 80.0,
        "mute": False,
        "probability": 1.0,
        "velocity_deviation": 0.0,
        "release_velocity": 64.0,
    }
    current.clip._next_note_id = 1000
    calls: list[Any] = []
    native_remove = current.clip.remove_notes_by_id
    native_add = current.clip.add_new_notes

    def remove(ids: Any) -> None:
        calls.append(("remove", tuple(ids)))
        native_remove(ids)

    def add(specifications: Any) -> Any:
        specifications = list(specifications)
        calls.append(("add", tuple(spec.pitch for spec in specifications)))
        return native_add(specifications)

    monkeypatch.setattr(current.clip, "remove_notes_by_id", remove)
    monkeypatch.setattr(current.clip, "add_new_notes", add)
    population = {
        "document_token": "document_a",
        "operation_id": "population_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": acknowledgement["content_fingerprint"],
        "changes": [],
        "deletions": [
            {
                "note_id": 99,
                "expected": {name: value for name, value in old.items() if name != "note_id"},
            }
        ],
        "additions": [{"note_key": "e10_n0", "note": note}],
    }
    assert valid_managed_request("sunny_managed_revise_note_population", [population])
    result = registry.dispatch("sunny_managed_revise_note_population", [population])
    assert result["outcome"] == "acknowledged", result
    assert calls == [("remove", (99,)), ("add", (72,))]
    assert sorted(current.clip._notes) == [41, 1000]
    assert current.clip._notes[41].velocity == 96.0
    assert current.clip._notes[1000].pitch == 72 and current.clip._notes[1000].start_time == 3.0
    assert result["result"]["note_population_update"]["addition_associations"] == [
        {"note_key": "e10_n0", "note_id": 1000}
    ]
    assert (
        result["result"]["note_population_update"]["observed_population_cardinality_match"] is True
    )
    assert result["result"]["note_population_update"]["untouched_notes_preserved"] is True
    assert registry._bindings[("project_a", "part_a")]["owned_note_ids"] == {41, 1000}
    assert current.clip._user_mpe_expression == existing_pressure
    assert current.clip._user_follow_actions == {"next": True}
    assert current.clip._envelopes == {"existing user lane"}
    recovered = registry.dispatch(
        "sunny_managed_operation",
        [{"document_token": "document_a", "operation_id": "population_a"}],
    )
    assert recovered == result
    assert calls == [("remove", (99,)), ("add", (72,))]
    assert current.slot.clip is current.clip


def test_current_ids_without_named_population_approval_do_not_authorize_deletion(
    current: Any,
) -> None:
    """Owning current IDs and the original two domains never imply deletion rights."""
    registry = current.registry
    acknowledgement = adopt(current, current.recovery.preview(request()))
    record = registry._bindings[("project_a", "part_a")]
    record["allowed_domains"] = ("existing_note_updates", "absent_mixer_step_lanes")
    expected = acknowledgement["note_identity"]["notes"][1].copy()
    del expected["note_id"]
    payload = {
        "document_token": "document_a",
        "operation_id": "population_a",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": acknowledgement["content_fingerprint"],
        "changes": [],
        "deletions": [{"note_id": 99, "expected": expected}],
        "additions": [],
    }
    receipt = registry.dispatch("sunny_managed_revise_note_population", [payload])
    assert receipt["outcome"] == "declined"
    assert receipt["native_mutation_started"] is False
    assert "PopulationAuthorityUnavailable" in receipt["error"]
    assert sorted(current.clip._notes) == [41, 99]
    assert record["owned_note_ids"] == {41, 99}
