"""Independent display-oracle fixtures; none claims an actual Live unit curve."""

from __future__ import annotations

import json
import math
import sys
from types import SimpleNamespace
from typing import Any

import pytest
from Sunny.native_units import NativeUnitError, resolve_native_display_value


class _NativeParameter:
    def __init__(
        self,
        name: str,
        oracle: Any = None,
        *,
        minimum: float = 0.0,
        maximum: float = 1.0,
        value: float = 0.0,
        items: tuple[str, ...] = (),
    ) -> None:
        self.name = name
        self.original_name = name
        self.min = float(minimum)
        self.max = float(maximum)
        self._value = float(value)
        self.default_value = float(value)
        self.is_quantized = bool(items)
        self.is_enabled = True
        self.state = 0
        self.automation_state = 0
        self.value_items = items
        self.canonical_parent: Any = None
        self.oracle = oracle
        self.calls: list[float] = []
        self.writes = 0
        self.callback: Any = None

    @property
    def value(self) -> float:
        return self._value

    @value.setter
    def value(self, value: float) -> None:
        self.writes += 1
        raise AssertionError("The read-only resolver attempted a native write")

    @property
    def display_value(self) -> float:
        raise AssertionError("Python numeric display_value is deliberately unavailable")

    def str_for_value(self, value: float) -> Any:
        self.calls.append(value)
        if self.callback:
            self.callback(value)
        return self.oracle(value)


class _NativeDevice:
    def __init__(self, class_name: str, parameters: list[_NativeParameter]) -> None:
        self.class_name = class_name
        self.type = 2
        self.is_active = True
        self.can_have_chains = False
        self.global_mode = 0
        self.parameters = tuple(parameters)
        for parameter in parameters:
            parameter.canonical_parent = self


@pytest.fixture(autouse=True)
def native_contract(monkeypatch: pytest.MonkeyPatch) -> None:
    """Install only native identities; the production resolver must use its oracle."""
    monkeypatch.setitem(
        sys.modules,
        "Live",
        SimpleNamespace(
            DeviceParameter=SimpleNamespace(DeviceParameter=_NativeParameter),
            Device=SimpleNamespace(Device=_NativeDevice),
        ),
    )


def _enum(name: str, items: tuple[str, ...], index: int) -> _NativeParameter:
    return _NativeParameter(name, minimum=0, maximum=len(items) - 1, value=index, items=items)


def _target(
    original: str,
    unit: str,
    oracle: Any,
    *,
    device_class: str = "StereoGain",
    minimum: float = 0,
    maximum: float = 1,
    value: float = 0,
) -> SimpleNamespace:
    parameter = _NativeParameter(original, oracle, minimum=minimum, maximum=maximum, value=value)
    if device_class == "StereoGain":
        # Deliberately nonstandard label orders prevent hidden ordinal assumptions.
        parameters = [
            parameter,
            _enum("Channel Mode", ("Right", "Stereo", "Swap", "Left"), 1),
            _enum("Mono", ("On", "Off"), 1),
            _enum("Mute", ("Off", "On"), 0),
        ]
        modes = {"Channel Mode": "Stereo", "Mono": "Off", "Mute": "Off"}
    else:
        parameters = [
            parameter,
            _NativeParameter(
                "Scale", lambda value: f"{value * 200:.1f} %", minimum=-2, maximum=2, value=0.5
            ),
            _enum("Adaptive Q", ("On", "Off"), 1),
        ]
        modes = {"global_mode": 0}
        if original != "Output Gain":
            name = original.split()[0] + " Filter On A"
            parameters.append(_enum(name, ("On", "Off"), 0))
            modes[name] = "On"
    device = _NativeDevice(device_class, parameters)
    return SimpleNamespace(parameter=parameter, device=device, unit=unit, modes=modes)


