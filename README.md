# Sunny

*An MCP server for AI-assisted music production*

Sunny provides AI agents with structured control over Ableton Live. The system combines a music theory engine with real-time transport, enabling operations from chord generation through parameter modulation.

## Motivation

Music production involves two kinds of knowledge: theoretical and practical. Theoretical knowledge concerns scales, harmony, voice leading, and orchestration. Practical knowledge concerns the DAW: where controls are, what parameters do, how sessions are structured.

AI models possess general knowledge but lack the specific interface to apply it. They cannot press buttons, turn knobs, or hear results. Sunny bridges this gap. The theory engine makes musical knowledge executable. The transport layer makes the DAW controllable. Together they enable AI participation in music production.

## Principles

**Theory first.** Musical operations should be expressed in musical terms. The AI requests a dominant chord, not MIDI notes 67, 71, 74. The theory engine translates between musical intention and technical implementation.

**Real-time response.** Parameter modulation requires low latency. The hybrid transport layer uses UDP for time-critical operations, achieving sub-5ms response.

**Reversibility.** Creative exploration requires the freedom to make mistakes. The snapshot system captures state before destructive operations, enabling recovery.

**Bounded agency.** The AI operates within defined limits. Operations that could cause irreversible harm require explicit confirmation.

## Architecture

| Component | Purpose |
|-----------|---------|
| `src/sunny/server/` | MCP interface and transport layer |
| `src/sunny/theory/` | Music theory engine |
| `src/sunny/test/` | Test suite |
| `lib/remotescript/` | Ableton Live extension |
| `docs/` | Documentation |

## Theory Engine

The theory engine encodes musical knowledge in executable form.

**Scales.** Over forty tonalities: church modes, symmetric scales, bebop scales, exotic modes. Given a root and scale type, returns constituent pitches.

**Functional harmony.** Chords classified by role: tonic, subdominant, dominant. Given a key and function, returns appropriate chords.

**Voice leading.** The nearest-tone algorithm for smooth progressions. Given two chords, computes optimal voice distribution minimising aggregate motion.

**Orchestration.** Instrument suggestions by emotional intent and register. Given a mood, returns characteristic instrument combinations.

## Transport Layer

Communication with Ableton Live uses a hybrid protocol.

**TCP** for structural commands: track creation, device insertion, clip manipulation. These operations must complete reliably.

**UDP** for parameter modulation: filter sweeps, volume automation, send adjustments. These operations must occur with minimal latency.

The remote script translates protocol messages to Live API calls. It runs within Ableton's Python environment and accesses the Live Object Model directly.

## Installation

```bash
# Clone and install
git clone https://github.com/averagestudentdontfail/Sunny.git
cd Sunny
make install

# Install remote script
cp -r lib/remotescript ~/Music/Ableton/User\ Library/Remote\ Scripts/Sunny
```

Configure Ableton: Preferences → Link, Tempo & MIDI → Control Surface → Sunny

## Configuration

Add to your AI assistant's MCP configuration:

```json
{
  "mcpServers": {
    "Sunny": {
      "command": "python",
      "args": ["-m", "sunny.server"],
      "cwd": "/path/to/Sunny"
    }
  }
}
```

Environment variables (see `.env.example`):

| Variable | Purpose | Default |
|----------|---------|---------|
| `SUNNY_LOG_LEVEL` | Logging verbosity | `INFO` |
| `SUNNY_SNAPSHOT_DIR` | Snapshot storage | `~/.sunny/snapshots` |
| `SUNNY_TCP_PORT` | Reliable transport | `9000` |
| `SUNNY_UDP_PORT` | Low-latency transport | `9001` |

## Development

```bash
make check      # Run all checks
make test       # Run tests with coverage
make lint       # Run linter
make typecheck  # Run type checker
make format     # Format code
```

## Documentation

The `docs/` directory contains comprehensive documentation:

| Document | Purpose |
|----------|---------|
| [philosophy.md](docs/philosophy.md) | Design worldview |
| [foundations.md](docs/foundations.md) | Technical foundations |
| [guide.md](docs/guide.md) | Practical operation |
| [standard.md](docs/standard.md) | Coding conventions |

## Requirements

- Python 3.10 or later
- Ableton Live 11 or later
- Dependencies: mcp, music21, python-osc

## Licence

MIT
