"""Closed, retained native authority for the compiler's legacy command algebra.

All native getters, retained identities and effects belong to Live's main
thread. Serialized paths and fingerprints describe an intent; they never
replace the actual objects captured before planning.
"""

from __future__ import annotations

import copy
import json
import uuid
from typing import Any

from .native_control import require_peer

MAX_SCOPES = 256
MAX_NODES = 65536
MAX_BYTES = 64 * 1024 * 1024
MAX_FRAME = 16 * 1024 * 1024
MIN_JOURNAL_BYTES = 8192
SCOPE_KEYS = frozenset({"schema_version", "bridge_instance", "document_token", "scope_id"})
INTENT_KEYS = SCOPE_KEYS | {"workflow_id", "operation_id", "ordinal", "graph_revision", "command"}
TOKEN_KEYS = (INTENT_KEYS - {"command"}) | {"fingerprint"}
LEGACY_METHODS = frozenset(
    {
        "sunny_legacy_scope",
        "sunny_legacy_read",
        "sunny_legacy_prepare",
        "sunny_legacy_execute",
        "sunny_legacy_operation",
        "sunny_legacy_finish",
    }
)
LEGACY_READS = frozenset(
    {
        "sunny_get_target_profile",
        "sunny_get_target_snapshot",
        "sunny_get_scene_count",
        "sunny_get_track_count",
        "sunny_get_return_track_count",
        "sunny_get_remote_log",
        "sunny_get_device_count",
        "get_notes_by_id",
        "get_notes_extended",
        "get_all_notes_extended",
        "sunny_get_step_envelope",
        "sunny_get_device_parameter",
        "sunny_resolve_native_display_value",
    }
)
_UNAVAILABLE = object()
_PROPERTIES = {
    "song": (
        "tempo",
        "signature_numerator",
        "signature_denominator",
        "is_playing",
        "session_record",
        "record_mode",
    ),
    "scene": ("name", "tempo_enabled", "time_signature_enabled"),
    "track": ("name", "mute", "solo", "arm", "implicit_arm"),
    "return_track": ("name", "mute", "solo"),
    "clip": (
        "name",
        "signature_numerator",
        "signature_denominator",
        "start_marker",
        "end_marker",
        "loop_start",
        "loop_end",
        "is_midi_clip",
        "is_audio_clip",
        "is_arrangement_clip",
        "is_session_clip",
        "is_take_lane_clip",
        "looping",
        "muted",
        "launch_mode",
        "launch_quantization",
        "legato",
        "velocity_amount",
        "groove",
        "has_envelopes",
        "is_playing",
        "is_recording",
        "is_overdubbing",
        "is_triggered",
        "will_record_on_start",
    ),
    "mixer": ("crossfade_assign", "panning_mode"),
    "parameter": (
        "name",
        "original_name",
        "value",
        "display_value",
        "min",
        "max",
        "state",
        "automation_state",
        "is_enabled",
        "is_quantized",
    ),
    "cue": ("name", "time"),
}


def _hex(value: Any, size: int = 32) -> bool:
    return type(value) is str and len(value) == size and all(c in "0123456789abcdef" for c in value)


def _uint(value: Any, *, positive: bool = False) -> bool:
    return type(value) is int and (1 if positive else 0) <= value <= 2147483647


def _scope(value: Any, *, fresh: bool = False) -> bool:
    return (
        type(value) is dict
        and SCOPE_KEYS <= set(value)
        and type(value["schema_version"]) is int
        and value["schema_version"] == 1
        and _hex(value["bridge_instance"])
        and _hex(value["document_token"])
        and (_hex(value["scope_id"]) or (fresh and value["scope_id"] is None))
    )


def command_valid(value: Any, *, readonly: bool | None = None) -> bool:
    """Validate an exact inner command against the existing closed algebra."""
    from .handler import _request_allowed

    if type(value) is not dict or set(value) != {"type", "path", "name", "args"}:
        return False
    if any(type(value[key]) is not str for key in ("type", "path", "name")):
        return False
    if type(value["args"]) is not list or value["name"].startswith(
        ("sunny_legacy_", "sunny_managed_", "sunny_ordinary_")
    ):
        return False
    if not _request_allowed(value["type"], value["path"], value["name"], value["args"]):
        return False
    read = value["type"] == "get" or (value["type"] == "call" and value["name"] in LEGACY_READS)
    return readonly is None or read == readonly


def valid_legacy_request(name: str, args: Any) -> bool:
    """Admit only the frozen schema1 methods, excluding recursive commands."""
    if name not in LEGACY_METHODS or type(args) is not list or len(args) != 1:
        return False
    value = args[0]
    if not _scope(value, fresh=name == "sunny_legacy_scope"):
        return False
    if name in {"sunny_legacy_scope", "sunny_legacy_finish"}:
        return set(value) == SCOPE_KEYS
    if name == "sunny_legacy_read":
        return (
            set(value) == SCOPE_KEYS | {"graph_revision", "command"}
            and _uint(value["graph_revision"])
            and command_valid(value["command"], readonly=True)
        )
    if set(value) != (INTENT_KEYS if name == "sunny_legacy_prepare" else TOKEN_KEYS):
        return False
    if not all(_hex(value[key]) for key in ("workflow_id", "operation_id")):
        return False
    if not _uint(value["ordinal"], positive=True) or value["ordinal"] > 4096:
        return False
    if not _uint(value["graph_revision"]):
        return False
    return (
        command_valid(value["command"], readonly=False)
        if name == "sunny_legacy_prepare"
        else _hex(value["fingerprint"], 64)
    )


def _bytes(value: Any) -> bytes:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"), allow_nan=False).encode(
        "utf-8"
    )


def _error(error: Any) -> str:
    return (
        str(error)[:2048].encode("utf-8", errors="replace")[:2048].decode("utf-8", errors="ignore")
    )


def _same(first: Any, second: Any) -> bool:
    return first is second or first == second


def _objects(value: Any, limit: int = MAX_NODES) -> tuple[Any, ...]:
    if isinstance(value, (str, bytes, bytearray, dict)) or not hasattr(type(value), "__len__"):
        raise RuntimeError("Native identity population is not a sized collection")
    if len(value) > limit:
        raise RuntimeError("Native identity population capacity exhausted")
    result = []
    for item in value:
        if len(result) >= limit:
            raise RuntimeError("Native identity population capacity exhausted")
        result.append(item)
    if len(result) != len(value):
        raise RuntimeError("Native identity population changed during observation")
    return tuple(result)


