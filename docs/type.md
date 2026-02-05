# Type System

**Document ID:** SUNNY-DOC-TYPE-001
**Version:** 1.0
**Date:** February 2026
**Status:** Active

---

## Overview

This document defines the type system for Sunny. Types encode invariants, units, and meaning. A well-designed type system prevents classes of errors at compile time and makes code self-documenting.

Read this document after [foundations.md](./foundations.md) and before [standard.md](./standard.md).

---

## Part I: First Principles

### 1.1 What Is a Type?

A type is a set of values with associated operations. Types encode:

**Invariants.** Properties that always hold for values of the type. A `MidiNote` is always in [0, 127]; attempting to construct an invalid value is an error.

**Units.** Physical or conceptual dimensions. A `Duration` is measured in beats, not seconds. A `Tempo` is beats per minute, not beats per second.

**Meaning.** Semantic intent beyond raw representation. Both `TrackIndex` and `MidiNote` are integers, but they are not interchangeable.

### 1.2 Type Categories

Sunny uses four categories of types:

| Category | Purpose | Example |
|----------|---------|---------|
| **Primitives** | Language-level values | `int`, `float`, `str`, `bool` |
| **Type Aliases** | Semantic intent for primitives | `MidiNote`, `Tempo`, `Duration` |
| **Named Tuples** | Immutable structured data | `NoteEvent`, `ChordVoicing`, `TimeSignature` |
| **Dataclasses** | Mutable structured state | `SessionInfo`, `TrackInfo`, `ClipInfo` |

### 1.3 Immutability Preference

Prefer immutable types. Immutability provides:

- **Thread safety.** No synchronization required.
- **Referential transparency.** Functions can be memoized.
- **Debugging clarity.** Values don't change unexpectedly.

Use mutable dataclasses only for state that genuinely changes over time (session state, connection health).

---

## Part II: Primitive Types and Constraints

### 2.1 Integer Types

| Type Alias | Range | Unit | Description |
|------------|-------|------|-------------|
| `MidiNote` | [0, 127] | semitones from C-1 | MIDI note number |
| `PitchClass` | [0, 11] | semitones from C | Note within octave (0=C, 11=B) |
| `Velocity` | [1, 127] | dimensionless | MIDI velocity (loudness) |
| `Interval` | [-127, 127] | semitones | Signed interval between pitches |

### 2.2 Float Types

| Type Alias | Range | Unit | Precision |
|------------|-------|------|-----------|
| `BeatPosition` | [0, ∞) | beats | BEAT_EPSILON (1e-9) |
| `Duration` | (0, ∞) | beats | DURATION_EPSILON (1e-6) |
| `Tempo` | [20, 999] | BPM | 0.01 BPM |
| `Frequency` | (0, ∞) | Hz | TOLERANCE_FREQUENCY_RELATIVE (1e-6) |

### 2.3 String Types

| Type | Pattern | Description |
|------|---------|-------------|
| Note Name | `[A-G][#b]?` | Scientific pitch name without octave |
| Scale Name | `[a-z_]+` | Lowercase with underscores |
| Roman Numeral | `[ivIV]+[0-9]*` | Chord degree notation |

---

## Part III: Coordinate Systems

### 3.1 Pitch Representations

Sunny uses three coordinate systems for pitch:

**MIDI Numbers.** Integer representation for computational operations.
- Range: 0-127
- Middle C (C4) = 60
- Formula: `midi = 12 * octave + pitch_class + 12`

**Scientific Pitch.** String representation for human readability.
- Format: `<note><accidental><octave>` (e.g., "C#4", "Bb3")
- Accidentals: `#` (sharp), `b` (flat)

**Frequency.** Physical representation in Hertz.
- Formula: `f = 440 * 2^((midi - 69) / 12)`
- A4 (MIDI 69) = 440 Hz by convention

**Conversion invariants:**
```
midi_to_frequency(frequency_to_midi(f)) ≈ f  (within TOLERANCE_FREQUENCY_RELATIVE)
parse_note(format_note(midi)) == midi
```

### 3.2 Time Representations

**Beats.** Primary internal representation.
- Zero-indexed from clip/arrangement start
- Fractional beats for sub-beat positions
- Duration is always positive

