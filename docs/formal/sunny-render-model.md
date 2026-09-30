# Sunny render model

## Status and scope

This document is authoritative for the maintained `sunny::render` layer and its host-neutral
`sunny::max` adapters: the LFO, ADSR envelope, sample-and-hold source, arpeggiator, arpeggio event
generator, tick transport, local quarter-note-position signal, and bounded message-to-audio
transfer, including fixed-capacity tick-to-sample-offset planning and Max ITM permanent-event
scheduling. It specifies their domains, state transitions, determinism, timing precision, and
failure semantics.

The render layer alone does not render audible PCM, execute an audio callback, host a plug-in,
communicate with Live, or implement a Max/MSP object. The opt-in SDK package implements the
traditional-Max modulation, local-clock, and global-ITM event boundaries specified in §6. Ableton session construction belongs to
`sunny::infrastructure::ableton` and the external contract in `docs/ableton-max-conformance.md`.
The word *render* here means deterministic control/event generation unless a future separately
specified signal target is added.

## 1. Common contract

Every fallible native operation returns `Result<T>` or `VoidResult`. Invalid input leaves the
object's prior valid configuration unchanged. The Python binding translates a declined render
operation to `ValueError`; it does not expose an invalid native state or silently clamp the value.

The render error family is:

| Code | Meaning |
|---:|---|
| 3600 `RenderInvalidParameter` | A finite control parameter is outside its declared domain, or an enum value is unknown |
| 3601 `RenderInvalidSampleRate` | Sample rate is non-finite, non-positive, or below a configured LFO frequency |
| 3602 `RenderEmptyPattern` | A note was requested from an empty arpeggiator input |
| 3603 `RenderInvalidPPQ` | PPQ is outside `[1, 65535]` |
| 3604 `RenderInvalidPosition` | A tick, seek, advance, or scheduled position is negative or stale |
| 3605 `RenderUnrepresentableTiming` | A duration is non-positive, malformed, or not an integral tick at the selected PPQ |
| 3606 `RenderInvalidBlockSize` | A signal-block maximum is zero or a processing vector exceeds its validated maximum |
| 3607 `RenderInvalidSignalBuffer` | A non-empty host-shaped block has no output sample buffer |
| 3608 `RenderInvalidChannelCount` | A zero-input/mono-output host-shaped processor was given a signal count outside its exact topology |
| 3609 `RenderControlQueueFull` | A bounded Max adapter cannot admit another pending control without exceeding its fixed capacity |
| 3610 `RenderNotConfigured` | A Max adapter process or silence operation was called before successful DSP setup |
| 3611 `RenderEventQueueFull` | A fixed event scheduler cannot admit another event or atomic note pair without exceeding 256 reserved event cells shared by pending and retained events |
| 3612 `RenderEventBufferFull` | A caller-provided event-output span cannot hold every event due in one admitted vector |

Core errors retain their more precise meanings. `InvalidMidiNote` reports octave expansion beyond
MIDI `[0,127]`; `InvalidVelocity` reports a note-on velocity outside `[1,127]`; and
`InvalidBeat`/`ArithmeticOverflow` report a declined floating-to-rational boundary or
unrepresentable checked result. No render operation substitutes a default note, duration,
position, or parameter after one of these failures.

Stateful render objects are not internally synchronised. One caller owns mutation and processing,
or supplies external serialisation.

## 2. Deterministic pseudo-random contract

The LFO and arpeggiator use `std::mt19937`, whose engine sequence is fixed by the C++ standard. The
default seed is zero. `set_seed(s)` restarts the corresponding random stream; LFO reset also
restarts its stream from the retained seed. This makes the same operation sequence reproducible.

No implementation-defined distribution or shuffle is part of the contract:

- random LFO values map one engine integer `x` to
  `2 * x / mt19937::max() - 1`;
- random arpeggios use descending Fisher–Yates permutation;
- each permutation index uses a 64-bit sample formed from two consecutive engine outputs and
  rejection sampling, so modulo bias and library-specific `std::shuffle` behaviour are excluded.

Exact floating output assumes ordinary IEC 60559/IEEE-754 binary `double`, the repository's
supported numeric model. The integer arpeggio permutation is independent of floating arithmetic.
Changing a seed or a pattern-affecting arpeggiator setting invalidates the locked pattern and resets
its step to zero. Re-reading an unchanged cached random pattern does not consume more randomness.

## 3. Modulation sources

### 3.1 LFO

An LFO state is

\[
L=(w,f,\phi,y,s,R),
\]

where waveform `w` is one of sine, triangle, saw, square, or random; frequency
\(f\in[0,\infty)\) Hz; phase \(\phi\in[0,1)\); current output \(y\in[-1,1]\); seed `s`; and
pseudo-random state `R`.

For a finite sample rate \(F_s>0\), processing first evaluates the current phase and then advances

\[
\phi'=(\phi + f/F_s)\bmod 1.
\]

The current implementation requires \(f\le F_s\), ensuring at most one cycle boundary per sample.
The waveform functions at phase \(p\) are:

| Waveform | Output |
|---|---|
| Sine | `sin(2πp)` |
| Triangle | piecewise linear `0 → 1 → -1 → 0` over phases `0, 1/4, 3/4, 1` |
| Saw | `2p - 1` |
| Square | `+1` for `p < 1/2`, otherwise `-1` |
| Random | the held seeded value, replaced after a crossed cycle boundary |

`reset()` sets phase and exposed current value to zero and restarts the random stream. The first
post-reset process call therefore observes phase zero. `set_phase` accepts only finite `[0,1)`
values; phase wrapping is processing semantics, not setter coercion. `set_waveform` accepts exactly
the five named enumerators. An integer-cast or otherwise unknown enum returns
`RenderInvalidParameter` and preserves the prior waveform and recurrence state.

### 3.2 ADSR envelope

Envelope configuration satisfies

\[
A,D,R\in[0,\infty)\text{ seconds},\qquad S\in[0,1].
\]

States are `Idle`, `Attack`, `Decay`, `Sustain`, and `Release`. `trigger()` captures the current
level \(y_a\) and enters Attack. `release()` from any active state captures the current level
\(y_r\) and enters Release. At sample rate \(F_s>0\), positive-duration stages use the linear
increments

\[
\Delta_A=\frac{1-y_a}{AF_s},\quad
\Delta_D=\frac{1-S}{DF_s},\quad
\Delta_R=\frac{y_r}{RF_s}.
\]

Thus release reaches zero after its configured duration regardless of whether release began during
Attack, Decay, or Sustain. A zero-duration stage is an immediate state transition and does not add
a hidden sample. Output is clamped by stage completion to `[0,1]`. `reset()` enters Idle and clears
the exposed and captured values.

### 3.3 Sample and hold

`trigger(x)` accepts a finite normalised input \(x\in[-1,1]\), replaces the held value, and returns
no sample. `value()` observes without mutation. `reset()` stores zero. The Python signature is
therefore `trigger(input: float) -> None`.

### 3.4 Validated signal blocks

`SignalBlockContext::create(F_s, N_{max})` represents host-neutral DSP setup facts and requires a
finite positive sample rate and positive signed maximum vector size representable as `size_t`.
`validate_frame_count(N)` accepts a signed actual count only in \([0,N_{max}]\) and returns the
checked unsigned extent used to construct a span. This ordering mirrors Max's signed `long`
arguments and prevents a negative host count from becoming a huge unsigned vector. LFO block
processing additionally requires the retained frequency \(f\le F_s\).
`Lfo::validate_context(context)` exposes that compatibility check without rendering, mutation, or
output access so a `dsp64` adapter can reject a structurally valid but processor-incompatible
candidate before publishing it. Every block overload repeats the same check at runtime as defense
in depth. `Lfo::set_frequency(f, context)` closes the inverse control path: it validates the scalar
domain first, then requires the candidate frequency to fit the active rate, and publishes only on
success without resetting phase, value, or random-stream state. Validation completes before either
processor state or any output element changes; a
rejected call is therefore all-or-none.

