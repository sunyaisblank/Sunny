"""Type stubs for sunny_native C++ extension module.

This module provides high-performance music theory computation.
"""

from enum import IntEnum
from typing import Any, Final

__version__: Final[str]
ABLETON_BRIDGE_PROTOCOL_VERSION: Final[int]
ABLETON_TARGET_SNAPSHOT_SCHEMA_VERSION: Final[int]

def validate_ableton_target_snapshot_json(payload: str) -> bool:
    """Validate one closed, versioned Ableton target snapshot JSON value."""
    ...

# =============================================================================
# Error Codes
# =============================================================================

class ErrorCode(IntEnum):
    Ok = 0
    InvalidMidiNote = 2100
    InvalidPitchClass = 2102
    InvalidBeat = 2137
    InvalidScaleName = 2110
    InvalidRomanNumeral = 2113
    ScaleGenerationFailed = 3100
    ChordGenerationFailed = 3101
    VoiceLeadingFailed = 3110
    EuclideanInvalidParams = 3121
    RenderInvalidParameter = 3600
    RenderInvalidSampleRate = 3601
    RenderEmptyPattern = 3602
    RenderInvalidPPQ = 3603
    RenderInvalidPosition = 3604
    RenderUnrepresentableTiming = 3605
    RenderInvalidBlockSize = 3606
    RenderInvalidSignalBuffer = 3607
    RenderInvalidChannelCount = 3608
    RenderControlQueueFull = 3609
    RenderNotConfigured = 3610
    RenderEventQueueFull = 3611
    RenderEventBufferFull = 3612

# =============================================================================
# Beat Type
# =============================================================================

class Beat:
    def __init__(self, numerator: int, denominator: int) -> None: ...
    @property
    def numerator(self) -> int: ...
    @property
    def denominator(self) -> int: ...
    def to_float(self) -> float: ...
    @staticmethod
    def from_float(beats: float, max_denom: int = 10000) -> Beat: ...
    @staticmethod
    def zero() -> Beat: ...
    @staticmethod
    def one() -> Beat: ...
    def __repr__(self) -> str: ...

# =============================================================================
# Note Event (read-only bridge/render value)
# =============================================================================

class NoteEvent:
    @property
    def pitch(self) -> int: ...
    @property
    def start_time(self) -> Beat: ...
    @property
    def duration(self) -> Beat: ...
    @property
    def velocity(self) -> int: ...
    @property
    def muted(self) -> bool: ...
    @property
    def release_velocity(self) -> int: ...
    def end_time(self) -> Beat: ...

# =============================================================================
# ChordVoicing
# =============================================================================

class ChordVoicing:
    notes: list[int]
    root: int
    quality: str
    inversion: int

    def __init__(self) -> None: ...
    def empty(self) -> bool: ...
    def __len__(self) -> int: ...

# =============================================================================
# VoiceLeadingResult
# =============================================================================

class VoiceLeadingResult:
    @property
    def voiced_notes(self) -> list[int]: ...
    @property
    def total_motion(self) -> int: ...
    @property
    def has_parallel_fifths(self) -> bool: ...
    @property
    def has_parallel_octaves(self) -> bool: ...

# =============================================================================
# Pitch Operations
# =============================================================================

def pitch_class(midi: int) -> int:
    """Get pitch class from MIDI note."""
    ...

def transpose(pc: int, interval: int) -> int:
    """Transpose pitch class."""
    ...

def invert(pc: int, axis: int = 0) -> int:
    """Invert pitch class."""
    ...

def interval_class(semitones: int) -> int:
    """Get interval class [0-6]."""
    ...

def note_name(pc: int, prefer_flats: bool = False) -> str:
    """Get note name."""
    ...

def note_to_pitch_class(name: str) -> int:
    """Parse note name to pitch class."""
    ...

def closest_pitch_class_midi(reference: int, target_pc: int) -> int:
    """Find closest MIDI note with target pitch class."""
    ...

# =============================================================================
# Pitch Class Set Operations
# =============================================================================

def pcs_transpose(pcs: set[int], n: int) -> set[int]:
    """Transpose pitch class set."""
    ...

def pcs_invert(pcs: set[int], axis: int = 0) -> set[int]:
    """Invert pitch class set."""
    ...

