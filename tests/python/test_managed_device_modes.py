"""Independent native enum/property phases on real retained Clip bindings."""

from __future__ import annotations

import copy
from typing import Any

import pytest
import test_managed_devices as fixture
from live_model import Device, DeviceType
from Sunny.managed import _digest
from Sunny.managed_devices import DEVICE_MODE_METHOD, valid_managed_device_request

target = fixture.target


class ModeParameter(fixture.Parameter):
    """Literal native setter callback, independent of resolver/producer expectations."""

    def _write(self, value: float) -> None:
        super()._write(value)
        if self.callback:
            self.callback(value)


class EqDevice(Device):
    """Native property fixture with observable setters and availability side effects."""

    def __init__(self) -> None:
        """Start in L/R with active factory extras and a deliberately permuted type list."""
        super().__init__("EQ Eight", "Eq8", DeviceType.audio_effect)
        self._mode = 1
        self.mode_writes: list[int] = []
        self.edit_mode = False
        self.oversample = False
        self._parameters = tuple(
            [
                fixture.enum("Device On", ("Off", "On"), 1),
                fixture.enum("Adaptive Q", ("On", "Off"), 0),
                fixture.Parameter(
                    "Scale", lambda value: f"{200 * value * value:.2f} %", value=0.25
                ),
                fixture.Parameter("Output Gain", lambda value: f"{24 * value - 12:.2f} dB"),
            ]
            + [
                parameter
                for band in range(1, 9)
                for parameter in (
                    fixture.enum(f"{band} Filter On A", ("On", "Off"), 0),
                    ModeParameter(
                        f"{band} Filter Type A",
                        maximum=7,
                        value=5,
                        value_items=(
                            "High Shelf",
                            "Notch",
                            "Low Cut12",
                            "Bell",
                            "Low Cut48",
                            "Low Shelf",
                            "High Cut12",
                            "High Cut48",
                        ),
                    ),
                    fixture.Parameter(
                        f"{band} Frequency A", lambda value: f"{200 + 4000 * value * value:.2f} Hz"
                    ),
                    fixture.Parameter(f"{band} Gain A", lambda value: f"{24 * value - 12:.2f} dB"),
                    fixture.Parameter(
                        f"{band} Resonance A", lambda value: f"{0.5 + 4 * value * value:.2f}"
                    ),
                )
            ]
        )
        for parameter in self._parameters:
            parameter._canonical_parent = self
        self._parameters[6]._state = 1

    @property
    def global_mode(self) -> int:
        """Return the genuine documented property domain."""
        return self._mode

    @global_mode.setter
    def global_mode(self, value: int) -> None:
        """Record the property write and native availability change only."""
        self.mode_writes.append(value)
        self._mode = value
        self._parameters[6]._state = 1 if value == 1 else 0


def insert_source(target: Any) -> None:
    """Establish sole native source authority before any effects."""
    fixture.apply(target, fixture.request(target, "source"))


def insert_eq(target: Any, monkeypatch: pytest.MonkeyPatch) -> EqDevice:
    """Use actual native insertion path; no name/index adoption."""
    insert_source(target)
    device = EqDevice()

    def insert(browser: str, index: int) -> None:
        assert browser == "EQ Eight"
        device._canonical_parent = target.record["track"]
        target.record["track"]._devices.insert(index, device)

    monkeypatch.setattr(target.record["track"], "insert_device", insert)
    fixture.apply(target, fixture.request(target, "effect", "EQ Eight"))
    return device


def mode_request(
    target: Any,
    browser: str,
    modes: list[dict[str, Any]],
    properties: list[dict[str, Any]] | None = None,
) -> dict[str, Any]:
    """Retain the independently observed current binding/device guard."""
    value = fixture.request(target, "effect", browser)
    value.pop("physical_intents")
    value.update(enum_intents=modes, property_intents=properties or [])
    value["operation_id"] = "mode" + str(len(target.registry._operations))
    return value


