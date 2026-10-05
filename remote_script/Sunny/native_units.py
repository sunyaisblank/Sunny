"""Read-only inversion of a finite set of native parameter display strings.

The caller captures actual Live objects on the main thread, applies the separate
version/ownership policy, then calls this resolver within that same operation.
Only ``value`` and the source-observed native ``str_for_value`` API are used.
The result is a display-resolution candidate, not a DSP transfer-function,
Python ABI, edition, operating-system, envelope, or persistence qualification.
"""

from __future__ import annotations

import math
import re
from decimal import ROUND_HALF_EVEN, Decimal, localcontext
from typing import Any, NoReturn

FORMATTER_BUDGET = 64
SOURCE_COMMIT = "e83d5192f321b24eb9daab843ac49a2d95d862b1"
_MAX_PARAMETERS = 512
_NUMBER = r"([+-]?(?:[0-9]+(?:\.[0-9]+)?|\.[0-9]+))"


class NativeUnitError(RuntimeError):
    """A bounded decline; callers must not turn one into a fallback write."""

    def __init__(self, reason: str, diagnostic: str, formatter_calls: int = 0) -> None:
        super().__init__(diagnostic)
        self.reason = reason
        self.formatter_calls = formatter_calls


def _fail(reason: str, diagnostic: str) -> NoReturn:
    raise NativeUnitError(reason, diagnostic)


def _number(value: Any, label: str, *, native: bool = False) -> float:
    if type(value) is not float if native else type(value) not in (int, float):
        _fail("InvalidObservation" if native else "InvalidIntent", label + " must be numeric")
    try:
        result = float(value)
    except (OverflowError, ValueError):
        _fail("InvalidIntent", label + " is not a finite number")
    if not math.isfinite(result):
        _fail("InvalidObservation" if native else "InvalidIntent", label + " must be finite")
    return result


def _same(first: Any, second: Any) -> bool:
    return first is second or bool(first == second)


def _state(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int) or value not in (0, 1, 2):
        _fail("InvalidObservation", label + " must be a native state in 0..2")
    return int(value)


def _descriptor(parameter: Any, *, quantized: bool, eligible: bool) -> dict[str, Any]:
    minimum = _number(parameter.min, "Parameter minimum", native=True)
    maximum = _number(parameter.max, "Parameter maximum", native=True)
    value = _number(parameter.value, "Parameter current value", native=True)
    if minimum >= maximum or not minimum <= value <= maximum:
        _fail("InvalidDomain", "Parameter domain must be nondegenerate and contain its value")
    if type(parameter.is_quantized) is not bool or parameter.is_quantized != quantized:
        _fail("InvalidDomain", "Parameter quantization differs from the registered control")
    if type(parameter.is_enabled) is not bool:
        _fail("InvalidObservation", "Parameter is_enabled must be a native Boolean")
    state = _state(parameter.state, "Parameter state")
    automation = _state(parameter.automation_state, "Parameter automation_state")
    if eligible and (not parameter.is_enabled or state != 0):
        _fail(
            "InactiveParameter", "The target or required mode parameter is not active and enabled"
        )
    if eligible and automation != 0:
        _fail("ExistingAutomation", "Initial native value admission requires automation_state=0")
    result: dict[str, Any] = {
        "minimum": minimum,
        "maximum": maximum,
        "value": value,
        "is_quantized": quantized,
        "is_enabled": parameter.is_enabled,
        "state": state,
        "automation_state": automation,
    }
    if quantized:
        raw = parameter.value_items
        if isinstance(raw, (str, bytes, dict)):
            _fail("InvalidDomain", "Mode labels must be a native string collection")
        items: list[str] = []
        for item in raw:
            if len(items) >= 64:
                _fail("InvalidDomain", "Mode label population exceeds the finite limit")
            items.append(item)
        if (
            len(items) < 2
            or any(type(item) is not str or not item for item in items)
            or len(set(items)) != len(items)
            or minimum != 0.0
            or maximum != float(len(items) - 1)
            or value != math.trunc(value)
        ):
            _fail("InvalidDomain", "Mode labels require an observed unique zero-based enum domain")
        result["value_items"] = list(items)
        result["label"] = items[int(value)]
    else:
        default = _number(parameter.default_value, "Parameter default_value", native=True)
        if not minimum <= default <= maximum:
            _fail("InvalidDomain", "Parameter default is outside its observed domain")
        result["default_value"] = default
    return result


