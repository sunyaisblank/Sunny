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
AI client ──MCP (stdio)──▶ sunny-mcp ──approved local route──▶ Sunny Remote Script ──▶ Ableton Live
                             │
                             └── theory engine and documents (no Live needed)
```

Before native authoring, save the workspace durably and run a fresh `doctor_ableton` in that
same MCP session to establish its original bridge/Set identity. Reconnect or a historical receipt
does not grant new mutation authority. The lifecycle and recovery rules below apply to every
native workflow.

Documents reach Live only as a project: a Score, one Timbre profile for each of its parts, and a
Mix graph. `project_plan_to_ableton` records a snapshot of the Live Set and the exact list of
changes without musical mutation. `project_apply_ableton_plan` applies that plan once, refuses if
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

## Delivery and supported profiles

Docker is the normal MCP delivery path. Live and its Sunny Remote Script remain native on the
Live PC. The intended primary profile is a Windows 11 client with 64-bit Windows PowerShell
5.1/.NET, Windows OpenSSH and a local Docker Desktop Linux/amd64 engine, paired with Windows
Live 12.4.5 Suite. The normal Windows operator needs no installed Python interpreter.

| Profile | Available implementation | Qualification boundary |
|---|---|---|
| Windows client, Linux/amd64 Docker Desktop, native Windows Live 12.4.5 Suite | Verified release, managed installer, transfer, stdio launcher, doctor and recovery | Final integrated release, both actual hosts, UI and audio remain pending |
| Live 12.3.x | Finite managed-authoring compatibility retained | Exact patch, edition, installed devices and host remain unqualified |
| Live 11 | Limited legacy note operations | Version-specific runtime types and real-host behaviour remain unqualified |
| POSIX native executable or developer Python tooling | Source build, offline tools, direct diagnostics and qualification kit | Development profile; does not replace the Windows operator |

The Windows operator requires physical local NTFS paths and the current user's actual User
Library. UNC or linked installation trees, shared-network project volumes, Windows containers,
ARM release images and future Live versions are outside this production profile. Paths may contain
spaces and Unicode; Docker bind-mount paths in these examples must not contain commas.

Current source exposes 197 MCP tools, bridge protocol 47 and target snapshot schema 35. Treat
`release.json`, its bridge contract and actual `tools/list` as the selected artifact's authority.
A version-floor capability claim is not proof of edition, licensing, installed devices, native
acceptance or sound. No exact host/version combination has completed all final qualification gates.
Intermediate OCI networking, paired-bridge and volume-restore witnesses do not qualify the final
protocol-47 release. The acceptance and evidence boundaries are tracked in
[the lifecycle contract](https://github.com/sunyaisblank/Sunny/issues/36),
[distribution](https://github.com/sunyaisblank/Sunny/issues/37),
[Windows operation](https://github.com/sunyaisblank/Sunny/issues/38),
[diagnostics](https://github.com/sunyaisblank/Sunny/issues/40) and
[native authority and transport](https://github.com/sunyaisblank/Sunny/issues/41).

Complete independent local preparation first. Manual, separately approved two-way SSH setup and
verification on the two actual machines is the last setup stage. Approval or working access in one
direction does not establish the other. Sunny never installs keys, accepts a changed host key,
enables sshd, changes firewall or persistent execution policy, stops Live, or changes licensing. A remote
command below is a final-stage procedure after that setup, not permission to establish access.

## The paired offline release

A maintainer builds a release from a clean committed checkout with Python 3.10+ and Docker Buildx:

```bash
python3 tools/release.py build --output /new/path/sunny-release --tag sunny-mcp:local-release
python3 tools/release.py verify --release /new/path/sunny-release \
  --expected-manifest-sha256 REPLACE_WITH_SEPARATELY_RETAINED_MANIFEST_SHA256
```

Retain the printed lowercase `release.json` SHA256 independently of the transferred directory.
The release contains `image.tar`, exact `native/Sunny` bytes, `installer/windows/` operators,
`operator/doctor.py`, `operator/release.py`, the entire `operator/live_qualification/` kit,
locked build inputs, dependency inventories and `release.json`. The producer freezes committed
source, pins the Ubuntu platform digest, signed dated apt snapshot, package versions/hashes and
dependency revisions in `release/build-inputs.json`, and records actual compiler inputs. This
closes release inputs; it does not promise byte-identical compiler output on every machine.
An ordinary development `docker build` does not produce this verified release unit.

Use the trusted source verifier for initial full inventory/pairing verification. Execute packaged
operators only after their bytes have been authenticated against that independently retained
manifest. The read-only POSIX/Python verifier can then run from the selected release without a
Docker daemon:

```bash
python3 /path/to/release/operator/release.py verify --release /path/to/release \
  --expected-manifest-sha256 REPLACE_WITH_SEPARATELY_RETAINED_MANIFEST_SHA256
python3 /path/to/release/operator/release.py load --release /path/to/release \
  --expected-manifest-sha256 REPLACE_WITH_SEPARATELY_RETAINED_MANIFEST_SHA256