**Seconds.** Physical time (requires tempo conversion).
- Formula: `seconds = beats * 60 / tempo`
- Not used internally; only for external APIs

**Samples.** Audio sample positions (requires sample rate).
- Formula: `samples = seconds * sample_rate`
- Used only by transport layer

**Conversion invariants:**
```
beats_to_seconds(seconds_to_beats(s, tempo), tempo) ≈ s
```

### 3.3 Parameter Normalization

All continuous device parameters are normalized to [0, 1]:

```
normalized = (value - min) / (max - min)
value = min + normalized * (max - min)
```

This abstraction allows uniform parameter control regardless of underlying range.

---

## Part IV: Structured Types

### 4.1 NoteEvent

The fundamental unit of melodic/rhythmic content.

```python
class NoteEvent(NamedTuple):
    pitch: MidiNote      # [0, 127]
    start_time: BeatPosition  # [0, ∞)
    duration: Duration   # (0, ∞)
    velocity: Velocity = 100  # [1, 127]
```

**Invariants:**
- `pitch` is a valid MIDI note
- `start_time >= 0`
- `duration > 0`
- `velocity >= 1` (velocity 0 is note-off)

### 4.2 TimeSignature

Meter representation.

```python
class TimeSignature(NamedTuple):
    numerator: int       # [1, 32] beats per measure
    denominator: int     # ∈ {1, 2, 4, 8, 16, 32} note value
```

**Invariants:**
- `numerator >= 1`
- `denominator` is a power of 2

**Semantics:**
- Bar length in beats = `numerator * (4 / denominator)`
- 4/4 = 4 beats; 6/8 = 3 beats; 5/4 = 5 beats

### 4.3 ChordVoicing

A chord as realized MIDI notes.

```python
class ChordVoicing(NamedTuple):
    notes: tuple[MidiNote, ...]  # Ascending order
    root: PitchClass             # [0, 11]
    quality: ChordQuality        # Major, minor, etc.
    inversion: int = 0           # [0, len(notes)-1]
```

**Invariants:**
- `notes` is sorted ascending
- `root` corresponds to the chord's theoretical root
- `inversion` specifies which note is in the bass

### 4.4 ProgressionChord

A chord in harmonic context.

```python
class ProgressionChord(NamedTuple):
    numeral: str         # Roman numeral (e.g., "ii", "V7")
    root: str            # Note name (e.g., "D", "G")
    quality: str         # Chord quality
    notes: tuple[MidiNote, ...]  # Voicing
```

**Usage:** Represents chords within a key, enabling functional analysis.

---

## Part V: State Types

### 5.1 SessionInfo

Current Ableton session state.

```python
@dataclass
class SessionInfo:
    tempo: Tempo
    time_signature: TimeSignature
    is_playing: bool
    track_count: dict[str, int]
    current_time: BeatPosition
```

**Lifecycle:** Mutable; updated on session changes.

### 5.2 TrackInfo

Information about a single track.

```python
@dataclass
class TrackInfo:
    index: int
    name: str
    track_type: TrackType
    color: int
    volume: float        # [0, 1]
    pan: float           # [-1, 1]
    muted: bool
    solo: bool
    armed: bool
```

### 5.3 ClipInfo

Information about a clip slot.

```python
@dataclass
class ClipInfo:
    track_index: int
    slot_index: int
    name: str
    length: Duration
    color: int
    is_looping: bool
    loop_start: BeatPosition
    loop_end: BeatPosition
```

---

## Part VI: Enumerations

### 6.1 Musical Enums

| Enum | Values | Usage |
|------|--------|-------|
| `ChordQuality` | major, minor, diminished, augmented, dominant, ... | Chord classification |
| `ScaleType` | major, minor, dorian, phrygian, ... | Scale selection |
| `NoteValue` | 1, 2, 4, 8, 16, 32, 8t, 16t | Rhythmic subdivision |

### 6.2 System Enums

| Enum | Values | Usage |
|------|--------|-------|
| `TrackType` | midi, audio, return, master, group | Track classification |
| `DeviceType` | instrument, audio_effect, midi_effect | Device classification |
| `TrustLevel` | untrusted, local, authenticated, admin | Security levels |