def _resolve(fixture: SimpleNamespace, target: float, tolerance: float = 0.0) -> dict[str, Any]:
    return resolve_native_display_value(
        fixture.parameter,
        device=fixture.device,
        device_class_name=fixture.device.class_name,
        parameter_original_name=fixture.parameter.original_name,
        unit=fixture.unit,
        target=target,
        tolerance=tolerance,
        expected_modes=fixture.modes,
    )


def _parameter(fixture: SimpleNamespace, name: str) -> _NativeParameter:
    return next(
        parameter for parameter in fixture.device.parameters if parameter.original_name == name
    )


def test_nonlinear_db_inversion_preserves_native_value_and_literal_display() -> None:
    """Invert a cubic test oracle on -3..7 without borrowing a native gain formula."""
    fixture = _target(
        "Gain",
        "Decibels",
        lambda value: f"{-40 + 75 * ((value + 3) / 10) ** 3:.2f} dB",
        minimum=-3,
        maximum=7,
        value=1.125,
    )
    result = _resolve(fixture, -6)
    expected_internal = -3 + 10 * math.pow(34 / 75, 1 / 3)
    assert abs(result["internal_value"] - expected_internal) < 0.002
    assert result["display"] == "-6.00 dB"
    assert result["display_value"] == -6.0
    assert result["display_increment"] == 0.01
    assert result["absolute_display_error"] == 0.0
    assert result["descriptor"]["value"] == fixture.parameter.value == 1.125
    assert fixture.parameter.writes == 0
    assert result["formatter_calls"] == len(fixture.parameter.calls) <= 64
    assert [sample["phase"] for sample in result["samples"][-3:]] == ["repeat"] * 3
    assert result["host_qualified"] is False
    assert result["native_knob_only"] is True
    json.dumps(result, allow_nan=False)


@pytest.mark.parametrize("band", range(1, 9))
def test_hertz_prefix_conversion_and_decimal_precision_are_literal(band: int) -> None:
    """The cubic oracle's exact midpoint is 1200 Hz, displayed as 1.20 kHz."""

    def oracle(value: float) -> str:
        hertz = 200 + 8000 * ((value + 2) / 8) ** 3
        return f"{hertz / 1000:.2f} kHz" if hertz >= 1000 else f"{hertz:.1f} Hz"

    fixture = _target(
        f"{band} Frequency A", "Hertz", oracle, device_class="Eq8", minimum=-2, maximum=6, value=-1
    )
    result = _resolve(fixture, 1200)
    assert result["internal_value"] == 2.0
    assert result["display"] == "1.20 kHz"
    assert result["display_value"] == 1200.0
    assert result["display_increment"] == 10.0
    assert result["eq8_scale_display"] == {
        "display": "100.0 %",
        "display_value": 100.0,
        "display_increment": 0.1,
    }
    assert result["modes"]["Adaptive Q"]["label"] == "Off"
    assert fixture.parameter.value == -1.0
    assert result["formatter_calls"] == len(fixture.parameter.calls) + 1
    assert len(_parameter(fixture, "Scale").calls) == 1


@pytest.mark.parametrize("original", [f"{band} Gain A" for band in range(1, 9)] + ["Output Gain"])
def test_eq_gain_and_output_gain_use_the_registered_db_identity(original: str) -> None:
    """Every finite gain identity resolves a literal 3 dB value on a foreign internal domain."""
    fixture = _target(
        original,
        "Decibels",
        lambda value: f"{value * 2:.2f} dB",
        device_class="Eq8",
        minimum=-6,
        maximum=6,
    )
    result = _resolve(fixture, 3)
    assert result["display"] == "3.00 dB"
    assert result["internal_value"] == 1.5
    assert fixture.parameter.value == 0.0


@pytest.mark.parametrize("band", range(1, 9))
def test_q_is_dimensionless_only_under_its_exact_registered_identity(band: int) -> None:
    """A squared test oracle has Q=4 at internal 2; no normalized Q scale is guessed."""
    fixture = _target(
        f"{band} Resonance A",
        "QualityFactor",
        lambda value: f"{value * value:.2f}",
        device_class="Eq8",
        minimum=1,
        maximum=4,
        value=1,
    )
    result = _resolve(fixture, 4)
    assert result["display_value"] == 4.0
    assert result["display_increment"] == 0.01
    assert abs(result["internal_value"] - 2.0) < 0.002


