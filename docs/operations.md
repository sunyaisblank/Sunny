# Sunny — Operational Governance

**Document ID:** SUNNY-OPS-001
**Version:** 2.0
**Date:** March 2026
**Status:** Active

---

## 1. Scope

This document establishes operational procedures, execution standards, and validation constraints for the Sunny C++23 music theory engine and MCP tool server. All operations are bounded by the type system, validated at entry points, and propagated through `std::expected<T, ErrorCode>`.

---

## 2. Design Principles

### 2.1 Deterministic Execution

| Principle | Implementation |
|-----------|----------------|
| Bounded parameters | Enum ranges, integral constraints checked at validation boundary |
| Exact arithmetic | `Beat{numerator, denominator}` — no floating-point in pitch/rhythm |
| Typed identifiers | `Id<T>` template prevents cross-domain substitution |
| Undoable mutations | `UndoStack` captures inverse operations for every Score/IR mutation |

### 2.2 Error Propagation

All fallible operations return `Result<T>` (alias for `std::expected<T, ErrorCode>`). The caller inspects the result before proceeding. No exceptions cross module boundaries; invariant violations assert rather than propagate.

---

## 3. Component Naming Convention

### 3.1 Format

`[Domain][Category][Sequence][Variant]`

The full domain code registry is documented in `standard.md` §2.2. Components follow the Sirius hierarchical naming system throughout.

### 3.2 Variant Codes

| Code | Meaning |
|------|---------|
| `A` | Primary production implementation |
| `B` | Alternative algorithm or approach |
| `X` | Experimental, not validated |

---

## 4. Bounded Operation Definitions

Operations are defined by their preconditions, postconditions, and invariants. The type system enforces structural preconditions; runtime validation covers semantic preconditions.

### 4.1 Score Creation

```
create_score(ScoreSpec) → Result<Score>

Preconditions:
  total_bars ≥ 1
  bpm > 0
  time_sig_num ≥ 1, time_sig_den ∈ {1, 2, 4, 8, 16, 32}
  parts.size() ≥ 0

Postconditions:
  validate_structural(score) produces no Error diagnostics
  score.parts.size() == spec.parts.size()
  ∀ part: part.measures.size() == spec.total_bars
  score.tempo_map contains entry at bar 1

Error codes:
  5000: DocumentStructure (spec produces invalid document)
```

### 4.2 Note Insertion

```
wf_insert_note(Score&, PartId, bar, voice, offset, Note, duration) → Result<MutationResult>

Preconditions:
  bar ∈ [1, score.metadata.bar_count]
  voice ∈ [0, measure.voices.size())
  offset ∈ [Beat::zero(), measure_duration)
  duration > Beat::zero()
  Note.pitch.letter ∈ [0, 6]
  Note.velocity.value ∈ [0, 127]

Postconditions:
  Event with NoteGroup payload exists at (bar, voice, offset)
  Measure fill invariant maintained (events + rests = measure duration)

Error codes:
  5100: InvalidOffset
  5101: OverlappingEvents
  5103: MeasureFillError
  5400: InvalidMutation
```

### 4.3 Score Compilation

```
compile_to_midi(Score, ppq) → Result<CompiledMidiResult>

Preconditions:
  is_compilable(score) (no Error-level validation diagnostics)
  ppq > 0

Postconditions:
  ∀ note ∈ result.midi.notes: note.note ∈ [0, 127]
  ∀ note: note.velocity ∈ [1, 127]
  ∀ note: note.tick ≥ 0
  result.midi.tempos is sorted by tick ascending
  result.midi.time_signatures is sorted by tick ascending

Error codes:
  5000: DocumentStructure (not compilable)
```

### 4.4 Region Operations

```
wf_transpose(Score&, variant<EventId, ScoreRegion>, DiatonicInterval) → Result<MutationResult>

Preconditions:
  If EventId: event exists in score
  If ScoreRegion: start < end, referenced parts exist

Postconditions:
  ∀ affected note: new_pitch = apply_interval(old_pitch, interval)
  Unaffected events unchanged

Error codes:
  5400: InvalidMutation (event not found)
```

---

## 5. Validation Framework

### 5.1 Validation Tiers

| Tier | Rules | Severity | Purpose |
|------|-------|----------|---------|
| Structural (S0–S11) | Error | Blocks compilation |
| Musical (M1–M10) | Warning | Advisory for compositional quality |
| Rendering (R1–R5) | Warning/Info | Advisory for DAW compilation |