For an admitted block, `Lfo::process_block` and `Envelope::process_block` evaluate exactly the same
scalar recurrence as \(N\) consecutive `process(F_s)` calls, in increasing sample order. The native
methods are `noexcept` and perform no allocation, locking, I/O, callback dispatch, or unbounded
search. An empty admitted span is a successful no-op. The convenience Python methods allocate and
return a list and are not an audio-thread interface.

`SampleAndHold::process_block` uses the same bounded validation and fills every admitted sample
with the retained scalar without changing it. A trigger/reset publication changes that scalar only
at the adapter's next vector boundary. This is a message-triggered constant-signal source, not a
signal inlet sampled by another signal or edge detector.

The span overload is appropriate only after a caller already owns a valid bounded span. The first
host-shaped overload accepts `(context, signed_frame_count, output_pointer)` and imposes the
trust-boundary order: validate the signed count, reject a null pointer for a non-empty block, then
construct the span and validate processor state. The Max-shaped mono overload accepts
`(context, signed_frame_count, signed_output_count, output_pointer_array)` and extends that order:
validate frame count; require exactly one output; for a non-empty vector require the array and
channel-zero pointers; only then select channel zero and construct a span. A rejected frame or
channel count never dereferences the array. Zero frames still require the declared mono topology
but admit a null array and remain a no-op. `RenderInvalidSignalBuffer` distinguishes missing
non-empty storage, `RenderInvalidChannelCount` distinguishes topology, and
`RenderInvalidBlockSize` distinguishes vector cardinality.

The complete generator-topology overload accepts
`(context, signed_frame_count, signed_input_count, input_pointer_array,
signed_output_count, output_pointer_array)`. Validation order is frame count, exact zero input
count, exact one output count, and then the non-empty output pointers. Because LFO and Envelope
have no signal inlets, the input array is never dereferenced and its pointer value is immaterial
when `input_count == 0`; an adapter may pass the exact callback value without normalising it. Any
nonzero or negative input count rejects before output topology, pointer access, processor state, or
output changes. Empty vectors retain the exact `0-in/1-out` topology requirement but admit null
arrays.

This contract is for a traditional MSP object with one `signal` outlet, not an MC object or a
`multichannelsignal` outlet. MC channel expansion changes `numouts` and also requires a musical
choice Sunny has not made: shared versus independent phase/envelope state across channels. A
future MC surface must specify that state algebra explicitly rather than treating `numouts > 1` as
several interchangeable calls to this mono method. Both processor types publish this fixed shape as
`signal_input_count == 0` and `signal_output_count == 1`, so an adapter does not duplicate literals.

C++ cannot prove that a non-null output array really contains the declared element or that channel
zero's allocation has the declared frame extent. A Max external must pass the exact host-owned
`ins`/`outs` arrays and counts after range-checking the platform `long` values; setup/ABI
provenance, callback flags/user data, and actual allocation ownership remain external evidence.

This two-phase shape is backpropagated from Max's `dsp64`/`perform64` lifecycle.
`sunny::max::LfoAdapter` and `EnvelopeAdapter` construct the current processing context from the
setup pair, then pass the perform callback's actual signed topology through the complete source
validator. A rejected rebuild invalidates that processing context without resetting recurrence
state or consuming queued controls. The opt-in SDK wrappers supply callback registration and
literal argument mapping.
Neither source inspection nor unit tests prove that the host supplied honest values, that the
binary loaded in a named Max build, or that it met an audio deadline.

Source time is call-driven: LFO phase and Envelope stage advance only for processed samples. To
preserve those semantics independently of patch-cord state, the traditional Sunny SDK wrappers
register their one-output perform routine without branching on outlet connection state. Omitting
`dsp_add64` would freeze state and is a different device policy. A named-host disconnect/reconnect
test remains necessary because source registration does not prove callback execution.

A DSP-chain rebuild has two validation layers. `SignalBlockContext::create` first validates the
host setup facts. Before admitting a candidate, `LfoAdapter` checks both the audio-owned retained
frequency and every pending frequency command against it. Queue inspection is permitted only under
the declared `configure_dsp`/`process` quiescence and producer mutex. A sample-rate reduction below
any such frequency rejects the candidate, clears the current processing context and configured
status, and leaves the queue and processor recurrence unchanged. Reusing the previous context would
apply a stale rate/vector contract to the newly built host chain. A message-thread frequency
publication validates against an active context before it is enqueued; while unconfigured its
syntax is admitted provisionally and every pending frequency is checked by the next rebuild. The
audio owner repeats the context-aware setter when applying that command. The runtime block check
remains authoritative as defense in depth.

All four SDK wrappers record the current `dsp64` maximum independently of processor admission and
always call `dsp_add64`. Their perform routine enters processing only when that exact rebuild was
admitted. Otherwise, or after a runtime processing failure, the shared
`silence_generator_output` path validates the current maximum, signed frame count, exact
zero-input/one-output topology, and output pointers before writing zeros. It never consults a stale
adapter context or drains controls. If the host itself supplies an invalid maximum/topology or
storage, even silence is rejected without dereference; C++ cannot manufacture safe storage from a
contradictory callback.

SignalBlockContext is immutable call/setup metadata, not a reset token and not the owner of a
processor. Passing a newly validated context therefore retains LFO phase/value/random-stream state,
Envelope stage/value/captured levels, and BlockClock position/fractional tick. The next admitted
call simply uses the new sample rate and maximum, so future phase/stage/tick increments reflect the
new rate. Only the processor's explicit `reset`/`stop`/position operations change the corresponding
state. The Max adapters build a candidate context on every DSP-chain rebuild and commit it only on
successful validation. They retain state by default; whether DSP stop/start additionally invokes
an explicit reset is a separate device policy, never an implication of `dsp64` itself.

LFO, Envelope, SampleAndHold, and BlockClock instances have mutable recurrence/control state and no
internal synchronisation. One instance therefore has exactly one exclusive owner. Direct setters,
trigger/release, reset, scalar/block processing, and queries must not execute concurrently. The Max
control adapters preserve that rule as follows:

1. every control publisher validates its scalar domain and takes one producer-side mutex;
2. that mutex serializes any main/timer/message callers into one logical producer;
3. the producer attempts one fixed-capacity SPSC publication and releases the mutex;
4. queue saturation rejects the new command as `RenderControlQueueFull` without changing the
   requested configuration or audio processor;
5. a configured process call validates frame extent, exact zero-input/one-output topology, and
   non-empty output storage before its first pop; rejection leaves queue, processor, and output
   unchanged;
6. at the next valid block edge, the exclusive audio owner pops and applies at most 64 commands in
   queue order, then renders the complete vector;
7. the audio path takes no mutex, allocates nothing, performs no I/O or Max call, and cannot drain
   more than the queue capacity even if publishers refill concurrently.

The queue uses lock-free `size_t` atomics, enforced at compilation, and release/acquire publication
around trivially copyable command cells. Lock-free counters separately retain setup failures,
enqueued, applied, rejected, and process-failure evidence; a separate bounded gauge reports command
cells currently pending in the publication stream. The producer increments that gauge before head
publication; the consumer decrements it after reading the cell but before releasing the tail cell
for reuse. It therefore remains in `[0,64]` even under concurrent refill. Admission is counted
before queue publication, so an observed applied command causally implies its enqueued count. The
pending gauge is neither a historical counter nor proof that the audio owner applied the command.
A status snapshot samples several atomics rather than freezing both threads, and concurrent
`clear_error` has atomic last-writer semantics. Counter monotonicity is conditional on fewer than
`2^64` occurrences of each event during one adapter lifetime. Controls are vector-boundary
operations, not sample-accurate messages. `configure_dsp` must not overlap `process`. This is an
explicit model condition, not a source-level lock: Cycling '74's SDK examples assign perform-read
fields from `dsp64`, but the public anatomy page describes chain construction without an explicit
old-perform quiescence theorem.
Named-host lifecycle stress remains necessary to validate that condition; a host that permits such
overlap would require a different atomic context-publication design before it is supported.

