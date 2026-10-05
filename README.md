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
standard input and output. `tools/list` returns the current inventory. Agents can edit measures,
parts, voices, notes, expression, Timbre profiles and Mix effects, and undo coherent project edits.
The engine is also available from Python.

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

After `workspace_save`, `project_realization_create` creates one selected Part's managed MIDI
clip and notes with a durable dispatch fence. `project_realization_inspect` reads its actual native
state; `project_realization_update` revises existing attacks and adds or deletes whole Events while
preserving retained native note IDs. These tools require the owning project's current revision. They retain receipts across server
restart and authored undo, detect user drift, and refuse duplicate creation. A lost reply requires
`project_realization_reconcile`, which queries the original token without replaying the mutation.
For several Parts, `project_realization_plan` preflights the selected owning Score, source/effect
controls, static Mixer, Step pan lanes, routing, removed-Part retirement and optional Song settings.
Pass its exact returned plan to `project_realization_apply` with explicit plan and selected-domain
approvals. All phases use the same owning revision and existing per-operation fences. Known
unsupported selections decline before the first fence; a newly inserted device still needs actual
native descriptor admission. A failure stops later phases and returns original attempt identifiers
for reconciliation. Plans also reserve the complete MCP reply within the 16 MiB coordinator
limit; select a smaller aggregate when that reservation or the 4 MiB apply-input limit is exceeded.
Final readback checks the selected physical controls and current cohorts;
unselected domains and audible/DSP equivalence remain explicit limits.
`project_realization_author_mix_lane` also authors a selected Part's Step panning lane when
the native envelope is absent, then checks sampled values. Existing envelopes are preserved.
To revise the whole selected envelope, use `project_realization_preview_mix_lane_replacement`
and `project_realization_replace_mix_lane`. Approval explicitly includes overwriting unsampled
state on that parameter; other parameter envelopes are preserved. Initial authoring and
replacement support up to 64 Step points and report actual samples, without claiming complete
breakpoint capture.
`project_realization_update_geometry` changes the Clip extent and flat meter while retaining
unchanged attacks. `project_realization_update` combines note, length and meter changes under
the final owning revision: it extends before adding later notes and removes tails before
shrinking. Each intermediate projection is fenced and read back separately. Reconcile a lost
reply using its original attempt identifier; the next update resumes from the verified stage.
`project_realization_preview_song_settings` and `project_realization_apply_song_settings`
apply explicitly approved constant tempo and initial meter to the whole Set. Uncertain Set-setting
replies block subsequent managed setters in the active history namespace until reconciliation or
fresh current Set approval. Opening another workspace or applying its backup is refused while
that uncertainty remains; original attempt history remains queryable.
`project_realization_plan_timbre` reads explicitly selected Drift cutoff and ADSR durations in Hz
and milliseconds. `project_realization_author_timbre` inserts or revises that source using actual
native formatter and parameter readback. Fresh readback of unchanged selected source and active
effect intent returns without repeating device setters. Selected bypassed effects are enabled,
configured and verified, then disabled; their saved configuration is distinguished from current
readable values while bypassed. Unselected sound fields remain reported as unapplied.
`project_realization_plan_effects` and `project_realization_author_effects` cover explicitly
selected native EQ Eight bands and Utility gain/width stages after Drift. Mix input trim uses
a separate Utility, and `set_channel_input_trim` edits its authored dB value. Each native mode
and physical phase is separately fenced and read back; unsupported effects remain unapplied.
`set_channel_flags` edits authored mute/solo values. The static Mixer plan, preview and apply
tools read selected fader, Stereo pan, mute and solo intent from the owning Channel. Fader dB
uses the actual native formatter; programme loudness needs supplied measurement evidence.
Solo requires explicit approval of its Set-wide audible effect. Existing selected automation
is preserved, so apply static pan before authoring its lane. A reopened Mixer needs separate
current-object adoption. `project_realization_retire_part` explicitly mutes a removed Part's
retained Track while preserving its Clip, notes, devices and historical receipts. Mute-only
retirement also retains Solo; the returned Solo state explains when other Tracks remain muted
via solo.
The routing plan, preview and apply tools join owning Aux sends to actual Returns and select
currently advertised output targets. `project_realization_inspect_routing` reads their current
identifiers and attached destinations; inspect again after changing the output type to select
a channel from the newly advertised list. Return Pre/Post policy remains unavailable; conflicting
enabled per-Aux requests decline before any write. A new Return updates all affected Part guards
and requires fresh Mixer approval. Existing flat native Groups can be explicitly adopted against
their complete owning membership; grouped Parts then need their own current Clip approval.
After a Live or bridge restart, `project_realization_preview_adoption` and
`project_realization_adopt` establish explicitly approved current Clip/note authority.
Device authority requires its separate preview/adoption pair. Saved receipts identify history;
fresh native observations and approval establish the current objects. Each result identifies
project domains that have not been applied.
If the Score changed offline before reopening Live, select
`projection_source: "retained_verified_realization"` for the Clip preview and adoption, and
repeat the returned `historical_projection_attempt` when approving adoption. This establishes
the current objects against their last verified musical baseline; the next update applies the
current Score. If physical intent also changed offline, use
`device_projection_source: "retained_verified_realization"` for the separate device preview/adoption
and repeat its returned `device_history_attempt`. This reads the saved finite physical targets and
original tolerances from verified history, then checks them against current native objects without
setters. The current owning source/effect writers apply the newly authored values after adoption.
The current logical device keys, classes and order must still agree; historical receipts never
restore old native identities.
For removed Parts, Clip recovery uses the last verified historical note/geometry projection;
separate mute-only Mixer adoption permits retirement without recreating the Score Part.

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
docker build -t sunny-mcp:0.4.0 .
docker image inspect sunny-mcp:0.4.0 --format '{{.Id}}'
```

Keep the returned `sha256:...` image ID. Use that same ID when exporting the bridge and in
the MCP configuration below, so rebuilding a tag cannot silently select different sources.
The image includes its exact installable Remote Script:

```bash
docker create --name sunny-bridge-export sunny-mcp:0.4.0
docker cp sunny-bridge-export:/opt/sunny/remote-script/Sunny ./Sunny
docker rm sunny-bridge-export
```

These commands create a stopped container and export files; they do not start Sunny or Live.
Replace the image tag in `docker create` with the retained image ID if the tag has changed.
The exported `Sunny/source.sha256` identifies the bundled Python sources.

```json
{
  "mcpServers": {
    "sunny": {
      "command": "docker",
      "args": ["run", "-i", "--rm", "--mount", "type=volume,source=sunny-data,target=/data", "-e", "SUNNY_ABLETON_HOST=host.docker.internal", "-e", "SUNNY_TCP_PORT=9001", "sha256:REPLACE_WITH_IMAGE_ID"]
    }
  }
}
```

This example uses Docker Desktop with Live on the same computer. Configure the bridge listener
as described below. For Live on another LAN computer, replace `host.docker.internal` with that
computer's address, such as `192.168.1.20`. On Linux Docker Engine, add
`"--add-host", "host.docker.internal:host-gateway"` to use the host machine. No `-p` or inbound
container port is needed: MCP uses stdio and the container connects outbound to TCP 9001.

For a native `sunny-mcp` on the same computer as Live, `SUNNY_ABLETON_HOST=127.0.0.1` connects to
the default loopback listener. Inside an ordinary Docker container, `127.0.0.1` refers to that
container and does not address Live on the host. Running without `SUNNY_ABLETON_HOST` starts an
offline authoring session with the same durable volume.

The volume retains saved work when a container is replaced. After authoring, call
`workspace_save` with `path: "/data/workspace.sunny.json"`. The next container restores that
file before accepting requests. Saving includes every Score, Timbre profile, Mix graph, shared
preset, corpus record, owning project relationship, rendering configuration, and identity
reservation. Native realization history is saved in a separate namespace directory in the same
volume and survives authored undo or backup recovery. Keep that directory with the workspace when
moving saved work; a missing history directory blocks native writes. Undo history and temporary
deployment plans end with the process. Changes require an
explicit save; closing the client does not save them automatically.

Each process reserves its configured workspace and backup before restoring or accepting requests.
A second process using those files exits with a workspace writer admission error. Save, open and
applied backup recovery also reserve their destinations; independent workspace files can run
separately. Stop the owning client before replacing its container or restoring its data. OS locks
release when the process exits, including a crash. Keep the stable hidden `.sunny-writer.lock`
sidecars: deleting or replacing them revokes publication and requires a restart. Parent-directory
aliases resolve to the same ownership. Final workspace symlinks and hardlinks are refused because
atomic replacement would split their identities. The Linux process behavior is tested; native
Windows locking and the final released Docker volume profile still require qualification.

For a native server, set `SUNNY_WORKSPACE_PATH` to your saved workspace path to enable the same
startup restore. `workspace_open` replaces authored state only after complete validation;
`workspace_import` refuses identity collisions. If the main file is corrupt, startup reports the
error and exits. Explicitly set `SUNNY_WORKSPACE_RECOVERY=backup` to restore the supported `.bak`,
then call `workspace_save` to repair the main file and remove the recovery setting. Recovery never
silently replaces the main file. Sample and external preset paths are retained; their files must
also be available to a later container.

`score_export_midi` returns an actual type-0 Standard MIDI File as `midi_base64`, along with its
compilation loss report. `score_compile_to_musicxml` returns notation XML. Both use the same
authored Score, with concert pitch in MIDI and instrument-transposed written pitch in MusicXML.

## Connecting Ableton Live

Live 12.4 is the primary target, with Live 12.3 compatibility retained. The finite operation
version floors below also describe limited legacy use; they do not qualify an untested future release.
Export the `Sunny` folder from the same image ID used by your MCP client, then copy that folder
into `Remote Scripts` under your configured Live User Library. Select Sunny as a control surface
in Live's Preferences, Link/Tempo/MIDI. For a native build, use the generated
`.bin/remote_script/Sunny` folder from that build. Restart or reload the control surface after
replacing the folder.

The script listens on TCP port 9001, bound to `127.0.0.1` unless `SUNNY_BIND_HOST` is set in the
environment of the Live process. If you bind to another interface, restrict access with a
firewall: the port accepts commands that change your Live Set.

For Docker Desktop on the Live computer, or for a client on another LAN computer, set
`SUNNY_BIND_HOST=0.0.0.0` in the environment that launches Live and permit TCP 9001 only from
the intended client or local Docker network. Keep the default `127.0.0.1` binding for a native
client on the same computer. One bridge client is served at a time; close another connected
Sunny client before starting a replacement. Do not expose the listener to the public internet.

Protocol version 46 and a SHA256 of all bundled Python sources must match the server. The first
ordinary request on each connection checks the bridge's source identity; reconnects check again.
A mismatch refuses that request before sending it and names the expected/observed identity.
Install the bridge from the correct image and reload it. Profile and Remote Script log queries
remain available to diagnose the mismatch. Source identity proves matching Sunny code; it does
not establish exact-host native API or playback qualification. The script runs inside Live's
own Python and retains the finite legacy Live 11 behaviour and Python 3.7 syntax compatibility.

Sunny's operation choices follow the version floors in the official
[Live Object Model reference](https://docs.cycling74.com/apiref/lom/), whose current reference
describes Live 12.4.5:

| Live version | Note insertion and readback | Native device insertion |
|---|---|---|
| Before 11.0 | Note intent retained; insertion unavailable | Unavailable |
| 11.0.x | Insert notes; read all pitches with note starts in `[0, generated clip end)` | Unavailable |
| 11.1–11.x | Insert notes; read the complete Clip note population | Unavailable |
| 12.0–12.2 | Same complete-population readback | Unavailable |
| 12.3.x and 12.4.x | Same complete-population readback | Reviewed native-device candidates |

Managed project authoring admits these two Live 12 minor versions. Later versions require
capability review before native writes.

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
configurations, third-party plug-ins, Group creation, and general automation-envelope authoring.
The managed Step panning lane has a separate guarded workflow and sampled readback. Static
legacy native parameter mapping uses explicit bindings; the finite managed source/effect tools
resolve physical values from actual native formatter observations. Temporary parameter control with
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
poll for newer records with `after_sequence`. Pass the returned `stream_id` on later calls as well
as `next_sequence`. A bridge restart changes the stream identity and reports `reset`; lost retained
records report `truncated`. `observed_at` timestamps each read. `has_more` indicates another page,
and `next_sequence` advances only through delivered records. Pages fit the native 16 MiB wire
limit, including Unicode escaping. The last 1,000 records remain in memory inside Live.
Sequence-only reads from an older bridge remain available for mismatch diagnosis; their missing
epoch and freshness metadata is explicitly unavailable.

This log route needs a working bridge and shares its command channel. It cannot diagnose a script
that never loaded or service a second request while the first remains in progress. `sunny-mcp`
writes its own diagnostics to standard error. Docker logs cover container output; native Live
`Log.txt` requires its separately authorized host route. General doctor and host-log support are
being completed under the [operating lifecycle contract](https://github.com/sunyaisblank/Sunny/issues/36).

The maintained final-host tooling is in `tools/live_qualification/`. Its `obligations.json` retains
all 23 original validation groups, their required evidence and applicability. Local toolkit tests
do not mark those groups passed. Export the tooling with the exact bridge from a retained image:

```bash
python3 tools/export_live_qualification.py \
  --image-id sha256:REPLACE_WITH_COMPLETE_IMAGE_ID \
  --output /new/path/Sunny-qualification.zip
