"""Retained managed identities and fault outcomes, without a running Live host.

Explicit source-contract fixtures do not establish saved/reopened host behavior.
"""

from __future__ import annotations

import copy
import threading
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import Clip, ClipSlot, LiveSet, Song, inject
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler, _request_allowed
from Sunny.managed import ManagedRegistry, _canonical_bytes, _digest


def test_managed_typed_digest_literal_oracle() -> None:
    """Independent literal bytes cover Unicode, IEEE bits, bool and wire integer ends."""
    value = {
        "bool": [False, True],
        "float": 587555049599699.8,
        "integer": [-(1 << 63), (1 << 64) - 1],
        "null": None,
        "signed_zero": [-0.0, 0.0],
        "unicode": "é漢𝄞",
    }
    expected = (
        "SM1;O6:{S4:bool;A2:[F;T;]S5:float;D4300b3069023969e;"
        "S7:integer;A2:[I-9223372036854775808;I18446744073709551615;]S4:null;N;"
        "S11:signed_zero;A2:[D8000000000000000;D0000000000000000;]S7:unicode;S9:é漢𝄞;}"
    ).encode()
    assert _canonical_bytes(value) == expected
    assert _digest(value) == "377adb935f9443dd8ebdb1438b23a229c4c132fa498b775507b40bf21bf2793c"
    assert _digest(False) != _digest(0)
    assert _digest(1) != _digest(1.0)
    assert _digest(0.0) != _digest(-0.0)


@pytest.mark.parametrize(
    "invalid", [float("inf"), float("-inf"), float("nan"), -(1 << 63) - 1, 1 << 64, chr(0xD800)]
)
def test_managed_digest_rejects_nonfinite_or_unrepresentable_values(invalid: Any) -> None:
    """Reject inputs which cannot share a finite valid UTF8 wire representation."""
    with pytest.raises((ValueError, UnicodeEncodeError)):
        _digest(invalid)


def test_managed_digest_has_finite_size_and_nesting_limits() -> None:
    """Reject excessive payloads before computing a content fingerprint."""
    with pytest.raises(ValueError, match="16 MiB"):
        _digest("a" * (16 * 1024 * 1024))
    value: Any = 0
    for _ in range(32):
        value = [value]
    assert _digest(value)
    with pytest.raises(ValueError, match="nesting"):
        _digest([value])


def test_actual_acknowledgement_has_literal_typed_fingerprints(target: Any) -> None:
    """Match a real offline Python acknowledgement to independent literal digests."""
    target.registry._document_token = "document_a"
    target.context["document_token"] = "document_a"
    request = intent(target, "operation_a")
    request["notes"] = request["notes"][:1]
    acknowledged = call(target, "sunny_managed_create_clip", request)
    assert acknowledged["outcome"] == "acknowledged"
    assert (
        acknowledged["request_fingerprint"]
        == "555afd544844f3d89e1c17892c830105ea7ff1e91a45f4ba5b2f64c27eda93f5"
    )
    assert (
        acknowledged["result"]["content_fingerprint"]
        == "b37c538c439e4641dd5ea19ccbb5cbb8d7568e643b8fe4a598ee43e84303c622"
    )


def call(target: Any, name: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
    """Dispatch one managed Song call and require a valid response envelope."""
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": name,
            "args": [] if payload is None else [payload],
        }
    )
    assert response["success"], response
    return response["value"]


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    """Attach the actual registry authorizer to explicit source-contract objects."""
    live = LiveSet(midi_tracks=1, return_tracks=1, python_envelope_api=True).install(monkeypatch)
    surface = live.surface
    registry = ManagedRegistry(surface)
    handler = LomHandler(
        surface, envelope_authorizer=registry.authorize_envelope, managed_registry=registry
    )
    registry.attach_handler(handler)
    target = SimpleNamespace(live=live, surface=surface, registry=registry, handler=handler)
    target.context = call(target, "sunny_managed_context")
    return target


