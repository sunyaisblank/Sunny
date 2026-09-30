# Sunny named-host validation handoff

These files prepare evidence collection; they do not contain Max, Max for Live, or Ableton Live
execution evidence. A complete record still requires the named commercial host and exact retained
artifacts described below.

## 1. Freeze one native target

Use one immutable Sunny source revision for all six cells. Build `sunny-max-evidence` from that
revision, then build the Max archive on the target OS/architecture. Keep the emitted ZIP and its
`.sha256` sidecar together and extract the ZIP's outer `Sunny/` directory without modifying it.
Standalone Max and Max for Live on the same native target must use the same ZIP and extracted
binaries.

Before launching a host, create a cell workspace. The command verifies the ZIP sidecar and exact
ZIP/extracted-tree byte equality, fills only observed environment/provenance fields, creates the
closed evidence directories, and emits exact post-host command arrays in `operator-inputs.json`.
It refuses to overwrite an existing cell and leaves every applicable check `not_run`.

```sh
python3 Sunny/misc/validation/prepare-max-host-run.py \
  --cell macos-arm64-standalone-max \
  --archive /retained/Sunny-0.4.0-macos-arm64.zip \
  --package-root '/Users/operator/Documents/Max 9/Packages/Sunny' \
  --output-root /retained/sunny-max-release \
  --source-revision 0123456789abcdef0123456789abcdef01234567 \
  --observed-at-utc 2026-09-01T04:00:00Z \
  --max-version 9.0.5 \
  --audio-driver 'Core Audio / Named Interface'
```

For a Max for Live cell add exact `--live-version` and `--max-for-live-version` values. If the
actual sample rate, vector sizes, Overdrive, or Scheduler in Audio Interrupt differ, supply the
corresponding flags; do not leave baseline values merely because they are preferred.

## 2. Install and verify the pinned max-test runner

The standalone harness is closed to Cycling '74 `max-test` revision
`8c5d833d4b1e454238ced7c866c34acedb69cc07`, package 1.2.1, and its
`max-sdk-base` submodule revision `b6d635cc69bac680c35a63fcfebba9d523ab0d6c`. A package-manager
installation at an unverified revision is not the pinned runner.

Clone the repository into the named Max Packages directory, check out the exact revision, update
submodules, verify both revisions, and build it with the supported host toolchain:

```sh
git clone https://github.com/Cycling74/max-test.git max-test
git -C max-test checkout 8c5d833d4b1e454238ced7c866c34acedb69cc07
git -C max-test submodule update --init --recursive
git -C max-test rev-parse HEAD
git -C max-test/source/max-sdk-base rev-parse HEAD
cmake -S max-test -B max-test/build
cmake --build max-test/build --config Release
```

Create `max-test/misc/max-test-config.json` with these exact bytes apart from the final platform
newline:

```json
{
  "port-send": 4792,
  "port-listen": 4791
}
```

Retain the two revision outputs, build transcript, configuration, and exact installed tree. Launch
the pinned Ruby runner from `max-test/ruby` and capture its complete console output. Examples:

```sh
ruby test.rb "/Applications/Max.app" arm64
ruby test.rb "C:\\Program Files\\Cycling '74\\Max 9"
```

At the pinned revision a second argument both selects the macOS architecture and suppresses the
script's explicit Ruby `exit`. After it prints the final pass/fail summary and database path, Max
has quit and the script has waited for the database flush, but the Ruby wrapper may remain alive.
Retain that output and stop only that finished wrapper. Do not copy the database until Max and the
wrapper are closed and no `-journal`, `-wal`, or `-shm` sidecar exists.

Select the exact positive test ID for `sunny-runtime-smoke.maxtest`, then execute the
`extract_max_test` command from the generated `operator-inputs.json`, writing stdout to its
`normalized_result_output`. Execute `apply_max_test` and write stdout to a new temporary
observation before replacing the scaffold. This can update exactly 14 standalone checks. It cannot
update active teardown, scheduler timing, MIDI delivery, rendered audio, or any Max for Live fact;
the separate native residual path below updates only those four residual checks on either host
kind.

## 3. Residual Max checks

`max-host-run-plan.json` is the closed measurement plan. Use its exact baseline, counts, scenario
names, expected MIDI bytes, render configurations, tolerances, and required filenames. In
particular:

- teardown means 1,000 active-DSP destruction cycles for every maintained object plus retained
  crash/hang and platform memory diagnostics;
- scheduler timing means a 32-event `click~` capture at 120 BPM and 48-tick spacing under the fully
  enabled path, both disabled-setting controls, a late-target rejection, and an ordinary-message
  control. Only the fully enabled supported path has a zero-sample-spacing-error criterion;
- MIDI delivery means bytes received through an independently named loopback input/receiver, not
  bytes leaving `xnoteout`; and
- rendered audio means lossless floating-point PCM with at least 4,096 settled frames and the four
  exact value/slope comparisons in the plan.

Retain the raw artifacts, tool versions, commands, exit status, and machine-readable measurements.
Do not set these four outcomes by hand. Their closed structured inputs are:

