"""Sunny core layer.

Thin scripting facade over the C++ theory engine (sunny_native).
All theory computation is delegated to the native extension; this module
adds only constants, note-name maps, and MIDI/octave conversions. There
is no pure-Python fallback: if the native backend is absent, theory
operations raise ImportError rather than fabricating results.
"""

from __future__ import annotations

try:
    import sunny_native

    NATIVE_AVAILABLE = True
except ImportError:  # pragma: no cover - exercised only without a build
    sunny_native = None  # type: ignore[assignment]
    NATIVE_AVAILABLE = False

# Constants mirrored from sunny/core/types/music_types.hpp.
MIDI_NOTE_MIN = 0
MIDI_NOTE_MAX = 127
VELOCITY_MIN = 1
VELOCITY_MAX = 127
PITCH_CLASS_COUNT = 12
TEMPO_MIN_BPM = 20.0
TEMPO_MAX_BPM = 999.0

# Pitch class names
PITCH_CLASS_NAMES_SHARP = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
PITCH_CLASS_NAMES_FLAT = ("C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B")

# Note name to pitch class mapping
NOTE_NAME_TO_PC = {
    "C": 0,
    "C#": 1,
    "Db": 1,
    "D": 2,
    "D#": 3,
    "Eb": 3,
    "E": 4,
    "Fb": 4,
    "E#": 5,
    "F": 5,
    "F#": 6,
    "Gb": 6,
    "G": 7,
    "G#": 8,
    "Ab": 8,
    "A": 9,
    "A#": 10,
    "Bb": 10,
    "B": 11,
    "Cb": 11,
    "B#": 0,
}

# Type aliases (for Python typing)
PitchClass = int  # [0, 11]
MidiNote = int  # [0, 127]
Velocity = int  # [1, 127]
Interval = int  # semitones


def is_native_available() -> bool:
    """Check if the native C++ backend is importable."""
    return NATIVE_AVAILABLE


_BACKEND_REQUIRED = (
    "sunny_native C++ backend is required. Install a platform wheel or "
    "build the source checkout with 'uv sync'. For a direct CMake build, "
    "ensure .bin/python is on sys.path."
)


def is_valid_pitch_class(pc: int) -> bool:
    """Check if value is a valid pitch class [0, 11]."""
    return 0 <= pc < PITCH_CLASS_COUNT


def is_valid_midi_note(note: int) -> bool:
    """Check if value is a valid MIDI note [0, 127]."""
    return MIDI_NOTE_MIN <= note <= MIDI_NOTE_MAX


def is_valid_velocity(vel: int) -> bool:
    """Check if value is a valid velocity [1, 127]."""
    return VELOCITY_MIN <= vel <= VELOCITY_MAX


# Theory operations - direct delegation, no fallback
def pitch_class(midi_note: int) -> int:
    """Get pitch class from MIDI note."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.pitch_class(midi_note)


def transpose(pc: int, interval: int) -> int:
    """Transpose pitch class by interval (mod 12)."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.transpose(pc, interval)


def invert(pc: int, axis: int = 0) -> int:
    """Invert pitch class around axis (mod 12)."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.invert(pc, axis)


def interval_class(semitones: int) -> int:
    """Get interval class [0, 6]."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.interval_class(semitones)


def note_name(pc: int, prefer_flats: bool = False) -> str:
    """Get note name from pitch class."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.note_name(pc, prefer_flats)


def closest_pitch_class_midi(reference: int, target_pc: int) -> int:
    """Find MIDI note with target_pc closest to reference."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.closest_pitch_class_midi(reference, target_pc)


def euclidean_rhythm(pulses: int, steps: int, rotation: int = 0) -> list[bool]:
    """Generate Euclidean rhythm pattern."""
    if sunny_native is None:
        raise ImportError(_BACKEND_REQUIRED)
    return sunny_native.euclidean_rhythm(pulses, steps, rotation)


# Pure conversions (deterministic bijections, not theory)
def midi_to_pitch_octave(midi: int) -> tuple[int, int]:
    """Convert MIDI note to (pitch_class, octave)."""
    return midi % 12, midi // 12 - 1


def pitch_octave_to_midi(pc: int, octave: int) -> int:
    """Convert pitch class and octave to MIDI note."""
    return (octave + 1) * 12 + pc


def note_name_to_midi(name: str, octave: int) -> int:
    """Convert note name and octave to MIDI note.

    Raises:
        ValueError: If the note name is not recognised.
    """
    note = name[:2] if len(name) > 1 and name[1] in "#b" else name[:1]
    pc = NOTE_NAME_TO_PC.get(note[0].upper() + note[1:])
    if pc is None:
        raise ValueError(f"Unknown note name: {name!r}")
    return pitch_octave_to_midi(pc, octave)


__all__ = [
    "NATIVE_AVAILABLE",
    "is_native_available",
    # Constants
    "MIDI_NOTE_MIN",
    "MIDI_NOTE_MAX",
    "VELOCITY_MIN",
    "VELOCITY_MAX",
    "PITCH_CLASS_COUNT",
    "TEMPO_MIN_BPM",
    "TEMPO_MAX_BPM",
    "PITCH_CLASS_NAMES_SHARP",
    "PITCH_CLASS_NAMES_FLAT",
    "NOTE_NAME_TO_PC",
    # Type aliases
    "PitchClass",
    "MidiNote",
    "Velocity",
    "Interval",
    # Validators
    "is_valid_pitch_class",
    "is_valid_midi_note",
    "is_valid_velocity",
    # Theory operations
    "pitch_class",
    "transpose",
    "invert",
    "interval_class",
    "note_name",
    "closest_pitch_class_midi",
    "euclidean_rhythm",
    # Conversions
    "midi_to_pitch_octave",
    "pitch_octave_to_midi",
    "note_name_to_midi",
]
