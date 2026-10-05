"""Finite current-object Mixer grants and once-only static writes; no host qualification."""

from __future__ import annotations

import copy
import math
import uuid
from decimal import ROUND_HALF_EVEN, Decimal, localcontext
from typing import Any

from .managed import _digest, _fingerprint, _key
from .managed_capacity import guard_managed_response_capacity, require_response_capacity
from .native_control import check_native_peer, native_call
from .native_mixer_units import _MixerObservation, resolve_native_mixer_display_value
from .native_units import _parse

PREVIEW_METHOD = "sunny_managed_preview_static_mixer"
INSPECTION_METHOD = "sunny_managed_inspect_static_mixer"
ADOPT_METHOD = "sunny_managed_adopt_static_mixer"
UPDATE_METHOD = "sunny_managed_update_static_mixer"
DOMAINS = ("volume", "pan", "mute", "solo")
MAX_PREVIEWS = 256
MAX_TRACKS = 4096
SONG_FLAGS = (
    "is_playing",
    "is_counting_in",
    "record_mode",
    "session_record",
    "session_automation_record",
    "arrangement_overdub",
    "overdub",
)
UNAVAILABLE = (
    "audio_dsp_equivalence",
    "programme_loudness",
    "pan_law",
    "depth_elevation_width",
    "other_static_mixer_roles",
    "routing_returns_sends_groups_master",
    "unknown_envelope_population",
    "unknown_device_opaque_state",
    "historical_native_identity",
)


def _closed(value: Any, fields: tuple[str, ...]) -> bool:
    return type(value) is dict and set(value) == set(fields)


def desired_valid(value: Any, domains: Any) -> bool:
    """Exactly the approved finite mask, never arbitrary future roles."""
    if (
        type(domains) is not list
        or not domains
        or len(domains) > 4
        or domains != [name for name in DOMAINS if name in domains]
        or type(value) is not dict
        or set(value) != set(domains)
    ):
        return False
    for name, item in value.items():
        if name == "volume":
            if (
                not _closed(item, ("target", "tolerance"))
                or any(
                    type(item[k]) not in (int, float) or not math.isfinite(item[k])
                    for k in ("target", "tolerance")
                )
                or item["tolerance"] < 0
            ):
                return False
        elif name == "pan":
            if type(item) not in (int, float) or not math.isfinite(item) or not -1 <= item <= 1:
                return False
        elif type(item) is not bool:
            return False
    return True


def valid_request(method: str, value: Any) -> bool:
    """Closed retained-binding requests; native descriptors are internally observed."""
    fields: tuple[str, ...] = (
        "document_token",
        "project_key",
        "binding_key",
        "expected_content_fingerprint",
        "expected_note_identity_fingerprint",
        "purpose",
        "selected_domains",
        "desired",
    )
    if method in (ADOPT_METHOD, UPDATE_METHOD):
        fields += (
            "operation_id",
            "preview_token",
            "preview_fingerprint",
            "approved_preview",
            "explicit_current_mixer_approval",
            "explicit_set_wide_audible_approval",
        )
    if method not in (
        PREVIEW_METHOD,
        INSPECTION_METHOD,
        ADOPT_METHOD,
        UPDATE_METHOD,
    ) or not _closed(value, fields):
        return False
    if (
        not all(_key(value[n]) for n in ("document_token", "project_key", "binding_key"))
        or not all(
            _fingerprint(value[n])
            for n in ("expected_content_fingerprint", "expected_note_identity_fingerprint")
        )
        or (
            value["purpose"] != "inspect"
            if method == INSPECTION_METHOD
            else value["purpose"] not in ("adopt", "update")
        )
        or not desired_valid(value["desired"], value["selected_domains"])
    ):
        return False
    if method in (PREVIEW_METHOD, INSPECTION_METHOD):
        return True
    return (
        value["purpose"] == ("adopt" if method == ADOPT_METHOD else "update")
        and _key(value["operation_id"])
        and type(value["preview_token"]) is str
        and len(value["preview_token"]) == 32
        and all(c in "0123456789abcdef" for c in value["preview_token"])
        and _fingerprint(value["preview_fingerprint"])
        and type(value["approved_preview"]) is dict
        and value["explicit_current_mixer_approval"] is True
        and type(value["explicit_set_wide_audible_approval"]) is bool
        and (
            "solo" not in value["selected_domains"]
            or value["explicit_set_wide_audible_approval"] is True
        )
    )


