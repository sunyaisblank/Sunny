"""Main-thread managed MIDI clip identities and retained operation outcomes.

This finite foundation does not adopt ownership from names/indices. MPE,
Follow Actions and envelope breakpoint populations are unobserved, so complete
destructive-update/recovery guards remain unavailable. There is no compensating
deletion or automatic retry after partial mutation.
"""

from __future__ import annotations

import copy
import hashlib
import json
import struct
import uuid
from typing import Any

from .handler import (
    _finite_number,
    _lom_sequence,
    _valid_note_dictionary,
    _valid_step_envelope_author,
    _valid_step_envelope_query,
)

MANAGED_SCHEMA_VERSION = 1
MANAGED_CALLS = frozenset(
    {
        "sunny_managed_context",
        "sunny_managed_operation",
        "sunny_managed_observe",
        "sunny_managed_sample_envelope",
        "sunny_managed_create_clip",
        "sunny_managed_replace_clip",
        "sunny_managed_rebind",
        "sunny_managed_author_envelope",
        "sunny_managed_update_notes",
    }
)
MANAGED_READS = frozenset(
    {
        "sunny_managed_context",
        "sunny_managed_operation",
        "sunny_managed_observe",
        "sunny_managed_sample_envelope",
    }
)


def _key(value: Any) -> bool:
    return (
        type(value) is str
        and 0 < len(value) <= 64
        and all(
            character in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-"
            for character in value
        )
    )


def _canonical_bytes(value: Any) -> bytes:
    """Schema1 typed encoding shared with C++; independent of JSON float spelling."""
    maximum_bytes = 16 * 1024 * 1024
    parts = [b"SM1;"]
    size = 4

    def append(chunk: bytes) -> None:
        nonlocal size
        size += len(chunk)
        if size > maximum_bytes:
            raise ValueError("Managed fingerprint payload exceeds 16 MiB")
        parts.append(chunk)

    def encode(item: Any, depth: int) -> None:
        if depth > 32:
            raise ValueError("Managed fingerprint nesting exceeds 32")
        if item is None:
            append(b"N;")
        elif type(item) is bool:
            append(b"T;" if item else b"F;")
        elif type(item) is int:
            if not -(1 << 63) <= item < (1 << 64):
                raise ValueError("Managed fingerprint integer is outside the wire domain")
            append(f"I{item};".encode("ascii"))
        elif type(item) is float:
            if not _finite_number(item):
                raise ValueError("Managed fingerprint float must be finite")
            append(b"D" + struct.pack(">d", item).hex().encode("ascii") + b";")
        elif type(item) is str:
            encoded = item.encode("utf-8")
            append(f"S{len(encoded)}:".encode("ascii"))
            append(encoded)
            append(b";")
        elif type(item) is list:
            append(f"A{len(item)}:[".encode("ascii"))
            for child in item:
                encode(child, depth + 1)
            append(b"]")
        elif type(item) is dict and all(type(name) is str for name in item):
            append(f"O{len(item)}:{{".encode("ascii"))
            for name in sorted(item):
                encode(name, depth + 1)
                encode(item[name], depth + 1)
            append(b"}")
        else:
            raise ValueError("Managed fingerprint value is outside the closed JSON domain")

    encode(value, 0)
    return b"".join(parts)


def _digest(value: Any) -> str:
    return hashlib.sha256(_canonical_bytes(value)).hexdigest()


def _fingerprint(value: Any) -> bool:
    return type(value) is str and len(value) == 64 and all(c in "0123456789abcdef" for c in value)


def _note_key(value: dict[str, Any]) -> tuple[Any, ...]:
    return (value["start_time"], value["pitch"], json.dumps(value, sort_keys=True))


_NOTE_FIELDS = frozenset(
    {
        "pitch",
        "start_time",
        "duration",
        "velocity",
        "mute",
        "probability",
        "velocity_deviation",
        "release_velocity",
    }
)
_UPDATE_FIELDS = _NOTE_FIELDS - {"probability", "velocity_deviation"}


def _valid_note_value(name: str, value: Any) -> bool:
    if name == "pitch":
        return type(value) is int and 0 <= value <= 127
    if name == "mute":
        return type(value) is bool
    if not _finite_number(value):
        return False
    if name == "duration":
        return float(value) > 0.0
    if name in ("velocity", "release_velocity"):
        return 0.0 <= float(value) <= 127.0
    if name == "probability":
        return 0.0 <= float(value) <= 1.0
    if name == "velocity_deviation":
        return -127.0 <= float(value) <= 127.0
    return name == "start_time"


def _valid_note_changes(changes: Any) -> bool:
    if type(changes) is not list or not 0 < len(changes) <= 65536:
        return False
    identities = set()
    for change in changes:
        if type(change) is not dict or set(change) != {"note_id", "expected", "updates"}:
            return False
        identity = change["note_id"]
        if type(identity) is not int or not -(1 << 31) <= identity < (1 << 31):
            return False
        if identity in identities:
            return False
        identities.add(identity)
        expected, updates = change["expected"], change["updates"]
        if (
            type(expected) is not dict
            or set(expected) != _NOTE_FIELDS
            or not all(_valid_note_value(name, value) for name, value in expected.items())
            or type(updates) is not dict
            or not updates
            or not set(updates) <= _UPDATE_FIELDS
            or not all(_valid_note_value(name, value) for name, value in updates.items())
            or ("start_time" in updates and float(updates["start_time"]) < 0.0)
        ):
            return False
    return True


