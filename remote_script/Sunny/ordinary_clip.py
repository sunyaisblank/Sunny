"""Finite ordinary Clip transactions, sharing the managed document epoch.

This retains native handles across reconnects. It never recovers authority from
indices, names, disk receipts or a new bridge instance. Other legacy mutations
and complete MPE/Follow Actions host qualification remain outside this stage.
"""

from __future__ import annotations

import copy
import uuid
from typing import Any

from .handler import _finite_number, _valid_note_dictionary
from .managed import _digest, _semantic_note
from .native_control import check_native_peer, native_call

CALLS = frozenset({"sunny_ordinary_prepare", "sunny_ordinary_execute", "sunny_ordinary_operation"})
_CONTEXT = {"schema_version", "bridge_instance", "document_token", "operation_id"}
_BOOLS = frozenset(
    {
        "looping",
        "muted",
        "has_envelopes",
        "has_groove",
        "is_session_clip",
        "is_arrangement_clip",
        "is_midi_clip",
        "is_audio_clip",
        "is_playing",
        "is_recording",
        "is_overdubbing",
        "is_triggered",
        "will_record_on_start",
        "legato",
    }
)
_PROPERTIES = _BOOLS | {
    "name",
    "signature_numerator",
    "signature_denominator",
    "start_marker",
    "end_marker",
    "loop_start",
    "loop_end",
    "launch_mode",
    "launch_quantization",
    "velocity_amount",
}


def _hex(value: Any, size: int) -> bool:
    return type(value) is str and len(value) == size and all(c in "0123456789abcdef" for c in value)


def valid_ordinary_request(name: str, args: list[Any]) -> bool:
    """Closed algebra shared with the native codec; no optional authority fields."""
    if name not in CALLS or len(args) != 1 or type(args[0]) is not dict:
        return False
    value = args[0]
    if (
        type(value.get("schema_version")) is not int
        or value["schema_version"] != 1
        or not all(
            _hex(value.get(key), 32)
            for key in ("bridge_instance", "document_token", "operation_id")
        )
    ):
        return False
    if name != "sunny_ordinary_prepare":
        return set(value) == _CONTEXT | {"fingerprint"} and _hex(value["fingerprint"], 64)
    if set(value) != _CONTEXT | {"action", "payload"} or type(value["payload"]) is not dict:
        return False
    payload = value["payload"]
    if value["action"] in ("undo", "redo"):
        return (
            set(payload) == {"binding_token", "generation"}
            and _hex(payload["binding_token"], 32)
            and type(payload["generation"]) is int
            and 0 < payload["generation"] <= 2147483647
        )
    if value["action"] != "create" or set(payload) != {
        "track_index",
        "slot_index",
        "clip_end",
        "notes",
    }:
        return False
    return (
        all(
            type(payload[key]) is int and 0 <= payload[key] <= 2147483647
            for key in ("track_index", "slot_index")
        )
        and type(payload["clip_end"]) is float
        and _finite_number(payload["clip_end"])
        and payload["clip_end"] > 0.0
        and type(payload["notes"]) is list
        and len(payload["notes"]) <= 65536
        and all(
            _valid_note_dictionary({"notes": [note]})
            and float(note["start_time"]) >= 0.0
            and float(note["start_time"]) + float(note["duration"]) <= payload["clip_end"]
            for note in payload["notes"]
        )
    )


