# Sunny Max package

This opt-in package builds four traditional MSP generators and one Max scheduler object:

- `sunny.lfo~`: a zero-signal-input, one-signal-output LFO;
- `sunny.adsr~`: a zero-signal-input, one-signal-output ADSR envelope;
- `sunny.hold~`: a zero-signal-input, one-signal-output held message value;
- `sunny.clock~`: a zero-signal-input, one-signal-output local quarter-note position clock.
- `sunny.events`: a bounded one-shot MIDI-list scheduler tied to Max's unnamed global ITM
  transport.

The four MSP externals use Sunny's fixed-capacity control-transfer adapters. Max message producers are
serialized outside the audio callback; accepted controls are applied in queue order at the next
signal-vector boundary. The perform callback performs no allocation, locking, I/O, outlet calls,
or unbounded queue drain. Callback shape and output storage are checked before any command is
drained. The `status` message posts separate setup, control, and process counters to the Max
console together with the current pending-control gauge; `clear_error` clears the retained
last-error code without resetting those counters. The left outlet remains the sole signal outlet;
the right message outlet emits the closed tuple `host_status configured sample_rate maximum_frames
process_observed process_calls last_frame_count process_healthy last_error`. The exact signed
`process_calls` value saturates at its maximum and otherwise advances once per entered callback.
These values are lock-free source
observations, not proof that the host met an audio deadline or preserved a connected signal path.

Every external also treats its load-time class surface as all-or-none. A null `class_new`, any
failed documented `class_addmethod`, or failed public `class_register` prevents publication of the
global class pointer. `sunny.events` registers its hidden permanent-slot class before its public
class. Every signal generator separately requires its one signal outlet to be created before an
instance can succeed, and `sunny.events` owns one paired reference to the unnamed global ITM for
its complete usable lifetime.

The package requires CMake 3.28, a C++23 compiler, this Sunny source tree, and a supported macOS or
Windows Max SDK toolchain. The Max SDK base dependency is pinned in `CMakeLists.txt`. The package
compiles the authoritative adapter, modulation, and signal-context translation units inside the Max
toolchain so Windows cannot accidentally mix an ordinary Sunny `/MD` archive with the SDK's `/MT`
external runtime.
Configure with `MAX_SDK_PACKAGE_OUT_OF_TREE=ON` (forced by this project) so all output is staged under
`<build>/package`; the build never writes directly into a user's Max Packages directory.

`sunny.clock~` emits the local quarter-note position at the start of each sample. Its tempo,
integer-tick seek, play, record, pause, and stop commands take effect at vector boundaries. It is
not a Max transport, a `clocksource live` path, or evidence of Ableton song position.

`sunny.events` follows Max's distinct ITM permanent-event contract. Its arbitrary message
publishers are serialized into 64 command cells, while one Max scheduler consumer owns 256 fixed
event slots. `event tick pitch velocity [release_velocity]` schedules one event; `note tick pitch
duration_ticks velocity [release_velocity]` admits the note-on/off pair atomically; omitted release
velocity defaults to 64. `event_after delay_ticks ...` and `note_after delay_ticks ...` derive a
target from a fresh global-transport sample: delay zero means the first representable Sunny tick
strictly after that sample. This is not a time reservation; normal application rejects the command
if scheduler latency makes its target stale. Every fired event is the three-atom list `[pitch,
attack_velocity_or_zero, release_velocity]`; `clear` is ordered with surrounding commands.
Successful dispatch stops every permanent time object in the equal-tick group before its fixed
slot is released for reuse; insufficient callback storage stops and consumes nothing.
Ticks must be strictly later than the global transport position when applied and must map
injectively into ITM's double tick domain. The hidden permanent slots use Max's documented
transport-attribute default to select the global ITM object, and successful command publication
serializes its command-clock wake. In Max for Live the unnamed global transport follows Live, but
sample-accurate downstream behavior additionally requires Overdrive, Scheduler in Audio Interrupt,
timely high-priority scheduling, and a downstream object documented to support it. `sunny.events`
never starts, stops, seeks, renames, or changes the resolution of that transport.
Its `status` message separates commands still pending in the producer queue, event cells reserved
for both pending and applied work, and events actually retained by the scheduler. None of those
source-side gauges proves that Max accepted or delivered a permanent callback. The left outlet
remains the event-list outlet; the right message outlet emits `event_status command_observed
scheduled_observed fired_observed cancelled_observed callbacks_healthy reserved_events
retained_events last_error`.
`transport_status` emits `transport_status current_tick ticks_per_quarter running transport_name`
from the same lifetime-owned global ITM. This sequential public-SDK snapshot can expose
host facts to a harness, but it neither mutates the transport nor proves Live identity or delivery.