def _semantic_note(note: dict[str, Any]) -> dict[str, Any]:
    return {
        name: (note[name] if name in ("pitch", "mute") else float(note[name]))
        for name in _NOTE_FIELDS
    }


def _proposed_notes(
    identity: dict[str, Any], changes: list[Any], clip_end: float
) -> dict[int, Any]:
    """Preflight the full proposed geometry before touching any native MidiNote."""
    if not identity["entire_clip_population_observed"]:
        raise RuntimeError(
            "NotePopulationUnavailable: note revisions require Live 11.1+ full readback"
        )
    before = {note["note_id"]: note for note in identity["notes"]}
    proposed = copy.deepcopy(before)
    geometry = set()
    for change in changes:
        note_id = change["note_id"]
        if note_id not in before or _digest(_semantic_note(before[note_id])) != _digest(
            _semantic_note(change["expected"])
        ):
            raise RuntimeError(
                "Managed note identity/value drift: expected native note does not match"
            )
        for name, value in change["updates"].items():
            proposed[note_id][name] = value if name in ("pitch", "mute") else float(value)
        note = proposed[note_id]
        if not all(_valid_note_value(name, note[name]) for name in _NOTE_FIELDS):
            raise RuntimeError("Managed proposed note is outside the finite native domain")
        if not 0.0 <= note["start_time"] < clip_end or not _finite_number(
            note["start_time"] + note["duration"]
        ):
            raise RuntimeError("Managed proposed note is outside the generated marker domain")
        if any(note[name] != before[note_id][name] for name in ("pitch", "start_time", "duration")):
            geometry.add(note_id)
    # No epsilon: adjacent half-open intervals are safe; any positive overlap
    # involving a moved/resized/repitched note can trigger Live's replacement.
    if not geometry:
        return proposed
    if any(
        not _finite_number(note["start_time"] + note["duration"])
        for population in (before, proposed)
        for note in population.values()
    ):
        raise RuntimeError(
            "NoteGeometryUnavailable: full-population interval endpoint is nonfinite"
        )
    by_pitch: dict[int, list[Any]] = {}
    for note in proposed.values():
        by_pitch.setdefault(note["pitch"], []).append(note)
    for notes in by_pitch.values():
        if not any(note["note_id"] in geometry for note in notes):
            continue
        notes.sort(key=lambda note: note["start_time"])
        for index, first in enumerate(notes):
            for second in notes[index + 1 :]:
                if second["start_time"] >= first["start_time"] + first["duration"]:
                    break
                if first["note_id"] in geometry or second["note_id"] in geometry:
                    raise RuntimeError(
                        "Managed note collision: same-pitch half-open intervals overlap"
                    )
    # The source-observed vector API does not establish atomic collision
    # handling for a batch. Until host-qualified, no destination may cover
    # another retained note's baseline interval, even if that note also moves.
    baseline: dict[int, list[Any]] = {}
    destinations: dict[int, list[Any]] = {}
    for note in before.values():
        baseline.setdefault(note["pitch"], []).append(note)
    for note_id in geometry:
        note = proposed[note_id]
        destinations.setdefault(note["pitch"], []).append(note)
    for pitch, targets in destinations.items():
        sources = sorted(baseline.get(pitch, []), key=lambda note: note["start_time"])
        targets.sort(key=lambda note: note["start_time"] + note["duration"])
        cursor = 0
        furthest: list[Any] = []
        for target in targets:
            end = target["start_time"] + target["duration"]
            while cursor < len(sources) and sources[cursor]["start_time"] < end:
                furthest.append(sources[cursor])
                furthest.sort(key=lambda note: note["start_time"] + note["duration"], reverse=True)
                del furthest[2:]
                cursor += 1
            if any(
                source["note_id"] != target["note_id"]
                and source["start_time"] + source["duration"] > target["start_time"]
                for source in furthest
            ):
                raise RuntimeError(
                    "IntermediateCollisionUnavailable: destination covers another retained baseline note"
                )
    return proposed


