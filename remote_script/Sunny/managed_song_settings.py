"""Explicit current Set-wide scalars for Live12.4.x / finite12.3.x candidates.

Song tempo/meter automation and marker populations are not authored or fully
observed. Native Python availability is checked; the Max LOM is not its ABI.
"""

from __future__ import annotations

import copy
import math
import uuid
from typing import Any

from .managed import _digest, _fingerprint, _key
from .managed_capacity import guard_managed_response_capacity, require_response_capacity

PREVIEW_METHOD = "sunny_managed_preview_song_settings"
APPLY_METHOD = "sunny_managed_apply_song_settings"
MAX_PREVIEWS = 256
MAX_COHORT = 4096
FIELDS = ("signature_numerator", "signature_denominator", "tempo")
FLAGS = (
    "is_playing",
    "is_counting_in",
    "record_mode",
    "session_record",
    "session_automation_record",
    "arrangement_overdub",
    "overdub",
    "is_ableton_link_enabled",
    "is_ableton_link_start_stop_sync_enabled",
    "tempo_follower_enabled",
    "nudge_down",
    "nudge_up",
    "back_to_arranger",
    "re_enable_automation_enabled",
    "loop",
    "metronome",
)
IDLE_FLAGS = FLAGS[:12]
CLIP_FLAGS = (
    "is_playing",
    "is_recording",
    "is_overdubbing",
    "is_triggered",
    "will_record_on_start",
)
UNAVAILABLE = (
    "arrangement_tempo_envelope_population",
    "arrangement_time_signature_markers",
    "unrelated_track_internal_content",
)
SCOPE = {
    "set_wide": True,
    "all_tracks_affected": True,
    "historical_song_identity_proven": False,
    "scene_overrides_preserved": True,
}


def _closed(value: Any, fields: tuple[str, ...]) -> bool:
    return type(value) is dict and set(value) == set(fields)


def _settings(value: Any) -> bool:
    return (
        _closed(value, ("tempo", "signature_numerator", "signature_denominator"))
        and type(value["tempo"]) in (int, float)
        and math.isfinite(value["tempo"])
        and 20.0 <= value["tempo"] <= 999.0
        and type(value["signature_numerator"]) is int
        and 1 <= value["signature_numerator"] <= 99
        and type(value["signature_denominator"]) is int
        and value["signature_denominator"] in (1, 2, 4, 8, 16)
    )


def valid_request(method: str, value: Any) -> bool:
    """Admit only closed finite preview and explicit current-settings requests."""
    fields: tuple[str, ...] = (
        "document_token",
        "project_key",
        "binding_key",
        "expected_content_fingerprint",
        "expected_note_identity_fingerprint",
        "desired",
    )
    if method == APPLY_METHOD:
        fields += (
            "operation_id",
            "preview_token",
            "preview_fingerprint",
            "approved_preview",
            "explicit_set_wide_approval",
        )
    if method not in (PREVIEW_METHOD, APPLY_METHOD) or not _closed(value, fields):
        return False
    if not all(
        _key(value[name]) for name in ("document_token", "project_key", "binding_key")
    ) or not _settings(value["desired"]):
        return False
    if not all(
        _fingerprint(value[name])
        for name in ("expected_content_fingerprint", "expected_note_identity_fingerprint")
    ):
        return False
    if method == PREVIEW_METHOD:
        return True
    return (
        _key(value["operation_id"])
        and type(value["preview_token"]) is str
        and len(value["preview_token"]) == 32
        and all(c in "0123456789abcdef" for c in value["preview_token"])
        and _fingerprint(value["preview_fingerprint"])
        and type(value["approved_preview"]) is dict
        and value["explicit_set_wide_approval"] is True
    )


def changed_fields(before: dict[str, Any], desired: dict[str, Any]) -> list[str]:
    """Return changed scalars in their deliberate native setter order."""
    return [name for name in FIELDS if before["settings"][name] != desired[name]]


def untouched(before: dict[str, Any], after: dict[str, Any]) -> bool:
    """Compare all declared observed fields except the three authorized scalars."""
    return bool(
        _digest({k: v for k, v in before.items() if k != "settings"})
        == _digest({k: v for k, v in after.items() if k != "settings"})
    )


