# Sunny

Sunny is a professional-grade MCP server designed to provide AI agents with complete agency over Ableton Live. The system implements a comprehensive music theory engine, real-time parameter modulation, and intelligent project management, enabling autonomous music production from initial composition through mixing to final master.

## Music Theory Engine

The theoretical foundation encompasses functional harmony analysis, voice leading algorithms, and a comprehensive scale system supporting over forty distinct tonalities. The theory engine classifies chords according to their functional role within a key:

$$
F(c) \in \{T, S, D\}
$$

where tonic function $T$ provides stability, subdominant function $S$ creates departure, and dominant function $D$ generates tension seeking resolution. This classification extends to secondary dominants, modal interchange chords, and the complete family of augmented sixth preparations.

The voice leading engine implements the nearest-tone algorithm for smooth chord progressions, minimising the aggregate voice motion while respecting part-writing constraints. For successive chords $C_1$ and $C_2$ with voicings $V_1$ and $V_2$, the algorithm minimises:

$$
\sum_{i=1}^{n} |V_2^{(i)} - V_1^{(i)}|
$$

subject to no parallel fifths, no parallel octaves, and proper resolution of tendency tones.

## Scale System

The scale system extends beyond the seven church modes to encompass symmetric scales, bebop scales, and exotic tonalities from various musical traditions. Symmetric scales exhibit interval patterns that map onto themselves under transposition by their generating interval. The octatonic scale alternates half and whole steps, producing a scale invariant under minor third transposition. The whole-tone scale consists entirely of major seconds, yielding invariance under major second transposition.

The bebop scales insert chromatic passing tones to align chord tones with strong beats during eighth-note runs. Double harmonic scales feature augmented seconds creating characteristic Middle Eastern and Romani sonorities.

## Orchestration Guidance

The orchestration module provides instrument suggestions based on emotional intent and voice register. Instruments are classified by their frequency role—bass, tenor, alto, soprano, and super—enabling appropriate voice assignment for chord voicings. Emotional colours including heroic, melancholy, ethereal, climactic, and pastoral map to characteristic instrument combinations drawn from orchestral practice.

## Transport Layer

The system implements hybrid TCP/UDP communication with Ableton Live. Reliable commands for session queries, track creation, and device configuration traverse TCP with acknowledgement. Low-latency parameter modulation employs UDP with OSC message formatting, achieving sub-5ms latency suitable for real-time performance control.

The OSC address space follows the hierarchical structure:

```
/track/{index}/device/{index}/param/{index} {value}
```

where values are normalised to the unit interval regardless of the underlying parameter range.

## Project Snapshots

Before any destructive operation, the system automatically creates a timestamped project snapshot enabling creative undo functionality. The snapshot manager enforces a configurable retention policy, removing oldest snapshots when the maximum count is exceeded. This safety mechanism permits exploratory editing without risk of irreversible loss.

## Architecture

The system is organised into domain-specific components. `Sunny.Server` implements the FastMCP server with lifespan management, TCP/UDP transport, and the complete tool registry. `Sunny.Theory` contains the music theory engine including harmony analysis, voice leading, cadence management, scale system, and orchestration guidance. `Sunny.Test` maintains the test suite with over 125 passing tests covering all functional modules. `Sunny.RemoteScript` provides the Ableton Live Remote Script for bidirectional communication. `Sunny.Governance` maintains the structural compliance standards and development roadmap.

## Installation

Clone the repository and install dependencies:

```bash
git clone https://github.com/averagestudentdontfail/Sunny.git
cd Sunny
pip install -e .
```

Copy the Remote Script to the Ableton User Library and enable it in Preferences under Control Surface.

## Setup

Once the Remote Script is installed and your AI client is configured (see Installation and Configuration), establishing a connection requires only two steps. Start Ableton Live first, ensuring the Sunny control surface is selected in Preferences under Link, Tempo & MIDI. Then start your AI client, which will automatically spawn the Sunny MCP server and connect to the Remote Script. The connection is established immediately and your AI assistant can begin invoking Sunny tools to control Ableton in real time.

## Configuration

Add to your AI assistant's MCP configuration:

```json
{
  "mcpServers": {
    "Sunny": {
      "command": "python",
      "args": ["-m", "Sunny.Server.main"],
      "cwd": "/path/to/Sunny"
    }
  }
}
```

## Documentation

The governance documentation in `Sunny.Governance` establishes naming conventions, technical standards, and the development roadmap. Component specifications are maintained in the module docstrings following Google style conventions.