The `help/` patchers demonstrate every control and status family. The `sunny.events` patch uses an
explicit right-to-left trigger pipeline to convert its three-atom state into `xnoteout 1`: note-on
selects attack velocity and a nonzero flag; note-off selects release velocity and a zero flag. It
does not attach `midiout` or select a port. The `docs/` reference pages supply object, argument, and
message metadata to Max's Documentation Window and object autocomplete.

`patchers/sunny-runtime-smoke.maxtest.maxpat` is a self-starting standalone-Max test for the pinned
Cycling '74 `max-test` harness. The exact repository revision, SQLite result contract, helper
patchers, expected assertions, and mapping into Sunny's host-evidence vocabulary are closed by
`misc/validation/max-test-harness.json`. In addition to discovery, public surface, signal topology,
DSP, callback, finite-output, and control checks, the smoke uses documented `thispatcher` scripting
names to disconnect and reconnect every wrapper's signal sink. Exact process-call snapshots must
advance while disconnected and again after reconnection, and every reconnected sink must receive a
fresh expected sample. The smoke also drives the unnamed standalone-Max `transport` and advances
its assertions from actual `sunny.events` callbacks. It checks ITM
schedule/fire, equal-tick note-off-before-note-on order, note release formatting, clear/reassignment,
and standalone transport identity. Its 1500 ms `delay` is only a failure watchdog; successful test
ordering is callback-driven. The lifecycle phases use bounded 120 ms observation windows and a
2000 ms failure watchdog; process-count advancement and new sample callbacks—not elapsed time—are
the pass conditions. Four host checks and every Max for Live transport claim remain outside
this smoke. A green max-test database is therefore an input to, not a replacement for, a complete
named-host observation and its retained evidence.

`misc/validation/max-host-run-plan.json` closes the remaining operator measurements: exact
teardown-cycle counts and diagnostics, timing settings/scenarios/sample spacing, downstream MIDI
bytes and receiver identity, floating-point PCM comparisons, and six Live transport-discontinuity
scenarios. `prepare-max-host-run.py` verifies one archive/sidecar/extracted-package byte set and
creates a non-overwriting, evidence-empty workspace for one release cell. It fills provenance and
applicability only; it never marks a check passed. For M4L,
`prepare-m4l-device-source.py` verifies the pinned assertion/upstream-console inputs and creates an
inventoried editable Max Audio Effect tree with the standalone `dac~` DSP-start edge removed and a
silent unconnected `plugout~`; the named Live/Max build must still save and freeze the `.amxd`.
`export-m4l-assertions.py` projects exactly one 64-fact run-ID-delimited console session and hashes
its assertion map, device, and transcript without mapping checks. `index-max-host-run.py` hashes the
twelve shared residual artifacts and, for M4L only, the saved device/assertion map/assertion
projection/scenario/console set without deriving an outcome. Native `apply-host-run` and
`verify-host-run` re-hash that transitive set, decode the two float-WAVE captures, map the device
assertions to five artifacts and thirteen checks, and derive the four residual outcomes plus the
conditional M4L discontinuity outcome. `run-named-live-validation.py` separately
retains a deterministic Ableton JSON-RPC transcript and dry-run plan, and reaches guarded mutation
only with an explicit `--apply`. The exact procedure and remaining `.amxd` host-authority blocker
are documented in `misc/validation/README.md`.

After Max and the official runner have closed the database, select its exact positive `test_id` and
project only those raw rows with the staged extractor. It rejects SQLite journal/WAL state, missing
columns, unfinished tests, unknown outcomes, symlink leaves, and a database that changes during
extraction. The C++ tool then re-hashes the retained database, requires the compiled exact manifest
digest and all 64 assertions, and maps only the 14 declared checks:

```sh
python3 misc/validation/export-max-test-sqlite.py \
  misc/validation/max-test-harness.json evidence/max-test/results.db3 42 > max-test-result.tmp
sunny-max-evidence apply-max-test \
  misc/validation/max-test-harness.json max-test-result.tmp observation.json \
  evidence max-test/results.db3 > observation.tmp
mv observation.tmp observation.json
```

Keep the normalized temporary result until the resulting observation has been reviewed. Python only
extracts unchanged database rows and a digest; the native command owns schema validation, digest
agreement, assertion-set equality, pass/fail conjunctions, and observation mutation.