def valid_managed_request(name: str, args: list[Any]) -> bool:
    """Admit one closed managed operation family; malformed input never reaches Live."""
    if name == "sunny_managed_context":
        return not args
    if name not in MANAGED_CALLS or len(args) != 1 or type(args[0]) is not dict:
        return False
    value = args[0]
    if not _key(value.get("document_token")):
        return False
    if name == "sunny_managed_operation":
        return set(value) == {"document_token", "operation_id"} and _key(value["operation_id"])
    binding_keys = {"document_token", "project_key", "binding_key"}
    if not _key(value.get("project_key")) or not _key(value.get("binding_key")):
        return False
    if name == "sunny_managed_observe":
        return set(value) == binding_keys
    if name == "sunny_managed_sample_envelope":
        return set(value) == binding_keys | {"parameter", "sample_times"} and (
            _valid_step_envelope_query(
                {"parameter": value["parameter"], "sample_times": value["sample_times"]}
            )
            and len(value["sample_times"]) <= 65536
            and value["parameter"]["kind"] in ("volume", "panning", "send")
        )
    operation_keys = binding_keys | {"operation_id"}
    if not _key(value.get("operation_id")):
        return False
    if name == "sunny_managed_rebind":
        return (
            set(value) == operation_keys | {"expected_manifest"}
            and type(value["expected_manifest"]) is dict
            and type(value["expected_manifest"].get("schema_version")) is int
            and value["expected_manifest"].get("schema_version") == MANAGED_SCHEMA_VERSION
        )
    if name == "sunny_managed_author_envelope":
        return (
            set(value) == operation_keys | {"expected_content_fingerprint", "lane"}
            and _fingerprint(value["expected_content_fingerprint"])
            and _valid_step_envelope_author(value["lane"])
            and value["lane"]["parameter"]["kind"] in ("volume", "panning", "send")
        )
    if name == "sunny_managed_update_notes":
        return (
            set(value) == operation_keys | {"expected_content_fingerprint", "changes"}
            and _fingerprint(value["expected_content_fingerprint"])
            and _valid_note_changes(value["changes"])
        )
    expected = operation_keys | {
        "clip_end",
        "signature_numerator",
        "signature_denominator",
        "notes",
    }
    if name == "sunny_managed_replace_clip":
        expected |= {"expected_content_fingerprint"}
        if not _fingerprint(value.get("expected_content_fingerprint")):
            return False
    return (
        set(value) == expected
        and _finite_number(value["clip_end"])
        and value["clip_end"] > 0.0
        and type(value["signature_numerator"]) is int
        and 1 <= value["signature_numerator"] <= 99
        and type(value["signature_denominator"]) is int
        and value["signature_denominator"] in (1, 2, 4, 8, 16)
        and type(value["notes"]) is list
        and (not value["notes"] or _valid_note_dictionary({"notes": value["notes"]}))
        and all(float(note["start_time"]) < float(value["clip_end"]) for note in value["notes"])
    )