def test_width_percent_and_native_balance_have_independent_scales() -> None:
    """Percent and observed L/R display coordinates are not internal 0..1 values."""
    width = _target(
        "Stereo Width",
        "Percent",
        lambda value: f"{400 * ((value + 5) / 20) ** 2:.1f} %",
        minimum=-5,
        maximum=15,
        value=-5,
    )
    result = _resolve(width, 100)
    assert result["internal_value"] == 5.0
    assert result["display_value"] == 100.0
    assert result["display_increment"] == 0.1

    def balance_oracle(value: float) -> str:
        amount = -50 + 100 * ((value + 8) / 10) ** 3
        return "C" if abs(amount) < 0.5 else f"{abs(amount):.0f}{'L' if amount < 0 else 'R'}"

    balance = _target("Balance", "StereoBalance", balance_oracle, minimum=-8, maximum=2, value=-8)
    result = _resolve(balance, 0.5)
    assert result["display"] == "25R"
    assert result["display_value"] == 0.5
    assert result["balance_full_scale"] == 50.0
    assert result["display_increment"] == 0.02
    expected_internal = -8 + 10 * math.pow(0.75, 1 / 3)
    assert abs(result["internal_value"] - expected_internal) < 0.03


def test_balance_calibration_observes_full_scale_and_center_without_hardcoding_50() -> None:
    """An independent 100L/100R oracle uses a 0.01 coordinate increment and C=0."""

    def oracle(value: float) -> str:
        amount = value * 100
        return "C" if amount == 0 else f"{abs(amount):.0f}{'L' if amount < 0 else 'R'}"

    fixture = _target("Balance", "StereoBalance", oracle, minimum=-1, maximum=1)
    centered = _resolve(fixture, 0)
    assert centered["display"] == "C"
    assert centered["display_value"] == 0.0
    assert centered["display_increment"] is None
    half_left = _resolve(fixture, -0.5)
    assert half_left["display"] == "50L"
    assert half_left["display_increment"] == 0.01
    assert half_left["balance_full_scale"] == 100.0


def test_infinity_bound_and_plateaus_are_usable_without_infinite_json_values() -> None:
    """Finite -20 dB remains reachable above a native display's -inf plateau."""
    fixture = _target(
        "Gain",
        "Decibels",
        lambda value: "-inf dB" if value <= -7 else f"{(value + 7) * 2 - 30:.2f} dB",
        minimum=-10,
        maximum=10,
        value=-10,
    )
    result = _resolve(fixture, -20)
    assert result["display"] == "-20.00 dB"
    assert abs(result["internal_value"] + 2) < 0.003
    assert result["samples"][0]["display_value"] is None
    assert result["samples"][0]["negative_infinity"] is True
    assert fixture.parameter.value == -10.0
    json.dumps(result, allow_nan=False)


def test_explicit_tolerance_is_not_enlarged_to_hide_a_display_gap() -> None:
    """A 0/100 jump cannot represent 50 within tolerance 1, despite is_quantized=False."""
    fixture = _target("Stereo Width", "Percent", lambda value: "0 %" if value < 0.5 else "100 %")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 50, 1)
    assert raised.value.reason == "ToleranceNotMet"
    assert raised.value.formatter_calls <= 61
    assert fixture.parameter.value == 0.0
    assert fixture.parameter.writes == 0
    assert len(fixture.parameter.calls) <= 64


@pytest.mark.parametrize("raw", ["1,20 kHz", "1.20 KHz", "1200", "1e3 Hz", "1.20\u00a0kHz", 1200.0])
def test_locale_missing_unit_and_wrong_result_categories_are_declined(raw: Any) -> None:
    """No native physical identity is inferred from a malformed or localized string."""
    fixture = _target("1 Frequency A", "Hertz", lambda _: raw, device_class="Eq8")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 1200)
    assert raised.value.reason == "UnsupportedDisplay"
    assert fixture.parameter.writes == 0


