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

The `sunny-mcp` program exposes theory and document workflows as MCP tools and speaks JSON-RPC on
standard input and output. `tools/list` returns the current inventory. Some structural editing
operations remain available only in C++. The engine is also available from Python.

```
AI client ──MCP (stdio)──▶ sunny-mcp ──TCP 9001──▶ Sunny Remote Script ──▶ Ableton Live
                             │
                             └── theory engine and documents (no Live needed)
```

Documents reach Live only as a project: a Score, one Timbre profile for each of its parts, and a
Mix graph. `project_plan_to_ableton` records a snapshot of the Live Set and the exact list of
changes without touching Live. `project_apply_ableton_plan` applies that plan once, refuses if
the observed Set properties have changed in the meantime, and returns a journal of every change it attempted,
including after a partial failure. `project_compile_to_ableton` does both in one call. Values
Live cannot represent, such as a fader above +6 dB, are refused before anything is sent.

A few quick tools (`create_progression_clip`, `apply_euclidean_rhythm`, `apply_arpeggio`) write a
single clip into an empty clip slot without a project. They record only changes Live
acknowledged, report a lost response as indeterminate rather than as a failure, and can be undone
with `undo_ableton_operation`.

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

Docker is the normal delivery path. It needs Docker installed on the client machine; a native
Sunny build is optional. MCP still travels over standard input and output, so the client starts
the container itself:

```bash
docker build -t sunny-mcp .
```

```json
{
  "mcpServers": {
    "sunny": {
      "command": "docker",
      "args": ["run", "-i", "--rm", "-e", "SUNNY_ABLETON_HOST=192.168.1.20", "sunny-mcp"]
    }
  }
}
```

Replace the address with the machine running Live. The container's only network use is the
outbound connection to the Remote Script.

## Connecting Ableton Live

Copy the Remote Script into the `Remote Scripts` folder of your Live User Library and select it
as a control surface (Preferences, Link/Tempo/MIDI, Control Surface: Sunny):

```bash
cp -r remote_script/Sunny "${HOME}/Music/Ableton/User Library/Remote Scripts/Sunny"
```

The script listens on TCP port 9001, bound to `127.0.0.1` unless `SUNNY_BIND_HOST` is set in the
environment of the Live process. If you bind to another interface, restrict access with a
firewall: the port accepts commands that change your Live Set.

The bridge and server must use the same bridge contract. The Remote Script runs inside Live's
own Python and retains Python 3.7 syntax compatibility for Live 11.

Sunny's operation choices follow the version floors in the official
[Live Object Model reference](https://docs.cycling74.com/apiref/lom/), whose current reference
describes Live 12.4.5:

| Live version | Note insertion and readback | Native device insertion |
|---|---|---|
| Before 11.0 | Note intent retained; insertion unavailable | Unavailable |
| 11.0.x | Insert notes; read all pitches with note starts in `[0, generated clip end)` | Unavailable |
| 11.1–11.x | Insert notes; read the complete Clip note population | Unavailable |
| 12.0–12.2 | Same complete-population readback | Unavailable |
| 12.3+ | Same complete-population readback | Version-admitted native Live devices |

The [Clip reference](https://docs.cycling74.com/apiref/lom/clip/) dates ranged note access to
11.0 and complete-population access to 11.1. Live 11.0 evidence includes
`observed_time_span` and sets `entire_clip_population_observed` to false: notes outside the query
remain unobserved. Snapshots omit notes on every version, so snapshot equality cannot establish
note-content stability. Live 11 already has [take lanes](https://www.ableton.com/en/live-manual/11/comping/);
this adapter's unavailable Live-11 take-lane observation cannot establish their absence.

[Track.insert_device](https://docs.cycling74.com/apiref/lom/track/) has a Live 12.3 floor and
native-device placement restrictions. Version eligibility does not establish edition, licensing,
installed devices, or exact-host acceptance. Max for Live availability remains unknown until
independent evidence is supplied. No exact version/edition/OS combination has yet passed Sunny's
real-host qualification.

Each deployment result reports capability and mapping gaps, including unsupported source
configurations, third-party plug-ins, Group creation, and automation-envelope authoring. Static
native parameter mapping currently requires explicit bindings. Temporary parameter control with
[live.remote~](https://docs.cycling74.com/reference/live.remote~/) disables automation and does
not author saved envelopes.

## Live testing

Development and CI need no Ableton. A real Live Set is checked last, from any machine that can
reach the one running Live:

1. On the Live machine, install the Remote Script as above and let it listen beyond loopback by
   setting `SUNNY_BIND_HOST=0.0.0.0` in the environment Live starts with (a user environment
   variable on Windows, `launchctl setenv` on macOS), then restart Live. Allow TCP 9001 through
   the firewall from the testing machine only.
2. Open an empty or scratch Set: the check adds two tracks.
3. On the testing machine, run the check through the local build or the Docker image:

   ```bash
   export SUNNY_LIVE_HOST=192.168.1.20
   export SUNNY_MCP_COMMAND="docker run -i --rm -e SUNNY_ABLETON_HOST -e SUNNY_TCP_PORT sunny-mcp"
   pytest tests/python/test_live_host.py -s
   ```

The check deploys a small two-part project, reads selected properties back, and prints what it
observed. The same scenario runs against the offline Live model in every CI build. This smoke
check covers one workflow; the independent device, Python runtime-type, routing, transport,
reconnect, persistence, and large-Set probes in
[issue #22](https://github.com/sunyaisblank/Sunny/issues/22) require additional host checks.

When something goes wrong inside Live, `get_ableton_remote_log` returns the Remote Script's own
records: every request with its outcome, every refusal and every error, numbered so a client can
poll for newer records with `after_sequence`. The log is held in memory inside Live (the last
1,000 records) and needs no access to Live's own log file. `sunny-mcp` writes its own diagnostics
to standard error, which `docker logs` or the MCP client's log shows.

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
code under test. CI builds and tests on Linux and macOS, runs the end-to-end tests, and builds the
Max externals on macOS and Windows.

## Limitations

- Sunny has not yet been run against a real Live Set end to end. Behaviour that Ableton's
  documentation does not settle is listed in GitHub issue #22.
- Known defects and their status are tracked as GitHub issues labelled `remediation`.
- Documents live in the server's memory for the life of the process. Score, Timbre, Mix and Corpus
  documents can be exported with `score_get_json`, `get_timbre_json`, `get_mix_json` and
  `get_corpus_json`. Whole-project save/open and automatic persistence remain unavailable.
- A fresh project deployment creates tracks and clips again. Reapplying edited documents requires
  an ownership/reconciliation workflow that is not yet implemented.
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