def _policy(device_class: str, original_name: str, unit: str) -> dict[str, Any]:
    if device_class == "StereoGain":
        units = {"Gain": "Decibels", "Balance": "StereoBalance", "Stereo Width": "Percent"}
        if units.get(original_name) != unit:
            _fail("UnknownCapability", "Utility identity/unit is outside the finite registry")
        return {"Channel Mode": "Stereo", "Mono": "Off", "Mute": "Off"}
    if device_class == "Eq8":
        if (original_name == "Output Gain" and unit == "Decibels") or (
            original_name == "Scale" and unit == "Percent"
        ):
            return {"global_mode": 0}
        match = re.fullmatch(r"([1-8]) (Frequency|Gain|Resonance) A", original_name)
        units = {"Frequency": "Hertz", "Gain": "Decibels", "Resonance": "QualityFactor"}
        if match and units[match.group(2)] == unit:
            return {"global_mode": 0, match.group(1) + " Filter On A": "On"}
    if device_class == "Drift":
        units = {
            "LP Freq": "Hertz",
            "Env 1 Attack": "Milliseconds",
            "Env 1 Decay": "Milliseconds",
            "Env 1 Release": "Milliseconds",
        }
        if units.get(original_name) == unit:
            observed = {"voice_mode": None, "voice_count": None}
            if original_name == "LP Freq":
                observed["LP Type"] = None
            return observed
    _fail("UnknownCapability", "Device/control/unit is outside the finite registry")


def _registered_context(capability_id: str) -> tuple[str, str, str, dict[str, Any]]:
    """Select only the 33 continuous identities in registry version 3.

    Callers supply no descriptor or mode policy. The native observation below
    still verifies every identity and required mode against the real population.
    """
    if type(capability_id) is not str:
        _fail("InvalidIntent", "Capability ID must be a string")
    utility = {
        "utility.gain": ("Gain", "Decibels"),
        "utility.balance": ("Balance", "StereoBalance"),
        "utility.width": ("Stereo Width", "Percent"),
    }
    if capability_id in utility:
        original_name, unit = utility[capability_id]
        return "StereoGain", original_name, unit, _policy("StereoGain", original_name, unit)
    drift = {
        "drift.lp.frequency": ("LP Freq", "Hertz"),
        "drift.env.1.attack": ("Env 1 Attack", "Milliseconds"),
        "drift.env.1.decay": ("Env 1 Decay", "Milliseconds"),
        "drift.env.1.release": ("Env 1 Release", "Milliseconds"),
    }
    if capability_id in drift:
        original_name, unit = drift[capability_id]
        return "Drift", original_name, unit, _policy("Drift", original_name, unit)
    if capability_id == "eq8.scale":
        return "Eq8", "Scale", "Percent", _policy("Eq8", "Scale", "Percent")
    if capability_id == "eq8.output_gain":
        return "Eq8", "Output Gain", "Decibels", _policy("Eq8", "Output Gain", "Decibels")
    match = re.fullmatch(r"eq8\.band\.([1-8])\.(frequency|gain|q)", capability_id)
    if match:
        control, unit = {
            "frequency": ("Frequency", "Hertz"),
            "gain": ("Gain", "Decibels"),
            "q": ("Resonance", "QualityFactor"),
        }[match.group(2)]
        original_name = match.group(1) + " " + control + " A"
        return "Eq8", original_name, unit, _policy("Eq8", original_name, unit)
    _fail("UnknownCapability", "Capability is outside the finite continuous display registry")