@pytest.mark.parametrize(
    "change,reason",
    [
        (lambda fixture: setattr(fixture.parameter, "is_enabled", False), "InactiveParameter"),
        (lambda fixture: setattr(fixture.parameter, "state", 1), "InactiveParameter"),
        (lambda fixture: setattr(fixture.parameter, "state", 2), "InactiveParameter"),
        (lambda fixture: setattr(fixture.parameter, "automation_state", 1), "ExistingAutomation"),
        (lambda fixture: setattr(fixture.parameter, "automation_state", 2), "ExistingAutomation"),
        (lambda fixture: setattr(fixture.parameter, "is_quantized", True), "InvalidDomain"),
        (
            lambda fixture: setattr(fixture.parameter, "canonical_parent", object()),
            "ParameterMismatch",
        ),
        (lambda fixture: setattr(fixture.device, "can_have_chains", True), "DeviceMismatch"),
        (lambda fixture: setattr(fixture.device, "is_active", False), "DeviceMismatch"),
        (
            lambda fixture: setattr(fixture.parameter, "str_for_value", None),
            "ObservationUnavailable",
        ),
    ],
)
def test_unavailable_ineligible_and_foreign_parameters_never_reach_formatter(
    change: Any, reason: str
) -> None:
    """Admission fails before sampling or writing when native facts are unsuitable."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    change(fixture)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == reason
    assert raised.value.formatter_calls == 0
    assert fixture.parameter.calls == []
    assert fixture.parameter.writes == 0


def test_mode_policy_is_registered_and_cannot_be_bypassed_by_caller() -> None:
    """A caller cannot omit Mute or request a non-Stereo mode to weaken policy."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    fixture.modes.pop("Mute")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "InvalidIntent"
    assert raised.value.formatter_calls == 0
    fixture.modes["Mute"] = "Off"
    _parameter(fixture, "Mute")._value = 1.0
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "UnsupportedMode"


def test_width_rejects_an_active_mid_side_substitute() -> None:
    """The same GUI slot does not establish Width semantics in Mid/Side mode."""
    fixture = _target("Stereo Width", "Percent", lambda value: f"{value * 100:.1f} %")
    alternative = _NativeParameter("Mid/Side Balance")
    alternative.canonical_parent = fixture.device
    fixture.device.parameters += (alternative,)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 50)
    assert raised.value.reason == "UnsupportedMode"
    alternative.is_enabled = False
    alternative.state = 2
    result = _resolve(fixture, 50)
    assert result["display_value"] == 50.0


@pytest.mark.parametrize("mode", [1, 2, True])
def test_eq_global_modes_do_not_default_to_stereo(mode: Any) -> None:
    """A/B frequency identity is admitted only with actual native global_mode=0."""
    fixture = _target(
        "1 Frequency A", "Hertz", lambda value: f"{100 + value:.1f} Hz", device_class="Eq8"
    )
    fixture.device.global_mode = mode
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 100.5)
    assert raised.value.reason == "UnsupportedMode"
    assert raised.value.formatter_calls == 0


def test_eq_scale_and_adaptive_q_are_retained_without_promoting_a_dsp_claim() -> None:
    """50% Scale and Adaptive Q On leave a 3 dB knob candidate explicitly limited."""
    fixture = _target(
        "1 Gain A", "Decibels", lambda value: f"{value * 6:.2f} dB", device_class="Eq8"
    )
    _parameter(fixture, "Scale")._value = 0.25
    _parameter(fixture, "Adaptive Q")._value = 0.0
    result = _resolve(fixture, 3)
    assert result["eq8_scale_display"]["display_value"] == 50.0
    assert result["modes"]["Adaptive Q"]["label"] == "On"
    assert result["native_knob_only"] is True
    assert result["host_qualified"] is False


