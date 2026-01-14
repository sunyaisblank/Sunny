# Sunny - Project Roadmap

**Document ID:** SUNNY-GOV-ROAD-001  
**Version:** 1.0  
**Date:** December 2025  
**Status:** Active Development (Phase 0)  
**Governance:** Compliant with `Structure.md` and `Standards.md`

---

## Table of Contents

1. [Executive Summary](#executive-summary)
2. [Vision & Goals](#vision--goals)
3. [Current State Assessment](#current-state-assessment)
4. [Development Phases](#development-phases)
5. [Metrics & KPIs](#metrics--kpis)
6. [Risk Assessment](#risk-assessment)

---

## Executive Summary

**Sunny** aims to provide a professional-grade MCP server granting AI agents **total agency** over Ableton Live, enabling:

- **Composition**: Scale-aware MIDI, Roman Numeral Analysis, polyrhythmic patterns
- **Mixing**: Auto-gain staging, signal routing, device parameter control
- **Arrangement**: Timeline-aware clip placement, locators, Cut/Copy/Paste

### Core Principles

1. **Music Theory First**: Every MIDI operation respects the project's scale/mode context
2. **Low-Latency Engineering**: Hybrid TCP/UDP for <5ms parameter modulation
3. **Safety by Default**: Automatic Project Snapshots before destructive operations
4. **Professional Quality**: Features that enable radio-ready productions

---

## Vision & Goals

### Short-Term (v0.1.0)
- [ ] Basic MCP server with session/track tools
- [ ] TCP communication with Remote Script
- [ ] Theory engine with major/minor scales

### Medium-Term (v0.2.0)
- [ ] Full mode support (all 7 church modes)
- [ ] Chord progression generation (Roman numerals)
- [ ] Device parameter mapping

### Long-Term (v1.0.0)
- [ ] UDP/OSC real-time control
- [ ] Auto-gain staging
- [ ] Arrangement view timeline tools
- [ ] VST deep scan and control

---

## Current State Assessment

### Prior Art Analysis

| Project | Strengths | Weaknesses |
|---------|-----------|------------|
| `ahujasid/ableton-mcp` | Stable, well-documented | No theory engine, TCP-only |
| `uisato/ableton-mcp-extended` | UDP support, device params | Experimental UDP, no safety |
| `xiaolaa2/ableton-copilot-mcp` | N/A | Limited documentation |

### Technology Stack

| Component | Technology | Rationale |
|-----------|------------|-----------|
| MCP Server | FastMCP (Python) | Official SDK, async support |
| Theory Engine | music21 | Industry-standard musicology |
| Transport | TCP + UDP/OSC | Reliability + low-latency |
| Remote Script | Python (Ableton API) | Native Ableton integration |
| M4L Device | Max/MSP | Sample-accurate MIDI |

---

## Development Phases

### Phase 0: Project Setup ✅
- [x] Research existing implementations
- [x] Create governance structure
- [ ] Create implementation plan
- [ ] User review and approval

### Phase 1: Foundation (2 weeks)
- FastMCP server skeleton
- TCP transport layer
- Basic Remote Script
- Core tools: session info, track management
- Snapshot system for Creative Undo

**Deliverables:**
- Working MCP server that can query Ableton session
- CRUD operations for tracks and clips

### Phase 2: Theory Engine (2 weeks)
- music21 integration
- Scale/mode awareness (all 7 modes)
- Roman Numeral chord generation
- Quantization with swing support
- Polyrhythm and tuplet generation

**Deliverables:**
- `generate_progression` tool with theory awareness
- `apply_quantization` with musical intelligence

### Phase 3: Mixing & Engineering (2 weeks)
- Device parameter deep scan
- Auto-gain staging (-6dB headroom)
- Signal routing (sidechain, sends, groups)
- UDP/OSC for real-time modulation

**Deliverables:**
- Professional mixing tools
- Sub-5ms parameter control

### Phase 4: Integration & Polish (1 week)
- End-to-end testing
- Documentation
- Example workflows
- Performance optimization

---

## Metrics & KPIs

### Performance Targets

| Metric | Target | Bound | Priority |
|--------|--------|-------|----------|
| TCP Command Latency | <50ms | 200ms | P0 |
| UDP/OSC Latency | <5ms | 20ms | P0 |
| Theory Engine Compute | <10ms | 50ms | P1 |
| Test Coverage | 100% | 80% | P1 |

### Quality Gates

| Gate | Condition | Enforcement |
|------|-----------|-------------|
| **G1** | All unit tests pass | CI/CD block |
| **G2** | No unsafe operations without snapshot | Code review |
| **G3** | Type hints on all public APIs | Ruff check |
| **G4** | Docstrings on all tools | Ruff check |

---

## Risk Assessment

### Technical Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| Ableton API limitations | High | Medium | Document workarounds |
| VST parameter access | High | Low | Manual Configure fallback |
| UDP packet loss | Low | Medium | Implement retry logic |
| music21 compatibility | Low | Low | Pin version |

### Operational Risks

| Risk | Probability | Impact | Mitigation |
|------|-------------|--------|------------|
| User data loss | Medium | Critical | Mandatory snapshots |
| Ableton crashes | Low | Medium | Graceful reconnection |
| Port conflicts | Medium | Low | Configurable ports |

---

## Appendix: Tool Registry

### Phase 1 Tools

| Tool | Category | Description |
|------|----------|-------------|
| `get_session_info` | Session | Get tempo, time sig, scale |
| `set_tempo` | Session | Set project tempo |
| `create_midi_track` | Track | Create MIDI track |
| `create_audio_track` | Track | Create audio track |
| `delete_track` | Track | Delete track (with snapshot) |
| `get_track_info` | Track | Get track details |
| `create_clip` | Clip | Create MIDI/audio clip |
| `add_notes_to_clip` | Clip | Add MIDI notes |
| `get_browser_tree` | Browser | Get browser category tree |
| `get_browser_items_at_path` | Browser | Navigate to browser path |
| `load_instrument_or_effect` | Browser | Load instrument by URI |
| `load_drum_kit` | Browser | Load drum rack and kit |

### Phase 2 Tools

| Tool | Category | Description |
|------|----------|-------------|
| `generate_progression` | Theory | Roman numeral chords |
| `apply_quantization` | Theory | Scale-aware quantize |
| `generate_melody` | Theory | Scale-aware melody |
| `add_rhythm_pattern` | Theory | Polyrhythm generation |
| `set_project_scale` | Theory | Set global scale/mode |

### Phase 3 Tools

| Tool | Category | Description |
|------|----------|-------------|
| `deep_scan_devices` | Device | Map all parameters |
| `set_device_parameter` | Device | Control device param |
| `auto_gain_stage` | Mixer | Maintain headroom |
| `create_sidechain` | Mixer | Sidechain routing |
| `create_send` | Mixer | Send to return track |
| `create_group` | Mixer | Group tracks |
