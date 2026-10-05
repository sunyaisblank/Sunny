"""Offline model of the Live Python API surface that Sunny's Remote Script touches.

A Remote Script runs against Live's embedded Python API, not the Max-side LOM
dictionaries. This module reproduces the Python-side types and documented
semantics for the objects, properties and functions the handler uses. Source
observations and the public Max LOM guide the modeled contract; an offline
model cannot establish the behavior of every embedded Python host version.

Sources:
    * The Live 12 Remote Scripts source mirror ``gluon/AbletonLive12_MIDIRemoteScripts``
      at ``e83d5192f321b24eb9daab843ac49a2d95d862b1``, ``_MxDCore/MxDCore.py``:
      the Max translation layer builds ``Live.Clip.MidiNoteSpecification``;
      note readback returns ``MidiNote`` objects and ranged queries use keyword arguments;
      enum properties are ``int`` subclasses set with plain ``int`` values; Boolean
      properties accept ``int`` 0/1.
    * ``_MxDCore/LomTypes.py``: routing values are ``RoutingType``/``RoutingChannel``
      objects exposing ``display_name`` and selected by equality from ``available_*``
      tuples; ``clip_slots``, ``devices``, ``tracks`` and similar are tuple types.
    * Pinned ``pushbase/automation_component.py``: Python ``automation_envelope`` /
      ``create_automation_envelope`` take actual DeviceParameter objects;
      ``insert_step(start, positive_duration, internal_value)`` and
      ``value_at_time(time)`` are the observed author/read subset. Its fixture
      requires explicit opt-in and does not imply a general version floor.
      ``pushbase/actions.py`` observes Session Clip → ClipSlot → Track parents;
      ``ableton/v3/live/util.py`` observes mixer parameter owner parents.
    * Live 11.0 Python API dump (nsuspray.github.io/Live_API_Doc/11.0.0.xml):
      ``Clip.add_new_notes`` "expects a Python iterable holding a number of
      Live.Clip.MidiNoteSpecification objects"; ``Vector``/``StringVector``/``IntVector``
      are read-only host containers distinct from ``list`` and ``tuple``.
    * Cycling '74 LOM reference: enum value domains (``Device.type``,
      ``DeviceParameter.state``/``automation_state``, ``ClipSlot.playing_status``,
      ``Clip.launch_mode``/``launch_quantization``, ``MixerDevice.crossfade_assign``
      0=A 1=none 2=B, ``panning_mode``), ``create_clip`` only on empty slots,
      ``Song.tempo`` 20..999, clip ``end_time`` semantics.
    * AbletonOSC (ideoforms/AbletonOSC) as a cross-check: routing is selected by
      assigning an object taken from ``available_*_routing_*``.

Where public documentation is silent the model stays conservative and says so,
referencing issue #22 (Live API assumptions settled only against a running Live).

Malformed host values are injected with :func:`inject`, which overrides one
property read on one object without disturbing the model's own semantics.
"""

# The model mirrors Live's API names; Ableton's documentation defines each
# member, so per-member docstrings would only restate the name.
# ruff: noqa: D102, D105, D107

from __future__ import annotations

import enum
import math
import sys
import types
from collections.abc import Callable, Iterable, Iterator
from typing import Any

# =============================================================================
# Injection of malformed host values
# =============================================================================

ABSENT = object()
"""Injected value that makes a property read raise ``AttributeError``."""


class _LiveProperty:
    """A host property whose reads honour injected values and whose writes validate."""

    def __init__(
        self,
        getter: Callable[[Any], Any],
        setter: Callable[[Any, Any], None] | None = None,
    ) -> None:
        self._getter = getter
        self._setter = setter
        self._name = getter.__name__

    def __set_name__(self, owner: type, name: str) -> None:
        self._name = name

    def setter(self, setter: Callable[[Any, Any], None]) -> _LiveProperty:
        return _LiveProperty(self._getter, setter)

    def __get__(self, obj: Any, owner: type | None = None) -> Any:
        if obj is None:
            return self
        injected = obj.__dict__.get("_injected")
        if injected is not None and self._name in injected:
            value = injected[self._name]
            if value is ABSENT:
                raise AttributeError(self._name)
            return value
        return self._getter(obj)

    def __set__(self, obj: Any, value: Any) -> None:
        if self._setter is None:
            # Boost.Python raises this for read-only properties.
            raise AttributeError(f"can't set attribute '{self._name}'")
        self._setter(obj, value)


def live_property(getter: Callable[[Any], Any]) -> _LiveProperty:
    """Declare a host property, like ``property`` but injectable."""
    return _LiveProperty(getter)


def inject(obj: Any, name: str, value: Any) -> None:
    """Make ``obj.name`` read as ``value`` (or raise if ``ABSENT``) until restored."""
    if not isinstance(getattr(type(obj), name, None), _LiveProperty):
        raise TypeError(f"{type(obj).__name__}.{name} is not an injectable Live property")
    obj.__dict__.setdefault("_injected", {})[name] = value


def restore(obj: Any, name: str) -> None:
    """Remove one injected value so the model's own semantics apply again."""
    obj.__dict__.get("_injected", {}).pop(name, None)


class ArgumentError(TypeError):
    """Mirror of ``Boost.Python.ArgumentError``, raised for C++ signature mismatches."""


# =============================================================================
# Host containers and enums
# =============================================================================


class Vector:
    """``Live.Base.Vector``: a read-only host container, neither list nor tuple."""

    def __init__(self, items: Iterable[Any] = ()) -> None:
        self._items = tuple(items)

    def __len__(self) -> int:
        return len(self._items)

    def __getitem__(self, index: int) -> Any:
        return self._items[index]

    def __iter__(self) -> Iterator[Any]:
        return iter(self._items)

    def __eq__(self, other: object) -> bool:
        return type(other) is type(self) and other._items == self._items  # type: ignore[attr-defined]

    __hash__ = None  # type: ignore[assignment]


class StringVector(Vector):
    """``Live.Base.StringVector``."""


class IntVector(Vector):
    """``Live.Base.IntVector``."""


class MidiNoteVector(Vector):
    """``Live.Clip.MidiNoteVector``: the container Live returns MIDI notes in."""


class DeviceType(enum.IntEnum):
    """``Device.type`` (Cycling '74: 0 undefined, 1 instrument, 2 audio effect, 4 MIDI effect)."""

    undefined = 0
    instrument = 1
    audio_effect = 2
    midi_effect = 4


class ParameterState(enum.IntEnum):
    """``DeviceParameter.state`` (0 enabled, 1 irrelevant, 2 disabled)."""

    enabled = 0
    irrelevant = 1
    disabled = 2


class AutomationState(enum.IntEnum):
    """``DeviceParameter.automation_state`` (0 none, 1 playing, 2 overridden)."""

    none = 0
    playing = 1
    overridden = 2


class ClipSlotPlayingStatus(enum.IntEnum):
    """``ClipSlot.playing_status`` (0 stopped, 1 playing, 2 recording)."""

    stopped = 0
    playing = 1
    recording = 2


class LaunchMode(enum.IntEnum):
    """``Clip.launch_mode`` (0 trigger, 1 gate, 2 toggle, 3 repeat)."""

    trigger = 0
    gate = 1
    toggle = 2
    repeat = 3


class Quantization(enum.IntEnum):
    """``Clip.launch_quantization`` (0 global, 1 none, 2 eight bars ... 14 1/32)."""

    q_global = 0
    q_no_q = 1
    q_8_bars = 2
    q_4_bars = 3
    q_2_bars = 4
    q_bar = 5
    q_half = 6
    q_half_triplet = 7
    q_quarter = 8
    q_quarter_triplet = 9
    q_eight = 10
    q_eight_triplet = 11
    q_sixtenth = 12
    q_sixtenth_triplet = 13
    q_thirtytwoth = 14


class CrossfadeAssignment(enum.IntEnum):
    """``MixerDevice.crossfade_assign`` (0 A, 1 none, 2 B)."""

    A = 0
    NONE = 1
    B = 2


