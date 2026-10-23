"""Actual ordinary authorizer against explicit native source-contract fixtures.

The notes listener fixture does not establish MPE/Follow Actions or real Live
host event coverage. No test here constitutes machine qualification.
"""

from __future__ import annotations

import copy
import threading
from types import SimpleNamespace

import pytest
from live_model import Clip, LiveSet
from Sunny.handler import BRIDGE_PROTOCOL_VERSION, LomHandler
from Sunny.managed import ManagedRegistry, _digest


@pytest.fixture
def target(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    """Attach the actual registry to explicit modeled note-event callbacks."""
    live = LiveSet(midi_tracks=2, return_tracks=1).install(monkeypatch)

    def add_listener(self, callback):
        self.__dict__.setdefault("_ordinary_listeners", []).append(callback)

    def has_listener(self, callback):
        return callback in self.__dict__.get("_ordinary_listeners", [])

    def remove_listener(self, callback):
        self.__dict__["_ordinary_listeners"].remove(callback)

    monkeypatch.setattr(Clip, "add_notes_listener", add_listener, raising=False)
    monkeypatch.setattr(Clip, "notes_has_listener", has_listener, raising=False)
    monkeypatch.setattr(Clip, "remove_notes_listener", remove_listener, raising=False)
    surface = live.surface
    registry = ManagedRegistry(surface)
    handler = LomHandler(surface, managed_registry=registry)
    registry.attach_handler(handler)
    target = SimpleNamespace(live=live, surface=surface, registry=registry, handler=handler)
    target.context = call(target, "sunny_managed_context")
    return target


def call(target, name, payload=None):
    """Require a valid outer envelope from the actual closed handler."""
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


def intent(target, number=1, action="create", payload=None):
    """Make one immutable musical intent with deterministic original authority."""
    if payload is None:
        payload = {
            "track_index": 0,
            "slot_index": 0,
            "clip_end": 4.0,
            "notes": [
                {
                    "pitch": 60,
                    "start_time": 0.0,
                    "duration": 1.0,
                    "velocity": 100,
                    "mute": False,
                    "probability": 1.0,
                    "velocity_deviation": 0.0,
                    "release_velocity": 64.0,
                }
            ],
        }
    return {
        **target.context,
        "operation_id": f"{number:032x}",
        "action": action,
        "payload": payload,
    }


def token(request):
    """Name exactly the original native operation for execution or queries."""
    return {
        **{key: value for key, value in request.items() if key not in ("action", "payload")},
        "fingerprint": _digest(request),
    }


def execute(target, request):
    """Prepare once, then execute once if native authority was retained."""
    prepared = call(target, "sunny_ordinary_prepare", request)
    if prepared["outcome"] == "prepared":
        return call(target, "sunny_ordinary_execute", token(request))
    return prepared


def test_create_undo_redo_retains_identity_and_looping(target):
    """Preserve valid ordinary looping Clips across typed inverse generations."""
    created = execute(target, intent(target))
    assert created["outcome"] == "acknowledged"
    slot = target.live.song.tracks[0].clip_slots[0]
    assert slot.clip.looping
    undo = intent(
        target, 2, "undo", {key: created["result"][key] for key in ("binding_token", "generation")}
    )
    undone = execute(target, undo)
    assert undone["outcome"] == "acknowledged" and not slot.has_clip
    redo = intent(
        target, 3, "redo", {key: undone["result"][key] for key in ("binding_token", "generation")}
    )
    redone = execute(target, redo)
    assert redone["outcome"] == "acknowledged" and slot.has_clip
    assert redone["result"]["generation"] == 2
    assert len(slot.clip.get_all_notes_extended()) == 1


def test_queued_execution_after_set_replacement_does_not_mutate(target):
    """Refuse stale originating Set authority before any replacement setter."""
    request = intent(target)
    assert call(target, "sunny_ordinary_prepare", request)["outcome"] == "prepared"
    target.surface._song = LiveSet(midi_tracks=2, return_tracks=1).song
    outcome = call(target, "sunny_ordinary_execute", token(request))
    assert outcome["outcome"] == "unknown_epoch"
    assert not target.surface.song().tracks[0].clip_slots[0].has_clip


def test_reorder_uses_original_track_instead_of_new_index(target):
    """Keep the exact Track through reorder without mutating its new index peer."""
    request = intent(target)
    original = target.live.song.tracks[0]
    assert call(target, "sunny_ordinary_prepare", request)["outcome"] == "prepared"
    target.live.song._tracks.reverse()
    assert call(target, "sunny_ordinary_execute", token(request))["outcome"] == "acknowledged"
    assert original.clip_slots[0].has_clip
    assert not target.live.song.tracks[0].clip_slots[0].has_clip


@pytest.mark.parametrize(
    "change", ["replacement", "notes", "control", "listener", "notes_reverted", "outside_notes"]
)
def test_undo_preserves_substitution_and_user_content(target, change):
    """Preserve substitute Clips and changed content when destructive authority expires."""
    created = execute(target, intent(target))
    assert created["outcome"] == "acknowledged"
    slot = target.live.song.tracks[0].clip_slots[0]
    clip = slot.clip
    if change == "replacement":
        slot.delete_clip()
        slot.create_clip(4.0)
    elif change == "notes":
        clip._notes.clear()
    elif change == "control":
        clip.name = "User title"
    elif change == "listener":
        clip._ordinary_listeners.clear()
    elif change == "outside_notes":
        note = copy.deepcopy(intent(target)["payload"]["notes"][0])
        note["start_time"] = 32.0
        target.handler._add_new_notes(clip, [note])
    else:
        for callback in clip._ordinary_listeners:
            callback()
    current = slot.clip
    undo = intent(
        target, 2, "undo", {key: created["result"][key] for key in ("binding_token", "generation")}
    )
    outcome = execute(target, undo)
    assert outcome["outcome"] == "declined" and not outcome["native_mutation_started"]
    assert slot.clip is current


def test_prepared_undo_rechecks_content_before_delete(target):
    """Refuse edits made between preparation and queued native execution."""
    created = execute(target, intent(target))
    undo = intent(
        target, 2, "undo", {key: created["result"][key] for key in ("binding_token", "generation")}
    )
    assert call(target, "sunny_ordinary_prepare", undo)["outcome"] == "prepared"
    slot = target.live.song.tracks[0].clip_slots[0]
    slot.clip.name = "Later edit"
    assert call(target, "sunny_ordinary_execute", token(undo))["outcome"] == "declined"
    assert slot.has_clip


def test_query_and_duplicate_execute_never_repeat_setters(target, monkeypatch):
    """Serve retained terminal evidence without replaying native mutations."""
    request = intent(target)
    result = execute(target, request)
    assert result["outcome"] == "acknowledged"
    slot = target.live.song.tracks[0].clip_slots[0]
    monkeypatch.setattr(type(slot), "create_clip", lambda *args: pytest.fail("repeated mutation"))
    assert call(target, "sunny_ordinary_operation", token(request)) == result
    assert call(target, "sunny_ordinary_execute", token(request)) == result


def test_native_partial_failure_retains_clip_and_original_journal(target, monkeypatch):
    """Keep a partial Clip and exact phase evidence instead of compensating."""
    monkeypatch.setattr(
        Clip, "add_new_notes", lambda *args: (_ for _ in ()).throw(RuntimeError("lost host return"))
    )
    request = intent(target)
    result = execute(target, request)
    assert (
        result["outcome"] == "partial"
        and result["started_calls"] == 2
        and result["returned_calls"] == 1
    )
    assert target.live.song.tracks[0].clip_slots[0].has_clip
    assert call(target, "sunny_ordinary_operation", token(request)) == result


def test_empty_rhythm_creates_empty_clip(target):
    """Allow a valid zero-pulse rhythm to create its empty looping Clip."""
    request = intent(target)
    request["payload"]["notes"] = []
    assert execute(target, request)["outcome"] == "acknowledged"


def test_shared_registry_refuses_socket_thread_before_document_access(target, monkeypatch):
    """Enforce main-thread authority before the first native document getter."""
    touched = []
    monkeypatch.setattr(target.surface, "song", lambda: touched.append(True))
    responses = []
    thread = threading.Thread(
        target=lambda: responses.append(
            target.handler.handle(
                {
                    "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
                    "type": "call",
                    "path": "song",
                    "name": "sunny_ordinary_prepare",
                    "args": [intent(target)],
                }
            )
        )
    )
    thread.start()
    thread.join()
    assert not touched and not responses[0]["success"]


def test_changed_intent_cannot_reuse_original_token(target):
    """Prevent the original operation ID from being rebound to a different target."""
    request = intent(target)
    assert call(target, "sunny_ordinary_prepare", request)["outcome"] == "prepared"
    changed = copy.deepcopy(request)
    changed["payload"]["slot_index"] = 1
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_ordinary_prepare",
            "args": [changed],
        }
    )
    assert not response["success"]