`ClockAdapter` uses that same shared queue, producer serialization, status, callback preflight, and
configuration-lifecycle contract for the ordered commands `tempo`, `position`, `play`, `record`,
`pause`, and `stop`. Tempo and position are rejected before publication unless they satisfy the
BlockClock domains; all six operations take effect at the next valid vector edge. It transfers no
Transport events and invokes no Max clock. A callback rejected for configuration, count, topology,
or storage leaves every pending command untouched. After those host-shape checks, the audio owner
applies admitted commands before planning the local recurrence. If that resulting recurrence has
an unrepresentable integer endpoint, the controls remain consumed and applied, while the clock
endpoint and output vector remain unchanged; the error is retained for reporting and the wrapper
silences the already validated output extent. This distinguishes accepted vector-edge control state
from successful signal rendering rather than pretending both operations are one transaction.

## 4. Arpeggiator

### 4.1 Configuration and expansion

An arpeggiator has a finite MIDI-note input sequence, direction, octave count
\(o\in[1,11]\), gate \(g\in(0,1]\), seed, locked pattern, and current step. Except for `Order`,
input notes are sorted ascending before octave expansion. Expansion maps each input note \(n\) to

\[
n+12k,\qquad k=0,\ldots,o-1.
\]

Every expanded note must remain in `[0,127]`. The whole generation is declined if any
transposition is outside MIDI range; invalid notes are not selectively dropped. `Order` preserves
input order within each octave. `Up`, `Down`, `UpDown`, and `DownUp` have their literal traversal
meaning, with the two turning points not repeated. `Random` applies the seeded permutation in §2.

Changing direction, octave range, seed, or notes clears the cache and returns the step to zero.
Changing gate does not rebuild a pitch pattern because gate affects only generated event duration.
`clear()` produces an explicitly empty state. `generate_pattern`, `next`, `current`, and
`pattern_length` then return `RenderEmptyPattern`; they never fabricate middle C.

### 4.2 Event generation

For a positive rational step \(d\), accepted gate \(g\), and pattern
\([p_0,\ldots,p_{m-1}]\), `generate_arpeggio` emits

\[
e_i=(p_i,\;i d,\;d\hat g,\;100),
\]

where \(\hat g\) is the rational approximation produced by `Beat::from_float(g, 10000)`.
Multiplication and onset calculation use checked rational arithmetic. Consequently timing after the
single documented floating-to-rational boundary is exact, reduced, and either wholly produced or
declined. There is no minimum-duration coercion.

As everywhere in Sunny's core, `Beat` is measured in whole notes: `d = Beat{1,4}` is a quarter-note
step. This is distinct from Live API and MIDI terminology, where one “beat” is one quarter note.

## 5. Block clock and tick transport

### 5.1 Construction and state machine

`BlockClock` is the callback-free clock recurrence; `Transport` composes it with a dynamic event
queue and synchronous callbacks. Both defaults use 480 pulses per quarter note (PPQ), and their
`create(q)` factories validate \(q\in[1,65535]\). Tempo is finite and in Live's documented
`[20,999]` BPM domain.

The exact coordinate maps are

\[
B_{Sunny}=\frac{ticks}{4q},\qquad Q_{MIDI/Live}=\frac{ticks}{q}=4B_{Sunny}.
\]

`TransportPosition::to_beats()` returns the first quantity as a canonical `Beat`;
`to_quarter_notes()` returns the second as the floating host-coordinate convenience value.

| Operation | Resulting state/effect |
|---|---|
| `play()` | `Playing` from any state |
| `record()` | `Recording` from any state |
| `pause()` | `Paused` from `Playing` or `Recording`; no-op from other states |
| `stop()` | `Stopped`, integer position zero, fractional remainder zero |

`Playing` and `Recording` are running clock states. `Recording` does not claim MIDI or audio input
capture; it distinguishes host intent while sharing advancement semantics. `is_playing()` is true
only for `Playing`; `is_running()` is true for either running state.

Stopping does not reconstruct already-dispatched one-shot events. Undispatched events remain in
the queue. Seeking to tick \(t\ge0\) resets the fractional remainder and discards queued events
strictly before `t` without callbacks. A backward seek cannot restore consumed events.

### 5.2 Scheduling and order

An event may be scheduled only at or after the current integer tick and must carry attack-or-zero
velocity `[0,127]` plus release velocity `[0,127]`; zero attack identifies note-off ordering.
Scheduling replaces any caller-supplied event start time with the canonical tick-derived Beat.
`schedule_note(t,p,d,v,r)` additionally requires positive whole-note duration `d`, note-on
velocity `v∈[1,127]`, release velocity `r∈[0,127]` (default 64), and

\[
4d\,q\in\mathbb Z.
\]

The note-off tick \(t+4dq\) is checked for overflow before either event enters the queue, so native
validation is all-or-none. Equal-tick dispatch order is:

1. note-offs before note-ons, permitting an unambiguous retrigger;
2. insertion order within the same event class.

Transport callbacks run synchronously during `advance` or `process_block`. The composed BlockClock
commits its endpoint before dispatch, so a callback may query and observes that endpoint. Dispatch
includes every queued event whose tick is at or before it. A callback must not re-enter a mutating
operation on the same Transport and must not throw; either unsupported behavior can leave externally
visible callback effects without a representable transactional result.

### 5.3 Audio-block conversion

For block size \(N\), tempo \(b\), PPQ \(q\), sample rate \(F_s\), and retained fractional tick
\(r_n\in[0,1)\), the abstract recurrence is

\[
a_n=r_n+\frac{Nbq}{60F_s},\qquad
\Delta_n=\lfloor a_n\rfloor,\qquad
r_{n+1}=a_n-\Delta_n.
\]

BlockClock advances by integer \(\Delta_n\) and returns the committed integer endpoints
`{tick_before, tick_after}`. The implementation evaluates the boundary conversion in `long double`
and retains `r`; it therefore removes the systematic loss caused by truncating every small block
independently. In the real-number model, cumulative quantisation is less than one tick.
Finite-precision rounding remains an audio-boundary approximation and is not described as
rational-exact time. Before integer conversion the accumulated delta must be strictly below
the exactly representable exclusive bound `2^63`; comparing with a floating conversion of
`INT64_MAX` is insufficient when that value rounds upward. Endpoint addition is checked separately.

The bounded position-signal form exposes the same recurrence, not a second clock. Let
\(x_n=t_n+r_n\) be the retained local tick position before the vector and

\[
s=\frac{bq}{60F_s}.
\]

For a running clock, output sample \(i\in[0,N)\) is the start-of-sample local position in quarter
notes,

\[
P_{n,i}=\frac{x_n+is}{q}.
\]

The output is projected to `double`; the recurrence and endpoint plan use `long double`. The first
sample therefore names the position before sample zero elapses, and the last sample is not the
post-vector endpoint. After all samples are written, the clock commits exactly the same
\((t_{n+1},r_{n+1})\) as the endpoint-only call above. A paused or stopped clock emits the constant
retained fractional position \(x_n/q\) and does not advance. An empty admitted vector writes
nothing and preserves the recurrence. `stop()` still resets both tick components to zero, while
`pause()` retains both.

The BlockClock and Transport host-shaped overloads accept a validated `SignalBlockContext` and a
signed actual block length \(N\in[0,N_{max}]\). A negative or oversize vector returns
`RenderInvalidBlockSize` before unsigned conversion or clock mutation; Transport also performs no
dispatch on that failure. An admitted call uses the checked extent and context sample rate and
otherwise has exactly the same recurrence as `process_block(N, F_s)`. Transport then dispatches at
the committed endpoint. This closes setup/vector cardinality, not event placement within the
vector.