def registered_native_mode_context(capability_id: str) -> tuple[str, str, int]:
    """Mirror only quantized identities in the same finite source registry.

    The managed owner still observes actual parent/type/domain/labels and retains
    the objects. No expected current mode or caller-supplied descriptor is used.
    """
    controls = {
        "utility.enabled": ("StereoGain", "Device On", 2),
        "utility.channel_mode": ("StereoGain", "Channel Mode", 4),
        "utility.mono": ("StereoGain", "Mono", 2),
        "utility.mute": ("StereoGain", "Mute", 2),
        "utility.left_invert": ("StereoGain", "Left Inv", 2),
        "utility.right_invert": ("StereoGain", "Right Inv", 2),
        "utility.bass_mono": ("StereoGain", "Bass Mono", 2),
        "utility.dc_filter": ("StereoGain", "DC Filter", 2),
        "eq8.enabled": ("Eq8", "Device On", 2),
        "eq8.adaptive_q": ("Eq8", "Adaptive Q", 2),
    }
    if capability_id in controls:
        return controls[capability_id]
    match = re.fullmatch(r"eq8\.band\.([1-8])\.(enabled|type)", capability_id)
    if match:
        enabled = match.group(2) == "enabled"
        return (
            "Eq8",
            match.group(1) + (" Filter On A" if enabled else " Filter Type A"),
            2 if enabled else 8,
        )
    _fail("UnknownCapability", "Mode identity is outside the finite native registry")


def valid_native_display_request(query: Any) -> bool:
    """Validate the closed wire request without observing any native object."""
    try:
        if type(query) is not dict or set(query) != {"capability_id", "target", "tolerance"}:
            return False
        _registered_context(query["capability_id"])
        _number(query["target"], "Display target")
        return _number(query["tolerance"], "Display tolerance") >= 0
    except NativeUnitError:
        return False


def resolve_registered_native_display_value(
    device: Any,
    capability_id: str,
    target: float,
    tolerance: float,
    *,
    retained_parameters: Any = None,
) -> dict[str, Any]:
    """Resolve an actual native population member from a closed capability ID.

    This is the bridge entry point. Edition, version and main-thread admission
    belong to the enclosing handler operation. No caller-selected name, unit,
    policy, internal curve or native value write is accepted here.
    """
    try:
        device_class, original_name, unit, policy = _registered_context(capability_id)
        import Live

        if not isinstance(device, Live.Device.Device) or device == None:  # noqa: E711
            _fail("DeviceMismatch", "An actual valid native Device is required")
        raw = device.parameters if retained_parameters is None else retained_parameters
        if isinstance(raw, (str, bytes, dict)):
            _fail("InvalidObservation", "Device.parameters is not a native object collection")
        matches = []
        for index, member in enumerate(raw):
            if index >= _MAX_PARAMETERS:
                _fail("InvalidObservation", "Device parameter population exceeds the finite limit")
            if (
                not isinstance(member, Live.DeviceParameter.DeviceParameter)
                or member == None  # noqa: E711
                or not _same(member.canonical_parent, device)
                or type(member.name) is not str
                or not member.name
                or type(member.original_name) is not str
                or not member.original_name
            ):
                _fail(
                    "ParameterMismatch", "Population members require native type, parent and names"
                )
            if member.name == original_name or member.original_name == original_name:
                matches.append(member)
        if len(matches) != 1:
            _fail(
                "AmbiguousParameter" if matches else "ObservationUnavailable",
                "Expected one exact native parameter identity: " + original_name,
            )
        return resolve_native_display_value(
            matches[0],
            device=device,
            device_class_name=device_class,
            parameter_original_name=original_name,
            unit=unit,
            target=target,
            tolerance=tolerance,
            expected_modes=policy,
        )
    except NativeUnitError:
        raise
    except Exception as error:
        raise NativeUnitError(
            "ObservationUnavailable", "Native observation failed: " + str(error)
        ) from error