class NativeGraph:
    """Retain ordered native identities and original writable before-state."""

    def __init__(
        self,
        owner: Any,
        previous: NativeGraph | None = None,
        aliases: Any = None,
        *,
        node_limit: int = MAX_NODES,
        byte_limit: int = MAX_BYTES,
        owned_clip: str | None = None,
    ) -> None:
        self.owner = owner
        self.nodes: dict[str, dict[str, Any]] = {}
        self.cohorts: dict[str, tuple[Any, str, tuple[Any, ...]]] = {}
        self.clips: dict[str, Any] = {}
        self.routing: dict[str, dict[str, Any]] = {}
        self.parents: list[tuple[Any, Any]] = []
        self.listeners: list[tuple[Any, Any]] = []
        self.dirty: set[str] = set()
        self.reading_notes: set[str] = set()
        self.note_read_drift: set[str] = set()
        self.encoded_bytes = 0
        self.node_limit, self.byte_limit = node_limit, byte_limit
        self.claimed_nodes = 0
        self.owned_clip = owned_clip
        if node_limit < 1 or byte_limit < 1:
            raise RuntimeError("Combined retained native graph capacity exhausted")
        if byte_limit < len(_bytes({"binding": "0" * 32, "path": "song", "kind": "song"})):
            raise RuntimeError("Combined retained native graph byte capacity exhausted")
        self.previous = previous
        self.aliases = aliases or (lambda path: path)
        self.song = owner._ensure_document()
        self.song_time = previous.song_time if previous is not None else None
        try:
            application = owner._handler._get_application()
            self.live_version = tuple(
                owner._handler._lom_integer(getattr(application, name)(), "Application version")
                for name in ("get_major_version", "get_minor_version", "get_bugfix_version")
            )
            if min(self.live_version) < 0:
                raise RuntimeError("Application returned a negative version component")
            if previous is not None and self.live_version != previous.live_version:
                raise RuntimeError("Original native Application version changed")
            self._charge({"live_version": self.live_version})
            self._capture()
            if previous is None:
                self.song_time = owner._handler._lom_float(
                    self.song.current_song_time, "retained Song playhead"
                )
            self._charge({"song_time": self.song_time})
            self.verify()
        except BaseException:
            self.close()
            raise
        finally:
            # Creation uses the previous graph only while verifying aliases.
            # Retaining it or its closure would keep an unaccounted graph chain.
            self.previous = None
            self.aliases = lambda path: path

    def _charge(self, value: Any) -> None:
        size = len(_bytes(value))
        self._require_bytes(size)
        self.encoded_bytes += size

    def _require_bytes(self, size: int) -> None:
        if size < 0 or self.encoded_bytes + size > self.byte_limit:
            raise RuntimeError("Retained native before-state capacity exhausted")

    def _claim_nodes(self, count: int) -> None:
        if count < 0 or self.claimed_nodes + count > self.node_limit:
            raise RuntimeError("Combined native graph identity capacity exhausted")
        self.claimed_nodes += count

    def _add(self, path: str, obj: Any, kind: str, parent: Any = None) -> None:
        require_peer()
        self._claim_nodes(1)
        if obj == None:  # noqa: E711
            raise RuntimeError("Native graph capacity exhausted or object invalid")
        old = self.previous.nodes.get(self.aliases(path)) if self.previous is not None else None
        if old is not None and not _same(old["object"], obj):
            raise RuntimeError("Unrelated native target substituted during owned creation")
        binding = old["binding"] if old else uuid.uuid4().hex
        before = old["before"] if old else {}
        # Unchanged creation aliases share their original immutable record;
        # a replacement graph does not duplicate retained baseline content.
        self._charge({"binding": binding, "path": path, "kind": kind})
        for name in _PROPERTIES.get(kind, ()):
            if old is not None:
                continue
            # Even the smallest scalar field needs room before its getter.
            self._require_bytes(len(_bytes({name: None})) - 3)
            try:
                value = getattr(obj, name)
            except AttributeError:
                value = _UNAVAILABLE
            try:
                self._charge({name: None if value is _UNAVAILABLE else value})
            except (TypeError, ValueError):
                # Groove is an actual native handle, never a string identity.
                if name != "groove":
                    raise
                self._charge({name: "opaque_native_handle"})
            before[name] = value
        if old is not None:
            for name, value in before.items():
                if name != "notes":
                    try:
                        self._charge({name: None if value is _UNAVAILABLE else value})
                    except (TypeError, ValueError):
                        self._charge({name: "opaque_native_handle"})
        self.nodes[path] = old or {
            "object": obj,
            "binding": binding,
            "kind": kind,
            "before": before,
        }
        if parent is not None:
            if not _same(obj.canonical_parent, parent):
                raise RuntimeError("Native target parent differs from the retained graph")
            self.parents.append((obj, parent))
        if kind == "clip":
            if (
                type(obj.is_midi_clip) is not bool
                or type(obj.is_audio_clip) is not bool
                or obj.is_midi_clip == obj.is_audio_clip
            ):
                raise RuntimeError("Native Clip audio/MIDI identity is unavailable")
            if old is not None and (
                obj.is_midi_clip != before["is_midi_clip"]
                or obj.is_audio_clip != before["is_audio_clip"]
            ):
                raise RuntimeError("Unrelated native Clip kind changed during owned creation")
            if obj.is_midi_clip is False:
                # An unrelated audio Clip still belongs to the identity graph;
                # MIDI note APIs and complete audio-content authority do not.
                before["notes"] = _UNAVAILABLE
                self._charge(None)
                return

            def changed() -> None:
                self.dirty.add(path)
                if binding in self.reading_notes:
                    self.note_read_drift.add(binding)

            # Install the original content boundary before reading it. A note
            # event during a getter is sticky, including an out-of-range edit
            # or an edit subsequently undone to the same serialized values.
            self._require_bytes(2)
            obj.add_notes_listener(changed)
            self.listeners.append((obj, changed))
            if not obj.notes_has_listener(changed):
                raise RuntimeError("Native note-change listener registration failed")
            node = self.nodes[path]
            if old is None:
                entire = self.live_version >= (11, 1, 0) and callable(
                    getattr(obj, "get_all_notes_extended", None)
                )
                span = None
                if not entire:
                    span = self.owner._handler._lom_float(
                        before["end_marker"], "retained legacy Clip note range"
                    )
                    if span <= 0.0:
                        raise RuntimeError("Retained legacy Clip note range is not positive")
                node["note_boundary"] = {
                    "entire_population": entire,
                    "time_span": span,
                    "owned_creation": path == self.owned_clip,
                }
            self._charge(node["note_boundary"])
            notes = self.observe_notes(node)
            if old is not None and notes != before["notes"]:
                raise RuntimeError("Unrelated native note content changed during creation")
            self._charge(notes)
            if old is None:
                before["notes"] = notes

    def note_values(self, population: Any) -> list[dict[str, Any]]:
        """Observe bounded native records without treating host vectors as lists."""
        values = [self.owner._handler._midi_note_dictionary(note) for note in _objects(population)]
        if len({note["note_id"] for note in values}) != len(values):
            raise RuntimeError("Native note population returned duplicate identities")
        return values

    def observe_notes(self, node: Any) -> list[dict[str, Any]]:
        """Read the original whole population or explicitly incomplete fixed range."""
        clip, boundary = node["object"], node["note_boundary"]
        if boundary["entire_population"]:
            callback = clip.get_all_notes_extended
        else:
            # Live 11.0 supports this Python keyword API, not get_all_notes.
            # Markers do not imply absence of notes outside this start range.
            def callback() -> Any:
                return clip.get_notes_extended(
                    from_pitch=0, pitch_span=128, from_time=0.0, time_span=boundary["time_span"]
                )

        return self.observe_note_read(node, callback)

    def observe_note_read(self, node: Any, callback: Any) -> list[dict[str, Any]]:
        """A reentrant note event during readback invalidates that observation."""
        binding = node["binding"]
        self.reading_notes.add(binding)
        try:
            values = self.note_values(callback())
            if binding in self.note_read_drift:
                raise RuntimeError("Native note population changed during readback")
            return values
        finally:
            self.reading_notes.discard(binding)

    def _cohort(self, key: str, owner: Any, attribute: str) -> tuple[Any, ...]:
        self._require_bytes(2)
        values = _objects(
            getattr(owner, attribute),
            self.node_limit - self.claimed_nodes + (1 if attribute == "devices" else 0),
        )
        self.cohorts[key] = (owner, attribute, values)
        # Native equality may use distinct Python wrappers. Compare by actual
        # equality, never treat a hash or Python wrapper id as identity proof.
        seen: dict[int, list[Any]] = {}
        unhashable = []
        for obj in values:
            require_peer()
            try:
                bucket = seen.setdefault(hash(obj), [])
            except TypeError:
                bucket = unhashable
            if obj == None or any(_same(obj, other) for other in bucket):  # noqa: E711
                raise RuntimeError("Native identity population is invalid or ambiguous")
            bucket.append(obj)
        return values

    def _track(self, path: str, track: Any, kind: str) -> None:
        self._add(path, track, kind)
        routing = self.routing_state(track, capture=True)
        self._charge(routing["snapshot"])
        old_path = self.aliases(path)
        old_routing = self.previous.routing.get(old_path) if self.previous else None
        self.routing[path] = (
            old_routing if old_routing and self.routing_equal(old_routing, routing) else routing
        )
        mixer = track.mixer_device
        self._add(path + "/mixer_device", mixer, "mixer", track)
        for name in ("volume", "panning", "track_activator"):
            self._add(path + "/mixer_device/" + name, getattr(mixer, name), "parameter", mixer)
        for index, parameter in enumerate(self._cohort(path + "/sends", mixer, "sends")):
            self._add(f"{path}/mixer_device/sends/{index}", parameter, "parameter", mixer)
        devices = self._cohort(path + "/devices", track, "devices")
        chain = tuple(device for device in devices if not _same(device, mixer))
        for index, device in enumerate(chain):
            device_path = f"{path}/devices/{index}"
            self._add(device_path, device, "device", track)
            for pi, parameter in enumerate(
                self._cohort(device_path + "/parameters", device, "parameters")
            ):
                self._add(f"{device_path}/parameters/{pi}", parameter, "parameter", device)
        if kind == "track":
            for index, slot in enumerate(self._cohort(path + "/slots", track, "clip_slots")):
                slot_path = f"{path}/clip_slots/{index}"
                self._add(slot_path, slot, "clip_slot", track)
                if type(slot.has_clip) is not bool:
                    raise RuntimeError("Native ClipSlot occupancy is unavailable")
                clip = slot.clip if slot.has_clip else None
                self.clips[slot_path] = clip
                if clip is not None:
                    self._add(slot_path + "/clip", clip, "clip", slot)

    def _capture(self) -> None:
        self._add("song", self.song, "song")
        for name, kind in (("tracks", "track"), ("return_tracks", "return_track")):
            for index, track in enumerate(self._cohort("song/" + name, self.song, name)):
                self._track(f"song/{name}/{index}", track, kind)
        self._track("song/master_track", self.song.master_track, "master_track")
        for index, scene in enumerate(self._cohort("song/scenes", self.song, "scenes")):
            self._add(f"song/scenes/{index}", scene, "scene")
        for index, cue in enumerate(self._cohort("song/cues", self.song, "cue_points")):
            self._add(f"song/cue_points/{index}", cue, "cue")

    @property
    def identity_count(self) -> int:
        """Count routing wrappers as retained identities as well as graph nodes."""
        return len(self.nodes) + sum(
            len(state["types"]) + len(state["channels"]) for state in self.routing.values()
        )

    def routing_state(self, track: Any, *, capture: bool = False) -> dict[str, Any]:
        """Retain real advertised handles and selections before planning."""
        if capture:
            self._require_bytes(2)
        types = _objects(
            track.available_output_routing_types,
            self.node_limit - self.claimed_nodes if capture else MAX_NODES,
        )
        if capture:
            self._claim_nodes(len(types))
        channels = _objects(
            track.available_output_routing_channels,
            self.node_limit - self.claimed_nodes if capture else MAX_NODES,
        )
        if capture:
            self._claim_nodes(len(channels))
        selected_type, selected_channel = track.output_routing_type, track.output_routing_channel
        if not any(_same(selected_type, item) for item in types) or not any(
            _same(selected_channel, item) for item in channels
        ):
            raise RuntimeError("Native routing selection is outside its advertised population")
        return {
            "types": types,
            "channels": channels,
            "type": selected_type,
            "channel": selected_channel,
            "targets": tuple(getattr(item, "attached_object", None) for item in types),
            "selected_target": getattr(selected_type, "attached_object", None),
            "audio": track.has_audio_output,
            "midi": track.has_midi_output,
            "snapshot": self.owner._handler._output_routing_snapshot(track),
        }

    @staticmethod
    def routing_equal(before: Any, after: Any) -> bool:
        """Compare native handles in addition to their serialized descriptions."""
        return (
            before["snapshot"] == after["snapshot"]
            and before["audio"] == after["audio"]
            and before["midi"] == after["midi"]
            and _same(before["type"], after["type"])
            and _same(before["channel"], after["channel"])
            and _same(before["selected_target"], after["selected_target"])
            and all(
                len(before[key]) == len(after[key])
                and all(_same(a, b) for a, b in zip(before[key], after[key]))
                for key in ("types", "channels", "targets")
            )
        )

    def verify(self, *, values: bool = False, admission: bool = True) -> None:
        if admission:
            require_peer()
        if not _same(self.owner._ensure_document(), self.song):
            raise RuntimeError("Original native document changed")
        if not _same(self.song.master_track, self.nodes["song/master_track"]["object"]):
            raise RuntimeError("Native master Track substituted")
        for obj, parent in self.parents:
            if admission:
                require_peer()
            if obj == None or not _same(obj.canonical_parent, parent):  # noqa: E711
                raise RuntimeError("Retained native parent or target changed")
        for obj, attribute, expected in self.cohorts.values():
            if admission:
                require_peer()
            actual = _objects(getattr(obj, attribute))
            if len(actual) != len(expected) or any(
                not _same(a, b) for a, b in zip(actual, expected)
            ):
                raise RuntimeError("Native graph reorder, substitution or external insertion")
        for path, clip in self.clips.items():
            if admission:
                require_peer()
            slot = self.nodes[path]["object"]
            if type(slot.has_clip) is not bool or slot.has_clip != (clip is not None):
                raise RuntimeError("Native ClipSlot occupancy changed")
            if clip is not None and not _same(slot.clip, clip):
                raise RuntimeError("Native Clip substituted")
            if clip is not None:
                self.check_before(
                    self.nodes[path + "/clip"],
                    ("is_midi_clip", "is_audio_clip", "is_arrangement_clip"),
                )
        for path, expected in self.routing.items():
            if admission:
                require_peer()
            if not self.routing_equal(expected, self.routing_state(self.node(path)["object"])):
                raise RuntimeError("Original native routing population or selection changed")
        for clip, listener in self.listeners:
            if not clip.notes_has_listener(listener):
                raise RuntimeError("Original native note-change listener is unavailable")
        if self.dirty:
            raise RuntimeError("Native note population edited after scope observation")
        if values:
            for node in self.nodes.values():
                if admission:
                    require_peer()
                self.check_before(node)

    @staticmethod
    def check_before(node: Any, fields: Any = None) -> None:
        for name, expected in node["before"].items():
            if name == "notes" or (fields is not None and name not in fields):
                continue
            if expected is _UNAVAILABLE:
                if fields is not None:
                    raise RuntimeError("Original writable before-state is unavailable")
                continue
            if getattr(node["object"], name) != expected:
                raise RuntimeError("Writable native before-state changed after planning")

    def node(self, path: str) -> dict[str, Any]:
        try:
            return self.nodes[path]
        except KeyError as error:
            raise RuntimeError("Target is absent from the original native graph") from error

    def parameter(self, device_path: str, name: str) -> dict[str, Any]:
        prefix = device_path + "/parameters/"
        matches = [
            node
            for path, node in self.nodes.items()
            if path.startswith(prefix)
            and (node["before"].get("name") == name or node["before"].get("original_name") == name)
        ]
        if len(matches) != 1:
            raise RuntimeError("Retained device parameter is absent or ambiguous")
        return matches[0]

    def close(self) -> None:
        for clip, listener in self.listeners:
            try:
                if clip.notes_has_listener(listener):
                    clip.remove_notes_listener(listener)
            except (AttributeError, RuntimeError):
                pass
        self.listeners.clear()

    def encoded_size(self) -> int:
        """Measure retained baselines using the same accounting as capture."""
        size = len(_bytes({"song_time": self.song_time})) + len(
            _bytes({"live_version": self.live_version})
        )
        size += sum(len(_bytes(state["snapshot"])) for state in self.routing.values())
        for path, node in self.nodes.items():
            size += len(_bytes({"binding": node["binding"], "path": path, "kind": node["kind"]}))
            if "note_boundary" in node:
                size += len(_bytes(node["note_boundary"]))
            for name, value in node["before"].items():
                try:
                    size += len(
                        _bytes(
                            (None if value is _UNAVAILABLE else value)
                            if name == "notes"
                            else {name: None if value is _UNAVAILABLE else value}
                        )
                    )
                except (TypeError, ValueError):
                    size += len(_bytes({name: "opaque_native_handle"}))
        return size