The position generator's complete host-shaped form validates, in order: signed frame count, exact
zero inputs, exact one output, and non-empty output array/channel storage. It then plans the complete
endpoint—including overflow checks—before writing the first output or changing clock state. Thus a
direct rejected BlockClock call leaves both output and clock unchanged. C++ still cannot prove the
host pointer's allocation extent. `ClockAdapter` performs the same callback preflight before it
drains controls; its post-control arithmetic-failure rule is the distinct rule specified in §3.4.

`BlockClock::process_block` is `noexcept`, O(1), and performs no allocation, locking, I/O, callback
dispatch, event traversal, or Max operation. Rejection is state-preserving; paused/stopped calls
validate the boundary and return equal endpoints without changing retained clock state. This is a
tractable source primitive used by `ClockAdapter` after that adapter establishes exclusive audio
ownership and bounded control transfer. It does not move Transport events or communicate with a
Max clock.

This API is **not sample-accurate event delivery**. A callback has no intra-block sample offset, so
events crossed inside a block are delivered when that `process_block` call reaches its endpoint.
Smaller blocks reduce that latency; they do not change the contract. Explicit `advance(k)` is exact
checked integer advancement and is inactive while stopped or paused.

Nor is `Transport::process_block` a real-time-safe perform routine. The context overload proves
only that its signed count was checked against the configured vector maximum. Dispatch may pop an
unbounded number of queued events and invokes user-provided `std::function` callbacks whose time,
allocation, locking, I/O, and Max operations are unconstrained. Scheduling can allocate, and all
transport state is unsynchronised and exclusively owned. Consequently no `Transport` method may
be called concurrently on one instance, and a future `perform64` adapter must not call this
transport surface. Such an adapter may use the bounded BlockEventScheduler placement algebra only
after adding producer transfer, a declared overflow policy, and Max-clock handoff or another
documented host path; scheduler or audio-thread correctness cannot be inferred beforehand.

### 5.4 Fixed-capacity sample-offset planning

`BlockEventScheduler` composes BlockClock with exactly 256 inline event cells and caller-owned event
output storage. It is an exclusive-owner native primitive: scheduling, control, queries, and block
processing on one instance must not overlap. It allocates no storage, invokes no callback, and
performs no locking, I/O, Max call, or unbounded search. Scheduling and processing may each perform
O(256) insertion, scan, or compaction work; the capacity is a semantic bound, not a claim about a
particular audio deadline.

Raw `schedule` and `schedule_note` share the validation and canonical tick-to-Beat construction used
by Transport. A note request constructs its note-on/note-off pair before mutation and requires two
free cells, so pair admission is all-or-none. Stored order is ascending tick, note-off before
note-on at the same tick, then insertion sequence. Queue saturation returns
`RenderEventQueueFull`; `clear_scheduled` clears all cells and restarts the insertion sequence.
Seeking discards events strictly before the requested integer tick without output. As with
Transport, an event at that integer tick is admitted even if the retained fractional clock
position has already passed it; it maps to the next vector's offset zero rather than being silently
discarded.

Let a running non-empty vector start at exact implementation position \(x=t+r\), use
\(s=bq/(60F_s)>0\) ticks per sample, and contain \(N\) samples. Its event domain is the half-open
interval

\[
[x,x+Ns).
\]

An event at integer tick \(k\le x\) maps to offset zero. Every later due event maps to the sample
interval that contains it,

\[
o(k)=\left\lfloor\frac{k-x}{s}\right\rfloor,\qquad 0\le o(k)<N.
\]

Thus quantisation is toward the start of the containing sample and is early by less than one sample
in the real-number model. An event exactly at \(x+Ns\) is not part of this vector and remains queued
for offset zero of the next non-empty running vector. The boundary comparison uses BlockClock's integer endpoint and fractional remainder separately:
`k < tick_after`, or `k == tick_after` with a positive fractional remainder. Offset calculation
subtracts integer ticks before floating conversion, then subtracts the start remainder. This
preserves sub-tick intervals when the absolute tick coordinate is large. The recurrence remains
deterministic under §2's numeric assumption but is not rational-exact time.

Before writing anything, processing validates the signed vector against SignalBlockContext,
preflights the complete clock plan, counts every due event, and requires the caller span to hold the
whole due prefix. Insufficient storage returns `RenderEventBufferFull`. Any cardinality, arithmetic,
or storage-capacity failure preserves the event span, fixed queue, and BlockClock state. Success
writes the ordered prefix, removes exactly those events, and commits the same BlockClock endpoint as
§5.3. Paused/stopped and empty vectors write and consume no events.

This closes a bounded source-side offset algebra, not Max delivery. It supplies no producer-to-audio
schedule-command transfer, Max clock, outlet, MIDI object, latency rule, or documented host path
that can honor the offsets. Dynamic Transport retains its endpoint callbacks and remains unsuitable
for `perform64`; BlockEventScheduler must not be described as sample-accurate Max or Ableton event
output until a separately bounded consumer of these offsets and named-host evidence close those
obligations. The ITM path below is not that consumer.

### 5.5 Fixed-capacity Max ITM event scheduling

`ItmEventAdapter` is a distinct scheduler-thread path; it does not consume BlockEventScheduler
offsets and is never called from `perform64`. Arbitrary Max message publishers are serialized by an
outside-consumer mutex into one 64-command SPSC stream. Successful command publication and its
consumer-clock wake happen under that same mutex, so even the host wake API sees one logical
publisher. One serial Max scheduler context exclusively owns a sorted queue of at most 256 events
and a matching 256-slot occupancy map. Producer publication, scheduler command application, and
permanent-event callbacks are therefore three explicit phases, rather than unsynchronised access
to one Transport.

`event(k,p,v,r)` accepts a nonnegative Sunny tick, MIDI pitch in `[0,127]`, attack-or-zero velocity
in `[0,127]`, and release velocity in `[0,127]` (default 64), where zero attack is note-off.
`note(k,p,d,v,r)` additionally requires integral duration ticks `d>0` and note-on velocity in
`[1,127]`; it constructs the note-on at `k` and note-off at `k+d` before
publication. A reservation gauge counts both retained events and events in pending commands.
Admission requires one cell for `event` or two cells for `note`, so a pair cannot be split by either
the 256-event bound or the 64-command bound. `clear` is itself a command and is ordered with events
published before and after it. A queued clear does not speculatively release reservations, so a
later publication may conservatively fail until the consumer applies that clear.

Status keeps the phases distinct. `commands_pending` is the current number of cells in the
producer-to-scheduler stream. `events_reserved` is the admission gauge just described, so it can be
nonzero before any ITM location exists. `events_retained` is the current scheduler-owned sorted
queue cardinality and changes only when the scheduler applies, clears, or fires events. In
particular, publication yields pending > 0, reserved > 0, retained = 0; successful application
yields pending = 0 and retained = reserved. The wrapper must not label the reservation gauge as
retained host state. These lock-free values are non-atomic as a combined snapshot and establish
source phase, not Max acceptance or callback delivery. `events_scheduled` counts invocations of the
required host-schedule action, `events_cancelled` counts invocations of the required host-stop
action during `clear`, and `events_fired` advances only after the required host-stop action has been
invoked for every member of the emitted group. These are action-bound source facts because the Max
SDK operations return no acceptance result; they are never manufactured when a callback is absent.

Let Sunny PPQ be \(q_s\), the Max ITM resolution sampled by the scheduler consumer be finite
\(q_h>0\), and the current global ITM position be finite \(c\ge0\). A source tick maps to the Max
double domain as

\[
h(k)=\operatorname{double}\!\left(\frac{kq_h}{q_s}\right).
\]