class _Observation:
    def __init__(
        self,
        parameter: Any,
        device: Any,
        device_class: str,
        original_name: str,
        policy: dict[str, Any],
    ) -> None:
        import Live

        self.parameter = parameter
        self.device = device
        self.device_class = device_class
        self.original_name = original_name
        self.policy = policy
        self.parameter_type = Live.DeviceParameter.DeviceParameter
        self.device_type = Live.Device.Device
        self.initial = self.capture()

    def capture(self) -> dict[str, Any]:
        device, parameter = self.device, self.parameter
        if (
            not isinstance(device, self.device_type)
            or device == None  # noqa: E711
            or not isinstance(parameter, self.parameter_type)
            or parameter == None  # noqa: E711
        ):
            _fail(
                "ParameterMismatch",
                "Actual valid native Device and DeviceParameter objects are required",
            )
        if (
            device.class_name != self.device_class
            or isinstance(device.type, bool)
            or not isinstance(device.type, int)
            or device.type != (1 if self.device_class == "Drift" else 2)
            or device.is_active is not True
            or device.can_have_chains is not False
        ):
            _fail(
                "DeviceMismatch", "An active flat native device of the registered role is required"
            )
        raw = device.parameters
        if isinstance(raw, (str, bytes, dict)):
            _fail("InvalidObservation", "Device.parameters is not a native object collection")
        # Iteration is bounded even for a malformed or infinite host collection.
        population: list[Any] = []
        for member in raw:
            if len(population) >= _MAX_PARAMETERS:
                _fail("InvalidObservation", "Device parameter population exceeds the finite limit")
            if (
                not isinstance(member, self.parameter_type)
                or member == None  # noqa: E711
                or not _same(member.canonical_parent, device)
                or type(member.name) is not str
                or not member.name
                or type(member.original_name) is not str
                or not member.original_name
            ):
                _fail(
                    "ParameterMismatch", "Population members require native type, parent and names"
                )
            population.append(member)

        def find(name: str) -> Any:
            found = [
                member
                for member in population
                if member.name == name or member.original_name == name
            ]
            if len(found) != 1:
                _fail(
                    "AmbiguousParameter" if found else "ObservationUnavailable",
                    "Expected one exact native parameter identity: " + name,
                )
            if found[0].original_name != name:
                _fail(
                    "ParameterMismatch",
                    "A public alias cannot substitute for the original identity",
                )
            return found[0]

        resolved = find(self.original_name)
        if not _same(parameter, resolved) or not _same(parameter.canonical_parent, device):
            _fail(
                "ParameterMismatch",
                "Captured target differs from its actual native population member",
            )
        if not callable(parameter.str_for_value):
            _fail("ObservationUnavailable", "The native str_for_value method is unavailable")
        descriptor = _descriptor(parameter, quantized=False, eligible=True)
        modes: dict[str, Any] = {}
        for name, expected in self.policy.items():
            if self.device_class == "Drift" and name in ("voice_mode", "voice_count"):
                index = getattr(device, name + "_index")
                raw_items = getattr(device, name + "_list")
                if isinstance(raw_items, (str, bytes, dict)):
                    _fail("InvalidObservation", "Native voice property needs a label collection")
                items: list[str] = []
                for item in raw_items:
                    if len(items) >= 64:
                        _fail("InvalidDomain", "Native voice property exceeds the finite limit")
                    items.append(item)
                if (
                    isinstance(index, bool)
                    or not isinstance(index, int)
                    or not items
                    or not 0 <= index < len(items)
                    or any(type(item) is not str or not item for item in items)
                    or len(set(items)) != len(items)
                ):
                    _fail("InvalidDomain", "Native voice index/list must be valid and unambiguous")
                modes[name] = {"index": int(index), "value_items": items, "label": items[index]}
            elif self.device_class == "Drift" and name == "LP Type":
                mode = _descriptor(find(name), quantized=True, eligible=True)
                if len(mode["value_items"]) != 2:
                    _fail("UnsupportedMode", "Drift needs the observed two-choice LP Type domain")
                modes[name] = mode
            elif name == "global_mode":
                actual = device.global_mode
                if isinstance(actual, bool) or not isinstance(actual, int) or actual != expected:
                    _fail("UnsupportedMode", "EQ Eight requires observed native global_mode=0")
                modes[name] = int(actual)
            else:
                mode = _descriptor(find(name), quantized=True, eligible=True)
                if name != "Channel Mode" and set(mode["value_items"]) != {"Off", "On"}:
                    _fail(
                        "UnsupportedMode",
                        "The required activator must advertise exactly Off and On",
                    )
                if mode["label"] != expected:
                    _fail("UnsupportedMode", "Required native mode is not observed: " + name)
                modes[name] = mode
        if self.original_name == "Stereo Width":
            alternatives = [
                member
                for member in population
                if member.name == "Mid/Side Balance" or member.original_name == "Mid/Side Balance"
            ]
            if alternatives:
                alternative = find("Mid/Side Balance")
                if type(alternative.is_enabled) is not bool:
                    _fail("InvalidObservation", "Mid/Side enablement must be a native Boolean")
                state = _state(alternative.state, "Mid/Side parameter state")
                if alternative.is_enabled and state == 0:
                    _fail(
                        "UnsupportedMode",
                        "An active Mid/Side substitute is outside Width semantics",
                    )
                modes["Mid/Side Balance"] = {"is_enabled": alternative.is_enabled, "state": state}
        if self.device_class == "Eq8":
            # These are observations, not caller-selectable policy. A knob value
            # is not an assertion about the effective scaled/adaptive response.
            modes["Scale"] = _descriptor(find("Scale"), quantized=False, eligible=False)
            adaptive = _descriptor(find("Adaptive Q"), quantized=True, eligible=False)
            if set(adaptive["value_items"]) != {"Off", "On"}:
                _fail("UnsupportedMode", "Adaptive Q requires observed Off/On labels")
            modes["Adaptive Q"] = adaptive
        return {
            "objects": population,
            "population": [
                {"name": member.name, "original_name": member.original_name}
                for member in population
            ],
            "parameter_index": next(
                index for index, member in enumerate(population) if _same(member, parameter)
            ),
            "descriptor": descriptor,
            "modes": modes,
        }

    def check(self) -> None:
        try:
            current = self.capture()
        except NativeUnitError as error:
            _fail("ObservationDrift", "Native descriptor/mode identity changed: " + str(error))
        initial_objects, current_objects = self.initial["objects"], current["objects"]
        if (
            len(initial_objects) != len(current_objects)
            or any(
                not _same(first, second) for first, second in zip(initial_objects, current_objects)
            )
            or any(
                self.initial[key] != current[key]
                for key in ("population", "parameter_index", "descriptor", "modes")
            )
        ):
            _fail(
                "ObservationDrift",
                "Native population, value, descriptor or mode changed during probing",
            )

    def named_parameter(self, name: str) -> Any:
        return next(member for member in self.initial["objects"] if member.original_name == name)


