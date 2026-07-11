# Sunny

*A music theory engine that gives AI agents compositional agency over
Ableton Live.*

Sunny is a C++23 engine that makes musical knowledge executable and a
DAW controllable. An AI agent connected over MCP can reason in musical
terms (a dominant chord, a Lydian melody, a crescendo into bar 17) and
have those intentions realised as structured documents, standard
notation formats, or clips in a running Ableton Live session.

## How it works

The engine models music as typed documents with formal contracts. Four
intermediate representations each own one concern:

| IR | Concern | Compiles to |
|---|---|---|
| Score | Notes, rhythm, harmony, form | MIDI, MusicXML, LilyPond, Ableton clips |
| Timbre | Sound design: sources, effects, modulation | Ableton device chains |
| Mix | Signal flow, levels, spatialisation | Ableton mixer configuration |
| Corpus | Ingested works and style analysis | Composer profiles |

Underneath sits a theory library covering pitch (as Z/12Z group
operations, with spelled pitches on the line of fifths), scales,
functional and chromatic harmony, voice leading, rhythm, tuning
systems, post-tonal techniques, and acoustics. Arithmetic on pitch and
time is exact: pitch classes are group elements, durations are
rationals, and overflow is checked. The engine prefers refusing an
invalid operation to guessing at a plausible one.

Everything is exposed as 93 MCP tools by the `sunny-mcp` binary, a
JSON-RPC server on stdio. When `SUNNY_ABLETON_HOST` is set, the server
connects to a Remote Script inside Ableton Live over TCP and the
Ableton-mutating tools go live; without it, the server runs offline and
those tools decline with a clear error while the theory and document
tools keep working.

```
AI client ──MCP/stdio──▶ sunny-mcp ──TCP 9001──▶ SunnyRemoteScript ──▶ Ableton Live
                           │
                           └── Sunny.Core (theory + IR documents)
```

## Status

The engine, the four IRs, the compilation targets, and the MCP surface
are implemented and covered by more than 1,500 tests, with mutation
testing and static analysis in the harness. The transport and the
Remote Script are verified against each other by loopback tests; the
remaining milestone is end-to-end validation against a live Ableton
session. `docs/decisions.md` records the engineering decisions and
their evidence.

## Building

Requires CMake 3.20+, a C++23 compiler (GCC 13+ or Clang 16+), and
Python 3.11+ with development headers for the optional bindings.

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

Or via the Makefile, which also drives the analysis harness:

```bash
make            # build + all tests
make coverage   # llvm-cov line/branch coverage
make analysis   # CodeQL static analysis + Mull mutation testing
make help       # everything else
```

## Connecting an AI client

Add the server to your MCP client configuration:

```json
{
  "mcpServers": {
    "sunny": {
      "command": "/path/to/Sunny/.bin/sunny-mcp",
      "env": {
        "SUNNY_ABLETON_HOST": "127.0.0.1",
        "SUNNY_TCP_PORT": "9001"
      }
    }
  }
}
```

Omit the `env` block to run offline. Under WSL2 with Ableton on the
Windows host, use the host IP from `/etc/resolv.conf` as
`SUNNY_ABLETON_HOST`.

## Connecting Ableton Live

Copy the Remote Script into Ableton's user library and select it as a
control surface:

```bash
cp -r src/SunnyRemoteScript "~/Music/Ableton/User Library/Remote Scripts/Sunny"
```

Then in Live: Preferences → Link, Tempo & MIDI → Control Surface →
Sunny. The script listens on TCP 9001 and translates length-prefixed
JSON requests into Live Object Model calls.

## Python scripting

The optional `sunny` package is a thin facade over the same engine via
pybind11 — useful for notebooks and scripts, not required for the MCP
server:

```python
from sunny.core.engine import TheoryEngine

engine = TheoryEngine()
engine.get_scale_notes("C", "lydian", octave=4)
engine.generate_progression("C", "major", ["I", "vi", "IV", "V"])
```

The package raises rather than falling back to approximations when the
native module is absent; build with the `release` preset first.

## Development

```bash
make test           # C++ suite (Catch2)
make python-check   # ruff + mypy + pytest for the Python facade
make mull           # mutation testing (clang-18 + libc++)
make codeql         # custom CodeQL queries
```

Sources follow a codename scheme (`HRCD001A` = Harmony domain, Cadence
category, component 001, revision A); the registry lives in
[docs/reference.md](docs/reference.md). The five formal specifications
in [docs/formal/](docs/formal/) are normative — source files cite them
by section number.

## Documentation

| Document | Contents |
|---|---|
| [docs/formal/](docs/formal/) | Formal specifications: music theory, and the Score, Timbre, Mix, and Corpus IRs |
| [docs/reference.md](docs/reference.md) | Codename registry, error-code ranges, operation contracts, schema versions, wire protocol |
| [docs/decisions.md](docs/decisions.md) | Decision record and audit history |

## Licence

MIT