def intent(target: Any, operation: str = "create") -> dict[str, Any]:
    """Return literal deterministic notes and generated clip geometry."""
    return {
        "document_token": target.context["document_token"],
        "operation_id": operation,
        "project_key": "project_a",
        "binding_key": "part_a",
        "clip_end": 4.0,
        "signature_numerator": 4,
        "signature_denominator": 4,
        "notes": [
            {
                "pitch": 60,
                "start_time": 0.0,
                "duration": 1.0,
                "velocity": 96,
                "mute": False,
                "probability": 1.0,
                "velocity_deviation": 0.0,
                "release_velocity": 64.0,
            },
            {
                "pitch": 67,
                "start_time": 2.0,
                "duration": 0.5,
                "velocity": 72,
                "mute": True,
                "probability": 1.0,
                "velocity_deviation": 0.0,
                "release_velocity": 32.0,
            },
        ],
    }


def observe(target: Any) -> dict[str, Any]:
    """Read the current managed native identity and content boundary."""
    return call(
        target,
        "sunny_managed_observe",
        {
            "document_token": target.context["document_token"],
            "project_key": "project_a",
            "binding_key": "part_a",
        },
    )


def replace(
    target: Any, acknowledgement: dict[str, Any], operation: str = "replace"
) -> dict[str, Any]:
    """Construct a new token with the prior observed content guard."""
    request = intent(target, operation)
    request["expected_content_fingerprint"] = acknowledgement["result"]["content_fingerprint"]
    request["notes"][0]["pitch"] = 64
    return request


def restart(target: Any) -> Any:
    """Recreate the bridge registry while retaining the same modeled Live Set."""
    registry = ManagedRegistry(target.surface)
    handler = LomHandler(
        target.surface, envelope_authorizer=registry.authorize_envelope, managed_registry=registry
    )
    registry.attach_handler(handler)
    fresh = SimpleNamespace(
        live=target.live, surface=target.surface, registry=registry, handler=handler
    )
    fresh.context = call(fresh, "sunny_managed_context")
    return fresh


def rebind(
    fresh: Any, acknowledgement: dict[str, Any], operation: str = "rebind"
) -> dict[str, Any]:
    """Construct explicit recovery from a persisted actual content manifest."""
    return {
        "document_token": fresh.context["document_token"],
        "operation_id": operation,
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_manifest": copy.deepcopy(acknowledgement["result"]["manifest"]),
    }


def test_create_ack_is_actual_readback_and_lost_reply_replays_without_duplicate(
    target: Any,
) -> None:
    """Recover the actual retained acknowledgement without duplicating native objects."""
    original = target.live.song.tracks[0]
    original.name = "User piano"
    original.clip_slots[0].create_clip(8.0)
    scene_name = target.live.song.scenes[0].name
    request = intent(target)
    acknowledgement = call(target, "sunny_managed_create_clip", request)
    assert acknowledgement["outcome"] == "acknowledged", acknowledgement
    result = acknowledgement["result"]
    assert result["content_boundary_complete"] is False
    assert result["structural_boundary_complete"] is True
    assert result["manifest"]["mpe_note_expression_state_observed"] is False
    assert result["manifest"]["follow_actions_state_observed"] is False
    assert result["observed_notes_match_request"] is True
    assert result["observed_clip_properties_match_request"] is True
    assert result["manifest"]["notes"][0]["pitch"] == 60
    assert result["manifest"]["notes"][1]["pitch"] == 67
    assert result["track_index"] == 1 and result["slot_index"] == 0
    assert target.live.song.tracks[1].name == "Sunny|project_a|part_a|track"
    assert target.live.song.tracks[1].clip_slots[0].clip.name == "Sunny|project_a|part_a|clip"
    # A reconnect keeps the Surface's main-thread registry. Lost response is
    # reconciled by a separate read, and exact token replay returns actual ack.
    recovered = call(
        target,
        "sunny_managed_operation",
        {
            "document_token": target.context["document_token"],
            "operation_id": "create",
        },
    )
    assert recovered == acknowledgement
    assert call(target, "sunny_managed_create_clip", request) == acknowledgement
    assert len(target.live.song.tracks) == 2
    assert original.name == "User piano" and original.clip_slots[0].clip.end_marker == 8.0
    assert target.live.song.scenes[0].name == scene_name


def test_same_token_different_payload_is_rejected_without_native_mutation(target: Any) -> None:
    """Reject a changed token payload while preserving the original native notes."""
    request = intent(target)
    call(target, "sunny_managed_create_clip", request)
    changed = copy.deepcopy(request)
    changed["notes"][0]["pitch"] = 61
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_managed_create_clip",
            "args": [changed],
        }
    )
    assert not response["success"] and "reused" in response["error"]
    assert len(target.live.song.tracks) == 2
    assert observe(target)["manifest"]["notes"][0]["pitch"] == 60


