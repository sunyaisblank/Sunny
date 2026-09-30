# Sunny

Sunny lets an AI agent compose music in Ableton Live using musical ideas rather than raw MIDI
numbers. An agent connected over the Model Context Protocol (MCP) can ask for a Lydian melody, a
ii-V-I in B-flat, a string section seated in the European layout, or a crescendo into bar 17.
Sunny turns each request into a checked musical document, and from there into notation files or
into tracks, clips, devices and mixer settings in a running Live Set.

Most of Sunny is a C++23 music engine that works without Ableton. Only the final step, applying a
document to a Live Set, needs Live.

## What it does

Sunny represents music as four kinds of document. Each owns one concern:

| Document | Holds | Can be turned into |
|---|---|---|
| Score | parts, notes, rhythm, harmony, dynamics, form | MIDI events, Standard MIDI Files, MusicXML, LilyPond, Live clips |
| Timbre | sound sources, effects, modulation, presets | Live device chains |
| Mix | channels, buses, sends, levels, panning | Live mixer settings |
| Corpus | ingested MIDI and MusicXML works, style statistics | composer and period profiles |

Beneath the documents sits a theory library covering pitch and spelling, scales, chords and
Roman-numeral analysis, voice leading and species counterpoint, rhythm, tuning systems, post-tonal
set theory and acoustics. Pitch and time arithmetic is exact: durations are rational numbers and
overflow is checked, and the engine refuses an invalid request rather than guessing.

Everything is exposed as MCP tools by the `sunny-mcp` program, which speaks JSON-RPC on standard
input and output. `tools/list` returns the current inventory. The same engine is also available
from Python.

```
AI client ──MCP (stdio)──▶ sunny-mcp ──TCP 9001──▶ Sunny Remote Script ──▶ Ableton Live
                             │
                             └── theory engine and documents (no Live needed)
```

A project deployment to Live is guarded. `project_plan_to_ableton` records a snapshot of the
Live Set and the exact list of changes without touching Live. `project_apply_ableton_plan`
applies that plan once, refuses if the Set has changed in the meantime, and returns a journal of
every change it attempted, including after a partial failure.

A separate `max-package/` builds five Max externals (`sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`,
`sunny.clock~`, `sunny.events`) from the same render code. See `max-package/readme.md`.

## Building

You need CMake 3.28 or later, a C++23 compiler (GCC 13+ or Clang 16+) and, for the optional
Python bindings, Python 3.10+ with development headers.

```bash
cmake --preset release          # configure into .bin/
cmake --build --preset release
ctest --preset release          # C++ tests
```

`cmake --preset debug` builds into `.bin-debug/`. To install the server, libraries, headers and
CMake package files, run `cmake --install .bin --prefix /your/prefix`.

The Makefile wraps common tasks:

```bash
make                  # build and run the C++ tests
make python-check     # ruff, mypy, native stub comparison, Python tests
make package-check    # build a separate program against the installed package
make max-sdk-header-check  # compile the Max wrappers against the pinned Max SDK headers
make coverage         # line and branch coverage (clang)
make analysis         # CodeQL queries and Mull mutation testing
make help
```

Source files are listed explicitly in CMake, so re-run the configure step after adding one.

## Using it with an AI client

Point your MCP client at the built server:

```json
{
  "mcpServers": {
    "sunny": {
      "command": "/path/to/Sunny/.bin/sunny-mcp",
      "env": { "SUNNY_ABLETON_HOST": "127.0.0.1", "SUNNY_TCP_PORT": "9001" }
    }
  }
}
```

Without `SUNNY_ABLETON_HOST` the server runs offline: theory, document and notation tools work,
and tools that change Live decline with an explicit error. `SUNNY_TCP_PORT` defaults to 9001.
Under WSL2 with Live on the Windows host, use the Windows host's IP address.

## Connecting Ableton Live

Copy the Remote Script into the `Remote Scripts` folder of your Live User Library and select it
as a control surface (Preferences, Link/Tempo/MIDI, Control Surface: Sunny):

```bash
cp -r remote_script/Sunny "${HOME}/Music/Ableton/User Library/Remote Scripts/Sunny"
```

The script listens on TCP port 9001, bound to `127.0.0.1` unless `SUNNY_BIND_HOST` is set in the
environment of the Live process. If you bind to another interface, restrict access with a
firewall: the port accepts commands that change your Live Set.

Note writing needs Live 11 or later. Inserting native devices for Timbre and Mix uses
`Track.insert_device`, which needs Live 12.3 or later and supports Live's built-in devices only.
Each compile result reports what Live could not represent (for example third-party plug-ins or
features the Live API does not expose) instead of silently dropping it.

## Python

The optional `sunny` package is a thin layer over the same engine:

```bash
uv build --wheel && uv pip install dist/sunny-*.whl    # or `uv sync` in a checkout
```

```python
from sunny.core.engine import TheoryEngine

engine = TheoryEngine()
engine.get_scale_notes("C", "lydian", octave=4)
engine.generate_progression("C", "major", ["I", "vi", "IV", "V"])
```

The package raises an error if its native module is missing rather than approximating.

## How it is tested

Development does not require Ableton. The C++ and Python suites exercise the engine, the
documents, the notation and MIDI writers, the MCP server and the TCP bridge. Live itself is
represented in tests by fakes of its Python API, judged against Ableton's documented behaviour.
CI builds and tests on Linux and macOS and builds the Max externals on macOS and Windows.

## Limitations

- Sunny has not yet been run against a real Live Set end to end. Behaviour that Ableton's
  documentation does not settle is listed in GitHub issue #22 and will be checked in a final live
  test from a separate machine running Live.
- Known defects and their status are tracked as GitHub issues labelled `remediation`.
- Documents live in the server's memory for the life of the process. Score, Mix and Corpus
  documents can be exported with `score_get_json`, `get_mix_json` and `get_corpus_json`; Timbre
  profiles have no JSON export tool yet.
- Audio is never rendered or analysed, so nothing Sunny reports is a claim about how the result
  sounds. Loudness targets and reference comparisons are intentions, not measurements.
- Live's current-scale setting is readable but not written.
- The Max externals have been built and checked against the Max SDK headers, but not yet loaded
  in Max or Max for Live.

## Repository layout

| Path | Contents |
|---|---|
| `include/sunny/`, `src/` | C++ engine: `core` (theory and documents), `render`, `max`, `infrastructure` (formats, MCP, Ableton bridge) |
| `apps/` | The `sunny-mcp` entry point |
| `remote_script/Sunny/` | The Ableton Live Remote Script and the shared bridge contract |
| `python/sunny/` | Python package |
| `max-package/` | Max externals, help patchers and reference pages |
| `tests/` | C++ and Python tests, mirroring the source layout |
| `docs/formal/` | Specifications of the music model, the four documents, projects and rendering |
| `cmake/`, `.analysis/` | Build policy, layering checks and static-analysis queries |

The layering rule is enforced at configure time: `core` depends on nothing else in Sunny,
`render` only on `core`, and `infrastructure` on both.

## Licence

MIT
