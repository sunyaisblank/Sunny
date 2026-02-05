"""Sunny Core Layer.

Component: CORE001A
Domain: CR (Core) | Category: PK (Package)

The core layer provides access to the C++ theory computation backend.
Pure Python fallback is available when native extension is not built.

Subsystems (via C++ sunny_native):
    - Pitch: Pitch class operations (Z/12Z group)
    - Scale: Scale definitions and generation
    - Harmony: Harmonic analysis and generation
    - VoiceLeading: Voice leading algorithms
    - Rhythm: Euclidean rhythm generation
"""

from typing import TYPE_CHECKING

# Try to import native backend
try:
    import sunny_native
    NATIVE_AVAILABLE = True
except ImportError:
    NATIVE_AVAILABLE = False
    sunny_native = None

# Constants
MIDI_NOTE_MIN = 0
MIDI_NOTE_MAX = 127
VELOCITY_MIN = 1
VELOCITY_MAX = 127
MIDI_VELOCITY_MIN = 1
MIDI_VELOCITY_MAX = 127
PITCH_CLASS_COUNT = 12
TEMPO_MIN_BPM = 20.0
TEMPO_MAX_BPM = 999.0
BEAT_EPSILON = 1e-9
LCM_MAX_DENOMINATOR = 10000

# Pitch class names
PITCH_CLASS_NAMES_SHARP = ("C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B")
PITCH_CLASS_NAMES_FLAT = ("C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B")


def is_native_available() -> bool:
    """Check if native C++ backend is available."""
    return NATIVE_AVAILABLE


def get_backend_info() -> dict:
    """Get backend configuration information."""
    if NATIVE_AVAILABLE:
        return {
            "backend": "native",
            "version": getattr(sunny_native, "__version__", "unknown"),
        }
    return {
        "backend": "python_fallback",
        "version": "0.1.0",
    }


# Type aliases (for Python typing)
PitchClass = int  # [0, 11]
MidiNote = int    # [0, 127]
Velocity = int    # [1, 127]
Interval = int    # semitones


def is_valid_pitch_class(pc: int) -> bool:
    """Check if value is a valid pitch class [0, 11]."""
    return 0 <= pc < 12


def is_valid_midi_note(note: int) -> bool:
    """Check if value is a valid MIDI note [0, 127]."""
    return MIDI_NOTE_MIN <= note <= MIDI_NOTE_MAX


def is_valid_velocity(vel: int) -> bool:
    """Check if value is a valid velocity [1, 127]."""
    return VELOCITY_MIN <= vel <= VELOCITY_MAX


# Core pitch operations - delegate to native if available
def pitch_class(midi_note: int) -> int:
    """Get pitch class from MIDI note."""
    if NATIVE_AVAILABLE:
        return sunny_native.pitch_class(midi_note)
    return midi_note % 12


def transpose(pc: int, interval: int) -> int:
    """Transpose pitch class by interval (mod 12)."""
    if NATIVE_AVAILABLE:
        return sunny_native.transpose(pc, interval)
    return (pc + interval) % 12


def invert(pc: int, axis: int = 0) -> int:
    """Invert pitch class around axis (mod 12)."""
    if NATIVE_AVAILABLE:
        return sunny_native.invert(pc, axis)
    return (2 * axis - pc) % 12


def interval_class(semitones: int) -> int:
    """Get interval class [0, 6]."""
    if NATIVE_AVAILABLE:
        return sunny_native.interval_class(semitones)
    ic = abs(semitones) % 12
    return min(ic, 12 - ic)


def note_name(pc: int, prefer_flats: bool = False) -> str:
    """Get note name from pitch class."""
    if NATIVE_AVAILABLE:
        return sunny_native.note_name(pc, prefer_flats)
    names = PITCH_CLASS_NAMES_FLAT if prefer_flats else PITCH_CLASS_NAMES_SHARP
    return names[pc % 12]


# Note name to pitch class mapping
NOTE_NAME_TO_PC = {
    "C": 0, "C#": 1, "Db": 1, "D": 2, "D#": 3, "Eb": 3,
    "E": 4, "Fb": 4, "E#": 5, "F": 5, "F#": 6, "Gb": 6,
    "G": 7, "G#": 8, "Ab": 8, "A": 9, "A#": 10, "Bb": 10,
    "B": 11, "Cb": 11, "B#": 0,
}
NOTE_NAME_TO_PITCH_CLASS = NOTE_NAME_TO_PC  # Alias


def midi_to_pitch_octave(midi: int) -> tuple[int, int]:
    """Convert MIDI note to (pitch_class, octave)."""
    return midi % 12, midi // 12 - 1


def pitch_octave_to_midi(pc: int, octave: int) -> int:
    """Convert pitch class and octave to MIDI note."""
    return (octave + 1) * 12 + pc


def note_name_to_midi(name: str, octave: int) -> int:
    """Convert note name and octave to MIDI note."""
    # Extract note and accidental
    if len(name) > 1 and name[1] in "#b":
        note = name[:2]
    else:
        note = name[0]
    pc = NOTE_NAME_TO_PC.get(note.upper(), 0)
    return pitch_octave_to_midi(pc, octave)


def closest_pitch_class_midi(reference: int, target_pc: int) -> int:
    """Find MIDI note with target_pc closest to reference."""
    if NATIVE_AVAILABLE:
        return sunny_native.closest_pitch_class_midi(reference, target_pc)
    current_pc = reference % 12
    diff = (target_pc - current_pc + 6) % 12 - 6
    result = reference + diff
    return max(0, min(127, result))


# Euclidean rhythm
def euclidean_rhythm(pulses: int, steps: int, rotation: int = 0) -> list[bool]:
    """Generate Euclidean rhythm pattern."""
    if NATIVE_AVAILABLE:
        return sunny_native.euclidean_rhythm(pulses, steps, rotation)

    # Fallback: Bresenham's algorithm
    if pulses <= 0 or steps <= 0 or pulses > steps:
        raise ValueError("Invalid Euclidean parameters")

    pattern = [False] * steps
    for i in range(steps):
        if (i * pulses) % steps < pulses:
            pattern[i] = True

    # Apply rotation
    if rotation != 0:
        rotation = rotation % steps
        pattern = pattern[rotation:] + pattern[:rotation]

    return pattern


__all__ = [
    # Backend info
    "is_native_available",
    "get_backend_info",
    "NATIVE_AVAILABLE",
    # Constants
    "MIDI_NOTE_MIN",
    "MIDI_NOTE_MAX",
    "VELOCITY_MIN",
    "VELOCITY_MAX",
    "MIDI_VELOCITY_MIN",
    "MIDI_VELOCITY_MAX",
    "PITCH_CLASS_COUNT",
    "TEMPO_MIN_BPM",
    "TEMPO_MAX_BPM",
    "BEAT_EPSILON",
    "LCM_MAX_DENOMINATOR",
    "PITCH_CLASS_NAMES_SHARP",
    "PITCH_CLASS_NAMES_FLAT",
    "NOTE_NAME_TO_PC",
    "NOTE_NAME_TO_PITCH_CLASS",
    # Type aliases
    "PitchClass",
    "MidiNote",
    "Velocity",
    "Interval",
    # Validators
    "is_valid_pitch_class",
    "is_valid_midi_note",
    "is_valid_velocity",
    # Pitch operations
    "pitch_class",
    "transpose",
    "invert",
    "interval_class",
    "note_name",
    "midi_to_pitch_octave",
    "pitch_octave_to_midi",
    "note_name_to_midi",
    "closest_pitch_class_midi",
    # Rhythm
    "euclidean_rhythm",
]