def _parse(text: Any, unit: str, *, gain_infinity: bool = False) -> tuple[Decimal, Any]:
    if type(text) is not str or not text or len(text) > 80:
        _fail("UnsupportedDisplay", "Native display must be a bounded nonempty string")
    value = text.strip(" \t")
    if gain_infinity and value in ("-inf dB", "-∞ dB", "−inf dB", "−∞ dB"):
        return Decimal("-Infinity"), None
    if unit == "StereoBalance" and value == "C":
        return Decimal(0), None
    suffixes = {
        "Decibels": r"[ \t]*dB",
        "Hertz": r"[ \t]*(Hz|kHz)",
        "Milliseconds": r"[ \t]*(ms|s)",
        "QualityFactor": "",
        "Percent": r"[ \t]*%",
        "StereoBalance": r"[ \t]*([LR])",
    }
    match = re.fullmatch(_NUMBER + suffixes[unit], value)
    if not match:
        _fail(
            "UnsupportedDisplay",
            "Native display unit/number/locale is outside the admitted grammar",
        )
    token = match.group(1)
    numeric = Decimal(token)
    exponent = numeric.as_tuple().exponent
    if not isinstance(exponent, int):
        _fail("UnsupportedDisplay", "Display number requires a finite decimal exponent")
    increment = Decimal(1).scaleb(exponent)
    if unit == "Hertz" and match.group(2) == "kHz":
        numeric *= 1000
        increment *= 1000
    if unit == "Milliseconds" and match.group(2) == "s":
        numeric *= 1000
        increment *= 1000
    if unit == "StereoBalance":
        if token.startswith(("-", "+")) or numeric <= 0:
            _fail("UnsupportedDisplay", "Balance requires unsigned positive L/R amounts or C")
        if match.group(2) == "L":
            numeric = -numeric
    return numeric, increment