class PanningMode(enum.IntEnum):
    """``MixerDevice.panning_mode`` (0 stereo, 1 split stereo)."""

    stereo = 0
    stereo_split = 1


class RoutingTypeCategory(enum.IntEnum):
    """``RoutingType.category``.

    The member names follow Live's ``RoutingTypeCategory``; the numeric values
    are the model's own, because no public source fixes them (#22). Consumers
    may rely on the value being a stable ``int`` subclass, not on the numbers.
    """

    external = 0
    rebounce = 1
    master = 2
    track = 3
    parent_group_track = 4
    none = 5
    invalid = 6


def _enum_setter(enum_type: type[enum.IntEnum]) -> Callable[[Any], enum.IntEnum]:
    """Validate an enum assignment the way MxDCore performs it: a plain int is accepted."""

    def convert(value: Any) -> enum.IntEnum:
        if isinstance(value, bool) or not isinstance(value, int):
            raise ArgumentError(f"expected {enum_type.__name__}, got {type(value).__name__}")
        try:
            return enum_type(int(value))
        except ValueError as error:
            raise RuntimeError(f"Invalid {enum_type.__name__} value {value}") from error

    return convert


def _bool(value: Any) -> bool:
    # MxDCore assigns Boolean properties with ints 0/1, so the host accepts both.
    if isinstance(value, bool):
        return value
    if isinstance(value, int) and value in (0, 1):
        return bool(value)
    raise ArgumentError(f"expected bool, got {type(value).__name__}")


def _float(value: Any) -> float:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ArgumentError(f"expected float, got {type(value).__name__}")
    return float(value)


def _string(value: Any) -> str:
    if not isinstance(value, str):
        raise ArgumentError(f"expected str, got {type(value).__name__}")
    return value


# =============================================================================
# Notes
# =============================================================================


class MidiNoteSpecification:
    """``Live.Clip.MidiNoteSpecification``: the only element type ``add_new_notes`` takes."""

    def __init__(
        self,
        pitch: int,
        start_time: float,
        duration: float,
        velocity: float = 100.0,
        mute: bool = False,
        probability: float = 1.0,
        velocity_deviation: float = 0.0,
        release_velocity: float = 64.0,
    ) -> None:
        if isinstance(pitch, bool) or not isinstance(pitch, int):
            raise ArgumentError(f"pitch: expected int, got {type(pitch).__name__}")
        self.pitch = pitch
        self.start_time = _float(start_time)
        self.duration = _float(duration)
        self.velocity = _float(velocity)
        self.mute = _bool(mute)
        self.probability = _float(probability)
        self.velocity_deviation = _float(velocity_deviation)
        self.release_velocity = _float(release_velocity)


class MidiNote:
    """``Live.Clip.MidiNote``: a read-only note returned by the extended note API."""

    __slots__ = (
        "note_id",
        "pitch",
        "start_time",
        "duration",
        "velocity",
        "mute",
        "probability",
        "velocity_deviation",
        "release_velocity",
    )

    def __init__(self, note_id: int, specification: MidiNoteSpecification) -> None:
        self.note_id = note_id
        self.pitch = specification.pitch
        self.start_time = specification.start_time
        self.duration = specification.duration
        self.velocity = specification.velocity
        self.mute = specification.mute
        self.probability = specification.probability
        self.velocity_deviation = specification.velocity_deviation
        self.release_velocity = specification.release_velocity


# =============================================================================
# Device parameters and devices
# =============================================================================


class DeviceParameter:
    """``Live.DeviceParameter.DeviceParameter``.

    Writes to ``value`` clamp to ``[min, max]``. ``display_value`` is stored as
    written (after clamping to the display range) so that a readback of a
    written display value is exact; Live's own display curves are not public.
    """

    def __init__(
        self,
        name: str,
        *,
        minimum: float = 0.0,
        maximum: float = 1.0,
        value: float = 0.0,
        default_value: float | None = None,
        value_items: tuple[str, ...] = (),
        to_display: Callable[[float], float] | None = None,
        from_display: Callable[[float], float] | None = None,
        display: float | None = None,
        original_name: str | None = None,
    ) -> None:
        self._name = name
        self._original_name = name if original_name is None else original_name
        self._min = float(minimum)
        self._max = float(maximum)
        self._value = float(value)
        self._default = float(value if default_value is None else default_value)
        self._is_quantized = bool(value_items)
        # Live's documented container for value_items is StringVector; whether
        # non-quantized parameters return an empty vector is unsettled (#22).
        self._value_items = StringVector(value_items)
        self._to_display = to_display or (lambda v: v)
        self._from_display = from_display or (lambda d: d)
        self._display = self._to_display(self._value) if display is None else float(display)
        self._is_enabled = True
        self._state = ParameterState.enabled
        self._automation_state = AutomationState.none
        self._canonical_parent: Any = None

    @live_property
    def canonical_parent(self) -> Any:
        return self._canonical_parent

    @live_property
    def name(self) -> str:
        return self._name

    @live_property
    def original_name(self) -> str:
        return self._original_name

    @live_property
    def min(self) -> float:
        return self._min

    @live_property
    def max(self) -> float:
        return self._max

    @live_property
    def default_value(self) -> float:
        return self._default

    @live_property
    def is_quantized(self) -> bool:
        return self._is_quantized

    @live_property
    def value_items(self) -> StringVector:
        return self._value_items

    @live_property
    def is_enabled(self) -> bool:
        return self._is_enabled

    @live_property
    def state(self) -> ParameterState:
        return self._state

    @live_property
    def automation_state(self) -> AutomationState:
        return self._automation_state

    def _clamp(self, value: float) -> float:
        clamped = min(max(value, self._min), self._max)
        return float(round(clamped)) if self._is_quantized else clamped

    @live_property
    def value(self) -> float:
        return self._value

    @value.setter
    def value(self, value: Any) -> None:
        self._write(self._clamp(_float(value)))
        self._display = self._to_display(self._value)

    def _write(self, value: float) -> None:
        # A write to an automated parameter overrides its automation, as in Live.
        if self._automation_state == AutomationState.playing:
            self._automation_state = AutomationState.overridden
        self._value = value

    def start_automation_playback(self) -> None:
        """Model-only helper: give the parameter automation that is currently playing."""
        self._automation_state = AutomationState.playing

    @live_property
    def display_value(self) -> float:
        return self._display

    @display_value.setter
    def display_value(self, display: Any) -> None:
        low, high = self._to_display(self._min), self._to_display(self._max)
        clamped = min(max(_float(display), low), high)
        self._write(self._clamp(self._from_display(clamped)))
        self._display = clamped


# The model's fader and send curves. Live prints -inf at the bottom of both
# ranges; the model uses a finite floor because whether Live reports a finite
# display_value there is unsettled (#22).
_FADER_FLOOR_DB = -70.0
_UNITY_FADER_VALUE = 0.85


def _volume_to_db(value: float) -> float:
    if value >= _UNITY_FADER_VALUE:
        return (value - _UNITY_FADER_VALUE) * 40.0
    return _FADER_FLOOR_DB + value / _UNITY_FADER_VALUE * -_FADER_FLOOR_DB


def _db_to_volume(db: float) -> float:
    if db >= 0.0:
        return _UNITY_FADER_VALUE + db / 40.0
    return (db - _FADER_FLOOR_DB) / -_FADER_FLOOR_DB * _UNITY_FADER_VALUE


def _send_to_db(value: float) -> float:
    return _FADER_FLOOR_DB + value * -_FADER_FLOOR_DB


def _db_to_send(db: float) -> float:
    return (db - _FADER_FLOOR_DB) / -_FADER_FLOOR_DB


def _on_off(name: str, value: float = 1.0) -> DeviceParameter:
    return DeviceParameter(name, value=value, value_items=("Off", "On"))


def volume_parameter() -> DeviceParameter:
    """Return a track volume fader at unity gain (0 dB, value 0.85; +6 dB at maximum)."""
    return DeviceParameter(
        "Track Volume",
        value=_UNITY_FADER_VALUE,
        to_display=_volume_to_db,
        from_display=_db_to_volume,
        display=0.0,
    )