def pcs_interval_vector(pcs: set[int]) -> list[int]:
    """Get interval vector."""
    ...

# =============================================================================
# Scale Operations
# =============================================================================

def generate_scale_notes(root_pc: int, intervals: list[int], octave: int) -> list[int]:
    """Generate scale MIDI notes."""
    ...

def is_note_in_scale(note: int, root_pc: int, intervals: list[int]) -> bool:
    """Check if note is in scale."""
    ...

def quantize_to_scale(note: int, root_pc: int, intervals: list[int]) -> int:
    """Quantize note to scale."""
    ...

def list_scale_names() -> list[str]:
    """List all built-in scale names."""
    ...

def scale_intervals(name: str) -> list[int]:
    """Get semitone intervals of a built-in scale."""
    ...

# =============================================================================
# Rhythm Operations
# =============================================================================

def euclidean_rhythm(pulses: int, steps: int, rotation: int = 0) -> list[bool]:
    """Generate Euclidean rhythm pattern."""
    ...

def euclidean_preset(name: str) -> list[bool]:
    """Get named Euclidean preset."""
    ...

# =============================================================================
# Harmony Operations
# =============================================================================

def negative_harmony(chord_pcs: set[int], key_root: int) -> set[int]:
    """Apply negative harmony transformation."""
    ...

def generate_chord_from_numeral(
    numeral: str, key_root: int, scale_intervals: list[int], octave: int = 4
) -> ChordVoicing:
    """Generate chord from Roman numeral."""
    ...

def generate_chord(root: int, quality: str, octave: int = 4) -> ChordVoicing:
    """Generate chord from root and quality."""
    ...

# =============================================================================
# Voice Leading
# =============================================================================

def voice_lead_nearest_tone(
    source_pitches: list[int],
    target_pitch_classes: list[int],
    lock_bass: bool = False,
    allow_parallel_fifths: bool = False,
    allow_parallel_octaves: bool = False,
) -> VoiceLeadingResult:
    """Compute optimal voice leading."""
    ...

def generate_close_voicing(pitch_classes: list[int], root_octave: int = 4) -> list[int]:
    """Generate close voicing."""
    ...

def generate_drop2_voicing(close_voicing: list[int]) -> list[int]:
    """Generate drop-2 voicing."""
    ...

# =============================================================================
# Render: Modulation
# =============================================================================

class LfoWaveform(IntEnum):
    Sine = 0
    Triangle = 1
    Saw = 2
    Square = 3
    Random = 4

class Lfo:
    def __init__(self) -> None: ...
    def set_frequency(self, hz: float) -> None: ...
    def set_waveform(self, waveform: LfoWaveform) -> None: ...
    def set_phase(self, phase: float) -> None: ...
    def set_seed(self, seed: int) -> None: ...
    def reset(self) -> None: ...
    def process(self, sample_rate: float) -> float: ...
    def process_block(self, sample_rate: float, frame_count: int) -> list[float]: ...
    def value(self) -> float: ...

class EnvelopeState(IntEnum):
    Idle = 0
    Attack = 1
    Decay = 2
    Sustain = 3
    Release = 4

class Envelope:
    def __init__(self) -> None: ...
    def set_attack(self, seconds: float) -> None: ...
    def set_decay(self, seconds: float) -> None: ...
    def set_sustain(self, level: float) -> None: ...
    def set_release(self, seconds: float) -> None: ...
    def trigger(self) -> None: ...
    def release(self) -> None: ...
    def reset(self) -> None: ...
    def process(self, sample_rate: float) -> float: ...
    def process_block(self, sample_rate: float, frame_count: int) -> list[float]: ...
    def value(self) -> float: ...
    def state(self) -> EnvelopeState: ...
    def is_active(self) -> bool: ...

class SampleAndHold:
    def __init__(self) -> None: ...
    def trigger(self, input: float) -> None: ...
    def process_block(self, sample_rate: float, frame_count: int) -> list[float]: ...
    def value(self) -> float: ...
    def reset(self) -> None: ...

# =============================================================================
# Render: Arpeggio
# =============================================================================

class ArpDirection(IntEnum):
    Up = 0
    Down = 1
    UpDown = 2
    DownUp = 3
    Random = 4
    Order = 5