def _search_parameter_display(
    parameter: Any,
    *,
    observation: Any,
    unit: str,
    target: float,
    tolerance: float,
    gain_infinity: bool = False,
    supplemental_observation: Any = None,
) -> dict[str, Any]:
    """Sole bounded search; adapters own actual-object context and unit policy."""
    calls = 0
    try:
        if unit not in (
            "Decibels",
            "Hertz",
            "QualityFactor",
            "Percent",
            "Milliseconds",
            "StereoBalance",
        ):
            _fail("InvalidIntent", "Unit is outside the finite display grammar")
        desired = _number(target, "Display target")
        admitted_tolerance = _number(tolerance, "Display tolerance")
        if (
            admitted_tolerance < 0
            or (unit in ("Hertz", "QualityFactor") and desired <= 0)
            or (unit in ("Percent", "Milliseconds") and desired < 0)
            or (unit == "StereoBalance" and not -1 <= desired <= 1)
        ):
            _fail("InvalidIntent", "Physical target/tolerance violates its declared unit domain")
        domain = observation.initial["descriptor"]
        samples = []
        readings: list[tuple[float, str, Decimal, Any]] = []
        supplemental = None

        def format_value(native_parameter: Any, value: float) -> str:
            nonlocal calls
            if calls >= FORMATTER_BUDGET:
                _fail("SearchExhausted", "The native formatter budget is exhausted")
            observation.check()
            calls += 1
            try:
                text: str = native_parameter.str_for_value(value)
            except Exception as error:
                _fail("ObservationUnavailable", "Native str_for_value failed: " + str(error))
            observation.check()
            return text

        with localcontext() as decimal_context:
            decimal_context.prec = 110
            decimal_context.rounding = ROUND_HALF_EVEN
            desired_decimal = Decimal(str(desired))
            tolerance_decimal = Decimal(str(admitted_tolerance))
            if supplemental_observation is not None:
                supplemental = supplemental_observation(format_value)

            def sample(value: float, phase: str) -> tuple[float, str, Decimal, Any]:
                if not math.isfinite(value) or not domain["minimum"] <= value <= domain["maximum"]:
                    _fail("InvalidDomain", "A search value escaped the observed native bounds")
                raw = format_value(parameter, value)
                physical, increment = _parse(
                    raw,
                    unit,
                    gain_infinity=gain_infinity,
                )
                if unit in ("Hertz", "QualityFactor") and physical <= 0:
                    _fail("UnsupportedDisplay", "Native display violates its positive unit domain")
                if unit in ("Percent", "Milliseconds") and physical < 0:
                    _fail("UnsupportedDisplay", "Width/time display must be nonnegative")
                reading = (value, raw, physical, increment)
                for previous in readings:
                    if value == previous[0] and reading[1:] != previous[1:]:
                        _fail(
                            "DisplayDrift",
                            "Repeated native display changed for the same internal value",
                        )
                readings.append(reading)
                ordered = sorted(readings, key=lambda entry: entry[0])
                if any(first[2] > second[2] for first, second in zip(ordered, ordered[1:])):
                    _fail("NonmonotonicDisplay", "Observed native display must be nondecreasing")
                samples.append({"internal_value": value, "display": raw, "phase": phase})
                return reading

            minimum, maximum = domain["minimum"], domain["maximum"]
            grid = []
            for index in range(17):
                fraction = index / 16.0
                # Convex arithmetic avoids overflow in (maximum - minimum).
                value = (
                    minimum
                    if index == 0
                    else maximum
                    if index == 16
                    else (minimum * (1.0 - fraction) + maximum * fraction)
                )
                grid.append(sample(value, "grid"))
            full_scale = None
            if unit == "StereoBalance":
                if grid[0][2] >= 0 or grid[-1][2] <= 0 or -grid[0][2] != grid[-1][2]:
                    _fail(
                        "UnsupportedDisplay",
                        "Balance requires symmetric observed L/R endpoint magnitudes",
                    )
                full_scale = grid[-1][2]
            if grid[0][2] == grid[-1][2]:
                _fail("UnresolvedDisplay", "The sampled display has no distinct endpoints")

            def normalized(reading: tuple[float, str, Decimal, Any]) -> Decimal:
                return reading[2] / full_scale if full_scale is not None else reading[2]

            if not normalized(grid[0]) <= desired_decimal <= normalized(grid[-1]):
                _fail(
                    "ValueOutsideDomain",
                    "Target is outside the observed physical display endpoints",
                )
            finite = [reading for reading in grid if reading[2].is_finite()]
            candidate = min(
                finite, key=lambda entry: (abs(normalized(entry) - desired_decimal), entry[0])
            )
            bracket = next(
                (
                    (left, right)
                    for left, right in zip(grid, grid[1:])
                    if normalized(left) <= desired_decimal <= normalized(right)
                ),
                None,
            )
            if bracket is None:
                _fail(
                    "UnresolvedDisplay", "No observed interval brackets the requested display value"
                )
            left, right = bracket
            # Reserve three calls for exact anchor repeats. Never enlarge tolerance.
            while (
                abs(normalized(candidate) - desired_decimal) > tolerance_decimal
                and calls < FORMATTER_BUDGET - 3
            ):
                midpoint = left[0] * 0.5 + right[0] * 0.5
                if midpoint == left[0] or midpoint == right[0]:
                    break
                current = sample(midpoint, "search")
                if current[2].is_finite() and abs(normalized(current) - desired_decimal) < abs(
                    normalized(candidate) - desired_decimal
                ):
                    candidate = current
                if normalized(current) < desired_decimal:
                    left = current
                else:
                    right = current
            if abs(normalized(candidate) - desired_decimal) > tolerance_decimal:
                _fail(
                    "ToleranceNotMet",
                    "No candidate met the explicit display tolerance within the bounded search",
                )
            for anchor in (grid[0], grid[-1], candidate):
                repeated = sample(anchor[0], "repeat")
                if repeated != anchor:
                    _fail("DisplayDrift", "A native display anchor changed during probing")
            observation.check()
            increment = candidate[3]
            if increment is not None and full_scale is not None:
                increment /= full_scale
            for encoded, reading in zip(samples, readings):
                physical = normalized(reading)
                encoded["display_value"] = float(physical) if physical.is_finite() else None
                encoded["negative_infinity"] = not physical.is_finite()
            return {
                "schema_version": 1,
                "unit": unit,
                "target": desired,
                "display_tolerance": admitted_tolerance,
                "internal_value": candidate[0],
                "display": candidate[1],
                "display_value": float(normalized(candidate)),
                "display_increment": float(increment) if increment is not None else None,
                "absolute_display_error": float(abs(normalized(candidate) - desired_decimal)),
                "balance_full_scale": float(full_scale) if full_scale is not None else None,
                "descriptor": dict(domain),
                "formatter_calls": calls,
                "samples": samples,
                "_supplemental_observation": supplemental,
            }
    except NativeUnitError as error:
        error.formatter_calls = calls
        raise
    except Exception as error:
        raise NativeUnitError(
            "ObservationUnavailable", "Native observation failed: " + str(error), calls
        ) from error


