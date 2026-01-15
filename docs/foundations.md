# Technical Foundations

*From Music Theory to Real-Time Control*

> "Without music, life would be a mistake."
> — Friedrich Nietzsche

## Overview

This document develops the technical foundations underlying Sunny. We proceed from music theory fundamentals through system architecture to the protocols that enable real-time control. Each section builds upon the previous; the reader who works through sequentially will understand the mechanics of the system.

Prerequisites: basic familiarity with music notation and digital audio workstations. Prior exposure to networking or API design is helpful; the relevant concepts are developed from first principles.

---

## Part I: Music Theory Fundamentals

### 1.1 Pitch and Frequency

Sound is vibration. When an object vibrates at a regular rate, it produces a pitch. The frequency of vibration, measured in Hertz (cycles per second), determines the pitch we perceive.

**Octave equivalence.** Pitches separated by a frequency ratio of 2:1 are perceived as equivalent. A4 vibrates at 440 Hz; A5 vibrates at 880 Hz. They share the same letter name because they share a perceptual quality.

**Equal temperament.** The Western twelve-tone system divides the octave into twelve equal intervals called semitones. The frequency ratio between adjacent semitones is $2^{1/12} \approx 1.0595$.

**MIDI representation.** MIDI assigns integers to pitches. Middle C is 60. Each semitone adds 1. The frequency of MIDI note $n$ is:

$$
f(n) = 440 \times 2^{(n-69)/12}
$$

### 1.2 Intervals

An interval is the distance between two pitches, measured in semitones.

| Semitones | Name | Example |
|-----------|------|---------|
| 0 | Unison | C to C |
| 1 | Minor second | C to C♯ |
| 2 | Major second | C to D |
| 3 | Minor third | C to E♭ |
| 4 | Major third | C to E |
| 5 | Perfect fourth | C to F |
| 7 | Perfect fifth | C to G |
| 12 | Octave | C to C |

Intervals have characteristic sounds. The perfect fifth sounds consonant; the minor second sounds dissonant. These qualities emerge from the frequency ratios involved.

### 1.3 Scales

A scale is a collection of pitches used as the basis for melody and harmony. Scales are defined by their interval pattern: the sequence of steps between adjacent degrees.

**The major scale.** Interval pattern: 2-2-1-2-2-2-1 (whole, whole, half, whole, whole, whole, half).

C major: C D E F G A B C

**The natural minor scale.** Interval pattern: 2-1-2-2-1-2-2.

A minor: A B C D E F G A

**Modes.** The seven modes of the major scale arise from starting on different degrees:

| Mode | Starting degree | Pattern |
|------|-----------------|---------|
| Ionian | 1 | 2-2-1-2-2-2-1 |
| Dorian | 2 | 2-1-2-2-2-1-2 |
| Phrygian | 3 | 1-2-2-2-1-2-2 |
| Lydian | 4 | 2-2-2-1-2-2-1 |
| Mixolydian | 5 | 2-2-1-2-2-1-2 |
| Aeolian | 6 | 2-1-2-2-1-2-2 |
| Locrian | 7 | 1-2-2-1-2-2-2 |

Sunny's scale system extends beyond the church modes to include symmetric scales, bebop scales, and exotic tonalities.

### 1.4 Chords

A chord is a simultaneous combination of pitches. Chords are typically built by stacking thirds.

**Triads.** Three-note chords built from root, third, and fifth.

| Type | Intervals | Example |
|------|-----------|---------|
| Major | 4-3 | C E G |
| Minor | 3-4 | C E♭ G |
| Diminished | 3-3 | C E♭ G♭ |
| Augmented | 4-4 | C E G♯ |

**Seventh chords.** Four-note chords adding the seventh above the root.

| Type | Intervals | Example |
|------|-----------|---------|
| Major seventh | 4-3-4 | C E G B |
| Dominant seventh | 4-3-3 | C E G B♭ |
| Minor seventh | 3-4-3 | C E♭ G B♭ |
| Diminished seventh | 3-3-3 | C E♭ G♭ B♭♭ |

### 1.5 Functional Harmony

Chords function according to their role in establishing, departing from, and returning to tonal stability.

**Tonic (T).** The chord of rest and arrival. In C major, C major is tonic.

**Subdominant (S).** Chords that depart from tonic. In C major, F major and D minor are subdominant.

**Dominant (D).** Chords that create tension seeking resolution to tonic. In C major, G major and B diminished are dominant.

The fundamental progression is T → S → D → T. Elaborations and substitutions provide variety whilst respecting the underlying logic.

Sunny classifies chords by function:

$$
F(c) \in \{T, S, D\}
$$

where $c$ is a chord and $F$ returns its functional role.

---

## Part II: Voice Leading

### 2.1 The Problem

Given two chords, how should the notes be distributed among voices? Many voicings are possible; some sound better than others. Voice leading is the art of choosing voicings that produce smooth, coherent progressions.

### 2.2 Principles

**Minimise motion.** Voices should move by small intervals. A voice moving by step is smoother than a voice leaping by a fifth.

**Avoid parallel fifths and octaves.** Two voices moving in parallel at the interval of a fifth or octave produce a hollow sound. This convention emerges from Renaissance counterpoint and remains in force.

**Resolve tendency tones.** Certain notes demand specific resolutions. The leading tone (seventh degree) resolves up to tonic. The seventh of a dominant chord resolves down by step.

### 2.3 The Nearest-Tone Algorithm

Sunny implements voice leading through the nearest-tone algorithm. For each note in the target chord, find the nearest note in the source chord. This minimises aggregate voice motion.

For chords $C_1$ and $C_2$ with voicings $V_1$ and $V_2$, the algorithm minimises:

$$
\sum_{i=1}^{n} |V_2^{(i)} - V_1^{(i)}|
$$