def send_parameter(name: str) -> DeviceParameter:
    """Return a send level at its minimum (0 dB at maximum)."""
    return DeviceParameter(name, to_display=_send_to_db, from_display=_db_to_send)


def panning_parameter() -> DeviceParameter:
    """Return a centred stereo pan, -1 (left) to 1 (right)."""
    return DeviceParameter(
        "Track Panning",
        minimum=-1.0,
        to_display=lambda v: v * 50.0,
        from_display=lambda d: d / 50.0,
    )


class Device:
    """``Live.Device.Device`` as inserted by ``Track.insert_device``."""

    def __init__(
        self,
        name: str,
        class_name: str,
        device_type: DeviceType,
        parameter_names: tuple[str, ...] = (),
        *,
        can_have_chains: bool = False,
    ) -> None:
        self._name = name
        self._class_name = class_name
        self._class_display_name = name
        self._type = device_type
        self._can_have_chains = can_have_chains
        # Every native Live device exposes "Device On" as parameter 0.
        self._parameters = (_on_off("Device On"),) + tuple(
            DeviceParameter(parameter_name) for parameter_name in parameter_names
        )
        self._canonical_parent: Any = None
        for parameter in self._parameters:
            parameter._canonical_parent = self

    @live_property
    def canonical_parent(self) -> Any:
        return self._canonical_parent

    @live_property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value: Any) -> None:
        self._name = _string(value)

    @live_property
    def class_name(self) -> str:
        return self._class_name

    @live_property
    def class_display_name(self) -> str:
        return self._class_display_name

    @live_property
    def type(self) -> DeviceType:
        return self._type

    @live_property
    def is_active(self) -> bool:
        return self._parameters[0].value != 0.0

    @live_property
    def can_have_chains(self) -> bool:
        return self._can_have_chains

    @live_property
    def latency_in_samples(self) -> int:
        return 0

    @live_property
    def latency_in_ms(self) -> float:
        return 0.0

    @live_property
    def parameters(self) -> tuple[DeviceParameter, ...]:
        return self._parameters


_INSTRUMENT = DeviceType.instrument
_AUDIO_EFFECT = DeviceType.audio_effect

# Native devices reachable through Track.insert_device (Live 12.3+). Class names
# are Live's internal device classes; parameters are the ones Sunny addresses.
# EQ Eight deliberately has no Dry/Wet control, as in Live.
NATIVE_DEVICES: dict[str, tuple[str, DeviceType, tuple[str, ...]]] = {
    "Analog": ("UltraAnalog", _INSTRUMENT, ("F1 Freq", "F1 Resonance", "Volume")),
    "Operator": ("Operator", _INSTRUMENT, ("Filter Freq", "Filter Res", "Volume")),
    "Wavetable": ("InstrumentVector", _INSTRUMENT, ("Filter 1 Freq", "Volume")),
    "Simpler": ("OriginalSimpler", _INSTRUMENT, ("Filter Freq", "Volume")),
    "Collision": ("Collision", _INSTRUMENT, ("Volume",)),
    "Drift": ("Drift", _INSTRUMENT, ("Filter Freq", "Volume")),
    "EQ Eight": ("Eq8", _AUDIO_EFFECT, ("1 Frequency A", "1 Gain A", "Output Gain")),
    "Compressor": ("Compressor2", _AUDIO_EFFECT, ("Threshold", "Ratio", "Dry/Wet")),
    "Gate": ("Gate", _AUDIO_EFFECT, ("Threshold",)),
    "Limiter": ("Limiter", _AUDIO_EFFECT, ("Ceiling", "Gain")),
    "Multiband Dynamics": ("MultibandDynamics", _AUDIO_EFFECT, ("Output Gain", "Amount")),
    "Saturator": ("Saturator", _AUDIO_EFFECT, ("Drive", "Dry/Wet")),
    "Utility": ("StereoGain", _AUDIO_EFFECT, ("Gain", "Stereo Width")),
    "Delay": ("Delay", _AUDIO_EFFECT, ("Feedback", "Dry/Wet")),
    "Reverb": ("Reverb", _AUDIO_EFFECT, ("Decay Time", "Dry/Wet")),
    "Chorus-Ensemble": ("Chorus2", _AUDIO_EFFECT, ("Rate", "Dry/Wet")),
    "Phaser-Flanger": ("PhaserNew", _AUDIO_EFFECT, ("Rate", "Dry/Wet")),
    "Auto Filter": ("AutoFilter", _AUDIO_EFFECT, ("Frequency", "Dry/Wet")),
}


def native_device(name: str) -> Device:
    """Instantiate one native device by its browser name, as ``insert_device`` does."""
    if name == "Audio Effect Rack":
        return Device(name, "AudioEffectGroupDevice", _AUDIO_EFFECT, can_have_chains=True)
    if name not in NATIVE_DEVICES:
        raise RuntimeError(f"Unknown or non-native device '{name}'")
    class_name, device_type, parameter_names = NATIVE_DEVICES[name]
    return Device(name, class_name, device_type, parameter_names)


class MixerDevice:
    """``Live.MixerDevice.MixerDevice`` of a Track, Return Track or the Main track."""

    def __init__(self, send_names: Iterable[str] = (), *, crossfade_assignable: bool = True):
        self._volume = volume_parameter()
        self._panning = panning_parameter()
        self._track_activator = _on_off("Speaker On")
        self._sends = tuple(send_parameter(name) for name in send_names)
        self._crossfade_assignable = crossfade_assignable
        self._crossfade_assign = CrossfadeAssignment.NONE
        self._panning_mode = PanningMode.stereo
        self._canonical_parent: Any = None
        for parameter in (self._volume, self._panning, self._track_activator, *self._sends):
            parameter._canonical_parent = self

    @live_property
    def canonical_parent(self) -> Any:
        return self._canonical_parent

    @live_property
    def volume(self) -> DeviceParameter:
        return self._volume

    @live_property
    def panning(self) -> DeviceParameter:
        return self._panning

    @live_property
    def track_activator(self) -> DeviceParameter:
        return self._track_activator

    @live_property
    def sends(self) -> tuple[DeviceParameter, ...]:
        return self._sends

    def _add_send(self, name: str) -> None:
        parameter = send_parameter(name)
        parameter._canonical_parent = self
        self._sends = self._sends + (parameter,)

    @live_property
    def crossfade_assign(self) -> CrossfadeAssignment:
        if not self._crossfade_assignable:
            # The Main track has no crossfader assignment.
            raise AttributeError("crossfade_assign")
        return self._crossfade_assign

    @crossfade_assign.setter
    def crossfade_assign(self, value: Any) -> None:
        if not self._crossfade_assignable:
            raise AttributeError("crossfade_assign")
        self._crossfade_assign = _enum_setter(CrossfadeAssignment)(value)  # type: ignore[assignment]

    @live_property
    def panning_mode(self) -> PanningMode:
        return self._panning_mode

    @panning_mode.setter
    def panning_mode(self, value: Any) -> None:
        self._panning_mode = _enum_setter(PanningMode)(value)  # type: ignore[assignment]


# =============================================================================
# Clips and clip slots
# =============================================================================


class Groove:
    """``Live.Groove.Groove`` from the Set's groove pool."""

    def __init__(self, name: str = "Swing 16ths 57") -> None:
        self.name = name


