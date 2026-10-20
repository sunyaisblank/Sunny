# Developing Sunny

The engine and ordinary development tests work without Ableton. Start with CMake and the affected product workflow; the optional analysis tools answer separate questions.

[Sunny](../README.md) · [Authoring](authoring.md) · [Operation](operations.md) · [Development](development.md) · [Completion](completion.md)

## Source ownership

Follow a musical request through these boundaries:

| Concern | Owning code |
|---|---|
| Theory, Score/Timbre/Mix/Corpus types, rules and mutations | `include/sunny/core/`, `src/core/` |
| Exact musical events to scheduled/render controls | `include/sunny/render/`, `src/render/` |
| Public MCP schemas, shared authoring sessions and project transactions | `src/infrastructure/mcp/` |
| MIDI, notation and target compilation | `src/infrastructure/formats/` |
| Native planning, durable dispatch, receipts and reconciliation | `src/infrastructure/ableton/` |
| Live's native objects and main-thread execution | `remote_script/Sunny/` |
| Locked delivery and Windows operation | `tools/release.py`, `Dockerfile`, `tools/windows/` |

The core owns musical meaning. MCP owns the public request boundary and session;
it delegates domain edits to the core. Native realization keeps operational
history outside authored undo and resolves current objects through the bridge.
The configure-time layer check prevents dependencies from pointing back up this
chain. Formal specifications describe contracts; Git and issues retain history.

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

For a build outside the checkout, use ordinary CMake instead of changing the
presets or moving an existing CMake cache:

```bash
cmake -S . -B /tmp/sunny-build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/sunny-build --parallel 2
ctest --test-dir /tmp/sunny-build --output-on-failure
```

The preset directories are generated output. Some existing trees also retain
qualification artifacts; check their provenance and consumers before removing
them. Keep authored workspaces, native histories and release archives outside
the checkout. Git preserves committed source history, not ignored evidence.

## Which checks to run

Use the affected existing domain tests for a focused change and the integrated
workflow when a change crosses ownership or persistence boundaries. Before a
release, complete the required integrated and platform gates. Retain valid
results for unchanged artifacts with their source identities.

CodeQL, mutation testing and coverage are optional tools for specific questions.
They do not replace a working public musical workflow or native qualification.
Do not run every analyser as a prerequisite to an unrelated small edit.

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
represented by one offline model of its Python API (`tests/python/live_model.py`), built from
Ableton's own Remote Script sources and the Live Object Model documentation. End-to-end tests run
the real `sunny-mcp` over MCP, through TCP and the real Remote Script, into that model, and check
the resulting Set: notes in beats, tracks, devices, mixer values, routing and the change journal,
for both Live 11 and Live 12.

Expected values in tests come from the relevant standard or a hand derivation, never from the
code under test. CI defines Linux/macOS engine and end-to-end checks, a locked release producer →
archive verifier → same-image full-release ZIP gate, named-volume crash/restore checks, actual
Windows installer/transfer/doctor fixtures, and Max builds on macOS/Windows. These workflow
definitions are not a claim that the current changes have run successfully in CI. Local source/model
and Windows fixture results remain distinct from final released-image and native-host evidence.

## Repository layout

| Path | Contents |
|---|---|
| `include/sunny/`, `src/` | C++ engine: `core` (theory and documents), `render`, `max`, `infrastructure` (formats, MCP, Ableton bridge) |
| `apps/` | The `sunny-mcp` entry point |
| `remote_script/Sunny/` | The native Live Remote Script, configuration consumer and shared bridge contract |
| `release/`, `tools/release.py`, `Dockerfile` | Locked paired image/native/operator release producer and verifier |
| `tools/windows/` | Managed Windows installation, transfer, stdio launcher, doctor and scoped recovery |
| `tools/live_qualification/` | Developer POSIX final-host kit and all 23 original obligations |
| `python/sunny/` | Python package |
| `max-package/` | Max externals, help patchers and reference pages |
| `tests/` | C++ and Python tests, mirroring the source layout |
| `docs/formal/` | Specifications of the music model, the four documents, projects and rendering |
| `cmake/`, `.analysis/` | Build policy, layering checks and static-analysis queries |

The layering rule is enforced at configure time: `core` depends on nothing else in Sunny,
`render` only on `core`, and `infrastructure` on both.

