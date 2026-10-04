"""Explicit adoption of selected current native Clip objects, without historical identity.

All calls run in the existing main-thread managed dispatch. Previews retain
actual handles privately and never install a binding. Adoption changes only
bridge authority: it never renames, reconstructs, or otherwise writes Live.
"""

from __future__ import annotations

import copy
import uuid
from typing import Any

from .managed import _digest, _fingerprint, _key
from .managed_capacity import guard_managed_response_capacity, require_response_capacity

ADOPTION_SCHEMA_VERSION = 1
MAX_ADOPTION_PREVIEWS = 256
ALLOWED_DOMAINS = (
    "existing_note_updates",
    "note_population_updates",
    "clip_geometry_updates",
    "absent_mixer_step_lanes",
)
PRESERVED_UNKNOWN_DOMAINS = ("mpe", "follow_actions", "existing_envelopes", "devices")


def _token(value: Any) -> bool:
    return type(value) is str and len(value) == 32 and all(c in "0123456789abcdef" for c in value)


def _text(value: Any, limit: int, *, empty: bool = False) -> bool:
    if type(value) is not str or (not empty and not value):
        return False
    try:
        return len(value.encode("utf-8")) <= limit
    except UnicodeEncodeError:
        return False


def _selector(value: Any) -> bool:
    if type(value) is not dict:
        return False
    if set(value) == {"track_index", "slot_index"}:
        return all(type(index) is int and 0 <= index < (1 << 31) for index in value.values())
    if set(value) == {"track_tag", "clip_tag"}:
        return all(_text(tag, 1024) for tag in value.values())
    return False


def valid_preview_request(value: Any) -> bool:
    """Closed selection request; neither names nor indices assert native ownership."""
    return (
        type(value) is dict
        and set(value) == {"document_token", "project_key", "binding_key", "selector"}
        and all(_key(value[name]) for name in ("document_token", "project_key", "binding_key"))
        and _selector(value["selector"])
    )


def valid_adoption_request(value: Any) -> bool:
    """Explicit current-preview approval; requires a product dispatch fence before delivery."""
    return (
        type(value) is dict
        and set(value)
        == {
            "document_token",
            "operation_id",
            "project_key",
            "binding_key",
            "preview_token",
            "preview_fingerprint",
            "explicit_adoption",
        }
        and all(
            _key(value[name])
            for name in ("document_token", "operation_id", "project_key", "binding_key")
        )
        and _token(value["preview_token"])
        and _fingerprint(value["preview_fingerprint"])
        and value["explicit_adoption"] is True
    )


