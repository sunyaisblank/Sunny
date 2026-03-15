# Sunny — Type System

**Document ID:** SUNNY-DOC-TYPE-001
**Version:** 2.0
**Date:** March 2026
**Status:** Active

---

## Overview

This document defines the type system for the Sunny C++23 music theory engine. Types encode invariants, units, and meaning at compile time. The type system uses `std::expected` for fallible operations, exact rational arithmetic for pitch and rhythm, and tagged identifiers to prevent cross-domain substitution.

---

## Part I: First Principles

### 1.1 What a Type Encodes

**Invariants.** Properties that always hold for values of the type. A `PitchClass` is always in [0, 11]; a `Beat` always has a nonzero denominator.

**Units.** Dimensional correctness. A `Beat` measures duration in whole-note fractions; a `PositiveRational` measures rate (BPM). Both are rational numbers, but they are not interchangeable because rate and duration compose differently.

**Meaning.** Semantic intent beyond raw representation. `PartId` and `EventId` both wrap `uint64_t`, but the `Id<T>` template tag prevents comparison between them.

### 1.2 Design Decisions

**Exact arithmetic over floating-point.** Pitch intervals and rhythmic durations use integer or rational types. No floating-point appears in the pitch or rhythm paths. This eliminates accumulation error in beat offset calculations and ensures that serialisation round-trips are exact.

**Tagged identifiers over raw integers.** The `Id<T>` template carries a phantom tag type that the compiler checks at every use site. Two identifiers with different tags cannot be compared, assigned, or passed to the wrong function, even though both are `uint64_t` at runtime.

**`std::expected` over exceptions.** All fallible operations return `Result<T>` (aliased to `std::expected<T, ErrorCode>`). The caller must inspect the result before accessing the value. No exceptions cross module boundaries.

---

## Part II: Primitive Types

### 2.1 Pitch Types

| Type | C++ type | Range | Description |
|------|----------|-------|-------------|
| `PitchClass` | `uint8_t` | [0, 11] | Chromatic pitch class (0 = C) |
| `MidiNote` | `uint8_t` | [0, 127] | MIDI note number |
| `Interval` | `int8_t` | [-127, 127] | Chromatic semitone displacement |

`PitchClass` is a type alias, not an enum. Use integer constants and arithmetic directly.

### 2.2 Spelled Pitch

```cpp
struct SpelledPitch {
    uint8_t letter;     // 0=C, 1=D, 2=E, 3=F, 4=G, 5=A, 6=B
    int8_t accidental;  // negative=flats, 0=natural, positive=sharps
    int8_t octave;      // -1..9 for MIDI-bounded
};
```