The adapter admits an applied event only when `h(k)` is finite, strictly greater than `c`, and its
existing adjacent source ticks remain distinguishable: `h(k-1) < h(k) < h(k+1)` wherever those
neighbors exist. This rejects the large-coordinate region where binary64 would collapse distinct
Sunny ticks rather than silently coalescing their time. The host-domain projection still has the
ordinary nearest-binary64 rounding error; it is injective over admitted adjacent source ticks, not
an exact rational identity.

Relative publication is a derived convenience over the same absolute model. For a nonnegative
integer delay \(d\), `tick_after_itm` computes

\[
k_0=\left\lfloor\frac{cq_s}{q_h}\right\rfloor+1,\qquad k=k_0+d,
\]

with checked integer arithmetic, then reuses the full injectivity check and requires
\(h(k)>c\). Thus `event_after 0` means the first representable Sunny tick strictly after the
sampled host position; it never means “now.” Sampling does not reserve host time. The command is
still preflighted against a later scheduler-consumer snapshot, so latency that makes \(h(k)\le c\)
causes transactional `RenderInvalidPosition` rejection rather than a late event.

After every event in one command passes that preflight, the scheduler consumer assigns fixed slots,
inserts by the shared tick/note-off/insertion order, and asks the SDK wrapper to set each slot's
permanent location time. The wrapper gives the hidden time-slot class a `TIME_FLAGS_TRANSPORT`
attribute, whose documented default is Max's global ITM object, and creates all 256 `t_timeobject`
instances before use with
`TIME_FLAGS_TICKSONLY | TIME_FLAGS_USECLOCK | TIME_FLAGS_PERMANENT | TIME_FLAGS_LOCATION |
TIME_FLAGS_POSITIVE`. The wrapper obtains Max's unnamed global ITM object once during successful
construction, increments its documented reference count before use, keeps that reference while
commands can sample its position, and decrements it only after its clocks and permanent slots are
stopped and destroyed. Neither the wrapper nor adapter starts, stops, seeks, renames, or changes
the resolution of that transport.
On request, the wrapper emits the sequential, non-transactional observation
`transport_status current_tick ticks_per_quarter running transport_name` from that retained ITM.
The SDK name may be empty for the unnamed global transport. This tuple uses only public ITM APIs
and exposes the inputs needed by a named-host harness; it is not an atomic transport snapshot or
evidence of Live identity, callback delivery, or sample accuracy.

The action functions are part of the adapter's mutation precondition, not optional instrumentation.
An event command with no schedule action is rejected and releases its reservation before slot or
queue mutation. A `clear` with retained events and no cancel action is rejected without changing
the retained queue or reservations; an empty clear needs no host action and remains a successful
ordered command. Thus no typed caller—including the installed-package consumer—can obtain
scheduled, retained, or cancelled evidence merely by passing null callbacks.

The first slot callback for an equal-source-tick group preflights complete output storage, asks the
wrapper to `time_stop` every permanent time object in the group, atomically copies and removes the
entire ordered group, releases every slot before downstream output, and emits
`[pitch attack_velocity_or_zero release_velocity]` lists.
An obsolete same-time callback that the host still delivers before reuse observes an inactive slot
and does nothing. An undersized native callback buffer returns `RenderEventBufferFull` without
stopping a host time object or consuming the group. A missing stop action returns
`RenderInvalidParameter` with the same transactional preservation. Downstream synchronous feedback can publish a
later command, but it
cannot mutate the scheduler-owned event queue reentrantly; the command clock applies it after the
current callback. Fired events are one-shot even if the host later loops or seeks across their
permanent location again.

Cycling '74 documents that an unnamed transport inside a Max for Live device follows Live, whereas
standalone Max uses its global internal transport. That establishes the selected clock path, not
unconditional sample accuracy. Sample-accurate delivery additionally requires Overdrive,
Scheduler in Audio Interrupt, a high-priority command applied before its target, and a downstream
object documented to accept sample-accurate events. Source tests and pinned-header compilation
prove capacity, ordering, conversion, C++/SDK shape, and failure behavior; actual ITM scheduling,
callback priority, downstream timing, seek/loop/preview behavior, binary lifetime, and deadline
performance remain named-host evidence. In particular, fixed-slot reuse after `time_stop` relies on
the host honoring cancellation before any obsolete callback can be delivered for that time object;
the source model cannot prove that scheduler-lifecycle fact.

## 6. Ableton and Max external alignment

The render transport is not Live's transport and does not drive the Live Object Model. Live session
tempo, clips, notes, devices, and mixer state are authored by the separately versioned bridge. A
shared word such as *tempo*, *tick*, or *recording* does not imply shared state or target evidence.
Cycling '74 documents Live API `beats` as quarter-note counts, so every `Beat`-to-LOM note boundary
multiplies by four. MIDI import/export and PPQ transport use the same scale explicitly; no adapter
may pass `Beat::to_float()` straight through as a host beat count.

BlockClock is a local constant-tempo integrator until its exclusive owner changes tempo or
position; it is not a Live phase observer. Cycling '74 documents Live as the clock source for the
Max for Live tempo system only through the applicable transport configuration: unnamed transports
synchronise by default, named transports require `clocksource live`, identically named transports
in separate device instances still run independently, and entering/leaving preview mode may disrupt
continuity. The Max transport reference additionally warns that beat-time song-position translation
can be inaccurate when the Live Set contains tempo changes. Therefore one observed BPM, one Max
transport tick, or one local BlockClock endpoint cannot be promoted into exact Live position or
tempo-automation evidence. Stronger correspondence requires a declared host timing path, validity
and discontinuity rules, per-boundary observations appropriate to that path, and named-Live tests.

The render processors alone are not a Max signal or scheduler contract. The `max-package/` SDK
project now closes a deliberately narrow traditional-Max subset with:

- `sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`, and `sunny.clock~`, each declared as zero signal
  inputs and one signal output;
- `sunny.events`, a non-DSP one-list-output object backed by 64 producer commands, 256 pre-created
  permanent ITM time objects, and one lifetime-owned reference to the unnamed global transport;
- constructor success for each signal generator only after its one `signal` outlet has been
  created and returned a non-null SDK pointer;
- public class publication only after every documented method and the class itself register with
  `MAX_ERR_NONE`; unregistered partial classes are freed, and `sunny.events` registers its hidden
  time-slot dependency before exposing the public class;
- `dsp64` construction of the validated context and unconditional perform registration;
- exact forwarding of signed counts and host arrays into the bounded adapter;
- `t_atom_long` preservation for integer messages before explicit Sunny-domain conversion;
- message schemas for LFO configuration/reset, ADSR parameters/gate/reset, held value/reset, and
  local clock tempo/position/state, plus absolute and sampled-relative integer event/note
  scheduling, clear, adapter status, and global-ITM transport status;
- phase-separated status gauges for pending control/ITM commands, reserved event capacity, and
  scheduler-retained events;
- fixed vector-boundary control timing and producer serialization;
- build-tree package staging with a pinned max-sdk-base revision;
- help patchers plus reference/autocomplete XML checked against actual method selectors, SDK
  argument schemas, C++ handler parameter types, and the ordered release-velocity-preserving
  `sunny.events` → `xnoteout` example topology.

`sunny.clock~` emits the §5.3 start-of-sample quarter-note coordinate from its own BlockClock. Its
optional creation argument selects local PPQ; `tempo`, `position`, `play`, `record`, `pause`, and
`stop` publish local commands. It does not inspect Max `transport`, select `clocksource live`, call
the Live Object Model, schedule MIDI, follow Live seeks/loops/tempo automation/preview state, or
claim Ableton song position. Those omissions are type and deployment boundaries, not undocumented
degradation modes.

