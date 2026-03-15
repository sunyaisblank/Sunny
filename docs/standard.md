# Sunny — Technical Standards

**Document ID:** SUNNY-STD-TECH-001
**Version:** 2.0
**Date:** March 2026
**Status:** Active

---

## 1. Performance Requirements

### 1.1 Latency Targets

| Operation | Target | Maximum | Measurement |
|-----------|--------|---------|-------------|
| TCP command round-trip (WSL2 → Windows) | <50ms | 200ms | Integration test |
| Theory engine computation (per tool call) | <10ms | 50ms | Unit test timing |
| MIDI compilation (32-bar, 4-part score) | <5ms | 20ms | Benchmark test |
| MusicXML/LilyPond compilation | <50ms | 200ms | Benchmark test |
| Score validation (full ruleset) | <10ms | 50ms | Unit test timing |

Performance targets are estimates pending integration test infrastructure.

### 1.2 Resource Limits

| Resource | Limit | Rationale |
|----------|-------|-----------|
| Score parts | 128 | Orchestral maximum |
| Score bars | 10,000 | Memory bound for Score IR |
| Corpus works per database | 1,000 | Analysis aggregation performance |
| MCP tool handlers per server | 200 | Registration map overhead |

---

## 2. Component Naming Convention

### 2.1 Format

All components use the Sirius hierarchical naming system:

`[Domain][Category][Sequence][Variant]`

**Example:** `SIWF001A`
- **Domain**: `SI` (Score IR)
- **Category**: `WF` (Workflow)
- **Sequence**: `001` (First component)
- **Variant**: `A` (Primary implementation)

### 2.2 Domain Codes

| Code | Domain | Description |
|------|--------|-------------|
| `PT` | Pitch | Pitch class, MIDI, spelled pitch, diatonic intervals |
| `SC` | Scale | Scale definitions, generation, relationships |
| `HR` | Harmony | Chord analysis, functions, non-tertian, chord-scale |
| `VL` | Voice leading | Voice motion, constraints, figured bass |
| `RH` | Rhythm | Euclidean, polyrhythm, metric modulation, swing |
| `TN` | Tensor | Beat arithmetic, events |
| `SR` | Serialism | Tone rows, matrix, combinatoriality |
| `GI` | Generalised interval | GIS (Lewin), Klumpenhouwer networks |
| `NR` | Neo-Riemannian | PLR transformations, Tonnetz |
| `ML` | Melody | Contour, statistics, sequences |
| `TU` | Tuning | Equal temperament, just intonation, historical |
| `AC` | Acoustics | Harmonic series, consonance, roughness, virtual pitch |
| `FM` | Form/Format | Phrase structure, external formats (MusicXML, LilyPond, MIDI, Ableton) |
| `RD` | Render | DSP/audio processing |
| `SI` | Score IR | Document hierarchy, temporal, validation, mutation, serialisation |
| `TI` | Timbre IR | Sound source, effects, modulation, descriptors |
| `MI` | Mix IR | Signal routing, levels, EQ, compression, spatial |
| `CI` | Corpus IR | Ingestion, analysis, style profiles, queries |
| `IN` | Infrastructure | Application services, transport, MCP |
| `MC` | MCP | Protocol handler, tool registration |
| `TS` | Test | Validation tests (mirrors source structure) |

### 2.3 Variant Codes

| Code | Meaning |
|------|---------|
| `A` | Primary production implementation |
| `B` | Alternative algorithm or approach |
| `X` | Experimental, not validated |

---

## 3. Error Handling

### 3.1 Result Type

All fallible operations return `std::expected<T, ErrorCode>` (aliased as `Result<T>`). Callers inspect the result before proceeding; no exceptions cross module boundaries.

### 3.2 Error Code Ranges

| Range | Domain | Description |
|-------|--------|-------------|
| 4000–4099 | Format | External format parse/generation errors |
| 4500 | Serialisation | JSON serialisation/deserialisation errors |
| 5000–5699 | Score IR | Structural, temporal, harmonic, mutation, rendering errors |
| 6000–6099 | Timbre IR | Type, validation, workflow errors |
| 7000–7099 | Mix IR | Type, validation, workflow errors |
| 8000–8034 | Corpus IR | Ingestion, analysis, validation errors |

### 3.3 Error Classification

| Class | Behaviour |
|-------|-----------|
| Transient (e.g. TCP timeout) | Retry with backoff; bounded retry budget |
| Permanent (e.g. invalid Score structure) | Propagate to caller with context |
| Invariant violation (e.g. denominator = 0) | Assert; do not propagate |

### 3.4 Undo Support

