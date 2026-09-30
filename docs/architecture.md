# Sunny architecture

This document describes the maintained repository shape and the constraints that keep its layers
separable. The formal documents in `docs/formal` define musical behaviour; this document defines
software structure.

## Layer model

Dependencies point inward:

```text
apps/sunny_mcp.cpp        python binding        Ableton Remote Script
         │                      │                       │
         └──────────────┬───────┘                       │ TCP
                        ▼                               │
             sunny::infrastructure ◀────────────────────┘
                  │              │
                  ▼              ▼
             sunny::render   sunny::core
                  ▲              ▲
                  └─ sunny::max_adapter
                              ▲
                    Max SDK wrappers (opt-in)
```

- `sunny::core` owns theory, exact value types, and the Score, Timbre, Mix, and Corpus document
  models. It has no dependency on a DAW, transport, executable, or Python.
- `sunny::render` owns validated deterministic control/event generation and depends only on
  `sunny::core`. Its exact state, seed, and timing contract is
  `docs/formal/sunny-render-model.md`; it is not an audio engine or host scheduler.
- `sunny::max_adapter` owns the host-neutral, bounded Max message-to-audio transfer and depends only
  on `sunny::render`. It serializes publishers outside audio but never exposes Max SDK types.
- `sunny::infrastructure` owns file formats, MCP, corpus ingestion, orchestration, and the Ableton
  adapter. It may depend on `core` and `render`; neither may depend on it.
- `apps` and `src/bindings` are composition roots. They construct dependencies and contain no
  domain policy.
- `remote_script/Sunny` is a separately deployed Python adapter for Ableton Live. It shares a wire
  contract with `sunny::infrastructure::ableton`, not source-level dependencies.
- `max-package` is a separately configured, platform-gated Max SDK composition root. It recompiles
  the authoritative adapter/modulation/context translation units inside the SDK toolchain so Windows
  runtime-library selection cannot diverge. Its four signal generators fail construction if their
  required one-outlet topology cannot be created, and no wrapper publishes its class global unless
  all documented methods and the public class register successfully; the core build never acquires
  an SDK dependency.

The CMake targets encode these boundaries as `sunny::core`, `sunny::render`,
`sunny::max_adapter`, and `sunny::infrastructure`. Production source lists are explicit. During
every configure, `cmake/Architecture.cmake` independently enumerates the owned source directories
and rejects an orphan implementation, a foreign implementation attached to the wrong target, a
forbidden `sunny/<layer>/...` include, or a different local target-link edge. This makes the stated
dependency graph an executable invariant rather than a diagram maintained by convention. Every
inspected source, header, package authority, and current reference document is also a configure
dependency, so editing an existing file reruns the gate during an incremental build. CTest
configures six deliberately invalid fixture projects and requires each failure class to be
rejected. The independent Max package uses the SDK's package-aware external discovery only inside
its five leaf wrapper directories.

## Repository topology

```text
Sunny/
├── apps/                    executable entry points
├── cmake/                   dependency and compiler-warning policy
├── docs/                    architecture, reference, decisions, and formal specifications
├── include/sunny/
│   ├── core/                public theory and document API
│   ├── render/              public deterministic rendering API
│   ├── max/                 public host-neutral Max adapter API
│   └── infrastructure/      public adapters and application API
├── max-package/             opt-in macOS/Windows Cycling '74 SDK package
├── python/sunny/            Python facade
├── remote_script/Sunny/     Ableton Live Remote Script
├── src/
│   ├── bindings/            pybind11 composition root
│   ├── core/                core implementations
│   ├── max/                 host-neutral Max transfer implementations
│   ├── render/              render implementations
│   └── infrastructure/      adapter implementations
└── tests/
    ├── architecture/       adversarial layer/source/dependency rejection fixtures
    ├── core/
    ├── render/
    ├── infrastructure/
    ├── max_sdk_headers/     wrapper compilation against pinned official SDK headers
    └── python/
```

Build products and analysis reports belong in ignored directories such as `.bin`, `.bin-debug`,
`.codeql-results`, and `.mull-report`; they are not repository structure.

## Naming and file policy

- Namespaces, functions, variables, data members, and files use `snake_case`.
- Types, concepts, and enum classes use `PascalCase`.
- Constants use `UPPER_SNAKE_CASE` where they are part of the established public API.
- Headers use `.hpp`; implementations use `.cpp`.
- A public header's path describes its meaning, for example
  `sunny/core/voice_leading/constraints.hpp`. Opaque component identifiers are not used.
- Includes use the installed form (`#include <sunny/...>`) and never depend on a source-tree-relative
  path.
- Tests use semantic behaviour names and mirror the production domain they exercise.

Formatting is defined by `.clang-format` and `.editorconfig`. C++ is compiled as ISO C++23 with
compiler extensions disabled and warnings treated as errors.

## Value, error, and ownership rules

- Small domain values have safe default states. Validated factories return `Result<T>`, an alias for
  `std::expected<T, ErrorCode>`.
- Exceptions do not cross public module or wire boundaries. Errors are returned or translated to a
  protocol error at the boundary.
- `Beat` is an encapsulated canonical rational value. Construction normalises valid program
  pairs, dynamic pairs use `Beat::from_ratio`, JSON readers reject malformed input, and component
  access is read-only. Comparison does not form overflowing cross-products, multiplication and
  division cross-cancel first, and checked operations report unrepresentable results. Host
  adapters convert this canonical whole-note coordinate into their declared units; they never
  repair internal rational spelling after planning or persistence.
- `PositiveRational` applies the same construction-before-validation rule to strictly positive
  tempo rates while remaining a distinct semantic type. Equivalent BPM fractions have one stored
  identity, dynamic pairs use a checked factory, and persistence is canonical before beat-unit
  resolution or conversion to Live/Max floating tempo coordinates.
