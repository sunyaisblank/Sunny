# Installing and operating Sunny

Use this guide for the paired Docker/Windows release, configuration, installation, recovery and diagnosis. These procedures describe the supported operating profile; final native qualification is still open.

[Sunny](../README.md) · [Authoring](authoring.md) · [Operation](operations.md) · [Development](development.md) · [Qualification](qualification.md)

## Find a procedure

- [Supported profile](#delivery-and-supported-profiles)
- [Build, verify and load a release](#the-paired-offline-release)
- [Runtime configuration](#external-runtime-configuration-and-mcp)
- [Windows transfer and installation](#windows-run-transfer-and-installation)
- [Authority and uncertain outcomes](#native-authority-failures-and-persistence)
- [Schema migration and rollback](#schema-migration-and-rollback)
- [Backup and restore](#stopped-writer-docker-backup-and-restore)
- [Windows diagnosis](#read-only-windows-diagnosis)
- [Final host qualification](qualification.md)

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

The Windows operator requires local NTFS paths and the current user's actual User Library.
The User Library may use OneDrive Cloud Files folders; junctions, symbolic links and unknown
reparse types are refused. UNC installation trees, shared-network project volumes, Windows containers,
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
`operator/README.md`, its `operator/docs/` guides, `operator/doctor.py`, `operator/release.py`, the entire `operator/live_qualification/` kit,
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
destination, an existing physical NTFS parent and PowerShell 7 (`pwsh.exe` on PATH) on the Live PC.
The client operator still runs in Windows PowerShell 5.1. Windows5.1's redirected binary-input
path failed actual-host qualification; the receiver therefore requires PowerShell7 explicitly.
Transfer never installs either PowerShell or Sunny, changes host settings, or starts Live:

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