def test_created_native_identities_survive_track_reordering_and_unknown_fields_block_deletion(
    target: Any,
) -> None:
    """Resolve moved native identities and decline deletion of unobserved content."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    owned = target.live.song.tracks[1]
    old_clip = owned.clip_slots[0].clip
    user = target.live.song.tracks[0]
    target.live.song._tracks.reverse()
    assert observe(target)["track_index"] == 0
    replaced = call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))
    assert replaced["outcome"] == "declined", replaced
    assert "MpeExpressionUnavailable" in replaced["error"]
    assert "FollowActionsUnavailable" in replaced["error"]
    assert owned.clip_slots[0].clip is old_clip
    assert observe(target)["manifest"]["notes"][0]["pitch"] == 60
    assert target.live.song.tracks[1] is user
    assert len(target.live.song.tracks) == 2


@pytest.mark.parametrize(
    "drift",
    [
        "notes",
        "name",
        "marker",
        "mixer",
        "routing",
        "envelopes",
        "new_clip",
        "arrangement",
        "take",
        "device",
        "arm",
        "playing",
    ],
)
def test_guard_preserves_user_edits_and_unrelated_native_content(target: Any, drift: str) -> None:
    """Decline isolated authoring when observed managed content or runtime drifts."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    track = target.live.song.tracks[1]
    clip = track.clip_slots[0].clip
    if drift == "notes":
        next(iter(clip._notes.values())).pitch = 61
    elif drift == "name":
        clip.name = "User rename"
    elif drift == "marker":
        clip.end_marker = 5.0
    elif drift == "mixer":
        track.mixer_device.panning.value = 0.3
    elif drift == "routing":
        inject(track, "output_routing_type", target.live.song.tracks[0].input_routing_type)
    elif drift == "envelopes":
        clip.create_automation_envelope(track.mixer_device.panning).insert_step(0.0, 4.0, 0.2)
    elif drift == "new_clip":
        target.live.song.create_scene(-1)
        track.clip_slots[1].create_clip(8.0)
    elif drift == "arrangement":
        inject(track, "arrangement_clips", (clip,))
    elif drift == "take":
        inject(track, "take_lanes", (object(),))
    elif drift == "device":
        track.insert_device("Utility")
    elif drift == "arm":
        track.arm = True
    elif drift == "playing":
        inject(clip, "is_playing", True)
    response = call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))
    assert response["outcome"] == "declined", response
    assert response["native_mutation_started"] is False
    assert track.clip_slots[0].clip is clip
    lane = {
        "parameter": {"kind": "panning"},
        "clip_end": 4.0,
        "interpolation": "step",
        "points": [{"time": 0.0, "value": 0.0}],
    }
    author = call(
        target,
        "sunny_managed_author_envelope",
        {
            "document_token": target.context["document_token"],
            "operation_id": "drift_lane",
            "project_key": "project_a",
            "binding_key": "part_a",
            "expected_content_fingerprint": acknowledgement["result"]["content_fingerprint"],
            "lane": lane,
        },
    )
    assert author["outcome"] == "declined" and author["native_mutation_started"] is False
    assert len(target.live.song.tracks) == 2


@pytest.mark.parametrize("fault", ["track", "clip", "notes", "readback"])
def test_fault_after_native_mutation_retains_partial_objects_and_never_reruns(
    target: Any, monkeypatch: Any, fault: str
) -> None:
    """Retain known partial native objects and report the same failure on replay."""
    if fault == "track":
        implementation = Song.create_midi_track

        def create(song: Any, index: int) -> None:
            implementation(song, index)
            raise RuntimeError("Lost after native Track create")

        monkeypatch.setattr(Song, "create_midi_track", create)
    elif fault == "clip":
        implementation = ClipSlot.create_clip

        def create(slot: Any, end: float) -> None:
            implementation(slot, end)
            raise RuntimeError("Lost after native Clip create")

        monkeypatch.setattr(ClipSlot, "create_clip", create)
    elif fault == "notes":
        implementation = Clip.add_new_notes

        def add(clip: Any, specifications: Any) -> Any:
            result = implementation(clip, specifications)
            raise RuntimeError(f"Lost after inserting {len(result)} notes")

        monkeypatch.setattr(Clip, "add_new_notes", add)
    else:
        monkeypatch.setattr(
            Clip,
            "get_all_notes_extended",
            lambda *_: (_ for _ in ()).throw(RuntimeError("Readback unavailable")),
        )
    request = intent(target)
    acknowledgement = call(target, "sunny_managed_create_clip", request)
    assert acknowledgement["outcome"] == "indeterminate", acknowledgement
    assert acknowledgement["native_mutation_started"] is True
    assert acknowledgement["partial_binding_retained"] is True
    assert len(target.live.song.tracks) == 2
    assert call(target, "sunny_managed_create_clip", request) == acknowledgement
    new_token = intent(target, "second_create")
    assert call(target, "sunny_managed_create_clip", new_token)["outcome"] == "declined"
    assert len(target.live.song.tracks) == 2