class OrdinaryClips:
    """Own only bindings created by this helper in its current native epoch."""

    def __init__(self, owner: Any) -> None:
        self.owner = owner
        self.bindings: dict[str, dict[str, Any]] = {}

    def close(self) -> None:
        for binding in self.bindings.values():
            self._unlisten(binding)

    def _unlisten(self, binding: dict[str, Any]) -> None:
        clip, callback = binding.get("clip"), binding.get("listener")
        if clip is not None and callback is not None:
            try:
                if clip.notes_has_listener(callback):
                    clip.remove_notes_listener(callback)
            except Exception:
                # The old native document can already have invalidated objects.
                pass
        binding["listener"] = None

    def _epoch(self, request: dict[str, Any]) -> bool:
        return (
            request["bridge_instance"] == self.owner._bridge_instance
            and request["document_token"] == self.owner._document_token
        )

    @staticmethod
    def _journal(
        request: dict[str, Any], fingerprint: str, action: str, outcome: str
    ) -> dict[str, Any]:
        return {
            **{key: request[key] for key in _CONTEXT},
            "fingerprint": fingerprint,
            "action": action,
            "outcome": outcome,
            "native_mutation_started": False,
            "started_calls": 0,
            "returned_calls": 0,
            "result": None,
            "error": None,
        }

    def _membership(self, binding: dict[str, Any], occupied: bool) -> None:
        import Live

        same = self.owner._same
        song = self.owner._ensure_document()
        track, slot = binding["track"], binding["slot"]
        if (
            not same(song, binding["song"])
            or not isinstance(track, Live.Track.Track)
            or type(track.has_midi_input) is not bool
            or not track.has_midi_input
            or not isinstance(slot, Live.ClipSlot.ClipSlot)
            or not same(slot.canonical_parent, track)
            or sum(same(candidate, track) for candidate in song.tracks) != 1
            or sum(same(candidate, slot) for candidate in track.clip_slots) != 1
            or type(slot.has_clip) is not bool
            or slot.has_clip != occupied
        ):
            raise RuntimeError("Ordinary native Song/Track/ClipSlot identity changed")
        if occupied:
            clip = binding["clip"]
            if (
                not isinstance(clip, Live.Clip.Clip)
                or not same(slot.clip, clip)
                or not same(clip.canonical_parent, slot)
            ):
                raise RuntimeError("Ordinary native Clip identity changed")
        elif slot.clip is not None:
            raise RuntimeError("Ordinary ClipSlot is no longer empty")

    def _capture(self, binding: dict[str, Any]) -> dict[str, Any]:
        self._membership(binding, True)
        clip = binding["clip"]
        properties = {
            name: self.owner._handler._serialise(getattr(clip, name)) for name in _PROPERTIES
        }
        for name, value in properties.items():
            if name in _BOOLS:
                valid = type(value) is bool
            elif name == "name":
                valid = type(value) is str
            elif name in (
                "signature_numerator",
                "signature_denominator",
                "launch_mode",
                "launch_quantization",
            ):
                valid = type(value) is int
            else:
                valid = _finite_number(value)
            if not valid:
                raise RuntimeError("Ordinary Clip returned invalid control state")
        if (
            not properties["is_midi_clip"]
            or not properties["is_session_clip"]
            or properties["is_audio_clip"]
            or properties["is_arrangement_clip"]
            or any(
                properties[name]
                for name in (
                    "has_envelopes",
                    "has_groove",
                    "is_playing",
                    "is_recording",
                    "is_overdubbing",
                    "is_triggered",
                    "will_record_on_start",
                )
            )
        ):
            raise RuntimeError("Ordinary Clip is no longer within the idle MIDI content boundary")
        notes = self.owner._handler._midi_notes_dictionary(clip.get_all_notes_extended())["notes"]
        if len(notes) > 65536 or len({note["note_id"] for note in notes}) != len(notes):
            raise RuntimeError("Ordinary complete note population is invalid or excessive")
        return {"properties": properties, "notes": sorted(notes, key=lambda note: note["note_id"])}

    def _guard(self, binding: dict[str, Any], action: str) -> None:
        if action == "undo":
            if binding["state"] != "clip" or binding["dirty"]:
                raise RuntimeError("Ordinary Clip was edited or is no longer retained")
            if _digest(self._capture(binding)) != binding["content_fingerprint"]:
                raise RuntimeError("Ordinary Clip content changed; refusing destructive undo")
            callback = binding.get("listener")
            if callback is None or not binding["clip"].notes_has_listener(callback):
                raise RuntimeError("Ordinary Clip note-change guard is unavailable")
        else:
            if binding["state"] != "empty":
                raise RuntimeError("Ordinary redo binding is no longer empty")
            if binding["generation"] >= 2147483647:
                raise RuntimeError("Ordinary native generation capacity exhausted")
            self._membership(binding, False)

    def dispatch(self, name: str, request: dict[str, Any]) -> dict[str, Any]:
        action = request.get("action", "unknown")
        fingerprint = (
            _digest(request) if name == "sunny_ordinary_prepare" else request["fingerprint"]
        )
        if not self._epoch(request):
            return self._journal(request, fingerprint, action, "unknown_epoch")
        operation_id = request["operation_id"]
        existing = self.owner._operations.get(operation_id)
        if existing is not None and (
            existing.get("ordinary") is not True
            or existing["journal"]["fingerprint"] != fingerprint
        ):
            raise RuntimeError("Operation token is already retained for another immutable intent")
        if name == "sunny_ordinary_operation":
            return copy.deepcopy(
                existing["journal"]
                if existing
                else self._journal(request, fingerprint, action, "unknown_operation")
            )
        if name == "sunny_ordinary_execute":
            if existing is None:
                return self._journal(request, fingerprint, action, "unknown_operation")
            journal = existing["journal"]
            if journal["outcome"] != "prepared":
                return copy.deepcopy(journal)
            self._execute(existing)
            return copy.deepcopy(journal)
        if existing is not None:
            return copy.deepcopy(existing["journal"])
        if len(self.owner._operations) >= 4096:
            raise RuntimeError("Retained operation capacity exhausted")
        journal = self._journal(request, fingerprint, action, "prepared")
        record = {"ordinary": True, "request": copy.deepcopy(request), "journal": journal}
        # Reserve the shared token before any read which might throw; refusals
        # retain an immutable original intent and can never later execute.
        self.owner._operations[operation_id] = record
        try:
            payload = request["payload"]
            if action == "create":
                song = self.owner._ensure_document()
                track = song.tracks[payload["track_index"]]
                binding = {
                    "song": song,
                    "track": track,
                    "slot": track.clip_slots[payload["slot_index"]],
                    "clip": None,
                    "state": "empty",
                    "generation": 0,
                    "dirty": False,
                    "listener": None,
                    "projection": copy.deepcopy(payload),
                    "token": uuid.uuid4().hex,
                }
                self._membership(binding, False)
                record["binding"] = binding
            else:
                binding = self.bindings.get(payload["binding_token"])
                if binding is None or binding["generation"] != payload["generation"]:
                    raise RuntimeError("Ordinary binding or generation is unavailable")
                self._guard(binding, action)
                record["binding"] = binding
        except Exception as error:
            journal["outcome"], journal["error"] = "declined", str(error)
        return copy.deepcopy(journal)

    def _execute(self, record: dict[str, Any]) -> None:
        journal, binding, action = record["journal"], record["binding"], record["request"]["action"]

        def call(callback: Any, *args: Any) -> Any:
            check_native_peer()
            journal["native_mutation_started"] = True
            journal["started_calls"] += 1
            result = native_call(callback, *args)
            journal["returned_calls"] += 1
            return result

        try:
            self._guard(binding, "redo" if action == "create" else action)
            if action == "undo":
                call(binding["slot"].delete_clip)
                self._membership(binding, False)
                self._unlisten(binding)
                binding["clip"], binding["state"] = None, "empty"
            else:
                payload = binding["projection"]
                call(binding["slot"].create_clip, payload["clip_end"])
                # Keep a partial native handle even if later observation fails.
                binding["clip"] = binding["slot"].clip
                binding["state"] = "clip"
                self.bindings[binding["token"]] = binding
                self._membership(binding, True)
                if payload["notes"]:
                    call(self.owner._handler._add_new_notes, binding["clip"], payload["notes"])
                binding["generation"] += 1
                binding["dirty"] = False

                def changed() -> None:
                    binding["dirty"] = True

                binding["listener"] = changed
                binding["clip"].add_notes_listener(changed)
                if not binding["clip"].notes_has_listener(changed):
                    raise RuntimeError("Ordinary native note-change listener registration failed")
                captured = self._capture(binding)
                actual = [
                    {key: value for key, value in note.items() if key != "note_id"}
                    for note in captured["notes"]
                ]
                if sorted((_digest(_semantic_note(note)) for note in actual)) != sorted(
                    (_digest(_semantic_note(note)) for note in payload["notes"])
                ):
                    raise RuntimeError("Ordinary inserted notes did not read back exactly")
                binding["content_fingerprint"] = _digest(captured)
            journal["result"] = {
                "binding_token": binding["token"],
                "generation": binding["generation"],
                "state": binding["state"],
                "content_fingerprint": binding.get("content_fingerprint"),
            }
            journal["outcome"] = "acknowledged"
        except Exception as error:
            journal["outcome"] = "partial" if journal["native_mutation_started"] else "declined"
            journal["error"] = str(error)