class ManagedSongSettings:
    """Retain explicit bounded current-object previews; never infer Song ownership."""

    def __init__(self, registry: Any) -> None:
        """Attach to the existing registry without conferring global authority."""
        self.registry = registry
        self._epoch: tuple[str, str] | None = None
        self._previews: dict[str, Any] = {}

    @staticmethod
    def _value(owner: Any, name: str, kind: type) -> Any:
        try:
            result: Any = getattr(owner, name)
        except AttributeError as error:
            raise RuntimeError(
                f"SongSettingsUnavailable: actual {name} access is missing"
            ) from error
        if type(result) is not kind:
            raise RuntimeError(f"SongSettingsUnavailable: invalid actual {name}")
        if kind is float and (not isinstance(result, float) or not math.isfinite(result)):
            raise RuntimeError(f"SongSettingsUnavailable: invalid actual {name}")
        if isinstance(result, str):
            try:
                valid = len(result.encode("utf-8")) <= 1024
            except UnicodeEncodeError:
                valid = False
            if not valid:
                raise RuntimeError(f"SongSettingsUnavailable: invalid actual {name}")
        return result

    @staticmethod
    def _objects(value: Any) -> tuple[Any, ...]:
        if isinstance(value, (str, bytes, dict)):
            raise RuntimeError("SongSettingsUnavailable: invalid native object collection")
        result = tuple(value)
        if len(result) > MAX_COHORT:
            raise RuntimeError("SongSettingsUnavailable: native cohort exceeds4096")
        return result

    def _current(self, request: dict[str, Any]) -> Any:
        song = self.registry._ensure_document()
        epoch = (self.registry._bridge_instance, self.registry._document_token)
        if epoch != self._epoch:
            self._previews.clear()
            self._epoch = epoch
        if request["document_token"] != epoch[1]:
            raise RuntimeError("SongSettingsUnavailable: current document epoch differs")
        application = self.registry._handler._get_application()
        version = tuple(
            getattr(application, name)()
            for name in ("get_major_version", "get_minor_version", "get_bugfix_version")
        )
        if (
            any(type(v) is not int or v < 0 for v in version)
            or version[0] != 12
            or version[1] not in (3, 4)
        ):
            raise RuntimeError(
                "SongSettingsUnavailable: candidate coverage is Live12.4.x and12.3.x"
            )
        return song

    def _capture(self, song: Any) -> tuple[dict[str, Any], tuple[Any, ...]]:
        import Live  # type: ignore[import-not-found]

        settings = {
            name: self._value(song, name, float if name == "tempo" else int) for name in FIELDS
        }
        if not _settings(settings):
            raise RuntimeError("SongSettingsUnavailable: invalid actual tempo/meter domains")
        flags = {name: self._value(song, name, bool) for name in FLAGS}
        tracks = self._objects(song.tracks)
        returns = self._objects(song.return_tracks)
        scenes = self._objects(song.scenes)
        cues = self._objects(song.cue_points)
        master = song.master_track
        cohort: list[Any] = [song, master, master.mixer_device]
        cohort.extend(tracks + returns + scenes + cues)
        track_values = []
        for index, track in enumerate(tracks):
            if not self.registry._same(track.canonical_parent, song):
                raise RuntimeError("SongSettingsUnavailable: actual Track parent differs")
            slots = self._objects(track.clip_slots)
            cohort.extend(slots)
            clips = []
            for slot_index, slot in enumerate(slots):
                occupied = self._value(slot, "has_clip", bool)
                clip = slot.clip if occupied else None
                if clip is not None:
                    cohort.append(clip)
                clips.append(
                    {
                        "index": slot_index,
                        "has_clip": occupied,
                        "clip": None
                        if clip is None
                        else {
                            "name": self._value(clip, "name", str),
                            **{name: self._value(clip, name, bool) for name in CLIP_FLAGS},
                        },
                    }
                )
            track_values.append(
                {"index": index, "name": self._value(track, "name", str), "clip_slots": clips}
            )
        mixer = master.mixer_device
        if not self.registry._same(mixer.canonical_parent, master):
            raise RuntimeError("SongSettingsUnavailable: actual Main mixer parent differs")
        try:
            tempo_parameter = mixer.song_tempo
        except AttributeError:
            tempo_parameter = None
        parameter = None
        if tempo_parameter is not None:
            if not isinstance(
                tempo_parameter, Live.DeviceParameter.DeviceParameter
            ) or not self.registry._same(tempo_parameter.canonical_parent, mixer):
                raise RuntimeError(
                    "TempoParameterUnavailable: actual Main song_tempo type/parent differs"
                )
            try:
                native = {
                    name: getattr(tempo_parameter, name)
                    for name in ("is_enabled", "state", "automation_state")
                }
            except (AttributeError, RuntimeError) as error:
                raise RuntimeError(
                    "TempoParameterUnavailable: actual Main song_tempo state access is unavailable"
                ) from error
            if type(native["is_enabled"]) is not bool or any(
                isinstance(native[name], bool)
                or not isinstance(native[name], int)
                or int(native[name]) not in (0, 1, 2)
                for name in ("state", "automation_state")
            ):
                raise RuntimeError(
                    "TempoParameterUnavailable: invalid actual Main song_tempo state"
                )
            parameter = {
                "is_enabled": native["is_enabled"],
                "state": int(native["state"]),
                "automation_state": int(native["automation_state"]),
            }
        # Include absence in the private identity sequence; it may not silently become a new handle.
        cohort.append(tempo_parameter)
        snapshot = {
            "settings": settings,
            "current_song_time": self._value(song, "current_song_time", float),
            "loop_start": self._value(song, "loop_start", float),
            "loop_length": self._value(song, "loop_length", float),
            "flags": flags,
            "tempo_parameter": parameter,
            "tracks": track_values,
            "return_tracks": [
                {"index": index, "name": self._value(track, "name", str)}
                for index, track in enumerate(returns)
            ],
            "master_track": {"name": self._value(master, "name", str)},
            "scenes": [self.registry._handler._scene_snapshot(scene) for scene in scenes],
            "cue_points": [
                {"name": self._value(cue, "name", str), "time": self._value(cue, "time", float)}
                for cue in cues
            ],
        }
        if (
            snapshot["current_song_time"] < 0
            or snapshot["loop_start"] < 0
            or snapshot["loop_length"] <= 0
        ):
            raise RuntimeError("SongSettingsUnavailable: invalid actual transport interval")
        return snapshot, tuple(cohort)

    @staticmethod
    def _idle(snapshot: dict[str, Any], desired: dict[str, Any]) -> None:
        if snapshot["current_song_time"] != 0.0 or any(
            snapshot["flags"][name] for name in IDLE_FLAGS
        ):
            raise RuntimeError(
                "SongSettingsUnavailable: initial settings require stopped origin/"
                "no recording/Link/Follower/nudge"
            )
        if any(scene["is_triggered"] for scene in snapshot["scenes"]) or any(
            slot["clip"] is not None and any(slot["clip"][name] for name in CLIP_FLAGS)
            for track in snapshot["tracks"]
            for slot in track["clip_slots"]
        ):
            raise RuntimeError("SongSettingsUnavailable: a Clip or Scene is active/pending")
        if snapshot["settings"]["tempo"] != desired["tempo"]:
            parameter = snapshot["tempo_parameter"]
            if parameter is None:
                raise RuntimeError(
                    "TempoParameterUnavailable: actual Main mixer song_tempo access is missing"
                )
            if (
                not parameter["is_enabled"]
                or parameter["state"] != 0
                or parameter["automation_state"] != 0
            ):
                raise RuntimeError(
                    "TempoParameterUnavailable: actual Main song_tempo must be "
                    "active/enabled/unautomated"
                )

    def _binding(self, request: dict[str, Any]) -> tuple[dict[str, Any], dict[str, Any]]:
        record = self.registry._bindings.get((request["project_key"], request["binding_key"]))
        if record is None:
            raise RuntimeError("SongSettingsUnavailable: current Part handles were not retained")
        observation = self.registry._require_in_place_guard(
            record, request["expected_content_fingerprint"]
        )
        if (
            observation["note_identity_fingerprint"]
            != request["expected_note_identity_fingerprint"]
        ):
            raise RuntimeError("SongSettingsUnavailable: approved native note IDs differ")
        return record, observation

    def preview(self, request: dict[str, Any]) -> dict[str, Any]:
        """Retain current objects/settings for explicit review; execute no setters."""
        if not valid_request(PREVIEW_METHOD, request):
            raise RuntimeError("SongSettingsUnavailable: malformed preview request")
        song = self._current(request)
        if len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("SongSettingsUnavailable: preview capacity exhausted")
        record, observation = self._binding(request)
        device_cohort = self.registry._devices.private_cohort(record)
        before, cohort = self._capture(song)
        self._idle(before, request["desired"])
        token = ""
        for _ in range(64):
            candidate = uuid.uuid4().hex
            if candidate not in self._previews:
                token = candidate
                break
        if not token:
            raise RuntimeError("SongSettingsUnavailable: preview token collision bound exhausted")
        approved = {
            "schema_version": 1,
            "context": {
                "bridge_instance": self.registry._bridge_instance,
                "document_token": self.registry._document_token,
            },
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "preview_token": token,
            "desired": copy.deepcopy(request["desired"]),
            "before": before,
            "binding_guard": {
                "content_fingerprint": observation["content_fingerprint"],
                "note_identity_fingerprint": observation["note_identity_fingerprint"],
                "device_identity_fingerprint": observation.get("device_identity_fingerprint"),
            },
            "scope": dict(SCOPE),
            "unavailable_domains": list(UNAVAILABLE),
        }
        result = {
            "schema_version": 1,
            "outcome": "previewed",
            "preview_token": token,
            "preview_fingerprint": _digest(approved),
            "preview": approved,
            "observation": observation,
        }
        require_response_capacity({"success": True, "value": result})
        self._previews[token] = {
            "record": record,
            "song": song,
            "cohort": cohort,
            "device_cohort": device_cohort,
            "result": copy.deepcopy(result),
            "used": False,
        }
        return result

    def _supplement(
        self,
        request: dict[str, Any],
        before: dict[str, Any],
        after: dict[str, Any],
        actual: dict[str, Any],
        operation: dict[str, Any],
    ) -> dict[str, Any]:
        guard = request["approved_preview"]["binding_guard"]
        return {
            "approved_preview": copy.deepcopy(request["approved_preview"]),
            "preview_fingerprint": request["preview_fingerprint"],
            "before": before,
            "after": after,
            "desired": copy.deepcopy(request["desired"]),
            "started_fields": list(operation["song_settings_progress"]["started_fields"]),
            "returned_fields": list(operation["song_settings_progress"]["returned_fields"]),
            "authority_scope": "explicit_current_set_settings",
            "historical_song_identity_proven": False,
            "desired_settings_match": after["settings"] == request["desired"],
            "observed_untouched_state_preserved": untouched(before, after),
            "clip_and_note_ids_preserved": actual["content_fingerprint"]
            == guard["content_fingerprint"]
            and actual["note_identity_fingerprint"] == guard["note_identity_fingerprint"]
            and actual.get("device_identity_fingerprint") == guard["device_identity_fingerprint"],
        }

    def apply(
        self, binding: tuple[str, str], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        """Apply only exact approved scalar differences, with retained partial evidence."""
        if not valid_request(APPLY_METHOD, request) or binding != (
            request["project_key"],
            request["binding_key"],
        ):
            raise RuntimeError("SongSettingsUnavailable: malformed explicit approval")
        song = self._current(request)
        preview = self._previews.get(request["preview_token"])
        if preview is None or preview["used"]:
            raise RuntimeError("SongSettingsUnavailable: preview is absent or consumed")
        preview["used"] = True
        approved = preview["result"]["preview"]
        if (
            any(
                request[name] != approved[name]
                for name in ("project_key", "binding_key", "preview_token")
            )
            or request["document_token"] != approved["context"]["document_token"]
            or request["expected_content_fingerprint"]
            != approved["binding_guard"]["content_fingerprint"]
            or request["expected_note_identity_fingerprint"]
            != approved["binding_guard"]["note_identity_fingerprint"]
        ):
            raise RuntimeError("SongSettingsUnavailable: logical approval differs from preview")
        if (
            _digest(request["approved_preview"]) != preview["result"]["preview_fingerprint"]
            or request["preview_fingerprint"] != preview["result"]["preview_fingerprint"]
            or _digest(request["desired"]) != _digest(approved["desired"])
        ):
            raise RuntimeError(
                "SongSettingsUnavailable: explicit approval differs from current preview"
            )
        record, before_binding = self._binding(request)
        device_cohort = self.registry._devices.private_cohort(record)
        if (
            before_binding.get("device_identity_fingerprint")
            != approved["binding_guard"]["device_identity_fingerprint"]
            or len(device_cohort) != len(preview["device_cohort"])
            or any(
                not self.registry._same(a, b)
                for a, b in zip(device_cohort, preview["device_cohort"])
            )
        ):
            raise RuntimeError(
                "SongSettingsUnavailable: approved device/parameter objects or state changed"
            )
        before, cohort = self._capture(song)
        if (
            record is not preview["record"]
            or not self.registry._same(song, preview["song"])
            or len(cohort) != len(preview["cohort"])
            or any(not self.registry._same(a, b) for a, b in zip(cohort, preview["cohort"]))
            or _digest(before) != _digest(approved["before"])
        ):
            raise RuntimeError(
                "SongSettingsUnavailable: current objects or finite settings changed after preview"
            )
        self._idle(before, request["desired"])
        operation["song_settings_progress"] = {"started_fields": [], "returned_fields": []}
        changed = changed_fields(before, request["desired"])
        anticipated = copy.deepcopy(before)
        anticipated["settings"] = copy.deepcopy(request["desired"])
        prospective = {
            **before_binding,
            "song_settings": self._supplement(
                request, before, anticipated, before_binding, operation
            ),
        }
        future = {
            **operation,
            "song_settings_progress": {"started_fields": changed, "returned_fields": changed},
        }
        prospective["song_settings"] = self._supplement(
            request, before, anticipated, before_binding, future
        )
        guard_managed_response_capacity(future, prospective)
        partial = {
            **operation,
            "outcome": "indeterminate",
            "native_mutation_started": bool(changed),
            "error": "\U0010ffff" * 1024,
            "song_settings_partial": {
                "before": before,
                "after": anticipated,
                "desired": request["desired"],
                "started_fields": changed,
                "returned_fields": changed,
            },
        }
        require_response_capacity({"success": True, "value": partial})

        def capture_phase() -> tuple[dict[str, Any], dict[str, Any]]:
            current, current_cohort = self._capture(song)
            if len(current_cohort) != len(cohort) or any(
                not self.registry._same(a, b) for a, b in zip(current_cohort, cohort)
            ):
                raise RuntimeError(
                    "SongSettingsUnavailable: native object cohort changed during setters"
                )
            if not untouched(before, current):
                raise RuntimeError(
                    "SongSettingsUnavailable: untouched finite Song state changed during setters"
                )
            current_record, observed = self._binding(request)
            current_device_cohort = self.registry._devices.private_cohort(current_record)
            if (
                current_record is not record
                or observed.get("device_identity_fingerprint")
                != approved["binding_guard"]["device_identity_fingerprint"]
                or len(current_device_cohort) != len(device_cohort)
                or any(
                    not self.registry._same(a, b)
                    for a, b in zip(current_device_cohort, device_cohort)
                )
            ):
                raise RuntimeError(
                    "SongSettingsUnavailable: retained Part/device state changed during setters"
                )
            return current, observed

        try:
            after, actual = before, before_binding
            for index, name in enumerate(changed):
                # The exact native Song object is retained; no index/name lookup or seek.
                operation["song_settings_progress"]["started_fields"].append(name)
                operation["native_mutation_started"] = True
                setattr(song, name, request["desired"][name])
                returned = operation["song_settings_progress"]["returned_fields"]
                returned.append(name)
                after, actual = capture_phase()
                # Every still-untouched scalar retains its original value. A mismatch
                # in an earlier phase never permits a later native setter. Final
                # scalar clamps may be acknowledged truthful completed mismatches.
                unwritten = [field for field in FIELDS if field not in returned]
                if any(
                    after["settings"][field] != before["settings"][field] for field in unwritten
                ):
                    raise RuntimeError(
                        "SongSettingsUnavailable: unwritten scalar changed during setters"
                    )
                required_written = returned if index + 1 < len(changed) else returned[:-1]
                if any(
                    after["settings"][field] != request["desired"][field]
                    for field in required_written
                ):
                    raise RuntimeError(
                        "SongSettingsUnavailable: completed scalar differs before the next setter"
                    )
            if not changed:
                after, actual = capture_phase()
            actual["song_settings"] = self._supplement(request, before, after, actual, operation)
            guard_managed_response_capacity(operation, actual)
            return actual
        except Exception:
            if operation.get("native_mutation_started"):
                try:
                    after, _ = self._capture(song)
                    candidate = {
                        "before": before,
                        "after": after,
                        "desired": request["desired"],
                        **operation["song_settings_progress"],
                    }
                    require_response_capacity(
                        {
                            "success": True,
                            "value": {
                                **operation,
                                "outcome": "indeterminate",
                                "error": "\U0010ffff" * 1024,
                                "song_settings_partial": candidate,
                            },
                        }
                    )
                    operation["song_settings_partial"] = candidate
                except Exception:
                    operation["song_settings_partial_unavailable"] = True
            raise