class ManagedRegistry:
    """Retain actual Live objects and operation records across TCP reconnects."""

    def __init__(self, surface: Any) -> None:
        self._surface = surface
        self._handler: Any = None
        self._song: Any = None
        self._bridge_instance = uuid.uuid4().hex
        self._document_token = uuid.uuid4().hex
        self._bindings: dict[tuple[str, str], dict[str, Any]] = {}
        self._operations: dict[str, dict[str, Any]] = {}
        self._author_context: Any = None

    def attach_handler(self, handler: Any) -> None:
        """Supply the existing adapter; all callbacks still run on its Live dispatch."""
        self._handler = handler

    @staticmethod
    def _same(first: Any, second: Any) -> bool:
        return first is second or first == second

    def _ensure_document(self) -> Any:
        song = self._surface.song()
        if self._song is None:
            self._song = song
        elif not self._same(self._song, song):
            self._song = song
            self._document_token = uuid.uuid4().hex
            self._bindings.clear()
            self._operations.clear()
            self._author_context = None
        return song

    @staticmethod
    def _tags(project: str, binding: str) -> tuple[str, str]:
        return f"Sunny|{project}|{binding}|track", f"Sunny|{project}|{binding}|clip"

    def _indices(self, record: dict[str, Any]) -> tuple[int, int]:
        import Live

        song = self._ensure_document()
        if (
            not isinstance(record["track"], Live.Track.Track)
            or not isinstance(record["slot"], Live.ClipSlot.ClipSlot)
            or not isinstance(record["clip"], Live.Clip.Clip)
            or not self._same(record["clip"].canonical_parent, record["slot"])
            or not self._same(record["slot"].canonical_parent, record["track"])
        ):
            raise RuntimeError("Managed native Track/Clip/ClipSlot parent identity changed")
        tracks = [i for i, track in enumerate(song.tracks) if self._same(track, record["track"])]
        if len(tracks) != 1:
            raise RuntimeError("Managed Track identity is no longer uniquely in this Live Set")
        slots = [
            i
            for i, slot in enumerate(record["track"].clip_slots)
            if self._same(slot, record["slot"])
        ]
        if len(slots) != 1 or not self._same(record["slot"].clip, record["clip"]):
            raise RuntimeError("Managed Clip/ClipSlot identity changed")
        return tracks[0], slots[0]

    def _capture(self, record: dict[str, Any]) -> dict[str, Any]:
        """Observe the finite note-only content boundary without GUI-unit assumptions."""
        handler = self._handler
        track_index, slot_index = self._indices(record)
        track, clip = record["track"], record["clip"]
        reasons = []
        if handler._device_chain(track):
            reasons.append("Native devices/racks are outside this initial complete manifest")
        slot_states = [
            slot.has_clip for slot in track.clip_slots if not self._same(slot, record["slot"])
        ]
        if any(type(state) is not bool for state in slot_states):
            raise RuntimeError("Managed ClipSlot returned invalid occupied state")
        other_clips = any(slot_states)
        if other_clips:
            reasons.append("Other Session content on the managed Track is outside this binding")
        clip_properties = {}
        for name in (
            "name",
            "signature_numerator",
            "signature_denominator",
            "start_marker",
            "end_marker",
            "loop_start",
            "loop_end",
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
            "launch_mode",
            "launch_quantization",
            "legato",
            "velocity_amount",
        ):
            clip_properties[name] = handler._serialise(getattr(clip, name))
        if type(clip_properties["name"]) is not str:
            raise RuntimeError("Managed Clip returned an invalid name")
        for name in ("start_marker", "end_marker", "loop_start", "loop_end", "velocity_amount"):
            if not _finite_number(clip_properties[name]):
                raise RuntimeError(f"Managed Clip returned invalid {name}")
        for name in (
            "signature_numerator",
            "signature_denominator",
            "launch_mode",
            "launch_quantization",
        ):
            if type(clip_properties[name]) is not int:
                raise RuntimeError(f"Managed Clip returned invalid {name}")
        for name in set(clip_properties) - {
            "name",
            "start_marker",
            "end_marker",
            "loop_start",
            "loop_end",
            "velocity_amount",
            "signature_numerator",
            "signature_denominator",
            "launch_mode",
            "launch_quantization",
        }:
            if type(clip_properties[name]) is not bool:
                raise RuntimeError(f"Managed Clip returned invalid Boolean {name}")
        if not (
            clip_properties["is_session_clip"] is True
            and clip_properties["is_arrangement_clip"] is False
            and clip_properties["is_midi_clip"] is True
            and clip_properties["is_audio_clip"] is False
            and clip_properties["looping"] is False
            and clip_properties["start_marker"] == 0.0
            and clip_properties["end_marker"] > 0.0
        ):
            reasons.append(
                "Only the generated nonlooping MIDI Session marker interval is supported"
            )
        if clip_properties["has_envelopes"] is not False:
            reasons.append("Complete native envelope breakpoint population is unavailable")
        if clip_properties["has_groove"] is not False:
            reasons.append("Groove content is outside this finite manifest")
        entire_notes = callable(getattr(clip, "get_all_notes_extended", None))
        if entire_notes:
            notes = clip.get_all_notes_extended()
        else:
            notes = clip.get_notes_extended(
                from_pitch=0, pitch_span=128, from_time=0.0, time_span=float(clip.end_marker)
            )
            reasons.append(
                "Entire Clip note population is unavailable outside the finite legacy range"
            )
        values = handler._midi_notes_dictionary(notes)["notes"]
        ids = [note["note_id"] for note in values]
        if (
            len(set(ids)) != len(ids)
            or any(
                type(identity) is not int or not -(1 << 31) <= identity < (1 << 31)
                for identity in ids
            )
            or any(
                not all(_valid_note_value(name, note[name]) for name in _NOTE_FIELDS)
                for note in values
            )
        ):
            raise RuntimeError(
                "Managed Clip returned invalid or duplicate native note identities/values"
            )
        # IDs locate current events, while semantic values survive a possible
        # host reopen ID change. Every observed event/property remains included.
        semantic_notes = [
            {name: value for name, value in note.items() if name != "note_id"} for note in values
        ]
        semantic_notes.sort(key=_note_key)
        identity = {
            "entire_clip_population_observed": entire_notes,
            "notes": sorted(values, key=lambda note: note["note_id"]),
        }
        track_properties = {}
        for name in (
            "name",
            "mute",
            "solo",
            "arm",
            "implicit_arm",
            "is_frozen",
            "is_grouped",
            "back_to_arranger",
            "has_audio_input",
            "has_midi_input",
            "has_audio_output",
            "has_midi_output",
        ):
            track_properties[name] = handler._serialise(getattr(track, name))
        if type(track_properties["name"]) is not str:
            raise RuntimeError("Managed Track returned an invalid name")
        if any(
            type(value) is not bool for name, value in track_properties.items() if name != "name"
        ):
            raise RuntimeError("Managed Track returned invalid Boolean state")
        if (
            track_properties["is_grouped"] is not False
            or track_properties["is_frozen"] is not False
        ):
            reasons.append("Grouped/frozen Track state is unsupported")
        content_counts: dict[str, int | None] = {}
        for name in ("arrangement_clips", "take_lanes"):
            try:
                sequence = _lom_sequence(getattr(track, name))
                if sequence is None:
                    raise RuntimeError(f"Invalid {name} collection")
                content_counts[name] = len(sequence)
                if sequence:
                    reasons.append(f"Nonempty {name} is outside this managed Session binding")
            except AttributeError:
                content_counts[name] = None
                reasons.append(f"{name} content was not observed")
        mixer = track.mixer_device
        if not self._same(mixer.canonical_parent, track):
            raise RuntimeError("Managed mixer does not belong to the native Track")
        mixer_state = {
            "panning_mode": handler._serialise(mixer.panning_mode),
            "crossfade_assign": handler._serialise(mixer.crossfade_assign),
        }
        for name, allowed in (("panning_mode", (0, 1)), ("crossfade_assign", (0, 1, 2))):
            if type(mixer_state[name]) is not int or mixer_state[name] not in allowed:
                raise RuntimeError(f"Managed mixer returned invalid {name}")
        for name in ("volume", "panning", "track_activator"):
            parameter = getattr(mixer, name)
            if not self._same(parameter.canonical_parent, mixer):
                raise RuntimeError("Managed parameter does not belong to the native mixer")
            mixer_state[name] = self._parameter_manifest(parameter)
        mixer_state["sends"] = []
        for parameter in mixer.sends:
            if not self._same(parameter.canonical_parent, mixer):
                raise RuntimeError("Managed send does not belong to the native mixer")
            mixer_state["sends"].append(self._parameter_manifest(parameter))
        routing = {}
        for prefix in ("input", "output"):
            for suffix in ("type", "channel"):
                name = f"{prefix}_routing_{suffix}"
                routing[name] = handler._routing_dictionary(getattr(track, name), name)
        manifest = {
            "schema_version": MANAGED_SCHEMA_VERSION,
            "track": track_properties,
            "clip": clip_properties,
            "notes": semantic_notes,
            "mixer": mixer_state,
            "routing": routing,
            "content_counts": content_counts,
            "devices_empty": not bool(handler._device_chain(track)),
            "other_session_clips_empty": not other_clips,
            "entire_clip_population_observed": entire_notes,
            "mpe_note_expression_state_observed": False,
            "follow_actions_state_observed": False,
        }
        return {
            "track_index": track_index,
            "slot_index": slot_index,
            "manifest": manifest,
            "content_fingerprint": _digest(manifest),
            "note_identity": identity,
            "note_identity_fingerprint": _digest(identity),
            "track_tag": record["track_tag"],
            "clip_tag": record["clip_tag"],
            "structural_boundary_complete": not reasons,
            "content_boundary_complete": False,
            "unavailable_reasons": reasons
            + [
                "MpeExpressionUnavailable: per-note expression fields were not observed",
                "FollowActionsUnavailable: Follow Action settings were not observed",
            ],
        }

    def _parameter_manifest(self, parameter: Any) -> dict[str, Any]:
        import Live

        if not isinstance(parameter, Live.DeviceParameter.DeviceParameter):
            raise RuntimeError("Managed mixer member is not a native DeviceParameter")
        result = {}
        for name in (
            "name",
            "original_name",
            "value",
            "min",
            "max",
            "is_quantized",
            "is_enabled",
            "state",
            "automation_state",
        ):
            result[name] = self._handler._serialise(getattr(parameter, name))
        if type(result["name"]) is not str or type(result["original_name"]) is not str:
            raise RuntimeError("Managed mixer parameter returned an invalid name")
        for name in ("value", "min", "max"):
            if not _finite_number(result[name]):
                raise RuntimeError("Managed mixer parameter has nonfinite internal domain")
        if not result["min"] <= result["value"] <= result["max"]:
            raise RuntimeError("Managed mixer value is outside its domain")
        if type(result["is_quantized"]) is not bool or type(result["is_enabled"]) is not bool:
            raise RuntimeError("Managed mixer parameter has invalid Boolean state")
        if any(
            type(result[name]) is not int or result[name] not in (0, 1, 2)
            for name in ("state", "automation_state")
        ):
            raise RuntimeError("Managed mixer parameter has invalid state")
        return result

    def _require_guard(
        self, record: dict[str, Any], fingerprint: str, *, destructive: bool = True
    ) -> dict[str, Any]:
        observation = self._capture(record)
        boundary = "content_boundary_complete" if destructive else "structural_boundary_complete"
        if not observation[boundary]:
            raise RuntimeError(
                "RecoveryUnavailable: " + "; ".join(observation["unavailable_reasons"])
            )
        if (
            fingerprint != record.get("content_fingerprint")
            or fingerprint != observation["content_fingerprint"]
        ):
            raise RuntimeError(
                "Managed content drift: preserve user edits and require a new reviewed plan"
            )
        self._handler._step_clip_interval(record["clip"], idle=True)
        if record["track"].arm is not False or record["track"].implicit_arm is not False:
            raise RuntimeError("Managed Track must be unarmed")
        return observation

    def authorize_envelope(self, track: Any, clip: Any, parameter: Any) -> bool:
        """Authorize only the exact active managed lane using retained native objects."""
        self._ensure_document()
        context = self._author_context
        if context is None:
            return False
        record = context["record"]
        try:
            track_index, slot_index = self._indices(record)
            self._handler._step_clip_interval(clip, idle=True)
            path = f"song/tracks/{track_index}/clip_slots/{slot_index}/clip"
            _, selected, domain = self._handler._step_envelope_target(
                path, clip, context["selector"]
            )
            observed = self._capture(record)
            actual_manifest = copy.deepcopy(observed["manifest"])
            # Creating only this absent lane may change the presence bit.
            # Existing breakpoint populations remain unobserved and untouched.
            actual_manifest["clip"]["has_envelopes"] = context["before"]["manifest"]["clip"][
                "has_envelopes"
            ]
            if (
                not self._same(track, record["track"])
                or not self._same(clip, record["clip"])
                or not self._same(parameter, context["parameter"])
                or not self._same(parameter, selected)
                or domain["automation_state"] == 2
                or record["track"].arm is not False
                or record["track"].implicit_arm is not False
                or record["track"].is_frozen is not False
                or record["track"].is_grouped is not False
                or _digest(actual_manifest) != context["before"]["content_fingerprint"]
                or observed["note_identity_fingerprint"]
                != context["before"]["note_identity_fingerprint"]
            ):
                return False
            envelope = clip.automation_envelope(parameter)
            if not context["creation_authorized"]:
                if (
                    envelope is not None
                    or observed["content_fingerprint"] != context["before"]["content_fingerprint"]
                ):
                    return False
                context["creation_authorized"] = True
                context["operation"]["native_mutation_started"] = True
            elif (
                not self._handler._envelope_valid(envelope)
                or observed["manifest"]["clip"]["has_envelopes"] is not True
            ):
                return False
            elif context["envelope"] is None:
                context["envelope"] = envelope
            elif not self._same(envelope, context["envelope"]):
                return False
        except Exception:
            return False
        return True

    def _require_in_place_guard(self, record: dict[str, Any], fingerprint: str) -> dict[str, Any]:
        observation = self._capture(record)
        if (
            fingerprint != record.get("content_fingerprint")
            or fingerprint != observation["content_fingerprint"]
            or observation["note_identity_fingerprint"] != record.get("note_identity_fingerprint")
        ):
            raise RuntimeError("Managed content/note identity drift: preserve user edits")
        if observation["note_identity"]["entire_clip_population_observed"] is not True:
            raise RuntimeError("RecoveryUnavailable: entire Clip note population is unavailable")
        self._handler._step_clip_interval(record["clip"], idle=True)
        if any(
            getattr(record["track"], name) is not False
            for name in ("arm", "implicit_arm", "is_frozen", "is_grouped")
        ):
            raise RuntimeError("Managed Track must be unarmed, unfrozen and ungrouped")
        return observation

    def dispatch(self, name: str, args: list[Any]) -> dict[str, Any]:
        """Run one managed operation on Live's main thread, retaining its outcome."""
        song = self._ensure_document()
        if name == "sunny_managed_context":
            return {
                "schema_version": MANAGED_SCHEMA_VERSION,
                "bridge_instance": self._bridge_instance,
                "document_token": self._document_token,
            }
        request = args[0]
        if name == "sunny_managed_observe":
            return self._observe(song, request)
        if name == "sunny_managed_sample_envelope":
            return self._sample_envelope(song, request)
        if request["document_token"] != self._document_token:
            return {"outcome": "unknown_epoch", "document_token": self._document_token}
        if name == "sunny_managed_operation":
            return copy.deepcopy(
                self._operations.get(
                    request["operation_id"],
                    {
                        "outcome": "unknown_operation",
                        "document_token": self._document_token,
                        "operation_id": request["operation_id"],
                    },
                )
            )
        binding = (request["project_key"], request["binding_key"])
        operation_id = request["operation_id"]
        fingerprint = _digest({"name": name, "request": request})
        previous = self._operations.get(operation_id)
        if previous is not None:
            if previous["request_fingerprint"] != fingerprint:
                raise RuntimeError("Managed operation token was reused with different content")
            return copy.deepcopy(previous)
        if len(self._operations) >= 4096:
            raise RuntimeError(
                "Managed journal is full; records are never evicted to permit duplicate execution"
            )
        # Reserve before ANY native mutation. Retain failures and partial objects;
        # replaying a failed record reports it and never starts another mutation.
        operation = {
            "document_token": self._document_token,
            "operation_id": operation_id,
            "request_fingerprint": fingerprint,
            "request": copy.deepcopy(request),
            "name": name,
            "outcome": "pending",
            "native_mutation_started": False,
        }
        self._operations[operation_id] = operation
        try:
            if name == "sunny_managed_create_clip":
                result = self._create(song, binding, request, operation)
            elif name == "sunny_managed_replace_clip":
                record = self._bindings.get(binding)
                if record is None:
                    raise RuntimeError(
                        "RecoveryUnavailable: managed native handles were not retained"
                    )
                self._require_guard(record, request["expected_content_fingerprint"])
                operation["native_mutation_started"] = True
                record["slot"].delete_clip()
                record["clip"] = None
                self._fill_clip(record, request)
                result = self._seal(record)
            elif name == "sunny_managed_rebind":
                result = self._rebind(song, binding, request)
            elif name == "sunny_managed_update_notes":
                result = self._update_notes(binding, request, operation)
            else:
                record = self._bindings.get(binding)
                if record is None:
                    raise RuntimeError(
                        "RecoveryUnavailable: managed native handles were not retained"
                    )
                before = self._require_in_place_guard(
                    record, request["expected_content_fingerprint"]
                )
                track_index, slot_index = self._indices(record)
                path = f"song/tracks/{track_index}/clip_slots/{slot_index}/clip"
                _, parameter, domain = self._handler._step_envelope_target(
                    path, record["clip"], request["lane"]["parameter"]
                )
                lane = request["lane"]
                if (
                    float(lane["clip_end"]) != float(record["clip"].end_marker)
                    or domain["automation_state"] == 2
                    or any(
                        not domain["minimum"] <= float(point["value"]) <= domain["maximum"]
                        for point in lane["points"]
                    )
                ):
                    raise RuntimeError(
                        "Managed lane differs from the actual marker/internal domain"
                    )
                if any(
                    not callable(getattr(record["clip"], method, None))
                    for method in ("automation_envelope", "create_automation_envelope")
                ):
                    raise RuntimeError("Native Python envelope authoring API is unavailable")
                if record["clip"].automation_envelope(parameter) is not None:
                    raise RuntimeError(
                        "EnvelopeRevisionUnavailable: an existing target lane is preserved"
                    )
                self._author_context = {
                    "record": record,
                    "parameter": parameter,
                    "selector": lane["parameter"],
                    "before": before,
                    "operation": operation,
                    "creation_authorized": False,
                    "envelope": None,
                }
                acknowledgement = self._handler._author_step_envelope(
                    path, record["clip"], request["lane"]
                )
                result = {"acknowledgement": acknowledgement, **self._seal(record)}
            operation.update({"outcome": "acknowledged", "result": result})
        except Exception as error:
            operation.update(
                {
                    "outcome": "indeterminate"
                    if operation["native_mutation_started"]
                    else "declined",
                    "error": str(error),
                    "partial_binding_retained": binding in self._bindings,
                }
            )
        finally:
            self._author_context = None
        return copy.deepcopy(operation)

    def _sample_envelope(self, song: Any, request: dict[str, Any]) -> dict[str, Any]:
        observed = self._observe(song, request)
        if observed["outcome"] != "observed":
            return observed
        record = self._bindings[(request["project_key"], request["binding_key"])]
        track_index, slot_index = self._indices(record)
        path = f"song/tracks/{track_index}/clip_slots/{slot_index}/clip"
        sampled = self._handler._get_step_envelope(
            path,
            record["clip"],
            {"parameter": request["parameter"], "sample_times": request["sample_times"]},
        )
        # Resolve and capture again within the same main-thread read dispatch.
        # No receipt/guard is refreshed, and samples are not breakpoint coverage.
        result = self._observe(song, request)
        if result["outcome"] == "observed":
            result["envelope"] = sampled
        return result

    def _observe(self, song: Any, request: dict[str, Any]) -> dict[str, Any]:
        result = {
            "schema_version": MANAGED_SCHEMA_VERSION,
            "context": {
                "bridge_instance": self._bridge_instance,
                "document_token": self._document_token,
            },
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "outcome": "unknown_epoch",
            "ownership_retained": False,
        }
        if request["document_token"] != self._document_token:
            return result
        record = self._bindings.get((request["project_key"], request["binding_key"]))
        if record is None:
            result["outcome"] = "recovery_unavailable"
        elif record.get("clip") is None or record.get("slot") is None:
            matches = [
                index
                for index, track in enumerate(song.tracks)
                if self._same(track, record["track"])
            ]
            result.update(
                {
                    "outcome": "partial_binding",
                    "native_handles_retained": True,
                    "known_track_index": matches[0] if len(matches) == 1 else None,
                    "recovery_available": False,
                }
            )
        else:
            # Inspection observes drift; it never refreshes mutation guards.
            result.update(
                {
                    "outcome": "observed",
                    "ownership_retained": True,
                    "observation": self._capture(record),
                }
            )
        return result

    def _update_notes(
        self, binding: tuple[str, str], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        record = self._bindings.get(binding)
        if record is None:
            raise RuntimeError("RecoveryUnavailable: managed native handles were not retained")
        before = self._capture(record)
        if (
            request["expected_content_fingerprint"] != record.get("content_fingerprint")
            or request["expected_content_fingerprint"] != before["content_fingerprint"]
            or before["note_identity_fingerprint"] != record.get("note_identity_fingerprint")
        ):
            raise RuntimeError("Managed content/note identity drift: preserve user edits")
        self._handler._step_clip_interval(record["clip"], idle=True)
        if (
            record["track"].arm is not False
            or record["track"].implicit_arm is not False
            or record["track"].is_frozen is not False
            or record["track"].is_grouped is not False
        ):
            raise RuntimeError("Managed Track must be unarmed, unfrozen and ungrouped")
        proposed = _proposed_notes(
            before["note_identity"], request["changes"], float(record["clip"].end_marker)
        )
        requested_ids = {change["note_id"] for change in request["changes"]}
        if not requested_ids <= record.get("owned_note_ids", set()):
            raise RuntimeError("Managed note identities are not retained insertion identities")
        clip = record["clip"]
        if not all(
            callable(getattr(clip, name, None))
            for name in ("get_notes_by_id", "apply_note_modifications")
        ):
            raise RuntimeError("Native Python note modification API is unavailable")
        # This vector contains actual existing MidiNote objects, not new-note
        # specifications. Native expression and other unexposed fields stay host-owned.
        native_notes = clip.get_notes_by_id(tuple(sorted(requested_ids)))
        actual = self._handler._midi_notes_dictionary(native_notes)["notes"]
        expected = [
            note for note in before["note_identity"]["notes"] if note["note_id"] in requested_ids
        ]
        if _digest(sorted(actual, key=lambda note: note["note_id"])) != _digest(expected):
            raise RuntimeError("Managed native note selection changed before mutation")
        # Validate retained identities and idle state again immediately before
        # the first setter. Reservation is already durable in the bridge journal.
        final = self._capture(record)
        if (
            request["document_token"] != self._document_token
            or final["content_fingerprint"] != before["content_fingerprint"]
            or final["note_identity_fingerprint"] != before["note_identity_fingerprint"]
        ):
            raise RuntimeError("Managed native context/population changed before mutation")
        self._handler._step_clip_interval(clip, idle=True)
        if record["track"].arm is not False or record["track"].implicit_arm is not False:
            raise RuntimeError("Managed Track became armed before mutation")
        operation["native_mutation_started"] = True
        changes = {change["note_id"]: change for change in request["changes"]}
        for note in native_notes:
            for name in changes[note.note_id]["updates"]:
                setattr(note, name, proposed[note.note_id][name])
        clip.apply_note_modifications(native_notes)
        result = self._seal(record)
        after = {note["note_id"]: note for note in result["note_identity"]["notes"]}
        previous = {note["note_id"]: note for note in before["note_identity"]["notes"]}
        result["note_update"] = {
            "before_manifest": before["manifest"],
            "before_note_identity": before["note_identity"],
            "before_note_identity_fingerprint": before["note_identity_fingerprint"],
            "notes_submitted": len(requested_ids),
            "observed_updates_match_request": all(
                note_id in after and _digest(after[note_id]) == _digest(proposed[note_id])
                for note_id in requested_ids
            ),
            "untouched_notes_preserved": all(
                note_id in after and _digest(note) == _digest(after[note_id])
                for note_id, note in previous.items()
                if note_id not in requested_ids
            ),
            "note_ids_preserved": set(previous) == set(after),
        }
        return result

    def _create(
        self,
        song: Any,
        binding: tuple[str, str],
        request: dict[str, Any],
        operation: dict[str, Any],
    ) -> dict[str, Any]:
        if binding in self._bindings:
            raise RuntimeError(
                "Managed binding already exists; observe/update rather than create again"
            )
        track_tag, clip_tag = self._tags(*binding)
        if any(
            track.name == track_tag
            or any(slot.has_clip and slot.clip.name == clip_tag for slot in track.clip_slots)
            for track in song.tracks
        ):
            raise RuntimeError(
                "RecoveryUnavailable: persisted logical tags exist but native ownership handles are missing"
            )
        if not song.scenes:
            raise RuntimeError(
                "Managed creation requires existing Scene0; no global Scene content is changed"
            )
        before = tuple(song.tracks)
        operation["native_mutation_started"] = True
        try:
            song.create_midi_track(-1)
        finally:
            created = [
                track for track in song.tracks if not any(self._same(track, old) for old in before)
            ]
            if len(created) == 1:
                # Capture even when the host raises after creating the Track.
                self._bindings[binding] = {
                    "track": created[0],
                    "slot": None,
                    "clip": None,
                    "track_tag": track_tag,
                    "clip_tag": clip_tag,
                }
        record = self._bindings.get(binding)
        if record is None or len(song.tracks) != len(before) + 1:
            raise RuntimeError("Native Track creation did not yield one unique new identity")
        record["track"].name = track_tag
        record["track"].arm = False
        record["track"].implicit_arm = False
        record["slot"] = record["track"].clip_slots[0]
        self._fill_clip(record, request)
        return self._seal(record)

    def _fill_clip(self, record: dict[str, Any], request: dict[str, Any]) -> None:
        try:
            record["slot"].create_clip(float(request["clip_end"]))
        finally:
            if record["slot"].has_clip:
                record["clip"] = record["slot"].clip
        clip = record["clip"]
        if clip is None:
            raise RuntimeError("Native Clip creation did not return an occupied slot")
        clip.name = record["clip_tag"]
        clip.looping = False
        clip.start_marker = 0.0
        clip.end_marker = float(request["clip_end"])
        clip.signature_numerator = request["signature_numerator"]
        clip.signature_denominator = request["signature_denominator"]
        clip.launch_mode = 0
        clip.launch_quantization = 1
        clip.legato = False
        clip.velocity_amount = 0.0
        clip.muted = False
        clip.groove = None
        record["requested_clip"] = {
            "end_marker": float(request["clip_end"]),
            "signature_numerator": request["signature_numerator"],
            "signature_denominator": request["signature_denominator"],
        }
        record["requested_notes"] = [
            {
                name: (value if name in ("pitch", "mute") else float(value))
                for name, value in note.items()
            }
            for note in request["notes"]
        ]
        record["owned_note_ids"] = set()
        if request["notes"]:
            record["owned_note_ids"] = set(self._handler._add_new_notes(clip, request["notes"]))

    def _seal(self, record: dict[str, Any]) -> dict[str, Any]:
        result = self._capture(record)
        record["content_fingerprint"] = result["content_fingerprint"]
        record["note_identity_fingerprint"] = result["note_identity_fingerprint"]
        result["track_tag"] = record["track_tag"]
        result["clip_tag"] = record["clip_tag"]
        if "requested_notes" in record:
            requested = sorted(record["requested_notes"], key=_note_key)
            result["observed_notes_match_request"] = _digest(requested) == _digest(
                result["manifest"]["notes"]
            )
            result["observed_clip_properties_match_request"] = all(
                result["manifest"]["clip"][name] == value
                for name, value in record["requested_clip"].items()
            )
        return result

    def _rebind(
        self, song: Any, binding: tuple[str, str], request: dict[str, Any]
    ) -> dict[str, Any]:
        if binding in self._bindings:
            raise RuntimeError("Managed binding already retains native identities")
        track_tag, clip_tag = self._tags(*binding)
        tracks = [track for track in song.tracks if track.name == track_tag]
        if len(tracks) != 1:
            raise RuntimeError("RecoveryUnavailable: persisted Track tag is absent or ambiguous")
        slots = [
            slot for slot in tracks[0].clip_slots if slot.has_clip and slot.clip.name == clip_tag
        ]
        if len(slots) != 1:
            raise RuntimeError("RecoveryUnavailable: persisted Clip tag is absent or ambiguous")
        record = {
            "track": tracks[0],
            "slot": slots[0],
            "clip": slots[0].clip,
            "track_tag": track_tag,
            "clip_tag": clip_tag,
        }
        result = self._capture(record)
        if not result["content_boundary_complete"]:
            raise RuntimeError("RecoveryUnavailable: " + "; ".join(result["unavailable_reasons"]))
        if _digest(result["manifest"]) != _digest(request["expected_manifest"]):
            raise RuntimeError(
                "RecoveryUnavailable: complete persisted content did not exactly match"
            )
        self._handler._step_clip_interval(record["clip"], idle=True)
        if record["track"].arm is not False or record["track"].implicit_arm is not False:
            raise RuntimeError("RecoveryUnavailable: recovered Track must be unarmed")
        self._bindings[binding] = record
        return self._seal(record)