def test_no_scene_declines_before_mutation_and_does_not_create_global_scene(target: Any) -> None:
    """Require an existing first Scene before creating any native Track."""
    target.live.song._scenes.clear()
    result = call(target, "sunny_managed_create_clip", intent(target))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert len(target.live.song.tracks) == 1 and len(target.live.song.scenes) == 0


def test_new_live_set_resets_epoch_and_does_not_prove_old_operation_absent(target: Any) -> None:
    """Distinguish a changed document from proof that an earlier operation was absent."""
    call(target, "sunny_managed_create_clip", intent(target))
    target.surface._song = LiveSet(midi_tracks=1).song
    stale = call(
        target,
        "sunny_managed_operation",
        {
            "document_token": target.context["document_token"],
            "operation_id": "create",
        },
    )
    assert stale["outcome"] == "unknown_epoch"
    assert "request" not in stale
    assert call(target, "sunny_managed_create_clip", intent(target))["outcome"] == "unknown_epoch"
    assert len(target.surface.song().tracks) == 1
    new_context = call(target, "sunny_managed_context")
    assert new_context["document_token"] != target.context["document_token"]
    assert new_context["bridge_instance"] == target.context["bridge_instance"]


def test_restart_requires_complete_persisted_manifest_and_unique_tags(target: Any) -> None:
    """Preserve tagged native objects while complete recovery remains unavailable."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    fresh = restart(target)
    assert fresh.context["bridge_instance"] != target.context["bridge_instance"]
    assert observe(fresh)["outcome"] == "recovery_unavailable"
    assert call(fresh, "sunny_managed_create_clip", intent(fresh))["outcome"] == "declined"
    assert len(target.live.song.tracks) == 2
    rebound = call(fresh, "sunny_managed_rebind", rebind(fresh, acknowledgement))
    assert rebound["outcome"] == "declined", rebound
    assert observe(fresh)["ownership_retained"] is False
    assert len(target.live.song.tracks) == 2


@pytest.mark.parametrize(
    "mismatch", ["note", "duplicate_tag", "missing_tag", "bool_schema", "type_alias"]
)
def test_restart_never_adopts_names_alone_or_incomplete_wrong_content(
    target: Any, mismatch: str
) -> None:
    """Reject ambiguous tags, wrong content and Boolean manifest schema aliases."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    fresh = restart(target)
    request = rebind(fresh, acknowledgement)
    if mismatch == "note":
        request["expected_manifest"]["notes"][0]["pitch"] = 61
    elif mismatch == "duplicate_tag":
        target.live.song.create_midi_track(-1)
        target.live.song.tracks[-1].name = target.live.song.tracks[1].name
    elif mismatch == "missing_tag":
        target.live.song.tracks[1].name = "User renamed"
    elif mismatch == "bool_schema":
        request["expected_manifest"]["schema_version"] = True
    else:
        request["expected_manifest"]["clip"]["muted"] = 0
    if mismatch == "bool_schema":
        assert not _request_allowed("call", "song", "sunny_managed_rebind", [request])
        return
    result = call(fresh, "sunny_managed_rebind", request)
    assert result["outcome"] == "declined", result
    assert observe(fresh)["ownership_retained"] is False


