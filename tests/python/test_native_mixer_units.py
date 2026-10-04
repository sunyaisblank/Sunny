"""Literal genuine MixerDevice/parameter oracles; no actual Live curve claim."""

from __future__ import annotations

from types import SimpleNamespace
from typing import Any

import pytest
import test_managed_devices as provider
from live_model import Device, DeviceType, LiveSet
from Sunny.native_mixer_units import resolve_native_mixer_display_value
from Sunny.native_units import NativeUnitError


class Volume(provider.Parameter):
    """Native volume whose write path is forbidden during display resolution."""

    def _write(self, value: float) -> None:
        raise AssertionError("Read-only native mixer resolver attempted a setter")


@pytest.fixture
def native(monkeypatch: pytest.MonkeyPatch) -> SimpleNamespace:
    """Install actual native-shaped Track/Mixer classes; dB curve is a literal oracle."""
    live = LiveSet(midi_tracks=1, return_tracks=1).install(monkeypatch)
    track = live.song.tracks[0]
    mixer = track.mixer_device
    parameter = Volume("Track Volume", lambda v: f"{-40 + 272 * v**3:.2f} dB", value=0.375)
    parameter._canonical_parent = mixer
    mixer._volume = parameter
    return SimpleNamespace(live=live, track=track, mixer=mixer, parameter=parameter)


def resolve(native: Any, target: float = -6.0, tolerance: float = 0.0) -> dict[str, Any]:
    """Use only internally fixed genuine volume role and explicit physical tolerance."""
    return resolve_native_mixer_display_value(
        native.track,
        mixer=native.mixer,
        parameter_kind="volume",
        target=target,
        tolerance=tolerance,
    )


def test_actual_volume_nonlinear_formatter_candidate_is_read_only(native: Any) -> None:
    """Independent -40+272*x^3 dB puts -6dB at0.5, unrelated to initial0.375."""
    candidate = resolve(native)
    assert candidate["internal_value"] == 0.5
    assert candidate["display"] == "-6.00 dB"
    assert candidate["display_value"] == -6.0
    assert candidate["current_display"]["display"] == "-25.66 dB"
    assert candidate["current_display"]["internal_value"] == 0.375
    assert candidate["display_increment"] == 0.01
    assert candidate["formatter_calls"] == 21 == len(native.parameter.calls)
    assert native.parameter.value == 0.375
    assert native.parameter.writes == []
    assert candidate["parameter_kind"] == "volume"
    assert candidate["mixer_capture"]["parameters"][0]["kind"] == "volume"
    assert candidate["host_qualified"] is False and candidate["native_knob_only"] is True
    assert "device_class_name" not in candidate and "population" not in candidate


def test_volume_nonstandard_native_bounds_have_no_normalized_fader_assumption(native: Any) -> None:
    """An independently affine native domain[-2,6] puts the same physical -6dB at2."""
    native.parameter._min, native.parameter._max = -2.0, 6.0
    native.parameter._value, native.parameter._default = 1.0, 0.0
    native.parameter.oracle = lambda v: f"{-40 + 272 * ((v + 2) / 8) ** 3:.2f} dB"
    assert resolve(native)["internal_value"] == 2.0
    assert native.parameter.value == 1.0


def test_finite_volume_search_observes_infinity_endpoint_without_choosing_silence(
    native: Any,
) -> None:
    """Native minimum can mean silence; a finite requested -6dB still chooses0.5."""
    native.parameter.oracle = lambda v: "-∞ dB" if v == 0 else f"{-40 + 272 * v**3:.2f} dB"
    candidate = resolve(native)
    assert candidate["internal_value"] == 0.5
    assert candidate["samples"][0]["negative_infinity"] is True
    assert candidate["samples"][0]["display_value"] is None
    assert candidate["display_value"] == -6.0


def test_volume_display_plateau_has_exact_tolerance_and_nominal_precision(native: Any) -> None:
    """Literal0.1dB glyphs resolve -2.1 but never manufacture unreachable -2.06."""
    native.parameter.oracle = lambda v: f"{-10 + 16 * v:.1f} dB"
    candidate = resolve(native, -2.1)
    assert candidate["display_value"] == -2.1 and candidate["display_increment"] == 0.1
    native.parameter.calls.clear()
    with pytest.raises(NativeUnitError) as caught:
        resolve(native, -2.06)
    assert caught.value.reason == "ToleranceNotMet"
    assert caught.value.formatter_calls == len(native.parameter.calls) <= 64
    assert native.parameter.value == 0.375


@pytest.mark.parametrize(
    "oracle,reason",
    [
        (lambda _: "-6,00 dB", "UnsupportedDisplay"),
        (lambda _: "-6.00 Hz", "UnsupportedDisplay"),
        (lambda v: f"{6 - 12 * v:.2f} dB", "NonmonotonicDisplay"),
    ],
)
def test_volume_locale_unit_and_decreasing_display_decline(
    native: Any, oracle: Any, reason: str
) -> None:
    """Actual native formatting must satisfy the same single unit grammar."""
    native.parameter.oracle = oracle
    with pytest.raises(NativeUnitError) as caught:
        resolve(native)
    assert caught.value.reason == reason
    assert caught.value.formatter_calls <= 64


def test_volume_target_above_actual_display_maximum_never_clamps(native: Any) -> None:
    """Authored+7 cannot become native+6; endpoint observation reports unavailable."""
    native.parameter.oracle = lambda v: f"{-100 + 106 * v:.2f} dB"
    with pytest.raises(NativeUnitError) as caught:
        resolve(native, 7.0)
    assert caught.value.reason == "ValueOutsideDomain"
    assert caught.value.formatter_calls == 18
    assert native.parameter.value == 0.375


