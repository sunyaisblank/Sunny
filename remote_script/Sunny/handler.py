"""LOM request handler — translates Sunny requests to Ableton API calls.

Each request has a type (get/set/call), a path into
the LOM object hierarchy, and a property or method name with arguments.

Path navigation:
    "song"                          → Live.Application.get_application().get_document()
    "song/tracks/0"                 → song.tracks[0]
    "song/tracks/0/clip_slots/1"    → song.tracks[0].clip_slots[1]
    "song/tracks/0/devices/0"       → song.tracks[0].devices[0]
    "song/master_track"             → song.master_track
    "song/return_tracks/0"          → song.return_tracks[0]
"""

from __future__ import annotations

import json
import logging
import math
import os
from typing import Any

from .build_identity import BRIDGE_SOURCE_SHA256

logger = logging.getLogger("sunny.remote_script.handler")


def _load_bridge_contract() -> tuple[int, int]:
    contract_path = os.path.join(os.path.dirname(__file__), "bridge_contract.json")
    with open(contract_path, encoding="utf-8") as contract_file:
        contract = json.load(contract_file)
    expected_keys = {
        "bridge_protocol_version",
        "target_snapshot_schema_version",
    }
    if not isinstance(contract, dict) or set(contract) != expected_keys:
        raise RuntimeError("Sunny bridge contract has an invalid shape")
    versions = tuple(contract[key] for key in sorted(expected_keys))
    if any(type(version) is not int or not 0 < version <= 0xFFFFFFFF for version in versions):
        raise RuntimeError("Sunny bridge contract versions must be positive uint32 values")
    return contract["bridge_protocol_version"], contract["target_snapshot_schema_version"]


BRIDGE_PROTOCOL_VERSION, TARGET_SNAPSHOT_SCHEMA_VERSION = _load_bridge_contract()

_SONG_GETS = frozenset(
    {
        "tempo",
        "signature_numerator",
        "signature_denominator",
        "is_playing",
        "session_record",
        "record_mode",
        "current_song_time",
    }
)
_SONG_SETS = frozenset({"tempo", "signature_numerator", "signature_denominator"})
_SONG_CALLS = frozenset(
    {
        "sunny_get_target_profile",
        "sunny_get_target_snapshot",
        "sunny_get_scene_count",
        "sunny_get_track_count",
        "sunny_get_return_track_count",
        "sunny_get_remote_log",
        "sunny_set_cue",
        "create_scene",
        "create_midi_track",
        "create_return_track",
        "sunny_managed_context",
        "sunny_ordinary_prepare",
        "sunny_ordinary_execute",
        "sunny_ordinary_operation",
        "sunny_managed_operation",
        "sunny_managed_observe",
        "sunny_managed_create_clip",
        "sunny_managed_replace_clip",
        "sunny_managed_rebind",
        "sunny_managed_author_envelope",
        "sunny_managed_update_notes",
        "sunny_managed_sample_envelope",
        "sunny_managed_revise_note_population",
        "sunny_managed_preview_adoption",
        "sunny_managed_adopt_clip",
        "sunny_managed_insert_device",
        "sunny_managed_update_device_parameters",
        "sunny_managed_preview_devices",
        "sunny_managed_inspect_devices",
        "sunny_managed_adopt_devices",
        "sunny_managed_update_clip_geometry",
        "sunny_managed_preview_song_settings",
        "sunny_managed_inspect_song_settings",
        "sunny_managed_apply_song_settings",
        "sunny_managed_preview_envelope_replacement",
        "sunny_managed_replace_envelope",
        "sunny_managed_update_device_modes",
        "sunny_managed_preview_static_mixer",
        "sunny_managed_inspect_static_mixer",
        "sunny_managed_adopt_static_mixer",
        "sunny_managed_update_static_mixer",
        "sunny_managed_routing_candidates",
        "sunny_managed_inspect_send",
        "sunny_managed_preview_routing",
        "sunny_managed_preview_group",
        "sunny_managed_apply_routing",
    }
)


def _canonical_index(value: str) -> bool:
    syntax_valid = value == "0" or (
        bool(value)
        and "1" <= value[0] <= "9"
        and all("0" <= character <= "9" for character in value[1:])
    )
    maximum_index = "2147483647"
    return syntax_valid and (
        len(value) < len(maximum_index)
        or (len(value) == len(maximum_index) and value <= maximum_index)
    )


def _path_kind(path: str) -> str | None:
    """Classify only canonical paths used by Sunny's documented bridge algebra."""
    segments = path.split("/")
    if segments == ["song"]:
        return "song"
    if not segments or segments[0] != "song" or any(not segment for segment in segments):
        return None

    if len(segments) == 3 and segments[1] == "scenes" and _canonical_index(segments[2]):
        return "scene"

    track_like_end = None
    if len(segments) >= 3 and segments[1] in ("tracks", "return_tracks"):
        if not _canonical_index(segments[2]):
            return None
        track_like_end = 3
        kind = "track" if segments[1] == "tracks" else "return_track"
    elif len(segments) >= 2 and segments[1] == "master_track":
        track_like_end = 2
        kind = "master_track"
    else:
        return None

    tail = segments[track_like_end:]
    if not tail:
        return kind
    if kind in ("track", "return_track") and tail == ["mixer_device"]:
        return "mixer_device"
    if kind == "master_track" and tail == ["mixer_device"]:
        return "master_mixer_device"
    if kind == "track" and len(tail) == 2 and tail[0] == "clip_slots" and _canonical_index(tail[1]):
        return "clip_slot"
    if (
        kind == "track"
        and len(tail) == 3
        and tail[0] == "clip_slots"
        and _canonical_index(tail[1])
        and tail[2] == "clip"
    ):
        return "clip"
    if len(tail) == 2 and tail[0] == "devices" and _canonical_index(tail[1]):
        return "device"
    if len(tail) == 2 and tail[0] == "mixer_device" and tail[1] == "volume":
        return "volume_parameter"
    if len(tail) == 2 and tail[0] == "mixer_device" and tail[1] == "track_activator":
        return (
            "master_activator_parameter"
            if kind == "master_track"
            else "return_activator_parameter"
            if kind == "return_track"
            else "track_activator_parameter"
        )
    if (
        kind in ("track", "return_track", "master_track")
        and len(tail) == 2
        and tail
        == [
            "mixer_device",
            "panning",
        ]
    ):
        return "master_panning_parameter" if kind == "master_track" else "panning_parameter"
    if (
        kind == "track"
        and len(tail) == 3
        and tail[0] == "mixer_device"
        and tail[1] == "sends"
        and _canonical_index(tail[2])
    ):
        return "send_parameter"
    return None


def _finite_number(value: Any) -> bool:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        return False
    try:
        return math.isfinite(value)
    except OverflowError:
        return False


def _protocol_index(value: Any, *, allow_append: bool = False) -> bool:
    minimum = -1 if allow_append else 0
    return not isinstance(value, bool) and isinstance(value, int) and minimum <= value <= 2147483647


_NOTE_FIELDS = (
    "note_id",
    "pitch",
    "start_time",
    "duration",
    "velocity",
    "mute",
    "probability",
    "velocity_deviation",
    "release_velocity",
)

# Live returns the created Track, Scene or Clip from these calls. Sunny's
# evidence is re-observed through dedicated requests, so the private host
# object is discarded rather than serialised.
_STRUCTURAL_CALLS = frozenset(
    {"create_midi_track", "create_return_track", "create_scene", "create_clip", "delete_clip"}
)


def _lom_sequence(value: Any) -> tuple[Any, ...] | None:
    """Return a sized host collection as a tuple, or None for any other value.

    Live's Python API returns tuples for most object collections and its own
    ``Vector``/``StringVector``/``IntVector`` containers elsewhere; which one a
    given property uses is not settled by public documentation. Both are sized
    and iterable. Iterators, strings and mappings are not collections here.
    """
    if isinstance(value, (list, tuple)):
        return tuple(value)
    if isinstance(value, (str, bytes, bytearray, dict)):
        return None
    host_type = type(value)
    if not hasattr(host_type, "__len__") or not hasattr(host_type, "__iter__"):
        return None
    items = tuple(value)
    return items if len(items) == len(value) else None


def _valid_routing_dictionary(value: Any) -> bool:
    return (
        type(value) is dict
        and set(value) == {"display_name", "identifier"}
        and type(value["display_name"]) is str
        and type(value["identifier"]) is str
    )