### 5.2 Error Code Ranges

| Range | Domain | Description |
|-------|--------|-------------|
| 4000–4099 | Format | External format parse/generation errors |
| 4500 | Serialisation | JSON round-trip errors |
| 5000–5699 | Score IR | Structural, temporal, harmonic, mutation, rendering |
| 6000–6099 | Timbre IR | Type, validation, workflow errors |
| 7000–7099 | Mix IR | Type, validation, workflow errors |
| 8000–8034 | Corpus IR | Ingestion, analysis, validation errors |

### 5.3 Validation Workflow

Validation runs at three points:
1. **On creation** — `create_score` validates structural integrity before returning
2. **On mutation** — each mutation function runs incremental validation and returns `MutationResult` with diagnostics
3. **On compilation** — `is_compilable()` gates entry to the compilation pipeline

---

## 6. Undo System

### 6.1 Mechanism

Every mutation function accepts an optional `UndoStack*`. When provided, the mutation pushes an `UndoEntry` containing the inverse operation. Undo restores the document to its pre-mutation state, including all invariants.

### 6.2 Constraints

- Undo entries are LIFO; redo entries are cleared on new mutation
- Grouped operations (via `UndoGroup` RAII guard) undo atomically
- The UndoStack does not persist across process boundaries

---

## 7. MCP Tool Execution

### 7.1 Tool Handler Contract

Each MCP tool handler:
1. Validates all required JSON parameters
2. Returns `error_response()` for invalid input without calling workflow functions
3. Looks up the target object (Score, TimbreProfile, MixGraph, etc.) by session ID
4. Delegates to the appropriate workflow or query function
5. Returns a JSON response with domain-specific fields

### 7.2 Session Lifecycle

Sessions are created at server start and persist until the server exits. Each IR domain maintains its own session store with auto-incrementing IDs. Objects are not shared across domains.

### 7.3 Tool Inventory

| Component | Domain | Tools | Session type |
|-----------|--------|-------|-------------|
| MCPT001A | Core theory | 7 | Orchestrator (shared) |
| MCPT002A | Timbre IR | 18 | TimbreSession |
| MCPT003A | Mix IR | 22 | MixSession |
| MCPT004A | Corpus IR | 22 | CorpusSession |
| MCPT005A | Score IR | 25 | ScoreSession |
| **Total** | | **94** | |

---

## 8. Component Registry

### 8.1 Core Theory

| Code | Function | Description |
|------|----------|-------------|
| PTPC001A | pitch_class operations | Pitch class arithmetic, D₁₂ group |
| PTMN001A | midi_note operations | MIDI number conversion |
| PTSP001A | SpelledPitch operations | Enharmonic-aware pitch |
| PTDI001A | DiatonicInterval operations | Interval quality and arithmetic |
| SCDF001A | ScaleDefinition | Scale interval patterns |
| SCGN001A | scale generation | Scale note enumeration |
| HRFN001A | harmonic function | Roman numeral analysis |

### 8.2 Score IR

| Code | Function | Description |
|------|----------|-------------|
| SITP001A | Score types | Enumerations, identifiers, value types |
| SIDC001A | Score document | Part, Measure, Voice, Event hierarchy |
| SITM001A | Score temporal | ScoreTime ↔ AbsoluteBeat ↔ tick conversion |
| SIVD001A | Score validation | Structural, musical, rendering rules |
| SIMT001A | Score mutation | Insert, delete, modify, transpose operations |
| SISZ001A | Score serialisation | JSON round-trip with schema versioning |
| SIWF001A | Score workflow | High-level composition functions |
| SIQR001A | Score query | Read-only score interrogation |
| SICM001A | Score compilation | Score → MIDI event data |
| SIVW001A | Score views | Piano reduction, short score, skeleton |

### 8.3 Infrastructure

| Code | Function | Description |
|------|----------|-------------|
| INWF001A | Compilation wrappers | MusicXML, LilyPond, Ableton compilation |
| INTP001A | LomTransport | TCP transport with length-prefixed framing |
| INOR001A | Orchestrator | Shared state for MCP tool handlers |
| FMAL001A | Ableton compiler | Score → Ableton via LomTransport |
| FMAL002A | Timbre compiler | TimbreProfile → Ableton device chains |
| FMAL003A | Mix compiler | MixGraph → Ableton mixer configuration |
| CIIN001A | Corpus ingestion | MIDI/MusicXML → IngestedWork |
