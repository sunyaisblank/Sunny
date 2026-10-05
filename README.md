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
single clip into an empty clip slot without a project. Save the workspace durably before using
them. Every create, undo and redo retains the original native object authority and writes a
durable attempt before sending it. A lost response remains indeterminate; inspect
`ordinary_clip_history` and use `ordinary_clip_reconcile` to query the original token. Neither
reconnect nor reconciliation repeats a mutation. Acknowledged creations can be undone with
`undo_ableton_operation` while their original native objects and unchanged content remain available.
Restart preserves query history; it does not restore the in-memory undo stack.

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
An explicitly configured port must be ASCII decimal 1–65535 without signs, whitespace or leading
zeros. Invalid ports and empty host addresses refuse startup instead of selecting another endpoint.
Under WSL2 with Live on the Windows host, use the Windows host's IP address.

Docker is the normal delivery path. It needs Docker installed on the client machine; a native
Sunny build is optional. MCP still travels over standard input and output, so the client starts
the container itself:

Build a paired candidate from a clean committed checkout with Python 3.10+ and Docker Buildx:

```bash
python3 tools/release.py build --output /new/path/sunny-release --tag sunny-mcp:local-release
python3 tools/release.py verify --release /new/path/sunny-release \
  --expected-manifest-sha256 REPLACE_WITH_RECORDED_MANIFEST_SHA256
```

Keep the printed manifest checksum separately. The directory contains `image.tar`, the exact
`native/Sunny` bridge, a Windows installer, the doctor CLI, dependency inventories and `release.json`.
The build freezes its committed source and pins the Ubuntu platform image, dated apt snapshot and
dependency commits in `release/build-inputs.json`. It records actual compiler/package inputs;
this does not promise byte-identical output from independent compilers. Ordinary `docker build`
remains a development route and does not produce the verified release directory.

For offline transfer, copy that whole directory and verify it against the retained manifest hash.
On the destination client, load it from a matching checkout:

```bash
python3 tools/release.py load --release /path/to/sunny-release \
  --expected-manifest-sha256 REPLACE_WITH_RECORDED_MANIFEST_SHA256
```

Use its printed
`loaded_image_local_immutable_id` when launching MCP. The manifest distinguishes the producing
store's local ID, archived OCI index/manifest/config digests, and any registry digest. A tag or a
local Docker ID is insufficient evidence of registry publication. Publication is pending an approved
destination; current manifests explicitly retain production and native qualification as pending.

Production configuration is external UTF-8 JSON with `configuration_schema_version: 1`.
Both consumers reject unknown fields, duplicate keys, future versions, malformed endpoints and
ambiguous mixing with legacy environment variables before opening a workspace or socket.
For an offline client, save this as an external file such as `C:\Sunny Configuration\offline.json`:

```json
{
  "configuration_schema_version": 1,
  "client": {
    "transport": {"mode": "offline"},
    "workspace": {"path": "/data/workspace.sunny.json", "recovery": "none"}
  }
}
```

```json
{
  "mcpServers": {
    "sunny": {
      "command": "docker",
      "args": ["run", "-i", "--rm", "--pull=never", "--mount", "type=volume,source=sunny-data,target=/data", "--mount", "type=bind,source=C:\\Sunny Configuration\\offline.json,target=/run/sunny/configuration.json,readonly", "--env", "SUNNY_CONFIG_PATH=/run/sunny/configuration.json", "sha256:REPLACE_WITH_LOADED_IMAGE_ID"]
    }
  }
}
```