- `TimeSignature` encapsulates a non-empty positive group partition and a power-of-two denominator.
  Dynamic/JSON input crosses a checked factory, while algorithms receive only valid immutable metre.
  Each target discharges that source property independently. MusicXML and LilyPond retain ordered
  additive groups; SMF carries a metronome-click interval but no grouping field; Live's documented
  Song/Clip surface retains only flat numerator/denominator scalars. Neither numerical agreement
  between click and group span nor successful scalar readback is grouping evidence. Reverse SMF
  ingestion applies the deterministic flat grouping and records nonmatching click intent rather
  than fabricating source structure. Target-specific requested/written evidence exposes each
  residual.
- Harmonic construction has one interval authority in Core. A registered `ChordVoicing` realization
  is complete and inversion-coherent or absent; it cannot carry a quality label after silently
  dropping an out-of-range member. Section-harmony workflows derive analysis from the active
  per-position key/mode and commit only after structural validation. A non-chord slash bass is not
  approximated as an inversion because the current voicing type has no independent pedal-bass
  field.
- Low-level `MidiFile` owns exact SMF track/channel and CC/program payload. Corpus may narrow that
  into one analytical Score Part, but only a sole source note channel and completed binary
  CC64/CC67 spans cross exactly into Score routing/directives. Threshold-equivalent switch bytes,
  unsupported or ambiguous controller state, Program Change, multiple note channels, and Type-1
  topology remain named correction evidence. An articulation mapping is never reverse-engineered
  from a raw tick event because its missing semantic articulation identity is core state, not an
  infrastructure guess.
- Corpus MIDI quantisation is a checked exact-rational transformation at the ingestion boundary:
  onset and duration independently snap to `1/g` whole-note units, positive half-grid ties round
  forward, and a positive duration cannot collapse below one grid unit. Only the two RMS evidence
  summaries use floating point. This internal analysis grid is neither Live Clip launch/groove
  quantisation nor Max transport/scheduler/playback quantisation, and it makes no host-timing claim.
- Owning resources use RAII. `TcpTransport` owns its socket through a private handle; local candidate
  sockets become object state only after a complete connection succeeds.
  Remote Script shutdown closes the listener and idle/partial-frame client, fences new dispatch,
  and cancels queued main-thread work on surface disconnect. Already-started requests keep their
  definite outcomes. A server instance is single-use; a surface reload constructs a fresh instance.
- Non-owning relationships are references, pointers, or spans whose lifetime is controlled by the
  composition root. Copy and move are explicitly disabled for stateful transports.
- MCP registration groups receive shared, identity-stable session stores from the composition
  root. Cross-IR project tools construct a non-owning `ProjectView`, validate exact Part bindings,
  and compile through one Score-derived Part-to-track map. Tool groups do not maintain anonymous
  duplicate stores in the production server.
- Shared MCP evidence has one private Infrastructure encoder authority. Standalone domain tools and
  aggregate Project tools call the same encoders for compilation reports, full validation
  diagnostics, Score tuning, notes, Clip-envelope clears, CuePoints, and scalar-property
  deployments; Timbre parameter deployments; device insertions; and Mix Return/Main Track,
  parameter, and parameter-coverage evidence. Handlers do not independently spell those fields or
  recompute report summaries.
  A validation diagnostic always exposes `rule`, `severity`, `message`, and its domain
  `error_code`, adding `location` and `part_id` exactly when the Core value carries them. This keeps
  public JSON downstream of the C++ evidence model without exporting a JSON dependency into Core.
- Score root identity is repository-owned. Core creation accepts it in `ScoreSpec`; derived-score
  APIs require the destination identity; and the MCP Score map key always equals stored
  `Score.id`. The checked session sequence admits `UINT64_MAX` once and then rejects creation or
  reduction without wrap, overwrite, or counter mutation.
- Corpus ownership and period membership are maintained as bidirectional relationships. Lifecycle
  workflows normalise those references, synchronously rebuild composer/period aggregates affected
  by ownership, analysis, period, or removal changes, and invalidate signature patterns only when
  their underlying composer analysis set changes. C14 rejects contradictory map identities,
  ownership, period membership, and bounded period ranges at the v2 load boundary; legacy v1
  documents remain lenient so migration is explicit rather than silently destructive.
- Undo history stores typed forward and inverse bridge-message batches. It does not retain callbacks
  capturing an orchestrator instance.

## Concurrency and the Live boundary

`Score` is a lock-free serialisable value, not a concurrently mutable object. Shared application
ownership uses `ScoreDocument`: copied handles share one logical document, readers retain immutable
`shared_ptr<const Score>` versions, and a writer prepares and structurally validates a private copy
under a per-document serializer. Publication is one pointer replacement under a short exclusive
snapshot lock. Readers therefore see the complete old or new version and can keep using an old
version after a commit. Raw Score mutation still requires exclusive ownership; only its
candidate-local ID allocator is touched, so independent document writers share no identity state.
The allocator indexes imported Event, Part, or Section IDs, fills the lowest positive hole, and is
discarded with a rejected candidate; derived views likewise use an output-local event namespace.
Raw edits,
formal workflows, undo, and redo all reject an exhausted `uint64` version before changing either
the value or its history; versions therefore cannot wrap and alias an older host-facing snapshot.

The production MCP composition remains deliberately request-serial: `McpServer` admits only one
`process_request` call at a time on an instance and retains that ownership through tool-handler
execution. Its identity-stable raw Score/Timbre/Mix/Corpus session maps are therefore exclusively
owned for each request. Registration is a pre-run lifecycle phase. A parallel dispatcher cannot
reuse those maps directly; it must adopt the Score snapshot boundary and define equivalent
ownership for every other session and cross-IR project plan.

The C++ transport serialises each request as a four-byte big-endian length followed by UTF-8 JSON.
Transport state is protected by its owning call path and reconnects on demand after startup or
connection loss.

Ableton's Live Object Model must be accessed from Live's main thread. The Remote Script therefore
uses its socket thread only for framing and parsing. It queues requests, schedules a main-thread
drain through the control-surface API, executes all LOM reads and writes there, and returns the
result to the waiting socket request. Note insertion uses Live's documented dictionary-shaped note
payload.