class AutomationEnvelope:
    """Source-contract fixture for Python ``insert_step``/``value_at_time``.

    Pinned Live 12 pushbase calls these methods with a real DeviceParameter and
    positive intervals in internal units. This finite interval model tests
    bridge behavior; it supplies no empirical persistence or host unit proof.
    """

    def __init__(self, parameter: DeviceParameter) -> None:
        self._parameter = parameter
        self._steps: list[tuple[float, float, float]] = []

    def insert_step(self, start: Any, duration: Any, value: Any) -> None:
        """Replace a positive, finite interval without affecting other intervals."""
        start, duration, value = _float(start), _float(duration), _float(value)
        stop = start + duration
        if (
            not all(math.isfinite(number) for number in (start, duration, stop, value))
            or start < 0.0
            or duration <= 0.0
            or stop <= start
            or not self._parameter.min <= value <= self._parameter.max
        ):
            raise ArgumentError(
                "insert_step requires a finite positive interval and internal value"
            )
        retained = []
        for previous_start, previous_stop, previous_value in self._steps:
            if previous_stop <= start or previous_start >= stop:
                retained.append((previous_start, previous_stop, previous_value))
            else:
                if previous_start < start:
                    retained.append((previous_start, start, previous_value))
                if previous_stop > stop:
                    retained.append((stop, previous_stop, previous_value))
        self._steps = sorted(retained + [(start, stop, value)])

    def value_at_time(self, time: Any) -> float:
        """Read the model's half-open interval value independently of requests."""
        time = _float(time)
        if not math.isfinite(time) or time < 0.0:
            raise ArgumentError("value_at_time requires a finite non-negative time")
        for start, stop, value in self._steps:
            if start <= time < stop:
                return value
        # A fixture convention outside authored intervals, not a documented
        # assertion about native envelope extrapolation.
        return self._parameter.value


class Clip:
    """``Live.Clip.Clip`` in a Session clip slot of a MIDI track."""

    def __init__(
        self,
        length: float,
        live_version: tuple[int, int, int] = (12, 3, 5),
        *,
        python_envelope_api: bool = False,
    ) -> None:
        self._live_version = live_version
        self._python_envelope_api = python_envelope_api
        self._canonical_parent: Any = None
        self._live_major_version = live_version[0]
        self._name = ""
        self._notes: dict[int, MidiNote] = {}
        # An explicit provider event model, not evidence about actual Live event
        # coverage for MPE/Follow Actions or an embedded Python ABI.
        self._note_listeners: list[Any] = []
        self._next_note_id = 1
        self._signature_numerator = 4
        self._signature_denominator = 4
        self._start_marker = 0.0
        self._end_marker = length
        self._loop_start = 0.0
        self._loop_end = length
        # A new Session MIDI clip loops by default.
        self._looping = True
        self._muted = False
        self._launch_mode = LaunchMode.trigger
        self._launch_quantization = Quantization.q_global
        self._legato = False
        self._velocity_amount = 0.0
        self._groove: Groove | None = None
        self._envelopes: set[str] = set()
        self._automation_envelopes: dict[DeviceParameter, AutomationEnvelope] = {}

    # --- notes --------------------------------------------------------------

    def add_notes_listener(self, callback: Any) -> None:
        """Model a retained note-change callback with duplicate registration refusal."""
        if callback in self._note_listeners:
            raise ArgumentError("notes listener is already registered")
        self._note_listeners.append(callback)

    def notes_has_listener(self, callback: Any) -> bool:
        """Observe the modeled native listener membership."""
        return callback in self._note_listeners

    def remove_notes_listener(self, callback: Any) -> None:
        """Remove exactly the recorded callback from the provider model."""
        self._note_listeners.remove(callback)

    def _notify_notes_changed(self) -> None:
        for callback in tuple(self._note_listeners):
            callback()

    def __getattribute__(self, name: str) -> Any:
        version = object.__getattribute__(self, "_live_version")
        if name in {"add_new_notes", "get_notes_by_id", "get_notes_extended", "remove_notes_by_id"}:
            if version < (11, 0):
                raise AttributeError(f"{name} requires Live 11.0")
        if name == "get_all_notes_extended" and version < (11, 1):
            raise AttributeError("get_all_notes_extended requires Live 11.1")
        if name in {"automation_envelope", "create_automation_envelope", "clear_envelope"}:
            if not object.__getattribute__(self, "_python_envelope_api"):
                raise AttributeError(f"{name} is not enabled in this source-contract fixture")
        return object.__getattribute__(self, name)

    def add_new_notes(self, specifications: Any) -> IntVector:
        try:
            items = list(specifications)
        except TypeError as error:
            raise ArgumentError("add_new_notes expects an iterable") from error
        for item in items:
            if not isinstance(item, MidiNoteSpecification):
                raise ArgumentError(
                    f"expected MidiNoteSpecification, got {type(item).__name__} {item!r}"
                )
        note_ids = []
        for specification in items:
            note_id = self._next_note_id
            self._next_note_id += 1
            self._notes[note_id] = MidiNote(note_id, specification)
            note_ids.append(note_id)
        if note_ids:
            self._notify_notes_changed()
        # Whether Live 11.0 returns IDs is unsettled (#22); later versions do.
        return IntVector(note_ids)

    def get_notes_by_id(self, note_ids: Any) -> MidiNoteVector:
        try:
            requested = list(note_ids)
        except TypeError as error:
            raise ArgumentError("get_notes_by_id expects an iterable of note IDs") from error
        if any(isinstance(note_id, bool) or not isinstance(note_id, int) for note_id in requested):
            raise ArgumentError("get_notes_by_id expects integer note IDs")
        return MidiNoteVector(self._notes[i] for i in requested if i in self._notes)

    def get_all_notes_extended(self) -> MidiNoteVector:
        return MidiNoteVector(sorted(self._notes.values(), key=lambda n: (n.start_time, n.pitch)))

    def get_notes_extended(
        self, from_pitch: int, pitch_span: int, from_time: float, time_span: float
    ) -> MidiNoteVector:
        # The public LOM defines selection by note start, not by overlap with
        # the interval or by the clip's markers. Python keyword invocation is
        # source-observed in MxDCore; final-host range qualification remains #22.
        if any(
            isinstance(value, bool) or not isinstance(value, int)
            for value in (from_pitch, pitch_span)
        ):
            raise ArgumentError("pitch range requires integers")
        from_time = _float(from_time)
        time_span = _float(time_span)
        if not (math.isfinite(from_time) and math.isfinite(time_span) and time_span > 0.0):
            raise ArgumentError("time range requires finite values and a positive span")
        notes = (
            note
            for note in self._notes.values()
            if from_pitch <= note.pitch < from_pitch + pitch_span
            and from_time <= note.start_time < from_time + time_span
        )
        return MidiNoteVector(sorted(notes, key=lambda n: (n.start_time, n.pitch)))

    def remove_notes_by_id(self, note_ids: Iterable[int]) -> None:
        previous = len(self._notes)
        for note_id in list(note_ids):
            self._notes.pop(note_id, None)
        if len(self._notes) != previous:
            self._notify_notes_changed()

    # --- identity and structure ---------------------------------------------

    @live_property
    def canonical_parent(self) -> Any:
        return self._canonical_parent

    @live_property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value: Any) -> None:
        self._name = _string(value)

    @live_property
    def is_audio_clip(self) -> bool:
        return False

    @live_property
    def is_midi_clip(self) -> bool:
        return True

    @live_property
    def is_arrangement_clip(self) -> bool:
        return False

    @live_property
    def is_session_clip(self) -> bool:
        return True

    @live_property
    def is_take_lane_clip(self) -> bool:
        # The adapter only qualifies this Python property on Live 12+. Take
        # lanes themselves already exist in Live 11 (Live 11 Comping manual).
        if self._live_major_version < 12:
            raise AttributeError("is_take_lane_clip")
        return False

    @live_property
    def signature_numerator(self) -> int:
        return self._signature_numerator

    @signature_numerator.setter
    def signature_numerator(self, value: Any) -> None:
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= 99:
            raise RuntimeError("Invalid signature numerator")
        self._signature_numerator = int(value)

    @live_property
    def signature_denominator(self) -> int:
        return self._signature_denominator

    @signature_denominator.setter
    def signature_denominator(self, value: Any) -> None:
        if isinstance(value, bool) or not isinstance(value, int) or value not in (1, 2, 4, 8, 16):
            raise RuntimeError("Invalid signature denominator")
        self._signature_denominator = int(value)

    @live_property
    def start_marker(self) -> float:
        return self._start_marker

    @start_marker.setter
    def start_marker(self, value: Any) -> None:
        value = _float(value)
        if value >= self._end_marker:
            raise RuntimeError("Invalid marker position: start marker must precede end marker")
        self._start_marker = value

    @live_property
    def end_marker(self) -> float:
        return self._end_marker

    @end_marker.setter
    def end_marker(self, value: Any) -> None:
        value = _float(value)
        if value <= self._start_marker:
            raise RuntimeError("Invalid marker position: end marker must follow start marker")
        self._end_marker = value

    @live_property
    def loop_start(self) -> float:
        return self._loop_start

    @live_property
    def loop_end(self) -> float:
        return self._loop_end

    @live_property
    def looping(self) -> bool:
        return self._looping

    @looping.setter
    def looping(self, value: Any) -> None:
        self._looping = _bool(value)

    @live_property
    def length(self) -> float:
        # Cycling '74: loop length when looped, else the marker distance.
        if self._looping:
            return self._loop_end - self._loop_start
        return self._end_marker - self._start_marker

    @live_property
    def end_time(self) -> float:
        # Cycling '74: for Session clips, Loop End when looped, else End Marker.
        return self._loop_end if self._looping else self._end_marker

    @live_property
    def muted(self) -> bool:
        return self._muted

    @muted.setter
    def muted(self, value: Any) -> None:
        self._muted = _bool(value)

    # --- launch, groove and envelopes ---------------------------------------

    @live_property
    def launch_mode(self) -> LaunchMode:
        return self._launch_mode

    @launch_mode.setter
    def launch_mode(self, value: Any) -> None:
        self._launch_mode = _enum_setter(LaunchMode)(value)  # type: ignore[assignment]

    @live_property
    def launch_quantization(self) -> Quantization:
        return self._launch_quantization

    @launch_quantization.setter
    def launch_quantization(self, value: Any) -> None:
        self._launch_quantization = _enum_setter(Quantization)(value)  # type: ignore[assignment]

    @live_property
    def legato(self) -> bool:
        return self._legato

    @legato.setter
    def legato(self, value: Any) -> None:
        self._legato = _bool(value)

    @live_property
    def velocity_amount(self) -> float:
        return self._velocity_amount

    @velocity_amount.setter
    def velocity_amount(self, value: Any) -> None:
        value = _float(value)
        if not 0.0 <= value <= 1.0:
            raise RuntimeError("Invalid velocity amount")
        self._velocity_amount = value

    @live_property
    def groove(self) -> Groove | None:
        return self._groove

    @groove.setter
    def groove(self, value: Any) -> None:
        # Assigning None as the clearing idiom is unsettled (#22).
        if value is not None and not isinstance(value, Groove):
            raise ArgumentError(f"expected Groove, got {type(value).__name__}")
        self._groove = value

    @live_property
    def has_groove(self) -> bool:
        return self._groove is not None

    @live_property
    def has_envelopes(self) -> bool:
        return bool(self._envelopes or self._automation_envelopes)

    def clear_all_envelopes(self) -> None:
        self._envelopes.clear()
        self._automation_envelopes.clear()

    def automation_envelope(self, parameter: Any) -> AutomationEnvelope | None:
        """Look up with an actual Python parameter object; never create on read."""
        if not isinstance(parameter, DeviceParameter):
            raise ArgumentError("automation_envelope expects a DeviceParameter object")
        return self._automation_envelopes.get(parameter)

    def create_automation_envelope(self, parameter: Any) -> AutomationEnvelope:
        """Create the source-observed Python envelope on explicit authoring."""
        if not isinstance(parameter, DeviceParameter):
            raise ArgumentError("create_automation_envelope expects a DeviceParameter object")
        if parameter in self._automation_envelopes:
            raise RuntimeError("Parameter already has an envelope")
        envelope = AutomationEnvelope(parameter)
        self._automation_envelopes[parameter] = envelope
        return envelope

    def clear_envelope(self, parameter: Any) -> None:
        """Clear one parameter's envelope without disturbing another's."""
        if not isinstance(parameter, DeviceParameter):
            raise ArgumentError("clear_envelope expects a DeviceParameter object")
        self._automation_envelopes.pop(parameter, None)

    def add_envelope(self, parameter_name: str) -> None:
        """Model-only helper: give the clip an automation envelope."""
        self._envelopes.add(parameter_name)

    # --- runtime state -------------------------------------------------------

    @live_property
    def is_playing(self) -> bool:
        return False

    @live_property
    def is_recording(self) -> bool:
        return False

    @live_property
    def is_overdubbing(self) -> bool:
        return False

    @live_property
    def is_triggered(self) -> bool:
        return False

    @live_property
    def will_record_on_start(self) -> bool:
        return False