subject to:
- No parallel fifths
- No parallel octaves
- Proper tendency tone resolution

The algorithm does not guarantee optimal results in all cases. It provides reasonable defaults that the user may override.

---

## Part III: System Architecture

### 3.1 Component Overview

Sunny comprises four components:

**MCP Server.** The interface between AI and the system. Exposes tools for project manipulation, theory queries, and device control.

**Theory Engine.** The music theory implementation. Provides scales, harmony, voice leading, cadences, and orchestration.

**Transport Layer.** The communication with Ableton Live. Hybrid TCP/UDP protocol for reliable commands and low-latency parameter control.

**Remote Script.** The Ableton Live extension. Receives commands and reports state through the transport layer.

### 3.2 Data Flow

```
AI Client → MCP Server → Transport → Remote Script → Ableton Live
                ↑                          ↓
           Theory Engine              State Updates
```

The AI client issues commands through MCP. The server processes commands, consulting the theory engine as needed. Commands flow to the transport layer, which forwards them to the remote script. The remote script manipulates Ableton Live. State changes flow back through the same path.

### 3.3 The Theory Engine

The theory engine comprises several modules:

**ScaleSystem.** Manages scale definitions and note generation. Given a root and scale name, returns the constituent pitches.

**HarmonyModule.** Classifies chords by function. Generates chords from functional specifications.

**VoiceLeadingEngine.** Computes optimal voice distributions. Implements the nearest-tone algorithm with constraint checking.

**CadenceEngine.** Generates and detects cadential patterns. Authentic, plagal, deceptive, and half cadences.

**OrchestrationGuide.** Maps emotional intent to instrumentation. Associates frequency roles with instrument families.

---

## Part IV: Transport Layer

### 4.1 Protocol Design

The transport layer uses a hybrid protocol:

**TCP for structural commands.** Track creation, device insertion, clip manipulation. These operations must complete reliably; retransmission is acceptable.

**UDP for parameter modulation.** Filter cutoff, volume faders, send levels. These operations must occur with minimal latency; occasional loss is acceptable.

### 4.2 OSC Message Format

Parameter messages use Open Sound Control (OSC) format:

```
/track/{index}/device/{index}/param/{index} {value}
```

Values are normalised to the unit interval [0, 1] regardless of the underlying parameter range. The remote script denormalises values before applying them.

### 4.3 Latency Requirements

Real-time parameter control requires sub-5ms latency. This constraint informs protocol design:

**UDP.** No connection overhead; no acknowledgement wait.

**Local networking.** Loopback or local subnet minimises network latency.

**Minimal processing.** Message parsing and value mapping are optimised for speed.

---

## Part V: Snapshot System

### 5.1 Purpose

The snapshot system provides reversibility. Before destructive operations, the system captures project state. If the operation produces unwanted results, the previous state can be restored.

### 5.2 Implementation

Snapshots capture:
- Track structure (names, types, routing)
- Clip arrangement (positions, lengths, content)
- Device chains (devices, parameters)
- Mixer state (volumes, pans, sends)

Snapshots are stored in the configured snapshot directory with timestamps. A retention policy limits storage consumption.

### 5.3 Retention Policy

| Parameter | Default |
|-----------|---------|
| Maximum snapshots | 50 |
| Maximum age | 24 hours |

Snapshots exceeding these limits are deleted oldest-first.

---

## Part VI: MCP Interface

### 6.1 The Model Context Protocol

MCP (Model Context Protocol) defines how AI models interact with external tools. Tools are functions the model can invoke; resources are data the model can read.

Sunny exposes tools for:
- Project queries (get tracks, get clips, get devices)
- Project modification (create track, add clip, set parameter)
- Theory operations (get scale, analyse chord, suggest voicing)
- Snapshot management (create snapshot, restore snapshot)

### 6.2 Tool Structure

Each tool has:
- **Name.** Identifier for invocation.
- **Description.** Explanation of function.
- **Parameters.** Typed arguments with defaults.
- **Return value.** Structured response.

Example:

```python
@mcp.tool()
async def get_scale_notes(
    root: str,
    scale_type: str = "major"
) -> list[int]:
    """Get MIDI notes for a scale.

    Args:
        root: Root note (e.g., "C", "F#")
        scale_type: Scale name (e.g., "major", "dorian")

    Returns:
        List of MIDI note numbers
    """
    return theory.get_scale_notes(root, scale_type)
```

### 6.3 Lifespan Management

The MCP server manages component lifecycles:

**Startup.** Initialise theory engine, connect to Ableton, create snapshot manager.

**Operation.** Process tool invocations, maintain state.

**Shutdown.** Disconnect gracefully, preserve final state.

---

## Part VII: Integration

### 7.1 Ableton Live Remote Script

The remote script is a Python module installed in Ableton's User Library. It:

- Receives commands over the transport layer
- Translates commands to Live API calls
- Reports state changes back to the server

The script runs within Ableton's Python environment. It accesses the Live Object Model (LOM) to query and manipulate the session.

### 7.2 State Synchronisation

Ableton Live is the source of truth. The remote script reports changes; the server maintains a cached view. Cache invalidation occurs on:

- Explicit refresh requests
- Detected state changes
- Periodic polling (fallback)

---

## Conclusion

Sunny integrates music theory, real-time communication, and AI interface into a coherent system. The theory engine encodes musical knowledge in executable form. The transport layer provides reliable and low-latency communication. The snapshot system ensures reversibility. The MCP interface exposes capability to AI models.

Understanding these foundations enables effective use and customisation. The theory engine can be extended with additional scales or harmonic systems. The transport layer can be adapted for alternative DAWs. The MCP interface can expose additional capabilities as needed.

*That is the technical foundation of Sunny.*