The adapter surface is capability-checked against the public Live Object Model. A versioned bridge
handshake reads Live's documented Application version functions and describes the target before a
compiler mutates it. Score note deployment uses the Live 11+ Clip API. Native device insertion uses
`Track.insert_device` and thus requires Live 12.3+; that API cannot insert plug-ins or Max for Live
devices. Older targets skip unsupported operations with explicit diagnostics. Where LOM exposes no
authoring operation—for example group-track creation, arbitrary preset loading, or
automation-envelope creation—the compiler reports an incomplete result and emits no substitute
command.

Aggregate deployment is defined in `docs/formal/sunny-project-model.md` as a guarded plan/apply
protocol. Planning validates the canonical project state, assembles one structural target
observation during a synchronous main-thread call, seeds a recording transport with that topology,
and dry-runs the complete Score → Timbre
→ Mix command sequence without target mutation. Applying rechecks both project and target, consumes
the plan once, and compares every mutation with its approved phase/type/path/name/arguments before
sending it. A mutation journal and post-apply snapshot survive failures. Deployment is still not a
transaction because public LOM supplies no rollback primitive; an acknowledged or indeterminate
earlier entry may therefore leave a partial Set.

An optional MCP `validation_context`, validated before target access, turns that actual consumed
plan and attempt into strict `AbletonValidationRecord` schema 1. The record retains canonical
project state, planning/apply/final snapshots, the exact ordered current-protocol requests,
journal, aggregate compiler/postcondition evidence, explicit operator provenance, and recovery
notes. Its `execution_trace_complete` verdict requires acknowledged real-transport entries and is
therefore false for command recording. It is deliberately independent of compiler completeness
and audible correctness. Without the context, the public result exposes a null record rather than
inventing edition, platform, Remote Script, or Max facts.

Max host evidence is a separate Infrastructure boundary because it validates retained JSON but
must not put nlohmann JSON or host-test policy into the real-time `sunny::max_adapter`. Strict
`MaxValidationRecord` schema 1 binds one source/package revision to one supported named-host/audio
configuration, the five binaries, and a closed content-addressed check set. The native parser owns
all conditional and derived-completeness rules; `sunny_native` only forwards JSON to it. The Max
SDK wrappers never parse or emit this record from their audio or scheduler paths. A separate closed
`MaxValidationObservation` carries unhashed host facts. The non-real-time `sunny-max-evidence`
executable resolves only regular files below declared package/evidence roots, streams their bytes
through the native SHA-256 materializer, emits the final record, and can recompute every digest.
This provenance workflow remains Infrastructure; it does not enter `sunny::max_adapter` or a host
callback.

The package's pinned `max-test` smoke is a bounded observation adapter, not a second evidence
authority. Each signal wrapper emits a closed right-outlet `host_status` tuple derived from its
lock-free adapter status, and `sunny.events` emits closed `event_status` and `transport_status`
tuples; their left outlets retain the documented signal/event products. The smoke owns an unnamed
standalone-Max transport precondition and advances a six-event assertion sequence from event
callbacks, with a delay used only as a failure watchdog. Before that trace, the patch uses named
`thispatcher` disconnect/connect operations, exact monotone process-call snapshots, and fresh
`test.sample~` callbacks to observe disconnected processing and reconnect continuity.
`max-test-harness.json` pins the upstream harness revision, database schema, 64 assertion names, and
a mapping for exactly 14 standalone-host checks. Four checks remain explicitly unautomated and
`live_transport_discontinuities` is
inapplicable to that host kind. The external runner/database must still be executed and retained,
then its bounded observations must enter the independent evidence workflow; staging or parsing the
patch alone is no runtime fact.

The remaining operator boundary is closed as a procedure without being relabelled host evidence.
`max-host-run-plan.json` fixes the six-cell baseline, pinned runner/submodule identities, residual
measurement counts, scheduler scenarios and sample spacing, received MIDI bytes, PCM criteria, and
Max for Live discontinuity scenarios. `prepare-max-host-run.py` validates one archive sidecar and
byte-for-byte archive/extracted-package equality before creating a non-overwriting cell workspace;
its observation contains only operator provenance, host applicability, and `not_run` outcomes. The
M4L cell additionally stages the exact assertion map. `prepare-m4l-device-source.py` verifies the
pinned upstream console helper and deterministically transforms the standalone smoke into editable
Max Audio Effect source whose JS protocol retains the same 64 assertion names. It removes the
standalone global-DSP `dac~` start edge and declares an unconnected `plugout~`, so the validation
device is silent; `export-m4l-assertions.py`
projects one marked console session with manifest/device/console digests and no check verdicts. The
separate `run-named-live-validation.py` is a newline-MCP client that retains every request/response,
the target-observing dry-run plan, and the optional guarded apply record. It also content-addresses
the installed server and complete Remote Script file tree and retains stable before/after snapshots
of the operator-named Live log. Mutation remains behind an explicit flag and operator cleanup step.
These scripts orchestrate Infrastructure authorities;
they do not enter Core, infer an `.amxd`, promote a standalone database to M4L evidence, or decide
that audio was heard.

Residual Max facts now follow the same dependency direction as max-test. The staged
`index-max-host-run.py` copies observation provenance and hashes the plan plus twelve shared raw
artifacts; an M4L index conditionally requires a named-host-saved `.amxd`, exact assertion map,
64-assertion projection, six-scenario result, and console. It never calculates an outcome.
`sunny::infrastructure` requires the compiled exact plan and assertion-map digests, re-hashes the
complete index and every transitive file, parses the structured teardown, scheduler, MIDI, render,
assertion, and conditional Live-scenario facts, decodes both IEEE-float WAVE files, reconstructs
timing impulse offsets, and derives the five artifact facts, thirteen shared M4L checks, four
residual facts, and M4L discontinuity fact. The scenario result must cite the marked assertion run.
`apply-host-run` composes that authority with a provenance-identical
observation; `verify-host-run` repeats the complete evaluation without changing one. This closes
interpretation and replay, not production or authentication of the host measurements, and it
cannot supply the saved/frozen `.amxd` or any assertion that still requires the named device context.