class Arpeggiator:
    def __init__(self) -> None: ...
    def set_direction(self, direction: ArpDirection) -> None: ...
    def set_octave_range(self, octaves: int) -> None: ...
    def set_gate(self, gate: float) -> None: ...
    def set_seed(self, seed: int) -> None: ...
    def set_notes(self, notes: list[int]) -> None: ...
    def clear(self) -> None: ...
    def generate_pattern(self) -> list[int]: ...
    def reset(self) -> None: ...
    def next(self) -> int: ...
    def current(self) -> int: ...
    def step(self) -> int: ...
    def pattern_length(self) -> int: ...
    def direction(self) -> ArpDirection: ...
    def octave_range(self) -> int: ...
    def gate(self) -> float: ...

def generate_arpeggio(
    voicing: ChordVoicing,
    direction: ArpDirection,
    step_duration: float = 0.25,
    gate: float = 0.5,
    octaves: int = 1,
) -> list[dict[str, Any]]:
    """Generate arpeggio note events."""
    ...

# =============================================================================
# Render: Transport
# =============================================================================

class TransportState(IntEnum):
    Stopped = 0
    Playing = 1
    Paused = 2
    Recording = 3

class TransportPosition:
    @property
    def ticks(self) -> int: ...
    @property
    def ppq(self) -> int: ...
    @property
    def tempo_bpm(self) -> float: ...
    def to_beats(self) -> float: ...
    def to_quarter_notes(self) -> float: ...
    def to_seconds(self) -> float: ...

class Transport:
    def __init__(self, ppq: int = 480) -> None: ...
    def play(self) -> None: ...
    def record(self) -> None: ...
    def stop(self) -> None: ...
    def pause(self) -> None: ...
    def set_tempo(self, bpm: float) -> None: ...
    def set_position(self, ticks: int) -> None: ...
    def state(self) -> TransportState: ...
    def position(self) -> TransportPosition: ...
    def tempo(self) -> float: ...
    def is_playing(self) -> bool: ...
    def is_running(self) -> bool: ...
    def schedule_note(
        self,
        tick: int,
        pitch: int,
        duration: float,
        velocity: int = 100,
        release_velocity: int = 64,
    ) -> None: ...
    def clear_scheduled(self) -> None: ...
    def advance(self, ticks: int) -> None: ...
    def process_block(self, sample_count: int, sample_rate: float) -> None: ...

# =============================================================================
# Infrastructure: Orchestrator
# =============================================================================

class BridgeMessageType(IntEnum):
    GetProperty = 0
    SetProperty = 1
    CallMethod = 2
    CreateClip = 3
    AddNotes = 4
    Batch = 5

class BridgeMessage:
    @property
    def type(self) -> BridgeMessageType: ...
    @property
    def path(self) -> str: ...
    @property
    def args(self) -> list[str]: ...
    @property
    def notes(self) -> list[NoteEvent]: ...

class OrchestratorResult:
    @property
    def success(self) -> bool: ...
    @property
    def operation_id(self) -> str: ...
    @property
    def message(self) -> str: ...

class Orchestrator:
    def __init__(self) -> None: ...
    def create_progression_clip(
        self,
        track_index: int,
        slot_index: int,
        root: str,
        scale: str,
        numerals: list[str],
        octave: int = 4,
        duration_beats: float = 4.0,
    ) -> OrchestratorResult: ...
    def apply_euclidean_rhythm(
        self,
        track_index: int,
        slot_index: int,
        pulses: int,
        steps: int,
        pitch: int,
        step_duration: float = 0.25,
    ) -> OrchestratorResult: ...
    def apply_arpeggio(
        self,
        track_index: int,
        slot_index: int,
        numerals: list[str],
        direction: str,
        step_duration: float = 0.25,
    ) -> OrchestratorResult: ...
    def undo(self) -> bool: ...
    def redo(self) -> bool: ...
    def can_undo(self) -> bool: ...
    def can_redo(self) -> bool: ...
    def clear_history(self) -> None: ...
    def drain_messages(self) -> list[BridgeMessage]: ...
    def pending_message_count(self) -> int: ...
    def set_max_undo_levels(self, levels: int) -> None: ...
