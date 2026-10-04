"""Finite current Return/output/send candidates; no historical identity grant.

Source candidate e83d519: Push2/browser_component.py:825 creates a Return;
Push2/routing.py:218-225 assigns advertised objects and :522-524 proves their
actual Track association. No invented Live.RoutingType class is required.
"""

from __future__ import annotations

import copy
import math
import re
import uuid
from typing import Any

from .managed import _digest, _fingerprint, _key
from .managed_capacity import guard_managed_response_capacity
from .managed_song_settings import ManagedSongSettings

SEND_INSPECTION_METHOD = "sunny_managed_inspect_send"
CANDIDATES_METHOD = "sunny_managed_routing_candidates"
PREVIEW_METHOD = "sunny_managed_preview_routing"
GROUP_PREVIEW_METHOD = "sunny_managed_preview_group"
APPLY_METHOD = "sunny_managed_apply_routing"
MAX_PREVIEWS = 256
MAX_TRACKS = 256
KINDS = (
    "create_return",
    "adopt_return",
    "output_type",
    "output_channel",
    "send_level",
    "adopt_group",
)


def _closed(value: Any, names: tuple[str, ...]) -> bool:
    return type(value) is dict and set(value) == set(names)


def _index(value: Any) -> bool:
    return type(value) is int and 0 <= value <= 0x7FFFFFFF


def _hash_id(value: Any) -> bool:
    return bool(
        type(value) is str
        and re.fullmatch(r"0|-?[1-9][0-9]{0,18}", value)
        and -(1 << 63) <= int(value) < (1 << 63)
    )


def _text(value: Any) -> bool:
    try:
        return type(value) is str and len(value.encode("utf-8")) <= 1024
    except UnicodeEncodeError:
        return False


def valid_intent(value: Any) -> bool:
    """Closed source-supported requests; no arbitrary callback/property name."""
    if type(value) is not dict or value.get("kind") not in KINDS:
        return False
    kind = value["kind"]
    if kind == "create_return":
        return _closed(value, ("kind", "aux_key")) and _key(value["aux_key"])
    if kind == "adopt_return":
        return (
            _closed(value, ("kind", "aux_key", "return_index"))
            and _key(value["aux_key"])
            and _index(value["return_index"])
        )
    if kind == "adopt_group":
        return bool(
            _closed(value, ("kind", "group_key", "track_index", "member_binding_keys"))
            and _key(value["group_key"])
            and _index(value["track_index"])
            and type(value["member_binding_keys"]) is list
            and 1 <= len(value["member_binding_keys"]) <= MAX_TRACKS
            and all(_key(item) for item in value["member_binding_keys"])
            and len(set(value["member_binding_keys"])) == len(value["member_binding_keys"])
        )
    if kind in ("output_type", "output_channel"):
        return bool(
            _closed(value, ("kind", "destination", "group_key", "route_identifier"))
            and value["destination"] in ("main", "group")
            and (
                (_key(value["group_key"]) and value["destination"] == "group")
                or (value["group_key"] is None and value["destination"] == "main")
            )
            and _hash_id(value["route_identifier"])
        )
    return bool(
        _closed(value, ("kind", "aux_key", "level_db", "tolerance_db", "requested_pre_fader"))
        and _key(value["aux_key"])
        and all(
            type(value[name]) in (int, float) and math.isfinite(value[name])
            for name in ("level_db", "tolerance_db")
        )
        and value["tolerance_db"] >= 0.0
        and type(value["requested_pre_fader"]) is bool
    )


def valid_request(method: str, value: Any) -> bool:
    """Validate before an existing journal reservation or native lookup."""
    if method == CANDIDATES_METHOD:
        return bool(
            _closed(
                value,
                (
                    "document_token",
                    "project_key",
                    "binding_key",
                    "expected_content_fingerprint",
                    "expected_note_identity_fingerprint",
                ),
            )
            and all(_key(value[name]) for name in ("document_token", "project_key", "binding_key"))
            and all(
                _fingerprint(value[name])
                for name in (
                    "expected_content_fingerprint",
                    "expected_note_identity_fingerprint",
                )
            )
        )
    if method == GROUP_PREVIEW_METHOD:
        return bool(
            _closed(value, ("document_token", "project_key", "binding_key", "intent", "selector"))
            and all(_key(value[name]) for name in ("document_token", "project_key", "binding_key"))
            and valid_intent(value["intent"])
            and value["intent"]["kind"] == "adopt_group"
            and value["binding_key"] in value["intent"]["member_binding_keys"]
            and _closed(value["selector"], ("track_index", "slot_index"))
            and all(_index(value["selector"][name]) for name in ("track_index", "slot_index"))
        )
    fields = (
        "document_token",
        "project_key",
        "binding_key",
        "expected_content_fingerprint",
        "expected_note_identity_fingerprint",
        "intent",
    )
    if method == APPLY_METHOD:
        fields += (
            "operation_id",
            "preview_token",
            "preview_fingerprint",
            "approved_preview",
            "explicit_current_routing_approval",
        )
    return bool(
        method in (PREVIEW_METHOD, SEND_INSPECTION_METHOD, APPLY_METHOD)
        and _closed(value, fields)
        and all(_key(value[name]) for name in ("document_token", "project_key", "binding_key"))
        and all(
            _fingerprint(value[name])
            for name in ("expected_content_fingerprint", "expected_note_identity_fingerprint")
        )
        and valid_intent(value["intent"])
        and (method != SEND_INSPECTION_METHOD or value["intent"]["kind"] == "send_level")
        and (
            method in (PREVIEW_METHOD, SEND_INSPECTION_METHOD)
            or (
                _key(value["operation_id"])
                and type(value["preview_token"]) is str
                and re.fullmatch(r"[0-9a-f]{32}", value["preview_token"])
                and _fingerprint(value["preview_fingerprint"])
                and type(value["approved_preview"]) is dict
                and value["explicit_current_routing_approval"] is True
            )
        )
    )


def guard(value: dict[str, Any]) -> dict[str, Any]:
    """Small finite hashes; actual native handles remain private in the preview."""
    return {
        name: value.get(name)
        for name in (
            "content_fingerprint",
            "note_identity_fingerprint",
            "device_identity_fingerprint",
        )
    }