The official database handoff follows the same dependency direction. A staged standard-library
Python script opens one quiescent SQLite file read-only, verifies its required columns, and projects
one explicit positive test ID into a closed raw-row JSON document with a database SHA-256. It does
not map assertions to Sunny checks. `sunny::infrastructure` validates that result, requires the exact
compiled manifest-file digest, re-hashes the database below the evidence root, requires the exact
64-assertion set, and updates only the 14 mapped checks. `sunny-max-evidence apply-max-test` is the
CLI composition root. Repeated check references to the same database share one materialization hash,
so the content address denotes one captured file rather than 14 sequential reads.

Release aggregation remains in the same native Infrastructure dependency direction.
`max-release-matrix.json` is a compiled-digest, closed six-cell manifest over the three package
targets and two host kinds. `sunny-max-evidence assemble-matrix` confines and hashes the six
deterministic record paths, strictly parses and embeds each record, and derives release completeness
only when every cell is complete and identity-correct. All records share one Sunny/package version
and source revision; standalone and Max for Live cells for the same target must bind identical
archive and binary hashes. `verify-matrix` re-reads those record files. It intentionally does not
re-read each record's package/evidence trees: those remain separate per-record `verify` operations,
and scheduler/audio settings remain conditions of the individual environment rather than hidden
matrix dimensions.

The supported-platform CI composition roots use one shared staged-package validator rather than
duplicated shell spot checks. That gate closes the five binary paths and the authored
manifest/readme/help/reference/patcher/validation sets, including the pinned harness and
release-matrix manifests, with byte equality for copied content. It is a build-artifact topology
adapter, not another runtime model and not evidence that Max loaded a binary.
The `sunny_max_archive` target is the sole Max distribution composition step: it places the exact
stage below `Sunny/`, emits one named ZIP and SHA-256 sidecar, and feeds both back through that same
validator for set/byte equality. CI transport may wrap those two files, but does not define their
inner bytes.

The LOM calls are public, but the Python Control Surface framework that hosts the TCP adapter is a
version-coupled private ABI. Its outside-Live test fallback is forbidden when a Live module is
present, because a synchronous fallback scheduler would violate the main-thread contract. See
`docs/ableton-max-conformance.md` for the evidence matrix and the distinct Max/MSP, Max for Live,
Max SDK, and RNBO deployment boundaries.

The closed two-field `remote_script/Sunny/bridge_contract.json` is the version authority. CMake
validates it and generates the installed C++ header; the self-contained Remote Script loads the
same manifest. Configure-time metadata validation also rejects current-facing reference examples
whose literal protocol/schema version diverges from that authority, and rejects a Python or Max
package version that diverges from the root CMake project version. Historical protocol numbers in
the decision record remain intentional history. CMake generates the installed `sunny/version.hpp`
from that same root version, and the native extension and validation-record parser consume it
instead of owning another literal. Configure-time source scanning also rejects stale
natural-language bridge-protocol literals in current native infrastructure. Python integration
then feeds the
adapter's actual serialized snapshot through the native parser, preventing separately green peer
suites from concealing a schema mismatch. A runtime-aware stub gate removes scikit-build's editable
import redirect, requires the extension from the current CMake build, and compares that object
graph with `stubs/sunny_native/__init__.pyi`; a stale installed binary cannot satisfy the gate.

