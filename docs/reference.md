# Sunny — Reference

Normative reference for the Sunny C++23 music theory engine: component naming, error taxonomy, operation contracts, type system, serialisation, and wire protocol. Every path, port, count, and version below is verified against the source tree; where the retired standards, operations, and type documents disagreed with the code, the code governs.

## 1. Component naming

Components use the Sirius scheme `[Domain][Category][Sequence][Variant]`. In `SIWF001A`, `SI` is the domain (Score IR), `WF` the category (Workflow), `001` the sequence, and `A` the variant.

The registry below lists the 21 domain codes and where each domain's components live in the tree.

| Code | Scope | Location |
|------|-------|----------|
| `PT` | Pitch: pitch class, MIDI, spelled pitch, diatonic intervals, pitch-class sets | `src/Sunny.Core/Pitch` |
| `SC` | Scale definitions, generation, relationships | `src/Sunny.Core/Scale` |
| `HR` | Harmony: chord analysis, functions, non-tertian, chord-scale | `src/Sunny.Core/Harmony` |
| `VL` | Voice leading: motion, constraints, figured bass | `src/Sunny.Core/VoiceLeading` |
| `RH` | Rhythm: Euclidean, time signatures, tuplets, polyrhythm, swing | `src/Sunny.Core/Rhythm` |
| `TN` | Tensor: core types, `ErrorCode`, beat arithmetic, events | `src/Sunny.Core/Tensor` |
| `SR` | Serialism: tone rows, matrix, combinatoriality | `src/Sunny.Core/PostTonal` |
| `GI` | Generalised interval systems (Lewin), Klumpenhouwer networks | `src/Sunny.Core/Transform` |
| `NR` | Neo-Riemannian: PLR transformations, Tonnetz | `src/Sunny.Core/Transform` |
| `ML` | Melody: contour, statistics, sequences | `src/Sunny.Core/Melody` |
| `TU` | Tuning: equal temperament, just intonation, historical | `src/Sunny.Core/Tuning` |
| `AC` | Acoustics: harmonic series, consonance, roughness, virtual pitch | `src/Sunny.Core/Acoustics` |
| `FM` | Form (phrase structure) and Format (MusicXML, LilyPond, MIDI, ABC, Ableton) | `src/Sunny.Core/Form`; `src/Sunny.Infrastructure/Format` |
| `RD` | Render: DSP and audio processing | `src/Sunny.Render` |
| `SI` | Score IR: document, temporal, validation, mutation, serialisation, workflow | `src/Sunny.Core/Score` |
| `TI` | Timbre IR: sound source, effects, modulation, descriptors | `src/Sunny.Core/Timbre` |
| `MI` | Mix IR: routing, levels, EQ, compression, spatial | `src/Sunny.Core/Mix` |
| `CI` | Corpus IR: ingestion, analysis, style profiles, queries | `src/Sunny.Core/Corpus` |
| `IN` | Infrastructure: application services, bridge, transport, session | `src/Sunny.Infrastructure` |
| `MC` | MCP: protocol server, tool registration | `src/Sunny.Infrastructure/Mcp` |
| `TS` | Tests (mirrors source structure) | `src/Sunny.Test` |

All 21 codes have components in the tree. `FM` is a known collision: it means Form in Sunny.Core (`FMST001A`, `FMMT001A`) and Format in Sunny.Infrastructure (`FMAB`, `FMAL`, `FMLY`, `FMMI`, `FMMX`, `FMSL`); the layer disambiguates. Two codes exist in the tree without registry entries: `BD` (Python binding, `src/Sunny.Infrastructure/Binding/BDPY001A.cpp`) and `RT` (real-time support, `src/Sunny.Infrastructure/RealTime/RTLF001A.h`).

Variant codes: `A` primary production implementation, `B` alternative algorithm, `X` experimental. Only `A` variants exist in the tree today.

## 2. Error taxonomy

All fallible operations return `Result<T>`, an alias for `std::expected<T, ErrorCode>`. No exceptions cross module boundaries. The single `ErrorCode` enum in `src/Sunny.Core/Tensor/TNTP001A.h` is the complete taxonomy: every value crossing a `Result<T>` boundary or stored in a `Diagnostic` is an enumerator there. There are no per-domain error namespaces.

Codes are allocated in thousands blocks; the table gives each block and the sub-range actually populated by the enum.

| Block | Domain | Populated range |
|-------|--------|-----------------|
| 2xxx | Validation (input and parameter checks) | 2100–2138 |
| 3xxx | Theory computation | 3010–3195 |
| 4xxx | Infrastructure (transport, session, MCP, OSC) and format | 4100–4401 infrastructure; 4500–4507 format |
| 5xxx | Score IR: structure, temporal, annotation, mutation, rendering, serialisation | 5000–5602 |
| 6xxx | Timbre IR | 6000–6031 |
| 7xxx | Mix IR | 7000–7034 |
| 8xxx | Corpus IR | 8000–8034 |