- `cycle-counts.json`: `schema_version`, the five ordered `objects` with `object_name`,
  `attempted_cycles`, `constructed_cycles`, `dsp_active_cycles`, and `destroyed_cycles`, followed
  by `host_exit_code`, `crash_count`, `hang_count`, `invalid_access_count`, and
  `definitely_lost_bytes`;
- `scenario-settings.json`: the plan's tempo/tick/event values and five ordered scenarios, each
  with `name`, both scheduler booleans, `downstream`, `target_ticks`, `callback_count`,
  `target_rejected`, and `message_times_ms`; `impulse-offsets.json` binds the timing-WAVE digest
  and maps the first four ordered scenarios to channels 0–3 and their observed nonzero sample
  offsets;
- `ports.json`: `schema_version`, `output_port`, `input_port`, `receiver_name`,
  `receiver_version`, and `channel`; `received-midi.json`: `schema_version`,
  `source_callback_lists`, and `received_bytes`; and
- `render-settings.json`: `schema_version`, `format` (`wave_ieee_float`), `sample_rate`,
  `minimum_frames`, `settled_start_frame`, `settled_frame_count`, and four ordered channel objects
  (`object_name`, `channel_index`, `configuration`). `comparison.json` retains
  `schema_version`, comparator name/version, its argument array, exit code, and the rendered-WAVE
  digest; Sunny does not trust its verdict.

Both WAVE files must use IEEE float32 or float64. The timing capture has four channels in the order
above; the render has `sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`, and `sunny.clock~` on channels
0–3. After all twelve shared files exist, execute `index_host_run` from
`operator-inputs.json`, writing stdout to a new temporary file at `host_run_result_output`. For an
M4L cell, first add the three device-context files described below. Move the result
into place only after the indexer exits successfully. The indexer hashes files and copies exact
provenance but never derives outcomes. Then execute `apply_host_run` into a new temporary
observation, run `verify_host_run`, and only then replace the cell observation. Native C++ re-hashes
all applicable files, reconstructs timing offsets from the timing WAVE, compares the exact MIDI bytes,
and calculates all four rendered-channel errors. A complete measurement that misses a criterion is
retained as `failed`; a malformed, mismatched, or drifting evidence set is rejected.

The residual index is the evidence file referenced by all four facts and transitively binds every
supporting digest. Re-run `verify_host_run` immediately before record assembly and retain its
transcript. The record schema content-addresses the index but does not authenticate the operator or
diagnose a hostile measurement tool.

## 4. Max for Live cells

The pinned `.maxtest.maxpat` runner is a standalone-Max harness. Opening it in Max while Live is
running does not make it a Max for Live device-context observation. At the pinned upstream revision,
`test.assert` also ignores input unless the standalone runner has installed its private `#T` test
unit. Do not relabel its SQLite database as M4L evidence.

The M4L scaffold includes a deterministic source transformation instead. With the exact pinned
`max-test` package source available, run the `m4l_device_source.prepare` command from
`operator-inputs.json`. It verifies the upstream `CheckConsoleClear.maxpat` digest and the exact
`max-test-harness.json`, then creates a non-overwriting editable tree containing
`sunny-m4l-validation-device.maxpat`, four transformed assertion helpers, a console helper, and
`sunny.m4l.assert.js`. The script replaces runner-private `test.assert`, `test.terminate`, and
`test.log` participation; the smoke's pinned `test.sample~` and `test.equals` dependencies remain
and therefore require max-test 1.2.1. It also removes the standalone `1` to `dac~` DSP-start edge
and turns that sink into an unconnected `plugout~`: Live owns device DSP and the validation device
must produce silence. The source adds `live.thisdevice` and emits one marked protocol session with
the same 64 assertion names. Its `source-inventory.json` binds every input and generated file. This
editable `.maxpat` tree is preparation, never a device-context observation.

Create a blank Max for Live Audio Effect in the named Live build, replace its patcher content with
the generated main patcher, place the generated support files on its search path, and verify every
dependency resolves from the exact extracted Sunny package and pinned max-test package. Do not add
an input connection to the generated, unconnected `plugout~`; its role is to declare the device
audio output while keeping the validation device silent. Save and freeze the device to the
`m4l_device_source.save_from_named_host_to` path. Retain the complete Live/Max console and copy the
run ID printed between `SUNNY_M4L_ASSERTIONS_BEGIN` and `SUNNY_M4L_ASSERTIONS_END`. After host exit,
run the `export_m4l_assertions` command and write its stdout atomically to
`m4l_assertion_result_output`. The exporter accepts exactly one complete target session, all and
only the 64 expected names, and `Pass`/`Fail`; it hashes the pinned mapping, saved `.amxd`, and raw
console, but never derives check outcomes. The scaffolded `scenario-results.template.json` fixes
the six ordered result shapes with deliberately invalid placeholders; replace every placeholder
from the actual run and write the completed object to the evidence path rather than treating the
template as evidence.