Bridge protocol v43 retains v4's distinction between acceptance and state evidence. Generic property
sets return an exact three-member object containing the property, requested value, and immediate
observed value. Named DeviceParameter writes return an exact thirteen-member object containing
identity, selected internal/display domain, actual internal range, quantisation, the conditional
continuous-default or quantized-label domain, active/automation state, and enabled state/readback
metadata. The read-only parameter observer returns the same target facts in twelve members
without the setter's requested-value echo and without changing or repairing the target. Score,
Timbre, and Mix compilers preserve these records; a production transport
cannot substitute an empty, missing, or extended acknowledgement. Equal readback on an inactive or
automated parameter remains an explicit completeness warning rather than an audible/durable-state
claim.
Protocol v43 also replaces raw Song collection gets with explicit scene-count and Return-count
calls. The peer returns only a bounded non-negative integer and recursively serializes only exact
JSON-safe values; unsupported private objects, non-string keys, non-finite numbers, and out-of-wire
integers fail instead of being coerced to strings. This keeps collection cardinality tractable
without treating an unpublished Live Python object representation as target evidence. The same
boundary rejects category-confused profile, snapshot, cue, Device, and DeviceParameter values
before serialization rather than making them conform with scalar constructors.
The generic evidence parser also preserves LOM scalar categories: integer properties and their
request echoes must remain integral, floating properties must remain floating and finite, and
Boolean/string values cannot cross categories. Numeric tolerance applies only within the floating
category; it cannot turn a fractional meter or index into verified integer state.
Timbre source insertion separately retains the requested append index and requested public Device
type, then returns a closed count/index delta plus Device display/runtime identity, observed public
type/activity, exact Boolean Rack-chain capability, sample/millisecond latency reports, and the
Track's post-insertion
audio/MIDI-output classification.
Only an active non-Rack instrument of the requested display class on a Track that Live classifies
as having audio rather than MIDI output increments `devices_verified`; insertion acknowledgement
remains the narrower `devices_created`.
Protocol v43 also requires its version on every request and response and admits only the documented
Sunny operation/path/argument algebra; the Remote Script is not an arbitrary LOM reflection proxy.
It retains v4's target-device-count read before mapped Mix insertion, so the
compiler derives the exact appended device address instead of assuming an empty chain. Protocol v43
normalises snapshots, counts, and `devices/N` path traversal through one mixer-excluding insertable
chain view because the public Track reference says raw `Track.devices` includes the mixer. Mix results
publish total scalar sources, explicit/missing mappings, non-parameter residuals, deployed writes,
and verified observations as distinct properties. Its structural-snapshot call is read-only and
uses documented Song, Scene, Track, ClipSlot, Clip, Device, MixerDevice, DeviceParameter, and
CuePoint properties; schema 34
retains every Scene's pending-launch predicate and override state, Clip
signature/marker/loop/activator state, device display/runtime
identity, type, activity, Rack-chain capability, and reported latency in samples/milliseconds.
Normal Track records additionally retain exact Back-to-Arrangement state and bounded fired/playing
Session-slot indices. Generated Track verification requires false/-1/-1, while generated Clip
verification compares the complete occupied-slot set with exactly `{0}`.
For Live 11+, normal Tracks also retain the exact cardinality of the distinct
`arrangement_clips` child list. A generated Part Track requires zero; an earlier modeled target
uses null and cannot verify that obligation. The count closes the empty-Arrangement proposition
without widening the wire to private Clip objects, while a nonzero count remains observed,
non-destructively incomplete target state.
The same version branch retains `take_lanes` cardinality. Generated Part Tracks require zero
because the public LOM exposes the lanes but not Ableton's documented Audition Mode, under which
one lane becomes audible instead of the main lane. Zero lanes closes that hidden playback-source
branch without serializing TakeLane objects or inventing an audition-state observation.
Each normal Track also retains the complete ordered ClipSlot matrix: exact occupancy and Stop
Button presence, Group/control predicates, playing/recording/triggered predicates, integer
playing status, and will-record-on-start state. The snapshot requires exact slot cardinality/order,
exact agreement between slot occupancy and the separate Clip records, and Live's documented
playing/recording/status and non-Group relations. Generated Part gates require every slot to be a
non-Group, idle, non-recording/non-will-record, non-triggered cell; Stop Button presence is retained
without becoming source intent. This makes later empty-cell actions visible while remaining a
sequential read-only observation rather than a stop, transaction, playback trace, or sonic proof.
Normal and Return Track records also retain the exact selected output-routing type/channel
dictionaries and each corresponding one-key available-routing collection. Every selected and
available option is closed over string display and identifier symbols, and the selected full
dictionary must occur in its advertised array. Equality therefore detects selection, option-set,
and ordering changes. Postconditions pair both selected and available symbols with the requested
Sunny `Master` or `Group(id)` edge and expose separate type/channel/set membership verdicts. The
reads are sequential rather than atomic, so a selection absent from the collection fails the
snapshot instead of producing internally contradictory evidence. No portable semantic identifier
is inferred. With no explicit binding, identity mapping and output-route verification remain false
and the Mix compiler issues zero routing writes. A named-target binding can conditionally admit a
materialisable Master edge; the later deployment section defines its two-stage proof boundary.
Normal Track records independently retain exact Boolean audio/MIDI-input classification. When the
Track is input-capable, they also retain exact selected input-routing type/channel dictionaries and
their exact one-key available-option wrappers, with both selected full dictionaries required to be
members. When neither input class is true, all four routing fields are explicit nulls; Return and
Main records omit the input surface. Generated Part gates require audio input false, MIDI input
true, and both membership checks. These facts close only the advertised input class and
Live-valid-selection propositions: source identity and external-input neutrality remain false,
input routing is never mutated, and monitoring/event/signal behavior remains outside the snapshot.
The same audio/MIDI conditional now closes exact floating `input_meter_level` and
`output_meter_level` in `[0,1]`; other normal Track kinds carry two nulls. Generated Part gates
require exact zero for both documented one-second hold peaks. Their independent/combined
quiescence verdicts expose recent metered activity without promoting a pair of sequential point
reads into continuous input/output silence, routing, interface, acoustic, or sonic evidence.
Track-like records retain mixer volume/panning/send internal and
display values, finite internal bounds, quantisation, parameter enablement/state/automation state,
and the LOM's conditional parameter domain: finite in-range `default_value` for a continuous
parameter, or an exact opaque string `value_items` vector for a quantised parameter,
an exact Track Activator
parameter record, and the public integer crossfade assignment
for normal/Return Tracks; the Master carries `null` because that property is unavailable there.
Every mixer also retains public integer panning mode 0/1.
The exact parser requires floating, finite, ordered internal bounds and an internal value inside
them. Aggregate evidence treats Track Activator as quantised and volume/pan/sends as unquantised,
following Ableton's switch-versus-continuous control model. Target-specific bound constants and
localized quantised labels are observed rather than invented. Their order and text participate in
guarded snapshot equality, but no label is assigned enum identity or a numeric value by Sunny.
Return Track records additionally retain exact Boolean mute, solo, and derived solo-mute state.
Normal Track records also retain
audio/MIDI-input and audio/MIDI-output classification, exact Boolean freeze state, nullable Group
Track parent, mute,
solo, and derived solo-mute state, while volatile playhead and UI selection state are excluded from
the plan precondition.
Song records retain exact Boolean `is_playing`, `is_counting_in`, `arrangement_overdub`,
`overdub`, `record_mode`, `session_record`, and `session_automation_record`. Aggregate Song evidence
requires every recording/count-in mode false but reports transport stop independently, allowing a
non-recording Live Set to keep playing. The current public `session_record_status` integer is not
interpreted because its value mapping is unspecified. No transport or recording control is
mutated.
The same Song record retains exact Boolean Link, Link start/stop sync, Tempo Follower, and both
tempo-nudge controls. Aggregate verification requires all five inactive through
`public_tempo_controls_quiescent_verified`, without mutating them. This closes the tractable public
control tuple only: the public Song surface does not expose incoming MIDI-sync enablement or the
tempo automation envelope. Those observation flags and `tempo_stability_verified` therefore remain
false even when current tempo equality and public-control quiescence both hold.
Two additional exact Song predicates close current stored-versus-effective divergence:
`back_to_arranger` and `re_enable_automation_enabled`. Aggregate Song verification requires both
false through `arrangement_playback_aligned_verified` and
`automation_overrides_quiescent_verified`. The bridge does not invoke either corresponding repair;
it records a sequential current-state condition rather than changing playback source or automation.
Song records additionally require exact false Arrangement Loop and metronome predicates through
`arrangement_loop_disabled_verified` and `metronome_disabled_verified`. This excludes active
Arrangement repetition and Live-generated click output without mutating either control. The loop
brace's start/length are dormant under the false loop invariant and remain outside schema 34.
Schema 34 also retains the complete version-available Song scale tuple and every documented
TuningSystem property. The note-range, reference-pitch, and note-tunings dictionaries are retained
as exact opaque JSON, with only their documented outer shapes validated. It deliberately excludes
semantic interpretation or Score mapping of those dictionaries, per-track Bypass Tuning, and
instrument/MPE behavior. It is an optimistic sequential
observation, not a host transaction or lock. Its scene count must agree with every observed track's
clip-slot count; Score compilation uses the same scene count and creates scene 0 through the public
Song API before addressing Session slot 0 when necessary.
Score schema 8 nevertheless closes source-side pitch intent independently: `ScoreTuning` stores a
reference note/frequency and a complete 128-index cent function. Ableton compilation retains that
exact function as requested evidence and reports zero tuning definitions written. Thus bounded
target observation, unsupported target mutation, and complete source intent remain three distinct
facts rather than one overloaded “tuning supported” Boolean. The guarded plan's canonical project
state includes the current schema-8 Score JSON, so any source tuning or structured harmony change
invalidates the one-shot plan.