def test_live11_0_create_retains_finite_evidence_without_claiming_whole_population(
    monkeypatch: Any,
) -> None:
    """Retain useful legacy creation evidence without permitting destructive recovery."""
    live = LiveSet((11, 0, 0), midi_tracks=1).install(monkeypatch)
    surface = live.surface
    registry = ManagedRegistry(surface)
    handler = LomHandler(surface, managed_registry=registry)
    registry.attach_handler(handler)
    target = SimpleNamespace(live=live, surface=surface, registry=registry, handler=handler)
    target.context = call(target, "sunny_managed_context")
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    assert acknowledgement["outcome"] == "acknowledged", acknowledgement
    assert acknowledgement["result"]["observed_notes_match_request"] is True
    assert acknowledgement["result"]["content_boundary_complete"] is False
    assert acknowledgement["result"]["manifest"]["entire_clip_population_observed"] is False
    assert (
        call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))["outcome"]
        == "declined"
    )
    fresh = restart(target)
    assert (
        call(fresh, "sunny_managed_rebind", rebind(fresh, acknowledgement))["outcome"] == "declined"
    )


def test_owned_envelope_authorizes_only_active_exact_native_lane_and_samples_independently(
    target: Any, monkeypatch: Any
) -> None:
    """Authorize one owned lane and distinguish actual readback from cached intent."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    clip = target.live.song.tracks[1].clip_slots[0].clip
    parameter = target.live.song.tracks[1].mixer_device.panning
    lane = {
        "parameter": {"kind": "panning"},
        "clip_end": 4.0,
        "interpolation": "step",
        "points": [{"time": 0.0, "value": -0.5}, {"time": 2.0, "value": 0.25}],
    }

    def generic() -> Any:
        return target.handler.handle(
            {
                "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
                "type": "call",
                "path": "song/tracks/1/clip_slots/0/clip",
                "name": "sunny_author_step_envelope",
                "args": [lane],
            }
        )

    assert not generic()["success"] and not clip.has_envelopes
    authorizations = []
    original = target.registry.authorize_envelope

    def authorize(track: Any, actual_clip: Any, actual_parameter: Any) -> bool:
        authorizations.append((track, actual_clip, actual_parameter, threading.get_ident()))
        return original(track, actual_clip, actual_parameter)

    target.handler._envelope_authorizer = authorize
    request = {
        "document_token": target.context["document_token"],
        "operation_id": "envelope",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": acknowledgement["result"]["content_fingerprint"],
        "lane": lane,
    }
    result = call(target, "sunny_managed_author_envelope", request)
    assert result["outcome"] == "acknowledged", result
    assert result["result"]["acknowledgement"]["steps_inserted"] == 2
    assert result["result"]["content_boundary_complete"] is False
    assert len(authorizations) == 3
    assert all(
        entry[1] is clip and entry[2] is parameter and entry[3] == threading.main_thread().ident
        for entry in authorizations
    )
    assert not original(target.live.song.tracks[1], clip, parameter)
    assert not generic()["success"]
    clip.automation_envelope(parameter).insert_step(2.0, 2.0, 0.4)
    query = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song/tracks/1/clip_slots/0/clip",
            "name": "sunny_get_step_envelope",
            "args": [{"parameter": {"kind": "panning"}, "sample_times": [0.25, 3.0]}],
        }
    )
    assert query["value"]["samples"] == [{"time": 0.25, "value": -0.5}, {"time": 3.0, "value": 0.4}]
    assert call(target, "sunny_managed_author_envelope", request) == result
    # Full envelope populations cannot be read through value_at_time samples.
    assert (
        call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))["outcome"]
        == "declined"
    )
    fresh = restart(target)
    assert call(fresh, "sunny_managed_rebind", rebind(fresh, result))["outcome"] == "declined"


@pytest.mark.parametrize("change", ["domain", "end", "device"])
def test_managed_envelope_lane_preflight_denies_before_mutation(target: Any, change: str) -> None:
    """Decline unsupported domains, marker intervals and device bindings before writes."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    lane = {
        "parameter": {"kind": "panning"},
        "clip_end": 4.0,
        "interpolation": "step",
        "points": [{"time": 0.0, "value": 0.0}],
    }
    if change == "domain":
        lane["points"][0]["value"] = 2.0
    elif change == "end":
        lane["clip_end"] = 5.0
    else:
        lane["parameter"] = {"kind": "device", "device_index": 0, "parameter_name": "Dry/Wet"}
    request = {
        "document_token": target.context["document_token"],
        "operation_id": "envelope",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": acknowledgement["result"]["content_fingerprint"],
        "lane": lane,
    }
    if change == "device":
        assert not _request_allowed("call", "song", "sunny_managed_author_envelope", [request])
    else:
        result = call(target, "sunny_managed_author_envelope", request)
        assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert not target.live.song.tracks[1].clip_slots[0].clip.has_envelopes


