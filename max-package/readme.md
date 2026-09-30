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

## Building

From the Sunny checkout, on macOS or Windows:

```sh
cmake -S max-package -B .bin-max
cmake --build .bin-max --config Release --target sunny_max_archive
```

The archive target builds all five externals and writes
`Sunny-0.4.0-<platform>-<architecture>.zip` with a `.sha256` sidecar in `.bin-max/`. The ZIP holds
one outer `Sunny/` directory containing the staged package. Publication never replaces an existing
archive with different contents: a changed build must use a new build directory or `SUNNY_MAX_ARCHIVE`
path, and an identical rebuild succeeds without rewriting the files. The macOS and Windows CI jobs
build the package and check that the staged files and archive match the sources.

## Limitations

The render logic behind these externals is tested on Linux through the host-neutral C++ library,
and the wrappers are compiled against the pinned Max SDK headers. Loading the externals in Max,
real-time audio behaviour, and Max for Live use have not yet been exercised in a running host.