def test_journal_family_collision_never_exposes_native_handles(target):
    """Keep ordinary retained handles out of the legacy managed journal response."""
    request = intent(target)
    assert call(target, "sunny_ordinary_prepare", request)["outcome"] == "prepared"
    response = target.handler.handle(
        {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "type": "call",
            "path": "song",
            "name": "sunny_managed_operation",
            "args": [
                {
                    "document_token": target.context["document_token"],
                    "operation_id": request["operation_id"],
                }
            ],
        }
    )
    assert not response["success"] and "journal family" in response["error"]


def test_native_generation_and_empty_slot_are_rechecked_on_redo(target):
    """Preserve user replacement work placed in an undone native ClipSlot."""
    created = execute(target, intent(target))
    undo = intent(
        target, 2, "undo", {key: created["result"][key] for key in ("binding_token", "generation")}
    )
    undone = execute(target, undo)
    assert undone["outcome"] == "acknowledged"
    redo = intent(
        target, 3, "redo", {key: undone["result"][key] for key in ("binding_token", "generation")}
    )
    assert call(target, "sunny_ordinary_prepare", redo)["outcome"] == "prepared"
    slot = target.live.song.tracks[0].clip_slots[0]
    slot.create_clip(8.0)
    replacement = slot.clip
    assert call(target, "sunny_ordinary_execute", token(redo))["outcome"] == "declined"
    assert slot.clip is replacement