```

The export uses an unstarted container, records image architecture and available revision metadata,
checks the exported source identity, and includes file checksums. It neither starts Live nor installs
anything. This qualification package is a transport artifact; the coherent production release and
its registry digest remain separate acceptance requirements.

Configure the exported `qualification/configuration.json` with the exact image, current tool count,
machine endpoints, Live version/edition and new evidence/workspace locations. Start with
`python host_runner.py inventory` from that directory. Mutations require explicit scratch approval,
the exact configured Set name, the observed native document token, stopped transport/recording, and
input monitoring Off on every Track. A different Set, including one with the same name, cannot inherit
that probe approval. The optional `SunnyHostProbe` surface on port 9002 creates notes and performs
native setters for selected tests; it is a mutation instrument, not a general health check.

Real execution comes after the integrated release's offline gates and approved machine setup.
Generic raw bridge mutations currently decline because they lack native document authority;
ordinary command guards and the running-cue check remain open in
[issue #41](https://github.com/sunyaisblank/Sunny/issues/41). Complete native authoring, saved-envelope
reopen/recovery, visual and audible observations, and all original groups remain required in
[issue #22](https://github.com/sunyaisblank/Sunny/issues/22). RDP installation or a connected TCP socket
does not establish audio readiness. The full lifecycle requirements, grouped findings and evidence
index are in [issue #36](https://github.com/sunyaisblank/Sunny/issues/36).

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
  documents can be exported separately. `workspace_save` persists their complete owning workspace;
  startup restore and `workspace_open` reopen it. Unsaved changes are lost when the process ends.
- The one-shot project deployment tools create a fresh realization. Managed Part tools support
  note revision, whole Event addition/deletion, separate Clip extent/meter changes and explicit
  selected pan-envelope replacement. Reopened objects require fresh explicit adoption.
  Managed device authoring covers selected Drift controls and finite EQ Eight/Utility stages;
  unsupported device intent, unselected Mixer controls, Return processing and Pre/Post policy
  remain reported as unapplied. Native Group creation and regrouping are unavailable.
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