class ClipSlot:
    """``Live.ClipSlot.ClipSlot`` of a (non-group) track."""

    def __init__(self, track: Track) -> None:
        self._track = track
        self._clip: Clip | None = None

    @live_property
    def canonical_parent(self) -> Track:
        return self._track

    @live_property
    def clip(self) -> Clip | None:
        return self._clip

    @live_property
    def has_clip(self) -> bool:
        return self._clip is not None

    def create_clip(self, length: Any) -> None:
        # Cycling '74: length in beats, greater than 0, only on empty slots of MIDI tracks.
        length = _float(length)
        if self._clip is not None:
            raise RuntimeError("Clip slot is not empty")
        if not self._track._is_midi:
            raise RuntimeError("Can only create MIDI clips in MIDI tracks")
        if not length > 0.0:
            raise RuntimeError("Clip length must be greater than 0")
        self._clip = Clip(
            length,
            self._track._song._application.version_tuple(),
            python_envelope_api=self._track._song._python_envelope_api,
        )
        self._clip._canonical_parent = self

    def delete_clip(self) -> None:
        if self._clip is None:
            raise RuntimeError("Clip slot is empty")
        self._clip = None

    @live_property
    def has_stop_button(self) -> bool:
        return True

    @live_property
    def is_group_slot(self) -> bool:
        return False

    @live_property
    def controls_other_clips(self) -> bool:
        return False

    @live_property
    def is_playing(self) -> bool:
        return False

    @live_property
    def is_recording(self) -> bool:
        return False

    @live_property
    def is_triggered(self) -> bool:
        return False

    @live_property
    def will_record_on_start(self) -> bool:
        return False

    @live_property
    def playing_status(self) -> ClipSlotPlayingStatus:
        return ClipSlotPlayingStatus.stopped


# =============================================================================
# Routing
# =============================================================================


class RoutingType:
    """``RoutingType``: a fresh host wrapper per read, equal when it names the same target."""

    def __init__(
        self, key: tuple[Any, ...], display_name: str, category: RoutingTypeCategory, target: Any
    ):
        self._key = key
        self._display_name = display_name
        self._category = category
        self._target = target

    @live_property
    def display_name(self) -> str:
        return self._display_name

    @live_property
    def category(self) -> RoutingTypeCategory:
        return self._category

    @live_property
    def attached_object(self) -> Any:
        return self._target

    def __eq__(self, other: object) -> bool:
        return isinstance(other, RoutingType) and other._key == self._key

    def __hash__(self) -> int:
        return hash(self._key)


class RoutingChannel:
    """``RoutingChannel``: a fresh host wrapper per read, equal when it names the same channel."""

    def __init__(self, key: tuple[Any, ...], display_name: str) -> None:
        self._key = key
        self._display_name = display_name

    @live_property
    def display_name(self) -> str:
        return self._display_name

    def __eq__(self, other: object) -> bool:
        return isinstance(other, RoutingChannel) and other._key == self._key

    def __hash__(self) -> int:
        return hash(self._key)


_STEREO_CHANNELS = ("1/2", "3/4", "1", "2", "3", "4")
_MIDI_CHANNELS = ("All Channels",) + tuple(f"Ch. {n}" for n in range(1, 17))


# =============================================================================
# Tracks
# =============================================================================


