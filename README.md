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
| Score | Notes, rhythm, harmony, form | MIDI event data, MusicXML, LilyPond, Ableton clips |
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

Everything is exposed as 116 MCP tools by the `sunny-mcp` binary, a
JSON-RPC server on stdio. When `SUNNY_ABLETON_HOST` is set, the server
connects to a Remote Script inside Ableton Live over TCP and the
Ableton-mutating tools go live; without it, the server runs offline and
those tools decline with a clear error while the theory and document
tools keep working.

```
AI client ──MCP/stdio──▶ sunny-mcp ──TCP 9001──▶ Sunny Remote Script ──▶ Ableton Live
                           │
                           └── sunny::core (theory + IR documents)
```

## Status

The implemented runtime surface—the theory engine, four in-memory IRs,
versioned JSON, MIDI-event/notation compilers, target-profile-aware Ableton
compilers, and 116 MCP tools—is covered by more than 1,900 tests, with
mutation testing and static analysis in the harness. The formal
specifications also identify design extensions that are not runtime
capabilities; each such boundary is labelled explicitly. The transport
and Remote Script are verified against each other by loopback tests. Remaining external milestones
are validation against named Live builds and supported-platform build/load/timing tests for the Max
package.
`docs/ableton-max-conformance.md` adversarially maps every deployment claim to
the public Live/Max contract and its tractability; `docs/decisions.md` records
the engineering decisions and their evidence.

The host-neutral render source model now includes a validated `SignalBlockContext` matching the
documented Max `dsp64` setup pair of sample rate and maximum vector size. Native LFO, ADSR, and
message-triggered held-value block processing is bounded, `noexcept`, allocation-free, and
all-or-none on rejection; the evolving sources are exactly scalar-equivalent and the held source
fills the vector with its retained scalar. `sunny::max_adapter` serializes any number of
message-side producers through
a producer-only mutex into a fixed 64-command SPSC transfer, then applies at most 64 ordered
commands on the exclusive audio owner at each vector boundary. Callback extent/topology/storage is
validated before the first command is drained, and a DSP rebuild checks every pending LFO
frequency. A rejected rebuild disables processing under the stale context while preserving source
state and pending controls; the four SDK wrappers still register perform and validate a bounded
zero-output fallback against the current callback maximum. The opt-in `max-package/` project wraps
that boundary as traditional `sunny.lfo~`,
`sunny.adsr~`, message-driven `sunny.hold~`, local `sunny.clock~`, and ITM-scheduled
`sunny.events` SDK externals and stages output under its build prefix. This is not an `.amxd`, Max
for Live deployment, or named-host validation. Max runtime results use strict C++
`MaxValidationRecord` schema 1; the staged package contains a canonical incomplete handoff rather
than a pre-filled success claim. The installed `sunny-max-evidence` executable accepts a separate
closed `MaxValidationObservation`, derives every archive/binary/evidence SHA-256 from regular files,
and later re-verifies those bytes; neither tool invents the host observations themselves.
The package also stages a self-starting smoke patcher for the exact pinned Cycling '74 `max-test`
revision. Machine-readable right outlets expose closed `host_status`, `event_status`, and
`transport_status` tuples. An exact process-call counter plus scripted patch-cord removal/recreation
checks disconnected callback progress and fresh output after reconnection. A separate
callback-driven unnamed-global-transport trace checks ITM firing, equal-tick order, note release
formatting, clear/reassignment, and standalone transport identity; elapsed-time delays are bounded
observation windows or failure watchdogs, not success evidence. The pinned harness manifest maps 64
concrete assertions to exactly 14 of the 19 host checks. The remaining four checks and the
standalone-inapplicable Live-discontinuity check stay explicit; a green standalone smoke result is
not a complete Max record or Max for Live certification.
The staged SQLite extractor preserves raw rows only; `sunny-max-evidence apply-max-test` re-hashes
the retained database and makes every assertion-to-observation decision in C++.
Release coverage is now a second closed native contract. The staged
`max-release-matrix.json` requires exactly six complete records: standalone Max and Max for Live on
macOS x86_64, macOS arm64, and Windows x86_64. `assemble-matrix` embeds and hashes those records,
while `verify-matrix` re-hashes the exact record files. All cells must share one source/package
version, and both host kinds for a native target must bind the same archive and five binaries. This
is coverage of six explicit recorded environments—not every scheduler setting, operator
authentication, or re-verification of the artifacts/evidence beneath each record.
Transport can consume the same context and rejects an actual vector above the configured maximum
before advancing or dispatching. Its MIDI callbacks remain intentionally block-endpoint events
without sample offsets; vector bounds are not misreported as Max sample accuracy.
Both Max ABI counts remain signed through validation; negative maximum/actual counts fail before
`size_t`, span, vector allocation, transport mutation, or callback effects.
The host-shaped modulation overloads also validate that signed actual count before inspecting the
output pointer or constructing a span; a non-empty null output fails distinctly, while an empty
block permits null and is a no-op.
Stateful render objects are explicitly single-owner. The Max adapter establishes one logical
producer only after serializing all message publications; its audio callback never takes that
mutex, drains a fixed maximum, or calls Max. `BlockClock` isolates the same O(1), callback-free
block-to-tick recurrence used by `ClockAdapter` and the shipped `sunny.clock~`; the composed
Transport block overload remains
unsuitable for `perform64` because it can drain an unbounded event backlog and invoke unconstrained
callbacks.
`BlockClock` is not presented as Live transport synchronization: Max for Live transport clock-source
selection, named-device isolation, preview discontinuities, and tempo-change position limits remain
explicit adapter/host obligations.
Rebuilding `SignalBlockContext` changes validated rate/vector metadata but does not reset LFO,
Envelope, or BlockClock state; an adapter that resets on DSP stop/start must make that an explicit
device policy.