SpelledPitch preserves enharmonic spelling (C# and Db are distinct). It has no ordering operators; use `midi_value()` when range comparison is needed. Construction from scientific pitch notation uses `from_spn("C#4")`.

### 2.3 Diatonic Interval

```cpp
struct DiatonicInterval {
    int chromatic;   // Semitone displacement (unbounded)
    int diatonic;    // Letter-name displacement (unbounded)
};
```

The pair `(chromatic, diatonic)` uniquely determines interval quality (major, minor, perfect, augmented, diminished). Diatonic intervals compose correctly under `interval_add` and invert under `interval_invert`.

---

## Part III: Temporal Types

### 3.1 Beat (Exact Rational)

```cpp
struct Beat {
    int numerator;
    int denominator;  // must be > 0
};
```

Beat represents duration or offset as an exact fraction of a whole note. Quarter note = `Beat{1, 4}`. Eighth note = `Beat{1, 8}`.

**Construction:** `Beat{2, 1}` for two whole notes. Do not use `Beat(2)` — the single-argument form sets denominator to 0 (aggregate initialisation leaves the second field zero-initialised).

**Arithmetic:** `Beat::zero()` returns `{0, 1}`. Addition, subtraction, multiplication by integer, and comparison operators are provided. `to_float()` returns the value in whole-note units; multiply by 4 when dividing by quarter-note BPM.

### 3.2 ScoreTime (Hierarchical Position)

```cpp
struct ScoreTime {
    uint32_t bar;  // 1-indexed
    Beat beat;     // offset within bar, non-negative
};
```

Bar 1 is the first bar. `SCORE_START` is `{1, Beat::zero()}`. Comparison operators use bar first, then beat.

### 3.3 PositiveRational (Rate)

```cpp
struct PositiveRational {
    int64_t numerator;   // > 0
    int64_t denominator; // > 0
};
```

Used for tempo (BPM). Kept distinct from `Beat` to prevent rate-duration confusion. `make_bpm(120)` constructs `{120, 1}`.

### 3.4 Temporal Conversion Chain

```
ScoreTime → AbsoluteBeat → RealTime (seconds) → TickTime (MIDI ticks)
```

Each conversion is bijective within the score's tempo and time signature maps. The default PPQ is 480. Tick quantisation uses Bresenham rounding to distribute rounding error evenly.

---

## Part IV: Identifier Types

### 4.1 Id Template

```cpp
template <typename T>
struct Id {
    uint64_t value;
    constexpr bool operator==(const Id&) const noexcept = default;
    constexpr auto operator<=>(const Id&) const noexcept = default;
};
```

The tag type `T` is never instantiated; it exists solely to distinguish identifier domains at compile time.

### 4.2 Identifier Aliases

| Alias | Tag | Domain |
|-------|-----|--------|
| `ScoreId` | `ScoreTag` | Score document |
| `PartId` | `PartTag` | Part within a score |
| `EventId` | `EventTag` | Event within a voice |
| `SectionId` | `SectionTag` | Formal section |
| `BeamGroupId` | `BeamGroupTag` | Beaming group |
| `TupletId` | `TupletTag` | Tuplet bracket |

All identifiers are unique within a single Score document. The hash specialisation for `Id<T>` enables use in `std::unordered_map`.

---

## Part V: Document Model Types

### 5.1 Score Hierarchy

```
Score
├── ScoreMetadata (title, composer, bar_count, ...)
├── TempoMap (bar → TempoEvent)
├── KeySignatureMap (bar → KeySignature)
├── TimeSignatureMap (bar → TimeSignature)
├── SectionMap (id → ScoreSection)
├── Parts[]
│   ├── PartDefinition (instrument, clef, range, ...)
│   └── Measures[]
│       └── Voices[]
│           └── Events[]
│               └── EventPayload (NoteGroup | RestEvent | ChordSymbolEvent | ScoreDirection)
├── HarmonicAnnotationLayer
└── OrchestrationLayer
```

### 5.2 Key Structured Types

**KeySignature:** `{SpelledPitch root, ScaleDefinition mode, int8_t accidentals}`

**TempoEvent:** `{ScoreTime position, PositiveRational bpm, BeatUnit beat_unit, TempoTransitionType transition_type, ...}`

**Note:** `{SpelledPitch pitch, VelocityValue velocity, optional<ArticulationType> articulation, optional<DynamicLevel> dynamic, optional<Ornament> ornament, bool tie_forward, ...}`

**NoteGroup:** One or more simultaneous notes with shared duration, tuplet context, and beam group.

**Event:** `{EventId id, Beat offset, EventPayload payload}` — the atomic unit of musical content.

**ScoreRegion:** `{ScoreTime start, ScoreTime end, vector<PartId> parts}` — empty parts vector means all parts.

### 5.3 Validation Diagnostic

```cpp
struct Diagnostic {
    ValidationSeverity severity;  // Error, Warning, Info
    std::string rule;             // e.g. "S1", "M3", "R2"
    std::string message;
    std::optional<ScoreTime> location;
    std::optional<PartId> part;
    int error_code;               // 5000–5699
};
```

---

## Part VI: Enumerations

### 6.1 Musical Enumerations

| Enum | Values | Range |
|------|--------|-------|
| `ArticulationType` | Staccato, Tenuto, Accent, Marcato, ... | 0–24 |
| `DynamicLevel` | pppp, ppp, pp, p, mp, mf, f, ff, fff, ffff, fp, sfz, sfp, rfz | 0–13 |
| `InstrumentType` | Violin, Piano, Trumpet, ... Custom | 0–80 |
| `InstrumentFamily` | Strings, Woodwinds, Brass, Percussion, Keyboard, Voice, Electronic | 0–6 |
| `Clef` | Treble, Bass, Alto, Tenor, Percussion, Tab | 0–5 |
| `FormFunction` | Expository, Developmental, Transitional, Cadential, Introductory, Closing, Parenthetical | 0–6 |
| `TexturalRole` | Melody, CounterMelody, HarmonicFill, BassLine, ... Accompagnato | 0–12 |
| `TextureType` | Monophonic, Homophonic, Polyphonic, ... MelodyAccompaniment | 0–9 |

### 6.2 System Enumerations

| Enum | Values | Usage |
|------|--------|-------|
| `ValidationSeverity` | Error, Warning, Info | Diagnostic classification |
| `DocumentState` | Draft, Valid, Compiled, Locked | Score lifecycle |
| `BeatUnit` | Whole, Half, DottedHalf, Quarter, ... Sixteenth | Tempo marking |
| `TempoTransitionType` | Immediate, Linear, MetricModulation | Tempo change shape |
| `HairpinType` | Crescendo, Diminuendo | Dynamic change |
| `GraceType` | Acciaccatura, Appoggiatura | Grace note type |

---

## Part VII: Error Handling

### 7.1 Result Type

```cpp
template <typename T>
using Result = std::expected<T, ErrorCode>;
```

`ErrorCode` is an integer. Error code ranges are allocated by domain (see `standard.md` §3.2).

### 7.2 Error Classification

| Class | Response | Example |
|-------|----------|---------|
| Recoverable | Return error to caller with context | Score not found in session |
| Transient | Retry with backoff (bounded) | TCP connection timeout |
| Invariant violation | Assert / abort | Beat denominator = 0 |

### 7.3 MCP Error Responses

MCP tool handlers return `{"error": "message"}` for invalid input or failed operations. The error message includes enough context for the caller to diagnose the problem without leaking implementation detail.

---

## Part VIII: Precision and Arithmetic

### 8.1 Exact vs Approximate

| Domain | Representation | Rationale |
|--------|---------------|-----------|
| Pitch intervals | Integer semitones | No accumulation error |
| Rhythmic duration | Exact rational (`Beat`) | Subdivision nests without rounding |
| Tempo | Exact rational (`PositiveRational`) | BPM comparisons are exact |
| Frequency | `double` | Physical quantity; precision is bounded by measurement |
| Roughness/consonance | `double` | Psychoacoustic approximation |

### 8.2 Floating-Point Boundaries

Floating-point enters the system at three boundaries:
1. **Acoustic calculations** (frequency, roughness, virtual pitch) — inherently approximate
2. **Temporal conversion to real time** — `absolute_beat_to_real_time` produces `double` seconds
3. **MIDI compilation** — tick quantisation uses Bresenham rounding with integer arithmetic; the floating-point BPM is converted to integer microseconds-per-beat before quantisation

No floating-point appears in the pitch class, interval, scale, or rhythmic arithmetic paths.

---

## Part IX: Serialisation

### 9.1 JSON Schema Versioning

Each IR document format carries a schema version integer. Deserialisation checks the version and rejects documents with a newer schema than the reader supports.

| IR | Current schema version |
|----|----------------------|
| Score | 3 |
| Timbre | 1 |
| Mix | 1 |
| Corpus | 1 |

### 9.2 Round-Trip Invariant

For every Score `s`:
```
score_from_json(score_to_json(s)) == s
```

This invariant is verified by the test suite for representative fixtures covering all event types, annotation layers, and global maps.