@pytest.mark.parametrize(
    "fault", ("parent", "quantized", "disabled", "state1", "state2", "automation")
)
def test_volume_native_domain_admission_precedes_formatter(native: Any, fault: str) -> None:
    """Wrong role/eligibility/automation cannot be used for a physical target."""
    if fault == "parent":
        native.parameter._canonical_parent = native.track
    elif fault == "quantized":
        native.parameter._is_quantized = True
        native.parameter._value_items = ("Off", "On")
    elif fault == "disabled":
        native.parameter._is_enabled = False
    elif fault.startswith("state"):
        native.parameter._state = int(fault[-1])
    else:
        native.parameter._automation_state = 1
    with pytest.raises(NativeUnitError):
        resolve(native)
    assert native.parameter.calls == []


@pytest.mark.parametrize("fault", ("value", "volume", "mixer", "send", "mode", "device", "track"))
def test_volume_search_rechecks_actual_parent_handle_and_untouched_context(
    native: Any, fault: str
) -> None:
    """Identical replacements and untouched native drift fail within one formatter call."""

    def change(_: float) -> None:
        if fault == "value":
            native.parameter._value = 0.4
        elif fault == "volume":
            new = Volume("Track Volume", native.parameter.oracle, value=0.375)
            new._canonical_parent = native.mixer
            native.mixer._volume = new
        elif fault == "mixer":
            from live_model import MixerDevice

            new = MixerDevice()
            new._canonical_parent = native.track
            native.track._mixer = new
        elif fault == "send":
            send = provider.Parameter("Send A", lambda _: "-10.0 dB")
            send._canonical_parent = native.mixer
            native.mixer._sends = (send,)
        elif fault == "mode":
            native.mixer._panning_mode = 1
        elif fault == "device":
            device = Device("Unknown preserved effect", "Unknown", DeviceType.audio_effect)
            device._canonical_parent = native.track
            native.track._devices.append(device)
        else:
            native.track._mute = True

    native.parameter.callback = change
    with pytest.raises(NativeUnitError) as caught:
        resolve(native)
    assert caught.value.reason == "ObservationDrift"
    assert caught.value.formatter_calls == 1
    assert native.parameter.writes == []


def test_volume_repeat_anchor_drift_and_native_exceptions_preserve_current_value(
    native: Any,
) -> None:
    """A late display change is observed rather than promoted into a candidate."""
    native.parameter.oracle = (
        lambda v: f"{-40 + 272 * v**3 + (1 if len(native.parameter.calls) > 18 else 0):.2f} dB"
    )
    with pytest.raises(NativeUnitError) as caught:
        resolve(native)
    assert caught.value.reason == "DisplayDrift"
    assert caught.value.formatter_calls == 19
    native.parameter.callback = lambda _: (_ for _ in ()).throw(
        RuntimeError("native formatter fails")
    )
    with pytest.raises(NativeUnitError) as caught:
        resolve(native)
    assert caught.value.reason == "ObservationUnavailable" and caught.value.formatter_calls == 1
    assert native.parameter.value == 0.375


def test_volume_role_and_finite_intent_are_closed(native: Any) -> None:
    """Unsupported mixer roles and nonfinite intent never acquire a candidate."""
    for kind in ("send", "panning", "Device On", None):
        with pytest.raises(NativeUnitError) as caught:
            resolve_native_mixer_display_value(
                native.track, mixer=native.mixer, parameter_kind=kind, target=-6.0, tolerance=0.0
            )
        assert caught.value.reason == "UnknownCapability"
    for value in (float("inf"), float("nan"), True):
        with pytest.raises(NativeUnitError):
            resolve(native, value)
    assert native.parameter.calls == []


def test_retained_binding_preview_preserves_baseline_and_grants_no_mixer_authority(
    monkeypatch: pytest.MonkeyPatch,
) -> None:
    """Current formatter evidence joins actual Registry content without granting setters."""
    from Sunny.native_mixer_preview import preview_retained_mixer_volume

    target = provider.target.__wrapped__(monkeypatch)
    mixer = target.record["track"].mixer_device
    parameter = Volume("Track Volume", lambda v: f"{-40 + 272 * v**3:.2f} dB", value=0.85)
    parameter._canonical_parent = mixer
    mixer._volume = parameter
    # Fixture replaces a concrete original Mixer control; explicitly revoke the old grant.
    if hasattr(target.registry, "_mixer"):
        target.registry._mixer.invalidate_retained_cohort(target.record)
    baseline = target.record["content_fingerprint"]
    before = target.registry._capture(target.record)
    observed = preview_retained_mixer_volume(
        target.registry,
        target.record,
        expected_content_fingerprint=baseline,
        target=-6.0,
        tolerance=0.0,
    )
    assert observed["candidate"]["current_display"]["display"] == "127.04 dB"
    assert observed["candidate"]["internal_value"] == 0.5
    assert observed["static_mixer_authority_granted"] is False
    assert observed["native_mutation_started"] is False
    assert target.record["content_fingerprint"] == baseline
    assert target.registry._capture(target.record) == before
    assert parameter.writes == []
    parameter._value = 0.4
    parameter.calls.clear()
    with pytest.raises(RuntimeError, match="drift"):
        preview_retained_mixer_volume(
            target.registry,
            target.record,
            expected_content_fingerprint=baseline,
            target=-6.0,
            tolerance=0.0,
        )
    assert parameter.calls == []