The cross-IR project model validates exact Score Part ↔ TimbreProfile ↔
ChannelStrip correspondence and derives all Live track targets from the
Score's authoritative Part ordering. `project_validate`,
`project_plan_to_ableton`, `project_apply_ableton_plan`, and the immediate
plan/apply convenience tool expose that aggregate boundary. Planning captures
a structural Live snapshot and exact dry-run command sequence without mutation;
applying is one-shot, rejects stale project/target state, and returns a mutation
journal even when Live fails part-way through. Collection order in downstream
documents cannot redirect a channel or device to another Part.

## Building

Requires CMake 3.28+, a C++23 compiler (GCC 13+ or Clang 16+), and
Python 3.10+ with development headers for the optional bindings.

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
# Optional: install headers, libraries, server, and CMake package metadata
cmake --install .bin --prefix /your/install/prefix
```

On macOS or Windows, the independent `max-package/` project recompiles the authoritative
host-neutral adapter/render translation units inside a pinned Cycling '74 SDK toolchain and stages
`sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`, and the explicitly local-position `sunny.clock~` under
its own build tree. It also stages `sunny.events`, a bounded permanent-event source on Max's
unnamed global ITM transport. In Max for Live that transport follows Live, but sample-accurate
delivery remains conditional on Max's scheduler/audio settings, timely high-priority admission,
and downstream-object support. Its event lists retain source release velocity as a third atom;
this does not itself prove MIDI-port, receiving-device, or audible behavior. The
`sunny_max_archive` target emits the exact staged `Sunny/` ZIP and SHA-256 sidecar consumed by host
evidence. This avoids an MSVC runtime mismatch and never installs directly into a user profile. See
`max-package/readme.md`.

Or via the Makefile, which also drives the analysis harness:

```bash
make            # build + all tests
make package-check # verify a clean downstream CMake consumer
make max-sdk-header-check # compile wrappers against pinned official Max SDK headers
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
cp -r remote_script/Sunny "${HOME}/Music/Ableton/User Library/Remote Scripts/Sunny"
```

Then in Live: Preferences → Link, Tempo & MIDI → Control Surface →
Sunny. The script listens on TCP 9001 and translates length-prefixed
JSON requests into Live Object Model calls. It binds to `127.0.0.1` by
default. Cross-host setups must deliberately set `SUNNY_BIND_HOST` for
the Ableton process and restrict access with the host firewall.

The common macOS path above is illustrative: use the `Remote Scripts`
folder under the User Library location configured in Live. Ableton supports
installing third-party Remote Scripts, but Sunny's `_Framework` host ABI is
version-coupled rather than a documented stable programming interface. The
bridge reports that distinction and the observed Live version at runtime.
The Python adapter targets Live 11 and 12; this is a compatibility target,
not a substitute for the named-build validation record described below.
The project apply and convenience-compile MCP tools can return versioned validation-record schema
1 when supplied explicit edition/platform/Remote Script and optional Max context. That record
retains the actual plan, journal, snapshots, and compiler evidence, while labelling unobservable
host facts as operator-supplied. It does not by itself certify sound, Max timing, or host identity.

## Python scripting

The optional `sunny` package is a thin facade over the same engine via
pybind11 — useful for notebooks and scripts, not required for the MCP
server:

```bash
uv build --wheel
uv pip install dist/sunny-*.whl
```

```python
from sunny.core.engine import TheoryEngine

