"""Actual MixerDevice volume display candidates; read-only, source-candidate proof.

The owning main-thread operation supplies actual retained objects and separately
checks the observed host version, Clip authority, automation/envelopes and frame
capacity. This helper grants no writes and captures no fictitious Device.
"""

from __future__ import annotations

from typing import Any

from .native_units import (
    SOURCE_COMMIT,
    NativeUnitError,
    _descriptor,
    _fail,
    _parse,
    _same,
    _search_parameter_display,
)

MAX_PARAMETERS = 512
MAX_DEVICES = 16
TRACK_FLAGS = (
    "mute",
    "solo",
    "arm",
    "implicit_arm",
    "is_frozen",
    "is_grouped",
    "back_to_arranger",
    "has_audio_input",
    "has_midi_input",
    "has_audio_output",
    "has_midi_output",
)


class _MixerObservation:
    def __init__(
        self, track: Any, mixer: Any, *, eligible_volume: bool = True, context_check: Any = None
    ) -> None:
        self.eligible_volume = eligible_volume
        self.context_check = context_check
        self.track = track
        self.mixer = mixer
        self.parameter = mixer.volume
        self.initial = self.capture()

    def capture(self) -> dict[str, Any]:
        import Live

        from .handler import LomHandler

        track, mixer = self.track, self.mixer
        if (
            not isinstance(track, Live.Track.Track)
            or track == None  # noqa: E711
            or not isinstance(mixer, Live.MixerDevice.MixerDevice)
            or mixer == None  # noqa: E711
            or not _same(track.mixer_device, mixer)
            or not _same(mixer.canonical_parent, track)
            or not _same(mixer.volume, self.parameter)
        ):
            _fail(
                "MixerMismatch",
                "Actual Track/MixerDevice/volume membership and parents are required",
            )
        flags = {name: getattr(track, name) for name in TRACK_FLAGS}
        if any(type(value) is not bool for value in flags.values()):
            _fail("InvalidObservation", "Track context requires genuine Boolean observations")
        modes = {name: getattr(mixer, name) for name in ("panning_mode", "crossfade_assign")}
        for name, allowed in (("panning_mode", (0, 1)), ("crossfade_assign", (0, 1, 2))):
            value = modes[name]
            if isinstance(value, bool) or not isinstance(value, int) or value not in allowed:
                _fail(
                    "InvalidObservation", "Mixer mode is outside its observed native integer domain"
                )
            modes[name] = int(value)
        members: list[tuple[str, int | None, Any]] = [
            (name, None, getattr(mixer, name)) for name in ("volume", "panning", "track_activator")
        ]
        raw = mixer.sends
        if isinstance(raw, (str, bytes, dict)):
            _fail("InvalidObservation", "Mixer sends require an actual native collection")
        for index, parameter in enumerate(raw):
            if len(members) >= MAX_PARAMETERS:
                _fail(
                    "InvalidObservation", "Mixer population exceeds the finite512-parameter domain"
                )
            members.append(("send", index, parameter))
        population = []
        objects: list[Any] = []
        for role, send_index, parameter in members:
            if (
                not isinstance(parameter, Live.DeviceParameter.DeviceParameter)
                or parameter == None  # noqa: E711
                or not _same(parameter.canonical_parent, mixer)
                or any(
                    type(getattr(parameter, name)) is not str
                    or not 0 < len(getattr(parameter, name).encode("utf-8")) <= 4096
                    for name in ("name", "original_name")
                )
                or any(_same(parameter, previous) for previous in objects)
            ):
                _fail(
                    "ParameterMismatch",
                    "Unique actual mixer parameters require native type/parent/names",
                )
            objects.append(parameter)
            quantized = parameter.is_quantized
            if type(quantized) is not bool:
                _fail("InvalidObservation", "Mixer parameter quantization must be Boolean")
            descriptor = _descriptor(
                parameter, quantized=quantized, eligible=role == "volume" and self.eligible_volume
            )
            if role == "volume" and quantized:
                _fail("InvalidDomain", "Native mixer volume must be continuous")
            population.append(
                {
                    "kind": role,
                    "send_index": send_index,
                    "name": parameter.name,
                    "original_name": parameter.original_name,
                    "descriptor": descriptor,
                }
            )
        if self.eligible_volume and not callable(getattr(self.parameter, "str_for_value", None)):
            _fail("ObservationUnavailable", "Actual native mixer volume formatter is unavailable")
        devices = LomHandler._device_chain(track)
        if len(devices) > MAX_DEVICES or any(
            not isinstance(device, Live.Device.Device)
            or device == None  # noqa: E711
            or not _same(device.canonical_parent, track)
            for device in devices
        ):
            _fail("MixerMismatch", "Actual finite Track device cohort/type/parents changed")
        return {
            "objects": tuple(objects),
            "devices": devices,
            "descriptor": population[0]["descriptor"],
            "mixer_capture": {**modes, "parameters": population},
            "track_context": flags,
        }

    def check(self) -> None:
        if self.context_check is not None:
            self.context_check()
        try:
            current = self.capture()
        except NativeUnitError as error:
            _fail("ObservationDrift", "Native mixer context changed: " + str(error))
        for name in ("objects", "devices"):
            if len(current[name]) != len(self.initial[name]) or any(
                not _same(first, second) for first, second in zip(current[name], self.initial[name])
            ):
                _fail("ObservationDrift", "Native mixer/Device/parameter handle cohort changed")
        if any(
            current[name] != self.initial[name]
            for name in ("descriptor", "mixer_capture", "track_context")
        ):
            _fail("ObservationDrift", "Native mixer values/descriptors/modes/Track context changed")