def _device_parameter_state(value: Any, label: str, parameter_name: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value not in (0, 1, 2):
        raise RuntimeError(f"Device parameter '{parameter_name}' returned invalid {label}")
    return value


def _valid_note_dictionary(value: Any) -> bool:
    if (
        not isinstance(value, dict)
        or set(value) != {"notes"}
        or not isinstance(value["notes"], list)
        or not value["notes"]
    ):
        return False
    expected = {
        "pitch",
        "start_time",
        "duration",
        "velocity",
        "mute",
        "probability",
        "velocity_deviation",
        "release_velocity",
    }
    for note in value["notes"]:
        if not isinstance(note, dict) or set(note) != expected:
            return False
        pitch = note["pitch"]
        velocity = note["velocity"]
        if isinstance(pitch, bool) or not isinstance(pitch, int) or not 0 <= pitch <= 127:
            return False
        if isinstance(velocity, bool) or not isinstance(velocity, int) or not 1 <= velocity <= 127:
            return False
        if not _finite_number(note["start_time"]) or note["start_time"] < 0:
            return False
        if not _finite_number(note["duration"]) or note["duration"] <= 0:
            return False
        if not isinstance(note["mute"], bool):
            return False
        if not isinstance(note["probability"], float) or note["probability"] != 1.0:
            return False
        if not isinstance(note["velocity_deviation"], float) or note["velocity_deviation"] != 0.0:
            return False
        release_velocity = note["release_velocity"]
        if (
            not isinstance(release_velocity, float)
            or not math.isfinite(release_velocity)
            or not 0.0 <= release_velocity <= 127.0
            or not release_velocity.is_integer()
        ):
            return False
    return True


def _valid_note_id_query(value: Any) -> bool:
    if not isinstance(value, dict) or set(value) != {"note_ids", "return"}:
        return False
    note_ids = value["note_ids"]
    fields = value["return"]
    if (
        not isinstance(note_ids, list)
        or not note_ids
        or any(
            isinstance(note_id, bool)
            or not isinstance(note_id, int)
            or not -2147483648 <= note_id <= 2147483647
            for note_id in note_ids
        )
        or len(set(note_ids)) != len(note_ids)
        or not isinstance(fields, list)
        or len(fields) != 9
        or any(not isinstance(field, str) for field in fields)
        or len(set(fields)) != len(fields)
    ):
        return False
    return set(fields) == {
        "note_id",
        "pitch",
        "start_time",
        "duration",
        "velocity",
        "mute",
        "probability",
        "velocity_deviation",
        "release_velocity",
    }


def _valid_all_notes_query(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and set(value) == {"return"}
        and isinstance(value["return"], list)
        and len(value["return"]) == 9
        and all(isinstance(field, str) for field in value["return"])
        and len(set(value["return"])) == 9
        and set(value["return"])
        == {
            "note_id",
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


def _valid_ranged_notes_query(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and set(value) == {"return", "from_pitch", "pitch_span", "from_time", "time_span"}
        and _valid_all_notes_query({"return": value["return"]})
        and type(value["from_pitch"]) is int
        and value["from_pitch"] == 0
        and type(value["pitch_span"]) is int
        and value["pitch_span"] == 128
        and _finite_number(value["from_time"])
        and value["from_time"] == 0.0
        and _finite_number(value["time_span"])
        and value["time_span"] > 0.0
    )


def _valid_envelope_parameter(value: Any) -> bool:
    if not isinstance(value, dict) or not isinstance(value.get("kind"), str):
        return False
    kind = value["kind"]
    if kind in ("volume", "panning"):
        return set(value) == {"kind"}
    if kind == "send":
        return set(value) == {"kind", "send_index"} and _protocol_index(value["send_index"])
    return (
        kind == "device"
        and set(value) == {"kind", "device_index", "parameter_name"}
        and _protocol_index(value["device_index"])
        and isinstance(value["parameter_name"], str)
        and bool(value["parameter_name"])
    )


def _valid_step_envelope_author(value: Any) -> bool:
    from .managed_envelope_revision import MAX_STEP_POINTS

    if (
        not isinstance(value, dict)
        or set(value) != {"parameter", "interpolation", "clip_end", "points"}
        or not _valid_envelope_parameter(value["parameter"])
        or value["interpolation"] != "step"
        or not _finite_number(value["clip_end"])
        or value["clip_end"] <= 0.0
        or not isinstance(value["points"], list)
        or not value["points"]
        or len(value["points"]) > MAX_STEP_POINTS
    ):
        return False
    previous = -1.0
    for point in value["points"]:
        if (
            not isinstance(point, dict)
            or set(point) != {"time", "value"}
            or not _finite_number(point["time"])
            or not _finite_number(point["value"])
            or not previous < float(point["time"]) < float(value["clip_end"])
        ):
            return False
        previous = float(point["time"])
    return value["points"][0]["time"] == 0.0


def _valid_step_envelope_query(value: Any) -> bool:
    if (
        not isinstance(value, dict)
        or set(value) != {"parameter", "sample_times"}
        or not _valid_envelope_parameter(value["parameter"])
        or not isinstance(value["sample_times"], list)
        or not value["sample_times"]
    ):
        return False
    previous = -1.0
    for time in value["sample_times"]:
        if not _finite_number(time) or time < 0.0 or float(time) <= previous:
            return False
        previous = float(time)
    return True


def _valid_request_arguments(req_type: str, kind: str, name: str, args: list[Any]) -> bool:
    if req_type == "get":
        return not args
    if req_type == "set":
        if len(args) != 1:
            return False
        value = args[0]
        if kind == "song" and name == "tempo":
            return _finite_number(value) and 20 <= value <= 999
        if kind in ("song", "clip") and name == "signature_numerator":
            return not isinstance(value, bool) and isinstance(value, int) and 1 <= value <= 99
        if kind in ("song", "clip") and name == "signature_denominator":
            return (
                not isinstance(value, bool) and isinstance(value, int) and value in (1, 2, 4, 8, 16)
            )
        if name == "name":
            return isinstance(value, str)
        if kind == "track" and name in ("arm", "implicit_arm"):
            return value is False
        if kind == "return_track" and name in ("mute", "solo"):
            return value is False
        if name in (
            "mute",
            "solo",
            "tempo_enabled",
            "time_signature_enabled",
            "looping",
            "muted",
        ):
            return isinstance(value, bool)
        if kind == "clip" and name in ("start_marker", "end_marker"):
            return _finite_number(value) and value >= 0
        if kind == "clip" and name == "launch_mode":
            return not isinstance(value, bool) and isinstance(value, int) and value == 0
        if kind == "clip" and name == "launch_quantization":
            return not isinstance(value, bool) and isinstance(value, int) and value == 1
        if kind == "clip" and name == "legato":
            return value is False
        if kind == "clip" and name == "velocity_amount":
            return isinstance(value, float) and math.isfinite(value) and value == 0.0
        if kind == "clip" and name == "groove":
            return value is None
        if kind == "mixer_device" and name == "crossfade_assign":
            return not isinstance(value, bool) and isinstance(value, int) and value == 1
        if kind in ("mixer_device", "master_mixer_device") and name == "panning_mode":
            return not isinstance(value, bool) and isinstance(value, int) and value == 0
        if kind == "track_activator_parameter" and name == "value":
            return isinstance(value, float) and math.isfinite(value) and value in (0.0, 1.0)
        if kind in ("return_activator_parameter", "master_activator_parameter") and name == "value":
            return isinstance(value, float) and value == 1.0
        if kind == "master_panning_parameter":
            return isinstance(value, float) and value == 0.0
        return _finite_number(value)
    if kind == "song":
        if name.startswith("sunny_ordinary_"):
            from .ordinary_clip import valid_ordinary_request

            return valid_ordinary_request(name, args)
        if name.startswith("sunny_managed_"):
            from .managed import valid_managed_request

            return valid_managed_request(name, args)
        if name in (
            "sunny_get_target_profile",
            "sunny_get_target_snapshot",
            "sunny_get_scene_count",
            "sunny_get_track_count",
            "sunny_get_return_track_count",
            "create_return_track",
        ):
            return not args
        if name in ("create_scene", "create_midi_track"):
            return len(args) == 1 and _protocol_index(args[0], allow_append=True)
        if name == "sunny_get_remote_log":
            return (
                len(args) in (1, 2)
                and type(args[0]) is int
                and 0 <= args[0] <= 2147483647
                and (
                    len(args) == 1
                    or (
                        type(args[1]) is str
                        and len(args[1]) == 32
                        and all(character in "0123456789abcdef" for character in args[1])
                    )
                )
            )
        if name == "sunny_set_cue":
            return (
                len(args) == 2
                and _finite_number(args[0])
                and args[0] >= 0
                and isinstance(args[1], str)
            )
    if kind in ("track", "return_track", "master_track"):
        if name == "sunny_get_device_count":
            return not args
        if kind in ("track", "return_track") and name == "sunny_set_output_routing_type":
            return len(args) == 1 and _valid_routing_dictionary(args[0])
        if kind in ("track", "return_track") and name == "sunny_set_output_routing_channel":
            return (
                len(args) == 2
                and _valid_routing_dictionary(args[0])
                and _valid_routing_dictionary(args[1])
            )
        if name == "insert_device":
            return (
                len(args) in (1, 2)
                and isinstance(args[0], str)
                and bool(args[0])
                and (len(args) == 1 or _protocol_index(args[1]))
            )
    if kind == "clip_slot":
        if name == "delete_clip":
            return not args
        return len(args) == 1 and _finite_number(args[0]) and args[0] > 0
    if kind == "clip":
        if name == "sunny_clear_all_envelopes":
            return not args
        if name == "sunny_author_step_envelope":
            return len(args) == 1 and _valid_step_envelope_author(args[0])
        if name == "sunny_get_step_envelope":
            return len(args) == 1 and _valid_step_envelope_query(args[0])
        return len(args) == 1 and (
            _valid_note_dictionary(args[0])
            if name == "add_new_notes"
            else _valid_note_id_query(args[0])
            if name == "get_notes_by_id"
            else _valid_all_notes_query(args[0])
            if name == "get_all_notes_extended"
            else _valid_ranged_notes_query(args[0])
            if name == "get_notes_extended"
            else False
        )
    if kind == "device":
        if name == "sunny_resolve_native_display_value":
            from .native_units import valid_native_display_request

            return len(args) == 1 and valid_native_display_request(args[0])
        if name == "sunny_get_device_parameter":
            return (
                len(args) == 2
                and isinstance(args[0], str)
                and bool(args[0])
                and args[1] in ("value", "display_value")
            )
        return (
            name == "sunny_set_device_parameter"
            and len(args) == 5
            and isinstance(args[0], str)
            and bool(args[0])
            and _finite_number(args[1])
            and args[2] in ("value", "display_value")
            and _finite_number(args[3])
            and _finite_number(args[4])
            and args[4] > args[3]
        )
    return False


def _request_allowed(req_type: str, path: str, name: str, args: list[Any]) -> bool:
    kind = _path_kind(path)
    if kind is None:
        return False
    allowed = False
    if kind == "song":
        allowed = (
            (req_type == "get" and name in _SONG_GETS)
            or (req_type == "set" and name in _SONG_SETS)
            or (req_type == "call" and name in _SONG_CALLS)
        )
    elif kind == "track":
        allowed = (
            req_type == "set" and name in ("name", "mute", "solo", "arm", "implicit_arm")
        ) or (
            req_type == "call"
            and name
            in (
                "insert_device",
                "sunny_get_device_count",
                "sunny_set_output_routing_type",
                "sunny_set_output_routing_channel",
            )
        )
    elif kind == "scene":
        allowed = req_type == "set" and name in (
            "name",
            "tempo_enabled",
            "time_signature_enabled",
        )
    elif kind == "return_track":
        allowed = (req_type == "set" and name in ("name", "mute", "solo")) or (
            req_type == "call"
            and name
            in (
                "insert_device",
                "sunny_get_device_count",
                "sunny_set_output_routing_type",
                "sunny_set_output_routing_channel",
            )
        )
    elif kind == "master_track":
        allowed = req_type == "call" and name in ("insert_device", "sunny_get_device_count")
    elif kind == "clip_slot":
        allowed = req_type == "call" and name in ("create_clip", "delete_clip")
    elif kind == "clip":
        allowed = (
            req_type == "set"
            and name
            in (
                "name",
                "signature_numerator",
                "signature_denominator",
                "start_marker",
                "end_marker",
                "looping",
                "muted",
                "launch_mode",
                "launch_quantization",
                "legato",
                "velocity_amount",
                "groove",
            )
        ) or (
            req_type == "call"
            and name
            in (
                "add_new_notes",
                "get_notes_by_id",
                "get_notes_extended",
                "get_all_notes_extended",
                "sunny_clear_all_envelopes",
                "sunny_author_step_envelope",
                "sunny_get_step_envelope",
            )
        )
    elif kind == "mixer_device":
        allowed = req_type == "set" and name in ("crossfade_assign", "panning_mode")
    elif kind == "master_mixer_device":
        allowed = req_type == "set" and name == "panning_mode"
    elif kind == "volume_parameter":
        allowed = req_type == "set" and name == "display_value"
    elif kind == "panning_parameter":
        allowed = req_type == "set" and name == "value"
    elif kind == "master_panning_parameter":
        allowed = req_type == "set" and name == "value"
    elif kind in (
        "track_activator_parameter",
        "return_activator_parameter",
        "master_activator_parameter",
    ):
        allowed = req_type == "set" and name == "value"
    elif kind == "send_parameter":
        allowed = req_type == "set" and name == "display_value"
    elif kind == "device":
        allowed = req_type == "call" and name in (
            "sunny_get_device_parameter",
            "sunny_set_device_parameter",
            "sunny_resolve_native_display_value",
        )
    return allowed and _valid_request_arguments(req_type, kind, name, args)


class LomHandler:
    """Translates LomRequest JSON to Ableton LOM API calls."""

    def __init__(
        self, surface, remote_log=None, *, envelope_authorizer=None, managed_registry=None
    ):
        self._surface = surface
        self._remote_log = remote_log
        # The surface dispatches Live operations on its main thread. Product
        # admission stays denied until managed project identities supply this
        # callback; a path/index/name never establishes ownership.
        self._envelope_authorizer = envelope_authorizer
        self._managed_registry = managed_registry

    @staticmethod
    def is_remote_log_request(request: object) -> bool:
        """Whether a request only reads the Remote Script's own log.

        Such a request touches no Live object, so the surface may answer it
        on the server thread even while Live's main thread is busy.
        """
        return (
            isinstance(request, dict)
            and request.get("type") == "call"
            and request.get("name") == "sunny_get_remote_log"
        )

    def handle(self, request: dict) -> dict:
        """Dispatch a single request and return a response dict."""
        if not isinstance(request, dict):
            return {"success": False, "error": "Request must be an object"}
        unexpected = set(request) - {"bridge_protocol_version", "type", "path", "name", "args"}
        if unexpected:
            return {"success": False, "error": "Request contains unknown fields"}
        version = request.get("bridge_protocol_version")
        if isinstance(version, bool) or version != BRIDGE_PROTOCOL_VERSION:
            return {
                "success": False,
                "error": f"Unsupported bridge protocol version: {version!r}",
            }
        req_type = request.get("type", "")
        path = request.get("path", "")
        name = request.get("name", "")
        args = request.get("args", [])

        try:
            if req_type not in {"get", "set", "call"}:
                return {"success": False, "error": f"Unknown request type: {req_type}"}
            if not isinstance(path, str) or not isinstance(name, str) or not name:
                return {"success": False, "error": "Request path/name must be non-empty strings"}
            if not isinstance(args, list):
                return {"success": False, "error": "Request args must be an array"}

            if not _request_allowed(req_type, path, name, args):
                return {
                    "success": False,
                    "error": f"Operation is outside Sunny bridge protocol v{BRIDGE_PROTOCOL_VERSION}",
                }

            if req_type == "call" and name == "sunny_get_remote_log":
                if self._remote_log is None:
                    return {"success": False, "error": "Remote log is not enabled"}
                return {"success": True, "value": self._remote_log.entries_after(*args)}

            if req_type == "call" and name == "sunny_get_target_profile":
                return {"success": True, "value": self._serialise(self._target_profile())}

            if req_type == "call" and name == "sunny_get_target_snapshot":
                return {"success": True, "value": self._serialise(self._target_snapshot())}

            if req_type == "call" and name.startswith("sunny_ordinary_"):
                if self._managed_registry is None:
                    return {"success": False, "error": "Native ownership registry is unavailable"}
                return {
                    "success": True,
                    "value": self._managed_registry.dispatch_ordinary(name, args),
                }

            if req_type == "call" and name.startswith("sunny_managed_"):
                if self._managed_registry is None:
                    return {"success": False, "error": "Managed ownership registry is unavailable"}
                return {"success": True, "value": self._managed_registry.dispatch(name, args)}

            obj = self._resolve_path(path)

            if req_type == "get":
                if args:
                    return {"success": False, "error": "Get requests do not accept args"}
                value = getattr(obj, name)
                if callable(value):
                    return {"success": False, "error": f"'{name}' is a method; use call"}
                return {"success": True, "value": self._serialise(value)}

            elif req_type == "set":
                if len(args) != 1:
                    return {"success": False, "error": "Set requests require exactly one arg"}
                getattr(obj, name)
                requested = args[0]
                setattr(obj, name, requested)
                observed = getattr(obj, name)
                return {
                    "success": True,
                    "value": self._serialise(
                        {"property": name, "requested": requested, "observed": observed}
                    ),
                }

            elif req_type == "call":
                if name == "sunny_get_scene_count":
                    return {"success": True, "value": self._collection_count(obj.scenes, "scenes")}
                if name == "sunny_get_track_count":
                    return {"success": True, "value": self._collection_count(obj.tracks, "tracks")}
                if name == "sunny_get_return_track_count":
                    return {
                        "success": True,
                        "value": self._collection_count(obj.return_tracks, "return_tracks"),
                    }
                if name == "sunny_set_cue":
                    evidence = self._set_cue(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_get_device_count":
                    return {"success": True, "value": len(self._device_chain(obj))}
                if name == "sunny_set_output_routing_type":
                    evidence = self._set_output_routing_type(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_set_output_routing_channel":
                    evidence = self._set_output_routing_channel(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "insert_device":
                    evidence = self._insert_device(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_set_device_parameter":
                    evidence = self._set_device_parameter(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_get_device_parameter":
                    evidence = self._get_device_parameter(obj, *args)
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_resolve_native_display_value":
                    evidence = self._resolve_native_display_value(obj, args[0])
                    return {"success": True, "value": self._serialise(evidence)}
                if name == "sunny_author_step_envelope":
                    acknowledgement = self._author_step_envelope(path, obj, args[0])
                    return {"success": True, "value": acknowledgement}
                if name == "sunny_get_step_envelope":
                    evidence = self._get_step_envelope(path, obj, args[0])
                    return {"success": True, "value": evidence}
                if name == "add_new_notes":
                    return {"success": True, "value": self._add_new_notes(obj, args[0]["notes"])}
                if name == "get_notes_by_id":
                    notes = obj.get_notes_by_id(tuple(args[0]["note_ids"]))
                    return {"success": True, "value": self._midi_notes_dictionary(notes)}
                if name == "get_all_notes_extended":
                    notes = obj.get_all_notes_extended()
                    return {"success": True, "value": self._midi_notes_dictionary(notes)}
                if name == "get_notes_extended":
                    query = args[0]
                    # Python keyword arguments/MidiNoteVector are observed in
                    # Live's MxDCore source, not the Max-side dictionary ABI.
                    notes = obj.get_notes_extended(
                        from_pitch=query["from_pitch"],
                        pitch_span=query["pitch_span"],
                        from_time=float(query["from_time"]),
                        time_span=float(query["time_span"]),
                    )
                    return {"success": True, "value": self._midi_notes_dictionary(notes)}
                if name in _STRUCTURAL_CALLS:
                    getattr(obj, name)(*args)
                    return {"success": True, "value": None}
                if name == "sunny_clear_all_envelopes":
                    obj.clear_all_envelopes()
                    has_envelopes = obj.has_envelopes
                    if not isinstance(has_envelopes, bool):
                        raise RuntimeError("Clip returned invalid has_envelopes state")
                    return {"success": True, "value": {"has_envelopes": has_envelopes}}
                method = getattr(obj, name, None)
                if method is None:
                    return {"success": False, "error": f"No method '{name}' on {path}"}
                result = method(*args)
                return {"success": True, "value": self._serialise(result)}

        except AttributeError as e:
            return {"success": False, "error": f"Attribute error: {e}"}
        except IndexError as e:
            return {"success": False, "error": f"Index error: {e}"}
        except Exception as e:
            logger.error("Handler error: %s", e, exc_info=True)
            return {"success": False, "error": str(e)}

    def _resolve_path(self, path: str) -> Any:
        """Navigate the LOM hierarchy from a slash-separated path.

        Starting object is the Song (Live Set document).
        """
        song = self._get_song()
        if not path or path == "song":
            return song

        segments = [s for s in path.split("/") if s]
        if segments[0] == "song":
            segments = segments[1:]

        obj = song
        for segment in segments:
            # Numeric index into a list property
            if segment.isdigit():
                idx = int(segment)
                if hasattr(obj, "__getitem__"):
                    obj = obj[idx]
                else:
                    # Try as a tuple/list
                    obj = list(obj)[idx]
            elif segment == "devices":
                # Cycling '74 documents Track.devices as including the mixer
                # device. Sunny devices/N is deliberately the insertable
                # device-chain view used by Track.insert_device.
                obj = self._device_chain(obj)
            else:
                obj = getattr(obj, segment)

        return obj

    def _get_song(self) -> Any:
        """Get the current Live Set (Song) object."""
        try:
            # Standard Ableton API path
            return self._surface.song()
        except (AttributeError, TypeError):
            pass

        try:
            # _Framework ControlSurface path
            if hasattr(self._surface, "_c_instance"):
                import Live

                app = Live.Application.get_application()
                return app.get_document()
        except Exception:
            pass

        raise RuntimeError("Cannot access Ableton Song object")

    @staticmethod
    def _live_module() -> Any:
        """Return Live's embedded ``Live`` module, imported only inside a host.

        Tests supply the offline model under the same module name.
        """
        import Live

        return Live

    @classmethod
    def _get_application(cls) -> Any:
        """Return Live's documented Application object."""
        return cls._live_module().Application.get_application()

    @classmethod
    def _add_new_notes(cls, clip: Any, notes: list[dict[str, Any]]) -> list[int]:
        """Insert validated wire notes as ``MidiNoteSpecification`` objects; return their IDs.

        ``Clip.add_new_notes`` accepts only an iterable of specifications, the
        form Ableton's own MxDCore builds with ``MidiNoteSpecification(**note)``.
        """
        specification_type = cls._live_module().Clip.MidiNoteSpecification
        specifications = tuple(
            specification_type(
                pitch=note["pitch"],
                start_time=float(note["start_time"]),
                duration=float(note["duration"]),
                velocity=float(note["velocity"]),
                mute=note["mute"],
                probability=note["probability"],
                velocity_deviation=note["velocity_deviation"],
                release_velocity=note["release_velocity"],
            )
            for note in notes
        )
        note_ids = _lom_sequence(clip.add_new_notes(specifications))
        if note_ids is None:
            raise RuntimeError("Clip.add_new_notes returned no note IDs")
        if len(note_ids) != len(specifications) or any(
            isinstance(note_id, bool) or not isinstance(note_id, int) for note_id in note_ids
        ):
            raise RuntimeError("Clip.add_new_notes returned invalid note IDs")
        return [int(note_id) for note_id in note_ids]

    @staticmethod
    def _midi_note_dictionary(note: Any) -> dict[str, Any]:
        """Convert one host ``MidiNote`` to the nine-field wire dictionary."""
        fields = {}
        for field in _NOTE_FIELDS:
            value = getattr(note, field)
            if field == "mute":
                valid = isinstance(value, bool)
            elif field in ("note_id", "pitch"):
                valid = not isinstance(value, bool) and isinstance(value, int)
                value = int(value) if valid else value
            else:
                valid = _finite_number(value)
                value = float(value) if valid else value
            if not valid:
                raise RuntimeError(f"Clip returned invalid MidiNote {field}")
            fields[field] = value
        return fields

    @classmethod
    def _midi_notes_dictionary(cls, notes: Any) -> dict[str, list[dict[str, Any]]]:
        """Convert a host ``MidiNoteVector`` to the wire ``{"notes": [...]}`` form."""
        items = _lom_sequence(notes)
        if items is None:
            raise RuntimeError("Clip returned an invalid MidiNote collection")
        return {"notes": [cls._midi_note_dictionary(note) for note in items]}

    def _target_profile(self) -> dict:
        """Report observed host facts and conservative deployment capabilities.

        The application version functions are part of the public LOM. The
        Control Surface Python framework hosting this adapter is not a stable
        public API, so its contract is identified as version-coupled. Live's
        public Application model does not expose Max for Live licence state.
        """
        application = self._get_application()
        major = self._lom_integer(application.get_major_version(), "Application major version")
        minor = self._lom_integer(application.get_minor_version(), "Application minor version")
        bugfix = self._lom_integer(application.get_bugfix_version(), "Application bugfix version")
        version_string = self._lom_string(
            application.get_version_string(), "Application version string"
        )
        if min(major, minor, bugfix) < 0:
            raise RuntimeError("Application returned a negative version component")
        return {
            "bridge_protocol_version": BRIDGE_PROTOCOL_VERSION,
            "adapter": {
                "name": "Sunny Remote Script",
                "runtime": "control_surface_python",
                "contract": "version_coupled_private",
                "source_sha256": BRIDGE_SOURCE_SHA256,
            },
            "live": {
                "version": {
                    "major": major,
                    "minor": minor,
                    "bugfix": bugfix,
                    "string": version_string,
                }
            },
            "capabilities": {
                "clip_add_new_notes": "available" if major >= 11 else "unavailable",
                "track_insert_device_native": (
                    "available" if (major, minor) >= (12, 3) else "unavailable"
                ),
                "automation_envelope_authoring": "unavailable",
                "group_track_creation": "unavailable",
                "arbitrary_browser_loading": "unavailable",
                "max_for_live": "unknown",
                "structural_snapshot": "available",
            },
        }

    def _resolve_native_display_value(self, device: Any, query: dict[str, Any]) -> dict[str, Any]:
        """One read-only main-thread operation over an observed native device.

        Source-candidate coverage is Live 12.3.x/12.4.x. Actual class, population,
        modes and display evidence still determine admission; an application
        version alone never qualifies an ABI, edition or interpreted sound.
        """
        from .native_units import NativeUnitError, resolve_registered_native_display_value

        result = {"schema_version": 1, **query}
        calls = 0

        def version() -> tuple[int, int, int]:
            application = self._get_application()
            observed = tuple(
                self._lom_integer(getattr(application, method)(), "Application version")
                for method in ("get_major_version", "get_minor_version", "get_bugfix_version")
            )
            if min(observed) < 0:
                raise NativeUnitError(
                    "InvalidObservation", "Application version must be nonnegative"
                )
            return observed

        try:
            initial_version = version()
            if initial_version[0] != 12 or initial_version[1] not in (3, 4):
                raise NativeUnitError(
                    "UnknownRegistryCoverage", "Native display candidates cover Live 12.3.x/12.4.x"
                )
            candidate = resolve_registered_native_display_value(
                device, query["capability_id"], query["target"], query["tolerance"]
            )
            calls = candidate["formatter_calls"]
            if version() != initial_version:
                raise NativeUnitError(
                    "HostVersionDrift", "Application version changed during observation", calls
                )
            return {**result, "outcome": "resolved", "candidate": candidate}
        except NativeUnitError as error:
            return {
                **result,
                "outcome": "declined",
                "reason": error.reason,
                "diagnostic": str(error),
                "formatter_calls": error.formatter_calls,
            }
        except Exception as error:
            return {
                **result,
                "outcome": "declined",
                "reason": "ObservationUnavailable",
                "diagnostic": "Native application observation failed: " + str(error),
                "formatter_calls": calls,
            }

    @staticmethod
    def _device_snapshot(device: Any) -> dict[str, Any]:
        """Return bounded public Device facts guarding addressing, shape, and latency."""
        device_type = device.type
        is_active = device.is_active
        can_have_chains = device.can_have_chains
        latency_in_samples = device.latency_in_samples
        latency_in_ms = device.latency_in_ms
        if (
            isinstance(device_type, bool)
            or not isinstance(device_type, int)
            or device_type not in (0, 1, 2, 4)
        ):
            raise RuntimeError("Device returned invalid type")
        if not isinstance(is_active, bool):
            raise RuntimeError("Device returned invalid is_active")
        if not isinstance(can_have_chains, bool):
            raise RuntimeError("Device returned invalid can_have_chains")
        if (
            isinstance(latency_in_samples, bool)
            or not isinstance(latency_in_samples, int)
            or not 0 <= latency_in_samples <= 2147483647
        ):
            raise RuntimeError("Device returned invalid latency_in_samples")
        if (
            not isinstance(latency_in_ms, float)
            or not math.isfinite(latency_in_ms)
            or latency_in_ms < 0
        ):
            raise RuntimeError("Device returned invalid latency_in_ms")
        return {
            "name": LomHandler._lom_string(device.name, "Device name"),
            "class_display_name": LomHandler._lom_string(
                device.class_display_name, "Device class_display_name"
            ),
            "class_name": LomHandler._lom_string(device.class_name, "Device class_name"),
            "type": device_type,
            "is_active": is_active,
            "can_have_chains": can_have_chains,
            "latency_in_samples": latency_in_samples,
            "latency_in_ms": latency_in_ms,
        }

    @staticmethod
    def _device_chain(track: Any) -> tuple[Any, ...]:
        """Return Track.devices without its separately exposed mixer device."""
        devices = tuple(track.devices)
        try:
            mixer = track.mixer_device
        except AttributeError:
            # Test doubles and older private host wrappers may already expose
            # only the insertable chain.
            return devices
        chain = []
        for device in devices:
            try:
                is_mixer = device is mixer or device == mixer
            except Exception:
                is_mixer = device is mixer
            if not is_mixer:
                chain.append(device)
        return tuple(chain)

    @classmethod
    def _track_like_snapshot(
        cls, track: Any, *, crossfade_assign_available: bool
    ) -> dict[str, Any]:
        return {
            "name": cls._lom_string(track.name, "Track name"),
            "devices": [cls._device_snapshot(device) for device in cls._device_chain(track)],
            "mixer": cls._mixer_snapshot(
                track.mixer_device, crossfade_assign_available=crossfade_assign_available
            ),
        }

    @staticmethod
    def _routing_dictionary(value: Any, property_name: str) -> dict[str, str]:
        """Describe one host ``RoutingType`` or ``RoutingChannel`` on the wire.

        The identifier is derived from what the route names, not from the host
        wrapper. Ableton's MxDCore uses ``hash(routing_object)``, which is only
        meaningful within one Live session and for one track's own options,
        whereas a Sunny plan names a route before its target track exists and
        applies it later. A type is identified by its category and display name,
        so "Master" the main output and a track called "Master" stay distinct;
        a channel by its display name within its type. Two advertised options
        with one identity are refused at mutation rather than chosen by order.
        """
        try:
            display_name = value.display_name
            category = value.category if "type" in property_name else None
        except AttributeError as error:
            raise RuntimeError(f"Track returned invalid {property_name} object") from error
        if type(display_name) is not str:
            raise RuntimeError(f"Track returned invalid {property_name} object")
        if category is None:
            return {"display_name": display_name, "identifier": display_name}
        if isinstance(category, bool) or not isinstance(category, int):
            raise RuntimeError(f"Track returned invalid {property_name} category")
        return {"display_name": display_name, "identifier": f"{int(category)}:{display_name}"}

    @classmethod
    def _routing_options(cls, value: Any, property_name: str) -> list[tuple[dict[str, str], Any]]:
        options = _lom_sequence(value)
        if options is None:
            raise RuntimeError(f"Track returned invalid {property_name} collection")
        return [(cls._routing_dictionary(option, property_name), option) for option in options]

    @classmethod
    def _routing_collection(cls, value: Any, property_name: str) -> dict[str, list[dict[str, str]]]:
        # The Max dictionary form, {property: [route, ...]}, is the wire shape.
        return {property_name: [route for route, _ in cls._routing_options(value, property_name)]}

    @classmethod
    def _advertised_route(cls, track: Any, property_name: str, requested: dict[str, str]) -> Any:
        """Return the one host object in ``available_*`` whose description is ``requested``."""
        matches = [
            option
            for route, option in cls._routing_options(getattr(track, property_name), property_name)
            if route == requested
        ]
        if not matches:
            raise RuntimeError(f"Requested output routing is not in {property_name}")
        if len(matches) != 1:
            raise RuntimeError(
                f"Requested output routing is ambiguous in {property_name} ({len(matches)} matches)"
            )
        return matches[0]

    @classmethod
    def _output_routing_snapshot(cls, track: Any) -> dict[str, Any]:
        selected_type = cls._routing_dictionary(track.output_routing_type, "output_routing_type")
        selected_channel = cls._routing_dictionary(
            track.output_routing_channel, "output_routing_channel"
        )
        available_types = cls._routing_collection(
            track.available_output_routing_types, "available_output_routing_types"
        )
        available_channels = cls._routing_collection(
            track.available_output_routing_channels, "available_output_routing_channels"
        )
        if selected_type not in available_types["available_output_routing_types"]:
            raise RuntimeError(
                "Track selected output_routing_type is not in available_output_routing_types"
            )
        if selected_channel not in available_channels["available_output_routing_channels"]:
            raise RuntimeError(
                "Track selected output_routing_channel is not in available_output_routing_channels"
            )
        return {
            "output_routing_type": selected_type,
            "output_routing_channel": selected_channel,
            "available_output_routing_types": available_types,
            "available_output_routing_channels": available_channels,
        }

    @classmethod
    def _set_output_routing_type(cls, track: Any, requested_type: dict[str, str]) -> dict[str, Any]:
        """Set one exact currently advertised type and return closed post-set evidence."""
        requested = {
            "display_name": requested_type["display_name"],
            "identifier": requested_type["identifier"],
        }
        before = cls._output_routing_snapshot(track)
        track.output_routing_type = cls._advertised_route(
            track, "available_output_routing_types", requested
        )
        after = cls._output_routing_snapshot(track)
        if after["output_routing_type"] != requested:
            raise RuntimeError("Track output routing type readback differs from request")
        return {
            "requested_type": requested,
            "available_output_routing_types_before": before["available_output_routing_types"],
            **after,
        }

    @classmethod
    def _set_output_routing_channel(
        cls,
        track: Any,
        expected_type: dict[str, str],
        requested_channel: dict[str, str],
    ) -> dict[str, Any]:
        """Revalidate the type, set one advertised channel, and return final route evidence."""
        expected = {
            "display_name": expected_type["display_name"],
            "identifier": expected_type["identifier"],
        }
        requested = {
            "display_name": requested_channel["display_name"],
            "identifier": requested_channel["identifier"],
        }
        before = cls._output_routing_snapshot(track)
        if before["output_routing_type"] != expected:
            raise RuntimeError("Track output routing type changed before channel mutation")
        track.output_routing_channel = cls._advertised_route(
            track, "available_output_routing_channels", requested
        )
        after = cls._output_routing_snapshot(track)
        if after["output_routing_type"] != expected:
            raise RuntimeError("Track output routing type changed during channel mutation")
        if after["output_routing_channel"] != requested:
            raise RuntimeError("Track output routing channel readback differs from request")
        return {
            "requested_type": expected,
            "requested_channel": requested,
            "output_routing_type_before": before["output_routing_type"],
            "available_output_routing_types_before": before["available_output_routing_types"],
            "available_output_routing_channels_before": before["available_output_routing_channels"],
            **after,
        }

    @classmethod
    def _input_routing_snapshot(cls, track: Any, input_capable: bool) -> dict[str, Any]:
        property_names = (
            "input_routing_type",
            "input_routing_channel",
            "available_input_routing_types",
            "available_input_routing_channels",
        )
        if not input_capable:
            return {property_name: None for property_name in property_names}

        selected_type = cls._routing_dictionary(track.input_routing_type, "input_routing_type")
        selected_channel = cls._routing_dictionary(
            track.input_routing_channel, "input_routing_channel"
        )
        available_types = cls._routing_collection(
            track.available_input_routing_types, "available_input_routing_types"
        )
        available_channels = cls._routing_collection(
            track.available_input_routing_channels, "available_input_routing_channels"
        )
        if selected_type not in available_types["available_input_routing_types"]:
            raise RuntimeError(
                "Track selected input_routing_type is not in available_input_routing_types"
            )
        if selected_channel not in available_channels["available_input_routing_channels"]:
            raise RuntimeError(
                "Track selected input_routing_channel is not in available_input_routing_channels"
            )
        return {
            "input_routing_type": selected_type,
            "input_routing_channel": selected_channel,
            "available_input_routing_types": available_types,
            "available_input_routing_channels": available_channels,
        }

    @staticmethod
    def _track_meter_snapshot(
        track: Any, audio_or_midi_track: bool, has_audio_output: bool
    ) -> dict[str, Any]:
        hold_property_names = ("input_meter_level", "output_meter_level")
        momentary_property_names = (
            "input_meter_left",
            "input_meter_right",
            "output_meter_left",
            "output_meter_right",
        )
        result: dict[str, Any] = {}
        for property_name in hold_property_names:
            if not audio_or_midi_track:
                result[property_name] = None
                continue
            value = getattr(track, property_name)
            if type(value) is not float or not math.isfinite(value) or not 0.0 <= value <= 1.0:
                raise RuntimeError(f"Track returned invalid {property_name} state")
            result[property_name] = value
        for property_name in momentary_property_names:
            if not has_audio_output:
                result[property_name] = None
                continue
            value = getattr(track, property_name)
            if type(value) is not float or not math.isfinite(value) or not 0.0 <= value <= 1.0:
                raise RuntimeError(f"Track returned invalid {property_name} state")
            result[property_name] = value
        return result

    @classmethod
    def _return_track_snapshot(cls, track: Any) -> dict[str, Any]:
        result = cls._track_like_snapshot(track, crossfade_assign_available=True)
        result.update(cls._output_routing_snapshot(track))
        for property_name in ("mute", "solo", "muted_via_solo"):
            value = getattr(track, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Return Track returned invalid {property_name} state")
            result[property_name] = value
        return result

    @staticmethod
    def _mixer_parameter_snapshot(parameter: Any) -> dict[str, Any]:
        value = parameter.value
        display_value = parameter.display_value
        minimum = parameter.min
        maximum = parameter.max
        is_quantized = parameter.is_quantized
        state = parameter.state
        automation_state = parameter.automation_state
        is_enabled = parameter.is_enabled
        if not isinstance(value, float) or not math.isfinite(value):
            raise RuntimeError("Mixer DeviceParameter returned invalid value")
        if not isinstance(display_value, float) or not math.isfinite(display_value):
            raise RuntimeError("Mixer DeviceParameter returned invalid display_value")
        if not isinstance(minimum, float) or not math.isfinite(minimum):
            raise RuntimeError("Mixer DeviceParameter returned invalid minimum")
        if not isinstance(maximum, float) or not math.isfinite(maximum) or maximum < minimum:
            raise RuntimeError("Mixer DeviceParameter returned invalid maximum")
        if value < minimum or value > maximum:
            raise RuntimeError("Mixer DeviceParameter value is outside its reported range")
        if not isinstance(is_quantized, bool):
            raise RuntimeError("Mixer DeviceParameter returned invalid is_quantized state")
        if isinstance(state, bool) or not isinstance(state, int) or state not in (0, 1, 2):
            raise RuntimeError("Mixer DeviceParameter returned invalid state")
        if (
            isinstance(automation_state, bool)
            or not isinstance(automation_state, int)
            or automation_state not in (0, 1, 2)
        ):
            raise RuntimeError("Mixer DeviceParameter returned invalid automation_state")
        if not isinstance(is_enabled, bool):
            raise RuntimeError("Mixer DeviceParameter returned invalid is_enabled state")
        default_value, value_items = LomHandler._device_parameter_domain(
            parameter, is_quantized, minimum, maximum, "Mixer DeviceParameter"
        )
        return {
            "value": value,
            "display_value": display_value,
            "minimum": minimum,
            "maximum": maximum,
            "is_quantized": is_quantized,
            "default_value": default_value,
            "value_items": value_items,
            "state": state,
            "automation_state": automation_state,
            "is_enabled": is_enabled,
        }

    @classmethod
    def _mixer_snapshot(cls, mixer: Any, *, crossfade_assign_available: bool) -> dict[str, Any]:
        panning_mode = mixer.panning_mode
        if (
            isinstance(panning_mode, bool)
            or not isinstance(panning_mode, int)
            or panning_mode not in (0, 1)
        ):
            raise RuntimeError("MixerDevice returned invalid panning_mode")
        crossfade_assign = None
        if crossfade_assign_available:
            crossfade_assign = mixer.crossfade_assign
            if (
                isinstance(crossfade_assign, bool)
                or not isinstance(crossfade_assign, int)
                or crossfade_assign not in (0, 1, 2)
            ):
                raise RuntimeError("MixerDevice returned invalid crossfade_assign")
        return {
            "volume": cls._mixer_parameter_snapshot(mixer.volume),
            "track_activator": cls._mixer_parameter_snapshot(mixer.track_activator),
            "panning": cls._mixer_parameter_snapshot(mixer.panning),
            "sends": [cls._mixer_parameter_snapshot(parameter) for parameter in mixer.sends],
            "crossfade_assign": crossfade_assign,
            "panning_mode": panning_mode,
        }

    @classmethod
    def _clip_slot_snapshot(cls, slot: Any, slot_index: int) -> dict[str, Any]:
        state = {}
        for property_name in (
            "has_clip",
            "has_stop_button",
            "is_group_slot",
            "controls_other_clips",
            "is_playing",
            "is_recording",
            "is_triggered",
            "will_record_on_start",
        ):
            value = getattr(slot, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"ClipSlot returned invalid {property_name} state")
            state[property_name] = value
        playing_status = slot.playing_status
        if (
            isinstance(playing_status, bool)
            or not isinstance(playing_status, int)
            or not 0 <= playing_status <= 2
        ):
            raise RuntimeError("ClipSlot returned invalid playing_status")
        if state["is_playing"] != (playing_status != 0):
            raise RuntimeError("ClipSlot returned incoherent is_playing state")
        if state["is_recording"] != (playing_status == 2):
            raise RuntimeError("ClipSlot returned incoherent is_recording state")
        if not state["is_group_slot"] and (playing_status != 0 or state["controls_other_clips"]):
            raise RuntimeError("ClipSlot returned incoherent non-group state")
        return {"slot": slot_index, **state, "playing_status": playing_status}

    @classmethod
    def _track_snapshot(
        cls,
        track: Any,
        tracks: list[Any] | tuple[Any, ...],
        live_11_content_state_available: bool,
        take_lane_state_available: bool,
    ) -> dict[str, Any]:
        result = cls._track_like_snapshot(track, crossfade_assign_available=True)
        arrangement_clip_count = None
        take_lane_count = None
        if live_11_content_state_available:
            arrangement_clips = _lom_sequence(track.arrangement_clips)
            if arrangement_clips is None:
                raise RuntimeError("Track returned invalid arrangement_clips collection")
            arrangement_clip_count = len(arrangement_clips)
        # Live 11 has take lanes in its UI, but this adapter only reads their
        # Python API topology on Live 12+. Null means unobserved, not absent.
        if take_lane_state_available:
            take_lanes = _lom_sequence(track.take_lanes)
            if take_lanes is None:
                raise RuntimeError("Track returned invalid take_lanes collection")
            take_lane_count = len(take_lanes)
        clip_slots = [
            cls._clip_slot_snapshot(slot, slot_index)
            for slot_index, slot in enumerate(track.clip_slots)
        ]
        clips = []
        for slot_state, slot in zip(clip_slots, track.clip_slots):
            if not slot_state["has_clip"]:
                continue
            slot_index = slot_state["slot"]
            clip = slot.clip
            has_envelopes = clip.has_envelopes
            if not isinstance(has_envelopes, bool):
                raise RuntimeError("Clip returned invalid has_envelopes state")
            clip_state = {}
            for property_name in (
                "is_audio_clip",
                "is_midi_clip",
                "is_arrangement_clip",
                "looping",
                "muted",
            ):
                value = getattr(clip, property_name)
                if not isinstance(value, bool):
                    raise RuntimeError(f"Clip returned invalid {property_name} state")
                clip_state[property_name] = value
            if clip_state["is_audio_clip"] == clip_state["is_midi_clip"]:
                raise RuntimeError("Clip returned incoherent audio/MIDI identity")
            if clip_state["is_arrangement_clip"]:
                raise RuntimeError("ClipSlot returned an Arrangement Clip")
            runtime_state = {}
            for property_name in (
                "is_playing",
                "is_recording",
                "is_overdubbing",
                "is_triggered",
                "will_record_on_start",
            ):
                value = getattr(clip, property_name)
                if not isinstance(value, bool):
                    raise RuntimeError(f"Clip returned invalid {property_name} state")
                runtime_state[property_name] = value
            has_groove = None
            launch_mode = None
            launch_quantization = None
            legato = None
            velocity_amount = None
            is_session_clip = None
            is_take_lane_clip = None
            if take_lane_state_available:
                is_take_lane_clip = clip.is_take_lane_clip
                if not isinstance(is_take_lane_clip, bool):
                    raise RuntimeError("Clip returned invalid Live 12+ take-lane identity")
                if is_take_lane_clip:
                    raise RuntimeError("ClipSlot returned a Take Lane Clip")
            if live_11_content_state_available:
                is_session_clip = clip.is_session_clip
                if not isinstance(is_session_clip, bool):
                    raise RuntimeError("Clip returned invalid Live 11+ location identity")
                if not is_session_clip:
                    raise RuntimeError("ClipSlot returned a non-Session Clip")
                has_groove = clip.has_groove
                if not isinstance(has_groove, bool):
                    raise RuntimeError("Clip returned invalid has_groove state")
                launch_mode = clip.launch_mode
                launch_quantization = clip.launch_quantization
                legato = clip.legato
                velocity_amount = clip.velocity_amount
                if (
                    isinstance(launch_mode, bool)
                    or not isinstance(launch_mode, int)
                    or not 0 <= launch_mode <= 3
                ):
                    raise RuntimeError("Clip returned invalid launch_mode")
                if (
                    isinstance(launch_quantization, bool)
                    or not isinstance(launch_quantization, int)
                    or not 0 <= launch_quantization <= 14
                ):
                    raise RuntimeError("Clip returned invalid launch_quantization")
                if not isinstance(legato, bool):
                    raise RuntimeError("Clip returned invalid legato state")
                if (
                    not isinstance(velocity_amount, float)
                    or not math.isfinite(velocity_amount)
                    or not 0.0 <= velocity_amount <= 1.0
                ):
                    raise RuntimeError("Clip returned invalid velocity_amount")
            start_marker = cls._lom_float(clip.start_marker, "Clip start_marker")
            end_marker = cls._lom_float(clip.end_marker, "Clip end_marker")
            end_time = cls._lom_float(clip.end_time, "Clip end_time")
            if end_marker < start_marker:
                raise RuntimeError("Clip returned inverted start/end markers")
            if not clip_state["looping"] and end_time != end_marker:
                raise RuntimeError("Clip returned incoherent unlooped end_time")
            clips.append(
                {
                    "slot": slot_index,
                    "name": cls._lom_string(clip.name, "Clip name"),
                    "is_audio_clip": clip_state["is_audio_clip"],
                    "is_midi_clip": clip_state["is_midi_clip"],
                    "is_arrangement_clip": clip_state["is_arrangement_clip"],
                    "is_session_clip": is_session_clip,
                    "is_take_lane_clip": is_take_lane_clip,
                    "length": cls._lom_float(clip.length, "Clip length"),
                    "signature_numerator": cls._lom_integer(
                        clip.signature_numerator, "Clip signature_numerator"
                    ),
                    "signature_denominator": cls._lom_integer(
                        clip.signature_denominator, "Clip signature_denominator"
                    ),
                    "start_marker": start_marker,
                    "end_marker": end_marker,
                    "end_time": end_time,
                    "looping": clip_state["looping"],
                    "muted": clip_state["muted"],
                    "has_envelopes": has_envelopes,
                    **runtime_state,
                    "launch_mode": launch_mode,
                    "launch_quantization": launch_quantization,
                    "legato": legato,
                    "velocity_amount": velocity_amount,
                    "has_groove": has_groove,
                }
            )
        group_index = None
        is_grouped = track.is_grouped
        if not isinstance(is_grouped, bool):
            raise RuntimeError("Track returned invalid is_grouped state")
        if is_grouped:
            group = track.group_track
            group_index = next(
                (index for index, candidate in enumerate(tracks) if candidate == group), None
            )
            if group_index is None:
                raise RuntimeError("Grouped track does not resolve within Song.tracks")
        is_frozen = track.is_frozen
        if not isinstance(is_frozen, bool):
            raise RuntimeError("Track returned invalid is_frozen state")
        arm = track.arm
        implicit_arm = track.implicit_arm
        if not isinstance(arm, bool):
            raise RuntimeError("Track returned invalid arm state")
        if not isinstance(implicit_arm, bool):
            raise RuntimeError("Track returned invalid implicit_arm state")
        input_state = {}
        for property_name in ("has_audio_input", "has_midi_input"):
            value = getattr(track, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Track returned invalid {property_name} state")
            input_state[property_name] = value
        audio_or_midi_track = any(input_state.values())
        output_state = {}
        for property_name in ("has_audio_output", "has_midi_output"):
            value = getattr(track, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Track returned invalid {property_name} state")
            output_state[property_name] = value
        back_to_arranger = track.back_to_arranger
        if not isinstance(back_to_arranger, bool):
            raise RuntimeError("Track returned invalid back_to_arranger state")
        slot_state = {}
        for property_name in ("fired_slot_index", "playing_slot_index"):
            value = getattr(track, property_name)
            if (
                isinstance(value, bool)
                or not isinstance(value, int)
                or value < -2
                or value >= len(track.clip_slots)
            ):
                raise RuntimeError(f"Track returned invalid {property_name} state")
            slot_state[property_name] = value
        gate_state = {}
        for property_name in ("mute", "solo", "muted_via_solo"):
            value = getattr(track, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Track returned invalid {property_name} state")
            gate_state[property_name] = value
        result.update(
            {
                "clip_slot_count": len(track.clip_slots),
                "arrangement_clip_count": arrangement_clip_count,
                "take_lane_count": take_lane_count,
                "clip_slots": clip_slots,
                "clips": clips,
                "group_track_index": group_index,
                **cls._input_routing_snapshot(track, audio_or_midi_track),
                **cls._track_meter_snapshot(
                    track, audio_or_midi_track, output_state["has_audio_output"]
                ),
                **cls._output_routing_snapshot(track),
                **input_state,
                **output_state,
                "is_frozen": is_frozen,
                "arm": arm,
                "implicit_arm": implicit_arm,
                "back_to_arranger": back_to_arranger,
                **slot_state,
                **gate_state,
            }
        )
        return result

    @staticmethod
    def _scene_snapshot(scene: Any) -> dict[str, Any]:
        state = {}
        for property_name in ("is_triggered", "tempo_enabled", "time_signature_enabled"):
            value = getattr(scene, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Scene returned invalid {property_name} state")
            state[property_name] = value
        return {
            "name": LomHandler._lom_string(scene.name, "Scene name"),
            "is_triggered": state["is_triggered"],
            "tempo_enabled": state["tempo_enabled"],
            "tempo": LomHandler._lom_float(scene.tempo, "Scene tempo"),
            "time_signature_enabled": state["time_signature_enabled"],
            "time_signature_numerator": LomHandler._lom_integer(
                scene.time_signature_numerator, "Scene time_signature_numerator"
            ),
            "time_signature_denominator": LomHandler._lom_integer(
                scene.time_signature_denominator, "Scene time_signature_denominator"
            ),
        }

    @staticmethod
    def _scale_snapshot(song: Any) -> dict[str, Any]:
        """Return the closed Song scale tuple documented by the current LOM."""
        root_note = song.root_note
        intervals = _lom_sequence(song.scale_intervals)
        scale_mode = song.scale_mode
        if (
            isinstance(root_note, bool)
            or not isinstance(root_note, int)
            or not 0 <= root_note <= 11
        ):
            raise RuntimeError("Song returned an invalid scale root note")
        if (
            intervals is None
            or not intervals
            or len(intervals) > 128
            or any(isinstance(value, bool) or not isinstance(value, int) for value in intervals)
        ):
            raise RuntimeError("Song returned invalid scale intervals")
        if not isinstance(scale_mode, bool):
            raise RuntimeError("Song returned an invalid scale mode")
        return {
            "root_note": int(root_note),
            "name": LomHandler._lom_string(song.scale_name, "Song scale_name"),
            "intervals": [int(value) for value in intervals],
            "mode": scale_mode,
        }

    @staticmethod
    def _opaque_tuning_value(value: Any, property_name: str) -> Any:
        """Retain a documented tuning dictionary without inventing member semantics."""
        if value is None or type(value) in (bool, str):
            return value
        if type(value) is int:
            if not -(1 << 63) <= value <= (1 << 64) - 1:
                raise RuntimeError(f"TuningSystem returned invalid {property_name}")
            return value
        if type(value) is float:
            if not math.isfinite(value):
                raise RuntimeError(f"TuningSystem returned invalid {property_name}")
            return value
        if type(value) is dict:
            if any(type(key) is not str for key in value):
                raise RuntimeError(f"TuningSystem returned invalid {property_name}")
            return {
                key: LomHandler._opaque_tuning_value(item, property_name)
                for key, item in value.items()
            }
        if type(value) in (list, tuple):
            return [LomHandler._opaque_tuning_value(item, property_name) for item in value]
        raise RuntimeError(f"TuningSystem returned invalid {property_name}")

    @classmethod
    def _opaque_tuning_dictionary(cls, value: Any, property_name: str) -> dict[str, Any]:
        if type(value) is not dict:
            raise RuntimeError(f"TuningSystem returned invalid {property_name} dictionary")
        result = cls._opaque_tuning_value(value, property_name)
        assert isinstance(result, dict)
        return result

    @classmethod
    def _tuning_snapshot(cls, song: Any) -> dict[str, Any]:
        """Retain documented TuningSystem state without guessing dictionary members."""
        tuning = song.tuning_system
        name = tuning.name
        pseudo_octave = tuning.pseudo_octave_in_cents
        if type(name) is not str:
            raise RuntimeError("TuningSystem returned an invalid name")
        if (
            type(pseudo_octave) is not float
            or not math.isfinite(pseudo_octave)
            or pseudo_octave <= 0
        ):
            raise RuntimeError("TuningSystem returned an invalid pseudo octave")
        note_tunings = cls._opaque_tuning_dictionary(tuning.note_tunings, "note_tunings")
        if len(note_tunings) != 1:
            raise RuntimeError("TuningSystem returned invalid note_tunings dictionary")
        relative_tunings = next(iter(note_tunings.values()))
        if type(relative_tunings) is not list or any(
            not _finite_number(value) for value in relative_tunings
        ):
            raise RuntimeError("TuningSystem returned invalid note_tunings dictionary")
        return {
            "name": name,
            "pseudo_octave_in_cents": pseudo_octave,
            "lowest_note": cls._opaque_tuning_dictionary(tuning.lowest_note, "lowest_note"),
            "highest_note": cls._opaque_tuning_dictionary(tuning.highest_note, "highest_note"),
            "reference_pitch": cls._opaque_tuning_dictionary(
                tuning.reference_pitch, "reference_pitch"
            ),
            "note_tunings": note_tunings,
        }

    def _target_snapshot(self) -> dict[str, Any]:
        """Observe plan-relevant Set structure in one synchronous main-thread call.

        Live exposes no snapshot transaction or topology lock. The returned
        value is therefore an optimistic precondition assembled sequentially
        during this call, not proof that the Set cannot change immediately
        before or after it.
        """
        song = self._get_song()
        profile = self._target_profile()
        version = profile["live"]["version"]
        live_version = (version["major"], version["minor"], version["bugfix"])
        tracks = song.tracks
        runtime_state = {}
        for property_name in (
            "is_playing",
            "is_counting_in",
            "arrangement_overdub",
            "overdub",
            "record_mode",
            "session_record",
            "session_automation_record",
            "is_ableton_link_enabled",
            "is_ableton_link_start_stop_sync_enabled",
            "tempo_follower_enabled",
            "nudge_down",
            "nudge_up",
            "back_to_arranger",
            "re_enable_automation_enabled",
            "loop",
            "metronome",
        ):
            value = getattr(song, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Song returned invalid {property_name} state")
            runtime_state[property_name] = value
        return {
            "schema_version": TARGET_SNAPSHOT_SCHEMA_VERSION,
            "target_profile": profile,
            "song": {
                "tempo": self._lom_float(song.tempo, "Song tempo"),
                "signature_numerator": self._lom_integer(
                    song.signature_numerator, "Song signature_numerator"
                ),
                "signature_denominator": self._lom_integer(
                    song.signature_denominator, "Song signature_denominator"
                ),
                **runtime_state,
                "scale": self._scale_snapshot(song) if live_version >= (12, 0, 5) else None,
                "tuning_system": (
                    self._tuning_snapshot(song) if live_version >= (12, 1, 0) else None
                ),
                "scene_count": len(song.scenes),
                "scenes": [self._scene_snapshot(scene) for scene in song.scenes],
                "tracks": [
                    self._track_snapshot(
                        track, tracks, live_version >= (11, 0, 0), live_version >= (12, 0, 0)
                    )
                    for track in tracks
                ],
                "return_tracks": [
                    self._return_track_snapshot(track) for track in song.return_tracks
                ],
                "master_track": self._track_like_snapshot(
                    song.master_track, crossfade_assign_available=False
                ),
                "cue_points": [
                    {
                        "name": self._lom_string(cue.name, "CuePoint name"),
                        "time": self._lom_float(cue.time, "CuePoint time"),
                    }
                    for cue in song.cue_points
                ],
            },
        }

    @classmethod
    def _insert_device(
        cls, track: Any, device_name: str, target_index: int | None = None
    ) -> dict[str, Any]:
        """Insert one native device and return closed structural/state evidence."""
        before_count = len(cls._device_chain(track))
        requested_index = before_count if target_index is None else target_index
        if requested_index > before_count:
            raise RuntimeError("Device insertion index exceeds the current device-chain length")

        if target_index is None:
            track.insert_device(device_name)
        else:
            track.insert_device(device_name, target_index)

        devices = cls._device_chain(track)
        after_count = len(devices)
        if after_count != before_count + 1 or requested_index >= after_count:
            raise RuntimeError("Device insertion did not produce the exact requested count/index")
        device = devices[requested_index]
        observed = cls._device_snapshot(device)
        track_outputs = {}
        for property_name in ("has_audio_output", "has_midi_output"):
            value = getattr(track, property_name)
            if not isinstance(value, bool):
                raise RuntimeError(f"Track returned invalid {property_name} state")
            track_outputs[f"track_{property_name}"] = value
        return {
            "requested_name": device_name,
            "requested_index": requested_index,
            "before_count": before_count,
            "after_count": after_count,
            "device_index": requested_index,
            "name": observed["name"],
            "class_display_name": observed["class_display_name"],
            "class_name": observed["class_name"],
            "type": observed["type"],
            "is_active": observed["is_active"],
            "can_have_chains": observed["can_have_chains"],
            "latency_in_samples": observed["latency_in_samples"],
            "latency_in_ms": observed["latency_in_ms"],
            **track_outputs,
        }

    @staticmethod
    def _set_cue(song: Any, beat: float, label: str) -> dict[str, Any]:
        """Create or rename a cue without the toggle/delete ambiguity of LOM.

        ``Song.set_or_delete_cue`` only acts at ``current_song_time`` and takes
        no arguments. This adapter preserves the playhead and updates an
        existing cue at the requested time instead of deleting it.
        """
        beat = float(beat)
        if type(label) is not str:
            raise RuntimeError("Cue label must be a string")
        for cue in song.cue_points:
            cue_time = LomHandler._lom_float(cue.time, "CuePoint time")
            if abs(cue_time - beat) < 1e-7:
                cue.name = label
                return {
                    "action": "updated",
                    "requested_time": beat,
                    "observed_time": LomHandler._lom_float(cue.time, "CuePoint time"),
                    "requested_name": label,
                    "observed_name": LomHandler._lom_string(cue.name, "CuePoint name"),
                }

        previous_time = song.current_song_time
        try:
            song.current_song_time = beat
            song.set_or_delete_cue()
        finally:
            song.current_song_time = previous_time

        for cue in song.cue_points:
            cue_time = LomHandler._lom_float(cue.time, "CuePoint time")
            if abs(cue_time - beat) < 1e-7:
                cue.name = label
                return {
                    "action": "created",
                    "requested_time": beat,
                    "observed_time": LomHandler._lom_float(cue.time, "CuePoint time"),
                    "requested_name": label,
                    "observed_name": LomHandler._lom_string(cue.name, "CuePoint name"),
                }
        raise RuntimeError(f"Ableton did not create a cue at beat {beat}")

    @staticmethod
    def _resolve_device_parameter(device: Any, parameter_name: str) -> Any:
        """Resolve one parameter by exact public or original name, never list order."""
        if type(parameter_name) is not str or not parameter_name:
            raise RuntimeError("Device parameter name must not be empty")
        matches = [
            parameter
            for parameter in device.parameters
            if parameter.name == parameter_name or parameter.original_name == parameter_name
        ]
        if not matches:
            raise RuntimeError(f"Device parameter '{parameter_name}' was not found")
        if len(matches) != 1:
            raise RuntimeError(
                f"Device parameter '{parameter_name}' is ambiguous ({len(matches)} matches)"
            )
        return matches[0]

    @staticmethod
    def _same_live_object(first: Any, second: Any) -> bool:
        # Live may supply distinct Python wrappers for one native identity.
        return first is second or first == second

    @staticmethod
    def _envelope_valid(envelope: Any) -> bool:
        # Pinned ableton.v2.base.liveobj_valid uses native equality with None
        # to reject invalid Live wrappers, not just Python identity.
        return envelope != None  # noqa: E711

    @classmethod
    def _step_clip_interval(cls, clip: Any, *, idle: bool) -> float:
        for name, expected in (
            ("is_session_clip", True),
            ("is_arrangement_clip", False),
            ("is_audio_clip", False),
            ("is_midi_clip", True),
            ("looping", False),
        ):
            if getattr(clip, name) is not expected:
                raise RuntimeError(
                    f"Step envelopes require a nonlooping MIDI Session clip ({name})"
                )
        start = cls._lom_float(clip.start_marker, "Step envelope clip start")
        end = cls._lom_float(clip.end_marker, "Step envelope clip end")
        if start != 0.0 or end <= start:
            raise RuntimeError("Step envelopes require the generated marker interval [0, clip_end)")
        if idle:
            for name in (
                "is_playing",
                "is_recording",
                "is_overdubbing",
                "is_triggered",
                "will_record_on_start",
            ):
                if getattr(clip, name) is not False:
                    raise RuntimeError(f"Step envelope authoring requires an idle clip ({name})")
        return end

    def _step_envelope_target(
        self, path: str, clip: Any, selector: dict[str, Any]
    ) -> tuple[Any, Any, dict[str, Any]]:
        import Live

        track = self._resolve_path("/".join(path.split("/")[:3]))
        slot = self._resolve_path(path.rsplit("/", 1)[0])
        if (
            not isinstance(track, Live.Track.Track)
            or not isinstance(clip, Live.Clip.Clip)
            or not self._same_live_object(clip.canonical_parent, slot)
            or not self._same_live_object(slot.canonical_parent, track)
            or not self._same_live_object(slot.clip, clip)
        ):
            raise RuntimeError("Step envelope clip does not belong to the resolved target track")

        kind = selector["kind"]
        owner = track.mixer_device
        if kind == "device":
            owner = self._device_chain(track)[selector["device_index"]]
            parameter = self._resolve_device_parameter(owner, selector["parameter_name"])
        elif kind == "send":
            parameter = owner.sends[selector["send_index"]]
        else:
            parameter = getattr(owner, kind)
        if (
            not isinstance(parameter, Live.DeviceParameter.DeviceParameter)
            or not self._same_live_object(parameter.canonical_parent, owner)
            or not self._same_live_object(owner.canonical_parent, track)
        ):
            raise RuntimeError("Step envelope parameter does not belong to the target track")

        minimum = self._lom_float(parameter.min, "Step envelope parameter minimum")
        maximum = self._lom_float(parameter.max, "Step envelope parameter maximum")
        observed = self._lom_float(parameter.value, "Step envelope parameter value")
        state = _device_parameter_state(parameter.state, "state", parameter.name)
        automation_state = _device_parameter_state(
            parameter.automation_state, "automation state", parameter.name
        )
        if (
            minimum >= maximum
            or not minimum <= observed <= maximum
            or parameter.is_quantized is not False
            or parameter.is_enabled is not True
            or state != 0
        ):
            raise RuntimeError("Step envelopes require an enabled, active continuous parameter")
        domain = {
            "matched_name": self._lom_string(parameter.name, "Step envelope parameter name"),
            "original_name": self._lom_string(
                parameter.original_name, "Step envelope original name"
            ),
            "minimum": minimum,
            "maximum": maximum,
            "unit": "internal",
            "state": state,
            "automation_state": automation_state,
        }
        return track, parameter, domain

    def _authorize_step_envelope(self, track: Any, clip: Any, parameter: Any) -> None:
        # This callback runs in the same Live main-thread dispatch as the
        # mutation. It must resolve managed ownership using these native
        # identities, and must not mutate Live objects itself.
        if (
            not callable(self._envelope_authorizer)
            or self._envelope_authorizer(track, clip, parameter) is not True
        ):
            raise RuntimeError("Native envelope authoring is not authorized for these identities")

    def _author_step_envelope(self, path: str, clip: Any, lane: dict[str, Any]) -> dict[str, Any]:
        if not callable(self._envelope_authorizer):
            raise RuntimeError("Native envelope authoring is not authorized")
        track, parameter, domain = self._step_envelope_target(path, clip, lane["parameter"])
        end = self._step_clip_interval(clip, idle=True)
        if end != float(lane["clip_end"]):
            raise RuntimeError("Step envelope marker interval differs from the authored clip_end")
        if domain["automation_state"] == 2:
            raise RuntimeError("Step envelope parameter automation is overridden")
        # Validate the entire lane before the first mutation. Values are Live
        # internal units; display_value availability and physical-unit maps
        # remain separately qualified.
        steps = []
        for index, point in enumerate(lane["points"]):
            value = float(point["value"])
            if not domain["minimum"] <= value <= domain["maximum"]:
                raise RuntimeError("Step envelope value is outside the actual parameter domain")
            start = float(point["time"])
            stop = (
                float(lane["points"][index + 1]["time"]) if index + 1 < len(lane["points"]) else end
            )
            steps.append((start, stop - start, value))
        lookup = getattr(clip, "automation_envelope", None)
        create = getattr(clip, "create_automation_envelope", None)
        if not callable(lookup) or not callable(create):
            raise RuntimeError("Native Python envelope authoring API is unavailable")
        envelope = lookup(parameter)
        created = not self._envelope_valid(envelope)
        if created:
            self._authorize_step_envelope(track, clip, parameter)
            envelope = create(parameter)
        if not self._envelope_valid(envelope) or not callable(
            getattr(envelope, "insert_step", None)
        ):
            raise RuntimeError(
                "Native envelope creation/lookup did not return an editable envelope"
            )
        for start, duration, value in steps:
            self._authorize_step_envelope(track, clip, parameter)
            envelope.insert_step(start, duration, value)
        # This is an acknowledgement of returned mutation calls, not readback
        # of authored points or proof of saved/reopened persistence.
        return {
            "action": "created" if created else "updated",
            "steps_inserted": len(steps),
            "parameter": domain,
        }

    def _get_step_envelope(self, path: str, clip: Any, query: dict[str, Any]) -> dict[str, Any]:
        _, parameter, domain = self._step_envelope_target(path, clip, query["parameter"])
        end = self._step_clip_interval(clip, idle=False)
        if any(time >= end for time in query["sample_times"]):
            raise RuntimeError("Step envelope sample times must be inside the marker interval")
        lookup = getattr(clip, "automation_envelope", None)
        if not callable(lookup):
            raise RuntimeError("Native Python envelope readback API is unavailable")
        envelope = lookup(parameter)
        present = self._envelope_valid(envelope)
        samples = []
        if present:
            sample = getattr(envelope, "value_at_time", None)
            if not callable(sample):
                raise RuntimeError("Native envelope does not expose value_at_time")
            for time in query["sample_times"]:
                value = self._lom_float(sample(float(time)), "Step envelope sampled value")
                if not domain["minimum"] <= value <= domain["maximum"]:
                    raise RuntimeError(
                        "Step envelope sampled value is outside the parameter domain"
                    )
                samples.append({"time": float(time), "value": value})
        return {"has_envelope": present, "parameter": domain, "samples": samples}

    @classmethod
    def _get_device_parameter(
        cls, device: Any, parameter_name: str, value_property: str = "value"
    ) -> dict[str, Any]:
        """Observe an exact-name parameter without changing or repairing target state."""
        if type(parameter_name) is not str or type(value_property) is not str:
            raise RuntimeError("Device parameter name/property must be strings")
        if value_property not in ("value", "display_value"):
            raise RuntimeError(f"Unsupported DeviceParameter property '{value_property}'")
        parameter = cls._resolve_device_parameter(device, parameter_name)
        observed = cls._lom_float(
            getattr(parameter, value_property), f"Device parameter '{parameter_name}' value"
        )
        actual_min = cls._lom_float(parameter.min, f"Device parameter '{parameter_name}' minimum")
        actual_max = cls._lom_float(parameter.max, f"Device parameter '{parameter_name}' maximum")
        if actual_max < actual_min:
            raise RuntimeError(f"Device parameter '{parameter_name}' returned an inverted range")
        if not isinstance(parameter.is_quantized, bool) or not isinstance(
            parameter.is_enabled, bool
        ):
            raise RuntimeError(f"Device parameter '{parameter_name}' returned an invalid Boolean")
        parameter_state = _device_parameter_state(parameter.state, "state", parameter_name)
        automation_state = _device_parameter_state(
            parameter.automation_state, "automation state", parameter_name
        )
        default_value, value_items = cls._device_parameter_domain(
            parameter,
            parameter.is_quantized,
            actual_min,
            actual_max,
            f"Device parameter '{parameter_name}'",
        )
        return {
            "matched_name": cls._lom_string(
                parameter.name, f"Device parameter '{parameter_name}' name"
            ),
            "original_name": cls._lom_string(
                parameter.original_name, f"Device parameter '{parameter_name}' original_name"
            ),
            "property": value_property,
            "observed": observed,
            "minimum": actual_min,
            "maximum": actual_max,
            "is_quantized": parameter.is_quantized,
            "default_value": default_value,
            "value_items": value_items,
            "is_enabled": parameter.is_enabled,
            "state": parameter_state,
            "automation_state": automation_state,
        }

    @classmethod
    def _set_device_parameter(
        cls,
        device: Any,
        parameter_name: str,
        value: float,
        value_property: str = "value",
        expected_min: float | None = None,
        expected_max: float | None = None,
    ) -> dict[str, Any]:
        """Set and read back an enabled parameter by its exact public LOM name."""
        if type(parameter_name) is not str or type(value_property) is not str:
            raise RuntimeError("Device parameter name/property must be strings")
        if value_property not in ("value", "display_value"):
            raise RuntimeError(f"Unsupported DeviceParameter property '{value_property}'")
        if (expected_min is None) != (expected_max is None):
            raise RuntimeError("Expected DeviceParameter range requires both min and max")
        parameter = cls._resolve_device_parameter(device, parameter_name)
        before = cls._get_device_parameter(device, parameter_name, value_property)
        if not before["is_enabled"]:
            raise RuntimeError(f"Device parameter '{parameter_name}' is disabled")
        if before["state"] == 2:
            raise RuntimeError(f"Device parameter '{parameter_name}' cannot be changed")
        actual_min = before["minimum"]
        actual_max = before["maximum"]
        if value_property == "value" and expected_min is not None:
            assert expected_max is not None
            range_scale = max(1.0, abs(float(expected_min)), abs(float(expected_max)))
            tolerance = range_scale * 1.0e-6
            if (
                abs(actual_min - float(expected_min)) > tolerance
                or abs(actual_max - float(expected_max)) > tolerance
            ):
                raise RuntimeError(
                    f"Device parameter '{parameter_name}' range "
                    f"[{actual_min}, {actual_max}] does not match expected "
                    f"[{float(expected_min)}, {float(expected_max)}]"
                )
        requested = float(value)
        setattr(parameter, value_property, requested)
        observation = cls._get_device_parameter(device, parameter_name, value_property)
        return {**observation, "requested": requested}

    @staticmethod
    def _collection_count(value: Any, property_name: str) -> int:
        items = _lom_sequence(value)
        if items is None:
            raise RuntimeError(f"Song returned invalid {property_name} collection")
        count = len(items)
        if count > 2147483647:
            raise RuntimeError(f"Song returned oversized {property_name} collection")
        return count

    @staticmethod
    def _lom_string(value: Any, property_name: str) -> str:
        """Require the exact scalar category documented by the LOM."""
        if type(value) is not str:
            raise RuntimeError(f"Live returned invalid {property_name}")
        return value

    @classmethod
    def _device_parameter_domain(
        cls,
        parameter: Any,
        is_quantized: bool,
        minimum: float,
        maximum: float,
        property_name: str,
    ) -> tuple[float | None, list[str] | None]:
        """Preserve the LOM's conditional domain without assigning label semantics."""
        if is_quantized:
            # Cycling '74 types value_items as StringVector; see issue #22.
            raw_items = _lom_sequence(parameter.value_items)
            if raw_items is None:
                raise RuntimeError(f"{property_name} returned invalid value_items")
            return None, [
                cls._lom_string(item, f"{property_name} value_items entry") for item in raw_items
            ]

        default_value = cls._lom_float(parameter.default_value, f"{property_name} default_value")
        if default_value < minimum or default_value > maximum:
            raise RuntimeError(f"{property_name} default_value is outside its reported range")
        return default_value, None

    @staticmethod
    def _lom_integer(value: Any, property_name: str) -> int:
        """Require an exact non-Boolean Python integer from the hosted LOM."""
        if type(value) is not int:
            raise RuntimeError(f"Live returned invalid {property_name}")
        return value

    @staticmethod
    def _lom_float(value: Any, property_name: str) -> float:
        """Require an exact finite Python float from the hosted LOM."""
        if type(value) is not float:
            raise RuntimeError(f"Live returned invalid {property_name}")
        if not math.isfinite(value):
            raise RuntimeError(f"Live returned non-finite {property_name}")
        return value

    @staticmethod
    def _serialise(value: Any) -> Any:
        """Convert only exact JSON-safe host values; never stringify private objects."""
        if value is None:
            return None
        if type(value) in (bool, str):
            return value
        if isinstance(value, int):
            # Live's enum properties are Boost.Python enums, which subclass int
            # (MxDCore serialises them with int()); bool is handled above.
            if not -(1 << 63) <= value <= (1 << 64) - 1:
                raise RuntimeError("Live returned an integer outside the JSON wire domain")
            return int(value)
        if type(value) is float:
            if not math.isfinite(value):
                raise RuntimeError("Live returned a non-finite floating value")
            return value
        if type(value) is dict:
            if any(type(key) is not str for key in value):
                raise RuntimeError("Live returned a dictionary with a non-string key")
            return {key: LomHandler._serialise(item) for key, item in value.items()}
        if type(value) in (list, tuple):
            return [LomHandler._serialise(v) for v in value]
        # Ableton vector/tuple types
        try:
            return [LomHandler._serialise(v) for v in value]
        except TypeError as error:
            raise RuntimeError("Live returned an unsupported private object") from error
