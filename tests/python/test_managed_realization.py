"""Retained managed identities and fault outcomes, without a running Live host.

Explicit source-contract fixtures do not establish saved/reopened host behavior.
"""

from __future__ import annotations

import copy
import threading
from types import SimpleNamespace
from typing import Any

import pytest
from live_model import Clip, ClipSlot, LiveSet, MidiNoteVector, Song, inject
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler, _request_allowed
from Sunny.managed import ManagedRegistry, _canonical_bytes, _digest, _proposed_notes


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
    assert observe(target)["observation"]["manifest"]["notes"][0]["pitch"] == 60


def test_created_native_identities_survive_track_reordering_and_unknown_fields_block_deletion(
    target: Any,
) -> None:
    """Resolve moved native identities and decline deletion of unobserved content."""
    acknowledgement = call(target, "sunny_managed_create_clip", intent(target))
    owned = target.live.song.tracks[1]
    old_clip = owned.clip_slots[0].clip
    user = target.live.song.tracks[0]
    target.live.song._tracks.reverse()
    assert observe(target)["observation"]["track_index"] == 0
    replaced = call(target, "sunny_managed_replace_clip", replace(target, acknowledgement))
    assert replaced["outcome"] == "declined", replaced
    assert "MpeExpressionUnavailable" in replaced["error"]
    assert "FollowActionsUnavailable" in replaced["error"]
    assert owned.clip_slots[0].clip is old_clip
    assert observe(target)["observation"]["manifest"]["notes"][0]["pitch"] == 60
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
        "schema_version": 1,
        "context": {
            "bridge_instance": target.context["bridge_instance"],
            "document_token": target.context["document_token"],
        },
        "project_key": "project_a",
        "binding_key": "part_a",
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
    current = observe(target)["observation"]
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
    actual = observe(target)["observation"]
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


@pytest.fixture
def note_target(target: Any, monkeypatch: pytest.MonkeyPatch) -> Any:
    """Model existing-object snapshots independently of the global Live fixture.

    The explicit apply boundary preserves opaque model sentinels. This checks
    adapter field selection; it does not establish native MPE geometry behavior.
    """
    target.created = call(target, "sunny_managed_create_clip", intent(target))
    target.clip = target.live.song.tracks[1].clip_slots[0].clip
    target.setter_calls = []
    target.apply_calls = []
    target.snapshots = set()
    target.clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
    target.clip._user_follow_actions = {"next": True}

    class SnapshotNote:
        note_id: int

        def __init__(self, native: Any) -> None:
            for field in (
                "note_id",
                "pitch",
                "start_time",
                "duration",
                "velocity",
                "mute",
                "probability",
                "velocity_deviation",
                "release_velocity",
            ):
                object.__setattr__(self, field, getattr(native, field))
            target.snapshots.add(id(self))

        def __setattr__(self, name: str, value: Any) -> None:
            target.setter_calls.append((self.note_id, name, value))
            object.__setattr__(self, name, value)

    def select(ids: Any) -> MidiNoteVector:
        return MidiNoteVector(
            SnapshotNote(target.clip._notes[i]) for i in ids if i in target.clip._notes
        )

    def apply(notes: Any) -> None:
        assert isinstance(notes, MidiNoteVector)
        assert all(id(note) in target.snapshots for note in notes)
        target.apply_calls.append(tuple(note.note_id for note in notes))
        for note in notes:
            native = target.clip._notes[note.note_id]
            for field in (
                "pitch",
                "start_time",
                "duration",
                "velocity",
                "mute",
                "probability",
                "velocity_deviation",
                "release_velocity",
            ):
                setattr(native, field, getattr(note, field))

    monkeypatch.setattr(target.clip, "get_notes_by_id", select)
    monkeypatch.setattr(target.clip, "apply_note_modifications", apply, raising=False)
    return target


def note_update(target: Any, updates: dict[str, Any], operation: str = "update") -> dict[str, Any]:
    """Construct one exact actual-ID intent from the initial acknowledged binding."""
    actual = target.created["result"]["note_identity"]["notes"][0]
    return {
        "document_token": target.context["document_token"],
        "operation_id": operation,
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": target.created["result"]["content_fingerprint"],
        "changes": [
            {
                "note_id": actual["note_id"],
                "expected": {name: value for name, value in actual.items() if name != "note_id"},
                "updates": updates,
            }
        ],
    }