`sunny.events` instead selects the documented global ITM scheduling path. It converts Sunny ticks
under §5.5, never controls the transport, and emits one-shot pitch/velocity lists only from ITM
permanent-event callbacks. It is not an audio-thread bridge from BlockEventScheduler, a named
transport abstraction, a looping clip, a MIDI port, or proof that arbitrary downstream objects
honor sample offsets. The two event mechanisms intentionally close different boundaries:
BlockEventScheduler proves source-vector placement; `sunny.events` delegates placement to Max's
transport scheduler.

The output list is deliberately not the four-atom input expected by `xnoteout`. The maintained
help patch therefore orders three list copies explicitly: it stores release velocity first, derives
the note-on/off flag and selects attack-or-release velocity second, and sends pitch last to trigger
`xnoteout 1`. Note-on becomes `(pitch, attack, 1, channel 1)`; note-off becomes
`(pitch, release, 0, channel 1)`. The example stops at formatted MIDI bytes: it has no `midiout`,
selected port, receiving device, or delivery evidence.

Max for Live integration still additionally requires:

- inlet/outlet and parameter schemas;
- message-thread, scheduler-thread, and audio-thread classification;
- vector-size, intra-vector offset, latency, and reset rules;
- real-time safety proof excluding allocation, locks, I/O, Live API calls, and unbounded work from
  DSP execution;
- `.amxd` or native package artefacts and named-host tests;
- rendered-signal criteria for sonic claims.

The host-neutral adapter deliberately contains no Max SDK pointer or scheduler operation. The thin
SDK layer owns object/outlet lifetime and unconditional callback registration. Each signal wrapper
requires its single `outlet_new(..., "signal")` call to return non-null before construction can
succeed; failure is reported and destroys the partial object. A failed DSP setup is reported
outside perform and marks that rebuild unadmitted. Perform then uses the shared current-maximum
silence path without entering the stale processor context. A runtime failure is stored in lock-free
status and takes the same bounded silence path. The callback never calls an outlet or error
function.

The Max SDK permits a clock as the supported perform-to-Max communication mechanism but forbids
qelems and outlets there. It also documents main-thread, timer-thread, audio-thread, SIAI, and
non-real-time arrangements rather than one fixed producer/consumer topology. The adapter therefore
does not assume one host producer: it first serializes every admitted publisher through an
outside-audio mutex, and only the resulting logical producer writes the SPSC queue. This proof does
not extend to future signal/event inlets until they route through the same classification boundary.

Native adapter and SDK-header tests still cannot be used as evidence that a Max scheduler or Live
audio engine is sample accurate. The remaining host gates and authoritative external references
are maintained in `docs/ableton-max-conformance.md`.

### 6.1 Named-host evidence algebra

`MaxValidationRecord` schema 1 is an Infrastructure evidence type, not an operation available to an
audio or scheduler callback. Let the artifact sequence (A) be exactly

\[
(\texttt{lfo},\texttt{adsr},\texttt{hold},\texttt{clock},\texttt{events})
\]

under their public Sunny object names, and let (C) be the ordered 19-check sequence encoded by the
schema. Each artefact binds the platform-derived package path and binary SHA-256. Each check has one
outcome in `{passed, failed, not_run, not_applicable}`. Passed or failed is equivalent to the
presence of both a package-relative evidence path and nonzero lowercase SHA-256; the other two
outcomes require both absent. Only `live_transport_discontinuities` in standalone Max and
`standalone_transport_identity` in Max for Live are inapplicable.

For one shape-valid record (R), its derived verdict is

\[
\operatorname{complete}(R)=
\neg\operatorname{example\_provenance}(R)
\land
\bigwedge_{a\in A}(a.\operatorname{discovered}\land a.\operatorname{instantiated})
\land
\bigwedge_{c\in C}
\begin{cases}
c.\operatorname{outcome}=\texttt{passed}, & \operatorname{applicable}(c,R.\operatorname{host})\\
c.\operatorname{outcome}=\texttt{not\_applicable}, & \text{otherwise.}
\end{cases}
\]

Shape validity additionally closes current Sunny/package versions, source revision, exact package-
archive digest, UTC timestamp, harness, supported OS/architecture, host-conditional version fields,
audio settings, array order/cardinality, relative paths, and hash spelling. The serialized
`complete` member must equal the derived value. `example_provenance` recognizes the package's
reserved `example_` harness, `replace-with-` environment strings, and deliberately uniform hex
revision/digest sentinels; those values remain parser-valid teaching material but can never certify
a run.

Let (O) be the closed `MaxValidationObservation` obtained from the named host, (P) the exact package
archive bytes, (B_a) the regular binary file at each required package path, and (E_c) the regular
evidence file present exactly for an observed check. Materialization is the partial function

\[
\operatorname{materialize}(O,P,\{B_a\},\{E_c\})=R
\]

that copies all facts in (O), derives
`SHA256(P)`, every `SHA256(B_a)`, and every applicable `SHA256(E_c)`, then requires (R) to satisfy
the strict record parser. It is undefined for an invalid observation, unavailable root, absent or
non-regular file, root escape, I/O instability, or invalid derived record. Verification reruns that
function from (R)'s unhashed projection and requires every derived digest to equal (R). This
establishes an auditable byte envelope only: truth and authenticity of the external observation,
host-file capture atomicity, other configurations, `.amxd` participation, and perceptual
equivalence are not derivable from the schema or materializer.

For each signal external, the machine observation emitted by `status` is the ordered tuple

\[
H=(c,F_s,M,p,u,n,h,e),
\]

where `(c,F_s,M)` is one generation-coherent configuration snapshot, `u` is the exact signed
saturating process-call count, `p` is `u > 0`, `n` is the last observed signed frame count, `h` is
`process_failures == 0`, and `e` is the retained Sunny error code. If (N) callbacks have entered
since construction then (u=\min(N,2^{63}-1)); saturation preserves monotonicity and Max's exact
integer atom representation. For `sunny.events`, the emitted
tuple is

\[
I=(q,s,f,x,h,r,k,e),
\]

where the first four predicates respectively mean that a command was admitted, an event was
scheduled, an event fired, and an event was cancelled; `h` means no callback failure has been
counted, `(r,k)` are the reserved and retained event gauges, and `e` is the retained error code.
Each tuple is a lock-free, non-transactional status snapshot outside the real-time callback. Thus a
tuple can establish that Sunny's wrapper observed a configured/called/healthy source path, but not
host buffer provenance, outlet connection, scheduling latency, deadline compliance, MIDI delivery,
or audible output.

The separate `transport_status` tuple is

\[
J=(c,q_h,r,n),
\]

where the entries are the sequentially sampled ITM tick, global resolution, normalized running
predicate, and SDK transport-name symbol. It identifies the wrapper's observed
timing inputs but is neither generation-coherent nor proof that the unnamed global transport is a
particular Live Set timeline.

Let (C_T) be the checks mapped by the pinned standalone `max-test` manifest:

\[
C_T=\{\texttt{package_discovery},\texttt{public_class_surface},
\texttt{signal_topology},\texttt{dsp_setup},\texttt{perform_callback},
\texttt{finite_signal_output},\texttt{disconnected_processing},
\texttt{reconnect_continuity},\texttt{control_dispatch},
\texttt{itm_schedule_fire},\texttt{itm_equal_tick_ordering},
\texttt{itm_clear_reassign},\texttt{release_velocity_formatting},
\texttt{standalone_transport_identity}\}.
\]

Every mapped check is the conjunction of the exact named assertions in
`max-test-harness.json`; all of those assertion rows must equal `Pass`. The other four standalone
checks—active DSP teardown, scheduler timing, downstream MIDI delivery, and rendered audio—have
automation state `not_automated`, while `live_transport_discontinuities` is
`not_applicable`. Consequently a successful smoke run defines observations for (C_T), not
`complete(R)`: the independent observation/materialization path must still represent every check
and bind the retained database/evidence bytes.