class Track:
    """``Live.Track.Track``: a MIDI track, a Return Track, or the Main track."""

    def __init__(self, song: Song, name: str, kind: str) -> None:
        self._song = song
        self._kind = kind  # "midi", "return" or "master"
        self._is_midi = kind == "midi"
        self._name = name
        self._mute = False
        self._solo = False
        self._arm = False
        self._implicit_arm = False
        self._devices: list[Device] = []
        self._mixer = MixerDevice(
            (f"Send {chr(ord('A') + i)}" for i in range(len(song._return_tracks)))
            if self._is_midi
            else (),
            crossfade_assignable=kind != "master",
        )
        self._mixer._canonical_parent = self
        self._clip_slots = tuple(ClipSlot(self) for _ in song._scenes) if self._is_midi else ()
        self._output_type: tuple[Any, ...] | None = None
        self._output_channel: tuple[Any, ...] | None = None
        self._input_type: tuple[Any, ...] = ("all_ins",)
        self._input_channel: tuple[Any, ...] = ("all_channels",)
        self._meters = dict.fromkeys(
            (
                "input_meter_level",
                "output_meter_level",
                "input_meter_left",
                "input_meter_right",
                "output_meter_left",
                "output_meter_right",
            ),
            0.0,
        )

    # --- identity and gates --------------------------------------------------

    @live_property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value: Any) -> None:
        self._name = _string(value)

    @live_property
    def mute(self) -> bool:
        if self._kind == "master":
            raise AttributeError("mute")
        return self._mute

    @mute.setter
    def mute(self, value: Any) -> None:
        self._mute = _bool(value)

    @live_property
    def solo(self) -> bool:
        if self._kind == "master":
            raise AttributeError("solo")
        return self._solo

    @solo.setter
    def solo(self, value: Any) -> None:
        self._solo = _bool(value)

    @live_property
    def muted_via_solo(self) -> bool:
        return False

    @live_property
    def can_be_armed(self) -> bool:
        return self._is_midi

    @live_property
    def arm(self) -> bool:
        return self._arm

    @arm.setter
    def arm(self, value: Any) -> None:
        if not self._is_midi:
            raise RuntimeError("Track cannot be armed")
        self._arm = _bool(value)

    @live_property
    def implicit_arm(self) -> bool:
        return self._implicit_arm

    @implicit_arm.setter
    def implicit_arm(self, value: Any) -> None:
        self._implicit_arm = _bool(value)

    @live_property
    def is_frozen(self) -> bool:
        return False

    @live_property
    def is_grouped(self) -> bool:
        return False

    @live_property
    def group_track(self) -> Track | None:
        return None

    @live_property
    def back_to_arranger(self) -> bool:
        return False

    @live_property
    def fired_slot_index(self) -> int:
        return -1

    @live_property
    def playing_slot_index(self) -> int:
        return -1

    # --- content -------------------------------------------------------------

    @live_property
    def clip_slots(self) -> tuple[ClipSlot, ...]:
        return self._clip_slots

    def _add_clip_slot(self, index: int) -> None:
        slots = list(self._clip_slots)
        slots.insert(index, ClipSlot(self))
        self._clip_slots = tuple(slots)

    @live_property
    def arrangement_clips(self) -> tuple[Clip, ...]:
        return ()

    @live_property
    def take_lanes(self) -> Vector:
        # API exposure, not Live's take-lane content, is gated here. Live 11
        # already has take lanes; this container's runtime type is unsettled (#22).
        if self._song._application.get_major_version() < 12:
            raise AttributeError("take_lanes")
        return Vector()

    @live_property
    def mixer_device(self) -> MixerDevice:
        return self._mixer

    @live_property
    def devices(self) -> tuple[Any, ...]:
        # Cycling '74 documents Track.devices as including the mixer device.
        # Its position is not documented, so the model reports it last.
        return tuple(self._devices) + (self._mixer,)

    def insert_device(self, device_name: Any, target_index: Any = None) -> None:
        # Track.insert_device inserts native devices only (Live 12.3+).
        if self._song._application.version_tuple() < (12, 3):
            raise AttributeError("insert_device")
        device_name = _string(device_name)
        index = len(self._devices) if target_index is None else target_index
        if isinstance(index, bool) or not isinstance(index, int) or index < 0:
            raise ArgumentError("target_index must be a non-negative int")
        if index > len(self._devices):
            raise RuntimeError("Device index out of range")
        device = native_device(device_name)
        if self._kind != "midi" and device.type == DeviceType.instrument:
            raise RuntimeError("Instruments can only be inserted on MIDI tracks")
        device._canonical_parent = self
        self._devices.insert(index, device)

    # --- signal flow ----------------------------------------------------------

    @live_property
    def has_midi_input(self) -> bool:
        if not self._is_midi:
            raise AttributeError("has_midi_input")
        return True

    @live_property
    def has_audio_input(self) -> bool:
        if not self._is_midi:
            raise AttributeError("has_audio_input")
        return False

    @live_property
    def has_audio_output(self) -> bool:
        # A MIDI track outputs audio only once it hosts an instrument.
        if not self._is_midi:
            return True
        return any(device.type == DeviceType.instrument for device in self._devices)

    @live_property
    def has_midi_output(self) -> bool:
        return not self.has_audio_output

    def _output_types(self) -> list[tuple[tuple[Any, ...], str, RoutingTypeCategory, Any]]:
        if self._kind == "master":
            return [(("external",), "Ext. Out", RoutingTypeCategory.external, None)]
        if not self.has_audio_output:
            return [(("none",), "No Output", RoutingTypeCategory.none, None)]
        options = [
            (("master",), "Master", RoutingTypeCategory.master, self._song._master),
            (("external",), "Ext. Out", RoutingTypeCategory.external, None),
        ]
        for track in self._song._return_tracks:
            if track is not self:
                options.append(
                    (("track", id(track)), track._name, RoutingTypeCategory.track, track)
                )
        return options

    def _output_channels(self, type_key: tuple[Any, ...]) -> list[tuple[tuple[Any, ...], str]]:
        if type_key == ("external",):
            return [(("external", name), name) for name in _STEREO_CHANNELS]
        if type_key == ("none",):
            # The channel of a disabled route has no documented name (#22).
            return [(("none",), "")]
        return [(type_key + ("track_in",), "Track In")]

    def _current_output(self) -> tuple[tuple[Any, ...], tuple[Any, ...]]:
        # Live re-selects a valid route when the current one disappears, for
        # example when an instrument turns a MIDI track into an audio track.
        type_keys = [option[0] for option in self._output_types()]
        if self._output_type not in type_keys:
            self._output_type = type_keys[0]
            self._output_channel = None
        channel_keys = [option[0] for option in self._output_channels(self._output_type)]
        if self._output_channel not in channel_keys:
            self._output_channel = channel_keys[0]
        return self._output_type, self._output_channel  # type: ignore[return-value]

    @live_property
    def available_output_routing_types(self) -> tuple[RoutingType, ...]:
        return tuple(RoutingType(*option) for option in self._output_types())

    @live_property
    def output_routing_type(self) -> RoutingType:
        type_key, _ = self._current_output()
        return next(RoutingType(*o) for o in self._output_types() if o[0] == type_key)

    @output_routing_type.setter
    def output_routing_type(self, value: Any) -> None:
        if not isinstance(value, RoutingType):
            raise ArgumentError(f"expected RoutingType, got {type(value).__name__}")
        if value not in self.available_output_routing_types:
            raise RuntimeError("Routing type is not available for this track")
        self._output_type = value._key
        self._output_channel = None

    @live_property
    def available_output_routing_channels(self) -> tuple[RoutingChannel, ...]:
        type_key, _ = self._current_output()
        return tuple(RoutingChannel(*option) for option in self._output_channels(type_key))

    @live_property
    def output_routing_channel(self) -> RoutingChannel:
        type_key, channel_key = self._current_output()
        return next(
            RoutingChannel(*o) for o in self._output_channels(type_key) if o[0] == channel_key
        )

    @output_routing_channel.setter
    def output_routing_channel(self, value: Any) -> None:
        if not isinstance(value, RoutingChannel):
            raise ArgumentError(f"expected RoutingChannel, got {type(value).__name__}")
        if value not in self.available_output_routing_channels:
            raise RuntimeError("Routing channel is not available for this track")
        self._output_channel = value._key

    def _input_types(self) -> list[tuple[tuple[Any, ...], str, RoutingTypeCategory, Any]]:
        if not self._is_midi:
            raise AttributeError("available_input_routing_types")
        return [
            (("all_ins",), "All Ins", RoutingTypeCategory.external, None),
            (("computer_keyboard",), "Computer Keyboard", RoutingTypeCategory.external, None),
            (("none",), "No Input", RoutingTypeCategory.none, None),
        ]

    def _input_channels(self, type_key: tuple[Any, ...]) -> list[tuple[tuple[Any, ...], str]]:
        if type_key == ("none",):
            return [(("none",), "")]
        return [
            (("all_channels",) if name == "All Channels" else ("channel", name), name)
            for name in _MIDI_CHANNELS
        ]

    @live_property
    def available_input_routing_types(self) -> tuple[RoutingType, ...]:
        return tuple(RoutingType(*option) for option in self._input_types())

    @live_property
    def input_routing_type(self) -> RoutingType:
        return next(RoutingType(*o) for o in self._input_types() if o[0] == self._input_type)

    @input_routing_type.setter
    def input_routing_type(self, value: Any) -> None:
        if not isinstance(value, RoutingType):
            raise ArgumentError(f"expected RoutingType, got {type(value).__name__}")
        if value not in self.available_input_routing_types:
            raise RuntimeError("Routing type is not available for this track")
        self._input_type = value._key
        self._input_channel = self._input_channels(value._key)[0][0]

    @live_property
    def available_input_routing_channels(self) -> tuple[RoutingChannel, ...]:
        self._input_types()
        return tuple(RoutingChannel(*o) for o in self._input_channels(self._input_type))

    @live_property
    def input_routing_channel(self) -> RoutingChannel:
        self._input_types()
        return next(
            RoutingChannel(*o)
            for o in self._input_channels(self._input_type)
            if o[0] == self._input_channel
        )

    @input_routing_channel.setter
    def input_routing_channel(self, value: Any) -> None:
        if not isinstance(value, RoutingChannel):
            raise ArgumentError(f"expected RoutingChannel, got {type(value).__name__}")
        if value not in self.available_input_routing_channels:
            raise RuntimeError("Routing channel is not available for this track")
        self._input_channel = value._key

    # --- meters ---------------------------------------------------------------

    def set_meters(self, level: float) -> None:
        """Model-only helper: simulate signal, as peak-hold and momentary meters show."""
        for name in self._meters:
            self._meters[name] = float(level)

    def _meter(self, name: str) -> float:
        # Input meters exist only where the track has an input.
        if name.startswith("input") and not self._is_midi:
            raise AttributeError(name)
        return self._meters[name]

    @live_property
    def input_meter_level(self) -> float:
        return self._meter("input_meter_level")

    @live_property
    def output_meter_level(self) -> float:
        return self._meter("output_meter_level")

    @live_property
    def input_meter_left(self) -> float:
        return self._meter("input_meter_left")

    @live_property
    def input_meter_right(self) -> float:
        return self._meter("input_meter_right")

    @live_property
    def output_meter_left(self) -> float:
        return self._meter("output_meter_left")

    @live_property
    def output_meter_right(self) -> float:
        return self._meter("output_meter_right")


