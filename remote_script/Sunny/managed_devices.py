"""Finite in-place native-device operations on an already retained Part binding.

The enclosing ManagedRegistry owns epochs, journals, main-thread dispatch and
durable send authority. This helper owns actual Device/parameter cohorts only;
it never adopts an object by name/index or compensates an uncertain insertion.
"""

from __future__ import annotations

import copy
import math
import uuid
from decimal import Decimal
from typing import Any

from .managed import _digest, _fingerprint, _key
from .managed_capacity import guard_managed_response_capacity
from .native_units import (
    NativeUnitError,
    _parse,
    _registered_context,
    _same,
    resolve_registered_native_display_value,
    valid_native_display_request,
)

MAX_DEVICES = 16
MAX_TARGETS = 32
MAX_PARAMETERS = 512
MAX_TEXT_BYTES = 4096
MAX_PREVIEWS = 64
DEVICE_METHODS = frozenset(
    ("sunny_managed_insert_device", "sunny_managed_update_device_parameters")
)
_DEVICES = {
    "Drift": ("Drift", 1, "source"),
    "Utility": ("StereoGain", 2, "effect"),
    "EQ Eight": ("Eq8", 2, "effect"),
}
DEVICE_PREVIEW_METHOD = "sunny_managed_preview_devices"
DEVICE_ADOPTION_METHOD = "sunny_managed_adopt_devices"


def _text(value: Any, maximum: int = MAX_TEXT_BYTES) -> bool:
    return type(value) is str and 0 < len(value.encode("utf-8")) <= maximum


def _number(value: Any) -> bool:
    return type(value) in (int, float) and math.isfinite(float(value))


def _empty_identity() -> dict[str, Any]:
    return {"schema_version": 1, "cohort": [], "opaque_state_observed": False}


def valid_managed_device_request(name: str, args: list[Any]) -> bool:
    """Admit one closed registered request; descriptors/modes are never supplied."""
    try:
        if name in (DEVICE_PREVIEW_METHOD, DEVICE_ADOPTION_METHOD):
            return _valid_adoption_request(name, args)
        if name not in DEVICE_METHODS or len(args) != 1 or type(args[0]) is not dict:
            return False
        value = args[0]
        expected = {
            "document_token",
            "operation_id",
            "project_key",
            "binding_key",
            "expected_content_fingerprint",
            "expected_device_identity_fingerprint",
            "device_key",
            "device",
            "physical_intents",
        }
        if set(value) != expected or any(
            not _key(value[key])
            for key in (
                "document_token",
                "operation_id",
                "project_key",
                "binding_key",
                "device_key",
            )
        ):
            return False
        if any(
            not _fingerprint(value[key])
            for key in ("expected_content_fingerprint", "expected_device_identity_fingerprint")
        ):
            return False
        device = value["device"]
        if type(device) is not dict or set(device) != {
            "browser_name",
            "class_name",
            "type",
            "role",
            "insertion_policy",
        }:
            return False
        registered = _DEVICES.get(device["browser_name"])
        if (
            registered is None
            or (device["class_name"], device["type"], device["role"]) != registered
            or type(device["type"]) is not int
        ):
            return False
        if device["insertion_policy"] != "AppendOwnedChain":
            return False
        intents = value["physical_intents"]
        if type(intents) is not list or len(intents) > MAX_TARGETS:
            return False
        identifiers = set()
        for intent in intents:
            if not valid_native_display_request(intent):
                return False
            if intent["capability_id"] in identifiers:
                return False
            identifiers.add(intent["capability_id"])
            device_class, _, unit, _ = _registered_context(intent["capability_id"])
            target = float(intent["target"])
            if (
                device_class != device["class_name"]
                or (unit in ("Hertz", "QualityFactor") and target <= 0)
                or (unit in ("Percent", "Milliseconds") and target < 0)
                or (unit == "StereoBalance" and not -1 <= target <= 1)
            ):
                return False
        return True
    except (KeyError, TypeError, ValueError, OverflowError, UnicodeError, NativeUnitError):
        return False


