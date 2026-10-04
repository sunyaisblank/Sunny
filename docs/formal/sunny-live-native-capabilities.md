# Sunny Native Live Parameter Capabilities

**Registry version:** 3\
**Status:** Source-candidate contracts; host qualification remains open\
**Scope:** finite native parameters, owning source/effect planning and managed revisions

## 1. Evidence and version boundary

The primary product target is Live 12.4, with compatible Live 12.3 behavior retained.
Exact patch, edition and operating system must be recorded during final host qualification;
the product target does not promote a public Max API reference to a private Python ABI guarantee.

The registry records the public LOM reference version **12.4.5** and the Live12 Python
source mirror commit **e83d5192f321b24eb9daab843ac49a2d95d862b1**. Its candidate scope
is Live 12.3.x and 12.4.x. Every patch in these reviewed minor versions is a source
candidate guarded by exact runtime class, parameter population, modes and display
evidence; 12.4.5 identifies the public documentation, rather than a patch ceiling.
This is not a verified Python ABI range, licensing matrix or host support statement.
Other minor versions, including 12.5+, return `UnknownRegistryCoverage`.
Existing Live11 compiler paths are unaffected. The registry never promotes a target
profile or reports a host as qualified.

The [DeviceParameter LOM](https://docs.cycling74.com/apiref/lom/deviceparameter/)
distinguishes internal values from GUI values, quantized labels, enablement, active
state and automation state. GUI units and native bounds alone do not define a
physical-to-internal conversion. The current public `display_value` reference and
the older pinned Max whitelist differ; this registry neither reads that property
nor infers its Python availability.

Pinned source observations identify native parameter names in
[`_Generic/Devices.py`](https://github.com/gluon/AbletonLive12_MIDIRemoteScripts/blob/e83d5192f321b24eb9daab843ac49a2d95d862b1/_Generic/Devices.py),
Utility current/legacy and Width/Mid-Side alternatives in
[`Push2/custom_bank_definitions.py`](https://github.com/gluon/AbletonLive12_MIDIRemoteScripts/blob/e83d5192f321b24eb9daab843ac49a2d95d862b1/Push2/custom_bank_definitions.py),
and zero-based quantized label indexing in
[`Push2/device_options.py`](https://github.com/gluon/AbletonLive12_MIDIRemoteScripts/blob/e83d5192f321b24eb9daab843ac49a2d95d862b1/Push2/device_options.py).
Source observations are distinct from public Max API and empirical host evidence.

## 2. Finite entries

There are **59** entries: 33 continuous and 26 quantized. The historical 48 identifiers
retain their semantics. Registry identifiers are distinct from semantic IR paths,
native object identities and chain indices. Band numbers in identifiers are native
EQ Eight numbers1..8.

| Identifier | Native class | Original parameter | GUI/semantic unit retained | Required observed mode |
|---|---|---|---|---|
| `drift.lp.frequency` | `Drift` | `LP Freq` | Hz | Instrument type1; actual LP Type and native voice mode/count retained |
| `drift.env.1.attack` | `Drift` | `Env 1 Attack` | ms | Instrument type1; native voice mode/count retained |
| `drift.env.1.decay` | `Drift` | `Env 1 Decay` | ms | Same |
| `drift.env.1.release` | `Drift` | `Env 1 Release` | ms | Same |
| `utility.gain` | `StereoGain` | `Gain` | dB | Stereo channel, Mono off, Mute off |
| `utility.balance` | `StereoGain` | `Balance` | Stereo balance | Same |
| `utility.width` | `StereoGain` | `Stereo Width` | Percent | Same; actual Width present/active, no active Mid/Side substitute |
| `eq8.band.N.frequency` | `Eq8` | `N Frequency A` | Hz | Native global mode0, band on |
| `eq8.band.N.gain` | `Eq8` | `N Gain A` | dB | Same; parameter itself active |
| `eq8.band.N.q` | `Eq8` | `N Resonance A` | Q | Same |
| `eq8.band.N.enabled` | `Eq8` | `N Filter On A` | Exact advertised two-choice enum | Native global mode0 |
| `eq8.band.N.type` | `Eq8` | `N Filter Type A` | Exact advertised eight-choice enum | Native global mode0, band on |
| `utility.enabled/channel_mode/mono/mute/left_invert/right_invert/bass_mono/dc_filter` | `StereoGain` | Actual named Device On/channel/mono/polarity/bass/DC controls | Exact native enum labels | Full unchanged current cohort |
| `eq8.enabled/adaptive_q` | `Eq8` | `Device On` / `Adaptive Q` | Exact native two-choice labels | Full observed native EQ modes |
| `eq8.scale` | `Eq8` | `Scale` | Percent | Native Stereo mode, actual unchanged EQ properties |
| `eq8.output_gain` | `Eq8` | `Output Gain` | dB | Native global mode0 |

The [official effect manual](https://www.ableton.com/en/live-manual/12/live-audio-effect-reference/)
defines Utility Width versus Mid/Side behavior and the distinct Gain and Balance
controls. EQ Eight has eight bands and eight response choices; some filter types
disable gain adjustment. Parameter state must therefore be observed. The registry
does not infer exact GUI labels or internal scales from these descriptions.

[Eq8Device.global_mode](https://docs.cycling74.com/apiref/lom/eq8device/) explicitly
encodes Stereo as0, L/R as1 and M/S as2. This must be an observed native device
property. Push's decorated “Eq Mode” wrapper is not treated as an actual native
DeviceParameter.

## 3. Pure actual-probe preflight

`preflight_live_native_parameter(id, intent, observed, expected_chain_index)`
performs no host operations. The caller must supply actual observations, retain
their provenance and native identities, and recheck managed ownership/drift before
mutation. The type contains optional observations so absent fields remain unknown.

Admission requires exact native class, instrument/effect type, active flat device and
expected chain position. A complete observed parameter population is required to
establish unique original/public names, including aliases that would collide with
the bridge resolver. A localized public name can match through the registered
original name; a matching public name with a different original identity cannot.
Targets and required mode parameters must be actual native DeviceParameter objects.
The [DeviceParameter reference](https://docs.cycling74.com/apiref/lom/deviceparameter/) makes
`default_value` available only for continuous parameters and `value_items` only for quantized
parameters. Native capture respects these getter preconditions; an unavailable quantized
default is explicit null, never a fabricated numeric value.

The actual target domain must be finite and nondegenerate, current value in range,
quantization matched, enabled and active (`state=0`) with no existing automation
(`automation_state=0`). Active or overridden automation is declined for this initial
write preflight; independent envelope readback has a separate read-only contract.
Utility's mode labels and EQ band activation require explicit observations.
Missing modes never default to Stereo, off or on. `Gain (Legacy)` and `Mid/Side
Balance` do not substitute for the registered identities.

Two mapping forms can produce a candidate:

- `LiveNativeInternalValue`: preserve the exact finite internal numeric request
  after checking actual bounds and integer membership for quantized parameters.
- `LiveNativeEnumChoice`: resolve an exact advertised label using the observed
  zero-based integer domain (`min=0`, `max=label_count-1`). Labels must be unique and
  nonempty; the current value must be an integer. Band activation/type require
  exactly2/8 choices. No fixed filter-type ordinal, synonym or locale translation
  is supplied.

Every physical-unit request returns `UnsupportedUnitMapping` after descriptor
validation. This includes dB, Hz, Q, percent and compositional stereo balance.
The registry does not normalize internal domains to0..1 or silently apply GUI
scales, interpolate physical mappings, clamp values or supply missing modes.

The read-only `sunny_resolve_native_display_value` Device operation accepts only
`{capability_id,target,tolerance}` and selects one of the 33 continuous entries.
Callers cannot supply names, descriptors or modes to bypass the registered policy.
The handler observes the actual application version before and after resolution.
The Python `native_units.resolve_registered_native_display_value` helper queries
the actual registered continuous parameter through `str_for_value`. Its bounded
64-call search retains every observed display, original/public parameter population,
domain and required mode. It rejects unparseable/localized displays, reversals,
drift, unreachable targets and exhausted budgets. It returns a candidate internal
knob value and its observed display error under an explicit caller tolerance;
display rounding does not establish an exact underlying physical value or filter
response. EQ Eight Scale and Adaptive Q are retained unchanged in the evidence;
the result always declares native-knob-only semantics. A decline preserves its
reason and formatter-call count; malformed evidence fails closed. Recording-only
transports report unavailable observations and do not send a query. This operation
performs no writes and establishes neither ownership nor full effect admission.
A later authorized write must format the independently read actual
parameter value, rather than formatting its requested candidate again.

The [Drift manual](https://www.ableton.com/en/live-manual/12/live-instrument-reference/#drift)
describes its low-pass filter and amplitude ADSR. The [DriftDevice reference](https://docs.cycling74.com/apiref/lom/driftdevice/)
exposes voice mode/count as actual list/index properties, rather than guessed parameter names.
The source pin names `LP Freq`, `LP Type` and the three `Env 1` time controls. Actual voice
lists/indices and the observed LP Type choice remain unchanged through resolution. Time displays
accept strict `ms` and `s` grammar, with seconds multiplied by 1000 into the authored millisecond
unit. The [edition table](https://www.ableton.com/en/live/compare-editions/) lists Drift and Utility
in Intro/Standard/Suite; EQ Eight is optional Standard/Suite. This is candidate availability,
while actual insertion and final host evidence determine installed-device acceptance.

`project_realization_plan_timbre` is read-only and requires explicit authored NativeAbleton Drift,
SubtractiveSynth source paths and exact source-device parameter bindings. At most four caller
selections read cutoff Hz or the three canonical nonlooping ADSR stage times in ms directly from
the authored profile. Existing binding ranges, curves and value_property are retained and are
not applied by this separate opt-in physical selection. The plan records the original codec and
all residual source/effect/other-domain leaves as RFC6901 pointers. It does not qualify oscillator,
filter response, sustain, modulation, sound or equivalence of the entire abstract source.

## 4. Candidate limits and later integration

A successful result is explicitly `RuntimeDescriptorMatchedCandidate` with
`host_qualified=false`, `envelope_runtime_qualified=false` and
`persistence_verified=false`. A continuous entry can identify the shape of an
internal Step-envelope candidate; a quantized entry is a static native choice and
does not enter the current continuous-envelope RPC subset.

These entries do not establish faithful coverage of an entire Utility/EQ effect,
all IR fields, resources, modes, signal-path behavior or device availability in a
particular edition. Auto Filter, other instruments/devices, general physical-unit
calibration, and other versions remain outside this registry batch. Subsequent
managed realization must preserve requested semantics and report remaining gaps.

Offline tests use literal synthetic descriptors, deliberate nonstandard enum
orders, unrelated aliases, missing observations, invalid domains and modes.
They verify pure decision/value behavior without proving a host scale. Final-host
qualification must record exact version/edition/OS and source/bridge identity,
observe actual native descriptors and units/modes, and independently verify writes.
Envelope persistence additionally requires Save As, close/reopen, reconnect and
read-only sampling without reauthoring or temporary remote control.


## Managed finite revisions

The owning effect planner reads selected whole Timbre EQ and Mix EQ/Utility stages
in authored order, including separate input trim. Peak, Low Shelf and High Shelf
map to actual native labels; unused bands are explicitly Off. Cut slopes, dynamic
EQ, linear phase, automatic gain and fractional wet intent are unavailable.
Input trim is a dedicated Utility before Mix effects; it never changes the output
fader. Utility setup observes Stereo/mono/mute/polarity/bass/DC controls and neutral
Gain/Balance/Width. EQ setup observes actual `global_mode`, `edit_mode` and
`oversample`, requests Stereo and Adaptive Q Off, and sets Scale100%/Output Gain0dB
before band resolution. These are native knob intentions, without DSP equivalence.

Each insertion, enabling, setup, physical and bypass phase receives a separate
durable dispatch fence and actual ACK. Missing response, mismatch or partial state
stops later phases. Current concrete Device/parameter cohorts and complete native
Clip/note evidence remain guarded. Reopened chains require separate explicit
whole-current-chain adoption. Authored bypass adopts only the actual Off control
and current handles; physical value agreement remains residual until explicitly
enabled and resolved. Native order conflicts are refused, without automatic
removal or reorder.

Geometry revision changes only Clip end and flat meter with unchanged attack keys,
full note values and native IDs; all tails must fit. End, numerator and denominator
are checked after each changed setter. Inactive loop-end state may remain independent
or reflect the actual unlooped Clip end; unexpected coupling stops the operation.
Fresh Clip adoption grants geometry as a fourth touched domain. Saved historical
two- or three-domain grants retain their exact scope until fresh explicit adoption.

Set settings support a single exact origin tempo converted through effective
quarter-note BPM and a single initial global meter. Transport is idle at origin;
Main tempo parameter eligibility, native Song/Track/Scene/cue and retained Part/Device
cohorts are checked before and after each changed scalar. Explicit approval affects
all Tracks. Arrangement tempo/meter populations remain unobserved, and notation
beat groups are reported as unapplied. A verified current no-op has no native setter.
Unknown Set-setting outcomes block later managed setters in the active history namespace;
zero-setter current-object adoption remains available to permit fresh explicit Set approval.
Workspace open and backup application cannot switch away from unresolved active history.
Earlier uncertainty remains retained and queryable. Startup on a different workspace does not
discover unrelated history directories, and legacy native tools do not use this managed barrier.

Selected Step pan replacement requires approval of the exact current preview and
whole selected parameter envelope, including unsampled state and same-parameter
modulation. It clears only that parameter, then creates/inserts once with actual
phase readback. Initial absent-envelope authoring shares the 64-Step bound and guards native
identity, notes and Device state before every phase and before sealing the result. Complete known
reply capacity is checked before its dispatch fence and before native creation. Interior samples are
incomplete breakpoint evidence; other parameter envelopes remain untouched. Native
Set save/reopen, exact Python API behavior and audible response remain final-host
qualification obligations.