For each M4L cell, use an Audio Effect device saved by the named Live/Max for Live build, instantiate
the exact package binaries in that device, and retain the device plus its console/assertion output.
Exercise every scenario in `max_for_live_transport_scenarios`: stop/start, both seek directions,
Arrangement loop, tempo change, preview entry/exit, and scheduler-setting changes. Pass requires no
duplicate one-shot, no cancelled/reassigned stale output, healthy callbacks, and zero final event
gauges. Preview/tempo observations must be retained as observed discontinuities; they are not
evidence that Max ticks equal an independently sampled Live playhead at every instant.

Retain that exact device as
`live-transport-discontinuities/sunny-validation-device.amxd`, the exact assertion mapping as
`max-test-harness.json`, the exported facts as `shared-assertions.json`, the console as
`host-console.txt`, and structured counts as `scenario-results.json`. The scenario result has
`schema_version`, a non-empty `live_set_name`, the saved device's SHA-256, the exact
`assertion_run_id`, and the six scenarios in plan order. Every scenario has `name`, `completed`,
`scheduled_event_count`, `expected_fire_count`,
`observed_fire_count`, `duplicate_fire_count`, `stale_output_count`, `callback_healthy`,
`final_reserved_slots`, `final_retained_events`, and a non-empty observed account of the actual
discontinuity. The native residual evaluator requires at least one scheduled event, exact expected
versus observed fire counts, zero duplicate/stale outputs, healthy callbacks, and zero final gauges
for all six scenarios. The assertion result has `schema_version`, exact harness/device/console
digests, the same run ID, and 64 ordered `{name, outcome}` entries. Native C++ re-parses the pinned
mapping, derives the five artifact facts and thirteen shared check outcomes, and requires the
scenario result to cite that assertion run before it derives the discontinuity outcome. These five
roles are required only for `max_for_live`; a standalone result containing them or an M4L result
omitting any of them is rejected.

The remaining authority is therefore the named-host-saved/frozen `.amxd` and its actual
device-context assertion, scenario, and console output. No repository-local command can create
those host facts, but every subsequent projection, mapping, and verdict is now deterministic.

## 5. Assemble and verify

After every applicable check in one observation has real evidence, execute `assemble_record` from
`operator-inputs.json`, write stdout to a new temporary record, execute `verify_record`, then move
the verified record to `record_output`. Never replace an earlier observation or record with a
failed command's empty/truncated stdout.

Once all six deterministic record paths exist, re-run each per-record verification and then:

```sh
sunny-max-evidence assemble-matrix \
  Sunny/misc/validation/max-release-matrix.json /retained/sunny-max-release \
  > /retained/sunny-max-release/release-matrix.tmp
sunny-max-evidence verify-matrix \
  /retained/sunny-max-release/release-matrix.tmp \
  Sunny/misc/validation/max-release-matrix.json /retained/sunny-max-release
mv /retained/sunny-max-release/release-matrix.tmp \
  /retained/sunny-max-release/release-matrix.json
```

## 6. Named Ableton Live plan/apply

Install the exact `remote_script/Sunny` directory into Live's configured third-party Remote Scripts
folder, select Sunny as the Control Surface, retain the copied tree/revision and Live log, and run
the helper against the installed `sunny-mcp`. Plan-only is the default:

```sh
python3 Sunny/misc/validation/run-named-live-validation.py \
  --server /installed/bin/sunny-mcp \
  --remote-script-root '/Users/operator/Music/Ableton/User Library/Remote Scripts/Sunny' \
  --live-log '/Users/operator/Library/Preferences/Ableton/Live 12.3.5/Log.txt' \
  --output-root /retained/sunny-live-plan \
  --live-edition Suite \
  --operating-system 'macOS 15.6' \
  --architecture arm64 \
  --remote-script-revision 0123456789abcdef
```

Review `dry-run-plan.json` and the exact pre-deployment snapshot before permitting mutation. Use a
new output directory and repeat with `--apply`, exact optional all-or-none Max facts, and at least
one operator-authored cleanup step:

```sh
python3 Sunny/misc/validation/run-named-live-validation.py \
  --server /installed/bin/sunny-mcp \
  --remote-script-root '/Users/operator/Music/Ableton/User Library/Remote Scripts/Sunny' \
  --live-log '/Users/operator/Library/Preferences/Ableton/Live 12.3.5/Log.txt' \
  --output-root /retained/sunny-live-apply \
  --live-edition Suite \
  --operating-system 'macOS 15.6' \
  --architecture arm64 \
  --remote-script-revision 0123456789abcdef \
  --max-version 9.0.5 \
  --max-for-live-version 9.0.5 \
  --license-state licensed \
  --cleanup-step 'Delete the Sunny Named Host Validation track after evidence retention.' \
  --apply
```

The helper content-addresses the exact server binary and every installed Remote Script tree file,
and retains stable before/after snapshots of the named Live log. The fixture creates one typed
note, requests the native `Analog` instrument, maps its exact
`Filter Freq` parameter, and requests one mixer level. The helper retains every JSON-RPC
request/response, decoded tool result, preflight plan, guarded apply, and returned strict
`AbletonValidationRecord`. That record owns the final structural snapshot and exact note,
device, parameter, and mixer readback. A non-null record is still not audible/rendered proof; retain
a Live render only with separately declared sample alignment, level, and comparison criteria.