def _valid_adoption_request(name: str, args: list[Any]) -> bool:
    if len(args) != 1 or type(args[0]) is not dict:
        return False
    value = args[0]
    context = {"document_token", "project_key", "binding_key"}
    if any(not _key(value.get(key)) for key in context):
        return False
    if name == DEVICE_ADOPTION_METHOD:
        return set(value) == context | {"operation_id", "preview_token", "approved_preview"} and (
            _key(value["operation_id"])
            and _key(value["preview_token"])
            and type(value["approved_preview"]) is dict
        )
    if (
        set(value) != context | {"expected_content_fingerprint", "devices"}
        or not _fingerprint(value["expected_content_fingerprint"])
        or type(value["devices"]) is not list
        or not 0 <= len(value["devices"]) <= MAX_DEVICES
    ):
        return False
    keys = set()
    for index, selection in enumerate(value["devices"]):
        if (
            type(selection) is not dict
            or set(selection) != {"device_key", "chain_index", "device", "physical_intents"}
            or type(selection["chain_index"]) is not int
            or selection["chain_index"] != index
        ):
            return False
        if selection["device_key"] in keys:
            return False
        keys.add(selection["device_key"])
        probe = {
            **{key: value[key] for key in context},
            "operation_id": "preview",
            "expected_content_fingerprint": value["expected_content_fingerprint"],
            "expected_device_identity_fingerprint": "0" * 64,
            **{key: selection[key] for key in ("device_key", "device", "physical_intents")},
        }
        if not valid_managed_device_request("sunny_managed_insert_device", [probe]) or (
            selection["device"]["role"] != ("source" if index == 0 else "effect")
        ):
            return False
    return True


def _descriptor(parameter: Any) -> dict[str, Any]:
    """Capture raw observed unselected domains without inventing enum semantics."""
    quantized = parameter.is_quantized
    if type(quantized) is not bool:
        raise RuntimeError("Native quantization state must be observed as Boolean")
    value = {
        "minimum": parameter.min,
        "maximum": parameter.max,
        "value": parameter.value,
        "default_value": None if quantized else parameter.default_value,
        "is_quantized": quantized,
        "is_enabled": parameter.is_enabled,
        "state": parameter.state,
        "automation_state": parameter.automation_state,
        "value_items": [],
    }
    if (
        any(
            type(value[key]) is not float or not _number(value[key])
            for key in ("minimum", "maximum", "value")
        )
        or (
            not quantized
            and (type(value["default_value"]) is not float or not _number(value["default_value"]))
        )
        or not value["minimum"] <= value["value"] <= value["maximum"]
    ):
        raise RuntimeError("Native device parameter needs a finite observed internal domain")
    if (
        value["minimum"] > value["maximum"]
        or any(type(value[key]) is not bool for key in ("is_quantized", "is_enabled"))
        or any(
            isinstance(value[key], bool)
            or not isinstance(value[key], int)
            or value[key] not in (0, 1, 2)
            for key in ("state", "automation_state")
        )
    ):
        raise RuntimeError("Native device parameter returned invalid descriptor state")
    value["state"], value["automation_state"] = int(value["state"]), int(value["automation_state"])
    if value["is_quantized"]:
        raw = parameter.value_items
        if isinstance(raw, (str, bytes, dict)):
            raise RuntimeError("Native enum labels require an actual collection")
        for item in raw:
            if len(value["value_items"]) >= MAX_PARAMETERS or not _text(item):
                raise RuntimeError("Native enum label population exceeds its finite domain")
            value["value_items"].append(item)
    return value


def _voice(device: Any, name: str) -> dict[str, Any]:
    index = getattr(device, name + "_index")
    raw = getattr(device, name + "_list")
    if isinstance(raw, (str, bytes, dict)):
        raise RuntimeError("Native voice property needs an actual label collection")
    items: list[str] = []
    for item in raw:
        if len(items) >= 64 or not _text(item):
            raise RuntimeError("Native voice property exceeds its finite label domain")
        items.append(item)
    if (
        isinstance(index, bool)
        or not isinstance(index, int)
        or not 0 <= index < len(items)
        or len(set(items)) != len(items)
    ):
        raise RuntimeError("Native voice index/list is invalid or ambiguous")
    return {"index": int(index), "value_items": items, "label": items[index]}