### 6.3 Functional Harmony

| Function | Scale Degrees | Characteristic |
|----------|---------------|----------------|
| Tonic (T) | I, vi, iii | Stability, rest |
| Subdominant (S) | IV, ii | Departure, preparation |
| Dominant (D) | V, vii° | Tension, resolution demand |

---

## Part VII: Error Types

### 7.1 Error Code Ranges

| Range | Category | Example |
|-------|----------|---------|
| 1xxx | Connection/Transport | CONNECTION_FAILED, TCP_TIMEOUT |
| 2xxx | Validation | INVALID_MIDI_NOTE, MISSING_PARAM |
| 3xxx | Theory | SCALE_GENERATION_FAILED |
| 4xxx | Session/Track | TRACK_NOT_FOUND, CLIP_CREATE_FAILED |
| 5xxx | Device | DEVICE_NOT_FOUND, PARAMETER_READ_ONLY |
| 6xxx | Security | RATE_LIMIT_EXCEEDED, UNAUTHORIZED |
| 7xxx | Snapshot | SNAPSHOT_NOT_FOUND |
| 9xxx | Internal | INTERNAL_ERROR, NOT_IMPLEMENTED |

### 7.2 Exception Hierarchy

```
SunnyError (base)
├── ConnectionError (1xxx)
├── ValidationError (2xxx)
├── TheoryError (3xxx)
├── SessionError (4xxx)
├── DeviceError (5xxx)
├── SecurityError (6xxx)
└── SnapshotError (7xxx)
```

---

## Part VIII: Precision and Tolerances

### 8.1 Machine Precision

| Constant | Value | Usage |
|----------|-------|-------|
| FLOAT64_EPSILON | 2.22e-16 | Double precision unit roundoff |
| FLOAT32_EPSILON | 1.19e-7 | Single precision unit roundoff |

### 8.2 Domain Tolerances

| Constant | Value | Usage |
|----------|-------|-------|
| BEAT_EPSILON | 1e-9 | Beat position equality |
| DURATION_EPSILON | 1e-6 | Duration positivity check |
| TOLERANCE_FREQUENCY_RELATIVE | 1e-6 | Frequency comparison |
| TOLERANCE_PARAMETER_NORMALIZED | 1e-4 | OSC/MIDI parameter roundtrip |

### 8.3 Comparison Functions

```python
def beats_equal(a: float, b: float) -> bool:
    """Test if two beat positions are equal within tolerance."""
    return abs(a - b) < BEAT_EPSILON

def duration_positive(d: float) -> bool:
    """Test if duration is meaningfully positive."""
    return d > DURATION_EPSILON
```

---

## Part IX: Type Conversion Reference

### 9.1 Pitch Conversions

```python
def midi_to_pitch_class(midi: int) -> int:
    """MIDI note to pitch class (0-11)."""
    return midi % 12

def midi_to_octave(midi: int) -> int:
    """MIDI note to octave (-1 to 9)."""
    return (midi // 12) - 1

def midi_to_frequency(midi: int) -> float:
    """MIDI note to frequency in Hz (A4=440)."""
    return 440.0 * (2.0 ** ((midi - 69) / 12.0))

def frequency_to_midi(freq: float) -> int:
    """Frequency to nearest MIDI note."""
    return round(69 + 12 * log2(freq / 440.0))
```

### 9.2 Time Conversions

```python
def beats_to_seconds(beats: float, tempo: float) -> float:
    """Convert beats to seconds at given tempo."""
    return beats * 60.0 / tempo

def seconds_to_beats(seconds: float, tempo: float) -> float:
    """Convert seconds to beats at given tempo."""
    return seconds * tempo / 60.0
```

---

## Conclusion

The type system encodes domain knowledge in the type definitions themselves. A `MidiNote` cannot be negative; a `Duration` cannot be zero; a `TimeSignature` denominator is always a power of two.

These constraints, enforced at the boundary where values enter the system, allow internal code to assume validity. The result is clearer code with fewer defensive checks and better error messages at the source of invalid data.

*That is the type system of Sunny.*