def resolve_native_mixer_display_value(
    track: Any,
    *,
    mixer: Any,
    parameter_kind: str,
    target: float,
    tolerance: float,
    context_check: Any = None,
) -> dict[str, Any]:
    """Resolve finite volume dB on actual objects, with no setters or ownership grant."""
    try:
        if parameter_kind != "volume" or type(parameter_kind) is not str:
            _fail("UnknownCapability", "The finite native Mixer role is volume only")
        observation = _MixerObservation(track, mixer, context_check=context_check)

        def current_display(format_value: Any) -> dict[str, Any]:
            value = observation.initial["descriptor"]["value"]
            raw = format_value(observation.parameter, value)
            amount, increment = _parse(raw, "Decibels", gain_infinity=True)
            return {
                "internal_value": value,
                "display": raw,
                "display_value": float(amount) if amount.is_finite() else None,
                "display_increment": float(increment) if increment is not None else None,
                "negative_infinity": not amount.is_finite(),
            }

        result: dict[str, Any] = _search_parameter_display(
            observation.parameter,
            observation=observation,
            unit="Decibels",
            target=target,
            tolerance=tolerance,
            gain_infinity=True,
            supplemental_observation=current_display,
        )
        result["current_display"] = result.pop("_supplemental_observation")
        result.update(
            parameter_kind="volume",
            parameter_name=observation.parameter.name,
            parameter_original_name=observation.parameter.original_name,
            mixer_capture=observation.initial["mixer_capture"],
            track_context=observation.initial["track_context"],
            device_cohort_count=len(observation.initial["devices"]),
            qualification="ObservedNativeMixerDisplayCandidate",
            source_commit=SOURCE_COMMIT,
            host_qualified=False,
            native_knob_only=True,
            coverage_limits=[
                "Tolerance compares displayed dB; no hidden rounding-error bound is inferred.",
                "Sampled monotonicity does not prove a complete fader transfer function.",
                "A finite volume candidate does not grant write or automation/envelope authority.",
                "Device opaque state and known owned parameter guards remain separately retained.",
                "Native formatted readback after any authorized write must be independently verified.",
                "Version, edition, operating system, loudness and persistence qualification remain separate.",
            ],
        )
        return result
    except NativeUnitError:
        raise
    except Exception as error:
        raise NativeUnitError(
            "ObservationUnavailable", "Native mixer observation failed: " + str(error)
        ) from error