class ManagedDevices:
    """Retain an owned append-only chain through later Clip/mixer acknowledgements."""

    def __init__(self, registry: Any) -> None:
        self.registry = registry
        self._seal_context: Any = None
        self._previews: dict[str, dict[str, Any]] = {}
        self._preview_document: Any = None

    def _chain(self, record: dict[str, Any]) -> tuple[Any, ...]:
        raw = self.registry._handler._device_chain(record["track"])
        if len(raw) > MAX_DEVICES:
            raise RuntimeError("Native chain exceeds the finite16-device domain")
        return tuple(raw)

    def _version(self) -> tuple[int, int, int]:
        application = self.registry._handler._get_application()
        version = tuple(
            self.registry._handler._lom_integer(
                getattr(application, method)(), "Application version"
            )
            for method in ("get_major_version", "get_minor_version", "get_bugfix_version")
        )
        if min(version) < 0 or version[0] != 12 or version[1] not in (3, 4):
            raise RuntimeError("UnknownRegistryCoverage: native devices cover Live12.3.x/12.4.x")
        return version

    def _context(self, request: dict[str, Any], version: tuple[int, int, int]) -> None:
        self.registry._ensure_document()
        if self._preview_document != self.registry._document_token:
            self._previews.clear()
            self._preview_document = self.registry._document_token
        if request["document_token"] != self.registry._document_token or self._version() != version:
            raise RuntimeError("Managed native device document/version context changed")

    def _handles(self, record: dict[str, Any]) -> tuple[Any, ...]:
        chain = self._chain(record)
        state = record.get("_managed_devices")
        retained = (
            state["handles"] if state is not None else record.get("adopted_device_cohort", ())
        )
        if len(chain) != len(retained) or any(not _same(a, b) for a, b in zip(chain, retained)):
            raise RuntimeError(
                "Managed native device cohort/order changed; preserve external edits"
            )
        return chain

    def _capture_owned(self, record: dict[str, Any]) -> dict[str, Any]:
        import Live

        chain = self._handles(record)
        state = record.get("_managed_devices")
        if state is None:
            if chain:
                # A preserve-only adopted cohort grants no device authoring or
                # parameter/opaque-state coverage. Its private handles are guarded.
                return {}
            identity = _empty_identity()
            return {"device_identity": identity, "device_identity_fingerprint": _digest(identity)}
        cohort = []
        for device, entry in zip(chain, state["entries"]):
            if (
                not isinstance(device, Live.Device.Device)
                or device == None  # noqa: E711
                or not _same(device.canonical_parent, record["track"])
            ):
                raise RuntimeError("Retained device native type/parent identity changed")
            declared = entry["device"]
            if (
                device.class_name != declared["class_name"]
                or not isinstance(device.type, int)
                or int(device.type) != declared["type"]
                or isinstance(device.type, bool)
                or device.can_have_chains is not False
                or device.is_active is not True
            ):
                raise RuntimeError("Retained device class/role/active/flat state changed")
            raw = device.parameters
            if isinstance(raw, (str, bytes, dict)):
                raise RuntimeError("Native parameters need an actual object collection")
            parameters: list[dict[str, Any]] = []
            handles: list[Any] = []
            for parameter in raw:
                if (
                    len(parameters) >= MAX_PARAMETERS
                    or not isinstance(parameter, Live.DeviceParameter.DeviceParameter)
                    or parameter == None  # noqa: E711
                    or not _same(parameter.canonical_parent, device)
                ):
                    raise RuntimeError("Native parameter population/type/parent is unavailable")
                if not _text(parameter.name) or not _text(parameter.original_name):
                    raise RuntimeError("Native parameter names exceed finite observed text bounds")
                handles.append(parameter)
                parameters.append(
                    {
                        "name": parameter.name,
                        "original_name": parameter.original_name,
                        "descriptor": _descriptor(parameter),
                    }
                )
            old = entry.get("parameter_handles")
            if old is not None and (
                len(handles) != len(old) or any(not _same(a, b) for a, b in zip(handles, old))
            ):
                raise RuntimeError("Retained native parameter identity/order changed")
            # Establish actual parameter identities immediately after insertion;
            # never replace this private cohort after an external replacement.
            if old is None:
                entry["parameter_handles"] = tuple(handles)
            if not parameters or not _text(device.name) or not _text(device.class_display_name):
                raise RuntimeError("Native device names/population are unavailable")
            modes: dict[str, Any] = {}
            if device.class_name == "Drift":
                modes = {name: _voice(device, name) for name in ("voice_mode", "voice_count")}
            elif device.class_name == "Eq8":
                mode = device.global_mode
                if isinstance(mode, bool) or not isinstance(mode, int) or mode not in (0, 1, 2):
                    raise RuntimeError("Native EQ Eight global mode is unavailable")
                modes = {"global_mode": int(mode)}
            cohort.append(
                {
                    "device_key": entry["device_key"],
                    "browser_name": declared["browser_name"],
                    "class_name": device.class_name,
                    "class_display_name": device.class_display_name,
                    "name": device.name,
                    "type": int(device.type),
                    "role": declared["role"],
                    "is_active": True,
                    "can_have_chains": False,
                    "parameters": parameters,
                    "modes": modes,
                }
            )
        identity = {"schema_version": 1, "cohort": cohort, "opaque_state_observed": False}
        return {"device_identity": identity, "device_identity_fingerprint": _digest(identity)}

    def retain_created_track_authority(self, record: dict[str, Any]) -> None:
        """Retain the sole initial grant after this registry creates a new Track.

        The owning registry calls this only after successful native Track/Clip
        creation, before its first ACK. Capture and ordinary seals never grant
        append authority, including empty preserve-only Clip adoptions.
        """
        if (
            record.get("authority_origin") == "explicit_adoption"
            or not any(member is record for member in self.registry._bindings.values())
            or self._chain(record)
        ):
            raise RuntimeError("No original empty managed Track creation authority")
        current = self.capture(record)
        state = record.get("_managed_devices")
        if state is None:
            state = {
                "handles": (),
                "entries": [],
                "baseline": current["device_identity_fingerprint"],
            }
            record["_managed_devices"] = state
        state["append_authority"] = "managed_track_creation"
        state["authority_track"] = record["track"]

    def capture(self, record: dict[str, Any]) -> dict[str, Any]:
        """Return current supplement without accepting a changed retained guard."""
        return self._capture_owned(record)

    def verify_retained_chain(self, record: dict[str, Any]) -> None:
        """Guard actual handle cohorts, then exact observed owned parameter/mode state."""
        current = self.capture(record)
        state = record.get("_managed_devices")
        if state is not None and current["device_identity_fingerprint"] != state["baseline"]:
            raise RuntimeError("Managed native device state drift; preserve external edits")

    def seal(
        self, record: dict[str, Any], result: dict[str, Any], authorized_device_change: bool = False
    ) -> None:
        """Retain only unchanged state or this helper's exact verified pending ACK."""
        current = self.capture(record)
        if not current:
            return
        if any(result.get(key) != value for key, value in current.items()):
            raise RuntimeError("Device observation differs from the actual retained cohort")
        state = record.get("_managed_devices")
        if authorized_device_change:
            context = self._seal_context
            if (
                context is None
                or context["record"] is not record
                or context["after"] != current["device_identity_fingerprint"]
                or context["operation"].get("outcome") != "pending"
            ):
                raise RuntimeError("No exact pending managed-device seal authority")
        elif state is not None and state["baseline"] != current["device_identity_fingerprint"]:
            raise RuntimeError("A Clip/mixer ACK cannot accept external native device state drift")
        if state is None:
            state = {
                "handles": (),
                "entries": [],
                "baseline": current["device_identity_fingerprint"],
            }
            record["_managed_devices"] = state
        state["baseline"] = current["device_identity_fingerprint"]

    def _readback(self, parameter: Any, candidate: dict[str, Any]) -> dict[str, Any]:
        value = parameter.value
        if type(value) is not float or not _number(value):
            raise RuntimeError("Actual native parameter readback is not finite")
        display = parameter.str_for_value(value)
        physical, increment = _parse(display, candidate["unit"])
        scale = candidate["balance_full_scale"]
        if scale is not None:
            physical = physical / Decimal(str(scale))
            if increment is not None:
                increment /= Decimal(str(scale))
        unit = candidate["unit"]
        if (
            (unit in ("Hertz", "QualityFactor") and physical <= 0)
            or (unit in ("Percent", "Milliseconds") and physical < 0)
            or (unit == "StereoBalance" and not -1 <= physical <= 1)
        ):
            raise RuntimeError("Native readback is outside its physical unit domain")
        target, tolerance = (
            Decimal(str(candidate["target"])),
            Decimal(str(candidate["display_tolerance"])),
        )
        return {
            "internal_value": value,
            "display": display,
            "display_value": float(physical),
            "display_increment": float(increment) if increment is not None else None,
            "absolute_display_error": float(abs(physical - target)),
            "matches_intent": abs(physical - target) <= tolerance,
            "formatter_calls": 1,
        }

    @staticmethod
    def _capacity(
        operation: dict[str, Any],
        before: dict[str, Any],
        after: dict[str, Any],
        initial: dict[str, Any],
        resolutions: list[dict[str, Any]],
        inserted: bool,
    ) -> None:
        future = copy.deepcopy(after)
        future["observed_notes_match_request"] = False
        future["observed_clip_properties_match_request"] = False
        # The strict native parser caps each display at80 characters and admits
        # ASCII digits/units/space/tab only. Escaped tabs take the worst two
        # wire bytes; the shared estimator bounds every future float at32bytes.
        future["device_update"] = {
            "before_observation": before,
            "before_device_identity": initial["device_identity"],
            "before_device_identity_fingerprint": initial["device_identity_fingerprint"],
            "inserted": inserted,
            "device_key": operation["request"]["device_key"],
            "resolutions": resolutions,
            "readbacks": [
                {
                    "capability_id": item["intent"]["capability_id"],
                    "internal_value": 0.0,
                    "display": "\t" * 80,
                    "display_value": 0.0,
                    "display_increment": 0.0,
                    "absolute_display_error": 0.0,
                    "matches_intent": False,
                    "formatter_calls": 1,
                }
                for item in resolutions
            ],
            "clip_and_note_ids_preserved": False,
            "output_role_transition": False,
            "native_knob_only": True,
            "host_qualified": False,
            "opaque_state_observed": False,
        }
        guard_managed_response_capacity(operation, future)

    def apply(
        self, name: str, record: dict[str, Any], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        """Apply once inside the caller's already-reserved journal operation."""
        if (
            not valid_managed_device_request(name, [request])
            or operation.get("outcome") != "pending"
            or operation.get("native_mutation_started") is not False
        ):
            raise RuntimeError("Invalid or already-attempted managed device operation")
        version = self._version()
        self._context(request, version)
        before = self.registry._require_in_place_guard(
            record, request["expected_content_fingerprint"]
        )
        self.verify_retained_chain(record)
        initial = self.capture(record)
        if (
            not initial
            or request["expected_device_identity_fingerprint"]
            != initial["device_identity_fingerprint"]
        ):
            raise RuntimeError("Managed device guard is absent/stale or preserve-only")
        state = record["_managed_devices"]
        declared = request["device"]
        entries = state["entries"]
        entry = next(
            (item for item in entries if item["device_key"] == request["device_key"]), None
        )
        inserted = name == "sunny_managed_insert_device"
        if inserted:
            if state.get("append_authority") not in (
                "managed_track_creation",
                "explicit_current_device_adoption",
            ) or not _same(state.get("authority_track"), record["track"]):
                raise RuntimeError(
                    "DeviceAuthorityUnavailable: capture/Clip adoption grants no insertion authority"
                )
            if entry is not None or len(entries) >= MAX_DEVICES:
                raise RuntimeError("Device key already retained or finite chain limit reached")
            if (declared["role"] == "source" and entries) or (
                declared["role"] == "effect"
                and (not entries or entries[0]["device"]["class_name"] != "Drift")
            ):
                raise RuntimeError("Append policy requires Drift source first, then owned effects")
            method = getattr(record["track"], "insert_device", None)
            if not callable(method):
                raise RuntimeError("Native Python Track.insert_device API is unavailable")
            self._capacity(operation, before, before, initial, [], True)
            old = state["handles"]
            operation["native_mutation_started"] = True
            try:
                method(declared["browser_name"], len(old))
            finally:
                actual = tuple(self.registry._handler._device_chain(record["track"]))
                created = [
                    device for device in actual if not any(_same(device, member) for member in old)
                ]
                state["partial_created_handles"] = tuple(created)
                if len(created) == 1:
                    entry = {
                        "device_key": request["device_key"],
                        "device": copy.deepcopy(declared),
                        "handle": created[0],
                    }
                    entries.append(entry)
                    state["handles"] = old + (created[0],)
            if entry is None:
                raise RuntimeError("Native insertion did not produce one retained new Device")
        elif entry is None or entry["device"] != declared:
            raise RuntimeError("Device key/declaration differs from retained insertion identity")
        device = entry["handle"]
        after_insert = self.capture(record)
        baseline = copy.deepcopy(after_insert["device_identity"])
        candidates = []
        for intent in request["physical_intents"]:
            candidate = resolve_registered_native_display_value(device, **intent)
            parameter = tuple(device.parameters)[candidate["parameter_index"]]
            candidates.append(
                {"intent": copy.deepcopy(intent), "candidate": candidate, "parameter": parameter}
            )
        # Every candidate resolves first. Re-read all cohorts/modes/descriptors
        # after the last formatter and before the first setter.
        if self.capture(record) != after_insert:
            raise RuntimeError("Native device state changed during batch candidate preflight")
        self._context(request, version)
        preflight = self.registry._capture(record)
        if (
            preflight["note_identity_fingerprint"] != before["note_identity_fingerprint"]
            or preflight["manifest"]["clip"] != before["manifest"]["clip"]
            or preflight["manifest"]["notes"] != before["manifest"]["notes"]
        ):
            raise RuntimeError("Native insertion changed the preserved Clip/note population")
        self._validate_binding_changes(before, preflight, inserted and declared["role"] == "source")
        serializable = [
            {"intent": item["intent"], "candidate": item["candidate"]} for item in candidates
        ]
        self._capacity(operation, before, preflight, initial, serializable, inserted)
        readbacks = []
        cohort_index = entries.index(entry)
        for item in candidates:
            current = self.capture(record)
            if current["device_identity"] != baseline:
                raise RuntimeError("Native modes/cohorts changed before a parameter setter")
            if (
                self.registry._capture(record)["content_fingerprint"]
                != preflight["content_fingerprint"]
            ):
                raise RuntimeError("Native Clip/Track state changed before a parameter setter")
            self._context(request, version)
            operation["native_mutation_started"] = True
            item["parameter"].value = float(item["candidate"]["internal_value"])
            readback = self._readback(item["parameter"], item["candidate"])
            readbacks.append({"capability_id": item["intent"]["capability_id"], **readback})
            if not readback["matches_intent"]:
                raise RuntimeError("Native formatted readback differs from the physical intent")
            parameter_index = item["candidate"]["parameter_index"]
            baseline["cohort"][cohort_index]["parameters"][parameter_index]["descriptor"][
                "value"
            ] = readback["internal_value"]
            if self.capture(record)["device_identity"] != baseline:
                raise RuntimeError(
                    "Native setter changed unselected descriptors, modes or parameter values"
                )
        after = self.registry._capture(record)
        self._validate_binding_changes(preflight, after, False)
        if after["note_identity_fingerprint"] != before["note_identity_fingerprint"]:
            raise RuntimeError("Native device operation changed retained note identities")
        update = {
            "before_observation": before,
            "before_device_identity": initial["device_identity"],
            "before_device_identity_fingerprint": initial["device_identity_fingerprint"],
            "inserted": inserted,
            "device_key": request["device_key"],
            "resolutions": serializable,
            "readbacks": readbacks,
            "clip_and_note_ids_preserved": True,
            "output_role_transition": declared["role"] == "source" and inserted,
            "native_knob_only": True,
            "host_qualified": False,
            "opaque_state_observed": False,
        }
        final = self.capture(record)
        self._context(request, version)
        prospective = {
            **after,
            "observed_notes_match_request": False,
            "observed_clip_properties_match_request": False,
            "device_update": update,
        }
        guard_managed_response_capacity(operation, prospective)
        self._seal_context = {
            "record": record,
            "operation": operation,
            "after": final["device_identity_fingerprint"],
        }
        try:
            result: dict[str, Any] = self.registry._seal(record, authorized_device_change=True)
        finally:
            self._seal_context = None
        result["device_update"] = update
        return result

    def preview(self, record: dict[str, Any], request: dict[str, Any]) -> dict[str, Any]:
        """Observe selected current objects for a separately fenced explicit adoption.

        Every member of the current finite known chain must be declared. An empty
        chain is an explicit prospective append-authority preview, without
        parameter formatter calls or native setters. Desired
        physical values must already match actual formatter readback: preview
        and adoption never repair or overwrite authored/native mismatches.
        """
        if not valid_managed_device_request(DEVICE_PREVIEW_METHOD, [request]):
            raise RuntimeError("Invalid closed current-device preview request")
        version = self._version()
        self._context(request, version)
        if len(self._previews) >= MAX_PREVIEWS:
            raise RuntimeError("Current-device preview journal is full; no records are evicted")
        chain = self._chain(record)
        if len(chain) != len(request["devices"]):
            raise RuntimeError(
                "UnsupportedDeviceCohort: every current finite native member needs a declaration"
            )
        entries = [
            {
                "device_key": selection["device_key"],
                "device": copy.deepcopy(selection["device"]),
                "handle": chain[index],
            }
            for index, selection in enumerate(request["devices"])
        ]
        state = {"handles": chain, "entries": entries, "baseline": None}
        candidate = {**record, "_managed_devices": state}
        observed = self.registry._capture(candidate)
        if observed["content_fingerprint"] != request["expected_content_fingerprint"] or observed[
            "note_identity_fingerprint"
        ] != record.get("note_identity_fingerprint"):
            raise RuntimeError(
                "Current-device preview requires the current retained Clip/note baseline"
            )
        self.registry._handler._step_clip_interval(record["clip"], idle=True)
        if any(
            getattr(record["track"], name) is not False
            for name in ("arm", "implicit_arm", "is_frozen", "is_grouped")
        ):
            raise RuntimeError("Current-device adoption requires idle unarmed native context")
        resolutions = []
        for index, selection in enumerate(request["devices"]):
            native = chain[index]
            for intent in selection["physical_intents"]:
                evidence = resolve_registered_native_display_value(native, **intent)
                parameter = tuple(native.parameters)[evidence["parameter_index"]]
                readback = self._readback(parameter, evidence)
                if not readback["matches_intent"]:
                    raise RuntimeError(
                        "AuthoredNativeMismatch: current formatted value differs from selected physical intent"
                    )
                resolutions.append(
                    {
                        "device_key": selection["device_key"],
                        "intent": copy.deepcopy(intent),
                        "candidate": evidence,
                        "current_readback": readback,
                    }
                )
        current = self.registry._capture(candidate)
        if current != observed:
            raise RuntimeError("Current native objects/modes/values changed during device preview")
        self._context(request, version)
        value = {
            "schema_version": 1,
            "binding_observation": observed,
            "devices": copy.deepcopy(request["devices"]),
            "resolutions": resolutions,
            "authority_origin": "explicit_current_device_adoption",
            "native_mutation_started": False,
            "native_knob_only": True,
            "host_qualified": False,
            "opaque_state_observed": False,
        }
        token = uuid.uuid4().hex
        fingerprint = _digest(value)
        response = {
            "outcome": "previewed",
            "document_token": request["document_token"],
            "project_key": request["project_key"],
            "binding_key": request["binding_key"],
            "preview_token": token,
            "preview_fingerprint": fingerprint,
            "preview": value,
        }
        from .managed_capacity import require_response_capacity

        require_response_capacity({"success": True, "value": response})
        self._previews[token] = {
            "request": copy.deepcopy(request),
            "value": value,
            "state": state,
            "record": record,
            "version": version,
        }
        return copy.deepcopy(response)

    def adopt(
        self, record: dict[str, Any], request: dict[str, Any], operation: dict[str, Any]
    ) -> dict[str, Any]:
        """Grant new current-object device authority without any native setter."""
        if (
            not valid_managed_device_request(DEVICE_ADOPTION_METHOD, [request])
            or operation.get("outcome") != "pending"
            or operation.get("native_mutation_started") is not False
        ):
            raise RuntimeError("Invalid managed current-device adoption")
        retained = self._previews.get(request["preview_token"])
        if (
            retained is None
            or retained["record"] is not record
            or request["approved_preview"] != retained["value"]
            or any(
                request[name] != retained["request"][name]
                for name in ("document_token", "project_key", "binding_key")
            )
        ):
            raise RuntimeError("Explicit device approval differs from the retained current preview")
        self._context(request, retained["version"])
        candidate = {**record, "_managed_devices": retained["state"]}
        observed = self.registry._capture(candidate)
        if observed != retained["value"]["binding_observation"]:
            raise RuntimeError("Current device/Clip identities or state changed after approval")
        self.registry._handler._step_clip_interval(record["clip"], idle=True)
        if any(
            getattr(record["track"], name) is not False
            for name in ("arm", "implicit_arm", "is_frozen", "is_grouped")
        ):
            raise RuntimeError("Current-device context became armed/frozen/grouped")
        # Repeat actual formatter evidence at approval time. A mode/formatter
        # drift with unchanged internal values cannot inherit the old preview.
        for item in retained["value"]["resolutions"]:
            selected = next(
                entry
                for entry in retained["state"]["entries"]
                if entry["device_key"] == item["device_key"]
            )
            parameter = selected["parameter_handles"][item["candidate"]["parameter_index"]]
            if self._readback(parameter, item["candidate"]) != item["current_readback"]:
                raise RuntimeError("Current formatted device value changed after explicit approval")
        final = self.registry._capture(candidate)
        if final != observed:
            raise RuntimeError("Current native cohort changed before device authority publication")
        self._context(request, retained["version"])
        supplement = {
            "preview_token": request["preview_token"],
            "preview_fingerprint": _digest(request["approved_preview"]),
            "authority_origin": "explicit_current_device_adoption",
            "native_mutation_started": False,
            "current_values_match_approved_intent": True,
            "native_knob_only": True,
            "host_qualified": False,
            "opaque_state_observed": False,
        }
        guard_managed_response_capacity(
            operation,
            {
                **final,
                "device_adoption": supplement,
                "observed_notes_match_request": False,
                "observed_clip_properties_match_request": False,
            },
        )
        previous = record.get("_managed_devices")
        old_content, old_notes = (
            record.get("content_fingerprint"),
            record.get("note_identity_fingerprint"),
        )
        record["_managed_devices"] = {
            **retained["state"],
            "authority_origin": "explicit_current_device_adoption",
            "append_authority": "explicit_current_device_adoption",
            "authority_track": record["track"],
        }
        self._seal_context = {
            "record": record,
            "operation": operation,
            "after": observed["device_identity_fingerprint"],
        }
        try:
            result: dict[str, Any] = self.registry._seal(record, authorized_device_change=True)
        except Exception:
            if previous is None:
                record.pop("_managed_devices", None)
            else:
                record["_managed_devices"] = previous
            record["content_fingerprint"], record["note_identity_fingerprint"] = (
                old_content,
                old_notes,
            )
            raise
        finally:
            self._seal_context = None
        result["device_adoption"] = supplement
        return result

    @staticmethod
    def _validate_binding_changes(
        before: dict[str, Any], after: dict[str, Any], source_insert: bool
    ) -> None:
        first, second = copy.deepcopy(before["manifest"]), copy.deepcopy(after["manifest"])
        if source_insert:
            if (
                first["track"]["has_midi_input"] is not True
                or second["track"]["has_midi_input"] is not True
                or first["track"]["has_audio_output"] is not False
                or second["track"]["has_audio_output"] is not True
                or second["track"]["has_midi_output"] is not False
            ):
                raise RuntimeError(
                    "Drift insertion did not yield the declared MIDI-to-audio output role"
                )
            for field in ("has_audio_output", "has_midi_output"):
                second["track"][field] = first["track"][field]
            # The host chooses default output routing and mixer eligibility
            # when an instrument creates audio output. Observe these exact
            # defaults; no route/mixer setter is authorized by this operation.
            for field in ("output_routing_type", "output_routing_channel"):
                second["routing"][field] = first["routing"][field]
            second["mixer"] = first["mixer"]
        second["devices_empty"] = first["devices_empty"]
        if second != first:
            raise RuntimeError("Device operation changed unselected Track/Clip/mixer/routing state")