For every signal wrapper (w), let (b_w), (d_w), and (r_w) be exact `process_calls` observations
taken respectively before its sink patch cord is removed, after a bounded disconnected observation
window, and after that patch cord is recreated and a second bounded observation window. The harness
uses documented `thispatcher` scripting names for the four exact disconnect/connect pairs and
requires

\[
0 < b_w < d_w < r_w.
\]

It then re-arms each `test.sample~` sink and requires a new callback with the wrapper-specific
expected sample. The 120 ms windows only allow the host to process vectors; neither their expiry nor
the 2000 ms watchdog is a passing condition. Thus the evidence is callback-count advancement plus
fresh post-reconnect output, under the captured standalone-Max graph. It does not establish active
object destruction, leak freedom, deadline compliance, or behavior in Max for Live.

For the ITM subset, the smoke stops and resets the unnamed standalone-Max `transport`, sets tempo
120, publishes relative events, and then starts that transport. Let the callback list trace be

\[
L=([60,100,23],[62,0,47],[61,100,31],[63,110,59],[63,0,59],[65,101,73]).
\]

The first five callbacks establish schedule/fire, equal-tick note-off-before-note-on ordering, and
note-on/release-velocity formatting. The fifth callback schedules `[64,100,71]`, clears it, and
schedules `[65,101,73]`; receiving the latter as the sixth callback establishes the bounded
clear/reassignment scenario. Final `event_status` and `transport_status` assertions run while the
transport is still running, after which the harness stops the transport and terminates. A 1500 ms
delay can only fail and terminate a stalled run; it does not order a successful trace. This proves
behavior relative to the unnamed standalone transport controlled by the patch, but not target
sample offset, scheduler-setting invariance, downstream MIDI, rendered audio, or Live Set transport
identity.

Let (D) be one closed, quiescent SQLite database and (Q) its normalized raw-row projection for one
explicit positive test ID. Schema 1 for (Q) contains exactly the database SHA-256, one completed
`tests` row, and the strictly increasing, uniquely named `assertions` rows joined by that test ID.
Every timestamp has the upstream local `YYYY-MM-DD HH:MM:SS` form, each assertion is temporally
inside the test interval, and each outcome is in `{Pass, Fail}`. Extraction is undefined when a
journal/WAL/SHM sidecar exists, required columns are absent, the run is missing or unfinished, the
database fails SQLite `quick_check`, or its digest changes across the read.

For the exact compiled manifest bytes (T), evidence-relative database path (p), and standalone
observation (O), native application is the partial function

\[
\operatorname{apply}_{T}(O,Q,D,p)=O'
\]

defined only when `SHA256(T)` equals the compiled schema-1 manifest digest,
`SHA256(D) = Q.database_sha256`, the test identity matches (T), and the unique assertion-name set in
(Q) equals the 64-name set induced by (T). For each (c \in C_T),

\[
O'[c].outcome =
\begin{cases}
\texttt{passed}, & \bigwedge_{a\in T[c]} Q[a]=\texttt{Pass}\\
\texttt{failed}, & \text{otherwise,}
\end{cases}
\qquad O'[c].evidence=p.
\]

Each artifact is discovered and instantiated only when the conjunction of its own required
assertions passes. All `not_automated` and host-inapplicable checks are unchanged. Record
materialization later hashes the same database path; repeated check references use one captured
digest. Thus Python is a row-transport adapter, while C++ remains the schema, mapping, and evidence
authority. This construction detects accidental result/database/manifest drift but still does not
authenticate a malicious extractor, operator, or host.

Let \(P_H\) be the closed external-run plan adjacent to the release manifest. Its repository-local
preparation function is deliberately weaker than an observation:

\[
\operatorname{prepare}_{P_H}(z,s,h,e)=W,
\]

where \(z\) is one platform ZIP, \(s\) its exact SHA-256 sidecar, \(h\) the extracted package root,
and \(e\) the operator-supplied environment. Preparation is defined only when the sidecar equals
`SHA256(z)`, every regular non-symlink file under (h) is byte-identical to the corresponding safe
`Sunny/` ZIP member, the release cell and archive name agree, source/timestamp/environment domains
are valid, and the destination cell does not exist. The resulting workspace \(W\) has the closed
evidence directories and correct opposite-host `not_applicable` outcome, but every applicable
check is `not_run` and every artifact discovery/instantiation fact is false. Thus

\[
\operatorname{prepare}_{P_H}(z,s,h,e) \not\Rightarrow \operatorname{observed}(h)
\quad\text{and}\quad
\operatorname{complete}(W)=\mathrm{false}.
\]

\(P_H\) further fixes measurable external inputs: 1,000 active-DSP destruction cycles per public
object with crash/leak diagnostics; 32 timely events at 120 BPM and 48-tick spacing, whose supported
fully enabled `click~` path has exact 2,400-sample spacing at 48 kHz, plus disabled-setting,
late-target, and ordinary-message controls; two exact channel-1 note-on/off pairs received through
an independent MIDI input; 4,096 or more settled lossless PCM frames for four closed value/slope
comparisons; and six Live transport-discontinuity scenarios. These criteria reduce operator
choice, but the resulting measurements remain named-host evidence and are not proved by \(P_H\).

Let (A_h) be the twelve closed shared evidence files for host environment (h), extended by the
host-saved device, exact pinned assertion map, normalized assertion projection, scenario result,
and console exactly when (h) is Max for Live. The editable M4L source transformation is a partial
function of the exact Sunny smoke/helpers, pinned assertion map, and pinned upstream console helper;
its codomain fixes `max_audio_effect`, removes the standalone global-DSP start edge, and substitutes
an unconnected `plugout~` declaration for `dac~`, so no validation signal is routed into Live;
its inventoried `.maxpat`/JavaScript result has no observation codomain. A separate exporter selects
one exact run-ID-delimited console interval and preserves all 64 `Pass`/`Fail` facts while binding
the manifest, saved-device, and console digests. It likewise has no Sunny-check outcome codomain.
The outcome-free indexer is the partial function

\[
\operatorname{index}_{P_H}(O_h,A_h)=I_h,
\]

defined only when observation provenance is present, every plan-relative role is a stable confined
regular file, and (I_h) contains `SHA256(P_H)`, the exact environment/provenance, ordered roles,
paths, and `SHA256(a)` for every (a\in A_h). It has no outcome codomain. Native evaluation is a
separate partial function

\[
\operatorname{eval}_{P_H}(I_h,A_h)=
(d_h,s_h,m_h,r_h,b_h?,q_h?,\ell_h?)
\in\{\mathrm{pass},\mathrm{fail}\}^{4+19\delta_h},
\]

where \(\delta_h=1\) only for Max for Live, \(b_h\) is the five-artifact Boolean vector, and
\(q_h\) is the thirteen shared-check vector. It is defined only when the compiled plan and
assertion-map digests, result digest, transitive file digests, provenance, closed JSON shapes, and
WAVE containers agree.
The evaluator derives teardown \(d_h\) from lifecycle/diagnostic counts; scheduler \(s_h\) from
scenario facts plus nonzero positions decoded from the four-channel timing WAVE; MIDI \(m_h\) from
the exact independent receiver bytes; render \(r_h\) from native float-WAVE sample and slope errors;
the M4L vectors \(b_h,q_h\) from the pinned conjunctions over the exact 64 device assertions; and
M4L discontinuities \(\ell_h\) from the six ordered device-context count/gauge observations bound
to the saved-device digest and the same assertion run ID. A criterion miss returns `fail`;
malformed or drifting evidence makes the function undefined. Consequently,

\[
\operatorname{eval}_{P_H}(I_h,A_h)\ \text{defined}
\not\Rightarrow \operatorname{hostAuthenticated}(h),
\]