Structured harmony also has one core spelling authority: `derive_chord_numeral_root` maps the
closed numeral/key algebra to an exact letter and accidental. S25, the compact MusicXML reader,
and Corpus ingestion call that function rather than maintaining target-specific derivations. The
compact interchange boundary retains one harmony chord and exact measure-local point; broader
MusicXML stacking, fractional alteration, frame, and multi-staff semantics remain fail-closed.
After a successful executing apply, the same exact snapshot is also interpreted as project-level
postcondition evidence. Every Score Part retains requested Track name/mute/solo and whether the Mix
model expects its mixer gate enabled. Verification requires the final Track identity, audio-output
rather than MIDI-output classification, exact own mute/solo state, both public arm states false,
an unfrozen and observed ungrouped Track, and—when the model expects that
channel enabled—`muted_via_solo = false`. An offline plan retains those requests with null
observations. Generated Part Track verification also requires final `crossfade_assign = 1`, the
public “neither A nor B” value, so the Main crossfader cannot attenuate that Track at its gain
stage, and final `panning_mode = 0`, so the single `panning.value` is interpreted as Stereo Pan
rather than replaced by independent left/right Split Stereo controls. This detects a pre-existing unrelated solo that still gates a generated Part without
mutating that unrelated Track or claiming rendered audio.
Group observation is explicit because a null parent and an absent observation are different facts.
Any final parent index makes the Part gate incomplete: Ableton Group Tracks are summing submixes
with their own mixer/effects and commonly become the contained Track's output. This does not verify
the parent, output routing, or Sunny GroupBus materialisation.
Output-route intent is loss-accounted independently from that group observation. The default
compiler retains every Channel, Group, and Aux edge as a residual because Live's routing
dictionaries are target-owned and have no portable Main/Group semantic identifier. Protocol v43
allows an explicit named-target binding—with exact type/channel dictionaries and non-empty
provenance—only for materialisable Channel/Aux-to-Master edges. Deployment journals a type mutation
and then a channel mutation, re-enumerating the type-dependent channel set and requiring exact
membership/readback at both boundaries. Group destinations remain residuals, recording plans never
claim verification, and a failed second stage exposes a possible partial mutation. Final evidence
is true only under the retained semantic admission; it is not an independent proof of named-Live
behavior, future routing, Main hardware output, signal, or sound.
The compiler sets and immediately reads back `arm = false` and `implicit_arm = false`; final
evidence exports separate `disarmed_verified` and `implicitly_disarmed_verified` verdicts. This
excludes record-arming and Push's second public arm state, not all input monitoring. The current
public Track LOM does not expose the monitoring selector that Ableton documents as capable of
suppressing clip output in Monitor In. Track evidence therefore exports false
`monitoring_state_observed` and `clip_output_not_suppressed_by_monitoring_verified`; an unfrozen
verified structural gate is not a playback verdict. Every generated Aux Return is separately
bound to its appended index, receives exact false mute/solo, neutral crossfade assignment, Stereo
Pan mode, Track Activator 1.0, and its modeled return pan, and is rechecked by final aggregate
evidence. That gate also requires the final Aux name, an enabled/active/unautomated activator, and
`muted_via_solo = false`; an unrelated Live solo therefore remains observable rather than silently
suppressing the wet path. Each generated Part Track similarly lowers Boolean mute intent to exact
floating activator 0.0/1.0. The source MasterBus has no mute or pan intent, so Mix compilation
projects the only neutral Live Main state: activator 1.0, Stereo Pan mode 0, and pan 0.0. A separate
Main gate requires enabled, active, unautomated activator/pan state and the neutral mode/value.
These gain-stage closures still prove neither target-owned routing, signal, nor sound.
Every planned native insertion is also re-evaluated against the snapshot's mixer-excluding Device
chain. Verification requires the exact final chain cardinality plus requested index, display
identity, public Device type, activity, false `can_have_chains`, and the bounded public latency
pair. The false Rack predicate is necessary because Sunny's device intent is a flat serial chain,
whereas an Ableton Rack can own parallel/nested chains, selector zones, and independent chain
mixers. Thus one matching device cannot hide an extra/missing processor or a later
identity/type/activity/Rack-shape/latency-report change. This remains structural state evidence;
`reported_latency_observed` does not become a
chain sum, and `render_path_latency_fully_observed` remains false because compensation/monitoring
modes, Track Delay, routing, buffers, drivers, external devices, and physical output are absent. Final
parameter values, routing, presets/resources, signal production, and sonic equivalence are
independent obligations.
Generated slot-0 Session Clips receive the same temporal treatment. Their final name, exact
audio-false/MIDI-true/Arrangement-false identity, meter, markers, loop flag, activator,
version-coupled launch/groove association, and `length` are compared with compiler-retained intent.
Live 11+ additionally requires the public Session predicate true and Take-Lane predicate false.
For the explicitly unlooped state, the public Clip contract defines length as end marker minus start
marker and `end_time` as the End Marker; both derived values must independently agree with intent.
The independently stored loop brace remains unobserved and dormant under `looping = false`;
a future looped compiler must model it. Live 11+ compilation clears `Clip.groove` to the null object with immediate readback, and
snapshot schema 34 requires `has_groove = false` for the generated clips. Groove absence closes that
one non-destructive timing/velocity modifier. Live 11+ compilation also sets Trigger launch mode,
no clip launch quantization, Legato off, and zero launch-velocity scaling with immediate readback;
the final snapshot must match. Aggregate evidence calls this bounded scalar equality
`launch_behavior_verified`. Because the public Clip LOM exposes no Follow Action state and Sunny
does not execute playback, it separately reports `follow_actions_observed = false` and
`one_shot_playback_verified = false`; none of these scalars measures rendered timing or velocity.
Schema 34 also requires exact Boolean `is_playing`, `is_recording`, `is_overdubbing`,
`is_triggered`, and `will_record_on_start` on every occupied Clip. Generated-Clip evidence retains
all five observations and verifies only when the Clip is neither active/queued nor currently or
prospectively recording. No stop mutation is issued. Because the fields are read sequentially,
`runtime_state_observed`, `recording_quiescence_verified`, and `playback_idle_verified` describe
the observation point rather than an atomic state machine or later stability.
The public Clip LOM also exposes no bank/sub-bank/program launch state, so generated-Clip evidence
keeps `midi_bank_program_state_observed` and `program_change_suppression_verified` false rather
than treating final Device/parameter equality as proof against launch-time preset selection.
Every fresh generated Clip also receives the public all-envelope clear before note insertion.
Protocol v43 requires an exact immediate Boolean `has_envelopes` response, and snapshot schema 34
requires final `has_envelopes = false`. This closes the public Clip-envelope absence proposition
because those envelopes can otherwise automate/modulate controls or carry MIDI controller data;
it neither authors automation nor proves MPE expression, non-Clip modulation, or rendered sound.
Per-note MPE remains a separate public-LOM residual. Ableton documents Pitch, Slide, and Pressure
expression envelopes owned by individual notes, while the current public note dictionaries expose
no curves or expression-clear operation. Aggregate Clip evidence therefore retains false
`mpe_note_expression_state_observed` and `mpe_note_expression_neutrality_verified` instead of
promoting Clip-envelope absence or nine-field ordinary-note equality into an MPE-neutrality claim.
Snapshot schema 34 does not contain notes, so this does not promote insertion-time complete
note readback by itself. Each insertion-time note deployment retains both the exact requested
eight-field Live tuples and observed nine-field tuples (including note ID), so its verdict is
externally reconstructable. After the snapshot, an executing aggregate apply uses that same intent
for a second complete-note query and requires both the exact created-ID set and requested property
multiset. These read-only response-dependent observations are protocol-validated but excluded from
the dry-run mutation sequence and mutation journal.
Global state is likewise interpreted rather than merely returned: final Song tempo/meter and the
Scene-0 row name/disabled tempo-meter overrides must match the Score compiler's stored intent.
The complete ordered Scene `is_triggered` vector must also be observed and false. This excludes a
documented pending Scene launch only at the sequential observation point; it neither fires/stops a
Scene nor proves atomicity with Track/Clip state, launch completion, future stability, or sound.
Each projected top-level section must have one unambiguous final CuePoint with matching time/name;
this does not make Live CuePoints capable of retaining Sunny's nested SectionMap hierarchy.
The final mixer projection collapses Score then Mix property deployments by exact parameter
path/property, so the last real phase owns the postcondition. Snapshot equality requires the
selected internal/display value, `is_enabled = true`, and `state = automation_state = 0`; an equal
value on a disabled, inactive, or automation-controlled parameter does not verify. Output routing,
pre/post send mode, signal, and
sound remain outside this scalar proof. Accordingly, Mix reports level requests/writes separately
from pre/post-mode requests/writes, and a level-only projection does not increment the complete
`sends_configured` counter.
Arbitrary native-device mappings use a separate selective postcondition because snapshot schema 34
does not enumerate every potentially large `Device.parameters` collection. Every immediate Timbre
and Mix deployment is self-contained and action-labelled. After the final structural snapshot has
verified the containing device, protocol v43 calls the read-only exact-name parameter observer and
compares final identity/value/enabled/active/automation facts; internal-value mappings also compare
Live's final `min`/`max`, while display-value mappings retain those internal bounds without claiming
they equal the GUI mapping interval. These observations occur after the snapshot and complete-note
queries in a deterministic sequence, but do not form a transaction or repair target divergence.