@pytest.mark.parametrize("drift", ["value", "bounds", "mode", "population", "scale", "adaptive"])
def test_descriptor_mode_and_identity_drift_decline_without_compensating_writes(drift: str) -> None:
    """A native/user change during the oracle is detected and is never overwritten."""
    fixture = _target(
        "1 Gain A", "Decibels", lambda value: f"{value * 6:.2f} dB", device_class="Eq8"
    )

    def change(_: float) -> None:
        if drift == "value":
            fixture.parameter._value = 0.75
        elif drift == "bounds":
            fixture.parameter.max = 2.0
        elif drift == "mode":
            fixture.device.global_mode = 1
        elif drift == "scale":
            _parameter(fixture, "Scale")._value = 0.75
        elif drift == "adaptive":
            _parameter(fixture, "Adaptive Q")._value = 0.0
        else:
            replacement = _NativeParameter("1 Gain A", fixture.parameter.oracle)
            replacement.canonical_parent = fixture.device
            fixture.device.parameters = (replacement,) + fixture.device.parameters[1:]

    fixture.parameter.callback = change
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 3)
    assert raised.value.reason == "ObservationDrift"
    assert fixture.parameter.writes == 0
    assert raised.value.formatter_calls == 2  # one Scale call, then first target call
    if drift == "value":
        assert fixture.parameter.value == 0.75


def test_display_drift_is_detected_by_the_required_anchor_repeats() -> None:
    """An endpoint changes its text only on the repeat, despite unchanged internal state."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    fixture.parameter.oracle = lambda value: (
        "0.00 dB" if value == 0 and len(fixture.parameter.calls) <= 17 else f"{value + 0.01:.2f} dB"
    )
    # Initially all nonzero values include +0.01, so the midpoint is 0.51 dB.
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.51)
    assert raised.value.reason == "DisplayDrift"
    assert raised.value.formatter_calls == 18


@pytest.mark.parametrize(
    "oracle,reason",
    [
        (lambda value: f"{100 - value * 100:.1f} %", "NonmonotonicDisplay"),
        (lambda _: "100 %", "UnresolvedDisplay"),
    ],
)
def test_nonmonotonic_and_constant_display_domains_are_not_invented(
    oracle: Any, reason: str
) -> None:
    """Neither an observed reversal nor a constant readout supplies a useful inverse."""
    fixture = _target("Stereo Width", "Percent", oracle)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 50)
    assert raised.value.reason == reason
    assert len(fixture.parameter.calls) <= 64


def test_ambiguous_alias_and_wrong_registered_units_decline_before_sampling() -> None:
    """An alias collision and Hz intent on Gain cannot silently select another identity."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    other = _NativeParameter("Other")
    other.name = "Gain"
    other.canonical_parent = fixture.device
    fixture.device.parameters += (other,)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "AmbiguousParameter"
    fixture.unit = "Hertz"
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 440)
    assert raised.value.reason == "UnknownCapability"
    assert fixture.parameter.calls == []


def test_runtime_formatter_failure_is_a_precise_read_only_decline() -> None:
    """A native API exception does not trigger a formula, display_value or write fallback."""

    def unavailable(_: float) -> str:
        raise RuntimeError("native argument unavailable")

    fixture = _target("Gain", "Decibels", unavailable)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "ObservationUnavailable"
    assert raised.value.formatter_calls == 1
    assert fixture.parameter.writes == 0