class LegacyAuthority:
    """Scopes plus original-token journals in the shared registry ID domain."""

    def __init__(self, owner: Any) -> None:
        self.owner = owner
        self.scopes: dict[str, dict[str, Any]] = {}
        self.used_bytes = 0
        self.used_nodes = 0
        self.reserved_nodes = 0

    def close(self) -> None:
        for scope in tuple(self.scopes.values()):
            self._close_scope(scope)
        self.scopes.clear()

    def _reserve(self, count: int) -> None:
        if count < 0 or self.used_bytes + count > MAX_BYTES:
            raise RuntimeError("Combined retained legacy byte capacity exhausted")
        self.used_bytes += count

    @staticmethod
    def _node_budget(command: Any, graph: NativeGraph | None) -> int:
        """Reserve known structure and a finite supported host-population allowance."""
        tracks = len(graph.cohorts["song/tracks"][2]) if graph else 0
        scenes = len(graph.cohorts["song/scenes"][2]) if graph else 0
        returns = len(graph.cohorts["song/return_tracks"][2]) if graph else 0
        routing = len(graph.routing) if graph else 0
        return {
            "create_scene": 1 + tracks,
            "create_midi_track": 1029 + scenes + returns + routing,
            "create_return_track": 1029 + tracks + routing,
            "create_clip": 1,
            "sunny_set_cue": 1,
            "insert_device": 1537 + tracks + returns,
            "sunny_set_output_routing_type": 1024,
        }.get(command["name"], 0)

    @staticmethod
    def _graph_budget(command: Any, graph: NativeGraph | None = None) -> int:
        """Reserve a finite supported creation/readback allowance before entry."""
        name = command["name"]
        # Unexpected larger host objects yield Partial, retaining the reserved
        # journal; they never expand native authority after an uncertain call.
        structural = {
            "insert_device": 4 * 1024 * 1024,
            "create_clip": 65536,
            "sunny_set_cue": 16384,
            "create_midi_track": 1024 * 1024,
            "create_return_track": 1024 * 1024,
            "create_scene": 65536,
            "sunny_set_output_routing_type": 65536,
            "sunny_set_output_routing_channel": 65536,
        }.get(name, 0)
        if graph is not None and name in {
            "create_scene",
            "create_midi_track",
            "create_return_track",
        }:
            population = graph.cohorts[
                "song/tracks" if name != "create_midi_track" else "song/scenes"
            ][2]
            structural += len(population) * 4096
        argument_size = len(_bytes(command["args"]))
        if command["type"] == "set" or name == "sunny_set_device_parameter":
            # A validated native float readback can have more encoded digits
            # than its requested display value, including the coupled field.
            structural += 512
        if graph is not None and command["type"] == "set" and name == "name":
            structural += len(graph.routing) * argument_size * 4
        return structural + argument_size * 2

    def _settle_record(self, record: Any) -> None:
        """Replace a terminal reservation with its actual encoded evidence."""
        actual = len(_bytes(record["request"])) + len(_bytes(record["journal"]))
        if actual > record["budget"]:
            journal = record["journal"]
            journal.update(
                outcome="partial" if journal["native_mutation_started"] else "declined",
                result=None,
                diagnostic=None,
                error="Native evidence exceeded reserved terminal capacity",
            )
            actual = len(_bytes(record["request"])) + len(_bytes(journal))
        self.used_bytes -= record["budget"] - actual
        record["budget"] = actual
        self.reserved_nodes -= record.pop("node_budget", 0)

    def _update_graph_bytes(self, record: Any, graph: NativeGraph) -> None:
        actual = graph.encoded_size()
        delta = actual - graph.encoded_bytes
        if delta > record["graph_budget"]:
            raise RuntimeError("Owned baseline exceeds its pre-entry retained byte allowance")
        if delta > 0:
            record["graph_budget"] -= delta
            record["budget"] -= delta
        else:
            self.used_bytes += delta
        graph.encoded_bytes = actual

    def _update_owned_routing(self, record: Any, graph: NativeGraph) -> None:
        command = record["request"]["command"]
        changed_path = command["path"]

        def retain(path: str, after: Any) -> None:
            before = graph.routing[path]
            node_delta = (
                len(after["types"])
                + len(after["channels"])
                - len(before["types"])
                - len(before["channels"])
            )
            byte_delta = len(_bytes(after["snapshot"])) - len(_bytes(before["snapshot"]))
            if self.used_nodes + node_delta > MAX_NODES or byte_delta > record["graph_budget"]:
                raise RuntimeError("Owned routing exceeds its pre-entry retained allowance")
            if node_delta > record.get("node_budget", 0):
                raise RuntimeError("Owned routing exceeds its pre-entry identity allowance")
            if node_delta > 0:
                record["node_budget"] -= node_delta
                self.reserved_nodes -= node_delta
            if byte_delta > 0:
                record["graph_budget"] -= byte_delta
                record["budget"] -= byte_delta
            else:
                self.used_bytes += byte_delta
            self.used_nodes += node_delta
            graph.encoded_bytes += byte_delta
            graph.routing[path] = after

        if command["name"] in {"sunny_set_output_routing_type", "sunny_set_output_routing_channel"}:
            retain(changed_path, graph.routing_state(graph.node(changed_path)["object"]))
        elif (
            command["type"] == "set"
            and command["name"] == "name"
            and record["target"]["kind"] in {"track", "return_track"}
        ):
            target = record["target"]["object"]
            for path, before in tuple(graph.routing.items()):
                after = graph.routing_state(graph.node(path)["object"])
                expected = copy.deepcopy(before["snapshot"])
                for index, option in enumerate(before["types"]):
                    if _same(getattr(option, "attached_object", None), target):
                        descriptor = self.owner._handler._routing_dictionary(
                            after["types"][index], "available_output_routing_types"
                        )
                        expected["available_output_routing_types"][
                            "available_output_routing_types"
                        ][index] = descriptor
                        if _same(option, before["type"]):
                            expected["output_routing_type"] = descriptor
                compared = {**before, "snapshot": expected}
                if not graph.routing_equal(compared, after):
                    raise RuntimeError("Unrelated native routing changed during owned rename")
                retain(path, after)

    def _epoch(self, request: Any) -> bool:
        self.owner._ensure_document()
        return (
            request["bridge_instance"] == self.owner._bridge_instance
            and request["document_token"] == self.owner._document_token
        )

    @staticmethod
    def _scope_reply(
        request: Any, outcome: str, scope: Any = None, error: Any = None
    ) -> dict[str, Any]:
        return {
            **{key: request[key] for key in SCOPE_KEYS},
            "outcome": outcome,
            "scope_id": scope["id"] if scope and outcome == "ready" else request["scope_id"],
            "graph_revision": scope["revision"] if scope and outcome == "ready" else None,
            "workflow_id": scope["workflow"] if scope and outcome == "ready" else None,
            "error": _error(error) if error is not None else None,
        }

    def _close_scope(self, scope: Any) -> None:
        if scope["closed"]:
            return
        scope["closed"] = True
        scope.pop("peer", None)
        self.scopes.pop(scope["id"], None)
        graph = scope.pop("graph")
        graph.close()
        self.used_nodes -= graph.identity_count
        self.used_bytes -= graph.encoded_bytes
        for record in self.owner._operations.values():
            if record.get("legacy") and record.get("scope") is scope:
                if record["journal"]["outcome"] == "prepared":
                    record["journal"]["outcome"] = (
                        "partial" if record["journal"]["native_mutation_started"] else "declined"
                    )
                    record["journal"]["error"] = "Original native scope closed before execution"
                for key in ("scope", "target", "parameter", "peer", "extra", "original_cues"):
                    record.pop(key, None)
                self._settle_record(record)

    def _find_scope(self, request: Any) -> Any:
        scope = self.scopes.get(request["scope_id"])
        if scope is None or scope["closed"]:
            raise RuntimeError("Original native scope is unavailable or closed")
        if request.get("graph_revision", scope["revision"]) != scope["revision"]:
            raise RuntimeError("Original native graph revision differs")
        scope["graph"].verify()
        return scope

    @staticmethod
    def _journal(request: Any, outcome: str) -> dict[str, Any]:
        from .managed import _digest

        token = {key: request[key] for key in TOKEN_KEYS - {"fingerprint"}}
        token["fingerprint"] = request.get("fingerprint") or _digest(request)
        return {
            **token,
            "outcome": outcome,
            "native_mutation_started": False,
            "started_calls": 0,
            "returned_calls": 0,
            "result": None,
            "diagnostic": None,
            "error": None,
        }

    def dispatch(self, name: str, request: Any) -> dict[str, Any]:
        if not self._epoch(request):
            if name in {"sunny_legacy_scope", "sunny_legacy_finish"}:
                return self._scope_reply(request, "unknown_epoch")
            if name == "sunny_legacy_read":
                return {
                    **{key: request[key] for key in SCOPE_KEYS},
                    "graph_revision": None,
                    "outcome": "unknown_epoch",
                    "value": None,
                    "error": None,
                }
            return self._journal(request, "unknown_epoch")
        if name == "sunny_legacy_scope":
            return self._capture_scope(request)
        if name == "sunny_legacy_finish":
            require_peer()
            scope = self.scopes.get(request["scope_id"])
            if scope is None:
                return self._scope_reply(request, "unknown_scope")
            self._close_scope(scope)
            return self._scope_reply(request, "closed")
        if name == "sunny_legacy_read":
            return self._read(request)
        journal = self._journal(request, "unknown_operation")
        existing = self.owner._operations.get(request["operation_id"])
        if existing is not None:
            if not existing.get("legacy") or any(
                existing["journal"][key] != journal[key] for key in TOKEN_KEYS
            ):
                raise RuntimeError("Operation token belongs to another immutable intent or family")
            if name == "sunny_legacy_execute" and existing["journal"]["outcome"] == "prepared":
                self._execute(existing)
            return copy.deepcopy(existing["journal"])
        if name != "sunny_legacy_prepare":
            return journal
        if len(self.owner._operations) >= 4096:
            raise RuntimeError("Shared retained operation capacity exhausted")
        intent_size = len(_bytes(request))
        # Note-ID results scale with the known request; other closed helper
        # results have a finite bounded reserve. A minimal Partial record is
        # always reserved before any native entry.
        result_budget = max(16384, len(_bytes(request["command"]["args"])) * 2)
        original_graph = self.scopes.get(request["scope_id"], {}).get("graph")
        graph_budget = self._graph_budget(request["command"], original_graph)
        node_budget = self._node_budget(request["command"], original_graph)
        if self.used_nodes + self.reserved_nodes + node_budget > MAX_NODES:
            raise RuntimeError("Combined pre-entry native identity capacity exhausted")
        reservation = intent_size + result_budget + MIN_JOURNAL_BYTES + graph_budget
        self._reserve(reservation)
        self.reserved_nodes += node_budget
        journal = self._journal(request, "prepared")
        record = {
            "legacy": True,
            "request": copy.deepcopy(request),
            "journal": journal,
            "budget": reservation,
            "result_budget": result_budget,
            "graph_budget": graph_budget,
            "node_budget": node_budget,
        }
        self.owner._operations[request["operation_id"]] = record
        try:
            peer = require_peer()
            scope = self._find_scope(request)
            record["scope"] = scope
            if scope["workflow"] not in (None, request["workflow_id"]) or scope["poisoned"]:
                raise RuntimeError("Original native workflow is unavailable for further writes")
            if scope["workflow"] is not None:
                require_peer(scope["peer"])
            if request["ordinal"] != scope["next"]:
                raise RuntimeError("Native workflow ordinal differs from the next original intent")
            previous = self.owner._operations.get(scope["previous"])
            if previous is not None and previous["journal"]["outcome"] != "acknowledged":
                raise RuntimeError("Previous native workflow operation is not acknowledged")
            graph = scope["graph"]
            target = graph.node(request["command"]["path"])
            record.update(scope=scope, target=target, peer=peer, extra={})
            self._prepare(record)
            scope["workflow"] = request["workflow_id"]
            scope["peer"] = peer
            scope["next"] += 1
            scope["previous"] = request["operation_id"]
        except Exception as error:
            journal["outcome"], journal["error"] = "declined", _error(error)
            if "scope" in record:
                record["scope"]["poisoned"] = True
            for key in ("scope", "target", "parameter", "peer", "extra", "original_cues"):
                record.pop(key, None)
            self._settle_record(record)
        return copy.deepcopy(journal)

    def _capture_scope(self, request: Any) -> dict[str, Any]:
        require_peer()
        if request["scope_id"] is not None:
            scope = self.scopes.get(request["scope_id"])
            if scope is None:
                return self._scope_reply(request, "unknown_scope")
            if scope["closed"] or scope["poisoned"]:
                return self._scope_reply(request, "closed")
            try:
                scope["graph"].verify(values=True)
                return self._scope_reply(request, "ready", scope)
            except Exception as error:
                self._close_scope(scope)
                return self._scope_reply(request, "declined", error=error)
        if len(self.scopes) >= MAX_SCOPES:
            return self._scope_reply(
                request, "declined", error="Retained native scope capacity exhausted"
            )
        graph = None
        try:
            graph = NativeGraph(
                self.owner,
                node_limit=MAX_NODES - self.used_nodes - self.reserved_nodes,
                byte_limit=MAX_BYTES - self.used_bytes,
            )
            if self.used_nodes + self.reserved_nodes + graph.identity_count > MAX_NODES:
                raise RuntimeError("Combined native graph node capacity exhausted")
            self._reserve(graph.encoded_bytes)
            self.used_nodes += graph.identity_count
            scope_id = uuid.uuid4().hex
            scope = {
                "id": scope_id,
                "graph": graph,
                "revision": 0,
                "workflow": None,
                "next": 1,
                "previous": None,
                "closed": False,
                "poisoned": False,
            }
            self.scopes[scope_id] = scope
            return self._scope_reply(request, "ready", scope)
        except Exception as error:
            if graph is not None:
                graph.close()
            return self._scope_reply(request, "declined", error=error)

    def _read(self, request: Any) -> dict[str, Any]:
        reply = {
            **{key: request[key] for key in SCOPE_KEYS},
            "graph_revision": None,
            "outcome": "declined",
            "value": None,
            "error": None,
        }
        scope = self.scopes.get(request["scope_id"])
        if scope is None or scope["closed"]:
            reply["outcome"] = "unknown_scope"
            return reply
        try:
            scope = self._find_scope(request)
            value = self.owner._handler.handle_retained(request["command"], scope["graph"])
            scope["graph"].verify()
            if len(_bytes(value)) + 1024 > MAX_FRAME:
                raise RuntimeError("Scoped native read exceeds the wire byte limit")
            reply.update(outcome="observed", value=value, graph_revision=scope["revision"])
        except Exception as error:
            scope["poisoned"] = True
            reply["error"] = _error(error)
        return reply

    def _prepare(self, record: Any) -> None:
        command = record["request"]["command"]
        graph, target = record["scope"]["graph"], record["target"]
        name, args, kind = command["name"], command["args"], command["type"]
        obj = target["object"]
        if kind == "set":
            graph.check_before(target, (name,))
            if target["kind"] == "parameter":
                graph.check_before(target)
        elif name == "sunny_set_device_parameter":
            parameter = graph.parameter(command["path"], args[0])
            graph.check_before(parameter)
            record["parameter"] = parameter
        elif name == "sunny_set_cue":
            before = graph.node("song")["before"]
            matches = [
                node
                for node in graph.nodes.values()
                if node["kind"] == "cue"
                and abs(float(node["before"]["time"]) - float(args[0])) < 1e-7
            ]
            if len(matches) > 1:
                raise RuntimeError("Retained CuePoint time is ambiguous")
            cue = matches[0] if matches else None
            self._cue_flags(graph)
            if cue:
                graph.check_before(cue)
            else:
                if before["session_record"] is not False or before["record_mode"] is not False:
                    raise RuntimeError("Cue creation requires the original recording-off flags")
                if before["is_playing"] is False and not self._cue_time_equal(
                    obj.current_song_time, graph.song_time
                ):
                    raise RuntimeError(
                        "New CuePoint creation requires the original stopped playhead"
                    )
            record["extra"].update(cue=cue)
            record["original_cues"] = tuple(
                node for node in graph.nodes.values() if node["kind"] == "cue"
            )
            if cue is None:
                record["extra"].setdefault(
                    "previous_time", graph.song_time if before["is_playing"] is False else None
                )
        elif name in {"sunny_set_output_routing_type", "sunny_set_output_routing_channel"}:
            graph.check_before(target)
            requested = args[-1]
            attribute = (
                "available_output_routing_types"
                if name.endswith("_type")
                else "available_output_routing_channels"
            )
            routing = graph.routing[command["path"]]
            options = routing["types"] if name.endswith("_type") else routing["channels"]
            matches = [
                option
                for option in options
                if self.owner._handler._routing_dictionary(option, attribute) == requested
            ]
            if len(matches) != 1:
                raise RuntimeError("Original advertised routing target is absent or ambiguous")
            parameter = matches[0]
            if name.endswith("_type"):
                attached = getattr(parameter, "attached_object", None)
                if attached is not None and not any(
                    _same(attached, node["object"])
                    for node in graph.nodes.values()
                    if node["kind"] in {"track", "return_track", "master_track"}
                ):
                    raise RuntimeError("Advertised routing destination is outside retained graph")
                # A Track-naming category must expose its actual target.
                if attached is None and any(
                    node["kind"] in {"track", "return_track", "master_track"}
                    and node["object"].name == requested["display_name"]
                    for node in graph.nodes.values()
                ):
                    raise RuntimeError("Routing target association is unavailable")
            record["extra"].update(route=parameter, routing=copy.deepcopy(routing["snapshot"]))
        elif name == "create_clip":
            if obj.has_clip is not False:
                raise RuntimeError("Retained ClipSlot is not empty")
        elif name in {
            "delete_clip",
            "add_new_notes",
            "sunny_clear_all_envelopes",
            "sunny_author_step_envelope",
        }:
            clip_node = graph.node(command["path"] + "/clip") if name == "delete_clip" else target
            graph.check_before(clip_node)
            clip = clip_node["object"]
            if clip.is_midi_clip is not True or clip.is_arrangement_clip is not False:
                raise RuntimeError(
                    "Legacy destructive/note work requires the retained Session MIDI Clip"
                )
            if any(
                getattr(clip, flag) is not False
                for flag in (
                    "is_playing",
                    "is_recording",
                    "is_overdubbing",
                    "is_triggered",
                    "will_record_on_start",
                )
            ):
                raise RuntimeError("Destructive legacy Clip work requires an idle retained Clip")
            if (
                name in {"delete_clip", "sunny_clear_all_envelopes"}
                and clip.has_envelopes is not False
            ):
                raise RuntimeError("Complete existing envelope content authority is unavailable")
            boundary = clip_node["note_boundary"]
            if not boundary["entire_population"] and (
                name == "delete_clip"
                or (name != "add_new_notes" and not boundary["owned_creation"])
            ):
                raise RuntimeError("Entire original Clip note population is unavailable")
            if name == "add_new_notes" and not boundary["entire_population"]:
                if any(
                    not 0.0 <= note["start_time"] < boundary["time_span"]
                    for note in args[0]["notes"]
                ):
                    raise RuntimeError("Note insertion lies outside the retained legacy range")
            if name == "sunny_author_step_envelope":
                record["extra"]["envelope_target"] = self.owner._handler.retained_envelope_target(
                    command["path"], graph, args[0]["parameter"]
                )
        record["max_phases"] = (
            len(args[0]["points"]) + 1
            if name == "sunny_author_step_envelope"
            else 4
            if name == "sunny_set_cue" and record["extra"]["cue"] is None
            else 1
        )

    def _transition(self, record: Any) -> None:
        """Admit only the exact owned structural delta, retaining all old nodes."""
        scope, command = record["scope"], record["request"]["command"]
        old, name, path, args = scope["graph"], command["name"], command["path"], command["args"]
        permitted_dirty = {path + "/clip"} if name == "delete_clip" else set()
        if old.dirty - permitted_dirty:
            raise RuntimeError("Unrelated native note event occurred during owned creation")
        allowed: dict[str, tuple[int, int]] = {}
        removed: set[str] = set()
        clip_change: str | None = None
        if name in {"create_midi_track", "create_scene", "create_return_track"}:
            key = {
                "create_midi_track": "song/tracks",
                "create_scene": "song/scenes",
                "create_return_track": "song/return_tracks",
            }[name]
            cohort = old.cohorts[key][2]
            index = len(cohort) if not args or args[0] == -1 else args[0]
            allowed[key] = (index, 1)
            if name == "create_scene":
                for key in old.cohorts:
                    if key.endswith("/slots"):
                        allowed[key] = (index, 1)
            if name == "create_return_track":
                for key, (_, _, values) in old.cohorts.items():
                    if key.startswith("song/tracks/") and key.endswith("/sends"):
                        allowed[key] = (len(values), 1)
        elif name == "insert_device":
            raw = old.cohorts[path + "/devices"][2]
            mixer = old.node(path + "/mixer_device")["object"]
            chain = [device for device in raw if not _same(device, mixer)]
            index = len(chain) if len(args) == 1 else args[1]
            # Native Track.devices can include the mixer; the logical chain
            # index is mapped to the raw population for identity verification.
            after = _objects(record["target"]["object"].devices)
            novel = [item for item in after if not any(_same(item, member) for member in raw)]
            after_chain = [item for item in after if not _same(item, mixer)]
            if (
                len(novel) != 1
                or index >= len(after_chain)
                or not _same(after_chain[index], novel[0])
            ):
                raise RuntimeError("Owned Device did not occupy its declared logical index")
            raw_index = next(i for i, item in enumerate(after) if _same(item, novel[0]))
            allowed[path + "/devices"] = (raw_index, 1)
        elif name in {"create_clip", "delete_clip"}:
            clip_change = path
            if name == "delete_clip":
                removed.add(path + "/clip")
        elif name == "sunny_set_cue":
            cohort = old.cohorts["song/cues"][2]
            index = sum(float(cue.time) < float(args[0]) for cue in cohort)
            allowed["song/cues"] = (index, 1)

        for key, (obj, attribute, before) in old.cohorts.items():
            after = _objects(getattr(obj, attribute))
            insertion = allowed.get(key)
            if insertion:
                index, count = insertion
                if len(after) != len(before) + count:
                    raise RuntimeError(
                        "Owned creation did not produce the exact native cohort delta"
                    )
                retained = after[:index] + after[index + count :]
                if any(_same(after[index], member) for member in before):
                    raise RuntimeError("Owned creation did not produce a novel native identity")
            else:
                retained = after
            if len(retained) != len(before) or any(
                not _same(a, b) for a, b in zip(retained, before)
            ):
                raise RuntimeError("Unrelated native cohort changed during owned creation")
        for slot_path, original in old.clips.items():
            slot = old.node(slot_path)["object"]
            if slot_path == clip_change:
                if name == "create_clip" and (slot.has_clip is not True or slot.clip is None):
                    raise RuntimeError("Owned Clip creation did not occupy original Slot")
                if name == "delete_clip" and slot.has_clip is not False:
                    raise RuntimeError("Owned Clip deletion did not empty original Slot")
            elif slot.has_clip != (original is not None) or (
                original is not None and not _same(slot.clip, original)
            ):
                raise RuntimeError("Unrelated occupied Clip changed during owned creation")

        novel_tracks = []
        if name in {"create_midi_track", "create_return_track"}:
            cohort_key = "song/tracks" if name == "create_midi_track" else "song/return_tracks"
            owner, attribute, _ = old.cohorts[cohort_key]
            novel_tracks = [_objects(getattr(owner, attribute))[allowed[cohort_key][0]]]
        for track_path, before in old.routing.items():
            track = old.node(track_path)["object"]
            after = old.routing_state(track)
            if old.routing_equal(before, after):
                continue
            own_instrument = (
                name == "insert_device"
                and track_path == path
                and before["audio"] is False
                and before["midi"] is True
                and after["audio"] is True
                and after["midi"] is False
            )
            if own_instrument:
                raw = _objects(track.devices)
                inserted = raw[allowed[path + "/devices"][0]]
                import Live

                if inserted.type != Live.Device.DeviceType.instrument:
                    raise RuntimeError(
                        "Audio routing transition is not from the owned native instrument"
                    )
                attached = getattr(after["type"], "attached_object", None)
                if attached is None or not _same(attached, old.song.master_track):
                    raise RuntimeError("Owned instrument did not select the retained native master")
                continue
            # Track creation may expose only the actual new destination. All
            # previous choices, order, channel handles and selections remain.
            remaining = [
                item
                for item in after["types"]
                if not any(
                    _same(getattr(item, "attached_object", None), member) for member in novel_tracks
                )
            ]
            if (
                not novel_tracks
                or len(remaining) != len(before["types"])
                or any(not _same(a, b) for a, b in zip(remaining, before["types"]))
                or not _same(before["type"], after["type"])
                or not _same(before["channel"], after["channel"])
                or len(before["channels"]) != len(after["channels"])
                or any(not _same(a, b) for a, b in zip(before["channels"], after["channels"]))
                or before["audio"] != after["audio"]
                or before["midi"] != after["midi"]
            ):
                raise RuntimeError("Unrelated native routing changed during owned creation")

        def old_path(new_path: str) -> str:
            parts = new_path.split("/")
            if name in {"create_midi_track", "create_scene"}:
                population = "tracks" if name == "create_midi_track" else "scenes"
                if len(parts) > 2 and parts[1] == population:
                    position = int(parts[2])
                    insertion = allowed["song/" + population][0]
                    if position == insertion:
                        return "new/" + new_path
                    if position > insertion:
                        parts[2] = str(position - 1)
                if name == "create_scene" and "clip_slots" in parts:
                    pi = parts.index("clip_slots") + 1
                    position, insertion = int(parts[pi]), allowed["song/scenes"][0]
                    if position == insertion:
                        return "new/" + new_path
                    if position > insertion:
                        parts[pi] = str(position - 1)
            if name == "insert_device" and new_path.startswith(path + "/devices/"):
                pi = len(path.split("/")) + 1
                insertion = (
                    len(
                        [
                            n
                            for p, n in old.nodes.items()
                            if p.startswith(path + "/devices/") and n["kind"] == "device"
                        ]
                    )
                    if len(args) == 1
                    else args[1]
                )
                position = int(parts[pi])
                if position == insertion:
                    return "new/" + new_path
                if position > insertion:
                    parts[pi] = str(position - 1)
            if name == "sunny_set_cue" and len(parts) > 2 and parts[1] == "cue_points":
                position, insertion = int(parts[2]), allowed["song/cues"][0]
                if position == insertion:
                    return "new/" + new_path
                if position > insertion:
                    parts[2] = str(position - 1)
            return "/".join(parts)

        candidate = NativeGraph(
            self.owner,
            old,
            old_path,
            node_limit=old.identity_count + record.get("node_budget", 0),
            byte_limit=old.encoded_bytes + record["graph_budget"],
            owned_clip=path + "/clip" if name == "create_clip" else None,
        )
        try:
            if old.dirty - permitted_dirty:
                raise RuntimeError(
                    "Unrelated native note event occurred during candidate observation"
                )
            candidate_nodes = candidate.identity_count
            node_delta = candidate_nodes - old.identity_count
            if self.used_nodes + node_delta > MAX_NODES or node_delta > record.get(
                "node_budget", 0
            ):
                raise RuntimeError("Owned creation exceeds combined native identity capacity")
            delta = candidate.encoded_bytes - old.encoded_bytes
            if delta > 0:
                if delta > record["graph_budget"]:
                    raise RuntimeError(
                        "Owned creation exceeds its pre-entry retained byte allowance"
                    )
                record["graph_budget"] -= delta
                record["budget"] -= delta
            else:
                self.used_bytes += delta
            old_bindings = {node["binding"] for node in old.nodes.values()}
            created = [
                dict(
                    binding_token=node["binding"],
                    kind=(
                        "mixer_parameter"
                        if node["kind"] == "parameter" and "/mixer_device/" in p
                        else "device_parameter"
                        if node["kind"] == "parameter"
                        else node["kind"]
                    ),
                    path=p,
                )
                for p, node in candidate.nodes.items()
                if node["binding"] not in old_bindings
                and node["kind"]
                in {
                    "track",
                    "return_track",
                    "scene",
                    "clip_slot",
                    "clip",
                    "parameter",
                    "device",
                    "cue",
                }
            ][:256]
            record["journal"]["diagnostic"] = {
                "phase": name,
                "target_binding": record["target"]["binding"],
                "graph_revision": scope["revision"] + 1,
                "created": created,
            }
            if len(_bytes(record["journal"]["diagnostic"])) > 16384:
                record["journal"]["diagnostic"]["created"] = []
            self.used_nodes += candidate_nodes - old.identity_count
            if node_delta > 0:
                record["node_budget"] -= node_delta
                self.reserved_nodes -= node_delta
            scope["graph"] = candidate
            scope["revision"] += 1
            old.close()
        except BaseException:
            candidate.close()
            raise

    def _execute(self, record: Any) -> None:
        journal, request, scope = record["journal"], record["request"], record["scope"]
        command = request["command"]

        def phase(label: str, callback: Any, *args: Any) -> Any:
            require_peer(record["peer"])
            if not self._epoch(request) or scope["closed"] or scope["poisoned"]:
                raise RuntimeError("Original native effect authority is revoked")
            scope["graph"].verify()
            target_fields = None
            if command["name"] == "sunny_author_step_envelope":
                target_fields = tuple(
                    name for name in record["target"]["before"] if name != "has_envelopes"
                )
                parameter = record["extra"]["envelope_target"][1]
                parameter_node = next(
                    node
                    for node in scope["graph"].nodes.values()
                    if _same(node["object"], parameter)
                )
                scope["graph"].check_before(parameter_node)
            scope["graph"].check_before(record["target"], target_fields)
            if command["name"] == "sunny_set_device_parameter":
                scope["graph"].check_before(record["parameter"])
            if command["name"] == "sunny_set_cue":
                self._cue_before_phase(record, label)
            # Native getters above may block. Recheck this exact generation
            # after observation and immediately before publishing setter entry.
            require_peer(record["peer"])
            if journal["started_calls"] >= record["max_phases"]:
                raise RuntimeError("Declared native effect phase capacity exhausted")
            created = journal["diagnostic"]["created"] if journal["diagnostic"] else []
            journal["diagnostic"] = {
                "phase": label,
                "target_binding": record["target"]["binding"],
                "graph_revision": scope["revision"],
                "created": created,
            }
            journal["native_mutation_started"] = True
            journal["started_calls"] += 1
            result = callback(*args)
            journal["returned_calls"] += 1
            if (
                command["name"]
                in {
                    "create_scene",
                    "create_midi_track",
                    "create_return_track",
                    "insert_device",
                    "create_clip",
                    "delete_clip",
                }
                or label == "cue_toggle"
            ):
                self._transition(record)
            if command["name"] == "sunny_set_cue":
                self._cue_after_phase(record, label)
            return result

        try:
            require_peer(record["peer"])
            self._find_scope(request)
            self._prepare(record)
            value = self.owner._handler.handle_retained(
                command, scope["graph"], phase=phase, prepared=record
            )
            if (
                len(_bytes(value)) > record["result_budget"]
                or len(_bytes(journal)) + len(_bytes(value)) + 1024 > MAX_FRAME
            ):
                raise RuntimeError("Native effect readback exceeds its reserved byte capacity")
            # Update only the exact primitive's verified owned before-state.
            graph = scope["graph"]
            if command["type"] == "set":
                node = graph.node(command["path"])
                if node["kind"] == "parameter" and command["name"] in {"value", "display_value"}:
                    self._retain_parameter_pair(node)
                else:
                    node["before"][command["name"]] = getattr(
                        record["target"]["object"], command["name"]
                    )
                coupled = {"end_marker": "loop_end", "start_marker": "loop_start"}.get(
                    command["name"]
                )
                if coupled:
                    node = graph.node(command["path"])
                    actual = getattr(node["object"], coupled)
                    if actual not in (node["before"][coupled], node["before"][command["name"]]):
                        raise RuntimeError("Native marker changed an unrelated loop boundary")
                    node["before"][coupled] = actual
            elif command["name"] == "sunny_set_device_parameter":
                node = graph.parameter(command["path"], command["args"][0])
                self._retain_parameter_pair(node)
            elif command["name"] == "add_new_notes":
                node = graph.node(command["path"])
                notes = graph.observe_notes(node)
                before = {n["note_id"]: n for n in node["before"]["notes"]}
                after = {n["note_id"]: n for n in notes}
                if any(after.get(note_id) != old for note_id, old in before.items()):
                    raise RuntimeError("Original native notes changed during insertion")
                if (
                    len(value) != len(set(value))
                    or set(after) - set(before) != set(value)
                    or len(value) != len(command["args"][0]["notes"])
                ):
                    raise RuntimeError("Native inserted note identity population differs")
                by_id = graph.observe_note_read(
                    node, lambda: node["object"].get_notes_by_id(tuple(value))
                )
                if {note["note_id"]: note for note in by_id} != {
                    note_id: after[note_id] for note_id in value
                }:
                    raise RuntimeError("Native inserted identity and population readbacks differ")
                for note_id, intended in zip(value, command["args"][0]["notes"]):
                    if {
                        key: v for key, v in after[note_id].items() if key != "note_id"
                    } != intended:
                        raise RuntimeError("Native inserted note readback differs")
                node["before"]["notes"] = notes
                graph.dirty.discard(command["path"])
            elif command["name"] == "sunny_set_cue":
                for node in graph.nodes.values():
                    if (
                        node["kind"] == "cue"
                        and abs(float(node["before"]["time"]) - float(command["args"][0])) < 1e-7
                    ):
                        node["before"]["name"] = node["object"].name
            elif command["name"] == "sunny_author_step_envelope":
                graph.node(command["path"])["before"]["has_envelopes"] = record["target"][
                    "object"
                ].has_envelopes
            self._update_owned_routing(record, graph)
            self._update_graph_bytes(record, graph)
            # A returned effect can still be observed after its peer leaves.
            # A later phase always performs a fresh admission check above.
            graph.verify(admission=False)
            journal["outcome"] = "acknowledged"
            journal["result"] = {
                "value": value,
                "target_binding": record["target"]["binding"],
                "graph_revision": scope["revision"],
            }
            if journal["diagnostic"] and not journal["diagnostic"]["created"]:
                journal["diagnostic"] = None
        except Exception as error:
            journal["outcome"] = "partial" if journal["native_mutation_started"] else "declined"
            journal["result"], journal["error"] = None, _error(error)
            scope["poisoned"] = True
        finally:
            for key in ("scope", "target", "parameter", "peer", "extra", "original_cues"):
                record.pop(key, None)
            self._settle_record(record)

    def _retain_parameter_pair(self, node: Any) -> None:
        """Retain only the exact written parameter's verified coupled readback."""
        parameter = node["object"]
        observed = self.owner._handler._mixer_parameter_snapshot(parameter)
        for field, observed_field in (
            ("min", "minimum"),
            ("max", "maximum"),
            ("is_quantized", "is_quantized"),
            ("is_enabled", "is_enabled"),
        ):
            if observed[observed_field] != node["before"][field]:
                raise RuntimeError("Unrelated parameter domain changed during native write")
        for field in ("value", "display_value"):
            node["before"][field] = observed[field]

    def _cue_flags(self, graph: NativeGraph) -> None:
        before = graph.node("song")["before"]
        for field in ("is_playing", "session_record", "record_mode"):
            actual = getattr(graph.song, field)
            if (
                type(before[field]) is not bool
                or type(actual) is not bool
                or actual != before[field]
            ):
                raise RuntimeError("Original cue transport flags changed or are unavailable")

    @staticmethod
    def _cue_time_equal(first: Any, second: Any) -> bool:
        # Keep the existing cue identity tolerance; never widen it to make
        # naturally advancing running transport look qualified.
        return type(first) is float and type(second) is float and abs(first - second) < 1e-7

    def _cue_current_time(self, graph: NativeGraph) -> float:
        return self.owner._handler._lom_float(graph.song.current_song_time, "cue phase playhead")

    def _cue_before_phase(self, record: Any, label: str) -> None:
        graph, extra = record["scope"]["graph"], record["extra"]
        self._cue_flags(graph)
        self._cue_original_before(record)
        if extra["cue"] is not None:
            graph.check_before(extra["cue"])
            return
        actual = self._cue_current_time(graph)
        if label == "cue_move_playhead":
            if extra["previous_time"] is None:
                # Running transport has one volatile witness frozen at this
                # execute entry, not a new scope or refreshed musical target.
                extra["previous_time"] = actual
            expected = extra["previous_time"]
        else:
            expected = extra["expected_time"]
        if not self._cue_time_equal(actual, expected):
            raise RuntimeError("Cue phase playhead differs from its retained expectation")
        if label in {"cue_restore_playhead", "cue_rename"}:
            cue = extra.get("created_cue")
            if cue is None:
                raise RuntimeError("Verified owned CuePoint is unavailable before next phase")
            graph.check_before(cue)

    @staticmethod
    def _cue_original_before(record: Any, *, renamed_target: bool = False) -> None:
        """Preserve every original CuePoint scalar across owned forward phases."""
        target = record["extra"]["cue"]
        for node in record["original_cues"]:
            if renamed_target and node is target:
                continue
            NativeGraph.check_before(node)

    def _cue_after_phase(self, record: Any, label: str) -> None:
        graph, extra = record["scope"]["graph"], record["extra"]
        self._cue_flags(graph)
        self._cue_original_before(record, renamed_target=label == "cue_rename")
        beat, wanted_name = record["request"]["command"]["args"]
        beat = float(beat)
        if extra["cue"] is not None:
            cue = extra["cue"]
        else:
            expected = (
                extra["previous_time"] if label in {"cue_restore_playhead", "cue_rename"} else beat
            )
            if not self._cue_time_equal(self._cue_current_time(graph), expected):
                raise RuntimeError("Returned cue phase changed its expected playhead")
            extra["expected_time"] = expected
            if label == "cue_toggle":
                original = {node["binding"] for node in record["original_cues"]}
                created = [
                    node
                    for node in graph.nodes.values()
                    if node["kind"] == "cue" and node["binding"] not in original
                ]
                if len(created) != 1 or not self._cue_time_equal(
                    created[0]["before"]["time"], beat
                ):
                    raise RuntimeError("Native toggle did not create the exact requested CuePoint")
                extra["created_cue"] = created[0]
            cue = extra.get("created_cue")
        if cue is not None:
            if not self._cue_time_equal(cue["object"].time, beat):
                raise RuntimeError("Retained CuePoint time changed during its operation")
            if label == "cue_rename":
                if cue["object"].name != wanted_name:
                    raise RuntimeError("Native CuePoint name differs from requested name")
            else:
                graph.check_before(cue)
