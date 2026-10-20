# Final release and Ableton qualification

This is the final stage after independent software work. The kit preserves every original obligation; a model, a doctor pass or a smoke test cannot replace the required native observations.

[Sunny](../README.md) · [Authoring](authoring.md) · [Operation](operations.md) · [Development](development.md) · [Completion](completion.md)

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

For a managed-operation lost-reply check, run the paired `fault_proxy.py` with `--config`,
`--drop-method` and `--log` (use `--help` for the supported methods). Point the client's production
JSON bridge route at its loopback listener; keep the qualification configuration's `bridge_host`
and `bridge_port` at the approved native forward. The proxy checks the original durable dispatch
fence using a bounded, network-isolated child of the same verified image and a read-only `/data`
bind. This preserves the image user's private history permissions when the host has a different
UID. Failed observation withholds the native request. One matched reply is retained in the evidence
log and withheld from the client; reconcile using the original token before any further write.

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