# =============================================================================
# Song, scenes, cue points and application
# =============================================================================


class Scene:
    """``Live.Scene.Scene``."""

    def __init__(self, name: str = "") -> None:
        self._name = name
        self._tempo_enabled = False
        self._time_signature_enabled = False
        self._tempo = 120.0
        self._numerator = 4
        self._denominator = 4

    @live_property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value: Any) -> None:
        self._name = _string(value)

    @live_property
    def is_triggered(self) -> bool:
        return False

    @live_property
    def tempo_enabled(self) -> bool:
        return self._tempo_enabled

    @tempo_enabled.setter
    def tempo_enabled(self, value: Any) -> None:
        self._tempo_enabled = _bool(value)

    @live_property
    def time_signature_enabled(self) -> bool:
        return self._time_signature_enabled

    @time_signature_enabled.setter
    def time_signature_enabled(self, value: Any) -> None:
        self._time_signature_enabled = _bool(value)

    # A disabled launch override reads as -1, as Live reports it.
    @live_property
    def tempo(self) -> float:
        return self._tempo if self._tempo_enabled else -1.0

    @live_property
    def time_signature_numerator(self) -> int:
        return self._numerator if self._time_signature_enabled else -1

    @live_property
    def time_signature_denominator(self) -> int:
        return self._denominator if self._time_signature_enabled else -1

    def enable_launch_overrides(self, tempo: float, numerator: int, denominator: int) -> None:
        """Model-only helper: give the scene a tempo and signature launch override."""
        self._tempo_enabled = True
        self._time_signature_enabled = True
        self._tempo, self._numerator, self._denominator = tempo, numerator, denominator


class CuePoint:
    """``Live.Song.CuePoint``: its time is read-only; its name is writable."""

    def __init__(self, time: float, name: str = "") -> None:
        self._time = time
        self._name = name

    @live_property
    def time(self) -> float:
        return self._time

    @live_property
    def name(self) -> str:
        return self._name

    @name.setter
    def name(self, value: Any) -> None:
        self._name = _string(value)


class TuningSystem:
    """``Live.TuningSystem.TuningSystem`` (Live 12.1+), with Max-form dictionaries."""

    def __init__(self) -> None:
        self.name = "12-TET"
        self.pseudo_octave_in_cents = 1200.0
        self.lowest_note = {"index": 0, "octave": -2}
        self.highest_note = {"index": 7, "octave": 8}
        self.reference_pitch = {"index": 9, "octave": 3, "frequency": 440.0}
        self.note_tunings = {"note_tunings": [100.0 * n for n in range(12)]}


