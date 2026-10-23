"""One finite owned effect mode phase; no generic property or ordinal writes."""

from __future__ import annotations

import copy
from typing import Any

from .managed_capacity import guard_managed_response_capacity
from .native_control import check_native_peer, native_call
from .native_units import registered_native_mode_context


def _admit(helper: Any, device: Any, intent: dict[str, Any]) -> dict[str, Any]:
    device_class, name, count = registered_native_mode_context(intent["capability_id"])
    if device.class_name != device_class:
        raise RuntimeError("Mode capability differs from retained native class")
    parameters = tuple(device.parameters)
    matches = [
        (index, parameter)
        for index, parameter in enumerate(parameters)
        if parameter.name == name or parameter.original_name == name
    ]
    if len(matches) != 1 or matches[0][1].original_name != name:
        raise RuntimeError("Native mode requires one exact actual original-name identity")
    index, parameter = matches[0]
    # The full owned capture has already checked actual type/parent/identity.
    from .managed_devices import _descriptor

    descriptor = _descriptor(parameter)
    labels = descriptor["value_items"]
    if (
        not descriptor["is_quantized"]
        or len(labels) != count
        or len(set(labels)) != count
        or descriptor["minimum"] != 0.0
        or descriptor["maximum"] != float(count - 1)
        or descriptor["value"] != int(descriptor["value"])
        or intent["label"] not in labels
        or not descriptor["is_enabled"]
        or descriptor["state"] not in (0, 1)
        or descriptor["automation_state"] != 0
    ):
        raise RuntimeError("Native mode label/domain/availability/automation admission failed")
    if count == 2 and set(labels) != {"Off", "On"}:
        raise RuntimeError("Native activator must advertise exact Off/On labels")
    return {
        "intent": copy.deepcopy(intent),
        "parameter_index": index,
        "parameter_original_name": name,
        "descriptor": descriptor,
        "target_internal": float(labels.index(intent["label"])),
        "parameter": parameter,
    }


def _check_change(
    expected: dict[str, Any],
    actual: dict[str, Any],
    index: int,
    *,
    parameter_index: int | None = None,
    target: float | None = None,
    active: bool | None = None,
    global_mode: int | None = None,
) -> None:
    """Accept only this selected native phase's observed availability effects."""
    candidate = copy.deepcopy(expected)
    member, observed = candidate["cohort"][index], actual["cohort"][index]
    if parameter_index is not None:
        member["parameters"][parameter_index]["descriptor"]["value"] = target
    if global_mode is not None:
        member["modes"]["global_mode"] = global_mode
    if active is not None:
        member["is_active"] = active
    if len(member["parameters"]) != len(observed["parameters"]):
        raise RuntimeError("Native mode changed the retained parameter population")
    for before, after in zip(member["parameters"], observed["parameters"]):
        # Values, ranges, identities, kinds, labels and automation remain exact.
        # Only availability on this selected Device may follow its mode change.
        for field in ("is_enabled", "state"):
            before["descriptor"][field] = after["descriptor"][field]
    if candidate != actual:
        raise RuntimeError("Native mode changed untouched values/cohorts/descriptors/properties")