Classification determines propagation: transient failures (TCP timeout) retry with backoff under a bounded budget; permanent failures (invalid document structure) propagate to the caller with context; invariant violations (Beat denominator of zero) assert rather than propagate.

Mutations on Score, Timbre, and Mix documents accept an optional `UndoStack*`. When provided, the mutation records an inverse operation; undo restores all invariants, and a failed restoration propagates rather than leaving the document inconsistent.

## 3. Operation contracts

The type system enforces structural preconditions; runtime validation covers semantic preconditions. The four contracts below are the reference cases; error codes name enumerators from §2.

### 3.1 create_score

```
create_score(ScoreSpec) → Result<Score>            [SIWF001A]

Preconditions:
  total_bars ≥ 1
  parts non-empty
  bpm ∈ [20, 999]
  time_sig_num ≥ 1; time_sig_den a power of two

Postconditions:
  validate_structural(score) produces no Error diagnostics
  score.parts.size() == spec.parts.size()
  every part has total_bars measures
  tempo map contains an entry at SCORE_START (bar 1)

Errors:
  5400 InvalidMutation      (zero bars, or empty part list)
  2136 InvalidBPM           (bpm outside [20, 999])
  2120 InvalidTimeSignature (numerator < 1, or denominator not a power of two)
```

### 3.2 wf_insert_note

```
wf_insert_note(Score&, PartId, bar, voice, offset, Note, duration,
               UndoStack* = nullptr) → Result<MutationResult>      [SIWF001A → SIMT001A]

Preconditions:
  part exists in score
  bar ∈ [1, part.measures.size()]
  a Voice with the given voice_index exists in the target measure

Postconditions:
  an Event with NoteGroup payload exists at (bar, voice, offset);
  events within the voice remain ordered by offset
  score.version is incremented; harmonic annotations covering the bar are marked stale

Errors:
  5400 InvalidMutation (part not found, bar out of range, or voice not found)
```

Offset, overlap, and measure-fill violations are not rejected at insertion; they surface as structural diagnostics (5100 InvalidOffset, 5101 OverlappingEvents, 5103 MeasureFillError) at the next validation pass.

### 3.3 compile_to_midi

```
compile_to_midi(Score, ppq = 480) → Result<CompiledMidiResult>     [SICM001A]

Preconditions:
  is_compilable(score) — no Error-level validation diagnostics
  ppq > 0

Postconditions:
  every note: note ∈ [0, 127], velocity ∈ [1, 127], tick ≥ 0
  result.midi.tempos and result.midi.time_signatures are sorted by tick ascending

Errors:
  5401 InvariantViolation (score not compilable)
```

Tempo or time-signature events whose temporal conversion fails are dropped and reported in the `CompilationReport` rather than failing the compilation.

### 3.4 wf_transpose

```
wf_transpose(Score&, variant<EventId, ScoreRegion>, DiatonicInterval,
             UndoStack* = nullptr) → Result<MutationResult>        [SIWF001A]

Preconditions:
  EventId target: the event exists in the score
  ScoreRegion target: start < end, referenced parts exist

Postconditions:
  every affected note satisfies new_pitch = apply_interval(old_pitch, interval)
  unaffected events unchanged

Errors:
  5400 InvalidMutation (event not found)
```

### 3.5 Validation tiers

Score IR validation (`SIVD001A`) partitions rules into three tiers by consequence.

| Tier | Rules | Severity | Effect |
|------|-------|----------|--------|
| Structural | S0–S16 | Error (S14 and S16 also emit advisory warnings) | Blocks compilation |
| Musical | M1–M10 | Warning/Info | Advisory |
| Rendering | R1–R5 | Warning/Info | Advisory |

Validation runs at three points: `create_score` validates structure before returning; each mutation bumps the document version and marks affected annotation layers stale, with diagnostics gathered on the next validation pass; `is_compilable()` gates entry to every compilation pipeline.

### 3.6 Undo system

Every mutation accepts an optional `UndoStack*`. Entries are LIFO; redo entries are cleared on any new mutation; operations grouped under the `UndoGroup` RAII guard undo atomically. The stack does not persist across process boundaries.

## 4. Type system

Types encode invariants, units, and meaning: pitch and rhythm use exact integer or rational arithmetic (no floating-point in those paths), identifiers carry phantom tags, and all fallible operations return `Result<T>`.

### 4.1 Pitch types

