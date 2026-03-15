# Sunny — Project Roadmap

**Document ID:** SUNNY-GOV-ROAD-001
**Version:** 2.0
**Date:** March 2026
**Status:** Active Development
**Governance:** Compliant with `Standards.md`

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Vision and Goals](#vision-and-goals)
3. [Current State](#current-state)
4. [Completed Work](#completed-work)
5. [Remaining Work](#remaining-work)
6. [Metrics and KPIs](#metrics-and-kpis)
7. [Risk Assessment](#risk-assessment)

---

## Executive Summary

Sunny is a C++23 music theory engine with MCP tool serving, designed to give AI agents compositional agency over scores, timbres, mixes, and corpora. The engine compiles scores to MIDI, MusicXML, LilyPond, and Ableton Live via a TCP bridge.

The architecture comprises four layers: Sunny.Core (music theory primitives and five IR document models), Sunny.Render (DSP/audio processing), Sunny.Infrastructure (compilation, transport, MCP serving), and Sunny.Test (validation). A Python thin wrapper provides scripting access. A Python Remote Script bridges the engine to Ableton Live over TCP.

94 MCP tools span seven domains: core theory (7), timbre IR (18), mix IR (22), corpus IR (22), and score IR (25). The formal specification covers 16 chapters and is fully implemented.

---

## Vision and Goals

Sunny enables an AI agent to compose, orchestrate, arrange, and produce music through a structured document model with formal validation. Each intermediate representation (Score, Timbre, Mix, Corpus) captures a distinct concern; the MCP layer exposes all operations as tool calls.

The system prioritises correctness over convenience: exact arithmetic for pitch and rhythm, compile-time type safety for identifiers, and validation at every mutation boundary.

### Forward Goals

- Integration testing with Ableton Live via TcpTransport and SunnyRemoteScript
- Production hardening of the TCP bridge (reconnection, error recovery)
- Performance profiling and optimisation of compilation pipelines
- Extended corpus ingestion (audio analysis, style transfer)

---

## Current State

### Codebase Metrics

| Metric | Value |
|--------|-------|
| C++ source files | ~162 |
| C++ lines of code | ~37,500 |
| Tests (Catch2) | 1,560 |
| Test targets | 4 (Core, Render, Infrastructure, Max) |
| MCP tools | 94 |
| C++ standard | C++23 |
| IR layers | 5 (Score, Timbre, Mix, Corpus, Core theory) |
| Python wrapper files | ~45 |

### Technology Stack

| Component | Technology |
|-----------|------------|
| Engine | C++23 (`std::expected`, concepts) |
| Build system | CMake 3.24+ |
| Test framework | Catch2 v3 |
| JSON | nlohmann/json |
| MCP server | Custom JSON-RPC 2.0 over stdio |
| Ableton bridge | Python Remote Script over TCP (length-prefixed JSON) |
| XML parsing | pugixml |

---

## Completed Work

### Formal Specification (all 16 chapters)

| Chapter | Domain | Components | Status |
|---------|--------|------------|--------|
| §1–3 | Pitch | PTPC, PTMN, PTPS, PTSP, PTDI, PTDN | Complete |
| §4 | Scale | SCDF, SCGN, SCRN | Complete |
| §5 | Harmony | HRFN, HRRN, HRNG, HRSD, HRCH, HRCD, HRNT, HRCS, HRST | Complete |
| §7 | Voice leading | VLNT, VLCN, VLFB, VLSC | Complete |
| §9 | Rhythm | RHEU, RHTS, RHTU, polyrhythm, swing | Complete |
| §10 | Form | FMST, FMMT | Complete |
| §11 | Transformational | SRTW, NRPL, GIST, GIKN | Complete |
| §12 | Pitch-class sets | PTPS (hexachord catalogue, Z-relation, similarity) | Complete |
| §13 | Tuning | TUET, TUJI, TUHT | Complete |
| §14 | Acoustics | ACHS, ACPL, ACRG, ACVP | Complete |
| §15 | Melody, external formats | MLCT, FMSL, FMLY, FMAB, FMMI, FMMX | Complete |
| §16 | Score IR | SITP, SIDC, SITM, SIVD, SIMT, SISZ, SIWF, SIQR, SICM, SIVW | Complete |

### Intermediate Representations

| IR | Types | Document | Validation | Serialisation | Workflow | MCP | Tests |
|----|-------|----------|------------|---------------|----------|-----|-------|
| Score | SITP001A | SIDC001A | SIVD001A | SISZ001A | SIWF001A | MCPT005A (25) | ~170 |
| Timbre | TITP001A | TIDC001A | TIVD001A | TISZ001A | TIWF001A | MCPT002A (18) | 130 |
| Mix | MITP001A | MIDC001A | MIVD001A | MISZ001A | MIWF001A | MCPT003A (22) | 106 |
| Corpus | CITP001A | CIDC001A | CIVD001A | CISZ001A | CIWF001A | MCPT004A (22) | ~108 |

### Compilation Targets

| Target | Component | Path |
|--------|-----------|------|
| MIDI | SICM001A (Core) | Score → CompiledMidi |
| MusicXML | FMMX002A (Infrastructure) | Score → XML string |
| LilyPond | FMLY002A (Infrastructure) | Score → LY string |
| Ableton | FMAL001A–003A (Infrastructure) | Score/Timbre/Mix → LOM commands via TCP |

### Infrastructure

| Component | Description |
|-----------|-------------|
| TcpTransport (INTP001A) | POSIX sockets, length-prefixed JSON, state machine |
| SunnyRemoteScript | Python Control Surface for Ableton (server.py, handler.py, surface.py) |
| Corpus ingestion (CIIN001A) | MIDI and MusicXML → IngestedWork with key estimation and quantisation |
| Analysis engine (CIAN001A) | 9 analytical domains (harmonic, melodic, rhythmic, formal, voice-leading, textural, dynamic, orchestration, motivic) |

---

## Remaining Work

### Integration Testing (priority: high)

The Ableton compilation path (FMAL001A–003A) has been unit-tested against a CommandBuffer mock. End-to-end testing requires a running Ableton instance with SunnyRemoteScript active.

- Validate TcpTransport connection/disconnection lifecycle against Ableton
- Verify Score → track/note injection round-trip
- Verify Timbre → device chain mapping
- Verify Mix → mixer configuration
- Measure latency under realistic workloads

Constraint: Ableton runs on Windows (path: `E:\Creative Libraries\Ableton\Live 12 Suite\`), while the engine runs in WSL2. Connection goes via the WSL2 host IP from `/etc/resolv.conf`.

### Production Hardening (priority: medium)

- Reconnection logic for TcpTransport after Ableton restart
- Error recovery when LOM commands fail (device not found, track deleted)
- Timeout handling for unresponsive Ableton sessions

### Performance (priority: low)

- Profile MIDI compilation for large scores (>100 bars, >20 parts)
- Profile corpus analysis across >50 works
- Evaluate whether hot paths cross translation unit boundaries and benefit from LTO

---

## Metrics and KPIs

### Performance Targets

| Metric | Target | Bound |
|--------|--------|-------|
| TCP command latency (WSL2 → Windows) | <50ms | 200ms |
| Theory engine computation (per tool call) | <10ms | 50ms |
| MIDI compilation (32-bar, 4-part score) | <5ms | 20ms |
| MusicXML/LilyPond compilation | <50ms | 200ms |
| Test suite (1,560 tests) | <10s | 30s |

Performance targets are estimates; measurement requires the integration test harness.

### Quality Gates

| Gate | Condition |
|------|-----------|
| G1 | All 1,560 tests pass |
| G2 | Compilation with `-Wall -Wextra -Wpedantic -Werror` (GCC/Clang) |
| G3 | No Error-level validation diagnostics in Score IR test fixtures |
| G4 | Score serialisation round-trip (JSON → Score → JSON) preserves all fields |

---

## Risk Assessment

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| WSL2 TCP latency exceeds target | Medium | Medium | Profile; fall back to shared file IPC if needed |
| Ableton LOM API limitations | High | Medium | Document unsupported operations; degrade gracefully |
| Large corpus memory pressure | Low | Medium | Streaming analysis; limit in-memory work count |
| libc++ vs libstdc++ ABI differences | Low | Low | CI tests on both; pin to libc++-18 for Mull |

### Operational Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Ableton version incompatibility | Medium | High | Test against Live 12; document minimum version |
| Port 9001 conflict | Medium | Low | Configurable via TcpTransport constructor |
| Remote Script installation path changes | Low | Low | Document symlink procedure per OS |