def test_observation_closes_context_tags_and_independent_note_identity(target: Any) -> None:
    """Read actual IDs independently while keeping semantic population fingerprints stable."""
    created = call(target, "sunny_managed_create_clip", intent(target))
    result = observe(target)
    assert set(result) == {
        "schema_version",
        "context",
        "project_key",
        "binding_key",
        "outcome",
        "ownership_retained",
        "observation",
    }
    assert result["context"] == {
        name: target.context[name] for name in ("bridge_instance", "document_token")
    }
    actual = result["observation"]
    assert actual["track_tag"] == "Sunny|project_a|part_a|track"
    assert actual["clip_tag"] == "Sunny|project_a|part_a|clip"
    assert [note["note_id"] for note in actual["note_identity"]["notes"]] == [1, 2]
    assert actual["content_fingerprint"] == created["result"]["content_fingerprint"]
    assert actual["note_identity_fingerprint"] == _digest(actual["note_identity"])
    assert actual["content_boundary_complete"] is False


def test_note_update_existing_snapshots_preserves_ids_opaque_fields_and_unrelated_notes(
    note_target: Any,
) -> None:
    """Edit all six admitted fields once, preserving unspecified native note fields."""
    target = note_target
    request = note_update(
        target,
        {
            "pitch": 61,
            "start_time": 0.25,
            "duration": 1.5,
            "velocity": 80.5,
            "mute": True,
            "release_velocity": 45.25,
        },
    )
    clip, slot = target.clip, target.live.song.tracks[1].clip_slots[0]
    other = copy.deepcopy(target.created["result"]["note_identity"]["notes"][1])
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "acknowledged" and result["native_mutation_started"] is True
    assert target.apply_calls == [(1,)] and len(target.setter_calls) == 6
    evidence = result["result"]
    first, second = evidence["note_identity"]["notes"]
    assert first == {
        "note_id": 1,
        "pitch": 61,
        "start_time": 0.25,
        "duration": 1.5,
        "velocity": 80.5,
        "mute": True,
        "release_velocity": 45.25,
        "probability": 1.0,
        "velocity_deviation": 0.0,
    }
    assert second == other
    assert all(
        evidence["note_update"][flag] is True
        for flag in (
            "observed_updates_match_request",
            "untouched_notes_preserved",
            "note_ids_preserved",
        )
    )
    assert evidence["note_update"]["before_manifest"] == target.created["result"]["manifest"]
    assert slot.clip is clip
    assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
    assert clip._user_follow_actions == {"next": True}
    assert evidence["manifest"]["mpe_note_expression_state_observed"] is False
    assert evidence["manifest"]["follow_actions_state_observed"] is False
    assert evidence["content_boundary_complete"] is False
    assert call(target, "sunny_managed_update_notes", request) == result
    assert target.apply_calls == [(1,)]  # Lost reply replay never applies twice.


@pytest.mark.parametrize(
    "updates",
    [
        {"pitch": 67, "start_time": 2.0},
        {"pitch": 67, "start_time": 1.5, "duration": 1.0},
        {"pitch": 67, "start_time": 2.25},
        {"start_time": 4.0},
        {"duration": 1e308, "start_time": 1e308},
    ],
)
def test_note_geometry_collision_or_marker_domain_declines_before_setter(
    note_target: Any,
    updates: dict[str, Any],
) -> None:
    """Literal same-pitch intersections and out-of-marker starts perform zero setters."""
    result = call(note_target, "sunny_managed_update_notes", note_update(note_target, updates))
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert not note_target.setter_calls and not note_target.apply_calls
    assert (
        observe(note_target)["observation"]["note_identity"]
        == note_target.created["result"]["note_identity"]
    )