| Type | Representation | Range | Usage |
|------|---------------|-------|-------|
| `PitchClass` | `uint8_t` (alias, not enum) | [0, 11] | Pitch-class set operations, chromatic identity |
| `MidiNote` | `uint8_t` | [0, 127] | MIDI output, acoustic calculations |
| `Velocity` | `uint8_t` | [1, 127] | MIDI velocity (0 is note-off, excluded) |
| `Interval` | `int8_t` | [−127, 127] | Chromatic semitone displacement |
| `SpelledPitch` | `{letter, accidental, octave}` | letter [0, 6], octave [−1, 9] | Enharmonic-preserving notation; construct via `from_spn("C#4")` |
| `DiatonicInterval` | `{chromatic, diatonic}` (unbounded `int`) | — | Quality-aware transposition; composes under `interval_add` |

`SpelledPitch` has no ordering operators; use `midi_value()` for range comparison.

### 4.2 Temporal types

`Beat` (`src/Sunny.Core/Tensor/TNBT001A.h`) is an exact rational with `std::int64_t` numerator and denominator, measuring duration or offset in whole-note fractions: quarter note = `Beat{1, 4}`. Canonical form has positive, irreducible denominator; `normalise` requires a nonzero denominator, and a zero denominator is an invariant violation.

Construction trap: `Beat` is an aggregate, so `Beat{2}` zero-initialises the denominator and produces the invalid `{2, 0}`. Always write both fields — `Beat{2, 1}` for two whole notes — or use `Beat::zero()` and `Beat::one()`.

`ScoreTime` is `{uint32_t bar, Beat beat}` with 1-indexed bars; `SCORE_START` is `{1, Beat::zero()}`; comparison orders by bar, then beat. `PositiveRational` (`int64_t/int64_t`, both positive) represents tempo; it is kept distinct from `Beat` because rate and duration compose differently. `make_bpm(120)` constructs `{120, 1}`.

### 4.3 Temporal conversion chain

Positions convert along a fixed chain defined in `src/Sunny.Core/Score/SITM001A.h`:

```
ScoreTime → AbsoluteBeat → RealTime (seconds) → TickTime (MIDI ticks)
```

`AbsoluteBeat(b, β)` is the sum of preceding measure durations plus β; `RealTime` integrates 60/BPM(t) over the AbsoluteBeat domain; `TickTime` multiplies by PPQ. `DEFAULT_PPQ` is 480. ScoreTime→AbsoluteBeat and AbsoluteBeat→TickTime are monotonically increasing, and tick quantisation uses Bresenham-style rounding with cumulative error at most 0.5 ticks per bar boundary.

### 4.4 Identifiers

`Id<T>` wraps a `uint64_t` with a phantom tag `T` that is never instantiated; identifiers with different tags cannot be compared or substituted, and a `std::hash` specialisation permits map keys. The aliases are `ScoreId`, `PartId`, `EventId`, `SectionId`, `BeamGroupId`, and `TupletId`, each unique within one Score document.

### 4.5 Enumeration ranges

All enumerations are `uint8_t`-backed and contiguous from 0; the table gives each range as verified in `SITP001A.h` (all except `InstrumentType`, which lives in `SIDC001A.h`).

| Enum | Range | Endpoints |
|------|-------|-----------|
| `ArticulationType` | 0–25 | Staccato … BendDown |
| `DynamicLevel` | 0–13 | pppp … rfz |
| `InstrumentType` | 0–61 | Violin … Custom |
| `InstrumentFamily` | 0–6 | Strings … Electronic |
| `Clef` | 0–5 | Treble … Tab |
| `FormFunction` | 0–6 | Expository … Parenthetical |
| `TexturalRole` | 0–12 | Melody … Accompagnato |
| `TextureType` | 0–9 | Monophonic … MelodyAccompaniment |
| `BeatUnit` | 0–7 | Whole … Sixteenth |
| `TempoTransitionType` | 0–2 | Immediate, Linear, MetricModulation |
| `HairpinType` | 0–1 | Crescendo, Diminuendo |
| `GraceType` | 0–1 | Acciaccatura, Appoggiatura |
| `ValidationSeverity` | 0–2 | Error, Warning, Info |
| `DocumentState` | 0–3 | Draft, Valid, Compiled, Locked |

### 4.6 Floating-point boundaries

Floating-point enters at three boundaries only: acoustic calculations (frequency, roughness, virtual pitch), conversion to real time (`double` seconds), and MIDI compilation, where BPM is converted to integer microseconds-per-beat before Bresenham quantisation. Pitch-class, interval, scale, and rhythmic arithmetic remain exact.

## 5. Serialisation

Each IR document format carries a schema version integer; deserialisation rejects documents newer than the reader supports (5600 SchemaVersionMismatch). Current versions, cited from the serialisation headers:

