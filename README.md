# Sunny

Sunny lets an AI agent compose and revise music in Ableton Live through the
Model Context Protocol (MCP). Its C++23 engine represents musical intent as
checked documents, so an agent can work with parts, notes, harmony, sounds and
mixing rather than a sequence of raw MIDI commands.

Most of Sunny works without Ableton. The engine can author, analyse, save and
export a project offline. Applying that project to a Live Set is a separate step.

Native Ableton qualification is still open. [Build and test results](https://github.com/sunyaisblank/Sunny/actions/workflows/ci.yml)
record verification for each source revision.

## Start here

- [Author music](docs/authoring.md): the project model, a first phrase, editing and native realization.
- [Install and operate](docs/operations.md): the paired release, Windows/Docker setup, configuration, backup and recovery.
- [Develop Sunny](docs/development.md): build, tests, source ownership and optional analysis tools.
- [Final qualification](docs/qualification.md): the remaining exact-host, save/reopen, visual and audio checks.

The [project goal](https://github.com/sunyaisblank/Sunny/issues/1#issuecomment-6301306386)
and [operating lifecycle](https://github.com/sunyaisblank/Sunny/issues/36) track
the accepted scope, remaining work and completion evidence.

These guides ship together in the offline release's `operator/` directory.
The detailed [formal specifications](https://github.com/sunyaisblank/Sunny/tree/lifecycle-distribution/docs/formal)
are references for implementers.

## The model

| Document | Owns |
|---|---|
| Score | Parts, notes, rhythm, harmony, expression and form |
| Timbre | Each Part's sound source, effects, modulation and presets |
| Mix | Channels, buses, sends, levels, panning and routing intent |
| Corpus | Ingested works and traceable style-analysis evidence |

An owning project joins a Score, its Timbre profiles and its Mix through stable
Part identities. Project edits and undo preserve those relationships.
`workspace_save` persists the authored workspace; unsaved changes remain in memory.
Native operation history is separate from musical undo.

```text
AI client → MCP over stdio → sunny-mcp → Sunny Remote Script → Ableton Live
                                │
                                └→ theory, documents, analysis and file exports
```

The theory library covers pitch and spelling, scales, chords, voice leading,
counterpoint, rhythm, tuning, post-tonal sets and acoustics. Musical time uses
checked rational arithmetic. Scores export MIDI, MusicXML and LilyPond.

## Build the engine

Use CMake 3.28+, a C++23 compiler (GCC 13+ or Clang 16+), and Python development
headers for the optional bindings:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
```

The executable is `.bin/sunny-mcp`. `tools/list` is its authoritative MCP tool
inventory. [Development](docs/development.md) explains the other build profiles
and checks; normal development needs no Live installation.

## Use it with Live

Docker is the normal delivery path. The production profile pairs a Windows 11
client running a Linux/amd64 Docker Desktop engine with native Windows Live
12.4.5 Suite. The Windows operators need no installed Python interpreter.

For ongoing work, save the workspace, establish fresh authority with
`doctor_ableton`, then use the managed `project_realization_*` plan, apply,
inspect and update tools. They retain original operation receipts and reconcile
lost replies without replaying mutations. The separate
`project_*_to_ableton` deployment tools create a fresh realization; they are not
the managed revision workflow.

No exact host/version combination has passed all final gates. Live 12.3 and
limited Live 11 compatibility remain separately unqualified. The engine does
not render or measure audio; native parameter readback cannot establish sound
or saved `.als` behavior. See the [supported native intent](docs/authoring.md#supported-native-intent).

## Licence

MIT