def resolve_native_display_value(
    parameter: Any,
    *,
    device: Any,
    device_class_name: str,
    parameter_original_name: str,
    unit: str,
    target: float,
    tolerance: float,
    expected_modes: dict[str, Any],
) -> dict[str, Any]:
    """Observe one finite native Device control; no setters or mapping formula.

    Expected modes must equal the registered policy. The shared search compares
    actual displayed values with the explicit tolerance; subsequent authorized
    write/readback and host qualification remain separate.
    """
    try:
        if any(
            type(value) is not str for value in (device_class_name, parameter_original_name, unit)
        ):
            _fail("InvalidIntent", "Registered class, original name and unit must be strings")
        policy = _policy(device_class_name, parameter_original_name, unit)
        if (
            type(expected_modes) is not dict
            or set(expected_modes) != set(policy)
            or any(
                type(expected_modes[key]) is not type(policy[key])
                or expected_modes[key] != policy[key]
                for key in policy
            )
        ):
            _fail(
                "InvalidIntent",
                "Expected modes must equal the registered policy; gates cannot be omitted",
            )
        observation = _Observation(
            parameter, device, device_class_name, parameter_original_name, policy
        )

        def observe_scale(format_value: Any) -> dict[str, Any]:
            scale = observation.named_parameter("Scale")
            raw = format_value(scale, observation.initial["modes"]["Scale"]["value"])
            amount, increment = _parse(raw, "Percent")
            return {
                "display": raw,
                "display_value": float(amount),
                "display_increment": float(increment),
            }

        result = _search_parameter_display(
            parameter,
            observation=observation,
            unit=unit,
            target=target,
            tolerance=tolerance,
            gain_infinity=device_class_name == "StereoGain" and parameter_original_name == "Gain",
            supplemental_observation=observe_scale if device_class_name == "Eq8" else None,
        )
        scale_display = result.pop("_supplemental_observation")
        result.update(
            device_class_name=device_class_name,
            parameter_original_name=parameter_original_name,
            parameter_index=observation.initial["parameter_index"],
            modes=observation.initial["modes"],
            eq8_scale_display=scale_display,
            population=observation.initial["population"],
            qualification="ObservedNativeDisplayCandidate",
            source_commit=SOURCE_COMMIT,
            host_qualified=False,
            native_knob_only=True,
            coverage_limits=[
                "Tolerance compares displayed numbers; no hidden physical rounding-error bound is inferred.",
                "Sampled monotonicity does not prove a global transfer function or search completeness.",
                "Balance is a native displayed coordinate, not an arbitrary pan law or physical angle.",
                "EQ Eight Scale and Adaptive Q are observed couplings, not a literal DSP response claim.",
                "Drift voice and LP Type modes do not qualify envelope shape, routing or modulation.",
                "Native readback after an authorized write and reopen must be independently verified.",
                "Version, edition, operating system, envelope and persistence qualification remain separate.",
            ],
        )
        return result
    except NativeUnitError:
        raise
    except Exception as error:
        raise NativeUnitError(
            "ObservationUnavailable", "Native observation failed: " + str(error)
        ) from error