| IR | Version | Constant |
|----|---------|----------|
| Score | 3 | `SCORE_IR_SCHEMA_VERSION`, `src/Sunny.Core/Score/SISZ001A.h` |
| Timbre | 1 | `TIMBRE_IR_SCHEMA_VERSION`, `src/Sunny.Core/Timbre/TISZ001A.h` |
| Mix | 1 | `MIX_IR_SCHEMA_VERSION`, `src/Sunny.Core/Mix/MISZ001A.h` |
| Corpus | 2 | `CORPUS_IR_SCHEMA_VERSION`, `src/Sunny.Core/Corpus/CISZ001A.h` |

The round-trip invariant `score_from_json(score_to_json(s)) == s` holds for every Score and is verified by the test suite across all event types, annotation layers, and global maps.

## 6. Wire protocol

### 6.1 MCP (stdio)

The MCP server (`sunny-mcp`, `src/Sunny.Infrastructure/Mcp`) speaks JSON-RPC 2.0 over stdio, one request per line, supporting `initialize`, `tools/list`, and `tools/call`. Tools are registered as:

```cpp
server.register_tool(name, description, input_schema /* JSON Schema */, handler);
```

### 6.2 TCP bridge (Ableton)

The engine reaches Ableton Live through a Python Remote Script (`src/SunnyRemoteScript`: `server.py`, `handler.py`, `surface.py`) listening on TCP port 9001 by default (`INTP001A.h` and `surface.py` agree; the port is configurable on both ends). Framing is a 4-byte big-endian `uint32` length prefix followed by a UTF-8 JSON payload; the server accepts one client at a time and caps payloads at 16 MB.

Requests address the Live Object Model by slash-separated path. The vocabulary, verified against `handler.py` and the C++ serialiser (`src/Sunny.Infrastructure/Bridge/INBR001A.cpp`):

```json
{"type": "get" | "set" | "call",
 "path": "song/tracks/0/devices/0",
 "name": "<property or method>",
 "args": []}
```

Responses are `{"success": bool}` with an optional `"value"` on success and an optional `"error"` string on failure. The C++ `LomRequestType` also serialises `observe` and `unobserve`, but the Remote Script handler rejects them as unknown; only get, set, and call are live.

### 6.3 Environment variables

Connection is opt-in, read at `sunny-mcp` start (`src/Sunny.Infrastructure/Mcp/main.cpp`): `SUNNY_ABLETON_HOST` names the Remote Script host (unset means offline mode, in which bridge operations decline loudly rather than pretend delivery), and `SUNNY_TCP_PORT` overrides the port (default 9001).

### 6.4 UDP path (Max for Live)

A separate low-latency path is reserved for a Max for Live device (`Sunny_UDP.amxd`) receiving OSC over UDP, default port 9877, for real-time parameter modulation. The engine-side pieces exist (`src/Sunny.Infrastructure/Protocol/WPOSC001A` codec, `src/Sunny.Infrastructure/RealTime/RTLF001A` ring buffer, the `sunny.parameter~` external); the device itself does not yet, so no UDP transport ships (see docs/decisions.md D3).

## 7. Ableton connection and performance targets

Ableton Live 12 runs on Windows (installed under `E:\Creative Libraries\Ableton\Live 12 Suite\`); the engine runs in WSL2. The connection goes to the Windows host via the WSL2 host IP read from `/etc/resolv.conf` in the guest, so port 9001 must be reachable across that boundary.

The targets below are design goals pending live measurement; the integration harness that would confirm them does not yet exist.

| Metric | Target | Bound |
|--------|--------|-------|
| TCP command latency (WSL2 → Windows) | < 50 ms | 200 ms |
| Theory engine computation (per tool call) | < 10 ms | 50 ms |
| MIDI compilation (32-bar, 4-part score) | < 5 ms | 20 ms |
| MusicXML/LilyPond compilation | < 50 ms | 200 ms |
| Full test suite | < 10 s | 30 s |

## 8. MCP tool inventory

The five tool components register 93 tools in total, counted from `register_tool` call sites in `src/Sunny.Infrastructure/Mcp`.

| Component | Domain | Tools | Prefix |
|-----------|--------|-------|--------|
| MCPT001A | Core theory | 7 | none |
| MCPT002A | Timbre IR | 18 | none |
| MCPT003A | Mix IR | 22 | none |
| MCPT004A | Corpus IR | 21 | none |
| MCPT005A | Score IR | 25 | `score_` |

Each component creates a `shared_ptr` session captured by its handler lambdas; the session holds domain objects keyed by auto-incrementing `uint64_t` IDs, lives from server start to exit, and shares nothing across domains. Every handler validates its JSON parameters first and returns an error response for invalid input without touching workflow functions, then looks up the target object by session ID, delegates to the workflow or query function, and returns a JSON response.