Before any Live operation, the Mix compiler also solves the typed relative-fader dependency graph.
Channel/group edges are deterministic additive-dB constraints; invalid references, cycles, and
out-of-domain results fail preflight. LUFS targets are deliberately outside that static algebra and
remain measured-audio residuals with explicit fallback values. This keeps the core C++ model—not
the Python bridge or target defaults—in control of tractable level semantics.
The same rule now applies to signal routing. Redundant channel/group and child/parent graph fields
must be exact mirrors under X10. Every coherent Channel, Group, and Aux output edge is counted and
retained. Without explicit bindings the Live compiler writes/verifies zero: target-owned routing
dictionaries and the public track-creation contract supply no Master-routing postcondition.
Both project entry points and the Mix compiler accept exact Part/Aux-to-Master dictionary bindings
with mapping provenance, validate target membership, and write/read back type then channel.
Unbound and unmaterialisable Group edges remain residuals. Audio-output classification is never
substituted for destination proof.
The group-authoring workflows are closed under X10: they reject duplicate member input without
mutation and remove all stale/duplicate reverse channel or nested-group edges during moves.
Nested-group and return-to-Master operations validate a candidate graph before commit, so X2 cycle
and X3b depth failures are transactional. This makes a successful workflow result a coherent graph
state while keeping X10 as the trust-boundary check for imported or directly assembled documents.
X11 additionally closes Aux-send identity: Channel and Group send vectors have finite values and
at most one record per source/AuxBus pair, while X6 checks every target. The Live compiler counts
enabled GroupBus sends as requested but cannot configure them while Group Tracks remain
unmaterialised.