def test_same_pitch_start_swap_declines_before_native_apply_until_batch_atomicity_is_qualified(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Final disjoint geometry alone does not admit temporarily covered retained notes."""
    create = intent(target)
    create["notes"][1]["pitch"] = 60
    create["notes"][1]["duration"] = 1.0
    created = call(target, "sunny_managed_create_clip", create)
    assert created["outcome"] == "acknowledged"
    clip = target.live.song.tracks[1].clip_slots[0].clip
    calls = []
    monkeypatch.setattr(clip, "get_notes_by_id", lambda ids: calls.append("select"))
    monkeypatch.setattr(
        clip, "apply_note_modifications", lambda notes: calls.append("apply"), raising=False
    )
    before = copy.deepcopy(created["result"]["note_identity"])
    changes = [
        {
            "note_id": note["note_id"],
            "expected": {name: value for name, value in note.items() if name != "note_id"},
            "updates": {"start_time": 2.0 if note["note_id"] == 1 else 0.0},
        }
        for note in before["notes"]
    ]
    result = call(
        target,
        "sunny_managed_update_notes",
        {
            "document_token": target.context["document_token"],
            "operation_id": "swap",
            "project_key": "project_a",
            "binding_key": "part_a",
            "expected_content_fingerprint": created["result"]["content_fingerprint"],
            "changes": changes,
        },
    )
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert "IntermediateCollisionUnavailable" in result["error"]
    assert not calls and observe(target)["observation"]["note_identity"] == before


def test_disjoint_transposition_retains_native_ids_and_unknown_fields(note_target: Any) -> None:
    """Ordinary non-colliding pitch changes pass both final and baseline geometry guards."""
    target = note_target
    request = note_update(target, {"pitch": 62})
    second = target.created["result"]["note_identity"]["notes"][1]
    request["changes"].append(
        {
            "note_id": 2,
            "expected": {name: value for name, value in second.items() if name != "note_id"},
            "updates": {"pitch": 69},
        }
    )
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "acknowledged", result
    assert [(note.note_id, note.pitch) for note in target.clip._notes.values()] == [
        (1, 62),
        (2, 69),
    ]
    assert target.apply_calls == [(1, 2)]
    assert target.clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
    assert target.clip._user_follow_actions == {"next": True}


@pytest.mark.parametrize("start,duration", [(1.0, 1.0), (2.5, 0.25)])
def test_note_geometry_halfopen_adjacency_is_admitted(
    note_target: Any,
    start: float,
    duration: float,
) -> None:
    """Notes ending at another's start or starting at its end do not overlap."""
    result = call(
        note_target,
        "sunny_managed_update_notes",
        note_update(note_target, {"pitch": 67, "start_time": start, "duration": duration}),
    )
    assert result["outcome"] == "acknowledged" and note_target.apply_calls == [(1,)]


def test_batch_collision_checks_other_changed_notes_before_setter(
    note_target: Any,
) -> None:
    """Individually free proposed notes must also be disjoint from each other."""
    request = note_update(note_target, {"pitch": 62, "start_time": 1.0, "duration": 1.0})
    second = note_target.created["result"]["note_identity"]["notes"][1]
    request["changes"].append(
        {
            "note_id": 2,
            "expected": {k: v for k, v in second.items() if k != "note_id"},
            "updates": {"pitch": 62, "start_time": 1.5},
        }
    )
    result = call(note_target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "declined" and "collision" in result["error"]
    assert not note_target.setter_calls and not note_target.apply_calls


@pytest.mark.parametrize(
    "updates",
    [
        {},
        {"pitch": True},
        {"pitch": 128},
        {"start_time": -0.25},
        {"duration": 0.0},
        {"duration": float("inf")},
        {"velocity": 128.0},
        {"mute": 1},
        {"release_velocity": -1.0},
        {"probability": 0.5},
        {"velocity_deviation": 2.0},
        {"note_id": 2},
        {"pressure": 0.5},
    ],
)
def test_note_update_closed_values_rejected_before_token_reservation(
    note_target: Any,
    updates: dict[str, Any],
) -> None:
    """Unsupported field families and malformed domains never reach native objects."""
    request = note_update(note_target, updates)
    assert not _request_allowed("call", "song", "sunny_managed_update_notes", [request])
    assert "update" not in note_target.registry._operations
    assert not note_target.setter_calls and not note_target.apply_calls


def test_recreated_note_identity_and_inspection_cannot_refresh_guard(
    note_target: Any,
) -> None:
    """Equal values with a different native identity cannot inherit note ownership."""
    target = note_target
    clip = target.clip
    previous = target.created["result"]
    clip._notes[5] = clip._notes.pop(1)
    clip._notes[5].note_id = 5
    actual = observe(target)["observation"]
    assert actual["content_fingerprint"] == previous["content_fingerprint"]
    assert actual["note_identity_fingerprint"] != previous["note_identity_fingerprint"]
    request = note_update(target, {"pitch": 61})
    request["changes"][0]["note_id"] = 5
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "declined" and "drift" in result["error"]
    assert not target.setter_calls and not target.apply_calls


def test_inspection_of_semantic_drift_does_not_authorize_new_guard(note_target: Any) -> None:
    """Read-only actual evidence can inform a plan but cannot accept a user edit."""
    target = note_target
    target.clip._notes[1].velocity = 81.0
    actual = observe(target)["observation"]
    request = note_update(target, {"pitch": 61})
    request["expected_content_fingerprint"] = actual["content_fingerprint"]
    request["changes"][0]["expected"]["velocity"] = 81.0
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "declined" and not target.setter_calls


def test_note_update_retains_actual_silent_mismatch_without_echo_or_retry(
    note_target: Any,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A returned native call can fail the independently observed value comparison."""
    monkeypatch.setattr(note_target.clip, "apply_note_modifications", lambda notes: None)
    request = note_update(note_target, {"pitch": 61})
    result = call(note_target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "acknowledged"
    assert result["result"]["note_identity"]["notes"][0]["pitch"] == 60
    assert result["result"]["note_update"]["observed_updates_match_request"] is False
    assert result["result"]["note_update"]["note_ids_preserved"] is True
    assert call(note_target, "sunny_managed_update_notes", request) == result
    assert len(note_target.setter_calls) == 1


def test_note_update_throw_after_apply_is_uncertain_and_never_reapplied(
    note_target: Any,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """A lost/failed outcome retains its token and actual partial native objects."""
    target = note_target
    apply = target.clip.apply_note_modifications

    def fail(notes: Any) -> None:
        apply(notes)
        raise RuntimeError("host failed after native apply")

    monkeypatch.setattr(target.clip, "apply_note_modifications", fail)
    request = note_update(target, {"pitch": 61})
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "indeterminate" and result["native_mutation_started"] is True
    assert observe(target)["observation"]["note_identity"]["notes"][0]["pitch"] == 61
    assert call(target, "sunny_managed_update_notes", request) == result
    assert target.apply_calls == [(1,)]


def test_legacy_note_update_requires_full_population_before_setter(
    note_target: Any,
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Live11.0 ranged readback cannot establish an all-note collision boundary."""
    target = note_target
    target.clip._live_version = (11, 0)
    record = target.registry._bindings[("project_a", "part_a")]
    # A legitimate legacy creation seals its finite ranged evidence, not a
    # substituted full-population query. The original insertion IDs are retained.
    baseline = target.registry._seal(record)
    request = note_update(target, {"velocity": 80.0})
    request["expected_content_fingerprint"] = baseline["content_fingerprint"]
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "declined" and "NotePopulationUnavailable" in result["error"]
    assert not target.setter_calls and not target.apply_calls


def test_additional_absent_lane_preserves_existing_envelope_and_opaque_note_fields(
    note_target: Any,
) -> None:
    """An absent parameter lane can be added without replacing an existing lane."""
    target = note_target
    clip, track = target.clip, target.live.song.tracks[1]
    clip._user_mpe_expression = {1: {"pressure": [0.1, 0.8]}}
    clip._user_follow_actions = {"next": True}
    before_ids = set(clip._notes)
    receipt = target.created
    lanes = {}
    for kind, first, second in (("panning", -0.5, 0.5), ("volume", 0.25, 0.75)):
        request = {
            "document_token": target.context["document_token"],
            "operation_id": f"envelope_{kind}",
            "project_key": "project_a",
            "binding_key": "part_a",
            "expected_content_fingerprint": receipt["result"]["content_fingerprint"],
            "lane": {
                "parameter": {"kind": kind},
                "clip_end": 4.0,
                "interpolation": "step",
                "points": [{"time": 0.0, "value": first}, {"time": 2.0, "value": second}],
            },
        }
        receipt = call(target, "sunny_managed_author_envelope", request)
        assert receipt["outcome"] == "acknowledged", receipt
        assert receipt["result"]["acknowledgement"]["action"] == "created"
        assert receipt["result"]["content_boundary_complete"] is False
        assert receipt["result"]["structural_boundary_complete"] is False
        lanes[kind] = clip.automation_envelope(getattr(track.mixer_device, kind))
    assert set(clip._notes) == before_ids and not target.setter_calls and not target.apply_calls
    assert clip._user_mpe_expression == {1: {"pressure": [0.1, 0.8]}}
    assert clip._user_follow_actions == {"next": True}
    assert clip.automation_envelope(track.mixer_device.panning) is lanes["panning"]
    assert [lanes["panning"].value_at_time(time) for time in (0.0, 1.0, 2.0, 3.0)] == [
        -0.5,
        -0.5,
        0.5,
        0.5,
    ]
    request["operation_id"] = "reject_existing_volume"
    request["expected_content_fingerprint"] = receipt["result"]["content_fingerprint"]
    request["lane"]["points"][0]["value"] = 0.5
    steps = copy.deepcopy(lanes["volume"]._steps)
    declined = call(target, "sunny_managed_author_envelope", request)
    assert declined["outcome"] == "declined" and declined["native_mutation_started"] is False
    assert "EnvelopeRevisionUnavailable" in declined["error"]
    assert lanes["volume"]._steps == steps


def test_managed_envelope_sampling_resolves_retained_objects_after_track_and_scene_reorder(
    target: Any,
) -> None:
    """Read actual parameter values on the retained Clip without a stale indexed RPC."""
    created = call(target, "sunny_managed_create_clip", intent(target))
    track = target.live.song.tracks[1]
    clip = track.clip_slots[0].clip
    request = {
        "document_token": target.context["document_token"],
        "operation_id": "pan_lane",
        "project_key": "project_a",
        "binding_key": "part_a",
        "expected_content_fingerprint": created["result"]["content_fingerprint"],
        "lane": {
            "parameter": {"kind": "panning"},
            "clip_end": 4.0,
            "interpolation": "step",
            "points": [{"time": 0.0, "value": -0.5}, {"time": 2.0, "value": 0.5}],
        },
    }
    authored = call(target, "sunny_managed_author_envelope", request)
    assert authored["outcome"] == "acknowledged"
    target.live.song.create_midi_track(0)
    target.live.song.create_scene(0)
    target.live.song.tracks[1].clip_slots[1].create_clip(4.0)
    wrong_clip = target.live.song.tracks[1].clip_slots[1].clip
    wrong_clip.create_automation_envelope(
        target.live.song.tracks[1].mixer_device.panning
    ).insert_step(0.0, 4.0, 0.9)
    query = {
        "document_token": target.context["document_token"],
        "project_key": "project_a",
        "binding_key": "part_a",
        "parameter": {"kind": "panning"},
        "sample_times": [0.0, 1.0, 2.0, 3.0],
    }
    journals = copy.deepcopy(target.registry._operations)
    guard = target.registry._bindings[("project_a", "part_a")]["content_fingerprint"]
    sampled = call(target, "sunny_managed_sample_envelope", query)
    assert sampled["outcome"] == "observed" and sampled["ownership_retained"] is True
    assert sampled["observation"]["track_index"] == 2
    assert sampled["observation"]["slot_index"] == 1
    assert track.clip_slots[1].clip is clip
    assert sampled["envelope"]["samples"] == [
        {"time": 0.0, "value": -0.5},
        {"time": 1.0, "value": -0.5},
        {"time": 2.0, "value": 0.5},
        {"time": 3.0, "value": 0.5},
    ]
    assert target.registry._operations == journals
    assert target.registry._bindings[("project_a", "part_a")]["content_fingerprint"] == guard
    clip._notes[1].velocity = 80.0
    drift = call(target, "sunny_managed_sample_envelope", query)
    assert drift["observation"]["content_fingerprint"] != guard
    request["operation_id"] = "no_adopt_after_sampling"
    request["expected_content_fingerprint"] = drift["observation"]["content_fingerprint"]
    request["lane"]["parameter"] = {"kind": "volume"}
    request["lane"]["points"] = [{"time": 0.0, "value": 0.25}]
    rejected = call(target, "sunny_managed_author_envelope", request)
    assert rejected["outcome"] == "declined" and rejected["native_mutation_started"] is False
    assert clip.automation_envelope(track.mixer_device.volume) is None
    query["binding_key"] = "unknown"
    missing = call(target, "sunny_managed_sample_envelope", query)
    assert missing["outcome"] == "recovery_unavailable" and "envelope" not in missing


def test_note_revision_preserves_sunny_owned_envelope_and_its_sample_values(
    note_target: Any,
) -> None:
    """Note-only revision does not require deleting or observing envelope breakpoints."""
    target = note_target
    lane = {
        "parameter": {"kind": "panning"},
        "clip_end": 4.0,
        "interpolation": "step",
        "points": [{"time": 0.0, "value": -0.5}, {"time": 2.0, "value": 0.25}],
    }
    target.created = call(
        target,
        "sunny_managed_author_envelope",
        {
            "document_token": target.context["document_token"],
            "operation_id": "envelope",
            "project_key": "project_a",
            "binding_key": "part_a",
            "lane": lane,
            "expected_content_fingerprint": target.created["result"]["content_fingerprint"],
        },
    )
    assert target.created["outcome"] == "acknowledged"
    parameter = target.live.song.tracks[1].mixer_device.panning
    envelope = target.clip.automation_envelope(parameter)
    result = call(target, "sunny_managed_update_notes", note_update(target, {"pitch": 61}))
    assert result["outcome"] == "acknowledged", result
    assert target.clip.automation_envelope(parameter) is envelope
    assert envelope.value_at_time(0.25) == -0.5 and envelope.value_at_time(3.0) == 0.25
    assert result["result"]["structural_boundary_complete"] is False
    assert result["result"]["content_boundary_complete"] is False


@pytest.mark.parametrize(
    "failure", ["armed", "playing", "replaced_clip", "bad_expected", "missing_id"]
)
def test_note_revision_ownership_and_idle_guards_precede_setters(
    note_target: Any,
    monkeypatch: pytest.MonkeyPatch,
    failure: str,
) -> None:
    """Decline changed native identity, selected values or active recording state."""
    target = note_target
    request = note_update(target, {"velocity": 80.0})
    if failure == "armed":
        target.live.song.tracks[1].arm = True
    elif failure == "playing":
        monkeypatch.setattr(Clip, "is_playing", property(lambda self: True))
    elif failure == "replaced_clip":
        slot = target.live.song.tracks[1].clip_slots[0]
        slot.delete_clip()
        slot.create_clip(4.0)
    elif failure == "bad_expected":
        request["changes"][0]["expected"]["velocity"] = 80.0
    else:
        monkeypatch.setattr(target.clip, "get_notes_by_id", lambda ids: MidiNoteVector([]))
    result = call(target, "sunny_managed_update_notes", request)
    assert result["outcome"] == "declined" and result["native_mutation_started"] is False
    assert not target.setter_calls and not target.apply_calls


def test_read_observation_unknown_epoch_has_actual_context_and_no_ownership(target: Any) -> None:
    """A changed document token cannot be mistaken for proof of missing content."""
    call(target, "sunny_managed_create_clip", intent(target))
    target.registry._document_token = "new_document"
    result = observe(target)
    assert result == {
        "schema_version": 1,
        "context": {
            "bridge_instance": target.context["bridge_instance"],
            "document_token": "new_document",
        },
        "project_key": "project_a",
        "binding_key": "part_a",
        "outcome": "unknown_epoch",
        "ownership_retained": False,
    }


def test_untouched_note_finite_fields_do_not_prove_finite_geometry(note_target: Any) -> None:
    """A literal overflow outside markers makes geometry unavailable before any setters."""
    identity = copy.deepcopy(note_target.created["result"]["note_identity"])
    # This is a finite observation-domain counterexample, not a claim that a
    # Live host admits this range or that the fake can qualify its availability.
    identity["notes"][1]["start_time"] = 1e308
    identity["notes"][1]["duration"] = 1e308
    changes = note_update(note_target, {"pitch": 61})["changes"]
    with pytest.raises(RuntimeError, match="NoteGeometryUnavailable"):
        _proposed_notes(identity, changes, 4.0)
    assert not note_target.setter_calls and not note_target.apply_calls
    # An existing untouched invalid endpoint does not require changing that
    # native note to revise a different note's velocity only.
    values = note_update(note_target, {"velocity": 81.0})["changes"]
    assert _proposed_notes(identity, values, 4.0)[1]["velocity"] == 81.0