@pytest.mark.parametrize(
    "target,tolerance", [(float("nan"), 0), (0.5, float("inf")), (True, 0), (0.5, -1)]
)
def test_invalid_intents_never_touch_the_native_formatter(target: Any, tolerance: Any) -> None:
    """Only finite explicit numeric targets and nonnegative tolerances are admitted."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, target, tolerance)
    assert raised.value.reason == "InvalidIntent"
    assert fixture.parameter.calls == []


def test_success_can_use_all_64_calls_while_reserving_three_anchor_repeats() -> None:
    """The last admitted bisection reaches 2**-48 before all three repeat checks."""
    fixture = _target("Stereo Width", "Percent", lambda value: f"{value:.40f} %")
    result = _resolve(fixture, 2**-48, 1e-29)
    assert result["internal_value"] == 2**-48
    assert result["formatter_calls"] == len(fixture.parameter.calls) == 64
    assert result["display_increment"] == 1e-40
    assert [sample["phase"] for sample in result["samples"]].count("search") == 44
    assert [sample["phase"] for sample in result["samples"][-3:]] == ["repeat"] * 3
    assert fixture.parameter.value == 0.0
    assert fixture.parameter.writes == 0


@pytest.mark.parametrize(
    "field,value,reason",
    [
        ("min", float("nan"), "InvalidObservation"),
        ("max", float("inf"), "InvalidObservation"),
        ("max", 0.0, "InvalidDomain"),
        ("_value", 2.0, "InvalidDomain"),
        ("_value", 0, "InvalidObservation"),
        ("default_value", 2.0, "InvalidDomain"),
        ("is_enabled", 1, "InvalidObservation"),
        ("state", True, "InvalidObservation"),
    ],
)
def test_malformed_native_descriptors_are_never_normalized(
    field: str, value: Any, reason: str
) -> None:
    """Typed native facts are validated before invoking the display oracle."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    setattr(fixture.parameter, field, value)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == reason
    assert raised.value.formatter_calls == 0


def test_mode_label_and_parameter_collections_are_bounded_even_when_malformed() -> None:
    """An infinite native-looking collection is stopped at the declared finite boundary."""
    import itertools

    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    _parameter(fixture, "Mono").value_items = itertools.repeat("Off")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "InvalidDomain"
    assert raised.value.formatter_calls == 0
    fixture.device.parameters = itertools.repeat(fixture.parameter)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "InvalidObservation"
    assert raised.value.formatter_calls == 0


def test_outside_display_domain_is_rejected_instead_of_clamped() -> None:
    """100 percent is the actual observed maximum; a 200 percent request is unchanged."""
    fixture = _target("Stereo Width", "Percent", lambda value: f"{value * 100:.1f} %")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 200, 1)
    assert raised.value.reason == "ValueOutsideDomain"
    assert fixture.parameter.value == 0.0
    assert fixture.parameter.writes == 0


def test_asymmetric_balance_endpoints_are_not_given_a_speculative_coordinate() -> None:
    """A 100L/50R range does not satisfy the declared symmetric native balance profile."""
    fixture = _target("Balance", "StereoBalance", lambda value: "100L" if value < 0.5 else "50R")
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "UnsupportedDisplay"


def test_missing_eq_coupling_evidence_and_disabled_band_do_not_default() -> None:
    """Native band admission requires explicit activation and both coupling observations."""
    fixture = _target(
        "1 Gain A", "Decibels", lambda value: f"{value * 6:.2f} dB", device_class="Eq8"
    )
    fixture.device.parameters = tuple(
        parameter for parameter in fixture.device.parameters if parameter.original_name != "Scale"
    )
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 3)
    assert raised.value.reason == "ObservationUnavailable"
    assert raised.value.formatter_calls == 0
    _parameter(fixture, "1 Filter On A")._value = 1.0  # Off in the reversed fixture labels.
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 3)
    assert raised.value.reason == "UnsupportedMode"


def test_native_identity_cannot_be_replaced_with_a_duck_typed_wrapper() -> None:
    """Matching names and numeric fields do not prove the native parameter type."""
    fixture = _target("Gain", "Decibels", lambda value: f"{value:.2f} dB")
    fixture.parameter = SimpleNamespace(**fixture.parameter.__dict__)
    with pytest.raises(NativeUnitError) as raised:
        _resolve(fixture, 0.5)
    assert raised.value.reason == "ParameterMismatch"
    assert raised.value.formatter_calls == 0