class ManagedMixer:
    """The sole Registry owns journals; this helper retains only actual grants/previews."""

    def __init__(self, registry: Any) -> None:
        self.registry = registry
        self._previews: dict[str, Any] = {}
        self._seal_context: Any = None

    def _same_cohort(self, left: tuple[Any, ...], right: tuple[Any, ...]) -> bool:
        return len(left) == len(right) and all(
            self.registry._same(a, b) for a, b in zip(left, right)
        )

    def _role_handles(self, record: Any) -> tuple[Any, ...]:
        track = record["track"]
        mixer = track.mixer_device
        import Live

        controls = (mixer.volume, mixer.panning, mixer.track_activator)
        if (
            not isinstance(track, Live.Track.Track)
            or not isinstance(mixer, Live.MixerDevice.MixerDevice)
            or not self.registry._same(mixer.canonical_parent, track)
            or any(
                not isinstance(p, Live.DeviceParameter.DeviceParameter)
                or not self.registry._same(p.canonical_parent, mixer)
                for p in controls
            )
            or any(
                self.registry._same(a, b) for i, a in enumerate(controls) for b in controls[i + 1 :]
            )
        ):
            raise RuntimeError(
                "StaticMixerAuthorityUnavailable: actual native control parents/types"
            )
        return (track, record["clip"], mixer, *controls)

    def _control_state(self, record: Any, domains: list[str]) -> dict[str, Any]:
        track, mixer = record["track"], record["track"].mixer_device
        values: dict[str, Any] = {
            "panning_mode": int(mixer.panning_mode),
            "crossfade_assign": int(mixer.crossfade_assign),
        }
        for name in domains:
            values[name] = (
                mixer.volume.value
                if name == "volume"
                else mixer.panning.value
                if name == "pan"
                else getattr(track, name)
            )
        if "mute" in domains:
            values["track_activator_value"] = mixer.track_activator.value
        return values

    def seal(self, record: Any, result: Any) -> None:
        """Only exact pending static operation may advance an existing finite control guard."""
        grant = record.get("_managed_mixer")
        if grant is None:
            return
        context = self._seal_context
        if context is None:
            self.verify_retained_mixer(record)
            return
        operation = context["operation"]
        if (
            context["record"] is not record
            or context["grant"] is not grant
            or operation.get("outcome") != "pending"
            or operation.get("name") != UPDATE_METHOD
            or operation.get("request_fingerprint")
            != _digest({"name": UPDATE_METHOD, "request": operation.get("request")})
            or not self._same_cohort(grant["handles"], self._role_handles(record))
            or result["content_fingerprint"] != context["after"]["content_fingerprint"]
            or result["note_identity_fingerprint"] != context["after"]["note_identity_fingerprint"]
        ):
            raise RuntimeError("StaticMixerAuthorityUnavailable: no exact pending seal authority")
        grant["guard"] = self._control_state(record, grant["domains"])

    def retain_created_track_authority(self, record: dict[str, Any]) -> None:
        """Only called by original native Track creation, never by ordinary seal/capture."""
        if record.get("authority_origin") == "explicit_adoption" or record.get("_managed_mixer"):
            raise RuntimeError("StaticMixerAuthorityUnavailable: no original creation grant")
        record["_managed_mixer"] = {
            "handles": self._role_handles(record),
            "domains": list(DOMAINS),
            "origin": "managed_track_creation",
            "guard": self._control_state(record, list(DOMAINS)),
        }

    def verify_retained_mixer(self, record: dict[str, Any]) -> None:
        """Verify concrete grant handles and its finite control guard without refreshing it."""
        grant = record.get("_managed_mixer")
        if (
            grant is not None
            and not self._same_cohort(grant["handles"], self._role_handles(record))
            or (
                grant is not None
                and grant["guard"] != self._control_state(record, grant["domains"])
            )
        ):
            raise RuntimeError(
                "StaticMixerAuthorityUnavailable: retained Mixer handles or guarded controls changed"
            )

    def invalidate_retained_cohort(self, record: dict[str, Any]) -> None:
        """Explicit structural owner hook, no baseline or grant inferred from inspection."""
        record.pop("_managed_mixer", None)
        self._previews = {k: v for k, v in self._previews.items() if v["record"] is not record}

    def authority_for_refresh(self, record: Any, candidate: Any) -> Any:
        """Read-only same-handle clean Clip refresh decision; never grants new domains."""
        grant = record.get("_managed_mixer")
        if grant is None:
            return None
        try:
            self.verify_retained_mixer(record)
            if self._same_cohort(grant["handles"], self._role_handles(candidate)):
                return {**grant, "domains": list(grant["domains"])}
        except (RuntimeError, AttributeError):
            pass
        return None

    def _current(self, request: Any) -> tuple[Any, list[int]]:
        song = self.registry._ensure_document()
        if request["document_token"] != self.registry._document_token:
            raise RuntimeError("StaticMixerUnavailable: current document epoch differs")
        app = self.registry._handler._get_application()
        version = [
            getattr(app, n)()
            for n in ("get_major_version", "get_minor_version", "get_bugfix_version")
        ]
        if (
            any(type(v) is not int or v < 0 for v in version)
            or version[0] != 12
            or version[1] not in (3, 4)
        ):
            raise RuntimeError("StaticMixerUnavailable: candidate coverage is Live12.4.x/12.3.x")
        return song, version

    def _record(self, request: Any) -> Any:
        record = self.registry._bindings.get((request["project_key"], request["binding_key"]))
        if record is None:
            raise RuntimeError("StaticMixerUnavailable: current retained Part binding is required")
        return record

    def _snapshot(self, record: Any, request: Any) -> tuple[dict[str, Any], tuple[Any, ...]]:
        import Live

        song, version = self._current(request)
        self.registry._indices(record)
        observation = self.registry._capture(record)
        if observation["note_identity"]["entire_clip_population_observed"] is not True:
            raise RuntimeError("StaticMixerUnavailable: entire native note population required")
        self.registry._handler._step_clip_interval(record["clip"], idle=True)
        if any(
            getattr(record["track"], n) is not False for n in ("arm", "implicit_arm", "is_frozen")
        ):
            raise RuntimeError("StaticMixerUnavailable: armed/frozen context")
        grouped = record["track"].is_grouped
        if type(grouped) is not bool:
            raise RuntimeError("StaticMixerUnavailable: actual grouped state required")
        if grouped:
            routing = getattr(self.registry, "_routing", None)
            if routing is None or routing.verify_retained_group(record) is not True:
                raise RuntimeError("StaticMixerUnavailable: exact current Group grant required")
        flags = {n: getattr(song, n) for n in SONG_FLAGS}
        if any(type(v) is not bool or v for v in flags.values()):
            raise RuntimeError("StaticMixerUnavailable: actual Song must be idle and not recording")
        mixer = record["track"].mixer_device
        native = _MixerObservation(record["track"], mixer, eligible_volume=False).initial
        cohort = (
            song,
            record["track"],
            record["slot"],
            record["clip"],
            mixer,
            *native["objects"],
            *self.registry._devices.private_cohort(record),
        )
        solo_cohort: list[dict[str, Any]] = []
        # Solo's audible coupling has a finite Set-wide observation; no foreign setters.
        for kind, raw in (("track", song.tracks), ("return", song.return_tracks)):
            if isinstance(raw, (str, bytes, dict)):
                raise RuntimeError("StaticMixerUnavailable: invalid native Track collection")
            for index, track in enumerate(raw):
                if (
                    len(solo_cohort) >= MAX_TRACKS
                    or not isinstance(track, Live.Track.Track)
                    or not self.registry._same(track.canonical_parent, song)
                ):
                    raise RuntimeError("StaticMixerUnavailable: actual ordered Track/Return cohort")
                cohort += (track,)
                entry = {"kind": kind, "index": index, "name": track.name}
                if type(entry["name"]) is not str or len(entry["name"].encode("utf-8")) > 1024:
                    raise RuntimeError("StaticMixerUnavailable: invalid Track name")
                for name in ("mute", "solo", "muted_via_solo"):
                    value = getattr(track, name)
                    if type(value) is not bool:
                        raise RuntimeError(
                            "StaticMixerUnavailable: actual Track gate must be Boolean"
                        )
                    entry[name] = value
                solo_cohort.append(entry)
        envelopes = {}
        for role, attribute in (
            ("volume", "volume"),
            ("pan", "panning"),
            ("mute", "track_activator"),
        ):
            if role in request["desired"]:
                lookup = getattr(record["clip"], "automation_envelope", None)
                if not callable(lookup):
                    raise RuntimeError("StaticMixerUnavailable: selected envelope API unavailable")
                envelopes[role] = lookup(getattr(mixer, attribute)) is not None
        return {
            "selected_envelopes": envelopes,
            "binding_observation": observation,
            "mixer_capture": native["mixer_capture"],
            "track_context": native["track_context"],
            "solo_cohort": solo_cohort,
            "song_flags": flags,
            "version": version,
        }, cohort

    def _guard(self, record: Any, request: Any) -> None:
        observed = self.registry._require_in_place_guard(
            record, request["expected_content_fingerprint"]
        )
        if observed["note_identity_fingerprint"] != request["expected_note_identity_fingerprint"]:
            raise RuntimeError("StaticMixerUnavailable: expected native note identities differ")

    def _admit(self, record: Any, desired: Any) -> None:
        from .native_units import _descriptor

        mixer = record["track"].mixer_device
        if any(n in desired for n in ("volume", "pan")) and (
            record["track"].has_audio_output is not True
            or record["track"].has_midi_output is not False
        ):
            raise RuntimeError("StaticMixerUnavailable: audio-output source role required")
        for name, attribute in (
            ("volume", "volume"),
            ("pan", "panning"),
            ("mute", "track_activator"),
        ):
            if name not in desired:
                continue
            parameter = getattr(mixer, attribute)
            quantized = name == "mute"
            descriptor = _descriptor(parameter, quantized=quantized, eligible=True)
            if parameter.is_quantized is not quantized:
                raise RuntimeError("StaticMixerUnavailable: selected control quantization differs")
            if name == "pan" and (
                mixer.panning_mode != 0
                or descriptor["minimum"] != -1.0
                or descriptor["maximum"] != 1.0
            ):
                raise RuntimeError(
                    "StaticMixerUnavailable: only actual Stereo [-1,+1] pan coordinate"
                )
            if name == "mute" and (
                descriptor["minimum"] != 0.0
                or descriptor["maximum"] != 1.0
                or descriptor["value"] != (0.0 if record["track"].mute else 1.0)
            ):
                raise RuntimeError(
                    "StaticMixerUnavailable: actual Track Activator does not mirror mute"
                )
            if not callable(getattr(record["clip"], "automation_envelope", None)):
                raise RuntimeError(
                    "StaticMixerUnavailable: selected native envelope admission unavailable"
                )
            if record["clip"].automation_envelope(parameter) is not None:
                raise RuntimeError("StaticMixerUnavailable: existing selected envelope preserved")

    def _readonly_capture(
        self, request: dict[str, Any], *, inspection: bool
    ) -> tuple[dict[str, Any], Any, tuple[Any, ...]]:
        """Observe eligible selected current controls; do not allocate authority."""
        method = INSPECTION_METHOD if inspection else PREVIEW_METHOD
        if not valid_request(method, request):
            raise RuntimeError("StaticMixerUnavailable: invalid closed read-only request")
        record = self._record(request)
        self._guard(record, request)
        self._admit(record, request["desired"])
        if request["purpose"] == "update":
            self._authority(record, request["selected_domains"])
        before, cohort = self._snapshot(record, request)

        def check() -> None:
            current, handles = self._snapshot(record, request)
            if current != before or not self._same_cohort(cohort, handles):
                raise RuntimeError(
                    "StaticMixerUnavailable: actual context changed during formatter"
                )

        candidates = {}
        if "volume" in request["desired"]:
            selected = request["desired"]["volume"]
            candidates["volume"] = resolve_native_mixer_display_value(
                record["track"],
                mixer=record["track"].mixer_device,
                parameter_kind="volume",
                target=selected["target"],
                tolerance=selected["tolerance"],
                context_check=check,
            )
        check()
        grant = record.get("_managed_mixer")
        body = {
            "schema_version": 1,
            "context": {
                "bridge_instance": self.registry._bridge_instance,
                "document_token": self.registry._document_token,
            },
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "purpose": request["purpose"],
            "selected_domains": list(request["selected_domains"]),
            "desired": copy.deepcopy(request["desired"]),
            "before": before,
            "candidates": candidates,
            "current_authority_domains": [] if grant is None else list(grant["domains"]),
            "scope": {
                "set_wide_audible_effect": "solo" in request["desired"],
                "historical_identity_proven": False,
                "native_knob_only": True,
                "host_qualified": False,
            },
            "unavailable_domains": list(UNAVAILABLE),
        }
        if inspection:
            body.update(authority_origin="none", native_mutation_started=False)
        return body, record, cohort

    def inspect(self, request: dict[str, Any]) -> dict[str, Any]:
        """Fresh finite control evidence; retain no preview, grant or journal."""
        body, _, _ = self._readonly_capture(request, inspection=True)
        result = {"schema_version": 1, "outcome": "observed", "inspection": body}
        require_response_capacity({"success": True, "value": result})
        return result

    def preview(self, request: dict[str, Any]) -> dict[str, Any]:
        """Read selected controls for separate approval, without any native setter."""
        if len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("StaticMixerUnavailable: preview capacity256")
        preview, record, cohort = self._readonly_capture(request, inspection=False)
        token = ""
        for _ in range(16):
            candidate_token = uuid.uuid4().hex
            if (
                type(candidate_token) is str
                and len(candidate_token) == 32
                and all(c in "0123456789abcdef" for c in candidate_token)
                and candidate_token not in self._previews
            ):
                token = candidate_token
                break
        if not token:
            raise RuntimeError("StaticMixerUnavailable: preview token collision bound exhausted")
        preview["preview_token"] = token
        result = {
            "schema_version": 1,
            "outcome": "previewed",
            "preview_token": token,
            "preview_fingerprint": _digest(preview),
            "preview": preview,
        }
        require_response_capacity({"success": True, "value": result})
        self._previews[token] = {
            "record": record,
            "cohort": cohort,
            "request": copy.deepcopy(request),
            "preview": copy.deepcopy(preview),
            "used": False,
        }
        return result

    def _authority(self, record: Any, domains: Any) -> None:
        self.verify_retained_mixer(record)
        grant = record.get("_managed_mixer")
        if grant is None or not set(domains) <= set(grant["domains"]):
            raise RuntimeError(
                "StaticMixerAuthorityUnavailable: explicit current selected-domain adoption required"
            )

    def _approved(self, method: str, record: Any, request: Any, operation: Any) -> Any:
        if (
            not valid_request(method, request)
            or operation.get("outcome") != "pending"
            or operation.get("request") != request
            or operation.get("name") != method
            or operation.get("request_fingerprint") != _digest({"name": method, "request": request})
            or operation.get("native_mutation_started") is not False
        ):
            raise RuntimeError("StaticMixerUnavailable: exact sole pending operation required")
        retained = self._previews.get(request["preview_token"])
        if retained is None or retained["used"] or retained["record"] is not record:
            raise RuntimeError("StaticMixerUnavailable: exact retained preview absent/consumed")
        retained["used"] = True
        approved = retained["preview"]
        if (
            request["approved_preview"] != approved
            or request["preview_fingerprint"] != _digest(approved)
            or any(request[n] != retained["request"][n] for n in retained["request"])
        ):
            raise RuntimeError(
                "StaticMixerUnavailable: approval differs from actual retained preview"
            )
        self._guard(record, request)
        self._admit(record, request["desired"])
        before, cohort = self._snapshot(record, request)
        if before != approved["before"] or not self._same_cohort(cohort, retained["cohort"]):
            raise RuntimeError(
                "StaticMixerUnavailable: current objects/state changed after preview"
            )
        candidate = approved["candidates"].get("volume")
        if candidate is not None:
            parameter = record["track"].mixer_device.volume
            if (
                parameter.str_for_value(candidate["internal_value"]) != candidate["display"]
                or parameter.str_for_value(parameter.value)
                != candidate["current_display"]["display"]
            ):
                raise RuntimeError(
                    "StaticMixerUnavailable: native candidate/current formatter drifted"
                )
            final, handles = self._snapshot(record, request)
            if final != before or not self._same_cohort(handles, retained["cohort"]):
                raise RuntimeError(
                    "StaticMixerUnavailable: formatter changed actual current context"
                )
        return retained

    def adopt(self, record: Any, request: Any, operation: Any) -> dict[str, Any]:
        """Fenced current-object grant only; no native setter or historical identity."""
        retained = self._approved(ADOPT_METHOD, record, request, operation)
        supplement = {
            "approved_preview": copy.deepcopy(retained["preview"]),
            "preview_fingerprint": request["preview_fingerprint"],
            "authority_origin": "explicit_current_mixer_adoption",
            "granted_domains": list(request["selected_domains"]),
            "native_mutation_started": False,
            "historical_identity_proven": False,
        }
        observed = retained["preview"]["before"]["binding_observation"]
        guard_managed_response_capacity(
            operation,
            {
                **observed,
                "observed_notes_match_request": False,
                "observed_clip_properties_match_request": False,
                "mixer_adoption": supplement,
            },
        )
        previous = record.get("_managed_mixer")
        old_content, old_notes = (
            record.get("content_fingerprint"),
            record.get("note_identity_fingerprint"),
        )
        record["_managed_mixer"] = {
            "handles": self._role_handles(record),
            "domains": list(request["selected_domains"]),
            "origin": "explicit_current_mixer_adoption",
            "guard": self._control_state(record, request["selected_domains"]),
        }
        try:
            result: dict[str, Any] = self.registry._seal(record)
            if (
                result["content_fingerprint"] != observed["content_fingerprint"]
                or result["note_identity_fingerprint"] != observed["note_identity_fingerprint"]
            ):
                raise RuntimeError(
                    "StaticMixerUnavailable: authority publication changed native content"
                )
        except Exception:
            record["content_fingerprint"], record["note_identity_fingerprint"] = (
                old_content,
                old_notes,
            )
            if previous is None:
                record.pop("_managed_mixer", None)
            else:
                record["_managed_mixer"] = previous
            raise
        result["mixer_adoption"] = supplement
        return result

    @staticmethod
    def _untouched(before: Any, after: Any, returned: list[str]) -> bool:
        left, right = copy.deepcopy(before), copy.deepcopy(after)
        # Remove only the declared native deltas; all hashes are recomputed from actual payloads.
        for value in (left, right):
            observation = value["binding_observation"]
            observation.pop("content_fingerprint")
            manifest = observation["manifest"]
            if "volume" in returned:
                manifest["mixer"]["volume"].pop("value")
                value["mixer_capture"]["parameters"][0]["descriptor"].pop("value")
            if "pan" in returned:
                manifest["mixer"]["panning"].pop("value")
                value["mixer_capture"]["parameters"][1]["descriptor"].pop("value")
            if "mute" in returned:
                manifest["track"].pop("mute")
                manifest["mixer"]["track_activator"].pop("value")
                value["track_context"].pop("mute")
                value["mixer_capture"]["parameters"][2]["descriptor"].pop("value")
                value["mixer_capture"]["parameters"][2]["descriptor"].pop("label", None)
                value["solo_cohort"][observation["track_index"]].pop("mute")
            if "solo" in returned:
                manifest["track"].pop("solo")
                value["track_context"].pop("solo")
                value["solo_cohort"][observation["track_index"]].pop("solo")
                for track in value["solo_cohort"]:
                    track.pop("muted_via_solo")
        return bool(_digest(left) == _digest(right))

    def _readback(self, record: Any, desired: Any, candidate: Any) -> dict[str, Any]:
        result = {}
        mixer = record["track"].mixer_device
        if "volume" in desired:
            value = mixer.volume.value
            raw = mixer.volume.str_for_value(value)
            numeric, increment = _parse(raw, "Decibels", gain_infinity=True)
            target, tolerance = (
                Decimal(str(desired["volume"][n])) for n in ("target", "tolerance")
            )
            with localcontext() as context:
                context.prec = 28
                context.rounding = ROUND_HALF_EVEN
                matches = numeric.is_finite() and abs(numeric - target) <= tolerance
            result["volume"] = {
                "internal_value": value,
                "display": raw,
                "display_value": float(numeric) if numeric.is_finite() else None,
                "display_increment": float(increment) if increment is not None else None,
                "negative_infinity": not numeric.is_finite(),
                "matches": matches,
            }
        if "pan" in desired:
            result["pan"] = {
                "internal_value": mixer.panning.value,
                "matches": mixer.panning.value == float(desired["pan"]),
            }
        for name in ("mute", "solo"):
            if name in desired:
                value = getattr(record["track"], name)
                result[name] = {"value": value, "matches": value is desired[name]}
                if name == "mute":
                    activator = mixer.track_activator.value
                    result[name]["track_activator_value"] = activator
                    result[name]["matches"] &= activator == (0.0 if value else 1.0)
        return result

    def apply(self, record: Any, request: Any, operation: Any) -> dict[str, Any]:
        """Write only admitted selected values, with exact per-phase preservation checks."""
        old_content, old_notes = (
            record.get("content_fingerprint"),
            record.get("note_identity_fingerprint"),
        )
        original_grant = record.get("_managed_mixer")
        old_grant = (
            None
            if original_grant is None
            else {**original_grant, "guard": copy.deepcopy(original_grant["guard"])}
        )
        retained = self._approved(UPDATE_METHOD, record, request, operation)
        self._authority(record, request["selected_domains"])
        approved = retained["preview"]
        before = approved["before"]
        desired = request["desired"]
        candidate = approved["candidates"].get("volume")
        initial, handles = self._snapshot(record, request)
        if initial != before or not self._same_cohort(handles, retained["cohort"]):
            raise RuntimeError("StaticMixerUnavailable: candidate repeat changed current context")
        actual_values = {
            "volume": record["track"].mixer_device.volume.value,
            "pan": record["track"].mixer_device.panning.value,
            "mute": record["track"].mute,
            "solo": record["track"].solo,
        }
        targets = {
            name: (
                candidate["internal_value"]
                if name == "volume"
                else float(desired[name])
                if name == "pan"
                else desired[name]
            )
            for name in desired
        }
        changed = [
            name for name in DOMAINS if name in desired and targets[name] != actual_values[name]
        ]
        progress: dict[str, list[str]] = {"started_fields": [], "returned_fields": []}
        operation["mixer_progress"] = progress
        prospective = {
            **before["binding_observation"],
            "observed_notes_match_request": False,
            "observed_clip_properties_match_request": False,
            "mixer_update": {
                "approved_preview": approved,
                "before": before,
                "after": before,
                "desired": desired,
                "readback": {},
                "started_fields": changed,
                "returned_fields": changed,
                "unavailable_domains": list(UNAVAILABLE),
            },
        }
        # Conservative strings for all possible formatter readback and bounded failure text.
        prospective["mixer_update"]["readback"] = {
            n: {
                "display": "\U0010ffff" * 80,
                "internal_value": 0.0,
                "display_value": 0.0,
                "display_increment": 0.0,
                "negative_infinity": False,
                "matches": True,
                "value": False,
                "track_activator_value": 0.0,
            }
            for n in desired
        }
        guard_managed_response_capacity(
            {
                **operation,
                "mixer_progress": {"started_fields": changed, "returned_fields": changed},
            },
            prospective,
        )
        require_response_capacity(
            {
                "success": True,
                "value": {
                    **operation,
                    "outcome": "indeterminate",
                    "error": "\U0010ffff" * 1024,
                    "mixer_partial": prospective["mixer_update"],
                },
            }
        )
        after = before
        try:
            for name in changed:
                current, handles = self._snapshot(record, request)
                if not self._same_cohort(handles, retained["cohort"]) or not self._untouched(
                    before, current, progress["returned_fields"]
                ):
                    raise RuntimeError(
                        "StaticMixerUnavailable: current handles/untouched state changed before setter"
                    )
                self._admit(record, desired)
                check_native_peer()
                progress["started_fields"].append(name)
                operation["native_mutation_started"] = True
                if name in ("volume", "pan"):
                    native_call(
                        setattr,
                        getattr(
                            record["track"].mixer_device,
                            "volume" if name == "volume" else "panning",
                        ),
                        "value",
                        targets[name],
                    )
                else:
                    native_call(setattr, record["track"], name, targets[name])
                progress["returned_fields"].append(name)
                after, handles = self._snapshot(record, request)
                if not self._same_cohort(handles, retained["cohort"]) or not self._untouched(
                    before, after, progress["returned_fields"]
                ):
                    raise RuntimeError(
                        "StaticMixerUnavailable: native setter changed untouched state"
                    )
                for returned in progress["returned_fields"]:
                    selected = self._readback(record, {returned: desired[returned]}, candidate)
                    if not selected[returned]["matches"]:
                        raise RuntimeError(
                            "StaticMixerUnavailable: actual native readback mismatch"
                        )
                final_phase, handles = self._snapshot(record, request)
                if final_phase != after or not self._same_cohort(handles, retained["cohort"]):
                    raise RuntimeError(
                        "StaticMixerUnavailable: formatter changed actual native state"
                    )
            readback = self._readback(record, desired, candidate)
            after, handles = self._snapshot(record, request)
            if (
                not self._same_cohort(handles, retained["cohort"])
                or not self._untouched(before, after, progress["returned_fields"])
                or not all(item["matches"] for item in readback.values())
            ):
                raise RuntimeError(
                    "StaticMixerUnavailable: final actual native readback/untouched mismatch"
                )
            supplement = {
                "approved_preview": copy.deepcopy(approved),
                "preview_fingerprint": request["preview_fingerprint"],
                "before": before,
                "after": after,
                "desired": copy.deepcopy(desired),
                "readback": readback,
                **copy.deepcopy(progress),
                "observed_untouched_state_preserved": True,
                "clip_and_note_ids_preserved": True,
                "authority_scope": "retained_selected_static_mixer_domains",
                "native_knob_only": True,
                "host_qualified": False,
                "set_wide_audible_effect": "solo" in desired,
                "unavailable_domains": list(UNAVAILABLE),
            }
            guard_managed_response_capacity(
                operation, {**after["binding_observation"], "mixer_update": supplement}
            )
            self._seal_context = {
                "record": record,
                "operation": operation,
                "grant": original_grant,
                "after": after["binding_observation"],
            }
            try:
                result: dict[str, Any] = self.registry._seal(record)
            finally:
                self._seal_context = None
            if (
                result["content_fingerprint"] != after["binding_observation"]["content_fingerprint"]
                or result["note_identity_fingerprint"]
                != before["binding_observation"]["note_identity_fingerprint"]
            ):
                raise RuntimeError("StaticMixerUnavailable: final binding publication mismatch")
            result["mixer_update"] = supplement
            return result
        except Exception:
            if old_grant is not None:
                record["_managed_mixer"] = old_grant
            record["content_fingerprint"], record["note_identity_fingerprint"] = (
                old_content,
                old_notes,
            )
            try:
                after, _ = self._snapshot(record, request)
            except Exception:
                after = None
            operation["mixer_partial"] = {
                "before": before,
                "after": after,
                "observed_after_available": after is not None,
                "desired": copy.deepcopy(desired),
                **copy.deepcopy(progress),
            }
            raise