Residual host files use the exact roles in `max-host-run-plan.json`. Run the staged indexer to
produce `evidence/host-run-result.json`, then pass that file through `apply-host-run` and
`verify-host-run` before record assembly. The indexer owns no threshold or verdict; native C++
reconstructs scheduler offsets from the timing WAVE and calculates render errors directly. An M4L
index is invalid without the host-saved validation device, exact marked assertion session, and six
ordered discontinuity measurements bound to that run, but those inputs still require an actual
Live/Max for Live execution.

Example from the Sunny checkout:

```sh
cmake -S max-package -B .bin-max
cmake --build .bin-max --config Release --target sunny_max_archive
```

The archive target depends on all five externals and emits
`Sunny-0.4.0-<platform>-<architecture>.zip` plus its `.sha256` sidecar in `.bin-max/`. The ZIP owns
one outer `Sunny/` directory and the exact staged file bytes; the macOS and Windows CI jobs validate
that equality before upload.
Archive publication is immutable: the target builds and hashes a candidate pair before publishing,
serializes writers to the same destination, and preserves existing files on failure. ZIP timestamps
are fixed so identical inputs can repeat successfully without changing the published bytes. A
changed input or an incomplete existing pair is rejected; choose a new build directory or set
`SUNNY_MAX_ARCHIVE` to a new path. Remove an old pair only when deliberately discarding it.
The sibling `.lock` and temporary `.work` paths are build metadata, not host evidence.
Archive and work paths must be disjoint from the staged package. The target resolves existing
directory aliases and rejects overlaps before creating directories or lock files.
Lock and work paths must not be symlinks.

Building the binaries does not by itself prove Max-host execution, disconnected-outlet continuity,
audio deadline performance, Max for Live availability, or Ableton Live integration. Those claims
require the named-host validation record specified in `docs/ableton-max-conformance.md`.
The macOS and Windows CI jobs run the same exact staged-package validator: all five external
binaries, all five help/reference pairs, five max-test patchers, both validation examples, the
pinned harness manifest, release-matrix and host-run manifests, SQLite extractor, residual indexer,
both remaining operator helpers, validation handoff, package manifest, and readme are required.
That gate establishes
package topology and copied bytes only, not host loading.
`misc/validation/max-validation-record.example.json` is a canonical but deliberately incomplete
schema-1 parser handoff. For an actual run, copy
`misc/validation/max-validation-observation.example.json`, replace its provenance/environment and
host observations, and retain one relative evidence artefact for every passed or failed applicable
check. That observation deliberately has no handwritten digest or completion fields. Derive the
record from the exact distributable archive, extracted package, and evidence directory with the
installed C++ tool:

```sh
sunny-max-evidence assemble observation.json Sunny-package.zip package-root evidence-root > record.tmp
sunny-max-evidence verify record.tmp Sunny-package.zip package-root evidence-root
mv record.tmp record.json
```

Writing a temporary result first prevents a failed assembly from replacing an earlier record. The
thin installed Python `sunny_native.validate_max_validation_record_json` entry remains available for
strict schema validation, but it does not read or hash evidence files.

The derived `complete` member must remain false until every platform binary was discovered and
instantiated and every check applicable to that one recorded host configuration passed. One
complete record is not a release matrix and does not authenticate the operator or its evidence.
The staged `example_` harness, `replace-with-` values, and uniform hexadecimal sentinels also force
the derived verdict false; changing outcomes alone cannot promote the template into evidence.

For release coverage, place the six completed records at the exact paths declared by
`misc/validation/max-release-matrix.json` below a common record root, after separately running
`verify` against each record's retained archive, extracted package, and evidence tree. Then derive
and re-check the aggregate:

```sh
sunny-max-evidence assemble-matrix \
  misc/validation/max-release-matrix.json release-records > release-matrix.tmp
sunny-max-evidence verify-matrix \
  release-matrix.tmp misc/validation/max-release-matrix.json release-records
mv release-matrix.tmp release-matrix.json
```

The six cells are standalone Max and Max for Live on macOS x86_64, macOS arm64, and Windows x86_64.
All must identify one Sunny/package source revision; both host kinds for a target must bind the same
archive and binaries. The matrix proves only those six explicit record environments. It does not
re-verify underlying evidence, authenticate the operator, or cover another Overdrive, Scheduler in
Audio Interrupt, audio-driver, vector-size, Max, or Live configuration.