class ManagedRecovery:
    """Keep bounded single-use current-object previews for one managed epoch."""

    def __init__(self, registry: Any, device_authorizer: Any = None) -> None:
        """Attach to the current registry without granting any native authority."""
        self._registry = registry
        self._device_authorizer = device_authorizer
        self._epoch: tuple[str, str] | None = None
        self._previews: dict[str, dict[str, Any]] = {}

    def _current(self, request: dict[str, Any]) -> Any:
        registry = self._registry
        song = registry._ensure_document()
        epoch = (registry._bridge_instance, registry._document_token)
        if self._epoch != epoch:
            self._previews.clear()
            self._epoch = epoch
        if request["document_token"] != epoch[1]:
            raise RuntimeError("AdoptionUnavailable: current document epoch differs")
        return song

    @staticmethod
    def _set_info(song: Any) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for name, limit in (("file_path", 4096), ("name", 1024)):
            try:
                value = getattr(song, name)
            except AttributeError:
                value = None
            if value is not None and not _text(value, limit, empty=True):
                raise RuntimeError(f"AdoptionUnavailable: invalid observed Set {name}")
            result[name] = value
        return result

    def _select(self, song: Any, request: dict[str, Any]) -> dict[str, Any]:
        selector = request["selector"]
        tracks = tuple(song.tracks)
        if "track_index" in selector:
            track_index = selector["track_index"]
            if track_index >= len(tracks):
                raise RuntimeError("AdoptionUnavailable: selected Track does not exist")
            track = tracks[track_index]
            slots = tuple(track.clip_slots)
            if selector["slot_index"] >= len(slots):
                raise RuntimeError("AdoptionUnavailable: selected ClipSlot does not exist")
            slot = slots[selector["slot_index"]]
        else:
            selected = [track for track in tracks if track.name == selector["track_tag"]]
            if len(selected) != 1:
                raise RuntimeError("AdoptionUnavailable: Track locator is absent or ambiguous")
            track = selected[0]
            selected = [
                slot
                for slot in track.clip_slots
                if slot.has_clip is True and slot.clip.name == selector["clip_tag"]
            ]
            if len(selected) != 1:
                raise RuntimeError("AdoptionUnavailable: Clip locator is absent or ambiguous")
            slot = selected[0]
        if slot.has_clip is not True or slot.clip is None:
            raise RuntimeError("AdoptionUnavailable: selected ClipSlot is empty")
        track_tag, clip_tag = self._registry._tags(request["project_key"], request["binding_key"])
        # Discovery labels remain actual observed labels in this initial subset.
        # Identical tags on a replacement object still require a fresh adoption.
        if track.name != track_tag or slot.clip.name != clip_tag:
            raise RuntimeError("AdoptionUnavailable: selected current objects lack the Sunny names")
        return {
            "track": track,
            "slot": slot,
            "clip": slot.clip,
            "track_tag": track_tag,
            "clip_tag": clip_tag,
            "_logical_binding": (request["project_key"], request["binding_key"]),
        }

    def _capture(self, song: Any, record: dict[str, Any]) -> dict[str, Any]:
        registry = self._registry
        registry._require_current_group(record)
        if not registry._same(record["track"].canonical_parent, song):
            raise RuntimeError("AdoptionUnavailable: current Track parent differs")
        observed: dict[str, Any] = registry._capture(record)
        if observed["note_identity"]["entire_clip_population_observed"] is not True:
            raise RuntimeError(
                "AdoptionUnavailable: entire current Clip note population is unavailable"
            )
        if len(observed["note_identity"]["notes"]) > 65536:
            raise RuntimeError("AdoptionUnavailable: current Clip note population exceeds 65536")
        registry._handler._step_clip_interval(record["clip"], idle=True)
        if any(
            getattr(record["track"], name) is not False
            for name in ("arm", "implicit_arm", "is_frozen")
        ):
            raise RuntimeError("AdoptionUnavailable: Track must be unarmed and unfrozen")
        if record["track"].name != record["track_tag"] or record["clip"].name != record["clip_tag"]:
            raise RuntimeError("AdoptionUnavailable: selected current names changed")
        return observed

    def _same_devices(self, record: dict[str, Any], devices: tuple[Any, ...]) -> bool:
        actual = tuple(self._registry._handler._device_chain(record["track"]))
        return len(actual) == len(devices) and all(
            self._registry._same(first, second) for first, second in zip(actual, devices)
        )

    def _previous(self, request: dict[str, Any], record: dict[str, Any]) -> Any:
        previous = self._registry._bindings.get((request["project_key"], request["binding_key"]))
        if previous is not None and any(
            not self._registry._same(previous.get(name), record[name])
            for name in ("track", "slot", "clip")
        ):
            raise RuntimeError(
                "AdoptionUnavailable: existing binding retains different native objects"
            )
        return previous

    def _device_authority(self, previous: Any) -> bool:
        if (
            previous is None
            or "_managed_devices" not in previous
            or self._device_authorizer is None
        ):
            return False
        try:
            self._device_authorizer.verify_retained_chain(previous)
            return True
        except Exception:
            # Explicit Clip adoption can preserve this current native cohort
            # without transferring stale logical Device/parameter authority.
            return False

    @staticmethod
    def _candidate_record(
        record: dict[str, Any],
        devices: tuple[Any, ...],
        previous: Any,
        device_authority: bool,
        mixer_authority: Any = None,
    ) -> dict[str, Any]:
        candidate = {**record, "adopted_device_cohort": devices}
        if device_authority:
            state = previous["_managed_devices"]
            # Copy mutable metadata without deep-copying actual native handles.
            candidate["_managed_devices"] = {
                **state,
                "entries": [dict(entry) for entry in state["entries"]],
            }
        candidate.pop("_managed_mixer", None)
        if mixer_authority is not None:
            candidate["_managed_mixer"] = {
                **mixer_authority,
                "domains": list(mixer_authority["domains"]),
                "guard": copy.deepcopy(mixer_authority["guard"]),
            }
        return candidate

    def _mixer_authority(self, previous: Any, candidate: Any) -> Any:
        if previous is None:
            return None
        return self._registry._mixer.authority_for_refresh(previous, candidate)

    @staticmethod
    def _mixer_disposition(authority: Any) -> Any:
        if authority is None:
            return None
        return _digest({name: authority[name] for name in ("domains", "origin", "guard")})

    def preview(self, request: dict[str, Any]) -> dict[str, Any]:
        """Retain current objects/settings for explicit review; execute no setters."""
        """Read and retain concrete current objects; publish no native binding or journal."""
        if not valid_preview_request(request):
            raise RuntimeError("AdoptionUnavailable: malformed preview request")
        song = self._current(request)
        if len(self._previews) >= MAX_ADOPTION_PREVIEWS:
            raise RuntimeError("AdoptionUnavailable: preview capacity exhausted")
        record = self._select(song, request)
        devices = tuple(self._registry._handler._device_chain(record["track"]))
        previous = self._previous(request, record)
        device_authority = self._device_authority(previous)
        mixer_authority = self._mixer_authority(previous, record)
        record = self._candidate_record(
            record, devices, previous, device_authority, mixer_authority
        )
        observation = self._capture(song, record)
        if not self._same_devices(record, devices):
            raise RuntimeError("AdoptionUnavailable: device cohort changed during capture")
        token = ""
        for _ in range(64):
            candidate = uuid.uuid4().hex
            if candidate not in self._previews:
                token = candidate
                break
        if not token:
            raise RuntimeError("AdoptionUnavailable: preview token collision bound exhausted")
        result = {
            "schema_version": ADOPTION_SCHEMA_VERSION,
            "outcome": "previewed",
            "context": {
                "bridge_instance": self._registry._bridge_instance,
                "document_token": self._registry._document_token,
            },
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "preview_token": token,
            "selector": copy.deepcopy(request["selector"]),
            "set_info": self._set_info(song),
            "observation": observation,
            "authority_origin": "none",
            "historical_identity_proven": False,
            "allowed_domains": list(ALLOWED_DOMAINS),
            "preserved_unknown_domains": list(PRESERVED_UNKNOWN_DOMAINS),
        }
        require_response_capacity(
            {"success": True, "value": {**result, "preview_fingerprint": "0" * 64}}
        )
        result["preview_fingerprint"] = _digest(result)
        self._previews[token] = {
            "song": song,
            "record": record,
            "devices": devices,
            "result": copy.deepcopy(result),
            "used": False,
            "previous": previous,
            "device_authority": device_authority,
            "mixer_disposition": self._mixer_disposition(mixer_authority),
        }
        return result

    def adopt(
        self, binding: tuple[str, str], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        """Install only explicitly approved current-object authority, with no Live writes."""
        if not valid_adoption_request(request) or binding != (
            request["project_key"],
            request["binding_key"],
        ):
            raise RuntimeError("AdoptionUnavailable: malformed explicit adoption request")
        song = self._current(request)
        preview = self._previews.get(request["preview_token"])
        if preview is None or preview["used"]:
            raise RuntimeError("AdoptionUnavailable: preview token is absent or already consumed")
        preview["used"] = True
        result = preview["result"]
        if request["preview_fingerprint"] != result["preview_fingerprint"] or binding != (
            result["project_key"],
            result["binding_key"],
        ):
            raise RuntimeError("AdoptionUnavailable: preview approval differs")
        registry = self._registry
        if not registry._same(song, preview["song"]):
            raise RuntimeError("AdoptionUnavailable: actual document changed")
        record = preview["record"]
        previous = self._previous(request, record)
        if previous is not preview["previous"]:
            raise RuntimeError(
                "AdoptionUnavailable: logical binding authority changed after preview"
            )
        device_authority = self._device_authority(previous)
        if device_authority != preview["device_authority"]:
            raise RuntimeError(
                "AdoptionUnavailable: device authority disposition changed after preview"
            )
        mixer_authority = self._mixer_authority(previous, record)
        if self._mixer_disposition(mixer_authority) != preview["mixer_disposition"]:
            raise RuntimeError(
                "AdoptionUnavailable: Mixer authority disposition changed after preview"
            )
        record = self._candidate_record(
            record, preview["devices"], previous, device_authority, mixer_authority
        )
        for key, retained in registry._bindings.items():
            if key == binding:
                continue
            if registry._same(retained["track"], record["track"]) or registry._same(
                retained.get("clip"), record["clip"]
            ):
                raise RuntimeError("AdoptionUnavailable: current objects belong to another binding")
        current = self._capture(song, record)
        if (
            not self._same_devices(record, preview["devices"])
            or self._set_info(song) != result["set_info"]
            or _digest(current) != _digest(result["observation"])
        ):
            raise RuntimeError(
                "AdoptionUnavailable: approved current objects or finite content changed"
            )
        ids = sorted(note["note_id"] for note in current["note_identity"]["notes"])
        candidate = dict(record)
        candidate.update(
            {
                "owned_note_ids": set(ids),
                "adopted_device_cohort": preview["devices"],
                "authority_origin": "explicit_adoption",
                "allowed_domains": tuple(result["allowed_domains"]),
                "content_fingerprint": current["content_fingerprint"],
                "note_identity_fingerprint": current["note_identity_fingerprint"],
            }
        )
        acknowledgement = copy.deepcopy(current)
        acknowledgement["adoption"] = {
            "preview_token": request["preview_token"],
            "preview_fingerprint": request["preview_fingerprint"],
            "authority_origin": "explicit_adoption",
            "historical_identity_proven": False,
            "allowed_domains": list(result["allowed_domains"]),
            "preserved_unknown_domains": list(PRESERVED_UNKNOWN_DOMAINS),
            "approved_note_ids": ids,
            "devices_preserved": True,
            # Reconstruct the approved full preview from this small metadata
            # and the exact returned observation; no duplicated note arrays.
            "preview_metadata": {
                name: copy.deepcopy(value)
                for name, value in result.items()
                if name not in ("observation", "preview_fingerprint")
            },
        }
        # Check the complete journal envelope before publishing bridge authority.
        guard_managed_response_capacity(operation, acknowledgement)
        registry._bindings[binding] = candidate
        return acknowledgement
