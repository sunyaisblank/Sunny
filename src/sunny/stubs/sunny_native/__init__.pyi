"""Type stubs for sunny_native C++ extension module.

This module provides high-performance music theory computation.
"""

from enum import IntEnum
from typing import List, Set, Dict, Any

__version__: str

# =============================================================================
# Error Codes
# =============================================================================

class ErrorCode(IntEnum):
    Ok = 0
    InvalidMidiNote = 1
    InvalidPitchClass = 2
    InvalidScaleName = 3
    InvalidRomanNumeral = 4
    ScaleGenerationFailed = 5
    ChordGenerationFailed = 6
    VoiceLeadingFailed = 7
    EuclideanInvalidParams = 8

# =============================================================================
# Beat Type
# =============================================================================

class Beat:
    numerator: int
    denominator: int

    def __init__(self, numerator: int, denominator: int) -> None: ...
    def to_float(self) -> float: ...
    @staticmethod
    def from_float(beats: float, max_denom: int = 10000) -> Beat: ...
    @staticmethod
    def zero() -> Beat: ...
    @staticmethod
    def one() -> Beat: ...
    def __repr__(self) -> str: ...

# =============================================================================
# ChordVoicing
# =============================================================================

class ChordVoicing:
    notes: List[int]
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
    voiced_notes: List[int]
    total_motion: int
    has_parallel_fifths: bool
    has_parallel_octaves: bool

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

def pcs_transpose(pcs: Set[int], n: int) -> Set[int]:
    """Transpose pitch class set."""
    ...

def pcs_invert(pcs: Set[int], axis: int = 0) -> Set[int]:
    """Invert pitch class set."""
    ...

def pcs_interval_vector(pcs: Set[int]) -> List[int]:
    """Get interval vector."""
    ...

# =============================================================================
# Scale Operations
# =============================================================================

def generate_scale_notes(root_pc: int, intervals: List[int], octave: int) -> List[int]:
    """Generate scale MIDI notes."""
    ...

def is_note_in_scale(note: int, root_pc: int, intervals: List[int]) -> bool:
    """Check if note is in scale."""
    ...

def quantize_to_scale(note: int, root_pc: int, intervals: List[int]) -> int:
    """Quantize note to scale."""
    ...

# =============================================================================
# Rhythm Operations
# =============================================================================

def euclidean_rhythm(pulses: int, steps: int, rotation: int = 0) -> List[bool]:
    """Generate Euclidean rhythm pattern."""
    ...

def euclidean_preset(name: str) -> List[bool]:
    """Get named Euclidean preset."""
    ...

# =============================================================================
# Harmony Operations
# =============================================================================

def negative_harmony(chord_pcs: Set[int], key_root: int) -> Set[int]:
    """Apply negative harmony transformation."""
    ...

def generate_chord_from_numeral(
    numeral: str,
    key_root: int,
    scale_intervals: List[int],
    octave: int = 4
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
    source_pitches: List[int],
    target_pitch_classes: List[int],
    lock_bass: bool = False,
    allow_parallel_fifths: bool = False,
    allow_parallel_octaves: bool = False
) -> VoiceLeadingResult:
    """Compute optimal voice leading."""
    ...

def generate_close_voicing(pitch_classes: List[int], root_octave: int = 4) -> List[int]:
    """Generate close voicing."""
    ...

def generate_drop2_voicing(close_voicing: List[int]) -> List[int]:
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
    def reset(self) -> None: ...
    def process(self, sample_rate: float) -> float: ...
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
    def value(self) -> float: ...
    def state(self) -> EnvelopeState: ...
    def is_active(self) -> bool: ...

class SampleAndHold:
    def __init__(self) -> None: ...
    def trigger(self, input: float) -> float: ...
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
    def set_notes(self, notes: List[int]) -> None: ...
    def clear(self) -> None: ...
    def generate_pattern(self) -> List[int]: ...
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
    octaves: int = 1
) -> List[Dict[str, Any]]:
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
    ticks: int
    ppq: int
    tempo_bpm: float

    def to_beats(self) -> float: ...
    def to_seconds(self) -> float: ...

class Transport:
    def __init__(self, ppq: int = 480) -> None: ...
    def play(self) -> None: ...
    def stop(self) -> None: ...
    def pause(self) -> None: ...
    def set_tempo(self, bpm: float) -> None: ...
    def set_position(self, ticks: int) -> None: ...
    def state(self) -> TransportState: ...
    def position(self) -> TransportPosition: ...
    def tempo(self) -> float: ...
    def is_playing(self) -> bool: ...
    def schedule_note(
        self, tick: int, pitch: int, duration: int, velocity: int = 100
    ) -> None: ...
    def clear_scheduled(self) -> None: ...
    def advance(self, ticks: int) -> None: ...

# =============================================================================
# Infrastructure: Session
# =============================================================================

class ConnectionState(IntEnum):
    Disconnected = 0
    Connecting = 1
    Connected = 2
    Reconnecting = 3
    Error = 4

class SessionMode(IntEnum):
    Idle = 0
    Playing = 1
    Recording = 2
    Overdubbing = 3

class SessionStateMachine:
    def __init__(self) -> None: ...
    def set_connected(self) -> None: ...
    def set_disconnected(self, reason: str = "") -> None: ...
    def set_connecting(self) -> None: ...
    def set_error(self, error: str) -> None: ...
    def connection_state(self) -> ConnectionState: ...
    def is_connected(self) -> bool: ...
    def set_mode(self, mode: SessionMode) -> None: ...
    def mode(self) -> SessionMode: ...
    def start_playing(self) -> None: ...
    def stop_playing(self) -> None: ...
    def start_recording(self) -> None: ...
    def stop_recording(self) -> None: ...
    def connection_state_string(self) -> str: ...
    def mode_string(self) -> str: ...

# =============================================================================
# Infrastructure: Orchestrator
# =============================================================================

class BridgeMessageType(IntEnum):
    QueryProperty = 0
    SetProperty = 1
    CallMethod = 2
    CreateClip = 3
    AddNotes = 4
    Batch = 5

class BridgeMessage:
    type: BridgeMessageType
    path: str
    args: List[Any]

class OrchestratorResult:
    success: bool
    operation_id: int
    message: str

class Orchestrator:
    def __init__(self) -> None: ...
    def create_progression_clip(
        self,
        track_index: int,
        slot_index: int,
        root: str,
        scale: str,
        numerals: List[str],
        octave: int = 4,
        duration_beats: float = 4.0
    ) -> OrchestratorResult: ...
    def apply_euclidean_rhythm(
        self,
        track_index: int,
        slot_index: int,
        pulses: int,
        steps: int,
        pitch: int,
        step_duration: float = 0.25
    ) -> OrchestratorResult: ...
    def apply_arpeggio(
        self,
        track_index: int,
        slot_index: int,
        numerals: List[str],
        direction: ArpDirection,
        step_duration: float = 0.25
    ) -> OrchestratorResult: ...
    def undo(self) -> bool: ...
    def redo(self) -> bool: ...
    def can_undo(self) -> bool: ...
    def can_redo(self) -> bool: ...
    def clear_history(self) -> None: ...
    def drain_messages(self) -> List[BridgeMessage]: ...
    def pending_message_count(self) -> int: ...
    def set_max_undo_levels(self, levels: int) -> None: ...