def apply_device_modes(
    helper: Any, record: dict[str, Any], request: dict[str, Any], operation: dict[str, Any]
) -> dict[str, Any]:
    """Apply inside the Registry's already-fenced once-only pending operation."""
    from .managed import _digest
    from .managed_devices import DEVICE_MODE_METHOD, valid_managed_device_request

    if (
        not valid_managed_device_request(DEVICE_MODE_METHOD, [request])
        or operation.get("outcome") != "pending"
        or operation.get("native_mutation_started") is not False
        or operation.get("name") != DEVICE_MODE_METHOD
        or operation.get("request") != request
        or operation.get("operation_id") != request.get("operation_id")
        or operation.get("document_token") != request.get("document_token")
        or operation.get("request_fingerprint")
        != _digest({"name": DEVICE_MODE_METHOD, "request": request})
        or helper.registry._operations.get(request.get("operation_id")) is not operation
        or helper.registry._bindings.get((request.get("project_key"), request.get("binding_key")))
        is not record
    ):
        raise RuntimeError("Invalid or already-attempted owned mode operation")
    version = helper._version()
    helper._context(request, version)
    before = helper.registry._require_in_place_guard(
        record, request["expected_content_fingerprint"]
    )
    helper.verify_retained_chain(record)
    initial = helper.capture(record)
    if (
        not initial
        or initial["device_identity_fingerprint"] != request["expected_device_identity_fingerprint"]
    ):
        raise RuntimeError("Mode phase needs current owned Device evidence")
    state = record["_managed_devices"]
    entry = next(
        (item for item in state["entries"] if item["device_key"] == request["device_key"]), None
    )
    if entry is None or entry["device"] != request["device"] or entry["device"]["role"] != "effect":
        raise RuntimeError("Mode phase does not match retained native effect authority")
    index, device = state["entries"].index(entry), entry["handle"]
    member = initial["device_identity"]["cohort"][index]
    properties = []
    if device.class_name == "Eq8":
        if set(member["modes"]) != {"global_mode", "edit_mode", "oversample"}:
            raise RuntimeError(
                "ObservationUnavailable: mode phase requires actual EQ native properties"
            )
    for intent in request["property_intents"]:
        properties.append(
            {"intent": copy.deepcopy(intent), "before": member["modes"]["global_mode"], "target": 0}
        )
    admitted = [_admit(helper, device, intent) for intent in request["enum_intents"]]
    if helper.capture(record) != initial:
        raise RuntimeError("Native mode cohort changed during preflight")
    helper._context(request, version)
    if helper.registry._capture(record)["content_fingerprint"] != before["content_fingerprint"]:
        raise RuntimeError("Native Clip/Track changed during mode preflight")
    serializable = [
        {key: value for key, value in item.items() if key != "parameter"} for item in admitted
    ]
    update = {
        "before_observation": before,
        "before_device_identity": initial["device_identity"],
        "before_device_identity_fingerprint": initial["device_identity_fingerprint"],
        "device_key": request["device_key"],
        "admitted_modes": serializable,
        "admitted_properties": properties,
        "readbacks": [],
        "property_readbacks": [],
        "clip_and_note_ids_preserved": True,
        "native_knob_only": True,
        "host_qualified": False,
        "opaque_state_observed": False,
    }
    future = copy.deepcopy(before)
    future["device_mode_update"] = copy.deepcopy(update)
    future["device_mode_update"]["readbacks"] = [
        {
            "capability_id": item["intent"]["capability_id"],
            "parameter_index": item["parameter_index"],
            "internal_value": item["target_internal"],
            "label": item["intent"]["label"],
        }
        for item in admitted
    ]
    future["device_mode_update"]["property_readbacks"] = [
        {"property": "global_mode", "value": 0} for _ in properties
    ]
    # Capacity estimator bounds every Boolean/numeric leaf by native maxima;
    # all possible strings are the already observed exact names/enum labels.
    guard_managed_response_capacity(operation, future)
    baseline = initial["device_identity"]

    def guard() -> None:
        helper._context(request, version)
        if (
            helper.capture(record)["device_identity"] != baseline
            or helper.registry._capture(record)["content_fingerprint"]
            != before["content_fingerprint"]
        ):
            raise RuntimeError("Native mode/Clip guard changed before setter")

    for item in properties:
        guard()
        check_native_peer()
        operation["native_mutation_started"] = True
        native_call(setattr, device, "global_mode", 0)
        actual = helper.capture(record)["device_identity"]
        _check_change(baseline, actual, index, global_mode=0)
        baseline = actual
        update["property_readbacks"].append(
            {"property": "global_mode", "value": device.global_mode}
        )
    for item in admitted:
        guard()
        parameter = item["parameter"]
        # Coupled mode setters may change eligibility; re-admit every remaining
        # exact handle/label immediately before its own setter.
        current = _admit(helper, device, item["intent"])
        if current["parameter"] is not parameter and current["parameter"] != parameter:
            raise RuntimeError("Retained native mode parameter identity changed")
        check_native_peer()
        operation["native_mutation_started"] = True
        native_call(setattr, parameter, "value", item["target_internal"])
        actual = helper.capture(record)["device_identity"]
        active = (
            item["intent"]["label"] == "On"
            if item["parameter_original_name"] == "Device On"
            else None
        )
        _check_change(
            baseline,
            actual,
            index,
            parameter_index=item["parameter_index"],
            target=item["target_internal"],
            active=active,
        )
        baseline = actual
        if parameter.value != item["target_internal"]:
            raise RuntimeError("Native mode readback did not match the admitted actual label")
        update["readbacks"].append(
            {
                "capability_id": item["intent"]["capability_id"],
                "parameter_index": item["parameter_index"],
                "internal_value": parameter.value,
                "label": tuple(parameter.value_items)[int(parameter.value)],
            }
        )
    after = helper.registry._capture(record)
    helper._validate_binding_changes(before, after, False)
    if after["note_identity_fingerprint"] != before["note_identity_fingerprint"]:
        raise RuntimeError("Native mode phase changed retained note identity")
    prospective = {**after, "device_mode_update": update}
    guard_managed_response_capacity(operation, prospective)
    helper._seal_context = {
        "record": record,
        "operation": operation,
        "after": after["device_identity_fingerprint"],
    }
    try:
        result: dict[str, Any] = helper.registry._seal(record, authorized_device_change=True)
    finally:
        helper._seal_context = None
    result["device_mode_update"] = update
    return result