Mutations on Score, Timbre, Mix documents accept an optional `UndoStack*`. When provided, the mutation records an inverse operation. Undo restores all invariants; if restoration fails, the failure propagates rather than leaving the document in an inconsistent state.

---

## 4. Communication Protocol

### 4.1 MCP Protocol (JSON-RPC 2.0 over stdio)

The MCP server (`sunny-mcp`) reads JSON-RPC 2.0 requests from stdin and writes responses to stdout. Each request is a single line of JSON.

Supported methods: `initialize`, `tools/list`, `tools/call`.

### 4.2 TCP Transport (Ableton Bridge)

**Default port:** 9001 (configurable via `TcpTransport` constructor)

**Framing:** 4-byte big-endian length prefix followed by UTF-8 JSON payload.

**State machine:** Disconnected → Connecting → Connected | Error

**Message format (LOM command):**
```json
{
  "action": "get|set|call",
  "path": "song/tracks/0/devices/0",
  "args": []
}
```

---

## 5. Type System

### 5.1 Pitch Representation

| Type | Representation | Usage |
|------|---------------|-------|
| `PitchClass` | `uint8_t` (0–11) | Pitch-class set operations, chromatic identity |
| `MidiNote` | `uint8_t` (0–127) | MIDI output, acoustic calculations |
| `SpelledPitch` | `{letter, accidental, octave}` | Score notation, interval arithmetic |
| `Interval` | `int8_t` | Scale definitions, chromatic displacement |
| `DiatonicInterval` | `{chromatic, diatonic}` | Enharmonic-aware transposition |

### 5.2 Temporal Representation

| Type | Representation | Usage |
|------|---------------|-------|
| `Beat` | `{numerator, denominator}` (exact rational) | Duration, offset within bar |
| `ScoreTime` | `{bar, beat}` | Hierarchical position in score |
| `PositiveRational` | `{numerator, denominator}` | BPM (rate, not duration) |

`Beat` is an aggregate; initialise with `Beat{2, 1}` (not `Beat(2)`, which gives denominator 0).

### 5.3 Identifier Types

All identifiers use the `Id<T>` template, where `T` is a tag type. The tag prevents accidental comparison between identifiers of different domains.

| Type alias | Tag | Usage |
|-----------|-----|-------|
| `ScoreId` | `ScoreTag` | Score document identity |
| `PartId` | `PartTag` | Part within a score |
| `EventId` | `EventTag` | Event within a voice |
| `SectionId` | `SectionTag` | Formal section |

---

## 6. Testing Requirements

### 6.1 Test Infrastructure

| Target | Framework | Count |
|--------|-----------|-------|
| `Sunny.Test.Core` | Catch2 v3 | 1,228 |
| `Sunny.Test.Render` | Catch2 v3 | 17 |
| `Sunny.Test.Infrastructure` | Catch2 v3 | 284 |
| `Sunny.Test.Max` | Catch2 v3 | 31 |
| **Total** | | **1,560** |

### 6.2 Test Naming

Tests are named by the postcondition they verify, prefixed with the component under test:

```
SITP001A: ScoreTime comparison transitivity
SIWF001A: create_score produces compilable output
SICM001A: compile_to_midi preserves note count
```

### 6.3 Test Categories

| Category | Gate | CI blocking |
|----------|------|-------------|
| Unit (postcondition verification) | Mandatory | Yes |
| Integration (Ableton round-trip) | Soft | No (requires running Ableton) |
| Mutation (Mull) | Advisory | No |
| Static analysis (CodeQL) | Advisory | No |

### 6.4 Build Command

```bash
cd bin && cmake .. -DCMAKE_BUILD_TYPE=Release && cmake --build . && ctest --output-on-failure
```

---

## 7. MCP Tool Registration

### 7.1 Tool Inventory

| Component | Domain | Tool count | Tool prefix |
|-----------|--------|------------|-------------|
| MCPT001A | Core theory | 7 | (none) |
| MCPT002A | Timbre IR | 18 | (none) |
| MCPT003A | Mix IR | 22 | (none) |
| MCPT004A | Corpus IR | 22 | (none) |
| MCPT005A | Score IR | 25 | `score_` |
| **Total** | | **94** | |

### 7.2 Session Pattern

Each MCPT component creates a `shared_ptr` to a session struct captured by tool handler lambdas. The session holds domain objects keyed by auto-incrementing `uint64_t` IDs. Tool handlers validate JSON parameters, delegate to workflow functions, and return JSON responses.

### 7.3 Registration API

```cpp
server.register_tool(
    "tool_name",
    "Human-readable description",
    {{"param1", "type hint"}, {"param2", "type hint"}},
    [session](const json& params) -> json { ... }
);
```