```

Use the returned `loaded_image_local_immutable_id` on that destination Docker store. OCI index,
manifest and config digests, the producer's local image ID and a registry manifest digest are
different identities. Never substitute a tag or describe a local ID as a published registry digest.
Publication remains pending an explicitly approved registry destination, visibility and retention.
The manifest's qualification fields remain pending until the required evidence exists.

On a normal Windows client, verify the independently retained manifest checksum and archive
checksum before loading; no Python is needed. The following uses the manifest only as data and
selects the destination store's actual immutable ID:

```powershell
$ReleaseDirectory = 'D:\Sunny Release'
$ManifestSHA = 'REPLACE_WITH_SEPARATELY_RETAINED_LOWERCASE_SHA256'
$DockerContext = 'desktop-linux'
$manifestPath = Join-Path $ReleaseDirectory 'release.json'
if ((Get-FileHash -LiteralPath $manifestPath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $ManifestSHA) {
  throw 'Release manifest checksum mismatch.'
}
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($row in $manifest.files) {
  $file = Join-Path $ReleaseDirectory $row.path.Replace('/', '\')
  if ((Get-Item -LiteralPath $file).Length -ne $row.bytes -or
      (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() -cne $row.sha256) {
    throw ('Release payload checksum mismatch: ' + $row.path)
  }
}
$archiveRow = @($manifest.files | Where-Object { $_.path -ceq 'image.tar' })
$archivePath = Join-Path $ReleaseDirectory 'image.tar'
if ($archiveRow.Count -ne 1 -or (Get-Item -LiteralPath $archivePath).Length -ne $archiveRow[0].bytes -or
    (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant() -cne $archiveRow[0].sha256) {
  throw 'Image archive checksum mismatch.'
}
docker --context $DockerContext image load --input $archivePath
if ($LASTEXITCODE -ne 0) { throw 'Image load failed.' }
$ImageId = $null
foreach ($identity in @($manifest.image.local_immutable_id, $manifest.image.oci_config_digest,
                       $manifest.image.oci_manifest_digest) | Select-Object -Unique) {
  $text = docker --context $DockerContext image inspect $identity 2>$null
  if ($LASTEXITCODE -eq 0) { $ImageId = ($text | ConvertFrom-Json)[0].Id; break }
}
if (-not $ImageId) { throw 'The loaded image exposed no verified immutable identity.' }
```

This checksum/load step does not prove native readiness. The verified Windows operators also
check the complete release payload and paired bridge, and client `Plan` checks the loaded image,
revision, Linux/amd64 platform and compiled configuration before its first SSH connection.
Keep the original reviewed candidate and its evidence unchanged; adding a new operator beside an
older release does not make it part of that release. Do not edit or create evidence/configuration
files inside the immutable release tree.

## External runtime configuration and MCP

Production configuration is separate UTF-8 JSON, selected by `SUNNY_CONFIG_PATH`. Schema 1 has
small `client` and `native` role sections. Both consumers reject duplicate or unknown fields,
future schemas, invalid endpoints and mixed legacy variables before workspace, logger or socket
effects. Configuration is bounded to 64 KiB/depth 8; ports are integers 1..65535. The native server
uses AF_INET and permits only an explicit numeric IPv4 loopback bind in production.

For offline authoring, create an external file such as `C:\Sunny Configuration\offline.json`:

```json
{
  "configuration_schema_version": 1,
  "client": {
    "transport": {"mode": "offline"},
    "workspace": {"path": "/data/workspace.sunny.json", "recovery": "none"}
  }
}
```

Validate using the exact loaded image, without network or project-volume access:

```powershell
$ClientConfig = 'C:\Sunny Configuration\offline.json'
docker --context $DockerContext run --rm --pull=never --network none `
  --mount "type=bind,source=$ClientConfig,target=/run/sunny/configuration.json,readonly" `
  $ImageId --validate-config /run/sunny/configuration.json
if ($LASTEXITCODE -ne 0) { throw 'Configuration validation failed.' }
```

For a simple offline MCP client, use an explicit named volume and immutable image:

```json
{
  "mcpServers": {
    "sunny": {
      "command": "docker",
      "args": ["--context", "desktop-linux", "run", "-i", "--rm", "--pull=never", "--network", "none", "--mount", "type=volume,source=sunny-data,target=/data", "--mount", "type=bind,source=C:\\Sunny Configuration\\offline.json,target=/run/sunny/configuration.json,readonly", "--env", "SUNNY_CONFIG_PATH=/run/sunny/configuration.json", "sha256:REPLACE_WITH_DESTINATION_IMAGE_ID"]
    }
  }
}
```

Theory, document and notation tools work offline; native tools report unavailable Live. Each
MCP client owns one container's stdin/stdout. Do not add a TTY or share unattended stdin between
clients. Durable operation depends on the configured workspace under the named volume; an
unconfigured ephemeral launch is not a production persistence profile.

For the selected remote profile, save another external configuration with both roles:

```json
{
  "configuration_schema_version": 1,
  "client": {
    "transport": {"mode": "tcp", "host": "host.docker.internal", "port": 49001},
    "workspace": {"path": "/data/workspace.sunny.json", "recovery": "none"}
  },
  "native": {"bridge": {"bind_host": "127.0.0.1", "port": 9001}}
}
```

Choose a free local forward port. `127.0.0.1` inside a container refers to that container;
`host.docker.internal` reaches the operator-owned Windows loopback SSH forward on Docker Desktop.
The launcher requires the workspace file directly under `/data`. It captures the external config
before validation so an edit cannot redirect that session between validation and native connection.
On the Live PC, select the external native configuration in Live's process environment with
`SUNNY_CONFIG_PATH` and restart Live deliberately. Addresses and library paths are configuration
data, not compiled machine defaults. Credentials and SSH connection data stay outside this JSON.

## Windows Run, transfer and installation

The separate connection file contains exactly these scalar fields, using existing approved keys
and verified known-host entries outside the release:

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

The profile requires an explicit local Windows account and a local Docker Desktop named-pipe
context. Remove inherited Sunny runtime variables and `DOCKER_HOST`, `DOCKER_CONTEXT`,
`DOCKER_TLS_VERIFY` or `DOCKER_CERT_PATH` before the explicit launcher; it refuses ambiguous
settings. Local script execution must already be permitted by machine policy. Docker daemon access
and trusted local code have substantial local authority: protect the machine, daemon and external
SSH files. Loopback binding and durable receipts do not authenticate an untrusted local process.
Plain bridge TCP across a LAN, arbitrary wrappers and unattended HTTP/services are outside this profile.

After all independent preparation and the final manual two-way SSH setup, select the same explicit
arguments for client preflight and the AI client's ordinary stdio command:

```powershell
$ConnectionFile = 'C:\Sunny Configuration\connection.json'
$ClientConfig = 'C:\Sunny Configuration\runtime.json'
$WorkspaceVolume = 'sunny-data'
$client = Join-Path $ReleaseDirectory 'installer\windows\SunnyClient.ps1'
$clientArgs = @{
  ConnectionFile=$ConnectionFile; ReleaseDirectory=$ReleaseDirectory
  ExpectedManifestSHA256=$ManifestSHA; ImageId=$ImageId
  ConfigurationFile=$ClientConfig; WorkspaceVolume=$WorkspaceVolume; DockerContext=$DockerContext
}
& $client -Action Plan @clientArgs
# Configure the MCP client to launch this same script with -Action Run and these arguments.
```

The MCP command is 64-bit `powershell.exe` (use its absolute executable path if client PATH resolves
a different build) with `-NoProfile -NonInteractive -File
PATH_TO_SunnyClient.ps1 -Action Run` and the same named arguments. For example, substitute the
selected values into this client configuration; the manifest checksum and immutable image are
literal arguments, not environment variables:

```json
{
  "mcpServers": {
    "sunny": {
      "command": "powershell.exe",
      "args": [
        "-NoProfile",
        "-NonInteractive",
        "-File",
        "D:\\Sunny Release\\installer\\windows\\SunnyClient.ps1",
        "-Action",
        "Run",
        "-ConnectionFile",
        "C:\\Sunny Configuration\\connection.json",
        "-ReleaseDirectory",
        "D:\\Sunny Release",
        "-ExpectedManifestSHA256",
        "REPLACE_WITH_RETAINED_LOWERCASE_SHA256",
        "-ImageId",
        "sha256:REPLACE_WITH_DESTINATION_IMAGE_ID",
        "-ConfigurationFile",
        "C:\\Sunny Configuration\\runtime.json",
        "-WorkspaceVolume",
        "sunny-data",
        "-DockerContext",
        "desktop-linux"
      ]
    }
  }
}
```

`Plan` verifies local release/image/configuration
through a network-disabled container, starts no SSH and mounts no project data. `Run` owns one
loopback-only SSH forward and attached Sunny container; stdout is protocol only. Tunnel loss ends
the session without replay. Normal cleanup checks exact container ID/name/owner and original daemon,
and preserves the project volume. Private jobs close owned local processes; abrupt interruption
can still leave a daemon container. Keep its printed recovery directory and ownership receipt.
After the recorded client ends, run `SunnyClient.ps1 -Action Recover -SessionDirectory PATH`. Recovery uses the receipt's original
Docker context/daemon and refuses changed or foreign scope before removal.

Transfer requires this release's exact `SunnyTransfer.ps1` hash in `release.json`, a new explicit
destination and an existing physical NTFS parent on the Live PC. It never installs Sunny or starts Live:

```powershell
$transfer = Join-Path $ReleaseDirectory 'installer\windows\SunnyTransfer.ps1'
$RemoteRelease = 'D:\Sunny Releases\release-REPLACE_WITH_REVISION'
$transferArgs = @{
  ConnectionFile=$ConnectionFile; ReleaseDirectory=$ReleaseDirectory
  ExpectedManifestSHA256=$ManifestSHA; DestinationDirectory=$RemoteRelease; TimeoutSeconds=600
}
& $transfer -Action Plan @transferArgs
& $transfer -Action Transfer @transferArgs
& $transfer -Action Status @transferArgs
```

`Plan` checks local data and connection prerequisites without SSH. Transfer admits at most 512
files/4 GiB in authenticated 1 MiB chunks, stages all bytes and returns a correlated committed
receipt. An identical committed rerun sends no payload. After interruption, retain the same source,
manifest checksum and destination; run `Status`, then `Recover`. Recovery verifies existing source
prefixes before resuming. `Clean` removes only proved journal-owned unfinished staging with matching
bytes; it preserves committed, foreign or edited data. Do not delete journals or lease files to bypass
refusal. Deadlines surround reads and filesystem phases; physical synchronous disk stalls are not
preemptible by this script. Injected write/rename/permission failures do not qualify physical disk
saturation or power loss.

Install into the current user's actual User Library shown in Live Settings > Library. Save Sets
and quit Live deliberately before Install, Rollback, Recover or Uninstall. Local operation uses the
verified release's `Sunny.ps1`; it never guesses the library, stops Live or changes Control Surface
settings. Remote operation uses fixed `SunnyRemote.ps1` operations with encoded JSON data:

```powershell
$admin = Join-Path $ReleaseDirectory 'installer\windows\SunnyRemote.ps1'
$adminArgs = @{
  ConnectionFile=$ConnectionFile; ReleaseDirectory=$ReleaseDirectory
  ExpectedManifestSHA256=$ManifestSHA; UserLibraryPath='D:\Actual User Library'
}
& $admin -Action Info @adminArgs
& $admin -Action Plan @adminArgs -RemoteReleaseDirectory $RemoteRelease
& $admin -Action Install @adminArgs -RemoteReleaseDirectory $RemoteRelease
& $admin -Action Status @adminArgs
```

For local installation, use `Sunny.ps1 -Action Plan`, then `-Action Install`, supplying
`-UserLibraryPath`, `-ReleaseDirectory` and `-ExpectedManifestSHA256`. The complete release is
verified, only the bridge is staged, and ownership plus the atomic state commit live under
`Remote Scripts/.sunny-managed`. Identical installation creates no new backup. Unknown or edited
Sunny content is preserved. A missing remote acknowledgment requires `Status`/installer `Recover`
before repeating a change: recovery restores the prior release before commit or completes the new
release after commit. Transfer, installer, client and doctor recovery own different resources.

Start Live deliberately and select Sunny in Preferences > Link/Tempo/MIDI with Input/Output None.
Run the matching read-only doctor, then `Confirm`. Local Confirm takes `-DoctorReport`; remote
Confirm takes `-RemoteDoctorReport`, naming the fresh report already on the Live PC. Arrange its
approved final-stage manual copy and verify its hash separately. Whole-release Transfer is not an
arbitrary report transfer. Confirm requires fresh correlated schema-1 readiness, selected active
source/protocol and successful launcher cleanup. Check the host/library association yourself;
the report alone cannot establish it. Confirmation is not sound or musical qualification.

A further forward `Install` cannot retire the recorded previous release before confirmation.
`Rollback` exchanges active/previous managed bridge files with Live closed; it does not downgrade
project data. `Uninstall` removes the managed active bridge, reports the retained backup and leaves
user Sets, settings, unrelated scripts and management state intact. Remove Sunny's Control Surface
slot manually afterward. Keep full compatible release/data backups until replacement lifecycle
checks have passed. Cross-user deployment and real-host power-loss recovery remain unqualified.

## Native authority, failures and persistence

Protocol 47 and the versioned SHA256 of bundled bridge Python sources must match the MCP binary.
The first ordinary request on every connection checks pairing; reconnect checks again. Mismatched
mutations decline before dispatch. Read-only profile/log routes remain available for diagnosis.
Pairing proves matching Sunny code, not native capability or playback acceptance.

Run fresh `doctor_ableton` in the same authoring MCP session before native preparation. It anchors
that session to the observed bridge instance and Set document token. Each queued request retains
its original admission epoch; a changed Set or bridge cannot inherit it. There is no automatic
refresh of native authority and no mutation replay after reconnect, timeout, cancellation or a
lost reply. Managed and ordinary tools use their durable attempt tokens. The legacy gateway records
workflow/stage fences and typed receipts before native effects; expert generic commands use
`legacy_ableton_request`, with `legacy_ableton_history` and `legacy_ableton_reconcile` for original-token
queries. Direct raw TCP mutation is outside this path. After native identity changes, inspect and
explicitly run a new read-only doctor/adoption workflow; a historical receipt is query authority,
not permission to dispatch again. An unresolved attempt remains uncertain until its original-token
query supplies admissible evidence.

The reader stays available while one worker runs tools. Typed request-ID cancellation reaches
active/queued work; requests have a 120-second lifetime including queue time. Shutdown and observed
stdin EOF revoke native admission, while EOF permits bounded local/read-only response drain.
Unread stdout has a five-second budget and pending output is at most 64 MiB; output failure ends
the session unsuccessfully. TCP resolution, connect, send/backpressure and receive have finite
budgets; partial requests are never replayed. A second native client receives busy while the
admitted client retains the bridge. The runtime's pinned Python 3 serves an owned bounded resolver
helper; it is separate from client Windows requirements. Cancellation or timeout does not prove
an already-started musical effect stopped, and synchronous kernel/Live calls cannot be promised
preemptible. Preserve uncertain receipts and stop subsequent phases rather than retrying setters.

Authored documents remain in memory until an explicit `workspace_save`. Save to the configured
`/data/workspace.sunny.json`; the next container restores it before accepting requests. Saved work
includes all document types, owning relationships, shared presets, corpus records, rendering
configuration and identity reservations. Native realization history is a separate namespace in
the same volume and survives authored undo and `.bak` recovery. Keep the namespace and workspace
together; missing history blocks native writes. Process-local undo stacks and temporary plans do
not survive restart. Samples and external preset files must also be backed up separately.

Every process reserves its configured workspace and backup before restore/admission. A second
writer is refused; open/save/applied recovery reserve their destinations too. Independent workspace
paths remain independent. Stop the owner before replacing a container or restoring data. OS locks
release after exit/crash. Keep stable hidden `.sunny-writer.lock` sidecars; replacement/deletion revokes
publication until restart. Parent-directory aliases share ownership, while final-file symlinks and
hardlinks are refused because atomic replacement splits their identities. Separate Linux-process
and intermediate named-volume witnesses exist; final-image and native Windows writer behaviour
remain qualification boundaries. See [writer admission](https://github.com/sunyaisblank/Sunny/issues/39).

### Schema migration and rollback

| Stored contract | Current supported reader/publication |
|---|---|
| Production configuration | Schema 1; explicit legacy-environment migration into a new file |
| Authored workspace | Reads versions 1/2; saves version 2 |
| Realization ledger | Reads schemas 1/2/3; publishes schema 2 without legacy workflows, schema 3 with them |
| Bridge / target snapshot | Protocol 47 / schema 35; paired build required |
| Managed installer and Confirm report | Independent schema 1 contracts |

Opening an older supported ledger reads it without rewriting. A later publication migrates its
stored representation while preserving managed/ordinary records and original tokens; schema 3 adds
legacy workflow/stage history. Never edit a schema number backward or drop fields to placate an older
binary. Before upgrading, stop the writer, retain the original workspace snapshot and complete
native namespace together, and test migration on a copied volume. An older image may not read newer
history. For software rollback, pair that old image and bridge with the compatible pre-upgrade
configuration and full restored volume; retain the newer volume for original-token investigation.
Bridge `Rollback` alone is not a data migration. Restoring authored work or `.bak` alone cannot
rewind the ledger or undo effects already performed in Live. Recheck readiness and current-object
adoption before resuming native writes.

Production startup recovery is explicit `client.workspace.recovery: "backup"`. A corrupt main file
normally fails startup; backup mode validates the supported `.bak` and restores authored state into
memory without silently repairing the main file. Review it, `workspace_save` explicitly to repair
the main file, then return recovery to `"none"`. It does not replace the realization ledger.
`workspace_open` validates before replacing authored state; `workspace_import` refuses identity
collisions. Neither operation is a replay permission.

Legacy environment mode is a migration/development input only when `SUNNY_CONFIG_PATH` is absent.
On the matching POSIX native executable, use `sunny-mcp --migrate-config NEW_OUTPUT`. The exported
standalone `native/Sunny/configuration.py --migrate-legacy NEW_OUTPUT` needs an operator Python
interpreter and produces the native role. Both create a new file exclusively and preserve old
settings/data. Deliberately combine required roles and validate with the exact image's
`--validate-config PATH` before selection. Migration refuses an unsafe native non-loopback bind.
Remove all five legacy variables (`SUNNY_ABLETON_HOST`, `SUNNY_TCP_PORT`, `SUNNY_WORKSPACE_PATH`,
`SUNNY_WORKSPACE_RECOVERY`, `SUNNY_BIND_HOST`) when selecting JSON; there is no merge or implicit
schema downgrade. The normal Windows launch does not require these developer migration tools.

### Stopped-writer Docker backup and restore

Save explicitly, close every writer on the volume and confirm its container ended before copying.
An active-write copy is not a consistent backup. Retain the whole volume: workspace, `.bak`, hidden
writer sidecars and every realization namespace. Also retain the matching immutable release,
manifest checksum, external configuration and external samples/presets. Use the verified image's
`tar` as a short-lived network-disabled utility; these commands require an existing backup directory
and do not start MCP or Live:

```powershell
$backup = 'D:\Sunny Backups'
$stem = 'sunny-data-' + [Guid]::NewGuid().ToString('N')
$partial = $stem + '.tar.incomplete'
$archive = $stem + '.tar'
docker --context $DockerContext volume inspect $WorkspaceVolume
if ($LASTEXITCODE -ne 0) { throw 'The source volume is missing.' }
docker --context $DockerContext run --rm --pull=never --network none --user 0:0 `
  --entrypoint tar --mount "type=volume,source=$WorkspaceVolume,target=/data,readonly" `
  --mount "type=bind,source=$backup,target=/backup" $ImageId `
  --numeric-owner -C /data -cf "/backup/$partial" .
if ($LASTEXITCODE -ne 0) { throw 'Backup failed; preserve the incomplete archive.' }
Move-Item -LiteralPath (Join-Path $backup $partial) -Destination (Join-Path $backup $archive)
$archiveHash = (Get-FileHash -LiteralPath (Join-Path $backup $archive) -Algorithm SHA256).Hash.ToLowerInvariant()
```

Retain that checksum separately with the release/image/configuration identities. Restore only your
trusted verified archive into a new volume, preserving the original and its newest receipts:

```powershell
$expectedArchiveHash = 'REPLACE_WITH_SEPARATELY_RECORDED_LOWERCASE_ARCHIVE_SHA256'
if ((Get-FileHash -LiteralPath (Join-Path $backup $archive) -Algorithm SHA256).Hash.ToLowerInvariant() -cne $expectedArchiveHash) {
  throw 'Backup checksum mismatch.'
}
$restored = 'sunny-data-restore-' + [Guid]::NewGuid().ToString('N')
docker --context $DockerContext volume create $restored
if ($LASTEXITCODE -ne 0) { throw 'Restore volume creation failed.' }
docker --context $DockerContext run --rm --pull=never --network none --user 0:0 `
  --entrypoint tar --mount "type=volume,source=$restored,target=/data,volume-nocopy" `
  --mount "type=bind,source=$backup,target=/backup,readonly" $ImageId `
  --numeric-owner --same-owner --same-permissions -C /data -xf "/backup/$archive"
if ($LASTEXITCODE -ne 0) { throw 'Restore failed; preserve both volumes and partial state.' }
```

Numeric ownership preserves Sunny UID/GID 1001. Use the same `/data` path; moving only the workspace
can break its recorded history location. Start exactly one offline MCP against the new volume and
read back authored documents, namespace and history diagnostics before selecting it in the launcher.
Keep the old volume until complete lifecycle checks pass. Restored fences stay query-only; reconcile
original tokens and establish fresh native authority rather than repeating musical mutations.
Container removal, installer uninstall and scoped recovery never delete the project volume.

## Read-only Windows diagnosis

Use the exact release's `installer/windows/SunnyDoctor.ps1`, trusted manifest checksum, image,
external configuration and volume. The release must actually contain this doctor and its Sunny,
SunnyRemote and SunnyClient helpers with the recorded hashes. The doctor captures verified bytes,
admits its process suspended into a private Windows job, records its identity before resume, and
uses the normal launcher. It needs Windows PowerShell 5.1/.NET, Docker Desktop and existing approved
OpenSSH files; no Windows Python interpreter is required.

Save work and close the ordinary authoring MCP first. Diagnosis starts its own client and does not
take another writer's lease or force-stop it. After the approved final route setup:

```powershell
$doctor = Join-Path $ReleaseDirectory 'installer\windows\SunnyDoctor.ps1'
& $doctor -Action Diagnose @clientArgs -TimeoutSeconds 60 -CleanupSeconds 20
# Add -ExportPath 'D:\Sunny Diagnostics\new-report.json' only for deliberate retention.
```

The default makes initialize, ping, tool discovery and one fresh correlated `doctor_ableton` call.
It observes paired source/protocol, stable bridge/Set identity and typed playing/recording flags;
it takes no full Set snapshot, calls no musical tool and does not probe port 9002. Readiness also
requires a normal protocol-only close and independently confirmed owned job/container absence on
the original daemon. A report created on the client is not already a file on the Live PC. Export
for Confirm deliberately, arrange its manual copy and verify the hash. This closes the diagnostic
session; authoring still requires its own fresh doctor admission.

`-PollSeconds 0` leaves `logs:null`. For a finite stream observation add `-PollSeconds 15
-PollInterval 0.25`. Duration is at most 60 seconds within the existing total maximum of 120 seconds;
cleanup has its own 5..30-second budget. Only `get_ableton_remote_log` runs in the same owned session.
Pages require fresh typed epochs, contiguous entries and consistent reset/gap/watermark flags.
Empty pages back off to five seconds; pagination advances immediately. At most 1,000 requests,
1,000 entries/page, 1,000 retained rows and 384 KiB of row JSON are admitted. The last valid cursor
and reset/gap/stale/omission facts remain visible after later progress. Lost/malformed/unavailable
pages, remaining pagination or exhausted bounds make readiness false; an uncertain request is not
retried. The native ring retains its latest 1,000 records and restarts change its stream identity.
Legacy logs without freshness/epoch metadata remain unavailable evidence.

Messages are absent by default. `-IncludeMessages` requires a positive poll duration and deliberately
retains redacted text. Logger names as well as messages may reveal project content; redaction does
not prove all content was removed. Export is opt-in, creates a new file exclusively and refuses
more than 1 MiB before creation. Raw native errors and captured stderr are not exported.

If cleanup is uncertain, retain the exact printed doctor recovery directory and durable receipt.
After the recorded doctor/client processes have ended, use the same trusted release:

```powershell
& $doctor -Action Recover -SessionDirectory $DoctorRecoveryDirectory `
  -ReleaseDirectory $ReleaseDirectory -ExpectedManifestSHA256 $ManifestSHA -CleanupSeconds 30
```

Recovery refuses active recorded identities, changed daemon/owner, altered helpers and foreign
content. It removes only proved owned containers and temporary state, confirms absence, and
preserves the project volume. If scope was never captured, retain evidence rather than infer
ownership from a directory name. A deadline never grants permission to retry a musical action.

When the native bridge did not load, `SunnyHost.ps1 -Action Info` reads local host/process evidence.
`-Action Log -LogPath PATH_TO_ACTUAL_Log.txt` returns metadata and matching counts from at most
64 KiB of that explicit physical native log. Partial lines/rotation are reported incomplete;
malformed complete UTF-8 fails. `-IncludeMessages` deliberately includes at most 100 selected,
redacted lines; project content may remain. `SunnyRemote.ps1 -Action Info`/`Log` expose those fixed
read-only operations over the approved final SSH connection. In-bridge logging shares the command
channel and cannot diagnose a missing bridge or service another request while one is running.
MCP diagnostics go to stderr; container logs do not replace independent native evidence.

The release's `operator/doctor.py` is a separate POSIX/developer option requiring Python. It admits
direct attached `docker run -i` with explicit external JSON or supported POSIX native commands,
owns its container label/CID and exact daemon, and has finite protocol/poll/cleanup budgets.
Windows native commands, arbitrary wrappers including SunnyClient, `docker exec`, detached and TTY
launches are refused by that Python CLI. Normal Windows operators use SunnyDoctor instead.

Local Windows fixtures cover literal MCP pipes, strict receipts, suspended/job/descendant cleanup,
UTF-8 and original JSON surrogate checks, NTFS link/permission refusals, finite polling/retention,
exclusive export and actual local Docker orphan recovery while preserving a foreign container and
named-volume sentinel. They do not exercise real SSH/Live, physical disk saturation/power loss or
preemption of synchronous kernel/Live stalls. A doctor pass cannot qualify musical effects,
installed libraries, licences, sound or the original 23 groups.

## Native capability limits

Managed authoring admits Live 12.3/12.4 only; later versions need review before writes. The bridge
retains Python 3.7 syntax compatibility and limited Live 11 behaviour inside Live's own Python.
Operation choices follow the official [Live Object Model reference](https://docs.cycling74.com/apiref/lom/):

| Live version | Note insertion/readback | Native device insertion |
|---|---|---|
| Before 11.0 | Note intent retained; insertion unavailable | Unavailable |
| 11.0.x | Insert notes; read starts in `[0, generated clip end)` | Unavailable |
| 11.1–12.2 | Complete Clip note population | Unavailable |
| 12.3.x / 12.4.x | Complete Clip note population | Reviewed candidates, exact host still pending |

The [Clip API](https://docs.cycling74.com/apiref/lom/clip/) distinguishes ranged access (11.0) from
complete-population access (11.1). Live 11.0 reports `observed_time_span` and
`entire_clip_population_observed:false`; out-of-range notes remain unobserved. The original ranged
observation remains fixed during a workflow. Additive insertion verifies the native returned note
IDs and observed notes; destructive note or envelope changes require complete-population evidence.
An explicitly owned new empty Clip may still receive its selected authored envelope. Snapshots omit notes
on every version, so snapshot equality cannot prove note-content stability. Live 11 has
[take lanes](https://www.ableton.com/en/live-manual/11/comping/); an unavailable observation cannot
establish absence. [Track.insert_device](https://docs.cycling74.com/apiref/lom/track/) has a 12.3 floor
and placement restrictions. Max for Live, actual installed devices and licensing need host evidence.

Deployment reports unsupported third-party/source intent, Group creation, general envelope
creation and unselected domains. Finite managed controls use native formatter/readback rather than
normalized-knob equivalence. Guarded Step panning is sampled evidence, not complete breakpoint or
DSP equivalence. [live.remote~](https://docs.cycling74.com/reference/live.remote~/) temporarily disables
automation; it does not author saved envelopes. Audio is neither rendered nor analysed.

`score_export_midi` returns type-0 Standard MIDI File bytes in `midi_base64` and a loss report.
`score_compile_to_musicxml` returns notation XML from the same authored Score, using concert pitch
for MIDI and instrument-transposed written pitch for MusicXML.

## Maintenance and final qualification

Maintainers update the base digest, signed dated package snapshot, exact package hashes/versions
and dependency commits together in `release/build-inputs.json`; review upstream/security and
compatibility changes before each release. Never patch a running production container or replace
locked indexes with mutable fallbacks. Rebuild a clean committed source revision, verify image and
exported native/operator bytes against the archive, and repeat affected config/protocol/persistence/
recovery checks. Native API or operating-profile changes require new host evidence. Retain the
previous compatible release, external config and whole stopped-writer backup until replacement
health/lifecycle checks pass. Breaking changes require an explicit migration and rollback constraint.

The qualification kit is developer POSIX Python tooling, not a Python prerequisite for Windows
operation. The image owns every kit byte and its read-only release verifier. Exporting now wraps the
exact complete offline release tree, including image archive and operators:

```bash
python3 tools/export_live_qualification.py \
  --image-id sha256:REPLACE_WITH_COMPLETE_IMMUTABLE_IMAGE_ID \
  --output /new/path/Sunny-qualification.zip
```

The unstarted-container export does not contact Live or install anything. Retain the returned ZIP
checksum and separate release manifest checksum. Extract to a new directory; the ZIP root is
`release/`. Verify the full immutable unit before qualification, using its image-owned helper:

```bash
python3 /path/to/release/operator/live_qualification/verify_artifacts.py \
  --release /path/to/release \
  --expected-manifest-sha256 REPLACE_WITH_SEPARATELY_RETAINED_MANIFEST_SHA256
cp /path/to/release/operator/live_qualification/configuration.json /external/path/qualification.json
```

The retained original `candidate.json` verification mode remains available for its original
candidate evidence; it is not the final full-release verifier. Never edit the configuration template,
obligations or evidence inside the immutable release. Configure the external copy with the selected
release directory/manifest checksum, source revision/hash, protocol/snapshot, actual immutable image,
actual tool count, host versions/edition/OS/licences, approved forwarded endpoints, explicit MCP argv
and new disposable workspace/evidence locations. Use the `release.py load` destination canonical
image ID only when it is one of the fully verified release graph's local, OCI index, manifest or
config identities. Tags and inferred labels do not substitute for that graph. Cross-store identity
unit fixtures are not a real cross-store daemon witness. Production runtime JSON and this
qualification configuration are separate contracts, even though each currently uses version 1.

The final kit launch must be direct `docker run -i --rm --pull=never`, with that selected immutable
image as its actual final image argument and no trailing command override. Bind one existing
external production JSON file read-only at `/run/sunny/configuration.json`, set exactly
`SUNNY_CONFIG_PATH=/run/sunny/configuration.json`, and bind the configured existing disposable
`host_mount_directory` writable at `/data`. The following argv illustrates that separate developer
profile; substitute the exact paths and verified ID in the external configuration:

```json
[
  "docker", "run", "-i", "--rm", "--pull=never",
  "--mount", "type=bind,source=/external/path/runtime.json,target=/run/sunny/configuration.json,readonly",
  "--mount", "type=bind,source=/external/path/disposable qualification data,target=/data",
  "--env", "SUNNY_CONFIG_PATH=/run/sunny/configuration.json",
  "sha256:REPLACE_WITH_VERIFIED_GRAPH_IMAGE_ID"
]
```

Executable/operator overlays, extra mounts, entrypoint or container-command changes, legacy/env-file
selectors, user/workdir overrides and host networking are declined before launch. This POSIX kit
profile does not wrap SunnyClient or establish SSH; use the separately approved forwarded endpoint.

Finish local gates on the integrated release first: real stdio and ordinary Docker networking,
resolver/unavailable/busy/wrong-pairing/deadline cases, restart, separate-process writer refusal on a
named volume, and stopped-writer backup/new-volume restore with literal authored/history readback.
Then manually establish and verify both approved SSH directions as the last machine setup step.
Record actual host identities and loopback-only native/forwarding scope before transfer/install/doctor.

Use a separately saved unique scratch Set named `SUNNY_HOST_QUALIFICATION_...`. Record its original
bridge instance and Set document token (`scratch_bridge_instance`/`scratch_document_token`) from
independent current read-only evidence; these original B/D identities are mandatory. The same name
on another Set is insufficient. Stop transport/recording and turn input monitoring Off on every Track
before ordinary mutating probes. Start with the explicit external configuration:

```bash
python3 /path/to/release/operator/live_qualification/host_runner.py \
  --config /external/path/qualification.json inventory
```

The optional `SunnyHostProbe` on port 9002 is a separately installed mutation instrument for finite
host-only probes, not a health service. Its scratch approval must refer to the same primary B/D.
Reviewed mutating kit requests use the C++ `legacy_ableton_request` authority gateway, durable
workspace/history and typed stage receipts instead of generic raw TCP setters. A running-cue probe
has its separate explicit running-scratch approval. Review each intended probe and pass its exact
approval; no script automatically refreshes authority or replays a lost mutation.

The optional real-mutating two-Part smoke test is a developer final-qualification check selected
only by the external authenticated configuration and explicit scratch approval:

```bash
SUNNY_LIVE_QUALIFICATION_CONFIG=/external/path/qualification.json \
SUNNY_LIVE_SCRATCH_APPROVED=1 \
python3 -m pytest tests/python/test_live_host.py -s
```

It authenticates the full paired release and image-owned kit, actual Docker/source contracts and
original B/D through a current doctor. It requires the same named stopped scratch Song, recording
and input monitoring Off, a new disposable workspace, and native history saved before and after
authoring. It leaves the authored material for independent observation and verifies that the release
inventory stayed unchanged. Old `SUNNY_LIVE_HOST`/legacy command settings no longer select native
writes. This smoke check does not satisfy the remaining musical/UI/audio obligations.

Complete transfer, install, fresh doctor, Confirm, author/revise, reconnect/lost-reply reconciliation,
update/failed-update recovery, compatible rollback and uninstall without losing user data/history.
Retain all 23 original groups in the image-owned `obligations.json`, including native runtime types,
independent note/control readback, saved-envelope reopen/recovery, visual/audible observations and
RDP connect/disconnect audio readiness. Local tests, an owned forward and a doctor pass do not mark
these groups passed. Keep evidence outside the release and against the exact artifacts/hosts in
[native qualification issue #22](https://github.com/sunyaisblank/Sunny/issues/22) and
[the lifecycle evidence contract](https://github.com/sunyaisblank/Sunny/issues/36). A blocked prerequisite
stays pending; final release/publication claims wait for the full required evidence and approved destination.

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

## Licence

MIT