@pytest.mark.parametrize(
    "field,value",
    [
        ("clip_end", True),
        ("clip_end", 0.0),
        ("clip_end", float("inf")),
        ("signature_numerator", True),
        ("signature_denominator", 3),
        ("operation_id", ""),
        ("project_key", "project/name"),
        ("binding_key", "a" * 65),
        ("foreign_track", 0),
    ],
)
def test_managed_algebra_rejects_invalid_closed_payload_without_track_creation(
    target: Any, field: str, value: Any
) -> None:
    """Reject malformed managed requests before native object creation."""
    request = intent(target)
    request[field] = value
    assert not _request_allowed("call", "song", "sunny_managed_create_clip", [request])
    assert len(target.live.song.tracks) == 1


def test_partial_track_query_preserves_actual_known_identity_without_claiming_clip(
    target: Any, monkeypatch: Any
) -> None:
    """Report a moved partial Track identity without fabricating Clip ownership."""
    original = Song.create_midi_track

    def create(song: Any, index: int) -> None:
        original(song, index)
        raise RuntimeError("Injected failure after Track create")

    monkeypatch.setattr(Song, "create_midi_track", create)
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    assert acknowledgement["outcome"] == "indeterminate"
    partial = observe(target)
    assert partial == {
        "outcome": "partial_binding",
        "ownership_retained": False,
        "native_handles_retained": True,
        "known_track_index": 1,
        "recovery_available": False,
    }
    target.live.song._tracks.reverse()
    assert observe(target)["known_track_index"] == 0


def test_unobservable_user_edits_cannot_make_destructive_replacement_or_rebind_safe(
    target: Any,
) -> None:
    """Preserve edits outside the finite structural manifest when all observed fields match."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    clip = target.live.song.tracks[1].clip_slots[0].clip
    # These model-only values stand for native fields this adapter cannot read.
    # Equal finite fields never prove that these user edits are absent.
    clip._user_mpe_expression = {"pressure": [0.0, 0.9]}
    clip._user_follow_actions = {"next": True}
    current = observe(target)
    assert current["content_fingerprint"] == acknowledgement["result"]["content_fingerprint"]
    assert current["structural_boundary_complete"] is True
    assert current["content_boundary_complete"] is False
    result = call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert (
        "MpeExpressionUnavailable" in result["error"]
        and "FollowActionsUnavailable" in result["error"]
    )
    fresh = restart(target)
    recovered = call(fresh, "sunny_managed_rebind", rebind(fresh, acknowledgement))
    assert recovered["outcome"] == "declined" and recovered["native_mutation_started"] is False
    assert clip._user_mpe_expression == {"pressure": [0.0, 0.9]}
    assert clip._user_follow_actions == {"next": True}
    assert target.live.song.tracks[1].clip_slots[0].clip is clip


def test_new_empty_scenes_shift_slot_locator_without_transferring_ownership(target: Any) -> None:
    """Resolve the same native Slot after unrelated Scene insertion."""
    call(target, "sunny_managed_create_clip", intent(target))
    slot = target.live.song.tracks[1].clip_slots[0]
    target.live.song.create_scene(0)
    actual = observe(target)
    assert actual["slot_index"] == 1
    assert target.live.song.tracks[1].clip_slots[1] is slot
    assert actual["structural_boundary_complete"] is True


def test_journal_limit_never_evicts_an_old_token_or_starts_new_mutation(target: Any) -> None:
    """Reject excess new operations while retaining an old exact-token acknowledgement."""
    request = intent(target)
    acknowledgement = call(target, "sunny_managed_create_clip", request)
    target.registry._operations.update({f"reserved_{i}": {} for i in range(4095)})
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_managed_create_clip",
            "args": [intent(target, "new")],
        }
    )
    assert not response["success"] and "journal is full" in response["error"]
    assert len(target.live.song.tracks) == 2
    assert call(target, "sunny_managed_create_clip", request) == acknowledgement