and a repository fixture proves only the evaluator algebra. The `.amxd` and every actual fact in
\(A_h\) still require the named commercial host; the source transformation and exporter do not
synthesize the thirteen shared device-context outcomes.

For a named Live attempt, let (b) be the installed server, (Y) the complete regular-file index
of the operator-named Remote Script root, and (L_0,L_1) stable snapshots of the named Live log
before and after the attempt. The executable handoff retains

\[
(\operatorname{SHA256}(b),\operatorname{SHA256}(Y),
  \operatorname{SHA256}(L_0),\operatorname{SHA256}(L_1))
\]

beside the JSON-RPC trace, dry-run plan, and conditional strict validation record. This rules out
unrecorded file substitution in the retained handoff but does not prove code signing, that Live
loaded the indexed tree, or that log prose establishes any modeled postcondition.

Let the native release-target set and host-kind set be

\[
P=\{(\texttt{macos},\texttt{x86\_64}),
(\texttt{macos},\texttt{arm64}),
(\texttt{windows},\texttt{x86\_64})\},\qquad
K=\{\texttt{standalone\_max},\texttt{max\_for\_live}\}.
\]

The closed release cell set is \(H=P\times K\), in the exact order and at the deterministic record
paths declared by `max-release-matrix.json`. For manifest bytes \(G\), record-root \(U\), and strict
records \(R_h\), native release materialization is the partial function

\[
\operatorname{matrix}_{G}(U)=M
\]

defined only when `SHA256(G)` equals the compiled manifest digest and, for every \(h\in H\), the
declared path resolves to a confined non-symlink regular file whose stable SHA-256 is retained in
\(M\), whose JSON parses as \(R_h\), whose environment identity is exactly \(h\), and for which
`complete(R_h)` holds. In addition,

\[
\forall h\in H:\quad
R_h.version=M.version\ \land\ R_h.revision=M.revision,
\]

and for the two host kinds \(k_1,k_2\) on each native target \(p\in P\),

\[
R_{(p,k_1)}.archive=R_{(p,k_2)}.archive
\quad\land\quad
R_{(p,k_1)}.binaries=R_{(p,k_2)}.binaries.
\]

The embedded records make matrix completeness self-contained; their file digests additionally bind
the exact aggregate inputs. This rejects missing/extra/reordered cells and a “release” assembled
from different builds. It does not quantify over unrecorded scheduler settings, re-run the
per-record archive/evidence verifier, authenticate the producer, or widen any observation beyond
its explicit environment.

## 7. Tractability boundary

| Property | Repository tractability | External evidence required |
|---|---|---|
| Input domains and state transitions | Fully tractable in native/Python tests | None |
| Bounded LFO/ADSR recurrence, constant held-value, and local position vectors | Fully tractable in native/Python tests | None |
| Signed-count/pointer validation before span construction | Fully tractable for the source overload | Adapter must supply the exact host-owned output pointer/shape |
| Max signal-outlet construction | Exactly one checked `signal` outlet call per generator is pinned by metadata and SDK-header gates | Binary construction, actual topology, and allocation-failure behavior require a named host |
| Max class/method registration | Null class creation and every nonzero method/public-class registration result fail closed before global class publication; the event dependency registers first | External discovery, binary loading, host error behavior, and actual selector dispatch require a named host |
| Max named-host evidence envelope | Strict observation/record schemas, platform paths, streamed file hashing, root confinement, closed check set, applicability, evidence pairing, digest re-verification, derived completeness, outcome-free residual indexing, inventoried M4L source transformation, run-delimited assertion projection, native assertion/structured/WAVE evaluation, and conditional M4L device/run binding are fully tractable | Host observation production/authentication, capture atomicity, the host-saved/frozen `.amxd`, actual device assertions, and every claimed runtime fact remain external |
| Max release matrix | Exact six-cell target/host product, manifest/record digests, embedded strict records, common revision, per-target archive/binary coherence, and derived aggregate completeness are tractable | Six actual complete host runs, per-record underlying-byte verification, producer authentication, and unrecorded scheduler configurations remain external |
| Pinned standalone `max-test` smoke | Exact upstream revision/database contract, self-start/termination topology, selector coverage, disconnected/reconnected callback-count trace, fresh reconnect output, callback-driven ITM trace, status/output assertions, quiescent raw-row extraction, manifest/database digest agreement, exact 64-assertion set, and 14-check native mapping are closed | Executing the runner in Max and trusting/authenticating its host observation require external work; the four residual outcomes have a separate native evaluator but still need host measurements |
| Bounded message-to-audio control transfer | Producer serialization, fixed 64-command queue, ordered vector-edge application, overflow/error evidence are tractable | Named scheduler/thread stress remains external |
| Max `dsp64` setup-to-`perform64` vector mapping | Represented by the opt-in SDK wrappers and SDK-header compilation | Binary build/load and exact callback observations in a named host |
| Source state across a new SignalBlockContext | Explicitly retained; new rate applies on the next admitted call | Adapter must declare any DSP stop/start reset policy |
| Rational arpeggio onset/duration after gate conversion | Fully tractable with checked arithmetic | None |
| Same-seed random pattern and LFO stream | Fully tractable under the numeric assumption in §2 | None |
| Stable equal-tick ordering | Fully tractable in native callback tests | None |
| Bounded tick-to-sample-offset event planning | Fixed 256-event storage, half-open interval algebra, containing-sample quantisation, ordering, exact-end deferral, and all-or-none output are tractable | A consumer of those source offsets remains external |
| Bounded global-ITM event scheduling | Producer serialization, 64-command/256-event admission, atomic pairs, strict-future checks, injective tick projection, lifetime-paired ITM reference ownership, one-shot grouping, and SDK shape are tractable | Actual permanent scheduling, callback priority, downstream support, discontinuities, and timing require named Max/Live tests |
| Small-block progress without per-block truncation loss | Tractable for the stated recurrence and implementation tests | None |
| Bounded callback-free BlockClock recurrence | O(1), `noexcept`, allocation/lock/I/O/callback-free under exclusive ownership | Honest adapter inputs and ownership still require host integration |
| Local start-of-sample quarter-note signal | Algebra, endpoint equivalence, stopped/paused hold, callback rejection, vector-edge controls, and failure policy are tractable | Named-host buffer/callback observation and deadline measurement |
| Exact real-time duration over unbounded processing | Not established; finite floating boundary and oscillator phase remain | Clock/render measurement |
| Intra-block sample-accurate event delivery | Source offsets and an alternative global-ITM scheduled-message path are represented separately | Named-host proof that all documented Max settings, arrival, downstream, and latency conditions hold |
| Live transport/state correspondence | Not represented by `sunny::render` | Versioned bridge readback in a named Live build |
| Local BlockClock ↔ Max/Live transport phase | Explicitly not inferred from shared BPM/tick vocabulary | Configured host clock path, discontinuity policy, tempo-change tests, named-host evidence |
| Exclusive ownership of each stateful render instance | Fully specified and preserved by the Max control adapters | Named-host producer/lifecycle stress |
| Transport suitability for `perform64` | Explicitly false: callbacks and backlog work are unbounded | Use BlockEventScheduler plus separate bounded producer transfer and supported Max handoff |
| Max scheduler/audio-thread correctness | Source adapter and deployable package shape are represented; runtime scheduling is not observed | Supported-platform binary and named-host tests |
| Audible or signal-level equivalence | Not established | Rendered audio, metrics, and listening criteria |

The strongest current claim is deterministic validated control/event generation, bounded
vector-edge Max modulation/local-clock transfer, SDK-shaped traditional external source, a bounded
callback-free clock and local quarter-note-position signal recurrence, and an explicit
block-quantised higher-level Transport. A fixed-capacity native event scheduler additionally gives
tick events deterministic containing-sample offsets without claiming delivery. The result remains
weaker than named-host DSP/event timing, Max/Live transport correspondence, Max for Live
integration, or audible equivalence.