def apply_modes(target: Any, request: dict[str, Any]) -> dict[str, Any]:
    """Enter only through an exact already-pending once-only journal entry."""
    operation = {
        "name": DEVICE_MODE_METHOD,
        "request": copy.deepcopy(request),
        "outcome": "pending",
        "native_mutation_started": False,
        "operation_id": request["operation_id"],
        "document_token": request["document_token"],
        "request_fingerprint": _digest({"name": DEVICE_MODE_METHOD, "request": request}),
    }
    target.registry._operations[request["operation_id"]] = operation
    result = target.registry.devices.apply(DEVICE_MODE_METHOD, target.record, request, operation)
    operation.update(outcome="acknowledged", result=result)
    return result


def test_eq_mode_phase_proves_native_property_labels_and_following_physical_value(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Bell is actual index3; Scale100% is sqrt(0.5), never normalized target100."""
    device = insert_eq(target, monkeypatch)
    before = target.registry._capture(target.record)
    modes = [
        {"capability_id": "eq8.adaptive_q", "label": "Off"},
        {"capability_id": "eq8.band.1.type", "label": "Bell"},
    ]
    modes += [{"capability_id": f"eq8.band.{band}.enabled", "label": "Off"} for band in range(2, 9)]
    result = apply_modes(
        target,
        mode_request(target, "EQ Eight", modes, [{"property": "global_mode", "label": "Stereo"}]),
    )
    assert device.mode_writes == [0]
    assert device._parameters[1].value == 1.0
    assert device._parameters[5].value == 3.0
    assert device._parameters[6].state == 0
    assert result["note_identity"] == before["note_identity"]
    assert result["manifest"]["clip"] == before["manifest"]["clip"]
    assert result["device_identity"]["cohort"][1]["modes"] == {
        "global_mode": 0,
        "edit_mode": False,
        "oversample": False,
    }
    assert result["device_mode_update"]["readbacks"][1]["label"] == "Bell"
    updated = fixture.request(
        target,
        "effect",
        "EQ Eight",
        [{"capability_id": "eq8.scale", "target": 100.0, "tolerance": 0.0}],
    )
    updated["operation_id"] = "scale"
    physical = fixture.apply(target, updated, insert=False)
    assert physical["device_update"]["readbacks"][0]["display"] == "100.00 %"
    assert abs(device._parameters[2].value ** 2 - 0.5) < 0.0001
    assert device._parameters[2].value != 0.5


def test_every_mode_choice_is_admitted_before_any_property_or_enum_setter(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Unknown second label cannot write the earlier native Stereo property/AdaptiveQ."""
    device = insert_eq(target, monkeypatch)
    request = mode_request(
        target,
        "EQ Eight",
        [
            {"capability_id": "eq8.adaptive_q", "label": "Off"},
            {"capability_id": "eq8.band.1.type", "label": "Peak"},
        ],
        [{"property": "global_mode", "label": "Stereo"}],
    )
    with pytest.raises(RuntimeError, match="admission failed"):
        apply_modes(target, request)
    assert device.mode_writes == []
    assert all(not parameter.writes for parameter in device.parameters)
    assert target.registry._operations[request["operation_id"]]["native_mutation_started"] is False


def test_inactive_effect_capture_requires_actual_device_off_then_can_enable_owned_handle(
    target: Any,
) -> None:
    """Off is native index0; a later explicit phase enables that same retained handle."""
    insert_source(target)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    device = target.record["track"].devices[1]
    before = target.registry._capture(target.record)
    result = apply_modes(
        target,
        mode_request(target, "Utility", [{"capability_id": "utility.enabled", "label": "Off"}]),
    )
    assert result["device_identity"]["cohort"][1]["is_active"] is False
    assert device.is_active is False
    assert result["note_identity"] == before["note_identity"]
    on = apply_modes(
        target,
        mode_request(target, "Utility", [{"capability_id": "utility.enabled", "label": "On"}]),
    )
    assert device.is_active is True
    assert on["device_identity"]["cohort"][1]["is_active"] is True
    device._parameters[0]._value = 0.0
    device._parameters[0]._value_items = ("Unknown", "On")
    with pytest.raises(RuntimeError, match="not proven bypassed"):
        target.registry.devices.capture(target.record)


def test_mode_phase_rejects_untouched_gain_drift_and_retains_started_operation(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """A mode setter cannot promote an unrelated value change into the retained baseline."""
    device = insert_eq(target, monkeypatch)
    device._parameters[5].callback = lambda _: setattr(device._parameters[7], "_value", 0.25)
    request = mode_request(
        target, "EQ Eight", [{"capability_id": "eq8.band.1.type", "label": "Bell"}]
    )
    old = target.record["_managed_devices"]["baseline"]
    with pytest.raises(RuntimeError, match="untouched"):
        apply_modes(target, request)
    assert target.registry._operations[request["operation_id"]]["native_mutation_started"] is True
    assert target.record["_managed_devices"]["baseline"] == old


def test_exact_legacy_eq_property_shape_stays_capturable_but_cannot_grant_mode_write(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Missing historical properties stay unknown, with no factory-value substitution."""
    device = insert_eq(target, monkeypatch)
    del device.edit_mode
    del device.oversample
    # Explicit current adoption is separate; this fixture freshly seals its known
    # legacy evidence to test only the versioned capture/mode admission boundary.
    capture = target.registry.devices.capture(target.record)
    assert capture["device_identity"]["cohort"][1]["modes"] == {"global_mode": 1}
    target.record["_managed_devices"]["baseline"] = capture["device_identity_fingerprint"]
    request = mode_request(
        target, "EQ Eight", [{"capability_id": "eq8.adaptive_q", "label": "Off"}]
    )
    assert valid_managed_device_request(DEVICE_MODE_METHOD, [request])
    with pytest.raises(RuntimeError, match="actual EQ native properties"):
        apply_modes(target, request)
    assert device.mode_writes == []
    assert all(not parameter.writes for parameter in device.parameters)


def test_utility_trim_neutral_setup_uses_actual_source_names_and_literal_db(
    target: Any, monkeypatch: pytest.MonkeyPatch
) -> None:
    """Actual reversed toggles use index1 for Off; Gain-7.5dB uses native0.1875."""
    insert_source(target)
    original = fixture.device

    def provider(browser: str) -> Any:
        created = original(browser)
        if browser == "Utility":
            extra = [
                fixture.enum(name, ("On", "Off"), 0)
                for name in ("Left Inv", "Right Inv", "Bass Mono", "DC Filter")
            ]
            for parameter in extra:
                parameter._canonical_parent = created
            created._parameters += tuple(extra)
        return created

    monkeypatch.setattr(fixture, "device", provider)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    result = apply_modes(
        target,
        mode_request(
            target,
            "Utility",
            [
                {"capability_id": capability, "label": label}
                for capability, label in (
                    ("utility.enabled", "On"),
                    ("utility.channel_mode", "Stereo"),
                    ("utility.mono", "Off"),
                    ("utility.mute", "Off"),
                    ("utility.left_invert", "Off"),
                    ("utility.right_invert", "Off"),
                    ("utility.bass_mono", "Off"),
                    ("utility.dc_filter", "Off"),
                )
            ],
        ),
    )
    assert result["device_mode_update"]["readbacks"][4]["internal_value"] == 1.0
    physical = fixture.request(
        target,
        "effect",
        "Utility",
        [
            {"capability_id": "utility.gain", "target": -7.5, "tolerance": 0.0},
            {"capability_id": "utility.width", "target": 100.0, "tolerance": 0.0},
            {"capability_id": "utility.balance", "target": 0.0, "tolerance": 0.0},
        ],
    )
    physical["operation_id"] = "trim"
    ack = fixture.apply(target, physical, insert=False)
    assert [value["display"] for value in ack["device_update"]["readbacks"]] == [
        "-7.50 dB",
        "100.00 %",
        "C",
    ]
    assert target.record["track"].devices[1].parameters[1].value == 0.1875


@pytest.mark.parametrize("fault", ("state2", "automation", "duplicate_labels"))
def test_modes_decline_native_domain_faults_before_any_setter(
    target: Any, monkeypatch: pytest.MonkeyPatch, fault: str
) -> None:
    """Official unavailable state, existing automation and repeated labels grant no write."""
    device = insert_eq(target, monkeypatch)
    parameter = device.parameters[5]
    if fault == "state2":
        parameter._state = 2
    elif fault == "automation":
        parameter._automation_state = 1
    else:
        parameter._value_items = (
            "Bell",
            "Bell",
            "Notch",
            "Low Cut12",
            "Low Cut48",
            "Low Shelf",
            "High Cut12",
            "High Cut48",
        )
    # This fixture establishes a newly observed literal baseline to isolate
    # native domain admission; ordinary observe never refreshes a real grant.
    capture = target.registry.devices.capture(target.record)
    target.record["_managed_devices"]["baseline"] = capture["device_identity_fingerprint"]
    payload = mode_request(
        target,
        "EQ Eight",
        [{"capability_id": "eq8.band.1.type", "label": "Bell"}],
        [{"property": "global_mode", "label": "Stereo"}],
    )
    with pytest.raises(RuntimeError, match="admission failed"):
        apply_modes(target, payload)
    assert device.mode_writes == []
    assert all(not member.writes for member in device.parameters)


def bypass_preview_request(target: Any) -> dict[str, Any]:
    """The whole current chain has explicit current handle/mode agreement."""
    value = fixture.preview_request(target)
    value["devices"][0]["physical_intents"] = []
    declaration = {
        "browser_name": "Utility",
        "class_name": "StereoGain",
        "type": 2,
        "role": "effect",
        "insertion_policy": "AppendOwnedChain",
    }
    value["devices"].append(
        {
            "device_key": "current_effect",
            "chain_index": 1,
            "device": declaration,
            "physical_intents": [],
            "enum_intents": [{"capability_id": "utility.enabled", "label": "Off"}],
            "property_intents": [],
            "authored_bypass": True,
        }
    )
    return value


def test_bypassed_current_effect_adoption_has_mode_authority_without_formatter_claim(
    target: Any,
) -> None:
    """A genuine Off effect is adopted without inaudible physical-value agreement."""
    insert_source(target)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    apply_modes(
        target,
        mode_request(target, "Utility", [{"capability_id": "utility.enabled", "label": "Off"}]),
    )
    chain = target.registry.devices._chain(target.record)
    target.record.pop("_managed_devices")
    target.record["adopted_device_cohort"] = chain
    before_calls = sum(len(p.calls) for d in chain for p in d.parameters)
    before_writes = sum(len(p.writes) for d in chain for p in d.parameters)
    preview = target.registry.devices.preview(target.record, bypass_preview_request(target))
    assert preview["preview"]["resolutions"] == []
    approved = fixture.adopt_request(target, preview)
    operation = {"request": approved, "outcome": "pending", "native_mutation_started": False}
    result = target.registry.devices.adopt(target.record, approved, operation)
    assert result["device_adoption"]["native_mutation_started"] is False
    assert sum(len(p.calls) for d in chain for p in d.parameters) == before_calls
    assert sum(len(p.writes) for d in chain for p in d.parameters) == before_writes
    request = mode_request(target, "Utility", [{"capability_id": "utility.enabled", "label": "On"}])
    request["device_key"] = "current_effect"
    on = apply_modes(target, request)
    assert on["device_identity"]["cohort"][1]["is_active"] is True
    assert target.registry.devices._chain(target.record) == chain


def test_bypass_adoption_rejects_mode_mismatch_and_changes_after_preview(target: Any) -> None:
    """Current intent agreement cannot silently turn an active effect into authored bypass."""
    insert_source(target)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    with pytest.raises(RuntimeError, match="AuthoredNativeMismatch"):
        target.registry.devices.preview(target.record, bypass_preview_request(target))
    apply_modes(
        target,
        mode_request(target, "Utility", [{"capability_id": "utility.enabled", "label": "Off"}]),
    )
    preview = target.registry.devices.preview(target.record, bypass_preview_request(target))
    approved = fixture.adopt_request(target, preview)
    target.record["track"].devices[1].parameters[0]._value = 1.0
    with pytest.raises(RuntimeError, match="changed after approval"):
        target.registry.devices.adopt(
            target.record,
            approved,
            {"request": approved, "outcome": "pending", "native_mutation_started": False},
        )


def test_private_cohort_retains_exact_devices_and_parameters_without_refresh(target: Any) -> None:
    """A read-only witness survives legitimate value ACKs and rejects object replacement."""
    insert_source(target)
    native = target.record["track"].devices[0]
    witness = target.registry.devices.private_cohort(target.record)
    assert witness == (native,) + tuple(native.parameters)
    baseline = target.record["_managed_devices"]["baseline"]
    assert target.registry.devices.private_cohort(target.record) == witness
    assert target.record["_managed_devices"]["baseline"] == baseline
    native.parameters[1]._value = 0.25
    with pytest.raises(RuntimeError, match="state drift"):
        target.registry.devices.private_cohort(target.record)
    assert target.record["_managed_devices"]["baseline"] == baseline
    native.parameters[1]._value = 0.0
    replacement = fixture.Parameter("LP Freq", lambda _: "200.00 Hz")
    replacement._canonical_parent = native
    native._parameters = (native.parameters[0], replacement) + native.parameters[2:]
    with pytest.raises(RuntimeError, match="parameter identity"):
        target.registry.devices.private_cohort(target.record)


@pytest.mark.parametrize("late", (False, True))
def test_mode_capacity_refusal_precedes_setters_or_retains_truthful_started_outcome(
    target: Any, monkeypatch: pytest.MonkeyPatch, late: bool
) -> None:
    """Finite frame proof is checked before writing and again before publishing an ACK."""
    import Sunny.managed_device_modes as module

    insert_source(target)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    device = target.registry.devices._chain(target.record)[1]
    original = module.guard_managed_response_capacity
    calls = []

    def bounded(operation: Any, result: Any) -> int:
        calls.append(result)
        if len(calls) == (2 if late else 1):
            raise RuntimeError("literal reply capacity limit")
        return original(operation, result)

    monkeypatch.setattr(module, "guard_managed_response_capacity", bounded)
    old = target.record["_managed_devices"]["baseline"]
    payload = mode_request(
        target, "Utility", [{"capability_id": "utility.enabled", "label": "Off"}]
    )
    with pytest.raises(RuntimeError, match="capacity limit"):
        apply_modes(target, payload)
    assert target.registry._operations[payload["operation_id"]]["native_mutation_started"] is late
    assert target.record["_managed_devices"]["baseline"] == old
    assert device.parameters[0].writes == ([0.0] if late else [])


def test_mode_readback_mismatch_never_seals_a_requested_label(target: Any) -> None:
    """A clamped native enum setter produces retained partial evidence, not a false ACK."""
    insert_source(target)
    fixture.apply(target, fixture.request(target, "effect", "Utility"))
    device = target.registry.devices._chain(target.record)[1]
    device.parameters[0].clamp = True
    old = target.record["_managed_devices"]["baseline"]
    payload = mode_request(
        target, "Utility", [{"capability_id": "utility.enabled", "label": "Off"}]
    )
    with pytest.raises(RuntimeError, match="integer|bypassed|untouched|discrete"):
        apply_modes(target, payload)
    assert target.registry._operations[payload["operation_id"]]["native_mutation_started"] is True
    assert target.record["_managed_devices"]["baseline"] == old
    assert device.parameters[0].writes == [0.0]