## Verification boundaries

- Catch2 tests cover pure domain behaviour, adapters, the real MCP dispatch route, and loopback TCP
  restart/reconnect behaviour.
- Python tests cover the facade and Remote Script contract with Live 11/12-shaped test doubles. A
  separate `mypy.stubtest` gate compares the current compiled native extension with its authored
  public stub, with a narrow allowlist only for pybind11 metaclasses, inferred enum `__members__`,
  and generic non-constructible result/view initializers.
- Render tests prove the per-call algebra, checked scheduling, and seed/state contract in
  `docs/formal/sunny-render-model.md`. They also prove bounded all-or-none LFO/ADSR recurrence
  vectors, constant held-value vectors, and local quarter-note-position vectors against a validated
  sample-rate/maximum-vector context.
  The API shape is backpropagated from Max `dsp64`/`perform64`. The `sunny_max_adapter` target now
  proves bounded vector-edge transfer: all modulation and local-clock control publishers serialize outside audio into a
  fixed SPSC queue, and the exclusive audio owner drains at most its 64-entry capacity without
  locks or allocation. Invalid callback topology/storage is rejected before the queue is drained,
  and candidate setup checks every pending frequency. The opt-in Max package supplies actual SDK
  callback registration and exact signed callback forwarding for `sunny.lfo~`, `sunny.adsr~`,
  `sunny.hold~`, and `sunny.clock~`, plus permanent-ITM registration for `sunny.events`. These tests
  still do not prove honest host values, ABI loading,
  scheduler arrangement, buffer provenance, or deadline behavior. `BlockClock` isolates an O(1),
  callback-free recurrence; its signal projection emits start-of-sample local quarter-note position
  and commits the same endpoint.
  Transport composes it with event storage and remains unsuitable for `perform64` because queue
  draining and callbacks are not bounded real-time work. Host-shaped modulation validates signed
  frame count, exact zero-input/one-output generator topology, and non-empty output pointers before
  span construction, so that trust ordering is enforceable without a Max SDK dependency.
- `BlockEventScheduler` supplies the distinct bounded event-planning primitive: 256 inline events,
  caller-owned output, containing-sample offsets over a half-open vector interval, and fail-closed
  queue/output capacity. It performs no allocation or callback, but has no cross-thread schedule
  transfer or Max consumer; those remain downstream architecture rather than inferred properties
  of the source planner.
- `ItmEventAdapter` supplies the separately bounded Max scheduler path: arbitrary publishers are
  serialized into 64 commands; one scheduler owner manages 256 event cells. Status separates
  pending commands, reservations shared by pending/retained work, and actually scheduler-retained
  events. Strict future-time, checked sampled-relative target derivation, and adjacent-tick
  injectivity are checked before fixed-slot mutation;
  host schedule/cancel/stop actions are mandatory for the corresponding mutation, so null
  callbacks cannot manufacture host-effect counters. Permanent callbacks group equal ticks before
  one-shot list output. `sunny.events` owns one paired SDK reference to the unnamed global ITM for
  its complete usable lifetime and binds those slots to it without mutating it. `transport_status`
  exposes the sampled ITM tick, resolution, running state, and SDK name as a
  non-transactional observation for host harnesses. This is not a
  `perform64` path and does not
  consume BlockEventScheduler offsets. SDK shape is tractable; actual Live synchronization,
  scheduler priority, downstream sample accuracy, discontinuities, and deadlines remain host
  evidence.
- `BlockClock` and `sunny.clock~` remain local source recurrences, not Live clock replicas. Max for
  Live clock-source selection, named transport isolation, preview discontinuities, and tempo-change
  position limitations require an actual device path and host evidence.
- `SignalBlockContext` is immutable setup metadata rather than lifecycle ownership. Replacing it
  after a DSP-chain rebuild retains render state unless the adapter deliberately invokes an
  explicit reset operation. An LFO adapter validates retained frequency against a candidate
  context before admission; rejection invalidates processing/configured state but preserves the
  recurrence and pending controls, preventing a new host chain from using stale setup metadata.
  Runtime processing repeats the compatibility guard. The context-aware frequency setter supplies
  the symmetric candidate-control gate, and the Max adapter publishes it through the bounded
  transfer without concurrent processor mutation. Every SDK wrapper registers perform on every
  rebuild and uses a shared current-maximum silence validator when that rebuild is unadmitted.
- The actual Ableton process remains an external integration boundary; loopback tests prove the
  protocol and scheduling contract, not a particular installed Live version.
- CI builds and uploads the staged native Max package on macOS and Windows and separately compiles
  the actual wrappers against pinned headers on Linux. A successful CI run is platform build/link
  evidence, not Max loading, callback scheduling, deadline, Max for Live, or audible evidence.

The standard local gates are:

```bash
cmake --preset release
cmake --build --preset release
ctest --preset release
make package-check
make max-sdk-header-check
PYTHONPATH=python:remote_script:.bin/python uv run pytest tests/python -q
uv build
uv run ruff format --check .
uv run ruff check .
uv run mypy python/sunny --ignore-missing-imports
```

`package-check` installs Sunny and any dependencies fetched on its behalf into an isolated prefix,
then configures, links, and runs a separate `find_package(Sunny)` consumer. This keeps the installed
API and transitive link interface aligned with the in-tree targets. `uv build` creates the source
distribution and rebuilds a platform wheel from it; the wheel contains both the `sunny` facade and
the `sunny_native` extension.