engine = TheoryEngine()
engine.get_scale_notes("C", "lydian", octave=4)
engine.generate_progression("C", "major", ["I", "vi", "IV", "V"])
```

The platform-specific wheel contains both the Python facade and its
`sunny_native` extension. A source checkout can instead use `uv sync`,
which builds the same extension in the project environment. The facade
raises rather than falling back to approximations if the native module
is absent.

### Ableton capability boundary

Score track/clip/note deployment uses the Live 11+ note API. Timbre and
Mix native-device insertion uses `Track.insert_device`, which requires
Live 12.3+ and supports native Live devices only. The public Live Object
Model does not author tempo/meter automation, arbitrary preset or plugin
loading, group-track creation, or clip automation. The three IR deployment
tools therefore return both `success` and `complete`; requested features
outside that boundary are preserved in the IR and listed in `warnings`
instead of being translated into fictional Live operations.

Every live compiler now obtains a target profile from the bridge. Within the
Live 11/12 adapter range, targets before Live 12.3 skip unsupported native
device operations before changing the set,
and MCP results include the exact observed version and capability states. A missing or internally
contradictory profile fails before mutation; offline command recording uses an explicit modelled
target instead of inferring capabilities from absent evidence.
Max for Live availability is reported as `unknown`: Live's public Application
version surface does not establish licence/edition state, and Max for Live
devices cannot be inserted by `Track.insert_device`.

Insertion is not treated as parameterisation. Timbre and Mix use explicit IR-source/domain/curve
to exact-name `DeviceParameter` mappings with value, activity, and automation readback evidence.
Current parameter evidence also retains Ableton's conditional domain: continuous defaults or
quantized `value_items`. Labels remain opaque target text rather than Sunny enum identities.
Bridge protocol v43 uses closed, versioned request and
response envelopes. Collection cardinality uses explicit scalar count calls; the Remote Script
rejects unsupported private objects, non-finite numbers, non-string object keys, and integers
outside the wire domain instead of stringifying them. Documented target strings, integers, and
floats must arrive in their exact Python scalar category; profile, snapshot, cue, Device, and
DeviceParameter evidence is never made conforming with `str()`, `int()`, or `float()`. The bridge reads the target's existing device
count before mapped Mix insertion, addresses the exact appended device, rejects ambiguous or
disabled/non-changeable parameter names, validates internal ranges, and returns immediate readback
plus parameter activity/automation evidence. Aggregate postconditions then use an explicit
read-only final parameter observation without repairing target state.
Snapshot schema 34 also retains exact Boolean `is_frozen`, `arm`, and `implicit_arm` state for
every normal Track. Generated-Part Tracks receive exact `arm = false` and `implicit_arm = false`
writes/readbacks and gates require those values plus a final unfrozen observation. Ableton
documents armed Tracks as admitting monitored input and recording/overdub participation, while a
frozen Track plays freeze audio instead of recalculating current clip/device state. Disarming does
not replace the monitoring selector: the current public Track LOM omits that selector even though
Ableton documents Monitor In as suppressing clip output, so results separately keep
`monitoring_state_observed = false` and
`clip_output_not_suppressed_by_monitoring_verified = false`.
Schema 34 also retains exact Boolean `has_audio_input`/`has_midi_input` classification on every
normal Track. A generated Part requires `(false, true)`. When either input class is true, the
snapshot additionally requires exact selected `input_routing_type`/`input_routing_channel`
dictionaries, exact one-key `available_input_routing_*` collection wrappers, and full-dictionary
membership of both selections; a non-input-capable normal Track instead carries four explicit
null routing fields. Return and Main Tracks carry none of these input fields. Final Part evidence
exports classification and membership verdicts, but keeps `input_source_identity_mapped = false`
and `external_input_neutrality_verified = false`: a Live-advertised selection, including one whose
presentation label resembles “No Input,” has no portable Sunny meaning in the public contract.
Sunny performs no input-routing write, and these sequential reads prove neither monitoring state,
incoming events, nor sound.
Protocol v43/schema 34 also reads the public one-second hold-peak `input_meter_level` and
`output_meter_level` on audio/MIDI normal Tracks as exact finite floating values in `[0,1]`; the
same fields are null on other normal Track kinds. On a Track with audio output it additionally
reads `input_meter_left`, `input_meter_right`, `output_meter_left`, and `output_meter_right` once;
the four channel-resolved momentary fields are null without audio output. A generated Part gate
requires all six final values to equal exactly `0.0` and exports separate hold, momentary-stereo,
and conjunctive quiescence verdicts. Nonzero, integral, non-finite, out-of-range, or conditionally
incoherent evidence fails or incompletes as appropriate. The bounded one-read policy avoids an
unbounded GUI-meter polling load. `continuous_input_silence_verified` and
`continuous_output_silence_verified` remain false: sequential hold and momentary observations are
useful activity evidence, not a continuous event, signal-path, interface, acoustic, or
rendered-sound proof.
Schema 34 also records each normal Track's exact `back_to_arranger`, `fired_slot_index`, and
`playing_slot_index`. A generated Part gate requires false, -1, and -1, separating the absence of a
queued Session action from Track-specific Arrangement alignment. Its Clip gate independently
requires the complete occupied Session-slot set to equal exactly `{0}`; another valid Clip on the
generated Track can no longer coexist with a verified projection. Sunny observes but does not stop,
delete, or invoke Back to Arrangement, so this remains non-destructive point-in-time evidence.
Live 11+ Track snapshots additionally retain the cardinality of the separate `arrangement_clips`
child list, and a generated Part gate requires exactly zero. Earlier modeled profiles carry null and
cannot verify this obligation. This closes Arrangement-content absence at the sequential read
without serializing private Clip objects, deleting user content, or claiming future, playback, or
sonic stability.
They also retain the separate `take_lanes` cardinality and require zero. Ableton documents that a
Take Lane is audible when Audition Mode is enabled, but the public LOM exposes no audition-state
property. Excluding the topology therefore closes the tractable generated-Track proposition without
guessing an unobservable default; it does not make the sequential snapshot atomic or prove sound.
Protocol v43/schema 34 independently retains exact `Scene.is_triggered` for every Scene, not only
the generated row. Final Song evidence requires the complete ordered vector to be false through
`all_scene_launches_quiescent_verified`. That excludes a documented pending/blinking Scene launch
only at the sequential reads; Sunny issues no Scene fire/stop command and proves neither launch
completion, recording preferences, atomicity with Track/Clip state, future stability, nor sound.
The same schema closes the intervening ClipSlot layer with one complete ordered `clip_slots`
vector per normal Track. Every record retains exact Clip occupancy, Stop Button presence,
Group/control predicates, playing/recording/triggered predicates, integer playing status, and
will-record-on-start state. Slot indices/cardinality must equal `clip_slot_count`, the `has_clip`
set must exactly equal the separate Clip records, and both peers enforce Live's documented
playing/recording/status and non-Group relations. Generated Part evidence requires every slot to
be non-Group, idle, non-recording/non-will-record, and non-triggered. `has_stop_button` remains
target observation because Sunny has no corresponding intent. This catches a pending action on a
later empty cell without issuing a slot fire/stop operation or claiming atomicity, future
stability, recording-preference state, playback, signal, or sound.
The same gate requires an observed null Group Track parent. Group membership can otherwise insert
a summing Track's mixer/effects and commonly reroute the Part, while leaving every child Track fact
unchanged. This does not materialise or verify Sunny GroupBuses in Live.
Protocol v43/schema 34 also retains each normal and Return Track's exact selected
`output_routing_type` and `output_routing_channel` dictionaries and both corresponding
`available_output_routing_*` one-key collection wrappers. Every option is closed over string
`display_name` and `identifier`; the selected full dictionary must be a member of the advertised
collection or the snapshot fails closed. Exact selection or option-set changes participate in
stale-plan equality. Final evidence exposes the selected and available symbols beside Sunny's
requested `Master` or `Group(id)` edge and independently reports selection observation and exact
available-set membership. By default, source-to-target identity mapping and route verification
remain false because the public contract publishes no portable Main/Group identifier. Callers may
provide an exact named-target type/channel binding plus non-empty provenance for a materialisable
Channel/Aux-to-Master edge. Sunny then performs separate journalled type and channel mutations,
re-enumerating membership after the type change and requiring exact final readback before removing
the residual. Group destinations remain residuals. This is conditional verification under the
admitted mapping; Sunny never guesses from localized display text, opaque identifiers, or
fresh-Track defaults.
Each generated Part Track is also explicitly assigned to neither side of Live's Main crossfader
with exact integer `crossfade_assign = 1` readback. Snapshot schema 34 retains the public
assignment enum for normal and Return Tracks and requires `null` for the Master; the Part Track
gate requires the final neutral value. This removes one documented gain-stage attenuator but does
not establish output routing, monitoring, launch, signal, or sound.
The compiler also sets exact integer `panning_mode = 0` before applying the Part's single pan
value. Live's Split Stereo mode uses separate left/right positioning and is not equivalent to
Sunny's one scalar pan model. Snapshot schema 34 retains mode 0/1 on every mixer, and the Part gate
requires final Stereo Pan mode without claiming that panning itself is acoustically neutral.
Each generated Part also receives an exact floating Track Activator value derived from its Boolean
mute intent. Final activator evidence requires the requested value plus `is_enabled = true`,
`state = 0`, and `automation_state = 0`; a matching but disabled, inactive, or automated parameter
does not verify. Snapshot schema 34 also retains finite ordered internal bounds and exact
`is_quantized` on every selected mixer DeviceParameter. Activators must report a quantised domain;
volume, pan, and sends must report a continuous domain. Internal-value requests must lie inside the
observed bounds, while display-dB requests retain those bounds without comparing incompatible
domains.
Generated Aux Return Tracks receive the corresponding closed gain-stage projection: exact
`mute = false`, `solo = false`, `crossfade_assign = 1`, and `panning_mode = 0` writes/readbacks,
plus Track Activator 1.0 and the AuxBus return pan on the active Stereo Pan parameter. Snapshot
schema 34 additionally
requires exact Boolean Return `mute`, `solo`, and `muted_via_solo` state. Aggregate Return evidence
binds each AuxBus ID to its appended Return index and requires the final name, neutral crossfade/
pan mode, active/unautomated activator, own gates false, and no derived solo suppression.
The source MasterBus has no mute or pan field, but Live's Main Track has an independent activator
and pan stage. Mix compilation therefore lowers absent source intent to Main activator 1.0,
Stereo Pan mode 0, and centered pan 0.0, and a separate final Main gate requires all three with
enabled, active, unautomated parameter evidence. Non-pan spatial fields and output-route identity
remain explicit residuals, and these observations still do not prove signal or sound.
Every Live 11+ generated MIDI Clip also receives an explicit null `groove` association with
immediate readback. Snapshot schema 34 retains version-coupled `has_groove` state, so exact stored
note positions are not silently promoted through a non-destructive groove transformation. This
still does not claim rendered timing or velocity equivalence.
The same Clip receives Trigger launch mode, no clip launch quantization, Legato off, and zero
launch-velocity scaling, all with immediate and final readback. Snapshot schema 34 retains those
typed launch fields. The public Clip LOM does not expose the Follow Action state documented by
Ableton, and Sunny does not fire or trace playback, so results explicitly report
`follow_actions_observed = false` and `one_shot_playback_verified = false`; the marker range is a
finite unlooped structural interval, not a playback guarantee.
The final Clip record also verifies the public object identity instead of inferring it solely from
the navigation path: audio false, MIDI true, and Arrangement false on every modeled target, plus
Session true and Take Lane false on Live 11+. For the required unlooped state, read-only
`end_time` must independently equal the requested End Marker. These facts are retained through
`clip_identity_verified` and `playback_end_verified`; they are not a playback trace.
Snapshot schema 34 additionally records the public Boolean Clip runtime tuple `is_playing`,
`is_recording`, `is_overdubbing`, `is_triggered`, and `will_record_on_start`. A generated Clip's
final gate requires all five to be false, exposing separate `recording_quiescence_verified` and
`playback_idle_verified` verdicts. Sunny does not issue a stop command: this is a sequential
observation at one instant, not an atomic lock, future-stability guarantee, or playback trace.
At Song scope, schema 34 also retains exact Boolean transport-running, count-in, Arrangement
Record, Session Overdub, Automation Arm, `arrangement_overdub`, and legacy `overdub` state. Final
Song evidence requires the six recording/count-in modes false through
`recording_modes_quiescent_verified`; `is_playing` is exposed separately through
`transport_stopped_verified` and does not block an otherwise aligned live-playback deployment.
Sunny neither stops transport nor changes a record control. The public integer
`session_record_status` remains outside the gate because its current reference supplies no value
mapping.
Song evidence also separates current tempo equality from tempo authority. Snapshot schema 34
retains exact Boolean Link, Link start/stop sync, Tempo Follower, and tempo-nudge state; the Song
gate requires all five controls inactive through
`public_tempo_controls_quiescent_verified`. Sunny does not change them. Incoming MIDI-sync
enablement and the tempo automation envelope have no complete public Song snapshot surface, so
`external_midi_sync_state_observed`, `tempo_automation_state_observed`, and
`tempo_stability_verified` remain false. A matching tempo is therefore a current scalar fact, not
a promise about its next value.
On Live 12.1+, snapshot schema 34 also retains the exact documented `TuningSystem` name,
pseudo-octave, note-range, reference-pitch, and note-tunings payloads. The four dictionary payloads
are deliberately opaque because Cycling '74 does not specify their member schema; exact retention
strengthens stale-plan and final evidence but does not authorize mutation, map them to
`ScoreTuning`, establish Track/device support, or verify audible pitch.
Schema 34 also observes `back_to_arranger` and `re_enable_automation_enabled`. The Song gate
requires both false through `arrangement_playback_aligned_verified` and
`automation_overrides_quiescent_verified`: a highlighted Back to Arrangement control means Session
playback differs from stored Arrangement content, while Re-Enable Automation means at least one
automated control is currently overridden. Sunny invokes neither repair action, and the two
sequential observations do not prove future stability.
The same Song gate requires exact `loop = false` and `metronome = false`, exported as
`arrangement_loop_disabled_verified` and `metronome_disabled_verified`. An enabled Arrangement
Loop repeats a bounded region, while the metronome can add audible clicks when playback or a Clip
launch runs. Sunny changes neither control. Loop start/length remain intentionally unobserved and
dormant under the required false loop invariant.
The independently stored loop brace is not claimed observed: while `looping = false`, Ableton
defines the played region by the verified clip start/end markers. A future looped projection must
add exact loop-start/end intent and evidence.
Ableton also documents launch-time MIDI Clip bank/sub-bank/Program Change controls, but the public
Clip LOM exposes no corresponding state and clip creation promises no neutral default. Sunny
therefore reports `midi_bank_program_state_observed = false` and
`program_change_suppression_verified = false`; device identity/parameter equality is not silently
promoted into proof that a launched Clip cannot select another preset.
Every generated Clip is also sent the public all-envelope clear before note insertion, with an
immediate exact `has_envelopes = false` readback. Snapshot schema 34 retains the same Boolean and
requires final absence, closing the tractable Clip-owned automation/modulation/MIDI-controller
layer without pretending to author Score automation or prove MPE expression, external control, or
audible output.
Per-note MPE expression is a separate model: Ableton documents note-owned pitch-bend, slide, and
pressure curves, while the public Clip note dictionaries expose none of them. Sunny therefore
exports `mpe_note_expression_state_observed = false` and
`mpe_note_expression_neutrality_verified = false`; clearing Clip automation and verifying the
ordinary nine-field note readback does not prove neutral expression.
Protocol v43 also retains each observed Device's public sample/millisecond latency reports in
insertion and snapshot evidence. They invalidate stale plans when changed, but remain explicitly
short of complete render-path latency: delay compensation/monitoring modes, Track Delay, routing,
buffers, drivers, external hardware, and acoustic delay are not inferred.
The same Device evidence retains exact Boolean `can_have_chains`. Ableton identifies true as a
Device Rack, whose parallel or nested chains, selectors, zones, and chain mixer controls have no
counterpart in Sunny's flat serial device intent. Immediate and final evidence expose
`flat_device_verified`; only a non-Rack insertion can verify. A Rack is reported as well-formed
target divergence rather than silently approximated or recursively guessed.
Compiler results report mapped, verified, missing scalar, and non-parameter residual state
separately; partial scalar equality is never presented as audible equivalence.

Relative faders are executable constraints rather than prose annotations. Channel/group references
resolve transitively as exact additive dB equations before target mutation. Cycles and missing
references are rejected transactionally. LUFS-relative intent remains explicitly
measurement-dependent—Sunny applies the stored absolute fallback and reports the residual instead
of treating programme loudness as a fader position. `resolve_mix_fader_levels` exposes the complete
proof state without requiring Ableton.

## Development

```bash
make test           # C++ suite (Catch2)
make package-check  # install and link an isolated CMake consumer
make python-check   # ruff + mypy + runtime-stub comparison + Python tests
make mull           # mutation testing (clang-18 + libc++)
make codeql         # custom CodeQL queries
```

Public C++ types use `PascalCase`; functions, variables, namespaces, and
files use `snake_case`; constants use `UPPER_SNAKE_CASE`. Public headers
live under `include/sunny`, implementations mirror them under `src`, and
tests mirror the production domains under `tests`. See
[docs/architecture.md](docs/architecture.md) for the dependency rules.

## Repository layout

| Path | Purpose |
|---|---|
| `include/sunny/` | Public C++ API, grouped by `core`, `render`, `max`, and `infrastructure` |
| `src/` | C++ implementations and the pybind11 binding |
| `apps/` | Executable entry points |
| `max-package/` | Opt-in Max SDK package, native externals, help, and reference metadata |
| `python/sunny/` | Python facade |
| `remote_script/Sunny/` | Ableton Live control-surface integration |
| `tests/` | Native and Python tests, mirroring production domains |
| `docs/formal/` | Normative music, IR, render, and aggregate project specifications |
| `cmake/` | Dependency and warning policy |

## Documentation

| Document | Contents |
|---|---|
| [docs/formal/](docs/formal/) | Formal specifications: music theory, the four IRs, and cross-IR project semantics |
| [docs/formal/sunny-project-model.md](docs/formal/sunny-project-model.md) | Exact cross-IR identity, validation, allocation, and aggregate compilation |
| [docs/formal/sunny-render-model.md](docs/formal/sunny-render-model.md) | Modulation, arpeggiator, deterministic seed, error, and tick-transport semantics |
| [docs/architecture.md](docs/architecture.md) | Repository structure, layer boundaries, naming, ownership, and concurrency |
| [docs/reference.md](docs/reference.md) | Error ranges, contracts, schema versions, and wire protocols |
| [docs/decisions.md](docs/decisions.md) | Decision record and audit history |
| [docs/ableton-max-conformance.md](docs/ableton-max-conformance.md) | Adversarial Live, Max for Live, Max SDK, and RNBO conformance matrix |

## Licence

MIT