class Song:
    """``Live.Song.Song``: the document the Remote Script edits."""

    def __init__(self, application: Application, scene_count: int = 1) -> None:
        self._application = application
        self._tempo = 120.0
        self._numerator = 4
        self._denominator = 4
        self._current_song_time = 0.0
        self._flags = dict.fromkeys(
            (
                "is_playing",
                "is_counting_in",
                "arrangement_overdub",
                "overdub",
                "record_mode",
                "session_record",
                "session_automation_record",
                "is_ableton_link_enabled",
                "is_ableton_link_start_stop_sync_enabled",
                "tempo_follower_enabled",
                "nudge_down",
                "nudge_up",
                "back_to_arranger",
                "re_enable_automation_enabled",
                "loop",
                "metronome",
            ),
            False,
        )
        self._scenes: list[Scene] = [Scene() for _ in range(scene_count)]
        self._return_tracks: list[Track] = []
        self._tracks: list[Track] = []
        self._master = Track(self, "Main", "master")
        self._cue_points: list[CuePoint] = []
        self._tuning_system = TuningSystem()
        self._root_note = 0
        self._scale_name = "Major"
        self._scale_intervals = IntVector((0, 2, 4, 5, 7, 9, 11))
        self._scale_mode = False

    # --- tempo, meter and transport -------------------------------------------

    @live_property
    def tempo(self) -> float:
        return self._tempo

    @tempo.setter
    def tempo(self, value: Any) -> None:
        value = _float(value)
        if not 20.0 <= value <= 999.0:
            raise RuntimeError("Tempo must be between 20 and 999 BPM")
        self._tempo = value

    @live_property
    def signature_numerator(self) -> int:
        return self._numerator

    @signature_numerator.setter
    def signature_numerator(self, value: Any) -> None:
        if isinstance(value, bool) or not isinstance(value, int) or not 1 <= value <= 99:
            raise RuntimeError("Invalid signature numerator")
        self._numerator = int(value)

    @live_property
    def signature_denominator(self) -> int:
        return self._denominator

    @signature_denominator.setter
    def signature_denominator(self, value: Any) -> None:
        if isinstance(value, bool) or not isinstance(value, int) or value not in (1, 2, 4, 8, 16):
            raise RuntimeError("Invalid signature denominator")
        self._denominator = int(value)

    @live_property
    def current_song_time(self) -> float:
        return self._current_song_time

    @current_song_time.setter
    def current_song_time(self, value: Any) -> None:
        self._current_song_time = max(0.0, _float(value))

    def _flag(name: str) -> _LiveProperty:  # type: ignore[misc]
        def read(self: Song) -> bool:
            return self._flags[name]

        read.__name__ = name
        return _LiveProperty(read)

    is_playing = _flag("is_playing")
    is_counting_in = _flag("is_counting_in")
    arrangement_overdub = _flag("arrangement_overdub")
    overdub = _flag("overdub")
    record_mode = _flag("record_mode")
    session_record = _flag("session_record")
    session_automation_record = _flag("session_automation_record")
    is_ableton_link_enabled = _flag("is_ableton_link_enabled")
    is_ableton_link_start_stop_sync_enabled = _flag("is_ableton_link_start_stop_sync_enabled")
    tempo_follower_enabled = _flag("tempo_follower_enabled")
    nudge_down = _flag("nudge_down")
    nudge_up = _flag("nudge_up")
    back_to_arranger = _flag("back_to_arranger")
    re_enable_automation_enabled = _flag("re_enable_automation_enabled")
    loop = _flag("loop")
    metronome = _flag("metronome")
    del _flag

    # --- pitch context ----------------------------------------------------------

    @live_property
    def root_note(self) -> int:
        return self._root_note

    @live_property
    def scale_name(self) -> str:
        return self._scale_name

    @live_property
    def scale_intervals(self) -> IntVector:
        # Cycling '74 documents a list of ints; the host container is unsettled (#22).
        return self._scale_intervals

    @live_property
    def scale_mode(self) -> bool:
        return self._scale_mode

    @live_property
    def tuning_system(self) -> TuningSystem:
        if self._application.version_tuple() < (12, 1):
            raise AttributeError("tuning_system")
        return self._tuning_system

    # --- collections --------------------------------------------------------------

    @live_property
    def tracks(self) -> tuple[Track, ...]:
        return tuple(self._tracks)

    @live_property
    def return_tracks(self) -> tuple[Track, ...]:
        return tuple(self._return_tracks)

    @live_property
    def master_track(self) -> Track:
        return self._master

    @live_property
    def scenes(self) -> tuple[Scene, ...]:
        return tuple(self._scenes)

    @live_property
    def cue_points(self) -> tuple[CuePoint, ...]:
        return tuple(sorted(self._cue_points, key=lambda cue: cue._time))

    # --- structural edits -----------------------------------------------------------

    @staticmethod
    def _insertion_index(index: Any, length: int) -> int:
        if isinstance(index, bool) or not isinstance(index, int):
            raise ArgumentError(f"expected int, got {type(index).__name__}")
        if index == -1:
            return length
        if not 0 <= index <= length:
            raise RuntimeError("Index out of range")
        return index

    def create_midi_track(self, index: Any = -1) -> Track:
        position = self._insertion_index(index, len(self._tracks))
        track = Track(self, f"{position + 1}-MIDI", "midi")
        self._tracks.insert(position, track)
        return track

    def create_return_track(self) -> Track:
        # Live appends Return Tracks and gives every track a send to the new return.
        letter = chr(ord("A") + len(self._return_tracks))
        track = Track(self, f"{letter}-Return", "return")
        self._return_tracks.append(track)
        for existing in self._tracks:
            existing._mixer._add_send(f"Send {letter}")
        return track

    def create_scene(self, index: Any = -1) -> Scene:
        position = self._insertion_index(index, len(self._scenes))
        scene = Scene()
        self._scenes.insert(position, scene)
        for track in self._tracks:
            track._add_clip_slot(position)
        return scene

    def set_or_delete_cue(self) -> None:
        # Toggles a cue at the current song time; takes no arguments.
        for cue in self._cue_points:
            if cue._time == self._current_song_time:
                self._cue_points.remove(cue)
                return
        self._cue_points.append(CuePoint(self._current_song_time))

    def set_transport_flag(self, name: str, value: bool) -> None:
        """Model-only helper: change a transport or recording predicate."""
        if name not in self._flags:
            raise KeyError(name)
        self._flags[name] = value


class Application:
    """``Live.Application.Application``."""

    def __init__(self, version: tuple[int, int, int] = (12, 3, 5)) -> None:
        self._version = version
        self._document: Song | None = None

    def version_tuple(self) -> tuple[int, int, int]:
        return self._version

    def get_major_version(self) -> int:
        return self._version[0]

    def get_minor_version(self) -> int:
        return self._version[1]

    def get_bugfix_version(self) -> int:
        return self._version[2]

    def get_version_string(self) -> str:
        return ".".join(str(part) for part in self._version)

    def get_document(self) -> Song:
        assert self._document is not None
        return self._document


# =============================================================================
# The importable ``Live`` module and a Control Surface stand-in
# =============================================================================


class LiveSet:
    """One Live process: an Application, its Song, and the importable ``Live`` module."""

    def __init__(
        self,
        version: tuple[int, int, int] = (12, 3, 5),
        *,
        scenes: int = 1,
        midi_tracks: int = 0,
        return_tracks: int = 0,
        python_envelope_api: bool = False,
    ) -> None:
        self.application = Application(version)
        self.song = Song(self.application, scenes)
        # Deliberately explicit; a Live major version is not evidence of this
        # private Python API or its saved/reopened behavior on that host.
        self.song._python_envelope_api = python_envelope_api
        self.application._document = self.song
        for _ in range(return_tracks):
            self.song.create_return_track()
        for _ in range(midi_tracks):
            self.song.create_midi_track(-1)
        self.module = self._build_module()

    def _build_module(self) -> types.ModuleType:
        def submodule(name: str, **members: Any) -> types.ModuleType:
            module = types.ModuleType(f"Live.{name}")
            module.__dict__.update(members)
            return module

        live = types.ModuleType("Live")
        live.Application = submodule(  # type: ignore[attr-defined]
            "Application", Application=Application, get_application=lambda: self.application
        )
        live.Base = submodule(  # type: ignore[attr-defined]
            "Base", Vector=Vector, StringVector=StringVector, IntVector=IntVector
        )
        live.Clip = submodule(  # type: ignore[attr-defined]
            "Clip",
            Clip=Clip,
            MidiNoteSpecification=MidiNoteSpecification,
            MidiNote=MidiNote,
            MidiNoteVector=MidiNoteVector,
        )
        live.ClipSlot = submodule("ClipSlot", ClipSlot=ClipSlot)  # type: ignore[attr-defined]
        live.Device = submodule("Device", Device=Device, DeviceType=DeviceType)  # type: ignore[attr-defined]
        live.DeviceParameter = submodule(  # type: ignore[attr-defined]
            "DeviceParameter",
            DeviceParameter=DeviceParameter,
            ParameterState=ParameterState,
            AutomationState=AutomationState,
        )
        live.MixerDevice = submodule("MixerDevice", MixerDevice=MixerDevice)  # type: ignore[attr-defined]
        live.Song = submodule("Song", Song=Song, CuePoint=CuePoint)  # type: ignore[attr-defined]
        live.Scene = submodule("Scene", Scene=Scene)  # type: ignore[attr-defined]
        live.Track = submodule(  # type: ignore[attr-defined]
            "Track",
            Track=Track,
            RoutingType=RoutingType,
            RoutingChannel=RoutingChannel,
            RoutingTypeCategory=RoutingTypeCategory,
        )
        return live

    def install(self, monkeypatch: Any) -> LiveSet:
        """Make ``import Live`` resolve to this Set for the duration of one test."""
        monkeypatch.setitem(sys.modules, "Live", self.module)
        return self

    @property
    def surface(self) -> Surface:
        """Return a Control Surface stand-in whose ``song()`` is this Set's document."""
        return Surface(self.song)


class Surface:
    """The part of ``_Framework.ControlSurface`` the handler uses: ``song()``."""

    def __init__(self, song: Song) -> None:
        self._song = song

    def song(self) -> Song:
        return self._song