This launches an offline authoring session. Each MCP client owns one container's stdio; do not
share one unattended stdin among clients or add a TTY. Live remains native. The remote production
profile uses the Windows `installer/windows/SunnyClient.ps1` launcher and an approved SSH route
to the loopback bridge. Its actual machine qualification remains under
[issue #38](https://github.com/sunyaisblank/Sunny/issues/38).
Raw TCP across a LAN is unauthenticated and is outside the supported production profile.

For a native `sunny-mcp` on the same computer as Live, `SUNNY_ABLETON_HOST=127.0.0.1` connects to
the default loopback listener. Inside an ordinary Docker container, `127.0.0.1` refers to that
container and does not address Live on the host. Running without `SUNNY_ABLETON_HOST` starts an
offline authoring session. Selecting a durable workspace requires explicit configuration.

The Windows launcher requires 64-bit PowerShell 5.1+, Windows OpenSSH, a running local
Linux/amd64 Docker Desktop engine, the verified loaded image and one named workspace volume.
Keep SSH credentials and verified known-host entries outside the release. A separate connection
file has exactly these fields:

```json
{
  "connection_schema_version": 1,
  "host": "REPLACE_WITH_APPROVED_LIVE_HOST",
  "port": 22,
  "user": "REPLACE_WITH_APPROVED_LOCAL_ACCOUNT",
  "identity_file": "C:\\Sunny Configuration\\approved_identity",
  "known_hosts_file": "C:\\Sunny Configuration\\approved_known_hosts"
}
```

The launcher never registers access or accepts a changed host key. Directional approval from
the Live PC to the client does not establish client-to-Live access. Set up the required direction
manually after independent preparation and checks.

For the remote runtime configuration, use both roles. Set `client.transport` to
`{"mode":"tcp","host":"host.docker.internal","port":49001}` and keep its workspace under
`/data`. Set `native` to `{"bridge":{"bind_host":"127.0.0.1","port":9001}}`.
The local forward port is configurable; choose a free port. Supply the native configuration
to Live through its process environment's `SUNNY_CONFIG_PATH`, then restart Live deliberately.
No host address or library location is compiled into either component.

```powershell
$client = 'D:\Sunny Release\installer\windows\SunnyClient.ps1'
& $client -Action Plan -ConnectionFile 'C:\Sunny Configuration\connection.json' `
  -ConfigurationFile 'C:\Sunny Configuration\runtime.json' -WorkspaceVolume sunny-data `
  -ReleaseDirectory 'D:\Sunny Release' -ExpectedManifestSHA256 $hash -ImageId $loadedImageId
# Use the same arguments with -Action Run as the MCP client command.
```

`Plan` verifies the local release, image and compiled configuration using an isolated container;
it does not start SSH or mount project data. `Run` starts an owned Windows loopback-only SSH
forward, verifies its listening process, then starts one attached Sunny container with inherited
MCP handles. Its normal stdout contains protocol messages only. Tunnel loss closes the session.
Cleanup removes only containers with the exact per-session owner, name and original daemon;
it preserves the project volume. A private Windows job closes owned SSH and Docker client
processes when the launcher ends. Abrupt interruption can leave a daemon container, especially
during preflight. The launcher prints a recovery directory to stderr before validation and
preserves it if cleanup cannot be confirmed. After the original client has ended, run
`SunnyClient.ps1 -Action Recover -SessionDirectory 'REPLACE_WITH_REPORTED_DIRECTORY'`.
Recovery refuses foreign or changed scope and confirms container absence before removing its
own temporary files. An owned listening forward alone does not prove native Live readiness.

The volume retains saved work when a container is replaced. After authoring, call
`workspace_save` with `path: "/data/workspace.sunny.json"`. The next container restores that
file before accepting requests. Saving includes every Score, Timbre profile, Mix graph, shared
preset, corpus record, owning project relationship, rendering configuration, and identity
reservation. Native realization history is saved in a separate namespace directory in the same
volume and survives authored undo or backup recovery. Keep that directory with the workspace when
moving saved work; a missing history directory blocks native writes. Undo history and temporary
deployment plans end with the process. Changes require an
explicit save; closing the client does not save them automatically.

The realization ledger reads schema 1 and writes schema 2 when publishing a new ordinary attempt.
Migration preserves existing managed records and keeps ordinary attempts separately. Older
binaries cannot read schema 2. Before a version rollback, stop the writer and restore a retained
compatible workspace together with its complete realization namespace; copying only an authored
`.bak` does not roll operational history back. Backup/recovery never grants replay authority.

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
Install the `native/Sunny` folder from the same verified release used by your MCP client into
`Remote Scripts` under your configured Live User Library. Select Sunny as a control surface
in Live's Preferences, Link/Tempo/MIDI. For a native build, use the generated
`.bin/remote_script/Sunny` folder from that build. Restart or reload the control surface after
replacing the folder.

The default legacy environment profile listens on TCP 9001 at `127.0.0.1`. Versioned native
configuration requires an explicit numeric IPv4 loopback address and port; IPv6 native binding is
unsupported by its current AF_INET server. Keep the plain native command port off the LAN.
The legacy variables remain available for existing local development, but a non-loopback bind
is outside the selected production profile.

Keep the default `127.0.0.1` binding for local operation and the selected SSH route. One bridge
client is served at a time; close another connected Sunny client before starting a replacement.
Remote access approval and effective forwarding/firewall scope are final machine setup steps.

The Windows release installer supports PowerShell 5.1+, the current user and a physical local NTFS
User Library. Supply the existing path shown in Live Settings > Library; it does not guess a default
folder, change Control Surface settings, stop Live, create SSH trust, or alter firewall settings.
Preview before installation:

```powershell
$installer = 'D:\Sunny Release\installer\windows\Sunny.ps1'
$library = 'D:\Actual User Library'
$release = 'D:\Sunny Release'
$hash = 'REPLACE_WITH_VERIFIED_MANIFEST_SHA256'
& $installer -Action Plan -UserLibraryPath $library -ReleaseDirectory $release -ExpectedManifestSHA256 $hash
# Save Sets and quit Live before Install, Rollback, Recover or Uninstall.
& $installer -Action Install -UserLibraryPath $library -ReleaseDirectory $release -ExpectedManifestSHA256 $hash
```

Install verifies the whole release, stages only its bridge, and records ownership and an atomic state
commit under `Remote Scripts/.sunny-managed`. An identical rerun makes no backup. Unknown or edited
Sunny content is preserved and reported. `Status` reads the recorded state. `Recover` restores the
old release before commit or finishes the new release after commit, including partial transfer and
cleanup failures. Updates use `Install` with the new verified directory. `Rollback` exchanges the
recorded active and previous releases. `Uninstall` removes the managed active script, reports the
retained backup, and leaves user Sets, settings, unrelated scripts and management state intact.
Remove Sunny from its Control Surface slot manually after uninstall.

After starting Live and selecting Sunny with Input/Output None, export a doctor result from the
configured route. `Confirm -DoctorReport PATH` accepts only fresh correlated read-only readiness
for the active source/protocol with successful launcher cleanup. Verify that route refers to this host and library before confirming;
the report cannot establish that association itself. A further forward update cannot retire the
older backup until confirmation. Rollback and uninstall stay available. Confirmation does not
qualify sound, licences or musical mutations. These transitions have isolated Windows filesystem
tests; real-host deployment, cross-user profiles and sudden power loss remain qualification gates.

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

1. Install the matching bridge as above and establish the approved route to its loopback listener.
   Keep transport/access setup separate from proof that the loaded script responds.
2. Open an empty or scratch Set: the check adds two tracks.
3. On the testing machine, run the check through the local build or the Docker image:

   ```bash
   export SUNNY_LIVE_HOST=REPLACE_WITH_APPROVED_FORWARDED_ENDPOINT
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
writes its own diagnostics to standard error. Docker logs cover container output.
`installer/windows/SunnyHost.ps1 -Action Info` reads local host and process evidence without a
bridge. `-Action Log -LogPath 'REPLACE_WITH_ACTUAL_NATIVE_Log.txt'` reads at most 64 KiB from that
explicit physical log and returns metadata and matching line counts by default. Unfinished
lines are omitted and reported incomplete; malformed complete UTF-8 lines fail explicitly.
`-IncludeMessages` deliberately includes at most 100 selected, redacted lines; project content
may still remain. It never exports automatically or changes log bytes.

For an approved remote connection, `SunnyRemote.ps1` accepts `Info` and `Log` through the same
separate connection file and verified release. Its fixed operations also include `Status`,
`Install`, `Recover`, `Rollback`, `Uninstall` and `Confirm`. Arguments cross as encoded JSON data;
the remote operator source is captured from the checksum-verified release. Installation requires
the full release already transferred to its explicit destination. No operation creates SSH trust,
stops Live, or changes security settings. Remote operation deadlines are finite; a missing
acknowledgment requires status/recovery before repeating a filesystem change. The actual two-PC
path remains a final qualification step under the
[operating lifecycle contract](https://github.com/sunyaisblank/Sunny/issues/36).

`doctor_ableton` observes paired source/protocol, current bridge/Set tokens, and three transport
booleans with a second identity check. It does not collect a Set snapshot or mutate Live. The CLI
also checks actual process launch, stdio initialization and tool inventory, with a finite deadline:

```bash
python3 tools/doctor.py --timeout 30 --cleanup-timeout 10 --poll-seconds 5 --export /new/path/diagnosis.json -- \
  docker run -i --rm --mount type=volume,source=sunny-data,target=/data sha256:REPLACE_WITH_IMAGE_ID
```

The release copy is `operator/doctor.py`. Add the configured bridge route to the launch arguments
when diagnosing Live; an offline launch reports a precise bridge failure. Polling validates epochs,
gaps and page bounds, backs off on failures, and records incomplete/stale outcomes. Export is opt-in,
exclusive and bounded to 1 MiB, with credential redaction and no automatic project-content capture.
After diagnosis, a separate cleanup budget verifies that its owned processes and any launched
container are gone. Direct attached `docker run -i` is supported on Linux and Windows; the CLI
assigns a private container ID file and ownership label, checks the daemon identity, and removes
only its own container. On Linux, a private supervisor also reaps owned native child processes.
Windows standalone native launches and Docker wrappers, `exec`, detached or TTY launches are
currently unsupported by this CLI. Failed cleanup makes the report unsuccessful and leaves
its ownership identifier for scoped recovery. Installer confirmation requires successful cleanup.
Reported version-floor capabilities remain claims for later host qualification. Image identity,
installed host/library, audio, licences and independent host logs need their own evidence.

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