class ManagedRouting:
    """Use the registry's current context and sole journal; never touch disk."""

    def __init__(self, registry: Any) -> None:
        """Attach current-object state to the existing once-only registry."""
        self.registry = registry
        self.song_capture = ManagedSongSettings(registry)
        self._epoch: Any = None
        self._previews: dict[str, Any] = {}
        self._returns: dict[tuple[str, str], Any] = {}
        self._groups: dict[tuple[str, str], Any] = {}

    def _same_many(self, left: Any, right: Any) -> bool:
        return len(left) == len(right) and all(
            self.registry._same(a, b) for a, b in zip(left, right)
        )

    def _current(self, request: dict[str, Any]) -> Any:
        song = self.registry._ensure_document()
        epoch = (self.registry._bridge_instance, self.registry._document_token)
        if self._epoch != epoch:
            self._epoch = epoch
            self._previews.clear()
            self._returns.clear()
            self._groups.clear()
        if request["document_token"] != epoch[1]:
            raise RuntimeError("RoutingUnavailable: current native document changed")
        application = self.registry._handler._get_application()
        version = (application.get_major_version(), application.get_minor_version())
        if any(type(v) is not int for v in version) or version[0] != 12 or version[1] not in (3, 4):
            raise RuntimeError(
                "RoutingUnavailable: finite source candidates cover Live12.4.x/12.3.x"
            )
        return song

    def _objects(self, value: Any) -> tuple[Any, ...]:
        result = self.song_capture._objects(value)
        if len(result) > MAX_TRACKS:
            raise RuntimeError("RoutingUnavailable: native cohort exceeds256")
        return result

    def _track(self, song: Any, track: Any) -> None:
        import Live

        if not isinstance(track, Live.Track.Track) or not self.registry._same(
            track.canonical_parent, song
        ):
            raise RuntimeError("RoutingUnavailable: actual native Track/type/parent differs")

    def _route(self, value: Any, *, channel: bool, song: Any) -> dict[str, Any]:
        """Hash is in-epoch selection only; target identity uses attached_object."""
        name = value.display_name
        identifier = str(hash(value))
        wire = self.registry._handler._routing_dictionary(
            value, "output_routing_channel" if channel else "output_routing_type"
        )
        if (
            not _text(name)
            or not _hash_id(identifier)
            or not _text(wire["display_name"])
            or type(wire["identifier"]) is not str
            or len(wire["identifier"].encode("utf-8")) > 2048
        ):
            raise RuntimeError("RoutingUnavailable: actual native route descriptor is invalid")
        target = None
        if not channel:
            try:
                attached = value.attached_object
            except AttributeError:
                attached = None
            if attached is not None:
                import Live

                if not isinstance(attached, Live.Track.Track):
                    attached = None
                else:
                    self._track(song, attached)
                    if self.registry._same(attached, song.master_track):
                        target = {"kind": "main", "index": None}
                    else:
                        matches = [
                            (kind, index)
                            for kind, objects in (
                                ("track", self._objects(song.tracks)),
                                ("return", self._objects(song.return_tracks)),
                            )
                            for index, item in enumerate(objects)
                            if self.registry._same(item, attached)
                        ]
                        if len(matches) != 1:
                            raise RuntimeError(
                                "RoutingUnavailable: attached native Track is absent/ambiguous"
                            )
                        target = {"kind": matches[0][0], "index": matches[0][1]}
        return {"display_name": name, "identifier": identifier, "attached_target": target}

    def _routes(self, track: Any, song: Any) -> tuple[dict[str, Any], tuple[Any, ...]]:
        types = self._objects(track.available_output_routing_types)
        channels = self._objects(track.available_output_routing_channels)
        if not any(
            self.registry._same(track.output_routing_type, item) for item in types
        ) or not any(self.registry._same(track.output_routing_channel, item) for item in channels):
            raise RuntimeError("RoutingUnavailable: actual selected route is not advertised")
        value = {
            "output_type": self._route(track.output_routing_type, channel=False, song=song),
            "output_channel": self._route(track.output_routing_channel, channel=True, song=song),
            "available_types": [self._route(item, channel=False, song=song) for item in types],
            "available_channels": [self._route(item, channel=True, song=song) for item in channels],
        }
        if len({item["identifier"] for item in value["available_types"]}) != len(types) or len(
            {item["identifier"] for item in value["available_channels"]}
        ) != len(channels):
            raise RuntimeError("RoutingUnavailable: native route hash collision")
        return value, (track.output_routing_type, track.output_routing_channel, *types, *channels)

    def _parameter(self, parameter: Any) -> dict[str, Any]:
        value = self.registry._parameter_manifest(parameter)
        if any(not _text(value[name]) for name in ("name", "original_name")) or any(
            type(value[name]) is not float or not math.isfinite(value[name])
            for name in ("value", "min", "max")
        ):
            raise RuntimeError("RoutingUnavailable: native parameter text/float domain differs")
        return value

    def _frame(self, song: Any) -> tuple[dict[str, Any], dict[str, Any]]:
        before, common = self.song_capture._capture(song)
        self.song_capture._idle(before, before["settings"])
        tracks, returns = self._objects(song.tracks), self._objects(song.return_tracks)
        rows, identities = [], []
        for kind, objects in (("track", tracks), ("return", returns)):
            for index, track in enumerate(objects):
                self._track(song, track)
                mixer = track.mixer_device
                if not self.registry._same(mixer.canonical_parent, track):
                    raise RuntimeError("RoutingUnavailable: native Mixer parent differs")
                sends = self._objects(mixer.sends)
                if len(sends) != len(returns):
                    raise RuntimeError("RoutingUnavailable: one concrete send per Return required")
                parameters = (mixer.volume, mixer.panning, mixer.track_activator, *sends)
                if any(
                    not self.registry._same(parameter.canonical_parent, mixer)
                    for parameter in parameters
                ):
                    raise RuntimeError("RoutingUnavailable: native parameter parent differs")
                routing, route_objects = self._routes(track, song)
                row = {
                    "kind": kind,
                    "index": index,
                    "name": track.name,
                    "mute": track.mute,
                    "solo": track.solo,
                    "mixer": {
                        "panning_mode": mixer.panning_mode,
                        "crossfade_assign": mixer.crossfade_assign,
                        "volume": self._parameter(mixer.volume),
                        "panning": self._parameter(mixer.panning),
                        "track_activator": self._parameter(mixer.track_activator),
                        "sends": [self._parameter(item) for item in sends],
                    },
                    "routing": routing,
                }
                if (
                    not _text(row["name"])
                    or type(row["mute"]) is not bool
                    or type(row["solo"]) is not bool
                    or isinstance(row["mixer"]["panning_mode"], bool)
                    or not isinstance(row["mixer"]["panning_mode"], int)
                    or row["mixer"]["panning_mode"] not in (0, 1)
                    or isinstance(row["mixer"]["crossfade_assign"], bool)
                    or not isinstance(row["mixer"]["crossfade_assign"], int)
                    or row["mixer"]["crossfade_assign"] not in (0, 1, 2)
                ):
                    raise RuntimeError("RoutingUnavailable: native Track scalar domain differs")
                row["mixer"]["panning_mode"] = int(row["mixer"]["panning_mode"])
                row["mixer"]["crossfade_assign"] = int(row["mixer"]["crossfade_assign"])
                rows.append(row)
                identities.append(
                    (
                        track,
                        mixer,
                        parameters[:3],
                        sends,
                        route_objects,
                        tuple(self.registry._handler._device_chain(track)),
                    )
                )
        return {"song": before, "mixers": rows}, {
            "common": common,
            "tracks": tracks,
            "returns": returns,
            "rows": identities,
        }

    def _witness_equal(self, left: Any, right: Any) -> bool:
        return bool(
            self._same_many(left["common"], right["common"])
            and self._same_many(left["tracks"], right["tracks"])
            and self._same_many(left["returns"], right["returns"])
            and len(left["rows"]) == len(right["rows"])
            and all(
                self.registry._same(a[0], b[0])
                and self.registry._same(a[1], b[1])
                and all(self._same_many(a[i], b[i]) for i in range(2, 6))
                for a, b in zip(left["rows"], right["rows"])
            )
        )

    def _bindings(self) -> tuple[list[Any], list[Any]]:
        values, private = [], []
        for (project, key), record in self.registry._bindings.items():
            observation = self.registry._require_in_place_guard(
                record, record["content_fingerprint"]
            )
            values.append(
                {
                    "project_key": project,
                    "binding_key": key,
                    "guard": guard(observation),
                    "track_index": observation["track_index"],
                    "slot_index": observation["slot_index"],
                    "before_manifest": copy.deepcopy(observation["manifest"]),
                    "before_group_authority": copy.deepcopy(observation.get("group_authority")),
                    "before_group_authority_fingerprint": observation.get(
                        "group_authority_fingerprint"
                    ),
                }
            )
            private.append((record, self.registry._devices.private_cohort(record), observation))
        return values, private

    def _destination(self, request: Any, song: Any) -> Any:
        intent = request["intent"]
        if intent["destination"] == "main":
            return song.master_track
        retained = self._groups.get((request["project_key"], intent["group_key"]))
        if retained is None:
            raise RuntimeError("RoutingUnavailable: current Group was not explicitly adopted")
        self._verify_group(retained, song)
        return retained["track"]

    def _verify_group(self, retained: Any, song: Any) -> None:
        self._track(song, retained["track"])
        for member in retained["members"]:
            self._track(song, member)
            if member.is_foldable is not False:
                raise RuntimeError("RoutingUnavailable: flat current Group topology changed")
        if (
            not any(self.registry._same(retained["track"], item) for item in song.tracks)
            or retained["track"].is_foldable is not True
            or retained["track"].is_grouped is not False
            or retained["track"].group_track is not None
        ):
            raise RuntimeError("RoutingUnavailable: retained current Group was replaced")
        current = tuple(
            item for item in song.tracks if self.registry._same(item.group_track, retained["track"])
        )
        if not self._same_many(current, retained["members"]):
            raise RuntimeError("RoutingUnavailable: current Group membership changed")

    def _select(self, request: Any, song: Any, record: Any) -> tuple[dict[str, Any], Any]:
        intent, kind = request["intent"], request["intent"]["kind"]
        if kind == "create_return":
            if len(self._objects(song.return_tracks)) >= MAX_TRACKS:
                raise RuntimeError(
                    "RoutingUnavailable: native Return collection capacity exhausted"
                )
            if (request["project_key"], intent["aux_key"]) in self._returns:
                raise RuntimeError(
                    "RoutingUnavailable: Aux already retains a current native Return"
                )
            name = f"Sunny|{request['project_key']}|{intent['aux_key']}|return"
            if not _text(name) or any(
                track.name == name for track in (*song.tracks, *song.return_tracks)
            ):
                raise RuntimeError("RoutingUnavailable: new Return tag conflicts")
            return {"return_tag": name}, None
        if kind == "adopt_return":
            returns = self._objects(song.return_tracks)
            if intent["return_index"] >= len(returns):
                raise RuntimeError("RoutingUnavailable: selected Return is absent")
            selected = returns[intent["return_index"]]
            previous = self._returns.get((request["project_key"], intent["aux_key"]))
            if previous is not None and not self.registry._same(previous, selected):
                raise RuntimeError("RoutingUnavailable: same-epoch Return replacement is denied")
            return {"return_tag": selected.name}, selected
        track = record["track"]
        if kind in ("output_type", "output_channel"):
            destination = self._destination(request, song)
            self._track(song, destination)
            if track.has_audio_output is not True:
                raise RuntimeError("RoutingUnavailable: source has no actual audio output")
            types = self._objects(track.available_output_routing_types)
            if kind == "output_type":
                candidates = [
                    item for item in types if str(hash(item)) == intent["route_identifier"]
                ]
                if len(candidates) != 1 or not self.registry._same(
                    getattr(candidates[0], "attached_object", None), destination
                ):
                    raise RuntimeError(
                        "TargetAssociationUnavailable: actual advertised route "
                        "does not name retained destination"
                    )
                selected = candidates[0]
            else:
                if not self.registry._same(
                    getattr(track.output_routing_type, "attached_object", None), destination
                ):
                    raise RuntimeError(
                        "TargetAssociationUnavailable: current actual output destination differs"
                    )
                candidates = [
                    item
                    for item in self._objects(track.available_output_routing_channels)
                    if str(hash(item)) == intent["route_identifier"]
                ]
                if len(candidates) != 1:
                    raise RuntimeError("RoutingUnavailable: current channel absent/ambiguous")
                selected = candidates[0]
            return {"route": self._route(selected, channel=kind == "output_channel", song=song)}, (
                selected,
                destination,
            )
        selected = self._returns.get((request["project_key"], intent["aux_key"]))
        matches = [
            index
            for index, item in enumerate(song.return_tracks)
            if self.registry._same(item, selected)
        ]
        if len(matches) != 1:
            raise RuntimeError("RoutingUnavailable: current Aux Return absent/replaced")
        parameter = track.mixer_device.sends[matches[0]]
        if record["clip"].automation_envelope(parameter) is not None:
            raise RuntimeError("RoutingUnavailable: preserve existing selected send envelope")
        candidate = self._send_candidate(record, parameter, request, song)
        return {
            "return_index": matches[0],
            "return_tag": selected.name,
            "candidate": candidate,
            "tap_policy_observed": False,
            "logical_send_complete": False,
        }, (selected, parameter)

    def _send_candidate(self, record: Any, parameter: Any, request: Any, song: Any) -> Any:
        from .native_units import _descriptor, _search_parameter_display

        helper = self
        baseline, witness = self._frame(song)

        class Observation:
            def __init__(self) -> None:
                self.initial = {
                    "descriptor": _descriptor(parameter, quantized=False, eligible=True)
                }

            def check(self) -> None:
                helper.registry._require_in_place_guard(
                    record, request["expected_content_fingerprint"]
                )
                current, handles = helper._frame(song)
                if current != baseline or not helper._witness_equal(witness, handles):
                    raise RuntimeError(
                        "RoutingUnavailable: native cohort drift during send display search"
                    )

        return _search_parameter_display(
            parameter,
            observation=Observation(),
            unit="Decibels",
            target=request["intent"]["level_db"],
            tolerance=request["intent"]["tolerance_db"],
            gain_infinity=True,
        )

    def candidates(self, request: dict[str, Any]) -> dict[str, Any]:
        """Inspect native in-epoch selection IDs without previews, writes or grants."""
        if not valid_request(CANDIDATES_METHOD, request):
            raise RuntimeError("RoutingUnavailable: malformed candidate inspection")
        song = self._current(request)
        record = self.registry._bindings.get((request["project_key"], request["binding_key"]))
        if record is None:
            raise RuntimeError("RoutingUnavailable: owning Part current handles unavailable")
        expected = request["expected_content_fingerprint"]
        observation = self.registry._require_in_place_guard(record, expected)
        if (
            observation["note_identity_fingerprint"]
            != request["expected_note_identity_fingerprint"]
        ):
            raise RuntimeError("RoutingUnavailable: candidate inspection note guard differs")
        device_handles = self.registry._devices.private_cohort(record)
        frame, witness = self._frame(song)
        confirmed = self.registry._require_in_place_guard(record, expected)
        after, actual = self._frame(song)
        if (
            observation != confirmed
            or frame != after
            or not self._witness_equal(witness, actual)
            or not self._same_many(device_handles, self.registry._devices.private_cohort(record))
            or not self.registry._same(song, self._current(request))
        ):
            raise RuntimeError("RoutingUnavailable: candidate inspection native state changed")
        observation["track_tag"] = record["track_tag"]
        observation["clip_tag"] = record["clip_tag"]
        result = {
            "context": {"bridge_instance": self._epoch[0], "document_token": self._epoch[1]},
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "observation": observation,
            "frame": frame,
            "native_mutation_started": False,
            "authority_origin": "none",
        }
        guard_managed_response_capacity({}, result)
        return result

    def inspect_send(self, request: dict[str, Any]) -> dict[str, Any]:
        """Read current native Send dB evidence without tokens or authority refresh."""
        return self._readonly_capture(request, inspection=True)

    def preview(self, request: dict[str, Any]) -> dict[str, Any]:
        """Zero native writes, grants or baseline changes."""
        return self._readonly_capture(request, inspection=False)

    def _readonly_capture(self, request: dict[str, Any], *, inspection: bool) -> dict[str, Any]:
        method = SEND_INSPECTION_METHOD if inspection else PREVIEW_METHOD
        if not valid_request(method, request):
            raise RuntimeError("RoutingUnavailable: malformed preview request")
        if request["intent"]["kind"] == "adopt_group":
            raise RuntimeError("RoutingUnavailable: use separate current Group bootstrap preview")
        song = self._current(request)
        if not inspection and len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("RoutingUnavailable: preview capacity exhausted")
        record = self.registry._bindings.get((request["project_key"], request["binding_key"]))
        if record is None:
            raise RuntimeError("RoutingUnavailable: owning Part current handles unavailable")
        bindings, private = self._bindings()
        observation = next(item[2] for item in private if item[0] is record)
        if (
            guard(observation)["content_fingerprint"] != request["expected_content_fingerprint"]
            or guard(observation)["note_identity_fingerprint"]
            != request["expected_note_identity_fingerprint"]
        ):
            raise RuntimeError("RoutingUnavailable: owning Part preview guard differs")
        device_cohort = self.registry._devices.private_cohort(record)
        before, witness = self._frame(song)
        selected, objects = self._select(request, song, record)
        after, actual = self._frame(song)
        if before != after or not self._witness_equal(witness, actual):
            raise RuntimeError("RoutingUnavailable: native preview capture drift")
        body = {
            "schema_version": 1,
            "context": {"bridge_instance": self._epoch[0], "document_token": self._epoch[1]},
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "intent": copy.deepcopy(request["intent"]),
            "binding_guard": guard(observation),
            "before": before,
            "affected_bindings": bindings,
            "selected": selected,
            "scope": {
                "affects_send_population": request["intent"]["kind"] == "create_return",
                "tap_policy_observed": False,
                "group_membership_mutated": False,
                "historical_identity_proven": False,
            },
        }
        if inspection:
            readback = self._send_readback(body, objects[1])
            confirmed_bindings, confirmed_private = self._bindings()
            confirmed, actual = self._frame(song)
            current_selected, current_objects = self._select(request, song, record)
            if (
                before != confirmed
                or not self._witness_equal(witness, actual)
                or bindings != confirmed_bindings
                or len(private) != len(confirmed_private)
                or any(
                    original[0] is not current[0] or original[2] != current[2]
                    for original, current in zip(private, confirmed_private)
                )
                or selected != current_selected
                or not self._same_many(objects, current_objects)
                or not self._same_many(device_cohort, self.registry._devices.private_cohort(record))
                or not self.registry._same(song, self._current(request))
            ):
                raise RuntimeError("RoutingUnavailable: Send inspection native state changed")
            body.update(
                authority_origin="none", native_mutation_started=False, send_readback=readback
            )
            result = {"outcome": "observed", "inspection": body, "observation": observation}
            guard_managed_response_capacity({}, result)
            return result
        token = uuid.uuid4().hex
        if token in self._previews:
            raise RuntimeError("RoutingUnavailable: preview token collision")
        body["preview_token"] = token
        result = {
            "preview_token": token,
            "preview_fingerprint": _digest(body),
            "preview": body,
            "observation": observation,
            "native_mutation_started": False,
            "authority_origin": "none",
        }
        guard_managed_response_capacity({}, result)
        self._previews[token] = {
            "used": False,
            "body": body,
            "song": song,
            "record": record,
            "private": private,
            "witness": witness,
            "objects": objects,
        }
        return result

    def verify_retained_group(self, record: Any) -> bool:
        """Central base guard calls this specific grant; no blanket grouped permit."""
        for (project, binding), current in self.registry._bindings.items():
            if current is record:
                return self.verify_group_selection(project, binding, record["track"])
        return False

    def verify_group_selection(self, project: str, binding: str, track: Any) -> bool:
        """Exact approved current hierarchy; grants no Clip or note ownership."""
        self.registry._ensure_document()
        song = self._current({"document_token": self.registry._document_token})
        for (owner, _), retained in self._groups.items():
            if owner != project or binding not in retained["binding_keys"]:
                continue
            index = retained["binding_keys"].index(binding)
            if not (
                self.registry._same(track, retained["members"][index])
                and self.registry._same(track.group_track, retained["track"])
            ):
                continue
            self._verify_group(retained, song)
            return True
        return False

    def capture_group_authority(self, project: str, binding: str, track: Any) -> dict[str, Any]:
        """Evidence of a retained current Group grant; never a Clip/device grant."""
        if not self.verify_group_selection(project, binding, track):
            return {}
        song = self.registry._ensure_document()
        tracks = self._objects(song.tracks)
        for (owner, group_key), retained in self._groups.items():
            if owner != project or binding not in retained["binding_keys"]:
                continue
            ordinal = retained["binding_keys"].index(binding)
            if not (
                self.registry._same(track, retained["members"][ordinal])
                and self.registry._same(track.group_track, retained["track"])
            ):
                continue
            self._verify_group(retained, song)

            def index_of(actual: Any) -> int:
                found = [
                    index for index, item in enumerate(tracks) if self.registry._same(actual, item)
                ]
                if len(found) != 1:
                    raise RuntimeError(
                        "RoutingUnavailable: current Group/member position is ambiguous"
                    )
                return found[0]

            authority = {
                "schema_version": 1,
                "context": {"bridge_instance": self._epoch[0], "document_token": self._epoch[1]},
                "project_key": project,
                "group_key": group_key,
                "approved_preview_fingerprint": retained["approved_preview_fingerprint"],
                "group_track_index": index_of(retained["track"]),
                "member_binding_keys": list(retained["binding_keys"]),
                "member_track_indices": [index_of(member) for member in retained["members"]],
                "selected_binding_key": binding,
                "selected_track_index": index_of(track),
                "authority_origin": "explicit_current_group_adoption",
                "historical_identity_proven": False,
            }
            return {"group_authority": authority, "group_authority_fingerprint": _digest(authority)}
        return {}

    def _group_objects(self, request: Any, song: Any) -> tuple[dict[str, Any], dict[str, Any]]:
        intent, tracks = request["intent"], self._objects(song.tracks)
        if intent["track_index"] >= len(tracks):
            raise RuntimeError("RoutingUnavailable: current Group index is absent")
        selected = tracks[intent["track_index"]]
        if (
            selected.is_foldable is not True
            or selected.is_grouped is not False
            or selected.group_track is not None
        ):
            raise RuntimeError("RoutingUnavailable: selected current object is not a native Group")
        self._track(song, selected)
        members = tuple(item for item in tracks if self.registry._same(item.group_track, selected))
        keys = intent["member_binding_keys"]
        if len(members) != len(keys) or not members:
            raise RuntimeError("RoutingUnavailable: exact complete current Group members required")
        for track, key in zip(members, keys):
            if (
                track.is_foldable is not False
                or track.name != f"Sunny|{request['project_key']}|{key}|track"
            ):
                raise RuntimeError(
                    "RoutingUnavailable: flat named current member selection differs"
                )
        previous = self._groups.get((request["project_key"], intent["group_key"]))
        if previous is not None and not self.registry._same(previous["track"], selected):
            raise RuntimeError("RoutingUnavailable: same-epoch Group replacement is denied")
        retained = {"track": selected, "members": members, "binding_keys": tuple(keys)}
        description = {
            "group_name": selected.name,
            "member_count": len(members),
            "members": [
                {
                    "binding_key": key,
                    "track_index": next(
                        index
                        for index, item in enumerate(tracks)
                        if self.registry._same(item, track)
                    ),
                    "name": track.name,
                }
                for track, key in zip(members, keys)
            ],
        }
        return description, retained

    def preview_group(self, request: dict[str, Any]) -> dict[str, Any]:
        """Bootstrap a current Group before separate explicit grouped Clip adoption."""
        if not valid_request(GROUP_PREVIEW_METHOD, request):
            raise RuntimeError("RoutingUnavailable: malformed current Group preview")
        song = self._current(request)
        if len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("RoutingUnavailable: preview capacity exhausted")
        selected, objects = self._group_objects(request, song)
        selector = request["selector"]
        tracks = self._objects(song.tracks)
        if selector["track_index"] >= len(tracks):
            raise RuntimeError("RoutingUnavailable: current anchor Track absent")
        track = tracks[selector["track_index"]]
        position = objects["binding_keys"].index(request["binding_key"])
        if not self.registry._same(track, objects["members"][position]):
            raise RuntimeError("RoutingUnavailable: current anchor is not the declared member")
        slots = self._objects(track.clip_slots)
        if (
            selector["slot_index"] >= len(slots)
            or slots[selector["slot_index"]].has_clip is not True
        ):
            raise RuntimeError("RoutingUnavailable: current anchor Clip absent")
        slot = slots[selector["slot_index"]]
        clip = slot.clip
        tag, clip_tag = self.registry._tags(request["project_key"], request["binding_key"])
        if track.name != tag or clip.name != clip_tag:
            raise RuntimeError("RoutingUnavailable: current anchor Sunny locators differ")
        self.registry._handler._step_clip_interval(clip, idle=True)
        record = {
            "track": track,
            "slot": slot,
            "clip": clip,
            "track_tag": tag,
            "clip_tag": clip_tag,
            "adopted_device_cohort": tuple(self.registry._handler._device_chain(track)),
        }
        observation = self.registry._capture(record)
        observation.update(track_tag=tag, clip_tag=clip_tag)
        before, witness = self._frame(song)
        token = uuid.uuid4().hex
        if token in self._previews:
            raise RuntimeError("RoutingUnavailable: preview token collision")
        body = {
            "schema_version": 1,
            "preview_token": token,
            "group_only": True,
            "context": {"bridge_instance": self._epoch[0], "document_token": self._epoch[1]},
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "intent": copy.deepcopy(request["intent"]),
            "selector": copy.deepcopy(selector),
            "binding_guard": guard(observation),
            "before": before,
            "selected": selected,
            "scope": {
                "group_membership_mutated": False,
                "historical_identity_proven": False,
                "part_authority_granted": False,
                "device_authority_granted": False,
            },
        }
        result = {
            "preview_token": token,
            "preview_fingerprint": _digest(body),
            "preview": body,
            "observation": observation,
            "native_mutation_started": False,
            "authority_origin": "none",
        }
        guard_managed_response_capacity({}, result)
        self._previews[token] = {
            "group_only": True,
            "used": False,
            "body": body,
            "song": song,
            "witness": witness,
            "objects": objects,
            "record": record,
            "observation": observation,
        }
        return result

    def _apply_group(
        self, binding: Any, request: Any, operation: Any, retained: Any, song: Any
    ) -> Any:
        body = retained["body"]
        if (
            request["approved_preview"] != body
            or request["preview_fingerprint"] != _digest(body)
            or request["intent"] != body["intent"]
            or request["expected_content_fingerprint"]
            != body["binding_guard"]["content_fingerprint"]
            or request["expected_note_identity_fingerprint"]
            != body["binding_guard"]["note_identity_fingerprint"]
            or binding != (body["project_key"], body["binding_key"])
        ):
            raise RuntimeError("RoutingUnavailable: exact current Group approval differs")
        retained["used"] = True
        before, witness = self._frame(song)
        selected, objects = self._group_objects(request, song)
        observation = self.registry._capture(retained["record"])
        observation.update(
            track_tag=retained["record"]["track_tag"], clip_tag=retained["record"]["clip_tag"]
        )
        if (
            not self.registry._same(song, retained["song"])
            or before != body["before"]
            or not self._witness_equal(witness, retained["witness"])
            or selected != body["selected"]
            or not self.registry._same(objects["track"], retained["objects"]["track"])
            or not self._same_many(objects["members"], retained["objects"]["members"])
            or observation != retained["observation"]
        ):
            raise RuntimeError("RoutingUnavailable: approved current Group/anchor objects changed")
        progress = {"started": [], "returned": []}
        result = {
            "group_adoption": {
                "schema_version": 1,
                "approved_preview": body,
                "after": before,
                "progress": progress,
                "current_group_and_members_match": True,
                "authority_origin": "explicit_current_group_adoption",
                "part_authority_granted": False,
                "device_authority_granted": False,
                "historical_identity_proven": False,
            }
        }
        guard_managed_response_capacity(operation, result)
        operation["routing_progress"] = progress
        objects["approved_preview_fingerprint"] = request["preview_fingerprint"]
        # Successful explicit current approval retires conflicting private grants.
        # Never mutate historical receipts or silently shrink an old Group cohort.
        conflicts = [
            key
            for key, previous in self._groups.items()
            if key[0] == request["project_key"]
            and (
                set(previous["binding_keys"]) & set(objects["binding_keys"])
                or any(
                    self.registry._same(old, new)
                    for old in previous["members"]
                    for new in objects["members"]
                )
            )
        ]
        for key in conflicts:
            del self._groups[key]
        self._groups[(request["project_key"], body["intent"]["group_key"])] = objects
        # No Part binding, native IDs, Device key or static Mixer grant installed.
        del self._previews[request["preview_token"]]
        return result

    def apply(
        self, binding: Any, request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        """Called only after the existing once-only journal/durable product fence."""
        if not valid_request(APPLY_METHOD, request) or binding != (
            request["project_key"],
            request["binding_key"],
        ):
            raise RuntimeError("RoutingUnavailable: malformed explicit current approval")
        song = self._current(request)
        retained = self._previews.get(request["preview_token"])
        if retained is None or retained["used"]:
            raise RuntimeError("RoutingUnavailable: actual preview was not retained")
        if retained.get("group_only"):
            return self._apply_group(binding, request, operation, retained, song)
        body = retained["body"]
        if (
            request["approved_preview"] != body
            or request["preview_fingerprint"] != _digest(body)
            or request["intent"] != body["intent"]
            or request["project_key"] != body["project_key"]
            or request["binding_key"] != body["binding_key"]
            or request["expected_content_fingerprint"]
            != body["binding_guard"]["content_fingerprint"]
            or request["expected_note_identity_fingerprint"]
            != body["binding_guard"]["note_identity_fingerprint"]
        ):
            raise RuntimeError("RoutingUnavailable: immutable approval differs from actual preview")
        retained["used"] = True
        current_bindings, private = self._bindings()
        before, witness = self._frame(song)
        if (
            not self.registry._same(song, retained["song"])
            or before != body["before"]
            or current_bindings != body["affected_bindings"]
            or not self._witness_equal(witness, retained["witness"])
            or any(
                a[0] is not b[0] or not self._same_many(a[1], b[1])
                for a, b in zip(private, retained["private"])
            )
        ):
            raise RuntimeError(
                "RoutingUnavailable: concrete native objects/finite baseline changed"
            )
        selected, objects = self._select(request, song, retained["record"])
        if selected != body["selected"]:
            raise RuntimeError("RoutingUnavailable: selected native intent changed")
        # Actual target objects are checked separately from equal JSON/hash descriptions.
        kind = body["intent"]["kind"]
        if kind in ("output_type", "output_channel", "send_level") and not self._same_many(
            objects, retained["objects"]
        ):
            raise RuntimeError("RoutingUnavailable: selected actual native handles changed")
        if kind == "adopt_return" and not self.registry._same(objects, retained["objects"]):
            raise RuntimeError("RoutingUnavailable: selected actual Return changed")
        progress = {"started": [], "returned": []}
        operation["routing_progress"] = progress
        # Exact current evidence is budgeted before publication; native creation
        # additionally reserves worst bounded new-send descriptors for each Track.
        self._require_apply_capacity(body, private, retained["record"], operation, progress)
        try:
            if kind == "create_return":
                old_returns = self._objects(song.return_tracks)
                operation["native_mutation_started"] = True
                progress["started"].append("create_return_track")
                song.create_return_track()
                progress["returned"].append("create_return_track")
                returns = self._objects(song.return_tracks)
                if len(returns) != len(old_returns) + 1 or not self._same_many(
                    returns[:-1], old_returns
                ):
                    raise RuntimeError("RoutingUnavailable: native Return append was not exact")
                created = returns[-1]
                operation["partial_return_index"] = len(returns) - 1
                after_create, created_witness = self._frame(song)
                if not self._append_preserved(before, after_create, witness, created_witness):
                    raise RuntimeError(
                        "RoutingUnavailable: native Return append changed untouched state"
                    )
                progress["started"].append("return_name")
                created.name = body["selected"]["return_tag"]
                progress["returned"].append("return_name")
                objects = created
            elif kind == "adopt_return":
                pass
            elif kind in ("output_type", "output_channel"):
                field = "output_routing_type" if kind == "output_type" else "output_routing_channel"
                current = getattr(retained["record"]["track"], field)
                if not self.registry._same(current, objects[0]):
                    operation["native_mutation_started"] = True
                    progress["started"].append(field)
                    setattr(retained["record"]["track"], field, objects[0])
                    progress["returned"].append(field)
            else:
                parameter = objects[1]
                value = selected["candidate"]["internal_value"]
                if parameter.value != value:
                    operation["native_mutation_started"] = True
                    progress["started"].append("send_value")
                    parameter.value = value
                    progress["returned"].append("send_value")
            after, actual = self._frame(song)
            preserved = self._preserved(body, after, witness, actual)
            readback = self._send_readback(body, objects[1]) if kind == "send_level" else None
            desired = (
                readback["matches_intent"]
                if readback is not None
                else self._desired(body, retained["record"], objects, song)
            )
            after_format, formatted_handles = self._frame(song)
            if after_format != after or not self._witness_equal(actual, formatted_handles):
                raise RuntimeError("RoutingUnavailable: native state changed during final readback")
            if not preserved:
                raise RuntimeError(
                    "RoutingUnavailable: selected operation changed untouched native state"
                )
            if kind == "create_return":
                mixer_helper = getattr(self.registry, "_mixer", None)
                if mixer_helper is not None:
                    for record, _, _ in private:
                        mixer_helper.invalidate_retained_cohort(record)
            affected = []
            for record, device_handles, original in private:
                observed = self.registry._capture(record)
                if observed.get("group_authority") != original.get(
                    "group_authority"
                ) or observed.get("group_authority_fingerprint") != original.get(
                    "group_authority_fingerprint"
                ):
                    raise RuntimeError("RoutingUnavailable: current Group authority changed")
                if observed["note_identity_fingerprint"] != original[
                    "note_identity_fingerprint"
                ] or not self._same_many(
                    self.registry._devices.private_cohort(record), device_handles
                ):
                    raise RuntimeError(
                        "RoutingUnavailable: native Part notes/device cohort changed"
                    )
                if not self._part_preserved(body, original, observed, record is retained["record"]):
                    raise RuntimeError("RoutingUnavailable: untouched native Part content changed")
                # Authorized routing/send-population delta only; no new grant.
                observed["track_tag"] = record["track_tag"]
                observed["clip_tag"] = record["clip_tag"]
                affected.append(
                    {
                        "project_key": next(
                            key[0]
                            for key, value in self.registry._bindings.items()
                            if value is record
                        ),
                        "binding_key": next(
                            key[1]
                            for key, value in self.registry._bindings.items()
                            if value is record
                        ),
                        "observation": observed,
                    }
                )
            result = self.registry._capture(retained["record"])
            result["track_tag"] = retained["record"]["track_tag"]
            result["clip_tag"] = retained["record"]["clip_tag"]
            result["routing"] = {
                "schema_version": 1,
                "approved_preview": body,
                "after": after,
                "progress": copy.deepcopy(progress),
                "desired_match": desired,
                "untouched_observed_state_preserved": preserved,
                "affected_observations": affected,
                "authority_origin": "explicit_current_adoption"
                if kind in ("adopt_return", "adopt_group")
                else "explicit_current_routing",
                "tap_policy_observed": False,
                "logical_send_complete": False,
                "send_readback": readback,
            }
            guard_managed_response_capacity(operation, result)
            # Seal all Part baselines privately, then publish the Return grant.
            # No partial baseline or grant survives a failed final seal. Mixer
            # revocation from actual send-population change intentionally stays.
            snapshots = []
            for record, _, _ in private:
                snapshot = dict(record)
                if "_managed_devices" in snapshot:
                    snapshot["_managed_devices"] = dict(snapshot["_managed_devices"])
                snapshots.append((record, snapshot))
            try:
                for index, (record, _, _) in enumerate(private):
                    sealed = self.registry._seal(record)
                    sealed.pop("observed_notes_match_request", None)
                    sealed.pop("observed_clip_properties_match_request", None)
                    if sealed != affected[index]["observation"]:
                        raise RuntimeError(
                            "RoutingUnavailable: native Part changed during final seal"
                        )
            except Exception:
                for record, snapshot in snapshots:
                    record.clear()
                    record.update(snapshot)
                raise
            if kind in ("create_return", "adopt_return"):
                self._returns[(request["project_key"], body["intent"]["aux_key"])] = objects
            del self._previews[request["preview_token"]]
            return result
        except Exception:
            try:
                candidate = dict(operation)
                candidate["routing_partial"] = self._frame(song)[0]
                guard_managed_response_capacity(candidate, None)
                operation["routing_partial"] = candidate["routing_partial"]
            except Exception as error:
                operation["routing_partial_unavailable"] = str(error)[:1024]
            raise

    def _require_apply_capacity(
        self, body: Any, private: Any, anchor: Any, operation: Any, progress: Any
    ) -> None:
        """Bound full anchor/affected replies before writes; no first-row shortcut.

        One Return may add at most one advertised target per old Track in this
        finite candidate. New Return descriptors/channels and new type channels
        reserve the entire bounded collection. Unexpected wider native deltas
        stop before later setters and retain a bounded partial-unavailable reply.
        """
        worst = "\x01" * 1024  # maximum ASCII-escaped bytes under text's UTF8 bound
        parameter = {
            "name": worst,
            "original_name": worst,
            "value": 0.0,
            "min": 0.0,
            "max": 1.0,
            "is_quantized": False,
            "is_enabled": True,
            "state": 2,
            "automation_state": 2,
        }
        route = {
            "display_name": worst,
            "identifier": "-9223372036854775808",
            "attached_target": {"kind": "return", "index": 255},
        }
        channel = {**route, "attached_target": None}
        after = copy.deepcopy(body["before"])
        observations = [
            {
                "project_key": item["project_key"],
                "binding_key": item["binding_key"],
                "observation": copy.deepcopy(captured[2]),
            }
            for item, captured in zip(body["affected_bindings"], private)
        ]
        anchor_index = next(index for index, item in enumerate(private) if item[0] is anchor)
        result = copy.deepcopy(observations[anchor_index]["observation"])
        kind = body["intent"]["kind"]
        if kind == "create_return":
            count = len(after["song"]["return_tracks"]) + 1
            after["song"]["return_tracks"].append({"index": count - 1, "name": worst})
            for row in after["mixers"]:
                row["mixer"]["sends"].append(copy.deepcopy(parameter))
                row["routing"]["available_types"].append(copy.deepcopy(route))
            after["mixers"].append(
                {
                    "kind": "return",
                    "index": count - 1,
                    "name": worst,
                    "mute": False,
                    "solo": False,
                    "mixer": {
                        "panning_mode": 1,
                        "crossfade_assign": 2,
                        "volume": parameter,
                        "panning": parameter,
                        "track_activator": parameter,
                        "sends": [parameter] * count,
                    },
                    "routing": {
                        "output_type": route,
                        "output_channel": channel,
                        "available_types": [route] * MAX_TRACKS,
                        "available_channels": [channel] * MAX_TRACKS,
                    },
                }
            )
            for observed in observations:
                observed["observation"]["manifest"]["mixer"]["sends"].append(parameter)
            result["manifest"]["mixer"]["sends"].append(parameter)
        elif kind == "output_type":
            row_index = body["affected_bindings"][anchor_index]["track_index"]
            after["mixers"][row_index]["routing"]["available_channels"] = [channel] * MAX_TRACKS
            after["mixers"][row_index]["routing"]["output_channel"] = channel
            after["mixers"][row_index]["routing"]["output_type"] = route
            # Legacy manifest identifiers are descriptions only, bounded text.
            for observed in (observations[anchor_index]["observation"], result):
                observed["manifest"]["routing"]["output_routing_type"] = {
                    "display_name": worst,
                    "identifier": worst * 2,
                }
                observed["manifest"]["routing"]["output_routing_channel"] = {
                    "display_name": worst,
                    "identifier": worst * 2,
                }
        result["routing"] = {
            "schema_version": 1,
            "approved_preview": body,
            "after": after,
            "progress": {
                "started": ["create_return_track", "return_name"],
                "returned": ["create_return_track", "return_name"],
            },
            "desired_match": False,
            "untouched_observed_state_preserved": False,
            "affected_observations": observations,
            "authority_origin": "explicit_current_adoption",
            "tap_policy_observed": False,
            "logical_send_complete": False,
            "send_readback": {
                "internal_value": 0.0,
                "display": "\x01" * 80,
                "display_value": 0.0,
                "absolute_display_error": 0.0,
                "negative_infinity": False,
                "matches_intent": False,
            },
        }
        guard_managed_response_capacity(operation, result)

    def _append_preserved(self, before: Any, after: Any, old: Any, new: Any) -> bool:
        normalized = copy.deepcopy(after)
        if len(normalized["song"]["return_tracks"]) != len(before["song"]["return_tracks"]) + 1:
            return False
        normalized["song"]["return_tracks"].pop()
        normalized["mixers"].pop()
        for row in normalized["mixers"]:
            if len(row["mixer"]["sends"]) != len(old["returns"]) + 1:
                return False
            row["mixer"]["sends"].pop()
        # Existing output-route availability may gain the new native Return;
        # retaining old options in exact order is checked via actual handles.
        for a, b in zip(before["mixers"], normalized["mixers"]):
            b["routing"]["available_types"] = [
                item
                for item in b["routing"]["available_types"]
                if item in a["routing"]["available_types"]
            ]
        common_before = copy.deepcopy(before)
        values_ok = normalized == common_before
        added = new["returns"][-1]
        old_common = tuple(item for item in new["common"] if not self.registry._same(item, added))
        handles_ok = (
            self._same_many(old["tracks"], new["tracks"])
            and self._same_many(old["returns"], new["returns"][:-1])
            and len(new["common"]) == len(old["common"]) + 1
            and self._same_many(old["common"], old_common)
        )
        for index, (a, b) in enumerate(zip(old["rows"], new["rows"][:-1])):
            handles_ok = (
                handles_ok
                and self.registry._same(a[0], b[0])
                and self.registry._same(a[1], b[1])
                and self._same_many(a[2], b[2])
                and self._same_many(a[3], b[3][:-1])
                and self._same_many(a[5], b[5])
            )
            old_types = len(before["mixers"][index]["routing"]["available_types"])
            new_types = len(after["mixers"][index]["routing"]["available_types"])
            handles_ok = handles_ok and 0 <= new_types - old_types <= 1
            handles_ok = (
                handles_ok
                and self._same_many(a[4][:2], b[4][:2])
                and self._same_many(a[4][2 + old_types :], b[4][2 + new_types :])
            )
            for route in a[4][2 : 2 + old_types]:
                handles_ok = handles_ok and any(
                    self.registry._same(route, item) for item in b[4][2 : 2 + new_types]
                )
            for route in b[4][2 : 2 + new_types]:
                if not any(self.registry._same(route, item) for item in a[4][2 : 2 + old_types]):
                    handles_ok = handles_ok and self.registry._same(
                        getattr(route, "attached_object", None), new["returns"][-1]
                    )
        return values_ok and handles_ok

    def _part_preserved(self, body: Any, before: Any, after: Any, anchor: bool) -> bool:
        normalized = copy.deepcopy(after["manifest"])
        original = before["manifest"]
        kind = body["intent"]["kind"]
        if kind == "create_return":
            if len(normalized["mixer"]["sends"]) != len(original["mixer"]["sends"]) + 1:
                return False
            normalized["mixer"]["sends"].pop()
        elif kind == "send_level" and anchor:
            index = body["selected"]["return_index"]
            normalized["mixer"]["sends"][index]["value"] = original["mixer"]["sends"][index][
                "value"
            ]
        elif kind in ("output_type", "output_channel") and anchor:
            if kind == "output_type":
                normalized["routing"]["output_routing_type"] = original["routing"][
                    "output_routing_type"
                ]
            normalized["routing"]["output_routing_channel"] = original["routing"][
                "output_routing_channel"
            ]
        return normalized == original

    def _preserved(self, body: Any, after: Any, before_handles: Any, after_handles: Any) -> bool:
        before, kind = body["before"], body["intent"]["kind"]
        if kind == "create_return":
            return self._append_preserved(before, after, before_handles, after_handles)
        normalized = copy.deepcopy(after)
        if kind in ("output_type", "output_channel", "send_level"):
            anchor = next(
                index
                for index, row in enumerate(body["affected_bindings"])
                if row["project_key"] == body["project_key"]
                and row["binding_key"] == body["binding_key"]
            )
            track = self._previews[
                next(
                    token for token, retained in self._previews.items() if retained["body"] is body
                )
            ]["private"][anchor][0]["track"]
            row_index = next(
                index
                for index, item in enumerate(before_handles["tracks"])
                if self.registry._same(track, item)
            )
            if kind == "send_level":
                normalized["mixers"][row_index]["mixer"]["sends"][body["selected"]["return_index"]][
                    "value"
                ] = before["mixers"][row_index]["mixer"]["sends"][body["selected"]["return_index"]][
                    "value"
                ]
            else:
                # A type setter may replace its channel population. A channel
                # setter changes only the current channel, never route options.
                selected_routing = normalized["mixers"][row_index]["routing"]
                original_routing = before["mixers"][row_index]["routing"]
                selected_routing["output_channel"] = original_routing["output_channel"]
                if kind == "output_type":
                    selected_routing["output_type"] = original_routing["output_type"]
                    selected_routing["available_channels"] = original_routing["available_channels"]
        # Route objects are intentionally selected/new channels may be renewed;
        # the destination and all unrelated native cohorts are still fixed.
        if normalized != before or not self._same_many(
            before_handles["common"], after_handles["common"]
        ):
            return False
        for index, (a, b) in enumerate(zip(before_handles["rows"], after_handles["rows"])):
            if (
                not self.registry._same(a[0], b[0])
                or not self.registry._same(a[1], b[1])
                or any(not self._same_many(a[i], b[i]) for i in (2, 3, 5))
            ):
                return False
            if kind not in ("output_type", "output_channel") or index != row_index:
                if not self._same_many(a[4], b[4]):
                    return False
            else:
                types_count = len(before["mixers"][index]["routing"]["available_types"])
                if not self._same_many(a[4][2 : 2 + types_count], b[4][2 : 2 + types_count]):
                    return False
                if kind == "output_channel" and (
                    not self.registry._same(a[4][0], b[4][0])
                    or not self._same_many(a[4][2 + types_count :], b[4][2 + types_count :])
                ):
                    return False
        return True

    def _desired(self, body: Any, record: Any, objects: Any, song: Any) -> bool:
        kind = body["intent"]["kind"]
        if kind in ("create_return", "adopt_return"):
            selected = objects
            return (
                any(self.registry._same(selected, item) for item in song.return_tracks)
                and selected.name == body["selected"]["return_tag"]
            )
        if kind == "adopt_group":
            self._verify_group(objects, song)
            return True
        if kind in ("output_type", "output_channel"):
            selected = getattr(
                record["track"],
                "output_routing_type" if kind == "output_type" else "output_routing_channel",
            )
            return self.registry._same(selected, objects[0]) and self.registry._same(
                getattr(record["track"].output_routing_type, "attached_object", None), objects[1]
            )
        return False

    def _send_readback(self, body: Any, parameter: Any) -> dict[str, Any]:
        from decimal import ROUND_HALF_EVEN, Decimal, localcontext

        from .native_units import _parse

        value = parameter.value
        if type(value) is not float or not math.isfinite(value):
            raise RuntimeError("RoutingUnavailable: invalid actual send value")
        display = parameter.str_for_value(value)
        amount, _ = _parse(display, "Decibels", gain_infinity=True)
        with localcontext() as context:
            context.prec = 110
            context.rounding = ROUND_HALF_EVEN
            error = abs(amount - Decimal(str(body["intent"]["level_db"])))
            match = amount.is_finite() and error <= Decimal(str(body["intent"]["tolerance_db"]))
        return {
            "internal_value": value,
            "display": display,
            "display_value": float(amount) if amount.is_finite() else None,
            "absolute_display_error": float(error) if amount.is_finite() else None,
            "negative_infinity": not amount.is_finite(),
            "matches_intent": bool(match),
        }
