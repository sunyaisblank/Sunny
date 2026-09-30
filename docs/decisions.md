# Sunny decision record

This record captures decisions that materially constrain future implementation. Earlier audit
snapshots and component-code registries were removed after their findings were incorporated; Git
history remains the archive.

## 1. Semantic repository topology

**Decision:** Public headers live under `include/sunny/{core,render,infrastructure}`. Implementations
mirror them under `src`, executable entry points live in `apps`, Python lives in `python/sunny`,
the Ableton adapter lives in `remote_script/Sunny`, and tests mirror production under `tests`.

**Reason:** A path must communicate ownership and dependency direction. Opaque component codes,
layer-prefixed directory names, and interleaved tests made navigation and API review unnecessarily
indirect.

**Consequence:** New files use semantic `snake_case` names and `.hpp`/`.cpp` extensions. The build
uses explicit source lists. A feature addition requires placing its API, implementation, and tests
in the corresponding domain.

## 2. Removed subsystems

**Decision:** Remove the incomplete Max/RNBO layer, OSC codec, unused SPSC real-time queue,
duplicate session state machine, and their tests.

**Reason:** None participated in the maintained MCP-to-Live path, several had no deployable peer,
and retaining them implied support that the repository could not verify. The supported real-time
integration is the Ableton Remote Script bridge.

**Consequence:** Reintroducing one of these capabilities requires a concrete user-facing
integration, an owned deployment path, and end-to-end tests. Dead error codes and documentation do
not remain as placeholders.

## 3. Language and build baseline

**Decision:** Build as ISO C++23 with extensions disabled. Treat compiler warnings as errors, use a
single root CMake graph, and maintain one repository formatting policy.

**Reason:** The project is one product with three native libraries, not independent nested builds.
A strict shared policy catches cross-layer mistakes and prevents per-directory compiler drift.

**Consequence:** `sunny::core`, `sunny::render`, and `sunny::infrastructure` are the supported CMake
targets. Dependencies are found locally when available and otherwise fetched at pinned versions.
Generated build and analysis output is ignored.

## 4. Portable exact arithmetic

**Decision:** Exact timing uses portable checked 64-bit rational algorithms. Compiler-specific
`__int128` arithmetic and warning suppressions are not part of the implementation.

**Reason:** The previous code claimed ISO portability while depending on a GCC/Clang extension and
could negate `INT64_MIN` during normalization. Cross-products also hid where overflow was an API
error rather than an invariant failure.

**Consequence:** `Beat` comparison uses continued fractions, products cross-cancel, checked
operations return `ArithmeticOverflow`, and MIDI tick conversion uses an overflow-free saturating
multiply/divide algorithm.

## 5. Trust boundaries and safe states

**Decision:** Runtime input enters domain types through validated factories or deserializers.
Public value objects have safe defaults, and fallible public operations return
`std::expected`-based results.

**Reason:** Raw aliases and indeterminate aggregate members allowed invalid values to travel far
from their source. Boundary validation localizes failures and makes document invariants reviewable.

**Consequence:** Modular pitch behaviour is explicit (`PitchClass::wrapped`); bounded values reject
instead of wrap; document loaders validate enum ranges, rational denominators, IDs, and structural
invariants before returning.

## 6. Resource ownership and history

**Decision:** Stateful resources use explicit RAII ownership. Live undo/redo stores typed forward
and inverse message batches, not type-erased callbacks.

**Reason:** Raw socket descriptors and callbacks capturing an orchestrator obscured lifetimes and
made move/destruction behaviour unsafe.

**Consequence:** `TcpTransport` is non-copyable and non-movable and owns a private socket handle.
Connection attempts use a local candidate and commit it only after success. Orchestrator history is
data, so undo and redo cannot call through a stale object capture.

## 7. Ableton main-thread contract

**Decision:** Socket work and Live Object Model work are separated. The Remote Script parses frames
on its socket thread and executes every LOM access through a scheduled main-thread drain.

**Reason:** Ableton's object model is not safe to manipulate from the network thread.

**Consequence:** The adapter tracks `queued → started → completed`. Its scheduling deadline cancels
only a callback that has not started and leaves that callback inert if Live invokes it later. Once
the main-thread call has started, the adapter waits for its definite result because public LOM has
no cancellation primitive. Network loss, process failure, or the native bounded response timeout
can still make a sent mutation indeterminate; the native delivery state and one-shot deployment
journal preserve that uncertainty and forbid treating it as a safe retry. Clip notes use
dictionary-shaped fields (`pitch`, `start_time`, `duration`, `velocity`, `mute`), compatible with
the supported Live 11/12 adapter paths. The server binds to loopback by default; non-loopback access
requires an explicit `SUNNY_BIND_HOST` setting and external network controls.

## 8. Recoverable Live connection

**Decision:** Live connectivity is optional at process startup and recoverable thereafter.

**Reason:** MCP clients and Ableton may start in either order, and the Remote Script may restart
without the MCP process restarting.

**Consequence:** Offline theory/document tools continue to work. Live-mutating operations decline
clearly while disconnected, then reconnect on demand using bounded nonblocking connection and I/O
timeouts.

## 9. Protocol tests exercise production routes

**Decision:** MCP integration tests dispatch JSON-RPC through `McpServer::process_request` and the
real tool registry. TCP tests use the production frame protocol over loopback.

**Reason:** A duplicated test dispatcher can agree with itself while the executable route remains
broken.

**Consequence:** Test helpers may supply fake domain peers, but they do not replace protocol
routing, schema lookup, transport framing, or orchestrator message generation.

## 10. External validation boundary

**Decision:** Report loopback and contract-test evidence precisely; do not call it a live Ableton
end-to-end result.

**Reason:** The repository can prove framing, reconnect behaviour, scheduling shape, request
translation, and supported note payloads without controlling a user's installed Ableton process.

**Consequence:** A release that claims Live end-to-end validation must record the Live version,
platform, Remote Script installation, exercised operations, and observed results.

## 11. Mix spatial effects are first-class

**Decision:** Mix-stage delay and reverberation are explicit `MixEffectParameters` variants with
bounded parameter domains. Mix schema version 2 introduced both; current schema version 3 adds
per-effect target maps and continues to read versions 1 and 2 without inventing mappings.

**Reason:** Auxiliary buses are specified and exposed as the shared reverb and delay mechanism, but
the previous model could only put unrelated EQ or dynamics values in their effect chains. Tests
therefore used an EQ as a reverb placeholder, making intent, persistence, MCP authoring, and target
device selection disagree.

**Consequence:** `add_channel_effect`, `add_bus_effect`, `add_aux_effect`, and `add_master_effect`
accept `delay` and `reverb`; the Ableton compiler maps them to native Delay and Reverb devices.
Invalid spatial-effect parameters and unknown JSON effect discriminators are rejected rather than
silently converted into a default EQ.

## 12. Ableton target facts precede deployment

**Decision:** Every production Live transport exposes a versioned target-profile handshake. The
profile records the observed Live version, bridge protocol and host-contract stability, and
capability states derived from documented thresholds. The native peer validates the profile rather
than trusting arbitrary feature flags.

**Reason:** Connectivity alone does not distinguish a Live 11 score target from a Live 12.3 native
device target. Late failure can leave a partially mutated set, and inferring Max for Live from the
Live version would conflate product version with licence/edition state.

**Consequence:** The abstract capability model skips `Clip.add_new_notes` below Live 11, while the
current Python adapter itself targets Live 11/12. Pre-12.3 targets skip `Track.insert_device` and
return incomplete compiler results without issuing unsupported calls. Max for Live remains
`unknown`. The Python `_Framework` host is labelled `version_coupled_private`, and its synchronous
test fallback cannot activate inside Live.

## 13. Protocol algebra contains only implemented peer operations

**Decision:** The LOM wire request algebra is synchronous `get`, `set`, and `call` only.

**Reason:** Earlier `observe`/`unobserve` variants and callback IDs had no Remote Script
implementation, no asynchronous event channel, and no consumer. Their presence falsely implied a
realtime observation capability.

**Consequence:** Session perception uses explicit polling. A future observation feature requires a
duplex subscription lifecycle, delivery ordering/backpressure rules, disconnection cleanup, and
end-to-end tests before it enters the public protocol.

## 14. Score identity controls aggregate Ableton targeting

**Decision:** Complete deployment validates Score, Timbre, and Mix together and derives a single
`PartId → Live track index` map from Score Part order. Timbre profiles and Mix channels resolve
through that map; their collection positions have no target semantics.

**Reason:** The formal IRs bind through `PartId`, but the former public compilers were callable only
against isolated MCP stores, and Mix translated channel vector position into Live track position.
A reordered but otherwise correspondent MixGraph could therefore configure the wrong Part while
each individual document remained locally valid.

**Consequence:** Registration groups accept identity-stable stores supplied by `McpSession`.
`project_validate` enforces exact one-to-one correspondence, and
the project compiler derives the whole view before issuing Score → Timbre → Mix mutations. The
result publishes the target map. Guarded plan/apply is specified separately below; public LOM still
does not provide rollback or global idempotence.

## 15. Ableton writes carry target-state evidence

**Decision:** Bridge protocol v4 introduced immediate readback evidence for every generic LOM
property set and every exact-name DeviceParameter write, and returns the existing device count used
to address mapped Mix insertions. Timbre and Mix target mappings declare their IR source interval,
Live target interval, deterministic curve, parameter name, and internal-versus-display property.
Timbre retains an explicit device index; Mix derives the target device from its owning effect and
the observed pre-insert chain length.

**Reason:** A successful setter only proves that the adapter accepted a call. Live may quantise,
constrain, transform, disable, or redirect a parameter, and a device/version-specific internal
range cannot be inferred from an IR value. The former Timbre compiler also incorrectly claimed
that current mapped values did not exist even though its typed path resolver could read them.

**Consequence:** Timbre and Mix mappings resolve and validate before the first target mutation. Internal
DeviceParameter mappings compare the declared target interval with Live's reported min/max before
the parameter write. Mapped Mix chains require an observed mixer-excluding chain count; the exact
inserted address is not caller-supplied or guessed. Score, Timbre, and Mix results retain requested,
observed, and verified values; the current v8 transport that omits or corrupts this evidence fails with
`ProtocolError`. Recording transports retain null observations and cannot claim verification. Mix
also publishes total, explicitly mapped, missing, and non-parameter residual paths. Scalar equality
still does not establish complete parameter coverage or audible equivalence.

## 16. Relative faders are a partial constraint system, not descriptive metadata

**Decision:** Channel and group references define exact additive-dB equations over the complete
channel/group/master fader graph. The graph is solved dependency-first before compilation; missing
references, cycles, non-finite inputs, and results above +12 dB are invalid. An absolute channel
setter clears prior relative intent. `MasterTarget` is classified separately as a rendered-audio
constraint and is never converted directly from LUFS to fader dB.

**Reason:** `RelativeLevel` was persisted and exposed but ignored by the Ableton compiler, which
always deployed `level_db`. Treating `lufs` as a gain would be a different error: programme
loudness depends on source energy, summation, and downstream nonlinear processing. The tractable
and measurement-dependent portions therefore require different proof states.

**Consequence:** Static channel/group relations and their dependants resolve transitively and are
used for Live fader writes. A loudness-relative node is reported as
`requires_loudness_measurement`; its dependants are `blocked_by_unresolved_reference`. Each uses
its stored explicit fallback and makes compilation incomplete. MCP can author channel relations and
inspect the full solution without a Live connection, while invalid mutation remains transactional.

## 17. Ableton project deployment is guarded plan/apply with a mutation journal

**Decision:** Aggregate deployment first canonicalises and validates the project, assembles one
documented-field structural Live observation during a synchronous main-thread call, and dry-runs
the exact Score → Timbre → Mix mutation sequence in an isolated topology-seeded recorder. Apply
consumes the move-only C++ plan object and MCP identifier once, requires exact project and target
equality, compares every mutation with the approved phase and complete wire request before sending,
and retains a post-attempt observation plus journal.

**Reason:** Semantic preflight alone did not control external state. A target could change between
requests; retrying after a lost acknowledgement could duplicate tracks; and the former `Result`
discarded all evidence about successful earlier mutations. Public LOM offers incremental calls but
no rollback or compare-and-swap transaction. Calling a failed mutation “rejected” was also unsound:
the mutation may have completed before readback or transport acknowledgement failed.

**Consequence:** Stale project/selected target structure and internal command drift fail before the
wrong mutation. Journal outcomes are `recorded_only`, `acknowledged`, `declined_before_send`, or
`indeterminate`; partial application remains visible to MCP clients. Bridge protocol v4 adds a
read-only snapshot over documented Song, Track, ClipSlot, Clip, Device, and CuePoint fields. This is
optimistic concurrency control, not a Live lock: external edits after the snapshot, unobserved
same-shape state, rollback, and global idempotence remain explicit residuals requiring a stronger
target-side reconciliation/transaction design and named-build tests.
Scene count is part of the observation and must agree with every track's clip-slot count. Score
deployment reads it again and uses documented `Song.create_scene(0)` before any slot-0 addressing
when the Session is empty.

## 18. Render APIs decline invalid state and state their timing limit

**Decision:** The maintained modulation, arpeggiator, and tick-transport APIs use the shared
`Result` error model at every numeric trust boundary. Random sources have fixed, seedable algorithms;
arpeggiator configuration invalidates locked patterns; octave overflow and empty input are declined;
transport note durations must be integral at PPQ; equal-tick note-offs precede note-ons; and audio
block conversion retains a fractional-tick remainder.

**Reason:** The former surface accepted NaN, negative times, invalid PPQ, and stale arpeggiator
caches; fabricated middle C for empty patterns; used implementation-dependent randomness; computed
release from sustain rather than the captured release level; and truncated every audio block in
isolation. Its header also called a callback API with no sample offset “sample accurate.” Those
behaviours contradicted the repository's decline-not-fabricate and evidence-scoped claims.

**Consequence:** Native and Python failure semantics now agree, seeded operation sequences replay,
and small blocks make cumulative tick progress. The formal render model explicitly classifies
callbacks as block-quantised. A future Max/MSP, Max for Live, RNBO, plug-in, or PCM target must add
its own event-offset, thread, latency, real-time-safety, packaging, host-test, and rendered-signal
contract; the present unit tests are not evidence for those external properties.

## 19. Floating time enters through one checked rational boundary

**Decision:** `Beat::from_float` returns `Result<Beat>`, rejects non-finite input and invalid
denominator bounds, reports magnitude overflow, and uses a checked bounded continued-fraction
approximation. Python Beat construction canonicalises numerator/denominator, rejects zero, and
exposes immutable fields. Native aggregate construction remains an explicitly internal-value
surface validated at document and workflow trust boundaries.

This native-construction portion is superseded by Decision 184; the floating-boundary decision
remains in force.

**Reason:** The former Stern–Brocot walk returned zero for NaN, a small unrelated integer for
infinity or large finite values, and only advanced one integer per iteration—so `120.0` became
`64/1` after its iteration cap. Python could also construct a zero denominator and mutate a valid
Beat into an invalid one, contradicting the exact-rational model.

**Consequence:** Float approximation is fallible and visible at every maintained caller. Values
such as 120 convert exactly, bounded approximations have a specified nearest-candidate rule, and
invalid time cannot enter through the Python extension. Rational arithmetic remains exact after
this one documented approximation boundary.

## 20. Sunny Beat and host beat are different units

**Decision:** `Beat` has one repository-wide unit: a fraction of a whole note. MIDI PPQ and Live
API `beats` use quarter-note coordinates, so every such boundary applies
`host_quarter_notes = 4 × Sunny Beat`. APIs that expose both coordinates name the host value
`quarter_notes`; a bare numeric pass-through is forbidden.

**Reason:** Cycling '74 explicitly defines Live timing beats as quarter notes, and PPQ means pulses
per quarter note. The Score compiler already honoured the factor of four, while render transport,
generic MIDI conversion, and the secondary orchestrator-to-LOM route treated `Beat{1,4}` as one
quarter of a host beat. The latter adapter also replaced the event's mute flag with false.

**Consequence:** Transport positions, scheduled callback events, SMF conversion, legacy Live
operations, and LOM note dictionaries now share the same explicit scale equation and preserve
mute. The formal Score and render specifications use whole-note Beat coordinates consistently.
Tests pin `Beat{1,4}` to 480 ticks at 480 PPQ and to 1.0 Live beat.

## 21. The Live bridge is a closed, versioned operation algebra

**Decision:** Bridge protocol v5 requires its exact version in every request and response. The
Remote Script accepts only canonical `song/...` paths and the finite set of documented operations
needed by Sunny's Score, Timbre, Mix, session-state, target-profile, and target-snapshot models.
Each operation has a checked argument shape and numeric domain. Native responses reject unknown or
contradictory fields and integers that cannot be represented by `LomValue::int`.

**Reason:** The v4 handshake identified the peer, but ordinary messages were unversioned and the
handler traversed arbitrary slash-separated Python attributes before invoking caller-selected
properties or methods. That was a generic Live reflection tunnel, not the capability-preserving
contract described by the compilers. It permitted calls with no Sunny preconditions or result
semantics, accepted relative/non-canonical paths, and allowed oversized JSON integers to narrow at
the C++ boundary.

**Consequence:** The complete external command vocabulary is reviewable against the public LOM
tables and contract-tested without Live. Unsupported Live operations cannot bypass target-profile
preflight through the wire. Adding a capability now requires a Sunny model, explicit path and
argument semantics, protocol advancement, and adversarial tests. This does not make the private
Control Surface host ABI public, authenticate a deliberately non-loopback deployment, make Live
mutations transactional, or substitute for the named-build validation record.

## 22. DeviceParameter equality is not activity or durability

**Decision:** Protocol v6 extends named DeviceParameter evidence with the public LOM `state` and
`automation_state`. The adapter rejects malformed states and state 2 before mutation, then re-reads
both states after the value write. Post-write state 1 and non-zero automation state remain
observable deployment warnings even when requested and immediate observed values compare equal.

**Reason:** `is_enabled` proves that a parameter is writable, not that its value currently affects
sound. Cycling '74 separately defines state 1 as changeable but inactive and exposes automation as
none, active, or overridden. Discarding those properties let an inaudible or automation-displaced
mapping appear complete.

**Consequence:** Scalar `verified` retains its narrow equality meaning. Timbre and Mix records now
carry activity and automation state, and aggregate compilation is incomplete when either condition
prevents a durable active-state claim. This remains state evidence, not rendered-audio or sonic
equivalence.

## 23. Sunny device indices exclude Track's mixer child

**Decision:** Protocol v8 defines every `devices/N` path, structural device snapshot, and device
count over one adapter-level insertable-chain view that removes the object equal to
`Track.mixer_device` from `Track.devices`.

**Reason:** The current Cycling '74 Track reference says that the `devices` child includes the
mixer device, while `Track.insert_device` addresses the insertable device chain. Raw list length
and index were therefore not sound evidence for a newly appended Mix device and could redirect a
parameter mapping.

**Consequence:** Snapshot preconditions, pre-insert counts, and named parameter paths share the same
index domain. Test doubles include the mixer at an adversarial list position. A named Live build
must still validate equality semantics of the private Python wrapper; failure to identify one
produces divergent snapshot/count evidence rather than a claim of public host certification.

## 24. Return-track addresses require observed target state

**Decision:** Mix compilation requires an observed existing return-track count whenever at least
one AuxBus will be materialised. An unavailable count is a pre-mutation `ProtocolError`; it is not
interpreted as zero. A graph without AuxBuses does not read this irrelevant target fact.

**Reason:** `Song.create_return_track` appends to the existing return collection, and channel sends
are addressed by that collection's index. Guessing an empty collection could rename or configure
an existing return and route a send to the wrong bus while reporting success. Conversely, making
an unrelated count read mandatory for a graph with no AuxBus would add a needless failure mode.

**Consequence:** Return creation and mapped device insertion now use the same evidence rule: if
current target structure determines a downstream path, Sunny observes it before the first mutation
or declines the compile. Unit tests prove both no-mutation failure on missing evidence and no
unnecessary state dependency when no return address is needed.

## 25. Missing target profiles grant no capabilities

**Decision:** Score, Timbre, and Mix compilation requires a valid target profile before the first
mutation. Successful compilation results carry that profile as a non-optional value. Offline
recorders compile against an explicit modelled target; a null profile is `ProtocolError` rather
than an implicit latest-version profile.

**Reason:** The transport contract defined null as “cannot probe,” but the compilers interpreted it
as permission for Live 11 `Clip.add_new_notes` and Live 12.3 `Track.insert_device`. This inverted the
meaning of missing evidence and allowed a transport with no version proof to bypass the capability
boundary entirely.

**Consequence:** Capability decisions are now always attributable to a concrete observed or
explicitly modelled Live version and bridge contract. Adversarial tests for all three compilers
prove that missing or internally contradictory profile evidence emits no target command, even when
a custom in-memory transport bypasses the wire parser. Older observed targets still preserve
unsupported intent and return explicit incomplete results as before.

## 26. Live addresses are a finite type boundary

**Decision:** Every compiler-derived Live collection address must be representable as a canonical
non-negative signed protocol index. Score preflights its Part ordinal range, Timbre preflights the
complete materialised device chain, and Mix preflights standalone channel and observed return
ranges before mutation. Error 4111 (`TargetAddressUnrepresentable`) names this boundary. Operation
summary counters use 64-bit unsigned values and are never reused as target indices.

**Reason:** C++ collection positions are `size_t`, while the closed bridge algebra deliberately
uses signed `int` values and canonical decimal paths. Mix previously computed
`base_track + static_cast<int>(ordinal)` and incremented a signed return index after using
`INT_MAX`; either operation could overflow or redirect a path. Separately, the formal Mix spec
called unknown AuxBus references warnings even though validation rule X6 already rejected them.

**Consequence:** Address algebra is checked as one preflight proof rather than discovered during
partial deployment. The largest signed return index remains usable for one return without a
terminal overflow, a second return is declined before any command, and invalid AuxBus references
are consistently graph errors. Tests cover all reachable boundary cases.

## 27. Requested automation is not deployment evidence

**Decision:** Timbre and Mix compiler results expose separate 64-bit
`automation_lanes_requested` and `automation_lanes_written` counters. The current public-LOM target
sets the first from the preserved IR and the second to zero. The formerly ambiguous
`automation_lanes` field is removed from native and MCP result contracts.

**Reason:** A zero called only “automation lanes” admits two incompatible interpretations: no lane
was requested, or requested lanes were not deployed. A warning partially disambiguates that state
for a human but does not give downstream software structural evidence. It also obscures the exact
amount of intent retained at the capability boundary.

**Consequence:** Callers can mechanically distinguish absence, preserved-but-unsupported intent,
and future successful authoring without deriving state from warning prose. Unit tests cover the
unsupported compiler result and MCP aggregate output rejects the obsolete ambiguous field.

## 28. Target-independent metres are narrowed only at the Live boundary

**Decision:** Score compilation requires its initial time signature to have a numerator in 1–99
and a denominator in {1, 2, 4, 8, 16}, the domain documented by Live. Failure returns 4112
(`TargetValueUnrepresentable`) before the compiler reads target state or emits a mutation. The
Remote Script request validator and native target-snapshot parser enforce the same domain. Sunny's
core time-signature model remains broader, and later meter events remain preserved as unsupported
automation.

**Reason:** A target-independent score may legitimately use a finer power-of-two denominator, but
Live cannot accept every such value in its current meter fields. The compiler previously created a
scene and could write tempo before Live rejected the signature, turning a statically decidable
domain mismatch into partial deployment. Snapshot evidence accepted the same impossible values.

**Consequence:** Target representability is now proved before target access, invalid peer evidence
cannot enter a guarded plan, and the bridge cannot be used to bypass the compiler's value subset.
Adversarial tests cover numerator 100 and denominator 32 at compiler, wire-policy, and snapshot
trust boundaries.

## 29. Path parsing cannot canonicalise hostile spellings

**Decision:** `LomPath::parse` preserves every slash-delimited segment, including empty segments.
`LomPath::is_canonical` defines the native ASCII syntax, and CommandBuffer/TCP transports reject a
noncanonical path with delivery state `not_sent`. The Python peer recognises indices with explicit
ASCII comparisons rather than Unicode `str.isdigit`; both peers cap the lexical value at signed
`INT_MAX` without converting attacker-sized decimal text.

**Reason:** The former parser dropped empty segments, so `/song/tracks/0` and
`song//tracks/0` became the admitted `song/tracks/0` before protocol validation. Python's Unicode
digit predicate also allowed more than one textual encoding of an index. That contradicted the
versioned contract's claim that paths have one canonical spelling. An unbounded digit string also
contradicted its finite signed index domain and weakened mutation-journal delivery evidence.

**Consequence:** Invalid spelling is never laundered into a valid address, offline recording has
the same path-syntax rule as network delivery, and local declines are provably unsent. The Live
peer still independently enforces the narrower operation/path-shape allowlist before traversal.

## 30. The Python peer does not inherit native integer bounds

**Decision:** Every scene, track, and device insertion index in a raw bridge request is checked by
one Python predicate against the protocol's signed integer domain. `create_scene` and
`create_midi_track` additionally admit the documented `-1` append sentinel; path and device
indices do not.

**Reason:** C++ `LomValue::int` makes generated requests finite, but the loopback Python server is
also a wire trust boundary and JSON integers in Python have arbitrary precision. Depending on the
Live host to reject an oversized index violates the closed request algebra and performs avoidable
object lookup.

**Consequence:** Native-generated and direct wire requests now inhabit the same index domain, and
`INT_MAX + 1` is rejected before LOM traversal for both structural and device insertion calls.

## 31. Articulation mappings are executable tagged unions with target-specific evidence

**Decision:** `ArticulationMapping` is a bounded, validated tagged union. A present mapping
replaces the compiler's default articulation stage; a missing mapping retains the documented
fallback. `Combined` executes children in order. Compilation carries keyswitch, CC, and program
events as typed data, applies velocity and duration transforms to notes, deduplicates identical
same-tick controls, and records mapped-versus-defaulted counts. Core channels are 1–16; the SMF
converter alone translates them to wire channels 0–15.

**Reason:** The persisted model previously promised six strategies while compilation ignored the
map and even ignored explicit numeric/written velocity evidence. The generic MIDI file layer then
dropped CC, program-change, and key-signature data. Meanwhile, treating any non-empty mapping as
unsupported in Live falsely described velocity and duration changes that were already deployable
as ordinary clip-note fields.

**Consequence:** C++, canonical JSON, undoable MCP authoring, compiled MIDI JSON, and SMF now share
one mapping algebra and validation domain. The public Live target deploys transformed musical
notes, but reports typed articulation control intent separately as requested and written; it does
not fabricate ordered keyswitch/CC/program deployment. A future Max or Live adapter must consume
the same typed events and prove its ordering and host semantics rather than reinterpret the IR.

The end-to-end authoring proof also made `insert_note` transactional over measure topology: it
carves covered rests, merges only exact coincident note spans, and rejects partial overlap or
out-of-measure ranges. Thus the MCP operation no longer makes a newly created score uncompilable
merely by adding its first note.

## 32. Grace notation and performed timing are separate, explicit projections

**Decision:** A grace NoteGroup reserves its positive written duration in Score topology and must
be homogeneous in grace state and type. Appoggiaturas sound across the full allocation.
Acciaccaturas sound for at most `Beat(1,32)` and are right-aligned inside it. Grace ties are
invalid. One shared resolver feeds typed MIDI, NoteEvent projection, SMF, and Ableton compilation;
notation compilers preserve symbolic grace markup without claiming playback equivalence.

**Reason:** The formal model said that compilation resolved grace timing, but MIDI treated grace
notes as ordinary notes while the generic NoteEvent path deleted them. MusicXML and LilyPond used
their symbolic grace constructs, whose playback conventions are target-owned. Leaving those three
behaviours unnamed made both internal timing and Live deployment ambiguous. A pre-beat policy that
generated negative clip coordinates would also violate Sunny's closed bridge subset even though
Live's public note dictionary documents floating start times without granting Sunny a portable
negative-time guarantee.

**Consequence:** The exact policy is statically testable and never creates negative Live note
coordinates. Structural rule S17 rejects groups for which one shared onset/duration policy cannot
exist. Rendering rule R4 now implements its documented comparison with the following ordinary
note rather than an unrelated fixed threshold. Contract tests prove the resolved coordinates at
the core and Live-command boundaries. Host-specific notation playback remains an explicit
unverified property.

## 33. Structural warnings are not compilation failures

**Decision:** `is_compilable` means that `validate_structural` contains no Error diagnostic, not
that the diagnostic vector is empty. Structural warnings remain visible in `validate_score` and
make its separate `valid` summary false, but do not block target-independent compilation.

**Reason:** S14 deliberately classifies stale NonChordTone analysis references as warnings, and
S16 does the same for irrelevant role-specific orchestration fields. The old predicate rejected
both, contradicting their severity and the documented compiler precondition. That also made
adding harmless advisory evidence capable of disabling MIDI and notation output.

**Consequence:** Invalid Beam/Tuplet references, missing required orchestration fields, and all
other structural Errors still block. Advisory S14/S16 cases compile and retain their diagnostics.
Tests assert both the warning and successful MIDI compilation so severity cannot silently become
target policy again.

## 34. Measure fill is interval topology, not a duration checksum

**Decision:** S2 validates the ordered positive-duration NoteGroup/Rest intervals as an exact
tiling of `[0, measure_duration)`. It independently checks every event offset and measured end
against the measure domain. S3 checks container order and overlap between measured intervals.
Zero-duration ChordSymbol/Direction events remain point events: they do not fill time and may lie
inside a sounding span.

**Reason:** Summing durations can equal a bar while the actual offsets contain a leading gap and
an overrun. The former S2 accepted that document; S3 only compared adjacent container entries and
also treated a point direction inside a note as a duration overlap. Both contradicted the formal
event interval and “fills exactly” invariants, and let deserialised scores bypass the stricter
mutation topology.

**Consequence:** Authoring, loading, compilation preflight, and the formal model now use the same
measure geometry. Non-positive measured durations, gaps, overruns, out-of-range point events,
unordered containers, and measured overlap block compilation. Point metadata remains expressible
without inventing rhythmic duration.

## 35. Tie adjacency is measured-event adjacency

**Decision:** S7 searches the same voice for the next positive-duration NoteGroup/Rest across
measure boundaries. A point ChordSymbol or Direction may occur between the tied notes without
breaking the tie. A Rest or a NoteGroup without the pitch does break it.

**Reason:** The Event algebra explicitly includes zero-duration performance directions attached
to time points. Treating the next vector element as the tie target made harmless point metadata
invalidate an otherwise contiguous tie, while the MIDI compiler independently projected only
NoteGroups. Validator and compiler therefore disagreed about adjacency.

**Consequence:** The structural predicate and MIDI tie-chain projection share the measured-event
sequence. Tests place a Direction at the join and prove that one continuous MIDI note is emitted;
rests and pitch mismatches remain blocking S7 errors.

## 36. Container position cannot disagree with musical identity

**Decision:** For every Part, `measures[i].bar_number == i + 1`. Within a Measure, Voice indices
are unique and strictly increasing, though they may be sparse after removal. `add_voice` inserts at
the canonical ordered position instead of appending blindly.

**Reason:** Compilers traverse Measure vectors but calculate absolute time from `bar_number`; a
duplicated or reordered number could therefore duplicate, exchange, or reorder target notes while
S1 still passed because the vector length was correct. Musical validation also compared adjacent
Voice vector entries while describing them as voice-index order. Duplicate or unsorted indices
made that analysis and cross-measure tie lookup ambiguous.

**Consequence:** Loading and compilation reject contradictory container identities before target
translation. Mutation output preserves the invariant for arbitrary new voice numbers. Tests cover
duplicate/reordered measures and Voices plus insertion of voices 2 then 1.

## 37. Target clips contain sounding output, not only written bars

**Decision:** Ableton clip length is the maximum of structural score length and every compiled
musical note end. MIDI compilation rejects an unrepresentable `start_tick + duration_ticks` before
publishing the event.

**Reason:** Fermatas and custom duration mappings may extend the final note beyond its written
allocation. A clip sized only from time-signature bars could accept the note dictionary yet place
part of the intended sounding interval beyond the clip's playback span. That is a target-level
loss caused by a core rendering decision.

**Consequence:** The Ableton boundary consumes the completed MIDI timing model when sizing clips.
A one-bar whole-note fermata yields both a seven-quarter-note note and a seven-quarter-note clip;
ordinary scores retain their structural length.

## 38. Notation metadata must render or fail visibly at the target boundary

**Decision:** The MusicXML compiler consumes the first-class `Note.ornament` and
`Note.technical` fields. It uses native MusicXML 4.0 elements when Sunny has enough state for a
lossless mapping. Otherwise it emits `other-technical` containing the preserved intent and adds a
compilation diagnostic. A rich `Ornament.Trill` takes precedence over the legacy
`Articulation.Trill` on the same note.

**Reason:** Both fields were serialised and promised by the Score IR but ignored by the exporter.
That made round-trip persistence look successful while the external notation boundary silently
discarded authored state. Some apparent native mappings are also false equivalences: MusicXML
hammer-on, pull-off, and slide elements require start/stop linkage that Sunny's current technical
variant does not carry.

**Consequence:** Trills (including supported interval and accidental detail), mordents, turns,
shakes, arpeggios, fingerings, string numbers, bends, breath marks, and caesuras now reach native
MusicXML elements. Missing linkage or nonstandard technical semantics remain observable in both
the XML extension and the compilation report; the compiler never fabricates a paired gesture.

## 39. MusicXML structure must preserve both notation and sound semantics

**Decision:** The MusicXML compiler emits every per-Note persisted notation field in MusicXML's
schema order. It distinguishes slashed and unslashed grace notes, emits sounding cue-sized notes
with `type size="cue"`, preserves lyrics and native notehead shapes, derives both sound and visual
tie stops from validated adjacency, and emits one graphical tuplet start/stop pair while retaining
time-modification on every member.

**Reason:** Well-formed XML was too weak an external-alignment criterion. The previous compiler
placed sound ties after `voice` and `type`, emitted starts without destination stops, repeated a
tuplet start on every member, used the sounding tuplet duration as a fallback graphical note type,
and silently dropped lyric/notehead data. A MusicXML consumer could therefore receive parseable XML
that contradicted both the W3C grammar and Sunny's persisted model.

**Consequence:** The XML child sequence, endpoint counts, tuplet written type, grace slash, lyric
escaping, notehead values, and cue-size/non-silence distinction are contract-tested. S7 also
rejects an ordinary duration tie whose apparent matching destination is a grace note, so target
compilers never need to encode a semantically impossible tie endpoint.

## 40. Point metadata is placed against the MusicXML cursor explicitly

**Decision:** The MusicXML compiler maintains an exact per-Voice cursor in divisions. Direction
and Harmony points use signed offsets from that cursor; global tempo, rehearsal, and hairpin points
use absolute offsets from the measure start and are emitted once. Clef and barline points
temporarily reposition with positive backup/forward pairs because those MusicXML nodes have no
offset child. Grace allocations advance with Voice-scoped forward elements.

**Reason:** Score IR permits zero-duration metadata inside a measured Note/Rest span. Emitting a
point after the containing note without an offset places it at the note's end, not at its stored
offset. The former compiler also emitted only downbeat TempoMap events, emitted non-downbeat
hairpins only when some NoteGroup happened to start there, and could duplicate a Part hairpin for
multiple Voices. Grace notes advanced no MusicXML cursor at all despite owning positive structural
allocation.

**Consequence:** Contract tests place directions, harmony, tempo changes, hairpin endpoints, clef
changes, repeats, and grace allocations adversarially inside a whole-measure Rest and inspect the
exact offset or cursor-restoration evidence. At this decision point Roman/extensions remained
visible only through MusicXML kind text with a lossy-semantic diagnostic; Decision 231 supersedes
that limitation with a typed numeral/degree path while retaining the legacy residual explicitly.

## 41. Direction variants own a closed conditional payload

**Decision:** Structural rule S19 makes `ScoreDirection`'s compact programmed representation a
tagged payload contract. Text variants require non-empty text, ClefChange requires `new_clef`, and
OttavaStart requires exactly ±12 or ±24 semitones. Fields populated outside their owning variant
are warnings and do not block compilation.

**Reason:** Optional aggregate fields made it possible to persist a direction whose discriminant
promised information that was absent, such as a ClefChange without a clef or an OttavaStart with
zero/seven semitones. Exporters then had to fabricate, silently ignore, or emit invalid target
state. Conversely, stale irrelevant fields do not change the selected variant and should not turn
an otherwise defined document into a compilation failure.

**Consequence:** Construction/loading and every compiler share a target-independent payload
precondition. MusicXML maps ±12 semitones to octave-shift size 8 and ±24 to size 15, with direction
derived from the sign; malformed values never reach target compilation. Tests distinguish the
blocking missing/invalid cases from three advisory irrelevant-field cases.

## 42. LilyPond projection uses exact synchronized notation layers

**Decision:** The LilyPond compiler retains arbitrary rational durations with exact multipliers,
renders hidden rests as spacers, keeps per-note chord attachments inside the chord construct, and
places zero-duration metadata in a synchronized exact-duration spacer voice. Native LilyPond 2.24
constructs are used where Sunny carries enough state; a visible fallback and compilation
diagnostic are emitted where it does not. Hairpin targets occur at their stored endpoints.

**Reason:** The former exporter approximated uncommon durations, printed hidden rests, attached
some per-note state at chord scope, and moved metadata inside a sustained event to the next event
boundary. It also silently discarded several persisted articulation, ornament, technical, lyric,
notehead, harmony, tempo, and grouping distinctions. Syntactically plausible source was therefore
not evidence that the engraving corresponded to Score IR.

**Consequence:** Contract tests inspect exact duration multipliers, spacer rests, chord-local
attachments, grace allocations, point ordering, beam brackets, and explicit fallback evidence.
Global tempo/rehearsal state is emitted only once, while Part and Voice metadata remains owned by
its Staff. Fields outside LilyPond's algebra remain observable rather than being silently erased.

## 43. Stored key and meter state is authoritative at notation boundaries

**Decision:** MusicXML emits the stored `KeySignature.accidentals` as `fifths` and maps its native
mode vocabulary directly; LilyPond maps major, minor, and the seven church modes directly. A
mid-measure global key is placed at its exact cursor position. Unequal meter groups use each
target's additive-meter construct, and a local one-measure key or meter override is followed by an
explicit reset whenever the next measure's effective global state differs.

**Reason:** Independently recomputing fifths at each exporter could contradict persisted state,
while treating every non-major scale as minor falsified modal intent. Standard key triples are
now proved coherent by S21; custom scales still need an independently stored signature because
their scale algebra is not statically known. Pulling a later key change
back to a measure downbeat changed its time. Summing `[3,2]` to an undifferentiated five erased
meter grouping, and omitting the reset allowed a local override to leak into following measures.

**Consequence:** Adversarial tests use a custom stored signature that cannot be inferred safely
from the tonic, a coherent Dorian scale, a non-downbeat key, a 3+2 meter, and a local meter reset. Unsupported scale
algebras retain target-valid output and produce a diagnostic instead of being mislabeled.

## 44. Tuplet nesting is a cumulative context graph

**Decision:** A NoteGroup or Rest stores its innermost TupletContext. The `nested_in` chain is
acyclic and resolves outward. A context's structural span is its local `normal × normal_type`
multiplied by every ancestor's `normal / actual` scale; the stored durations of all descendant
events sum to that span. Direct events plus direct child contexts equal `actual`, and all members
are contiguous. RestEvent therefore carries the same optional context as NoteGroup.

**Reason:** The model claimed nested tuplets while each exporter tracked only one active ID and S8
summed only directly tagged NoteGroups. A child could not satisfy both its own local span and its
parent's flattened span, and a rest could not participate at all. MusicXML 4 explicitly separates
the cumulative sounding ratio in `time-modification` from numbered graphical tuplet levels;
LilyPond 2.24 expresses the same structure with nested `\tuplet` blocks.

**Consequence:** MusicXML emits a reduced cumulative ratio (for example 9:4 for nested 3:2 in 3:2)
and numbered start/stop boundaries for every active level. LilyPond opens and closes the exact
context-chain suffix and uses the innermost written duration for notes or rests. Serialization and
mutations preserve rest contexts, while S8 blocks cycles, conflicts, zero ratios, wrong counts,
noncontiguous membership, and incorrect cumulative spans before compilation.

## 45. Part spans are validated once and projected according to target capability

**Decision:** S20 validates Hairpin and PartDirective as non-empty half-open spans in the owning
Part's effective meter, permitting only the canonical score endpoint `(total_bars + 1, 0)`.
Hairpins may not overlap. Divisi requires a count of at least two; an irrelevant count is advisory.
Notation targets emit exact visible start/end directive evidence, while Ableton retains an
explicit non-deployment warning instead of fabricating temporal Track automation or instrument
controls. Hairpin velocity interpolation uses cumulative exact Beat time, and a missing target
means one continuous dynamic-ladder step.

**Reason:** The mutation APIs accepted inverted, empty, out-of-score, and ambiguous overlapping
spans, while the formal text incorrectly claimed those values were validated. Hairpin interpolation
treated every bar as one whole note, so mixed meter changed the intended curve. A targetless
hairpin was documented as a one-step change but performed no change. Part directives survived JSON
yet disappeared from both notation targets, and their Ableton warning did not distinguish visible
notation preservation from unavailable scoped host deployment.

**Consequence:** Construction and compilation share one blocking span contract. Tests cover the
terminal endpoint, mixed 3/4→4/4 interpolation, an implicit mf→f crescendo, overlapping spans,
Divisi payloads, and exact MusicXML/LilyPond directive offsets. Ableton continues to expose the
unsupported playback boundary rather than issuing fictional Live Object Model requests.

## 46. Only standardized state-complete pedal ranges become MIDI controls

**Decision:** SustainingPedal compiles to MIDI CC64 and UnaCorda to CC67, using value 127 at the
PartDirective start and 0 at its end. All other directives produce compilation evidence rather
than an invented MIDI event. In particular, HalfPedal is not narrowed to MIDI 1.0's binary damper
switch, and TreCorde does not synthesize an unknown prior soft-pedal state.

**Reason:** The MIDI Association defines CC64 Damper and CC67 Soft Pedal as off at values ≤63 and
on at values ≥64. These two half-open ranges therefore have an exact channel-message projection.
It does not define a continuous half-pedal depth in that switch contract, while most instrument
techniques need a configured sampler/device mapping. Ableton can contain MIDI-controller clip
envelopes, but Sunny's reviewed public LOM subset has no admitted authoring operation for them.

**Consequence:** SMF receives deterministic start/end pedal controls and the typed MIDI model
retains their Part identity. Ableton reports two requested and zero written control events for one
pedal range, plus the PartDirective capability warning. No raw LOM or Max operation is fabricated;
a future envelope/Max adapter must consume the same typed events and prove its own ordering and
write/readback contract.

## 47. Standard key identity is closed before target projection

**Decision:** S21 derives the expected fifths count for major, minor, and the seven church modes
from tonic spelling and rejects a contradictory stored `accidentals` value. `KeySignature`
identity includes tonic spelling, fifths, and the complete stored scale definition, so a
mode-only change cannot be mistaken for unchanged state. Custom/unrecognized scales retain an
independent fifths count because the model has no general derivation for their interval algebra.

**Reason:** MusicXML projected stored fifths while LilyPond projected tonic and mode. A malformed
standard triple could therefore produce two different keys from one Score IR, and the former
equality operator ignored mode entirely when deciding whether a local override needed a reset.
That violated the single-source-of-truth claim upstream of both exporters.

**Consequence:** Standard keys either validate once and project consistently or are blocked before
compilation. D Dorian with zero fifths is accepted; C major with one sharp is rejected. Custom
scales remain expressible, and their target-specific semantic limits continue to produce explicit
compilation evidence.

## 48. Staff topology is explicit notation ownership, not DAW routing

**Decision:** A Voice stores a 0-based `staff_index`; a Part stores positive `staff_count` and an
optional complete top-to-bottom `staff_clefs` vector. S22 rejects impossible assignments or partial
clef vectors. MusicXML projects this state as `<staves>`, numbered clefs, and 1-based `<staff>`
children. LilyPond projects it as ordered Staff contexts inside a `PianoStaff`. An empty clef vector
means the existing default clef applies to every staff; no piano/bass heuristic is inferred.

**Reason:** `staff_count` previously survived JSON but both notation exporters rendered exactly one
staff. Voice index is polyphonic identity, not staff ownership, so assigning voices by index would
have invented a convention. MusicXML explicitly requires staff assignment for multi-staff notes
and directions and numbers staves from top to bottom; LilyPond requires distinct notation contexts.
Those target contracts expose the missing source-model fields and validation rule.

**Consequence:** Piano reductions and short scores now assign treble and bass Voices explicitly and
carry differentiated clefs. Creation and measure insertion materialize one resting Voice per
configured staff; an explicit add-Voice mutation accepts staff ownership. MIDI and Ableton continue
to merge all Voices of one Part into the Part's playback stream: staff layout neither creates a
Live track nor changes `PartId` routing. Mid-measure cross-staff movement remains outside the model
and is stated rather than approximated.

## 49. Boolean notation endpoints define a validated single-identity profile

**Decision:** S23 treats `NoteGroup.slur_start/slur_end` and legacy glissando articulations as one
active span per Voice, Ottava Direction points as one active state per Staff, and Pedal points as
one physical state per Part.
Orphan stops, overlapping starts, empty spans, incomplete ottavas, concurrent chord glissandi, and
ambiguous same-time endpoints are structural errors. A terminal unmatched PedalDown alone is valid
and means sustain through the final bar line. If Direction points and a SustainingPedal
PartDirective coexist, their ranges must match exactly. On a shared slur note, stop precedes start.

**Reason:** MusicXML uses numbered identities to distinguish overlapping slurs, glissandi, pedals,
and octave shifts; Sunny's booleans and unnumbered Directions cannot reconstruct those identities.
Both exporters previously printed arbitrary orphan endpoints, and emitted a chained slur's start
before its stop despite MusicXML requiring incoming score order. LilyPond likewise needs labels for
overlapping regular slurs, while it explicitly permits final pedaling to omit sustain-off.

**Consequence:** Every accepted span has the same pairing interpretation in MusicXML and LilyPond,
and default target identity 1 is sufficient. Chained slurs emit stop/start in that order. More
general overlapping or chordal span notation now has a precise model-extension requirement—stable
span IDs and endpoint numbers—instead of silent target-dependent guessing.

## 50. Damper-pedal notation and playback share one Part-scoped range algebra

**Decision:** PedalDown/PedalUp Direction pairs denote the same Part-scoped sustain ranges as
SustainingPedal PartDirectives. If both representations are present S23 requires exact range
equality. MIDI emits CC64 on/off for either representation and deduplicates exact duplicates. A
terminal open Direction range is notated through the final bar line and receives a safe CC64-off at
the MIDI score endpoint.

**Reason:** Direction pedal points were preserved by MusicXML/LilyPond but silently ignored by the
MIDI and Ableton paths, while the independently stored PartDirective form produced CC64. Treating
the visually anchored mark as Staff-local would also be false for one physical piano damper pedal
and impossible to express on the Part's single MIDI channel. Two source representations of the
same state must either agree or be rejected before target projection.

**Consequence:** A written sustain mark now affects SMF playback and enters Ableton's explicit
requested-but-not-writable control count. MusicXML retains the visual Staff anchor, LilyPond emits
the mark in that Staff, and playback applies to the whole Part. Conflicting duplicate ranges block
compilation rather than generate order-dependent CC state.

## 51. Tempo transitions are incoming exact relationships with bounded target projections

**Decision:** A TempoEvent's transition describes how the preceding event reaches that destination.
S24 requires the initial event at the exact score origin to be Immediate, closes every positive
rate and enum payload, requires a Linear duration to equal the complete preceding AbsoluteBeat
interval, and verifies MetricModulation by exact effective-quarter rational algebra. RealTime and
the exact instantaneous-tempo query use the same incoming interpretation. RegionView materialises
the instantaneous rate at a cut as a new Immediate origin and shortens an intersected ramp.

SMF samples a Linear curve deterministically on the tick lattice with a stated held-step bound,
the 24-bit tempo-word error remains explicit, and ramps exceeding a 4096-event bound are refused.
MusicXML and LilyPond place the transition label at the source point and the target mark at the
destination. Ableton sets only the initial effective-quarter Song.tempo after a 20–999 preflight;
source requested/written counts and a capability warning preserve every later event/transition.

The rate-positivity validation portion is superseded by Decision 185: positivity and canonical
fraction identity are now enforced before a `PositiveRational` exists. The incoming-transition and
target-projection decisions remain in force.

**Reason:** The specification and type comment already said “previous tempo changes to this one,”
but the clock integrator treated Linear as outgoing, notation labelled its endpoint as its start,
SMF collapsed it to two steps, RegionView could orphan it, and Ableton wrote raw marked-unit BPM as
though every rate were quarter-note based. Cycling '74 documents Song.tempo as the current
automatable scalar but exposes no tempo-envelope authoring operation in the admitted LOM surface;
MusicXML `<sound tempo>` and MIDI Set Tempo are point values. Target product capability therefore
cannot be substituted for an API operation that Sunny does not possess.

**Consequence:** One validated TempoMap now yields one clock function and traceable target-specific
loss. Half-note = 60 deploys as Live quarter tempo 120; a linear ramp has exact source semantics,
bounded MIDI approximation, truthful notation anchors, and explicit Live non-deployment. Hidden
holds, discontinuities, malformed metric equations, late origins, and unbounded event generation
block before target mutation.

## 52. Mutations are closed over measure topology and endpoint identity

**Decision:** A successful mutation on a valid Score IR must preserve the exact measured-event
partition and S23 endpoint algebra. Fixed-onset duration growth may consume only Rest coverage;
shrinkage materialises Rest coverage. Copy preserves a slur/glissando only when both source
endpoints are selected. Delete cascades to the paired endpoint. Retrograde and cross-Part copying
remove spans whose identity cannot be transformed and return a `MUT1` warning. Candidate-based
operations commit only after the complete destination topology is proved.

**Reason:** Raw duration changes, appending copied events, and deleting one Boolean endpoint each
produced documents that the compiler then had to reject. The model cannot claim internal alignment
if its own public edit algebra is not closed over its structural invariants. Nor can it invent a
cross-Part or reversed span identity absent stable source IDs.

**Consequence:** Successful edits remain compilable by construction for their affected topology;
musical collisions fail without changing the score. Process-monotonic EventId allocation may leave
gaps after a rejected candidate, but document state, version, and undo history do not change.

## 53. Meter and map coordinates are checked before temporal arithmetic

**Decision:** S5 validates positive groups, representable numerator, and a power-of-two denominator
for global and measure-local time signatures. Exact-duration conversion is fallible at trust
boundaries. S4 and S6 require every tempo/key point to lie inside its active global measure and both
maps to begin at the exact score origin. A meter mutation retiles derived Rest coverage through the
next meter change, rejects clipped music or point events, and recomputes every affected incoming
Linear tempo duration.

**Reason:** Public aggregate structs permit malformed raw values. Validation previously called
`measure_duration()` before establishing the meter domain, so hostile input could bypass a typed
diagnostic. A meter setter also changed the map without changing voice tiling or ramp interval
lengths, making a successful edit structurally contradictory.

**Consequence:** Malformed meters return `InvalidTimeSignature` or S5 instead of entering unchecked
rational arithmetic. A 4/4-to-3/4 edit trims only trailing rests; a whole-note event that would be
clipped rejects atomically. Tempo, key, and meter now share one checked coordinate interpretation
before any exporter or Ableton deployment sees them.

## 54. BeamGroup is an explicit primary-beam profile, not incomplete secondary notation

**Decision:** S14 defines BeamGroup as one explicit primary beam containing at least two unique,
score-ordered, contiguous measured events in one Voice and Measure. Eligibility uses written
duration (the innermost tuplet `normal_type` when present), and every NoteGroup has an exact local
back-reference. IDs are globally unique. `beam_breaks` is reserved but non-compilable until the
model stores explicit beam level and left/right or hook state. Mutations that cannot transfer or
retain local beam identity remove the complete group and return `MUT1`.

**Reason:** MusicXML associates each note with one of five states at each of eight numbered beam
levels, including directional hooks ([MusicXML `<beam>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/beam/)).
LilyPond manual `[`/`]` delimiters define the primary extent, while exact secondary control uses
per-stem `stemLeftBeamCount` and `stemRightBeamCount`
([LilyPond manual beams](https://lilypond.org/doc/v2.25/Documentation/notation/beams.html#manual-beams)).
A Sunny boundary index supplies neither algebra. It also previously allowed dangling references,
unordered or skipped members, one-event groups, quarter-note tuplets, and destination clones whose
IDs did not belong to any local group; the exporters could consequently widen or invent a beam.

**Consequence:** Every compilable BeamGroup now maps exactly to MusicXML beam-number 1
begin/continue/end and LilyPond manual primary brackets. Secondary beams remain target defaults
inside that exact extent, never falsely claimed source state. Copy, cross-Part transfer,
retrograde, insertion through a group, and duration scaling expose any removed source identity in
their mutation result, and candidate commits cannot leave a dangling beam graph.

## 55. Lyrics are verse-lane state, not note text

**Decision:** Schema version 4 replaces `Note.lyric: Option<String>` with ordered
`Note.lyrics: Vec<LyricSyllable>`. Each syllable stores non-empty text, a positive verse number,
`Single|Begin|Middle|End` word position, and whether it extends across following ordinary
NoteGroups. S26 gives the lane `(Part, staff, Voice, verse)` a closed word-state machine, requires
first-note chord ownership and canonical verse ordering, excludes grace attachment, and requires
each extender to reach a later note. Versions 1–3 migrate their string to verse 1 `Single`.

**Reason:** MusicXML lyric underlay natively distinguishes verse `number`, four syllabic values,
and extender state
([MusicXML `<lyric>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/lyric/),
[MusicXML `<syllabic>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/syllabic/)).
LilyPond aligns each Lyrics context to a named melodic voice and gives hyphens, skipped notes, and
extenders distinct `--`, `_`, and `__` syntax
([LilyPond extenders and hyphens](https://lilypond.org/doc/v2.25/Documentation/notation/extenders-and-hyphens)).
A string attached independently to every chord pitch could not identify a verse, prove word
continuity, locate a melisma endpoint, or select a truthful LilyPond alignment lane.

**Consequence:** MusicXML emits numbered structured lyrics and derives exact
start/continue/stop extender events. LilyPond emits one native stanza context per verse, aligned by
an invisible exact-duration NullVoice rather than degrading text below individual notes. MIDI and
Ableton continue to ignore notation-only lyric state without changing sound, while serialization
retains a deterministic migration path for older documents.

## 56. A key signature separates written fifths from analytical scale identity

**Decision:** `KeySignature.accidentals` is the authoritative traditional written signature;
`root` plus `mode` is analytical identity. S21 closes every active scale interval to a strictly
ascending pitch-class profile rooted at zero, requires unused capacity to be zero, and requires any
name that resolves to Sunny's registry to use the canonical name and exact registered payload.
`ScaleDefinition` owns its name and description, and schema version 4 persists both; arbitrary
unregistered or anonymous valid profiles remain custom scales. LilyPond projects a non-native mode
by expanding the stored fifths through the traditional accidental order into exact
`Staff.keyAlterations`. MusicXML retains `<fifths>` with `<mode>none`; MIDI retains the same fifths
with an explicitly diagnosed major/minor proxy.

**Reason:** A borrowed mode name could dangle, JSON round-trip discarded every mode name and
description, unused interval slots were silently lost, and a registered name such as `major` could
contradict its stored intervals while still selecting a native exporter command. LilyPond's
documented `keyAlterations` property directly accepts `(step . alteration)` state
([LilyPond non-traditional key signatures](https://lilypond.org/doc/v2.25/Documentation/snippets/contemporary-music-_002d-non_002dtraditional-key-signatures)).
MusicXML permits either traditional `fifths`/`mode` or repeated `key-step`/`key-alter` pairs
([MusicXML `<key>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/key/));
Sunny's semitone scale offsets do not determine the latter's diatonic spellings. Guessing a nearest
major/minor LilyPond key or invented MusicXML altered steps therefore changed evidence rather than
projecting it.

**Consequence:** A named scale cannot select target semantics that disagree with its payload, and
its owned identity survives serialization. Native major/minor/church modes retain their native
notation commands. Other modes preserve the exact written signature in both notation targets and
make the missing analytical target algebra observable. Standard MIDI's two-valued mode limit is
likewise visible to Ableton deployment through the shared compilation report rather than being a
silent reinterpretation.

## 57. MusicXML import preserves its traditional key algebra or rejects the other branch

**Decision:** The compact `MusicXmlMeasure` stores authoritative `key_fifths` and optional
`key_mode` in addition to compatibility-derived tonic and major/minor-family fields. Parsing all
nine tonal mode names derives the tonic with the mode's line-of-fifths adjustment; `none` has no
invented tonic. Writing uses stored fifths, preserves whether mode was present, and rejects
contradictory tonic/mode/fifths evidence or a value outside MusicXML's closed mode vocabulary.
The non-traditional `key-step`/`key-alter` branch is rejected because the compact type does not
carry its ordered alteration sequence. Corpus ingestion maps a representable registered mode to
the exact Score `KeySignature` and rejects a key lacking the scale intervals Score IR requires.

**Reason:** The reader previously implemented `mode != minor` as major and used the XML library's
zero default when `<fifths>` was absent. D Dorian therefore became C major, while a valid
non-traditional key also became C major. MusicXML 4 defines major, minor, seven church-mode names,
and `none` as its exact mode vocabulary
([MusicXML `mode` data type](https://www.w3.org/2021/06/musicxml40/musicxml-reference/data-types/mode/)).
It also defines traditional fifths/mode and non-traditional altered-step sequences as exclusive
branches ([MusicXML `<key>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/key/)).
Defaulting the absent required field crossed those branches and fabricated evidence.

**Consequence:** Traditional modal keys round-trip without collapse, and D Dorian ingestion
produces a coherent D-Dorian/zero-fifths Score key. Unsupported non-traditional or interval-free
`none` input fails at the named trust boundary instead of silently contaminating harmonic
analysis, MIDI metadata, notation output, or Ableton deployment downstream.

## 58. ABC modal suffixes select signatures; unsupported K-fields do not default to major

**Decision:** `AbcParseResult` now retains the spelled tonic, canonical mode name, and exact
traditional fifths count alongside its compatibility pitch-class/minor views. The reader supports
the ABC 2.1 tonic-plus-mode subset for major, minor, Ionian, Aeolian, Mixolydian, Dorian,
Phrygian, Lydian, and Locrian, including concatenated and case-insensitive mode names. Per-note
implicit accidentals are expanded from the derived fifths count. Explicit-key accidentals,
clef/transposition modifiers, bagpipe keys, `K:none`, and unknown suffixes are rejected by this
compact NoteEvent profile.

**Reason:** The previous parser recognized only exact minor spellings and ignored every other
suffix. `K:DDor` consequently applied D major's F-sharp/C-sharp signature, and `K:G mixolydian`
also applied a major leading tone. ABC 2.1 specifies the seven modal columns, case-insensitive
first-three-letter matching, and examples such as D Dorian and G Mixolydian with no accidentals
([ABC 2.1 `K:` field](https://abcnotation.com/wiki/abc%3Astandard%3Av2.1/)). The same field can
also carry explicit alterations and layout/transposition controls that Sunny's compact result type
does not store, so accepting and ignoring them would be a false parse.

**Consequence:** Standard modal ABC input now produces the correct playback pitches and retains
inspectable key evidence. Inputs outside the declared subset fail as `InvalidAbcFile` rather than
silently changing pitch or discarding a transport/layout instruction.

## 59. MIDI compilation closes the SMF key-signature domain before file conversion

**Decision:** `MidiKeySigData` has the exact Standard MIDI File profile: a signed fifths count in
−7…+7 and a two-valued major/minor mode. `compile_to_midi` omits a Score key outside that fifths
domain, increments `dropped_key_sig_events`, and records its score position and source count. A
non-native analytical mode inside the domain still uses the documented major/minor proxy with
separate degradation evidence. The low-level SMF reader, writer, and compiled-data adapter reject
out-of-domain fifths and mode bytes.

**Reason:** Score IR can coherently describe a traditional signature wider than seven accidentals;
for example, C-double-sharp major implies +14 fifths. The SMF Key Signature meta event defines only
−7 through +7 and mode values 0 (major) and 1 (minor)
([MIDI Association Standard MIDI Files](https://midi.org/standard-midi-files),
[MIDI Association key-signature discussion quoting the SMF field](https://midi.org/community/midi-software/key-signature-message)).
Previously the compiler constructed an invalid `MidiKeySigData` value and deferred failure until
file conversion, while Ableton-facing compilation exposed neither a drop count nor complete-state
evidence.

**Consequence:** Target-independent validation does not reject an otherwise coherent score, and
all sounding notes still compile. The narrower SMF metadata loss is detected at the first target
boundary, is counted by `has_drops()`, and propagates through Ableton results and the public MCP
report so a successful deployment cannot be mistaken for an exact one.

## 60. Compact importers fail closed and Corpus construction is loss-accountable

**Decision:** The compact MusicXML reader explicitly accepts only the sequential integer profile
its C++ representation can store. It strictly parses all numeric fields, preserves surrounding
schema whitespace, and rejects decimal timing, microtonal alterations, additive/composite metre,
string voice labels, non-numeric measure labels, `backup`/`forward`, mid-measure attributes,
multi-staff/transposition state, grace/cue/unpitched notes, orphan or unequal-duration chords, and
invalid required part structure. Corpus MIDI ingestion creates every allocated non-overlapping
voice, splits cross-bar notes into exact tie chains, preserves all reconcilable tempo/metre/key
events, and rejects off-bar metre changes or conflicting same-tick metadata. Corpus MusicXML
ingestion preserves all globally coherent bar-level metre and modal-key changes and rejects
divergent per-part contexts, noncanonical measure labels, or any failed Score insertion.

**Reason:** Pugixml's convenience integer conversion maps malformed text to zero. The previous
reader therefore converted several invalid or richer MusicXML values into plausible defaults.
MusicXML 4.0 deliberately defines `divisions` and `duration` as positive decimals, `alter` as a
decimal semitone count, `voice` and `beats` as strings, and measure `number` as a token; it also
uses `backup` to coordinate voices
([MusicXML divisions](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/divisions/),
[duration](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/duration/),
[alter](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/alter/),
[voice](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/voice/),
[time](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/time/),
[measure](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/measure-partwise/)).
Those legal algebras cannot be represented by the compact type and must not be accepted as if they
had survived. Downstream, ignored insertion results meant simultaneous MIDI tones and MusicXML
chord members could disappear while ingestion still succeeded; using only the first metadata
context likewise erased modulation, metre, and tempo evidence.

**Consequence:** A successful compact parse has a precise representability meaning. A successful
Corpus ingestion contains every accepted note and map event, ends in a compilable Score, and can
recompile a split cross-bar MIDI note to its original sounding interval. Broader MusicXML remains
available through the richer Score exporter but is outside this reader profile until its missing
types are modeled.

## 61. Scala `.scl` parsing follows its numeric grammar without accepting junk

**Decision:** The Scala reader preserves an empty first data line as the scale description,
requires an exact non-negative degree count and exactly that many degree lines, accepts positive
integer ratios with optional whitespace around one slash, accepts finite cents only when the
numeric token contains a period, and permits format-specified trailing text only after horizontal
whitespace. Negative or zero ratios, attached junk, overflow/non-finite cents, and malformed line
counts fail. The writer returns `Result`, rejects multiline or comment-shaped descriptions,
non-finite values, non-positive or cache-contradictory ratios, and nonzero unused ratio fields.

**Reason:** The previous `from_chars` and `strtod` calls did not verify where conversion stopped;
tokens such as `3x/2`, `3/2x`, `1junk` counts, and overflowed cents could therefore be accepted.
It also discarded the empty description line required by the format. The Scala format explicitly
defines an empty description, count-controlled degree lines, period-selected cents, positive
ratios, whitespace, and ignored text after a valid pitch value
([Scala `.scl` format](https://w.huygens-fokker.org/scala/scl_format.html)).

**Consequence:** Parser permissiveness now matches the external grammar rather than C library
prefix-conversion behavior, and writer success guarantees that reparsing cannot reinterpret the
description or ratio algebra.

## 62. MusicXML integer timing is preflighted over the whole projected cursor algebra

**Decision:** Both MusicXML writers use checked LCM and duration-unit arithmetic. The compact
writer's explicit tractable domain is positive `int` divisions and duration units and it rejects a
malformed Beat, non-integral projection, LCM overflow, or unit overflow. The richer Score compiler
computes divisions from every value it will project—not only measured event durations, but also
event offsets, global point positions, span boundaries, and global or local measure durations—and
returns `ArithmeticOverflow` before DOM construction if any value is outside that domain.
MusicXML graphical note data is treated independently: Sunny derives all standard types from
`1024th` through `maxima` and an exact augmentation-dot count, but omits optional `<type>`/`<dot>`
data for an arbitrary rational instead of labelling it as a quarter note. If that omission also
prevents projection of an explicit Sunny cue-size notehead, the compiler records a diagnostic.

**Reason:** MusicXML defines `<divisions>` as the number of divisions per quarter note and requires
duration calculations to account for tuplets; its `divisions` data type can be decimal, although
integer values are preferred for interoperability
([MusicXML `<divisions>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/divisions/),
[MusicXML divisions data type](https://www.w3.org/2021/06/musicxml40/musicxml-reference/data-types/divisions/)).
It separately defines `<type>` as optional graphical note information with a closed vocabulary
from `1024th` through `maxima`
([MusicXML `<note>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/note/),
[MusicXML note-type-value](https://www.w3.org/2021/06/musicxml40/musicxml-reference/data-types/note-type-value/)).
The former implementation used unchecked `std::lcm`, narrowed its result to `int`, multiplied
duration units without overflow checks, and chose divisions from note lengths alone. A point at
one seventh of a whole note could therefore become offset zero when the notes used quarters, while
large coprime denominators could wrap. The graphical fallback independently asserted a quarter
note that was not source evidence.

**Consequence:** Successful export proves exact integral cursor coordinates throughout the
declared implementation range, including metadata-only denominators. Invalid low-level state fails
without reaching rational assertions, target-range exhaustion is explicit, and sounding duration
remains exact even when MusicXML has no truthful graphical note token for it.

## 63. The compact ABC reader accepts one exact monophonic algebra and rejects the rest

**Decision:** `AbcHeader` now distinguishes absent/free metre from a specified meter, distinguishes
an explicit `L:` from the standard-derived unit length, and retains the exact beat unit associated
with an optional positive integer `Q:` rate. A complete compact tune requires one positive `X:` and
one `K:`. Recognized header numeric fields consume their entire token and malformed, duplicate, or
unrepresentable values reject. The body admits letter notes, `z` rests, plain bar lines, comments,
pitch-wide bar accidentals, and exact positive integer/slash length multipliers, including repeated
bare slashes. Duration multiplication and cumulative time are checked. A pitch outside MIDI
0…127 rejects instead of being clamped. Syntax whose semantics the flat `NoteEvent` result cannot
retain—chords, repeats, tuplets, broken rhythm, ties/slurs, grace/decorations, multi-measure rests,
voices, directives/macros, chord symbols, and body/inline field changes—rejects rather than being
skipped or sequentialised.

**Reason:** ABC 2.1 makes absent `M:` free metre and derives an absent `L:` from meter: 1/16 below
3/4 and 1/8 otherwise. It defines a `Q:` rate relative to one or more explicit beat values, exact
integer and slash note-length forms (including repeated bare slash division), comment syntax,
pitch-wide accidental propagation by default, and separate constructs for chords, repeats,
tuplets, overlays, and inline fields
([ABC 2.1 standard](https://abcnotation.com/wiki/abc%3Astandard%3Av2.1)). The previous reader
defaulted every absent meter to 4/4, always defaulted `L:` to 1/8, discarded the `Q:` beat unit,
ignored failed `from_chars` conversions, multiplied duration integers unchecked, clamped pitches,
parsed note letters inside comments, and skipped structural delimiters individually. Thus malformed
input could appear valid, `[CEG]` became three sequential notes, a repeat was erased, and a body
field could leak one of its value letters into the melody.

**Consequence:** Successful ABC parsing now means the complete accepted sounding stream and header
timing evidence are represented exactly inside the deliberately narrow result type. Legal ABC
outside that type's algebra fails at the named boundary rather than silently changing playback or
fabricating defaults; broader ABC support requires adding the corresponding model state first.

## 64. Scala ratio conversion does not use unison as an error sentinel

**Decision:** The public integer `ratio_to_cents(num, den)` helper returns `Result<double>` and
rejects a non-positive numerator or denominator as `InvalidJIRatio`. Parser, writer, cent-table
validation, and public tests consume that checked result.

**Reason:** Zero cents is the exact, valid image of ratio 1/1. Returning `0.0` for 0/1, 1/0, or a
negative ratio made invalid state observationally identical to unison and contradicted the Scala
format's positive-ratio grammar. The shared core tuning conversion already exposes this
precondition as a checked result.

**Consequence:** Every public tuning conversion boundary now distinguishes a valid unison from an
invalid ratio; no caller can accidentally persist or compare a fabricated zero-cent interval after
failed validation.

## 65. Low-level LilyPond helpers preserve exact time or reject invalid state

**Decision:** `ly_pitch`, `ly_key`, and `ly_time_signature` now return `Result`; an invalid pitch
letter no longer becomes C, non-positive meter fields reject, and an empty or invalidly spelled
chord rejects. `ly_duration` accepts every positive Beat. It derives conventional values from
LilyPond's `1024` through `1`, `breve`, `longa`, and `maxima` vocabulary with an exact number of
augmentation dots, then uses `1*N/D` for any remaining positive rational. `ly_fragment` requires
ordered, non-overlapping, positive event intervals, inserts exact rests for onset gaps, emits muted
events as rests, and uses checked time accumulation.

**Reason:** LilyPond specifies absolute letter pitches with octave marks, note/rest durations with
numbers and dots, and exact rational duration multipliers that affect musical time
([LilyPond writing pitches](https://lilypond.org/doc/v2.25/Documentation/notation/writing-pitches.html),
[LilyPond scaling durations](https://lilypond.org/doc/v2.25/Documentation/notation/scaling-durations.html),
[LilyPond writing rests](https://lilypond.org/doc/v2.25/Documentation/notation/writing-rests.html)).
The former compact helper supported only values through 64th and one dot even though its target
could encode more, substituted C for an invalid `SpelledPitch`, emitted an empty chord, rendered a
muted NoteEvent as a sounding note, and concatenated events without examining `start_time`. Its
claim to preserve rhythm was therefore false for gaps, overlap, reordering, mute state, and legal
arbitrary rational durations.

**Consequence:** A successful fragment now has a precise monophonic temporal meaning from time zero,
and every valid rational duration within Beat's domain reaches LilyPond without rounding. Invalid
or polyphonic flat input fails rather than producing plausible but musically different source;
polyphony remains the responsibility of the hierarchical Score compiler.

## 66. Flat MusicXML adapters preserve their complete admitted NoteEvent stream

**Decision:** `musicxml_to_note_events` and `note_events_to_musicxml` return `Result`. The inverse
adapter requires one writer-valid sequential compact part and materialises every rest as a muted
NoteEvent while retaining equal-duration chord starts. The forward adapter requires ordered,
non-overlapping positive intervals; represents gaps and muted events as rests; represents
same-start, equal-duration pitched groups with `<chord>`; and rejects other overlap. It emits no
time signature because an arbitrary flat stream supplies no measure algebra from which 4/4 can be
inferred. Since `MusicXmlScore` has no velocity field, success requires the compact canonical
values 80 for sounding notes and 0 for muted rests. The spelling key is bounded before its
int8-backed line-of-fifths conversion, and generated output must pass the checked compact writer.

**Reason:** The previous forward adapter ignored every muted event and every `start_time`, placed
the remaining notes sequentially in one declared 4/4 measure, and returned no loss evidence. The
inverse adapter skipped rests, silently skipped an out-of-MIDI spelling, accepted arbitrary manual
state, and discarded every part after the first. Those are model changes, not representational
details: a gap became an onset, a rest disappeared, simultaneity became sequence, and multi-part
content appeared successfully converted.

**Consequence:** Success now establishes an exact flat timing/rest/chord projection within the
compact canonical velocity profile. Richer meter, performance velocity, independent overlapping
voices, and multiple parts must use Score IR or a future adapter result that explicitly models
their preservation; they cannot disappear behind a value-returning convenience function.

## 67. Compact MusicXML divisions are encoding state, not a score property

**Decision:** `MusicXmlScore.divisions` is removed. The reader requires a positive integer
`<divisions>` declaration before the first duration in each part, carries later declarations only
as part-local parser state, and immediately normalizes every duration into an exact `Beat`. The
writer derives one checked positive integer divisions basis from all normalized durations.

**Reason:** MusicXML defines divisions per quarter note as the scale used to interpret duration
values and allows attributes to change during a part
([MusicXML `<divisions>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/divisions/)).
A score-wide scalar could therefore hold only the last declaration encountered across all parts.
The writer already ignored it and recomputed its basis, so the public field was stale pseudo-state:
changing it changed nothing, while parsing a new declaration silently overwrote earlier evidence.
Defaulting an undeclared first scale to one similarly invented timing not present in the document.

**Consequence:** The compact model now has one authoritative temporal representation. Source files
with changing divisions retain their exact musical durations, manually constructed scores cannot
pretend to select a writer scale, and a duration without an established source scale fails instead
of being interpreted through an undocumented default.

## 68. SMF structural termination and note pairing are fail-closed

**Decision:** The low-level SMF reader requires the standard six-byte header, exactly consumes all
declared track chunks and the complete input, and requires one final zero-length End-of-Track meta
event in every track. Note endings must match a pending same-channel/same-pitch note start and must
produce a positive duration; unmatched endings, same-tick pairs, and unterminated starts reject.

**Reason:** The Standard MIDI File contract makes End of Track non-optional so every track has an
exact ending, and Note On with velocity zero is a note ending rather than a new silent note
([MIDI Association Standard MIDI Files](https://midi.org/standard-midi-files)). The previous parser
ignored an unmatched note ending, accepted a zero-duration pair that its own writer rejected,
accepted missing or non-final End-of-Track events, and returned success with undeclared bytes after
the last track. Each case allowed malformed source to become a plausible but different `MidiFile`.

**Consequence:** Successful parsing now proves the structural envelope and retained note intervals
are internally closed. Deliberately lossy Corpus analysis remains a separate, documented projection;
malformed pairing or track termination is no longer confused with intentional loss of an
unmodelled valid event.

## 69. Flat MIDI conversion does not erase Sunny mute state

**Decision:** `note_events_to_midi` rejects a muted `NoteEvent` because the low-level SMF note model
has no retained mute field; it no longer omits that event and returns apparent success. The inverse
analytical projection validates the SMF format/PPQ envelope and every channel, rejects the
in-memory-only keyswitch marker, and explicitly documents that valid track/channel ownership is
collapsed for Corpus use.

**Reason:** Absolute timing means omitting a muted note does not need to shift later onsets, but it
still destroys source state and makes a subsequent inverse indistinguishable from a stream in
which the event never existed. Conversely, Corpus voice inference intentionally combines MIDI
channels and tracks into analytical voices; that is a named lossy projection, not an exact
interchange round trip. Mixing those two contracts hid both the accidental loss and the intended
one.

**Consequence:** Success in the flat forward adapter now proves that every supplied source event
became an SMF note. Corpus ingestion may still collapse source routing under its documented
confidence model, but malformed channel/PPQ state and Sunny-only control markers cannot leak
through that path as ordinary musical notes.

## 70. Legacy Orchestrator messages are checked before the Ableton wire

**Decision:** `LomProtocol::from_note_event` and `BridgeDispatcher::to_lom_request` return `Result`.
Note conversion rejects malformed/non-positive Beat state and invalid velocity before doing a
direct rational-to-double calculation. Dispatcher translation requires canonical paths, complete
message argument shapes, non-empty method/property names, and a wholly consumed finite positive
clip-length token. AddNotes requires a non-empty wholly valid batch. Any failure is recorded as
declined before send and does not call the transport.

**Reason:** Live 11 `Clip.add_new_notes` requires finite non-negative starts, positive durations,
MIDI pitches, and valid velocities. The Remote Script already enforced Sunny's narrower form, but
the public native helper called `Beat::to_float()` before checking its denominator, and the
dispatcher used throwing `std::stod` on arbitrary queue text. Thus malformed state could assert in
debug, throw through `dispatch`, or reach the wire only to be rejected by the second peer. That
violated the conformance model's claim that the native-to-host boundary is closed and fail-closed.

**Consequence:** Queue delivery now has two independent validation layers without relying on the
Live process to sanitize native state. Invalid orchestration input is observable as not sent, and
successful note translation establishes the exact Sunny subset of the documented Live note
dictionary before floating-point serialization.

## 71. Scala cent serialization discharges its exact round-trip claim

**Decision:** Finite cent intervals are serialized with `max_digits10` significant digits and an
explicit decimal point. Ratio intervals remain exact numerator/denominator tokens.

**Reason:** `ScalaTuning` stores cents as `double`, and its public contract promises that a valid
writer result parses back to the same model. The previous fixed six-decimal output silently rounded
all more precise values. Simply switching to default formatting would create a second error:
Scala selects a ratio when an interval token has no period, so an integral value such as `200`
would return as ratio 200/1 rather than 200 cents.

**Consequence:** Every finite cent value admitted by the writer recovers the identical binary
floating-point value on parse, while integral cent values remain in the cents branch. The declared
write-parse identity is now an implemented property rather than an approximate display convention.

## 72. Parsed SMF Type 1 state retains track topology

**Decision:** `MidiFile` stores the declared `track_count`, and every retained tempo, meter, key,
program, controller, and note event stores its source track index. Parsing populates both even for
empty tracks. The existing Type-0 writer requires exactly one declared track and rejects any event
whose track index is not zero. The analytical `midi_to_note_events` projection checks track bounds
before deliberately collapsing track/channel ownership.

**Reason:** SMF Type 1 is a simultaneous multi-track structure, not merely Type 0 with a different
header word. The parser accepted it while flattening events into vectors with no ownership and
discarding empty tracks, yet returned `format = 1`. That was stale structural state: the format tag
claimed a topology the model could not describe, and downstream code could not distinguish two
source tracks from one interleaved track.

**Consequence:** Successful Type-1 parsing now retains the complete track topology for every event
class Sunny models. Corpus analysis may still merge it under its named lossy policy, while future
Type-1 writing or part mapping has the source identity needed to remain loss-accountable.

## 73. Valid unmodelled SMF events produce explicit loss evidence

**Decision:** `MidiFile` retains Note Off velocity and contains a `MidiParseLoss` counter for each
valid event class it parses but does not otherwise model: unknown meta events, SysEx, polyphonic
aftertouch, channel pressure, and pitch bend. The Type-0 writer rejects nonzero loss evidence.
Corpus ingestion accepts the analytical projection but adds one manual-correction record for every
nonzero loss class.

**Reason:** Silently skipping a syntactically valid event differs from rejecting malformed input.
The parser previously returned the same `MidiFile` for a plain note stream and for one carrying
pitch bends, aftertouch, SysEx setup, or unretained metadata. It also discarded explicit Note Off
velocity despite being able to retain and write that byte directly. Neither callers nor Corpus
users could tell that a successful import omitted performance or device state.

**Consequence:** Low-level write success now proves no known parse loss remains unresolved, and a
Corpus work exposes every admitted loss class through its existing correction model. Extending
`MidiFile` with a future event type can reduce these counters without changing the meaning of prior
successful imports.

## 74. SMF End of Track retains the declared track length

**Decision:** `MidiFile.track_end_ticks` stores the absolute tick of each parsed End-of-Track event.
Its cardinality equals the declared track count. Type-0 writing preserves an explicit endpoint,
rejects one before the final emitted event, and derives the endpoint from the last event only when
manual state leaves the vector empty.

**Reason:** End of Track is required precisely so a track has an exact ending, including silence
needed for looping or concatenation. The parser enforced the event's presence but discarded its
accumulated delta; the writer then always emitted EOT at delta zero after the last modeled event.
Thus a valid empty track ending at tick 1920 returned as a zero-length track, and trailing silence
vanished without entering `MidiParseLoss`.

**Consequence:** Parsed Type-0 and Type-1 topology now includes exact track length as well as event
ownership. A successful exact rewrite preserves trailing silence, while contradictory manual end
state is rejected rather than shortening content or wrapping a delta-time value.

## 75. Scala projection proves octave-periodic representability

**Decision:** General `.scl` parsing and writing retain any valid formal period, but
`scala_to_cent_table` succeeds only for exactly twelve listed degrees whose final degree is exactly
1200 cents. A non-octave period returns `FormatError`.

**Reason:** Scala defines degree zero as implicit and the last listed scale degree as the formal
octave, period, or interval of equivalence; it expressly does not assume that value is 2/1 or 1200
cents ([Scala help, “Scales”](https://www.huygens-fokker.org/scala/help.htm)). Sunny's
`TuningTable`, by contrast, is a deviation function on **Z/12Z**, and its frequency equation repeats
that table under powers of two. The previous adapter ignored the twelfth Scala interval entirely,
so twelve divisions of a tritave could masquerade as an octave-periodic pitch-class table.

**Consequence:** The broad interchange model still round-trips non-octave scales. Success from the
narrow adapter now proves the source period has the octave recurrence assumed by every
`TuningTable` consumer; an incompatible but valid Scala scale cannot silently acquire different
equivalence semantics.

## 76. SMF time signatures retain metronome and notation semantics

**Decision:** `MidiTimeSignatureEvent` retains the Time Signature meta-event's `cc` byte (MIDI
clocks per metronome click) and `bb` byte (notated 32nd notes per MIDI quarter) in addition to its
numerator and denominator. Parsing stores both bytes and Type-0 writing emits the stored values;
manually constructed events default to the conventional 24 and 8.

**Reason:** The standard payload is `FF 58 04 nn dd cc bb`; `cc` determines the metronome-click
interval and `bb` relates notated 32nd notes to the MIDI quarter
([MIDI Association discussion quoting the SMF specification](https://midi.org/community/midi-specifications/time-signature-understanding)).
The previous parser treated the event as fully modeled but discarded its last two bytes, while the
writer unconditionally substituted 24/8. A non-default metronome grouping therefore changed on an
otherwise successful exact rewrite and generated no `MidiParseLoss` evidence.

**Consequence:** A retained Time Signature event now has one authoritative representation for its
complete standard payload. Exact Type-0 rewriting preserves click grouping and notation scaling,
while core score compilation initially received the conventional defaults because it had no
corresponding fields. Decision 187 supersedes that Score-compiler defaulting behavior; it does not
change this low-level round-trip rule.

## 77. Paired SMF notes prove FIFO representability

**Decision:** Before expanding paired `MidiNoteEvent` intervals, the Type-0 writer orders note
starts by tick and requires the corresponding end ticks for each channel/key to be nondecreasing.
Equal-endpoint Note Off messages are emitted in the same stable onset order. A nested same-key
pairing returns `FormatError`.

**Reason:** MIDI Note On and Note Off messages identify a channel and key, not a particular note
instance. Sunny's parser therefore pairs overlapping instances FIFO. A manually constructed model
could previously contain a note from tick 0 to 480 and a second same-key note from 120 to 240;
writing and parsing that model paired the first ending with the first onset and silently changed
both durations and potentially their release velocities. The byte stream could carry the message
sequence, but not the nested interval identities asserted by the source model.

**Consequence:** Successful low-level writing now proves that the paired interval representation
is recoverable under the parser's explicit pairing rule. Sequential, duplicate, and FIFO-overlap
cases remain representable, while an ambiguous nested identity cannot pass as an exact rewrite.

## 78. Retained SMF events preserve within-track source order

**Decision:** Every retained SMF event stores a one-based source order within its track; paired
notes store the separate order of their Note On and Note Off messages. The Type-0 writer emits a
fully ordered parsed model by `(tick, source_order)`. A manually constructed model may leave all
orders zero and use Sunny's canonical priority order, but mixing explicit and unspecified order or
supplying a non-monotone/duplicate order returns `FormatError`.

**Reason:** Delta zero does not make simultaneous MIDI messages commutative. A Program Change or
Control Change before a Note On can affect that onset, while the same message after it need not.
The parser previously partitioned messages into typed vectors and the writer always imposed its
own class priority, so valid source order vanished without `MidiParseLoss` evidence. Track identity
and absolute tick alone were insufficient to describe the stream accepted from the SMF boundary.

**Consequence:** Exact rewriting retains the sequential semantics of all modeled events at a tick,
not merely their timestamped multiset. Generated models still receive deterministic safe ordering,
and partial pseudo-order cannot silently combine source evidence with invented placement.

## 79. Flat MIDI construction rejects SMPTE division at entry

**Decision:** `note_events_to_midi` requires its `ppq` argument to be nonzero with bit 15 clear,
matching the parser, writer, compiled-model adapter, and inverse projection. A high-bit value returns
`InvalidMidiPPQ` before any Beat-to-tick arithmetic.

**Reason:** In the SMF header, a division with bit 15 set is an SMPTE time-code encoding, not pulses
per quarter note. The adapter previously interpreted that bit pattern as a large PPQ, returned a
nominally valid `MidiFile`, and deferred rejection until `write_midi`. This made construction
success weaker than the type's own timing invariant and performed arithmetic under the wrong unit
model.

**Consequence:** Every public entry into Sunny's `MidiFile` PPQ profile now shares the same temporal
domain. SMPTE-timed input remains explicitly outside the model rather than temporarily
masquerading as musical-quarter resolution.

## 80. The ABC compact model retains accepted tune identity and metadata

**Decision:** `AbcHeader` stores the required positive `X:` value as a signed 64-bit
`reference_number` and retains every accepted `A/B/C/D/F/G/H/N/O/R/S/Z` information field as an
ordered `(code, value)` entry. Repeated descriptive fields remain distinct and ordered.

**Reason:** ABC 2.1 defines `X:` as the tune reference number and its string-valued information
fields as tune documentation; repeated string fields add information rather than replacing it
([ABC 2.1 information fields](https://abcnotation.com/wiki/abc%3Astandard%3Av2.1/)). The compact
parser validated tune identity and explicitly accepted twelve descriptive field classes but then
discarded their values. Two semantically different accepted headers therefore returned the same
model without any declared loss.

**Consequence:** The read-only compact projection still rejects richer performance instructions,
voices, and body fields, but everything in the header subset it claims to accept is now observable
to callers. Tune identity and repeated attribution/history data no longer disappear on successful
parse.

## 81. Scala degree annotations are retained interchange state

**Decision:** `ScalaInterval` stores the optional horizontally separated text after its numeric
pitch token as canonical `trailing_text`. Parsing trims only the separating/outer horizontal
whitespace, and writing restores a nonempty annotation after one space. Writer state with padded or
multiline annotation text is rejected so write-parse identity remains exact.

**Reason:** The Scala `.scl` format says anything after a valid pitch value is ignored by the
numeric interpreter, and examples use that area for degree names
([Scala scale file format](https://www.huygens-fokker.org/scala/scl_format.html)). Sunny explicitly
accepted this syntax but discarded the text, making an annotated scale observationally identical
to an unannotated one and preventing the declared interchange round trip from preserving all
accepted fields.

**Consequence:** Numeric tuning behavior is unchanged, while human degree names and annotations
survive parse/write/parse. The public interval model now accounts for the complete line subset the
reader accepts instead of treating valid suffix data as invisible.

## 82. Rich MusicXML export is not falsely specified as compact-importable

**Decision:** The `compile_score_to_musicxml` contract now states that it emits the richer Score
export profile and that only its sequential intersection is accepted by `parse_musicxml`. The
compact parser's fail-closed rejection of cursor, multi-staff, and other unrepresented constructs
is unchanged.

**Reason:** MusicXML uses `<backup>` and `<forward>` to coordinate multiple voices and staves
([MusicXML 4.0 `<backup>`](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/backup/)).
The rich compiler correctly emits those constructs, while the compact `MusicXmlScore` algebra is
deliberately sequential and rejects them. Its public header nevertheless claimed every compiler
result was parseable by the compact reader, contradicting both external MusicXML semantics and the
implemented profile boundary.

**Consequence:** Internal and external contracts now agree on the tractable round-trip domain.
Well-formed rich export remains available, the narrow reader remains loss-averse, and callers are
not promised an impossible inverse for multi-voice or multi-staff documents.

## 83. Native Ableton transport enforces the complete peer algebra

**Decision:** `LomProtocol::validate_request` classifies the exact protocol-v8 path shapes,
get/set/call allowlists, argument cardinalities and types, Live value domains, parameter ranges,
and note dictionaries already enforced by the Remote Script. `CommandBuffer`, `TcpTransport`, and
`JournaledLomTransport` invoke it before recording or sending; `send_notes` validates its canonical
`add_new_notes` request. Failure is a `NotSent` response.

**Reason:** Canonical path validation alone prevented reflective traversal but did not close the
native side of the wire. A direct public `LomRequest` could still carry an unknown method, invalid
meter/tempo, non-finite scalar, empty device name, incoherent parameter range, or malformed note
batch and rely on the Python process to refuse it. That made native success/recording semantics
broader than the documented bridge and moved input validation across the Ableton host boundary.

**Consequence:** Both peers now independently prove the same request algebra. Offline recordings
cannot contain commands the Live peer would reject structurally, invalid state never consumes a
deployment-plan slot or socket frame, and `NotSent` once again means no target-side uncertainty was
introduced.

## 84. Ableton cue deployment distinguishes intent, action, and observed state

**Decision:** Convert every top-level `ScoreSection.start` to Live quarter-beat time before target
access. Each successful live `sunny_set_cue` response must contain exactly the echoed requested
time/name, finite observed time, observed name, and either `created` or `updated`. Compilation
retains a deployment record per cue and reports requested, created, updated, and verified counts.
Recording-only transport records use the distinct `recorded_only` action and cannot increment a
target-mutation or verification count.

**Reason:** The documented
[Song](https://docs.cycling74.com/apiref/lom/song/) operation sets or deletes a cue only at the
current playhead, and [CuePoint](https://docs.cycling74.com/apiref/lom/cuepoint/) exposes its time
and name as separate observed state. Sunny's adapter therefore has to preserve the playhead and may
reuse a cue within its explicit `1e-7` quarter-beat tolerance. Its former empty acknowledgement was
unable to distinguish reuse from creation, while the native compiler counted every call as
created. Separately, a failed section-time conversion was silently ignored after earlier Live
mutations.

**Consequence:** Public compilation evidence no longer overclaims cue creation. Malformed live
evidence is a protocol error, readback divergence remains visible and unverified, and invalid
section positions fail before any target access or mutation instead of disappearing from the
deployed structure. The operation remains incremental and non-transactional once live mutation
begins.

## 85. Ableton note and meter results preserve requested cardinality

**Decision:** Score deployment reports `notes_requested` beside `notes_written` and
`time_signature_events_requested` beside `time_signature_events_written`. Requested notes count
the ordinary Live note dictionaries emitted by the shared MIDI projection. Requested meter events
count source TimeSignatureMap entries. Written counters retain their existing narrower meaning:
payloads accepted by successful adapter calls and the initial Song signature applied through its
current-value properties.

**Reason:** The documented [Clip](https://docs.cycling74.com/apiref/lom/clip/) note insertion API is
available only on the supported Live 11+ capability path, while
[Song](https://docs.cycling74.com/apiref/lom/song/) exposes current signature fields but no admitted
meter-automation writer. A zero or one write count cannot distinguish no source intent from intent
preserved but unrepresentable on that target. The asymmetry already had requested/written pairs for
tempo, articulation controls, and automation lanes.

**Consequence:** Callers can calculate target-boundary loss without reconstructing the Score or
guessing from warnings. `notes_written` remains acknowledgement evidence rather than a claim that
Sunny read back and proved the entire clip-note set; a future exact readback capability would need
its own observed/verified evidence.

## 86. Timbre and Mix structural results pair intent with admitted mutations

**Decision:** Timbre reports requested/created source devices and requested/inserted enabled
effects. Mix reports requested/created Group and Return Tracks, requested/inserted enabled effects,
requested/configured enabled sends, and requested/configured channel strips. All are 64-bit summary
counters and appear in both standalone and aggregate MCP results.

**Reason:** The documented [Song](https://docs.cycling74.com/apiref/lom/song/) surface can create
Return Tracks but exposes no Group Track creation operation. The documented
[Track.insert_device](https://docs.cycling74.com/apiref/lom/track/) path is Live 12.3+, native-only,
and does not materialise plug-ins or Max for Live devices. A zero mutation counter therefore cannot
distinguish absent intent from a preserved source, effect, or bus that the selected target cannot
represent. Timbre also deliberately refuses to insert effects when its instrument source was not
materialised.

**Consequence:** Capability loss is mechanically visible without parsing warning prose. Enabled
effects on GroupBuses and unsupported targets remain in requested cardinality, while successful
call counters retain their narrower acknowledged-mutation meaning. Decision 101 adds distinct
source-device verification; neither counter family implies sonic equivalence.

## 87. Hierarchical sections have a closed domain and an explicit flat-CuePoint residual

**Decision:** Structural rule S27 requires every ScoreSection, recursively, to have a nonempty
label, a valid non-empty half-open span within the score (allowing the terminal bar line as an end),
and an in-domain optional FormFunction. SectionMap is explicitly optional and may be partial. Live
deployment projects only top-level starts and reports total, projected, and unprojected node counts;
nested residuals produce a completeness warning.

**Reason:** Sunny's SectionMap is a tree of labelled spans, while the documented
[CuePoint](https://docs.cycling74.com/apiref/lom/cuepoint/) state is a flat time/name collection and
the [Song](https://docs.cycling74.com/apiref/lom/song/) mutation operates at current time. A child
may share its parent's start, so sending both through the tolerance-based safe adapter would rename
one cue rather than preserve two nodes or their hierarchy. The former compiler silently visited
only the top level, while structural validation admitted empty labels, invalid/empty spans, and
out-of-range form functions. The formal text also required full top-level coverage even though new
scores and public workflows validly use empty or partial maps.

**Consequence:** Invalid section payloads fail before Ableton target access, and unavoidable
hierarchy loss is quantified rather than hidden. Sunny does not invent flattened labels or duplicate
time semantics absent from either model, and its internal specification now matches the tractable
annotation contract implemented by workflows and validation.

## 88. Part-local meter has a notation-rich and performance-tractable boundary

**Decision:** MusicXML and LilyPond may retain any structurally valid Part-local TimeSignature.
MIDI, generic NoteEvent, and Ableton performance compilation require its exact measure duration to
equal the active global duration. An unequal duration returns `TargetValueUnrepresentable` before
event generation or target access. A non-identical equal-duration grouping remains playable but is
reported through `dropped_time_sig_events`; Ableton also counts it as requested but not written and
warns that Song meter is global.

**Reason:** Sunny validates each local measure against its local duration, but the shared
ScoreTime-to-absolute-time function derives later bar positions solely from TimeSignatureMap.
Blindly compiling a local 3/4 bar under global 4/4 therefore inserts an unstated quarter-note gap,
while independently accumulating Part time would make global tempo, key, and section bar addresses
ambiguous. MusicXML and LilyPond have Part-local measure structure; Standard MIDI metadata and the
documented [Ableton Song](https://docs.cycling74.com/apiref/lom/song/) meter state are global.

**Consequence:** Performance output no longer silently changes timing under unequal polymeter.
Equal-duration alternatives such as 2/2 under 4/4 keep exact note timing while exposing their
notation-metadata loss. Full unequal-duration polymeter requires a future explicit multi-timeline
model that defines how global maps relate to each Part, not an exporter-local guess.

## 89. Live current-scale state is documented but not yet safely deployable

**Decision:** Ableton Score results report global and nonredundant Part-local key intents as
`key_signature_events_requested` and currently report `key_signature_events_written = 0`, with a
capability warning. SMF compilation also counts nonredundant Measure.local_key state in
`dropped_key_sig_events`. No root/name/mode call is admitted to the bridge yet.

**Reason:** The current [Song](https://docs.cycling74.com/apiref/lom/song/) contract exposes
writable `root_note`, `scale_name`, and `scale_mode` plus read-only `scale_intervals`, so the former
complete silence was no longer externally honest. However, Ableton's
[Live 12 release notes](https://www.ableton.com/en/release-notes/live-12/) identify the Max API
`scale_mode` addition at 12.1.5, and the
[tuning-system manual](https://www.ableton.com/en/live-manual/12/using-tuning-systems/) states that
scale choosers can disappear under a loaded tuning. Accepted scale names are target state, and one
Song scale cannot retain a changing global map or Part-local overrides. Sequential root/name writes
can also partially mutate the Set if name selection fails.

**Consequence:** Nominal MIDI note indices still deploy, but completeness no longer implies their
audible frequencies, Live's scale UI, or scale-aware devices match the Score. A future tractable capability must include a versioned
threshold, tuning/current-scale snapshot, closed name mapping, interval readback, guarded plan
state, and explicit residuals for later/local keys before it can increment the written counter.

## 90. Scala interchange, core temperament, Score pitch, and Live tuning are not conflated

**Decision:** `ScalaTuning` remains a general interchange value and its admitted text write/parse
round trip is semantic identity. `scala_to_cent_table` returns the core `TuningTable` type but is a
partial, lossy projection restricted to twelve explicit intervals with an exact 1200-cent period;
it has no inverse round-trip claim. Neither value is currently Score state, so neither generates a
MIDI, NoteEvent, render, or Ableton tuning mutation. The fixed-table frequency equation now matches
the implementation and rejects non-finite/non-positive output state.

**Reason:** The current [Live 12.1 release
notes](https://www.ableton.com/en/release-notes/live-12/) and
[TuningSystem reference](https://docs.cycling74.com/apiref/lom/tuningsystem/) expose writable
current tuning name, range, reference-pitch, and note-tuning properties. This made Sunny's silence
about its disconnected Scala utility externally ambiguous. It did not, however, make Scala data a
Score performance request: the conversion discards description, ratio spelling, formal-degree
annotations, and general cardinality/period data, while Score contains only integer 12-TET/SPN
pitches. Live's public reference also labels several values as dictionaries without specifying
their member schema, and changing the target tuning can retune existing note indices.

**Consequence:** Parsing an `.scl` file no longer carries a documented implication that deployed
sound changes, and the impossible Scala→table→Scala conservation law is removed. A future Live
tuning path must start with an explicit Score-owned tuning model, a Live-12.1 capability, closed
dictionary schemas, stale-state/retune-policy guards, complete set/readback evidence, and explicit
per-track bypass/device support. Until then Sunny honestly models the capability as unreachable,
not implemented-by-association.

## 91. Accepted Live note dictionaries do not prove audible pitch

**Decision:** Any nonempty Score note deployment to Live 12 or later adds a dedicated warning that
the Set tuning, track bypass, and device support were not observed. `notes_requested` continues to
count nominal 12-TET MIDI indices and `notes_written` continues to count dictionaries accepted by
`Clip.add_new_notes`; neither is frequency evidence. Documentation no longer says sounding pitch
is correct merely because key/scale state was left untouched.

**Reason:** The [Live tuning-system
manual](https://www.ableton.com/en/live-manual/12/using-tuning-systems/) states that after a tuning is
loaded, piano-roll notes represent the tuning and that changing/removing it can change the pitches
of existing notes. It also limits reliable tuned output to Live's built-in instruments and
appropriately configured MPE-capable targets, with a per-track bypass. The current target profile
and structural snapshot observe none of those conditions. Exact `pitch: 60` transport evidence
therefore proves an integer index, not C4 at 12-TET frequency.

**Consequence:** The Live adapter remains useful and loss-accountable without making an audible
claim it cannot test. A future alignment proof must include tuning state in plan/readback evidence,
model the track bypass/support boundary, and ultimately use rendered or measured output when sonic
equivalence matters. Live 11 omits this tuning-specific warning because the Set-tuning feature is a
Live-12 condition, but note-call success still does not certify an instrument's audio.

## 92. Live note creation retains the IDs that the API returns

**Decision:** Each nonempty Part note call produces an `AbletonNoteDeployment`. On a real
transport, `Clip.add_new_notes` must return an integer list containing exactly one unique note ID
per requested dictionary; missing, duplicate, or wrong-sized evidence is `ProtocolError`.
`note_batches_requested`, `note_batches_executed`, and `note_ids_returned` expose the aggregate
cardinalities. Offline transports record the batch as `recorded_only` and claim neither execution
nor IDs. `notes_written` retains its established narrower successful-call meaning.

**Reason:** The current [Clip LOM
reference](https://docs.cycling74.com/apiref/lom/clip/) explicitly says that `add_new_notes`
returns the IDs of the added notes. Sunny's Python peer already serialised that return value, but
the native compiler discarded it and incremented the same write counter for an executing Live peer
and a command buffer. This erased available creation identity and made malformed adapter responses
indistinguishable from valid ones.

**Consequence:** Note creation has stronger, mechanically inspectable evidence without being
overstated as full note readback. Returned IDs prove the call's creation cardinality and identity;
they do not prove that every pitch/time/duration/velocity/mute property remains equal, nor that the
notes sound as intended. Decision 99 adds the separate property readback; audible output remains
unproved. The bridge continues to admit Sunny's deterministic five-field note subset; Live's
optional probability, velocity-deviation, and release-velocity fields are not silently invented.

## 93. The five-field Live note profile does not erase retained SMF release velocity

**Decision:** Sunny's Score-to-Live bridge remains an exact deterministic five-field note subset:
pitch, start, duration, attack velocity, and mute. It does not accept or invent Live's optional
probability, velocity-deviation, or release-velocity members. The distinct low-level `MidiFile`
model continues to retain and rewrite Note Off velocity, but `midi_to_note_events` now returns
`TargetValueUnrepresentable` for any nonzero release velocity because its target type cannot carry
that evidence.

**Reason:** The [Clip reference](https://docs.cycling74.com/apiref/lom/clip/) exposes a richer
optional note dictionary than Sunny's protocol. That does not make unrequested stochastic state
part of a deterministic Score. It does expose an internal asymmetry: the SMF parser already retained
release velocity, while the analytical NoteEvent adapter silently erased it before Corpus/Score
ingestion. Calling `Note` "full performance metadata" further overstated the admitted algebra.

**Consequence:** Absence of probability/deviation/release state in a Score is now distinguished
from loss of explicit state in an imported MIDI file. The former is outside the declared model; the
latter fails closed at the exact projection boundary. `Note` is described as carrying Score-owned
metadata. A future extension can add these fields, but it must first define finite domains,
defaults, tie-chain ownership, notation/SMF residuals, serialization, Live protocol versioning, and
returned-note readback rather than opportunistically adding JSON keys.

## 94. A created Session Clip receives its own explicit meter

**Decision:** Protocol v8 admits `signature_numerator` and `signature_denominator` writes at the
canonical Clip path, with exactly the same numerator 1–99 and denominator {1, 2, 4, 8, 16} domain
as the Song properties. After `ClipSlot.create_clip`, Score compilation sets and immediately reads
back both properties on every new Clip. The earlier preflight remains mutation-free and proves the
initial Score meter lies in this shared target subset.

**Reason:** The current [ClipSlot
reference](https://docs.cycling74.com/apiref/lom/clipslot/) documents `create_clip(length)` only as
creation of a positive-length MIDI clip in an empty slot. It does not promise that the new object
inherits `Song.signature_numerator` or `Song.signature_denominator`. The separate [Clip
reference](https://docs.cycling74.com/apiref/lom/clip/) exposes both signature properties as
writable/observable. Sunny previously set Song meter and then relied on an unstated host default,
so a non-4/4 Score could have correct nominal note timing but an unproved Clip grid/meter.

**Consequence:** Session clip meter is now external state with retained equality evidence, not a
default assumption. A malformed live readback is `ProtocolError`; a well-typed divergent readback
is successful but unverified and incomplete. `time_signature_events_written` remains one because
it counts the initial source map event represented, not the number of target property setters;
per-Clip writes are counted and identified by `property_writes` and `property_deployments`. The
closed operation-algebra expansion advances the bridge from v7 to v8, preventing an older peer
from appearing compatible with a plan it cannot execute.

## 95. Scene launch cannot silently replace Sunny's Song tempo or meter

**Decision:** After ensuring Scene 0 exists and writing the initial Score tempo/meter to Song,
Ableton Score compilation writes `false` to Scene 0's `tempo_enabled` and
`time_signature_enabled` properties and immediately reads both back. These are the only Scene
operations admitted by protocol v8. A well-typed divergent readback makes compilation incomplete;
missing or malformed evidence is `ProtocolError` on a real transport.

**Reason:** The current [Scene reference](https://docs.cycling74.com/apiref/lom/scene/) specifies
independent tempo and time-signature values with enable flags. Only a disabled flag makes launch
use the Song's corresponding state. Sunny deliberately puts every generated Session clip in Scene
0, but previously neither observed nor controlled those flags. An existing scene override could
therefore replace the just-written Song tempo or meter as soon as the generated clips were fired.

**Consequence:** The generated Session row has an explicit Song-governed tempo/meter contract
instead of inheriting arbitrary pre-existing Scene launch behavior. Offline plans retain the two
commands without claiming execution; live deployments retain requested/observed Boolean equality.
Sunny still does not claim tempo- or meter-automation authoring: disabling a launch override is a
static playback precondition, and later Score map points remain explicit residuals.

## 96. A finite Score compiles to an explicit one-shot Clip interval

**Decision:** Every generated Session Clip receives `start_marker = 0`, `end_marker` equal to the
validated compiled clip length, and `looping = false`. All three writes use generic immediate
readback evidence. Protocol v8 admits only finite non-negative marker values and a Boolean loop
flag; the compiler's end is strictly positive because `create_clip` already requires that domain.

**Reason:** The current [ClipSlot
reference](https://docs.cycling74.com/apiref/lom/clipslot/) documents only a positive `length`
argument for `create_clip`. It states no initial loop or marker values. The separate [Clip
reference](https://docs.cycling74.com/apiref/lom/clip/) makes `looping`, `start_marker`, and
`end_marker` independently writable and defines a non-looped clip's length as the distance between
its markers. Sunny's Score has a finite structural duration, extended when rendered articulation
holds a note longer; leaving a created Session Clip looped could repeat that finite document
indefinitely and turn an accepted structural call into a false playback-duration claim.

**Consequence:** Sunny can now distinguish “Live accepted a positive creation length” from “the
resulting Clip was observed as the intended finite one-shot interval.” Marker or loop divergence is
retained as unverified/incomplete; malformed evidence fails the live compilation. This does not
invent launch intent: Clip launch mode, launch quantization, legato, and whether/when a user fires
the clip remain target/user state outside the current Score model.

## 97. Guarded snapshots include the Scene, Clip, Track, and Device state Sunny controls

**Decision:** Target snapshot schema 2 adds an exact ordered `scenes` array and extends every
occupied Clip record with signature numerator/denominator, start/end markers, loop state, and
activator state. Scene
records retain name, both override-enable flags, and their returned tempo/meter values. The native
parser now requires exact field sets at every object level; it rejects extra fields, malformed
conditional Scene values, invalid meter domains, non-finite or reversed markers, duplicate slots,
invalid Device type/activity evidence, and any mismatch between `scene_count`, the Scene array, and
each track's clip-slot count. Device records also retain `class_display_name`, `class_name`, public
type, and activity alongside their user-visible name.
Normal Track records retain `has_audio_output`, `has_midi_output`, `mute`, `solo`, and
`muted_via_solo` as exact Booleans.

**Reason:** Schema 1 guarded topology and a few stable Clip fields, but protocol v8 now deliberately
controls Scene launch overrides and Clip meter/active-one-shot state. A target could change any of those
properties between planning and application without changing scene count, track topology, clip
occupancy, name, type, or derived length. Exact stale-plan comparison would then be exact only over
an obsolete projection. Accepting unknown snapshot fields also contradicted the bridge's closed,
versioned evidence model.

**Consequence:** Pre-apply comparison and post-attempt observation cover the external playback
state for which Sunny now makes scalar claims. Disabled Scene overrides must return the documented
`-1` sentinel; enabled signatures must fit the admitted Live meter domain; Clip markers must be
finite and ordered. This remains an optimistic sequential main-thread observation, not an atomic
Live lock, and it still excludes volatile playhead/UI state and unmodelled device/audio behavior.

## 98. The full-score Session row carries the Score title

**Decision:** Protocol v8 admits `Scene.name`, and Ableton Score compilation sets Scene 0's name to
`Score.metadata.title` with immediate readback before creating the Part tracks and clips. The Scene
row denotes the complete finite Score; Part names continue to label tracks/clips, and top-level
Section starts continue to label CuePoints.

**Reason:** Score title is preserved by notation targets but previously disappeared from the
Ableton projection. The current [Scene reference](https://docs.cycling74.com/apiref/lom/scene/)
exposes a writable/observable name, and Sunny already chooses Scene 0 as the one row containing
every Part's full-score clip. Mapping the title there is direct and does not conflate the document
with one Part or one internal formal section.

**Consequence:** Ableton deployment now has readback evidence for document identity at the chosen
Session representation boundary. A divergent Scene name is incomplete, malformed live evidence is
a protocol error, and an offline plan remains `recorded_only`. Sunny still does not rename the Live
Set file itself or fabricate multiple section scenes; those would require separate target and
arrangement models.

## 99. Returned note IDs are used for exact five-property readback

**Decision:** After a real `add_new_notes` call returns its exact unique ID list, compilation calls
`get_notes_by_id` with those IDs and requests only `note_id`, `pitch`, `start_time`, `duration`,
`velocity`, and `mute`. Protocol v8 closes both the request and response shapes. The observed ID
set must equal the created set; each record must be exactly typed/ranged. Requested and observed
five-property tuples are compared as multisets. Results retain `observed_notes`, per-deployment
`properties_verified`, and aggregate `notes_verified`/`note_batches_verified` counts.
Decision 102 adds a total-Clip query after this ID-scoped proof.

**Reason:** The current [Clip reference](https://docs.cycling74.com/apiref/lom/clip/) documents both
the creation-ID return and an ID-based query whose caller may select returned properties. Creation
cardinality alone could not detect target-side quantisation or alteration. The reference does not
promise that queried notes preserve request ordering, so positional comparison would add an
unstated assumption. Querying all note members would also conflate Sunny's deterministic intent
with probability, velocity deviation, and release velocity that the Score does not own.

**Consequence:** Real deployment now distinguishes call acceptance, creation identity/cardinality,
and final equality of the exact five-field Sunny note profile. A well-formed property difference is
successful but warned and incomplete; malformed records, wrong IDs, duplicate IDs, or a wrong
cardinality are `ProtocolError`. Recording transports issue no query and claim no readback. Even a
fully verified note batch remains nominal MIDI/clip state: Live-12 tuning, instrument support, and
rendered audio are independent evidence boundaries.

## 100. A generated Score Clip is explicitly active

**Decision:** Protocol v8 admits the Boolean Clip `muted` property. After creating each Score Clip,
compilation sets and immediately reads back `muted = false`. Target snapshot schema 2 includes this
activator bit in every exact occupied-Clip record.

**Reason:** The current [Clip
reference](https://docs.cycling74.com/apiref/lom/clip/) defines `muted = true` as the Clip Activator
being off. `ClipSlot.create_clip` documents its empty-slot and positive-length preconditions but
does not promise an activator default. Note-level `mute = false`, exact note readback, correct
markers, and `looping = false` therefore still could coexist with a Clip that does not play.

**Consequence:** Sunny's finite Session projection now has scalar evidence that each generated Clip
is active as well as non-looping and correctly bounded. A divergent activator readback makes the
result incomplete; malformed evidence is a protocol error; a recording plan claims no readback.
The claim remains narrower than audible equivalence because Track mute/solo/routing, device state,
instrument availability, Live-12 tuning, and rendered output are separate boundaries.

## 101. Timbre source insertion verifies active instrument identity

**Decision:** A real protocol-v8 `Track.insert_device` response is a closed evidence object carrying
the requested name/index, exact before/after insertable-chain counts, observed device index,
`name`, `class_display_name`, `class_name`, public Device `type`, and `is_active`. Timbre source
compilation first requires a read-only count proving that the mixer-excluding target chain is
empty. The response also carries the enclosing Track's `has_audio_output` and `has_midi_output`.
Compilation retains insertion evidence as `device_deployments` and increments `devices_verified`
only when the count/index evidence is exact, the display class equals the requested Live UI name, the type is
instrument, the device is active, and Live classifies the Track as audio-output and not
MIDI-output. Recording transports retain the request with no observations
or verification. Snapshot schema 2 now guards the same identity/type/activity fields for every
device.

**Reason:** The current [Track
reference](https://docs.cycling74.com/apiref/lom/track/) specifies the availability and restrictions
of `insert_device`, while the [Device
reference](https://docs.cycling74.com/apiref/lom/device/) separately exposes display/runtime class
identity, role type, and activity. A successful method call alone did not prove what occupied
`devices/0`, whether it was an instrument rather than an effect, or whether it could participate in
audio generation. `Song.create_midi_track` also documents no empty-device-chain postcondition;
silently composing with default/existing devices would invalidate the compiler's source/effect
index model. These gaps sat directly between structural success and the project model's Timbre
source obligation.

**Consequence:** `devices_created` remains the narrow accepted-insertion count;
`devices_verified` is stronger target-state evidence. Missing or malformed live insertion evidence
is `ProtocolError`; an absent count is also `ProtocolError`, and a nonempty chain is rejected before
source insertion with `TargetValueUnrepresentable`. A well-formed wrong identity/type or inactive
source is retained, warned, and incomplete. A wrong Track output classification is treated the
same way. On a real transport, an unverified source cannot anchor dependent effect or parameter
addresses, so those mutations are skipped while their intent remains in requested counters and
deployment residuals; a recording plan may continue through modeled unexecuted structure. This
still does not prove source-parameter completeness, presets, routing, tuning,
nonzero rendered audio, or sonic equivalence. Mix effect insertion has not been promoted to this
source-specific verified counter and retains its existing structural/parameter evidence boundary.

## 102. Note verification proves total Clip membership

**Decision:** After the exact `get_notes_by_id` readback, protocol v8 calls
`get_all_notes_extended` with a closed dictionary containing only the same six return fields:
`note_id`, pitch, start, duration, velocity, and mute. The complete-Clip response must have the
requested cardinality and created-ID set, and its unordered six-field record multiset must equal the
ID-scoped response. Summary verification counters use this total response.

**Reason:** The current [Clip
reference](https://docs.cycling74.com/apiref/lom/clip/) exposes both ID-scoped and all-note queries,
including caller-selected return fields. Proving every returned creation ID had correct properties
did not prove the absence of an additional note. Such a note could arise from a host/default
assumption, concurrent target edit, or adapter defect while the earlier subset proof still passed.
The reference promises neither query ordering, so both comparisons must remain multiset-based.

**Consequence:** A verified batch now means that, at the paired readback boundary, the generated
Clip contains exactly Sunny's deterministic requested note set—no extra or missing records—and the
two public queries agree. Extra/missing notes, inconsistent query results, wrong IDs, or malformed
records are `ProtocolError`; a consistent well-formed property difference is retained and warned as
unverified. This remains sequential evidence rather than an atomic lock and does not prove future
stability, Live-only stochastic fields, tuning, device behavior, or rendered audio.

## 103. Timbre audio effects receive structural and active-state verification

**Decision:** Each enabled, materialised Timbre effect is inserted through the same closed
protocol-v8 evidence adapter as the source. Its target index must equal the observed pre-insert
count, the count must increase by exactly one, `class_display_name` must equal the requested Live UI
name, public Device type must be audio effect (2), the device must be active, and the Track must
remain audio-output and not MIDI-output. `effects_verified` counts records satisfying all
conditions; source and effects appear in insertion order in `device_deployments`.

**Reason:** Verifying the source did not transitively verify later mutations to its device chain.
An acknowledged effect insertion could still resolve to the wrong class/type, be inactive, or
leave the Track without the audio-output classification that makes downstream audio processing
meaningful. The same public Device/Track evidence and exact append algebra apply, so retaining a
weaker proof solely because the device role is “effect” was internally inconsistent.

**Consequence:** Timbre now distinguishes effect intent, accepted insertion, and verified active
target state through `effects_requested`, `effects_inserted`, and `effects_verified`. Missing or
malformed real evidence is `ProtocolError`; well-formed identity/type/activity/output divergence is
retained, warned, and incomplete. Recording plans retain deployment records with null observations
and zero verification. This still does not prove complete parameterisation, wet/dry behavior,
routing, rendered audio, or sonic equivalence. Decision 104 applies the same structural evidence
to Mix effects.

## 104. Mix effect insertion receives the same exact structural proof

**Decision:** Every nonempty materialisable Mix effect chain reads its target's mixer-excluding
device count, even when no effect has parameter mappings. Each enabled effect is inserted at the
exact observed append index through the shared closed evidence adapter. The result retains ordered
`device_deployments` and exposes `effects_verified`; verification requires the exact count/index
delta, requested display class, audio-effect type, activity, and audio-output/non-MIDI-output Track
classification. On a real transport, only a verified effect may anchor its mapped parameter paths;
otherwise those resolved deployments are retained without sending dependent writes. Recording plans
may continue through modeled unexecuted structure.

**Reason:** The prior Mix count read was conditional on parameter addressing. An unmapped effect
therefore had only call acknowledgement, and even a mapped parameter could coincidentally resolve
on a wrong/inactive device exposing the same parameter name. This made identical public
Device/Track evidence mean “required” for Timbre and “discarded” for Mix, contrary to the shared
external-alignment model.

**Consequence:** Mix now distinguishes `effects_requested`, `effects_inserted`, and
`effects_verified`, while `device_deployments` exposes each attempted channel, return, or master
append. Missing count or malformed live evidence is `ProtocolError`; well-formed divergence is
warned and incomplete; recording plans retain null observations and zero verification. Parameter
coverage/readback remains independent, but a live parameter mutation is gated by verified device
structure. GroupBus non-materialisation, routing residuals, automation, rendered audio, and sonic
equivalence remain explicit gaps.

## 105. Nested target evidence is closed, not merely its envelope

**Decision:** Every protocol-v8 target-evidence object is exact. Generic property evidence contains
only `property`, `requested`, and `observed`. Named DeviceParameter evidence contains only
`matched_name`, `original_name`, `property`, `requested`, `observed`, `minimum`, `maximum`,
`is_quantized`, `state`, and `automation_state`. An additional member is malformed evidence and
produces `ProtocolError`, just as a missing or incorrectly typed member does.

**Reason:** The outer request/response envelopes, snapshots, CuePoint evidence, note records, and
device-insertion evidence already enforced closed field sets, but the two most reused scalar
parsers checked only for required members. A peer could therefore widen those objects without a
protocol-version or model decision. That contradicted Sunny's closed operation algebra and made
forward-looking metadata indistinguishable from an unexamined target property that might alter the
meaning of verification.

**Consequence:** Target evidence cannot acquire semantics by accident: widening requires an
explicit protocol and compiler-model review. Adversarial tests inject a well-typed unknown member
into both generic and DeviceParameter responses and require failure. This is a conformance repair
within protocol v8, not a wire-version change, because v8 already specifies exact evidence and the
Remote Script emits precisely these established shapes. It does not strengthen immediate scalar
equality into durability, activity, routing, or audible-output proof.

## 106. The post snapshot discharges generated Track gate obligations

**Decision:** Aggregate project planning retains one Track-gate intent record per Score Part. Its
`expected_mixer_enabled` value is true exactly when the Mix channel is not muted and either no
project channel is soloed or that channel is soloed. After a successful executing apply, Sunny
evaluates the exact post snapshot and verifies a record only when the indexed Track has the Part
name, reports audio output and no MIDI output, retains the requested own `mute` and `solo` values,
and is not `muted_via_solo` whenever its Mix gate is expected enabled. Results expose requested and
verified counts, every observation, and divergence warnings. Recording plans retain the intent with
null observations and zero verification.

**Reason:** Immediate `mute`/`solo` equality does not determine the documented derived
`muted_via_solo` property. In particular, a soloed pre-existing Live Track can gate all newly
created unmuted/non-solo Parts. Snapshot schema 2 already captured that state and Track output
classification, but project apply merely returned the post snapshot without backpropagating it into
the compilation claim. Thus Sunny could have all scalar setters verify while Live explicitly
reported a generated channel as mixer-gated.

**Consequence:** A well-formed post-state contradiction keeps command application successful but
makes the aggregate result warned and incomplete. Sunny does not clear unrelated solo state, and it
does not demand `muted_via_solo = true` for a channel the project intentionally excludes: exact own
mute/solo evidence already preserves that intent, while derived exclusion can depend on broader
Live routing/solo semantics. Malformed post evidence fails as `ProtocolError`. Verified Track gates
establish identity, output role, and the selected mixer blockers only; device signal, routing to the
master, nonzero rendered audio, and audible/sonic equivalence remain unproved.

## 107. Redundant Mix routing fields are exact mirrors

**Decision:** Validation rule X10 requires every `GroupBus.member_channels` entry to name an
existing ChannelStrip whose `group_assignment` names that same GroupBus exactly once, and every
assigned channel must appear exactly once in that group's list. The same bidirectional rule applies
between a parent's `member_groups` and a child GroupBus whose `output` names that parent. Missing,
dangling, duplicate, or contradictory edges produce error 7035
(`InconsistentRoutingMembership`) and block compilation.

**Reason:** The Mix model intentionally stores convenient forward and reverse graph references,
and its workflows update both. Validation previously used `ChannelStrip.group_assignment` and
`GroupBus.output` for reachability while DAG construction also consumed the member lists, without
requiring agreement. A serialized or directly constructed graph could therefore denote two
different signal flows, and downstream code could silently select whichever representation it
happened to traverse.

**Consequence:** Static reachability, route inventory, workflows, serialization, and target
projection now share one coherent graph. Adversarial tests cover one-sided, duplicate, and
wrong-parent edges. This does not make GroupBus deployment available in Live; it ensures the
unrepresentable external route is at least derived from an internally unambiguous intent.

## 108. Live output routing is an explicit unresolved target edge

**Decision:** Mix results expose `output_routes_requested`, `output_routes_written`,
`output_routes_verified`, and deterministic `output_route_residuals` for every Channel, Group, and
Aux output edge. The current Ableton compiler records all intent and reports zero writes and zero
verification with an incompleteness warning. It does not treat `has_audio_output` or a newly created
Track/Return's current default as evidence that the edge reaches Sunny's MasterBus.

**Reason:** The current [Track
reference](https://docs.cycling74.com/apiref/lom/track/) exposes output routing as separate
get/set dictionaries whose selected values must come from target-owned available-routing
collections. It documents neither a stable universal Master identifier nor a creation-time Master
route postcondition. Audio-output classification distinguishes an audio-producing track from a
MIDI-output track; it says nothing about whether that audio is sent to Master, another track,
Sends Only, or an external output.

**Consequence:** Even a simple otherwise verified Mix is incomplete until its output edges are
mapped and observed. A future route writer must discover a unique semantic target without guessing,
use exact dictionary set/readback evidence, review protocol and snapshot schemas, and pass tests in
a named Live build, including renamed/reordered targets. Sunny deliberately does not mutate an
ambiguous route or claim that a host default realises the Mix DAG. Send-level verification and
Track gate/output-role verification remain useful but independent evidence.

## 109. Generic LOM evidence preserves numeric categories

**Decision:** A generic property response must preserve the request's scalar category in both its
echo and observation. Signed/unsigned JSON integers remain integral and compare exactly; a
floating request requires finite floating echo/observation and uses the existing relative
tolerance. Boolean and string categories remain exact. A JSON float near an integer is not valid
integer evidence.

**Reason:** The LOM distinguishes `int`, `float`, `double`, `bool`, and `symbol`, but the native
evidence helper previously collapsed all JSON numbers into double comparison. Thus an integer
property such as `signature_numerator = 4` could be “verified” by `4.000001`, and even its purported
request echo could silently change categories. Numeric proximity cannot repair a malformed target
type or prove that a closed integral host domain was preserved.

**Consequence:** Floating target quantisation remains distinguishable from wire malformation, while
integer meter/gate/index-style state cannot acquire fractions. Adversarial tests cover a fractional
integer observation and fractional request echo. The change tightens protocol-v8 evidence to the
already declared LOM types and requires no wire-version change.

## 110. Max notifications may invalidate plans but cannot make them atomic

**Decision:** Sunny retains no `observe`/`unobserve` protocol operations in v8. A future Max for
Live observation target may use `live.observer` notifications to mark a cached plan stale, but it
must not reconcile Live directly in the notification or claim that observation closes the
snapshot-to-mutation race. Any reaction is a separately ordered deferred action with its own target
precondition and evidence.

**Reason:** Cycling '74's current Max for Live API overview states that Live Set changes cannot be
triggered directly from notifications and recommends `deferlow` for a subsequent API interaction.
That transition moves work to a later queue event; it does not supply a lock, transaction, or
compare-and-swap, and another target edit can occur between notification and reaction. An observer
also introduces lifecycle questions absent from Sunny's sequential request/response protocol.

**Consequence:** The existing exact pre-apply poll, guarded mutation stream, journal, and post poll
remain the strongest implemented concurrency evidence. Reintroducing subscriptions requires an
asynchronous channel plus specified initial-value semantics, ordering/coalescing, object deletion,
unsubscribe, reconnect/resubscribe, causality, backpressure, and deferred-write behavior. Max
observation can improve invalidation latency; it cannot be reported as atomic deployment or safe
self-healing.

## 111. Group workflows are closed under the exact-mirror invariant

**Decision:** `create_group_bus` rejects duplicate requested channel IDs before mutation and, on
success, removes each requested channel from every existing GroupBus member list before assigning
it once to the new group. `assign_channel_to_group` likewise removes every stale or duplicate
reverse edge, rather than trusting only the channel's declared previous assignment, before adding
the one requested edge.

**Reason:** X10 made contradictory forward/reverse routing invalid, but the authoring workflows
could still create that invalid state themselves. Creating a second group around an already-grouped
channel overwrote `group_assignment` while leaving the first group's member list intact; duplicate
input was copied verbatim. Reassignment cleaned only the declared previous group, so imported stale
edges survived a supposedly corrective operation.

**Consequence:** Every successful group workflow leaves the affected channel with one exact
bidirectional edge. Missing/duplicate input failures are transactional, and adversarial tests cover
duplicate creation, implicit moves, and repair of stale duplicated listings. X10 still rejects any
unrelated inconsistency in imported or directly constructed graphs; the workflow does not silently
certify the whole document.

## 112. Device insertion evidence retains the requested public type

**Decision:** Every `AbletonDeviceInsertionDeployment` and its Timbre, Mix, and project MCP
projection retains `requested_type` in addition to requested name/index, observed type, and the
derived `type_verified` verdict. The admitted requested values remain Device type 1 for an
instrument and type 2 for an audio effect.

**Reason:** The public Device model exposes display identity, runtime class, type, and activity as
separate facts. Sunny compared the observed type against an internal function argument but then
discarded that argument. A stored Boolean verdict without its proposition is not independently
auditable and makes later post-state evaluation depend on recompiling hidden context.

**Consequence:** Result consumers can now reconstruct the exact type proposition and distinguish
an instrument request from an audio-effect request without trusting Sunny's Boolean alone. This is
result-evidence closure, not a wire change: the Remote Script insertion response already returns the
observed type, and protocol v8's request algebra is unchanged.

## 113. Final snapshots re-evaluate every planned native insertion

**Decision:** Aggregate postconditions retain one Device record per planned Timbre or Mix native
insertion. An executing apply verifies it only when snapshot schema 3 reports the requested
mixer-excluding device index with the exact requested display identity and public type, active, and
inside a chain whose final cardinality exactly equals the last planned append. Results expose
`devices_requested`, `devices_verified`, and all requested/observed fields through the project MCP
surface. Recording plans retain requests and null observations.

**Reason:** Immediate insertion readback occurs before later Timbre/Mix operations and is not a
durability guarantee. A later mutation or external edit can remove, move, rename, deactivate, or
replace the device; checking only the indexed device would also let an unmodelled extra processor
alter the chain while every requested index still matched. The existing exact post snapshot already
contains every documented Device fact needed for this structural check, so treating it as
intractable would discard available evidence.

**Consequence:** Well-formed final divergence leaves command application completed but warns and
makes the aggregate incomplete. An extra or missing device invalidates every insertion obligation
for that containing chain; malformed stored paths/types/cardinalities are `ProtocolError` rather
than ordinary Live divergence. This closes final Device structure only. Parameter durability,
routing, presets/resources, rendered signal, and sonic equivalence remain unproved, and unsupported
source intent with no insertion attempt remains represented by its existing requested/created
counters and warnings rather than a fictional Device postcondition.

## 114. Final snapshots re-evaluate generated Clip structure, not notes

**Decision:** Aggregate postconditions retain one slot-0 Clip record per Score Part and, after an
executing apply, compare final name, MIDI role, marker interval, documented unlooped `length`,
initial signature, loop flag, and activator state. The requested values are reconstructed from the
Score compiler's exact stored property deployments; a missing, duplicate, or contradictory stored
intent is `ProtocolError`. Results expose `clips_requested`, `clips_verified`, and all records
through project MCP.

**Reason:** Immediate scalar and note readback occurs before Timbre and Mix deployment and cannot
prove that the Clip structure survived the whole command sequence. Snapshot schema 3 retains
the documented structural fields. For an unlooped Clip the public reference defines
`length` as the distance from start marker to end marker, so Sunny can verify it without inventing a
second duration model. Recompiling duration separately would risk divergence from articulation- or
grace-extended note timing already resolved by the Score compiler.

**Consequence:** A missing Clip, audio Clip substitution, renamed Clip, changed meter/markers,
looping, activator state, or inconsistent derived length makes the completed deployment warned and
incomplete. Offline plans retain intent with null observations. Snapshot schema 3 does not contain
notes, so the complete-note query remains insertion-time evidence; final note membership, signal
production, launch timing, and audible equivalence remain unproved.

## 115. Final snapshots re-evaluate Song, Scene, and projected Cue state

**Decision:** Aggregate postconditions reconstruct the requested initial Song tempo/signature and
Scene-0 name/disabled tempo-meter override flags from exact Score property deployments, then compare
them with the final snapshot. They also retain every projected top-level section cue and verify it
only when exactly one final CuePoint lies within the adapter's `1e-7` beat identity window and has
the requested name. MCP exposes Song-state and Cue requested/verified counts and records.

**Reason:** Immediate setter and safe-cue responses occur before the rest of project deployment.
Returning a later snapshot without interpreting its already-modelled Song, Scene, and Cue fields
allowed a subsequent edit to invalidate global playback time/grid or section navigation while the
aggregate still exposed only earlier success. A time-only Cue search without uniqueness would also
silently choose among an adversarial duplicate pair.

**Consequence:** Later tempo/meter changes, Scene override re-enablement/renaming, missing or renamed
CuePoints, and ambiguous same-window cues warn and make the completed deployment incomplete.
Malformed stored intent is `ProtocolError`. Unrelated extra cues remain outside Sunny's ownership;
later tempo/meter automation, nested SectionMap hierarchy, launch quantisation, playback, and audio
remain independent residuals.

## 116. Snapshot schema 3 closes final mixer scalar state

**Decision:** Target snapshot schema 3 adds a closed `mixer` record to every normal, Return, and
Master Track. It contains volume and panning plus the sends collection; every DeviceParameter record
contains finite internal `value`, finite numeric `display_value`, `state`, and `automation_state` in
the closed 0–2 domain. A normal Track must expose exactly one send per observed Return Track.
Aggregate postconditions collapse Score then Mix property deployments by exact path/property and
verify each final mixer intent only when the selected value matches and both states are zero.

**Reason:** Immediate scalar readback can be invalidated later in the same aggregate deployment or
by an external edit. It also cannot make an equal value effective when the DeviceParameter is
inactive or controlled/overridden by automation. MixerDevice publicly owns volume, panning, and one
send per Return, while DeviceParameter publicly distinguishes internal/display values and the two
states; these facts are therefore tractable in the existing synchronous post observation. Adding
them changes the closed snapshot shape and requires an explicit schema version.

**Consequence:** Pre-apply equality now detects selected mixer changes, state changes, and
Track/Return send-cardinality changes. Final project results expose
`mixer_properties_requested`/`mixer_properties_verified` and exact records; value divergence,
inactive state, or automation warns and incompletes. Protocol v8 remains unchanged because the
snapshot carries its own schema discriminator. Schema 2 payloads fail closed. This still proves no
output-routing destination, pre/post send mode, parameter range, nonzero signal, or sonic
equivalence.

## 117. Aux-send identity and Group send loss are explicit

**Decision:** Validation rule X11 requires every ChannelStrip and GroupBus Aux-send level to be
finite and permits at most one send record per source/AuxBus pair, including disabled records. X6
now checks GroupBus as well as ChannelStrip send targets. Ableton compilation counts every enabled
GroupBus send in `sends_requested`, configures zero of those sends, and emits a dedicated warning
because the Group Track is not materialised.

**Reason:** The formal model already said send levels were finite, but runtime validation did not
enforce it. Duplicate direct/deserialised records denoted two competing final values for one Live
`MixerDevice.sends[index]` address, and Group sends were neither target-validated nor counted by the
compiler. Snapshot-v3 send cardinality cannot rescue an ambiguous or silently erased source model.

**Consequence:** Invalid send algebra fails before target access with error 7036
(`InvalidAuxSend`), while missing targets retain X6's referential error. Workflow-created channel
sends remain unique by update semantics; hostile direct graphs are independently rejected.
Unsupported GroupBus send intent is now loss-accountable without inventing a Track path. This does
not implement Group Tracks, pre-fader mode, or output routing.

## 118. Note-verification evidence retains the proposition it evaluates

**Decision:** Every requested nonempty Part note batch produces an `AbletonNoteDeployment` that
retains and exports the exact requested deterministic
note tuples—pitch, Live-beat start and duration, velocity, and mute—beside the created IDs, observed
tuples, and verification flags. Recording-only plans retain the requested tuples, and a target below
Live 11 retains them with the distinct `unsupported` action; both keep their created-ID and
observed-note collections empty and all execution/readback verdicts false.

**Reason:** `properties_verified` previously exposed a Boolean conclusion and the observed Clip
records while discarding the requested property multiset used to reach it. A consumer could not
independently reconstruct that comparison, distinguish two different requested performances with
the same cardinality, or use the compilation record as the exact intent for a later final-state
query. A count is not a substitute for the proposition being verified.

**Consequence:** Score and aggregate project MCP results now carry self-contained immediate note
evidence for executing, offline, and capability-skipped batches. The exported tuples are the already-compiled
Live-domain payload after grace/articulation projection, so no second timing model is introduced.
This is a result-schema closure, not a protocol-v8 wire change, and it does not by itself prove
post-deployment persistence, active tuning, device support, signal production, or audible equality.

## 119. Aggregate note durability is a separate final read-only obligation

**Decision:** After all guarded Score, Timbre, and Mix mutations and the post-deployment structural
snapshot, an executing project apply issues one closed `Clip.get_all_notes_extended` query for each
inserted note batch. It verifies only when the final unique note-ID set equals the IDs returned by
insertion and the unordered pitch/start/duration/velocity/mute multiset equals the compiler-retained
request. Results expose final batch/note requested and verified counts plus exact requested IDs,
requested tuples, observed records, and identity/property verdicts. Unsupported batches remain
requested but are not queried or claimed verified.

**Reason:** Snapshot schema 3 deliberately omits potentially large note collections, while the
documented Clip API already provides a selective complete-note query. Immediate readback proves the
state only before Timbre and Mix deployment. The guarded plan also previously misclassified
read-only note queries as mutations, even though their exact ID arguments can depend on the real
insertion response and cannot be predicted by an offline recorder.

**Consequence:** Valid read-only GetProperty operations and the admitted target/profile/device/note
query methods are forwarded through `JournaledLomTransport` without entering or competing with the
dry-run mutation sequence; they remain protocol-validated. A well-formed extra, missing, replaced,
or property-changed final note set warns and makes the completed aggregate incomplete. A malformed
closed response is `ProtocolError`, and a failed query is `SendFailed`, after retaining the mutation
journal. The snapshot and note query are sequential observations rather than an atomic transaction,
and neither created-ID/property equality nor query order proves tuning, device support, nonzero
signal, playback timing, or audible equivalence. Protocol v8 is unchanged because it already admits
the exact complete-note query.

## 120. A configured send level is not a configured Aux send

**Decision:** Mix results split every enabled Channel/Group Aux-send request into level and
pre/post-mode obligations. `send_levels_requested`/`send_levels_configured` report the admitted
`MixerDevice.sends[index].display_value` projection; `send_modes_requested`/
`send_modes_configured` report the independent Boolean mode. `sends_configured` now counts only
complete logical sends for which both obligations were applied, so it remains zero on the current
public bridge. Every materialised Channel send warns for its explicit requested `pre-fader` or
`post-fader` mode; Group sends retain both unconfigured obligations.

**Reason:** `AuxSendLevel.pre_fader` is not optional intent: false requests post-fader behavior just
as true requests pre-fader behavior. Writing and reading back only the level cannot establish either
mode, and a newly created Track's default mode is not an admitted postcondition. The previous
`sends_configured++` therefore converted a partial scalar projection into a false claim that the
whole send was configured, while warning only for the non-default-looking Boolean value.

**Consequence:** Consumers can distinguish exact level-call coverage from semantic send coverage,
and `complete` remains false whenever an enabled send mode cannot be deployed. Snapshot schema 3
may still verify final send level/value/activity/automation; it does not promote that scalar into
mode, routing, signal, or audible-send proof. No wire operation or protocol version changes.

## 121. Nested GroupBus authoring is closed under the routing invariant

**Decision:** The core and MCP surfaces now expose `assign_group_to_group` and
`route_group_to_master`. Both operate transactionally on a candidate graph, remove every stale or
duplicate reverse `member_groups` edge for the child, install exactly one mirrored child-output /
parent-membership edge (or a direct Master output), and commit only if complete Mix validation has
no error.

**Reason:** X10 already required `GroupBus.output.parent_group_id` and its parent's
`member_groups` list to be exact mirrors, X2 required an acyclic routing DAG, and X3b bounded nesting
depth. Yet Sunny's authoring API could create only Channel→Group edges; valid nested groups had to
be assembled by direct field mutation or deserialisation. A supposedly complete model whose legal
state cannot be produced through its public workflows is internally misaligned, and naïve
single-sided mutation would recreate the contradiction fixed for Channels in Decision 111.

**Consequence:** Valid nesting, moves, stale-edge repair, and return-to-Master routing are expressible
without bypassing invariants. Missing groups, self/cyclic routes, excessive depth, or any other
resulting structural error leave the original graph unchanged and return the exact validation error.
This improves Sunny's internal model and residual accounting; it does not make Ableton's public LOM
capable of materialising or assigning Group Tracks.

## 122. DeviceParameter deployment evidence retains its complete proposition and action

**Decision:** Every Timbre and Mix parameter deployment retains and exports its requested mapping
range; exact requested property/name; resolved current/original name; observed value and internal
bounds; quantisation, enabled, active, and automation facts; and an explicit `not_applied`,
`recorded_only`, or `set` action. Protocol-v9 setter evidence is closed at eleven members by adding
`is_enabled` to the prior result.

**Reason:** The public DeviceParameter model exposes identity, internal range, quantisation,
enabled state, active state, automation state, and internal/display values as independent facts.
The bridge already returned most of them, but compiler results discarded the resolved identity,
bounds, and quantisation; they also discarded the requested range used to validate an internal
mapping. A null observation could mean either an offline recorded write or a dependent write that
was never attempted because its Device was unverified. A Boolean verdict without that proposition
and action is not independently auditable evidence.

**Consequence:** Immediate parameter results are self-contained across direct Timbre, direct Mix,
and aggregate MCP surfaces. A real setter response missing, extending, or mistyping any of its
eleven fields is `ProtocolError`; recording and unmaterialised targets retain intent without
claiming target facts. Immediate scalar equality still proves only the sampled value, and
inactive/automated state still warns rather than becoming a durable or audible claim.

## 123. Protocol v9 adds selective non-repairing DeviceParameter durability evidence

**Decision:** The closed bridge advances from protocol v8 to v9 and admits
`sunny_get_device_parameter(name, property)` only on canonical `devices/N` paths. Its exact
ten-member result contains resolved current/original identity, selected internal/display property,
observed scalar, internal minimum/maximum, quantisation, enabled state, active state, and automation
state. It never writes, re-enables, clears automation, or rejects a well-formed disabled/inactive/
non-changeable/automated parameter merely for that divergent state. After the final structural
snapshot, an executing project apply queries every actually set Timbre/Mix mapping whose containing
Device verified. Results retain `device_parameters_requested`/`device_parameters_verified` and
one complete obligation/observation record per logical mapping.

**Reason:** Immediate setter readback precedes later project mutations and cannot prove durability.
Snapshot schema 3 deliberately closes the bounded mixer parameters but not arbitrary, potentially
large Device parameter collections. Reusing the setter for verification would repair the state
being tested. Cycling '74 documents `name`, `original_name`, `value`, `display_value`, `min`, `max`,
`is_quantized`, `is_enabled`, `state`, and `automation_state` as distinct public DeviceParameter
facts, so a selective read is tractable without inventing an all-device snapshot schema. Widening
the closed method/response algebra without advancing the protocol would contradict the bridge's
own version discipline.

**Consequence:** Final verification requires exact resolved identity, selected-value equality,
enabled state, `state = 0`, and `automation_state = 0`; internal-value mappings additionally require
final min/max equality. For display-value mappings the returned internal bounds remain evidence but
the range verdict is null because they are not the GUI-visible mapping interval. Well-formed value,
range, enabled, active, or automation divergence warns and leaves a completed deployment
incomplete. Malformed evidence is `ProtocolError`; a failed lookup is `SendFailed`; the mutation
journal survives both. The validated read bypasses the guarded mutation plan and journal. Snapshot,
note, and parameter observations are sequential rather than atomic, and prove neither modulation,
signal production, nor sonic equivalence. Named-build Live validation remains required.

## 124. Protocol v10 backpropagates bounded Live pitch context into snapshot schema 4

**Decision:** The closed bridge advances from protocol v9 to v10, and target snapshot schema 3
advances to schema 4. On Live 12.0.5+, every snapshot must contain the exact four-field Song scale
record `root_note`, `name`, `intervals`, and `mode`; on Live 12.1+, it must also contain the exact
two-field TuningSystem subset `name` and `pseudo_octave_in_cents`. Earlier target profiles must use
explicit nulls. These records participate in stale-plan equality and are retained in aggregate
final Song evidence. The evidence explicitly reports that full tuning definition, per-track tuning
bypass, instrument support, and audible pitch are not verified.

**Reason:** Live distinguishes current scale state from the TuningSystem that reinterprets MIDI
note indices. The Song reference fully types root, name, interval list, and scale-mode state; Live
12.0.5 release notes identify the Max for Live `scale_mode` addition. Live 12.1 release notes and the
TuningSystem reference fully type its name and pseudo-octave. By contrast, the public reference does
not close the member schemas of the lowest/highest/reference-pitch dictionaries, exposes no Track
Bypass Tuning property, and cannot prove whether an instrument or MPE target follows the tuning.
Keeping only a generic warning discarded tractable external state; importing all tuning semantics
would invent a wire model that the reviewed documentation does not specify.

**Consequence:** A scale/tuning change in the observed subset now invalidates a stale plan and is
visible in final MCP evidence. Version mismatch, Boolean-as-integer intervals, empty/oversized
interval lists, invalid roots, non-finite/nonpositive pseudo-octaves, missing fields, or unknown
members fail with `ProtocolError`. The Remote Script does not probe unsupported older versions.
Sunny still writes zero key/tuning events and retains its Live-12 audible-pitch warning: tuning
dictionaries, Track bypass, device/MPE behavior, rendered audio, and frequency equivalence remain
explicit residuals requiring named-build Live validation.

## 125. Protocol v11 removes Clip groove from the stored-note proof boundary

**Decision:** The closed bridge advances from protocol v10 to v11 and admits exactly one new Clip
write: `groove` with the null object. Score compilation issues that write on Live 11+ immediately
after establishing the active finite Clip interval and requires the generic property evidence to
echo and observe null. Target snapshot schema 4 advances to schema 5 by adding `has_groove` to every
occupied Clip: an exact Boolean is required on Live 11+, while older modeled targets require null
and are not probed. Aggregate generated-Clip postconditions require final groove absence and export
the proposition, observation, and verdict. They also export explicit false rendered timing and
velocity verdicts.

**Reason:** `Clip.add_new_notes` and its complete-note queries establish stored note position and
velocity, not necessarily the values used at playback. Cycling '74 exposes `Clip.groove` and
`has_groove` independently from the note dictionaries. Ableton's groove documentation says an
associated groove can apply straight quantisation, proportional timing displacement, per-voice
random offsets, and velocity transformation in real time, scaled by the Set's global groove
amount. `ClipSlot.create_clip(length)` specifies no initial groove state. Thus exact note readback
plus an assumed host default was externally misaligned. The null association is the smallest
closed, tractable invariant; importing Groove base/timing/random/velocity parameters would be
unnecessary once no Groove object is associated.

**Consequence:** A generated Live 11+ Clip cannot silently acquire a documented non-destructive
groove transformation while still verifying. An existing occupied Clip's association change also
breaks exact snapshot equality. Null/object-type divergence or malformed/version-incoherent
`has_groove` evidence is `ProtocolError`; a final true value warns and makes the generated Clip
postcondition incomplete. Sunny still does not call `Clip.quantize` or launch the Clip, and no-groove
evidence does not constrain launch velocity, device/MIDI processing, latency, routing, scheduler
behavior, signal production, or rendered audio. Named-build validation must confirm that the
version-coupled Python wrapper maps the public null-object setter to `None` as modeled.

## 126. Protocol v12 retains bounded Device latency without inventing a render-path sum

**Decision:** The closed bridge advances from protocol v11 to v12 and target snapshot schema 5
advances to schema 6. Every observed top-level mixer-excluding Device and every successful native
Device insertion must include `latency_in_samples` as a non-negative signed-LOM integer and
`latency_in_ms` as a non-negative finite floating value. The pair participates in exact snapshot
equality and is retained by immediate and aggregate Device evidence. Results independently export
`reported_latency_observed` and the explicit false residual
`render_path_latency_fully_observed`; generated Clip evidence keeps
`audible_timing_verified = false`.

**Reason:** Cycling '74 exposes both latency properties as independent get/observe Device facts,
so discarding them loses tractable external state that can change plan assumptions. They do not,
however, define total playback latency. Ableton documents automatic Device Delay Compensation,
Reduced Latency When Monitoring, and Track Delay as distinct timing mechanisms. The public Song and
Track LOM pages expose none of those control states. Max for Live devices can also declare latency
to the host for compensation. Audio-interface buffers and drivers, parallel
or return routing, Max editor state, external hardware, and acoustic propagation add further
unclosed variables. Summing top-level Device reports would therefore be a false system model rather
than a conservative approximation.

**Consequence:** A changed reported Device latency invalidates a guarded plan and remains visible
after deployment. Missing, extended, Boolean, negative, non-integral sample, integral-millisecond,
non-finite, or out-of-signed-int evidence is `ProtocolError`; no target default is substituted.
Device structural verification additionally requires that the bounded report was present, but no
part of the result calls the pair a total, compensated, or audible latency. Named-build validation
must confirm the private Python wrapper's concrete types and update behavior. A future rendered-
timing claim requires an explicit playback/render experiment or a larger model covering complete
routing, compensation/monitoring/Track Delay state, buffers, drivers, external endpoints, and the
measurement reference event.

## 127. Protocol v13 replaces assumed one-shot playback with bounded Clip launch evidence

**Decision:** The closed bridge advances from protocol v12 to v13 and target snapshot schema 6
advances to schema 7. For every generated Live-11+ Session Clip, Score compilation sets and reads
back the exact tuple `launch_mode = 0` (Trigger), `launch_quantization = 1` (None),
`legato = false`, and floating `velocity_amount = 0.0`. Snapshot schema 7 adds those four fields to
every occupied Clip: Live 11+ requires the exact documented types and domains; older modeled
targets require null and are not probed. Aggregate Clip evidence retains requested and observed
values and exports `launch_behavior_verified`. The current adapter independently exports
`follow_actions_observed = false`, `one_shot_playback_verified = false`, and
`audible_timing_verified = false` under this adapter.

**Reason:** An unlooped marker interval does not specify how a Session Clip is launched. Cycling
'74 exposes four independent Live-11+ launch properties. Ableton documents that Trigger ignores
release, Gate stops on release, Toggle stops on the next press, Repeat retriggers at the clip
quantization rate, Legato inherits another Clip's play position, clip quantization can delay onset,
and launch velocity can scale Clip volume. The manual also documents Follow Actions that can stop,
restart, or launch other clips and can override loop/region behavior. The reviewed public Clip LOM
contains no Follow Action properties. `ClipSlot.create_clip(length)` specifies none of these
postconditions, and Sunny neither fires the Clip nor observes a playback trace. Therefore the
“finite one-shot interval” wording in Decisions 96 and 97 exceeded the external model even though
the marker/loop scalars themselves were correctly read back.

**Consequence:** Current specifications call the generated range a *finite unlooped structural
interval*. Trigger/None/Legato-off/velocity-zero removes four documented sources of launch
ambiguity without pretending to disable or inspect Follow Actions. Missing, extended,
Boolean-as-integer, out-of-domain, integral-velocity, non-finite, or version-incoherent launch
evidence is `ProtocolError`; well-formed final divergence warns and makes the Clip postcondition
incomplete. Existing groove and Device-latency residuals remain unchanged. A future one-shot or
audible-timing claim requires a public and closed Follow Action model (or a deliberately scoped
named-build adapter for it) plus an explicit launch/playback/render experiment with a declared
reference event; scalar configuration alone is insufficient.

## 128. MIDI Clip bank/program launch state remains an explicit public-LOM residual

**Decision:** Generated-Clip postconditions add two independent false residuals without widening
bridge protocol v13 or snapshot schema 7: `midi_bank_program_state_observed = false` and
`program_change_suppression_verified = false`. These fields do not affect the bounded structural
Clip verdict and cannot be promoted by exact note, launch, groove, Device identity, or
DeviceParameter evidence.

**Reason:** Ableton's Clip View manual states that launching a MIDI Clip can send bank,
sub-bank, and Program Change messages to external devices and supporting plug-ins, allowing each
Clip to select a different sound. It instructs users to leave the choosers at “—” to suppress those
messages. The current public Cycling '74 Clip LOM reference exposes no bank, sub-bank, or program
property, and `ClipSlot.create_clip(length)` specifies neither those fields nor their default.
Assuming the UI default would make Timbre identity/parameter readback appear stronger than the
documented launch path: a later Program Change could select different target state at playback.

**Consequence:** Sunny issues no undocumented or private bank/program setter and does not call a
clearing method that the public model lacks. Results now make the missing observation and
suppression proof machine-readable. A named-build experiment can characterize creation defaults
but cannot turn a private/default behavior into a portable public-LOM guarantee. Closing this gap
requires Ableton to expose the state publicly, a deliberately version-scoped private adapter with
explicit compatibility evidence, or a playback/MIDI-output experiment that observes the absence
of bank/program messages at a declared boundary.

## 129. Protocol v14 clears and verifies generated Clip envelopes

**Decision:** The closed bridge advances from protocol v13 to v14 and target snapshot schema 7
advances to schema 8. Before inserting notes, Score compilation sends the exact no-argument
`sunny_clear_all_envelopes` call for every freshly generated Session Clip. The adapter invokes the
public `Clip.clear_all_envelopes()` method, immediately reads `Clip.has_envelopes`, and returns an
exact one-member object containing that Boolean. Results retain requested, executed, and verified
clear counts plus one per-Part deployment record. Snapshot schema 8 adds exact Boolean
`has_envelopes` to every occupied Clip; aggregate generated-Clip verification requires false.

**Reason:** Ableton documents Clip Envelopes as time-varying control data that can automate or
modulate mixer/device controls and carry MIDI controller data. Exact note dictionaries, static
mixer/device readback, and `ClipSlot.create_clip(length)` do not prove that this independent layer
is absent. Unlike the bank/sub-bank/program residual, the public Clip model exposes both a clearing
operation and an observable absence proposition. Treating fresh-Clip defaults as neutral would
discard a tractable target fact; treating a successful void method as verification would confuse
accepted mutation with observed state.

**Consequence:** A real transport must return exactly Boolean `has_envelopes`; missing, extended,
or category-crossing evidence is `ProtocolError`. A well-formed true immediate or final observation
warns and leaves the project incomplete. Recording-only plans preserve the clear request with null
observation and claim neither execution nor verification. Sunny clears every envelope only on the
fresh Clip that it creates; it does not author Score automation or infer equivalence for MPE note
expression, modulation outside the Clip, external controllers, Device behavior, signal production,
or rendered sound.

## 130. Per-note MPE expression remains distinct from Clip-envelope absence

**Decision:** Generated-Clip postconditions add two independent false residuals without widening
bridge protocol v14 or target snapshot schema 8:
`mpe_note_expression_state_observed = false` and
`mpe_note_expression_neutrality_verified = false`. These fields cannot be promoted by exact
ordinary-note membership, `Clip.has_envelopes = false`, nominal pitch context, or static
Device/parameter evidence.

**Reason:** Ableton's MPE manual documents note-owned Pitch, Slide, and Pressure expression
envelopes and says MPE data can be viewed and edited for notes in every MIDI Clip regardless of how
the Clip was created. The current public Cycling '74 Clip note dictionaries expose ordinary note
identity, pitch/time/duration, velocity/mute, probability, velocity deviation, and release
velocity, but no MPE curve representation or expression-clear operation. The public
`clear_all_envelopes()` method is documented as removing Clip automation; equating it with the
manual's separate per-note expression envelopes would collapse two different host models.
`add_new_notes` also specifies no neutral MPE postcondition.

**Consequence:** Sunny's exact five-field note verdict remains reconstructable and valid for that
selected dictionary subset, but cannot imply neutral per-note pitch bend, slide, pressure, audible
pitch, or timbre. No private/UI operation is invented. A portable closure requires Ableton to
expose a typed public MPE note schema and neutralization/readback operation. A deliberately
version-scoped adapter, MIDI-output observation, or rendered-audio experiment may establish a
narrower named-build proposition, but must declare its endpoint and device behavior.

## 131. Protocol v15 closes the ordinary deterministic Live note tuple

**Decision:** The closed bridge advances from protocol v14 to v15 while target snapshot schema 8
remains unchanged. Every `Clip.add_new_notes` specification now contains exactly eight fields:
Sunny's pitch, start time, duration, velocity, and mute state plus adapter-owned floating
`probability = 1.0`, `velocity_deviation = 0.0`, and `release_velocity = 64.0`. Both immediate
queries and the aggregate final query request exactly those eight properties plus `note_id`.
Requested and observed deployment records retain all values, and multiset equality covers the
complete nine-member readback record modulo note identity/order handling.

**Reason:** Cycling '74 publicly specifies the three optional fields, their domains, and their
defaults. Probability below one can suppress a structurally present note; velocity deviation can
change its played attack; release velocity can affect note-off response. Therefore the former
five-field equality was internally correct but externally weaker than the public API made
tractable. Explicitly sending documented deterministic values is stronger than depending on an
implicit creation default, and reading them back separates requested state from accepted mutation.
Score still has no source field for these properties: v15 defines a deliberately narrow Ableton
adapter projection rather than pretending to preserve absent Score intent. Low-level SMF with
nonzero retained Note Off velocity continues to fail analytical projection instead of being
silently replaced by 64.

**Consequence:** Native and Python peers reject missing, additional, reordered-as-duplicate,
wrong-valued, or category-confused insertion/query shapes. Readback requires floating finite
probability in [0,1], deviation in [-127,127], and release velocity in [0,127]; integral encodings
fail rather than crossing the documented scalar category. A well-formed target divergence is
retained and makes note verification incomplete. This closes ordinary stochastic/deviation/release
state for Sunny-created notes but not per-note MPE expression, tuning, MIDI processing, Device
response, routing, scheduling, or rendered audio.

## 132. Protocol v16 closes Track freeze state and exposes the monitoring boundary

**Decision:** The closed bridge advances from protocol v15 to v16 and target snapshot schema 8
advances to schema 9. Every normal Track snapshot now contains exact Boolean `is_frozen`. Each
generated-Part Track postcondition requests `is_frozen = false`, retains the final observation,
and verifies it independently as `unfrozen_verified`; the selected Track-gate verdict additionally
requires that result. The same evidence object exports immutable false residuals
`monitoring_state_observed` and
`clip_output_not_suppressed_by_monitoring_verified`.

**Reason:** The current Cycling '74 Track LOM exposes read-only observable `is_frozen`. Ableton's
Track Freeze manual says frozen Session clips play freeze audio files instead of recalculating
their current devices and clip settings. Exact live MIDI notes, Device identity, and parameter
values therefore do not describe the signal source of a frozen Track. This proposition is
tractable without inventing an unfreeze mutation. Monitoring has a different contract boundary:
Ableton's current manual says Monitor In suppresses clip output, and older official LOM references
expose writable `current_monitoring_state`, but the current public Track LOM omits every monitoring
property. Sunny does not promote a legacy or private Python member into its current portable
algebra. The same review distinguishes the inactive loop brace: current Clip documentation calls
`loop_start`/`loop_end` the loop/clip bounds, while the Live manual specifically defines an
unlooped Clip's played range by its independent start/end markers. Since Sunny requires
`looping = false`, loop-brace state is dormant for the bounded structural interval and is not
silently described as observed.

**Consequence:** A missing, additional, non-Boolean, or malformed `is_frozen` field is
`ProtocolError`. Freeze changes invalidate an exact guarded plan; a final true value warns and
makes the generated Track gate incomplete even when name, audio-output classification, mute, and
solo still match. `unfrozen_verified = true` still cannot prove clip output: the two monitoring
residuals remain false, output routing remains unresolved, and Sunny does not launch or render the
Clip. Structural project `complete` therefore remains narrower than playback or audible
completion. A named-Live compatibility test may justify a future version-scoped monitoring
capability, but it must first reconcile the current/legacy public documentation and add closed
set/readback, snapshot, and final evidence. If Sunny ever admits `looping = true`, loop-brace
intent/readback becomes mandatory; until then, changes to that dormant brace are outside schema 9.

## 133. Generated Part Track gates require top-level group membership

**Decision:** Without changing protocol v16 or snapshot schema 9, each generated-Part Track
postcondition now requests a null `group_track_index`. Executing evaluation distinguishes
`group_membership_observed` from the nullable `observed_group_track_index` and exports a separate
`ungrouped_verified` verdict. The selected Track gate verifies only when membership was observed
and the final index is null.

**Reason:** Schema 9 already resolves the public `Track.is_grouped`/`group_track` relationship into
an exact nullable Song-track index, but aggregate evaluation discarded that fact. Ableton defines
Group Tracks as summing containers with mixer controls and audio effects. Grouping usually changes
a contained Track's Audio To route from Main to the Group Track unless it already has custom
routing; group slots can also launch or stop contained clips. Consequently, exact Part-track
name/output classification, own mute/solo, Device chain, parameters, notes, and unfrozen state can
all remain unchanged while a newly introduced parent submix changes processing, gating, launch
control, and destination. Treating null as both “observed ungrouped” and “not observed” would repeat
the acceptance/evidence collapse the project model is designed to prevent.

**Consequence:** A recording plan exports null requested/observed indices with
`group_membership_observed = false` and claims no ungrouped verification. A real final snapshot
sets observation true; null membership verifies, while any resolved parent index warns and makes
the Part Track gate incomplete. Existing exact snapshot equality already invalidates a guarded
plan if an observed Track is regrouped before apply, so no new wire field or version bump is
needed. This closes only direct generated-Track membership. It does not model a Group Track,
validate its mixer/devices/output, materialise Sunny GroupBuses in Live, or close arbitrary output
routing; those remain independent residuals.

## 134. Protocol v17 neutralizes generated Part Track crossfade assignment

**Decision:** The closed bridge advances from protocol v16 to v17 and target snapshot schema 9
advances to schema 10. Normal and Return Track MixerDevice paths admit exactly one new mutation:
integer `crossfade_assign = 1`. Score compilation applies that value to every generated Part Track
immediately after naming it and requires the generic exact set/readback evidence. Schema 10 adds
`crossfade_assign` to every mixer record: exact integer 0–2 for normal and Return Tracks, and exact
null for the Master where the property is unavailable. Each generated-Part Track postcondition
retains requested and final observed assignment, exports `crossfade_neutral_verified`, and includes
that verdict in the selected Track gate.

**Reason:** Cycling '74 documents `MixerDevice.crossfade_assign` as an observable/settable integer
enum, `0 = A`, `1 = none`, and `2 = B`, unavailable on the Master. Ableton documents that the Main
crossfader can affect any number of normal and Return Tracks: an A/B assignment can attenuate or
mute the Track according to crossfader position, while neither assignment leaves it unaffected.
That action occurs at the Track's gain stage and does not alter signal routing. Consequently, exact
activator, mute/solo, derived solo-mute, fader/pan, Device, Clip, unfrozen, and ungrouped evidence
could all verify while the Main crossfader independently suppresses a generated Part. The property
and its complete enum are publicly tractable, so leaving it as an unnamed host-default residual
would be weaker than the external model permits.

**Consequence:** Native and Python peers reject A (`0`), B (`2`), Boolean, floating, Master, or
otherwise malformed mutation shapes; the snapshot parser rejects missing, extended,
category-confused, out-of-domain, or non-null Master evidence. Exact snapshot equality makes any
observed normal/Return assignment change stale-plan-visible. A well-formed final A/B value on a
generated Part warns and makes its Track gate incomplete. Return Track assignments are retained as
exact snapshot facts but Sunny does not yet neutralize them or promote them into a final Return
output verdict. Crossfade neutrality closes only the documented Main-crossfader gain influence on
the generated Part Track; it does not prove routing, monitoring, Group/Return processing, launch,
signal production, rendered audio, or audible equivalence.

## 135. Protocol v18 fixes generated Part Tracks to Stereo Pan mode

**Decision:** The closed bridge advances from protocol v17 to v18 and target snapshot schema 10
advances to schema 11. Normal, Return, and Master MixerDevice paths admit exactly one new
mutation: integer `panning_mode = 0`. Score compilation applies that value to every generated
Part Track before writing its single `panning.value` scalar and requires immediate exact
readback. Schema 11 adds exact integer `panning_mode` 0–1 to every mixer record. Each generated
Part postcondition retains requested and final observed mode, exports `stereo_panning_verified`,
and includes that verdict in the selected Track gate.

**Reason:** Cycling '74 documents `MixerDevice.panning_mode` as `0 = Stereo` and `1 = Split
Stereo`, with separate left/right split parameters in the latter mode. Ableton describes Stereo
Pan as one relative left/right gain control and Split Stereo as two independent channel-position
operations. Consequently, an exact stored `panning.value` can survive while no longer being the
active operation represented by Sunny's one-scalar pan model. This is publicly tractable and
applies to normal, Return, and Master mixers, so the snapshot must observe it without assuming a
host default.

**Consequence:** Native and Python peers reject Split Stereo (`1`), Boolean, floating, or
otherwise malformed mutation shapes; the snapshot parser rejects missing, extended,
category-confused, or out-of-domain mode evidence. Exact snapshot equality makes changes on any
mixer stale-plan-visible. A well-formed final Split Stereo value on a generated Part warns and
makes its Track gate incomplete. Return and Master modes remain exact snapshot facts but are not
currently final mode obligations. Stereo Pan mode preserves the target operation type of Sunny's
pan projection; it does not prove neutral panning, routing, rendered audio, or audible
equivalence.

## 136. Protocol v19 disarms both public arm states on generated Part Tracks

**Decision:** The closed bridge advances from protocol v18 to v19 and target snapshot schema 11
advances to schema 12. Normal Track paths admit exactly two new mutations: Boolean `arm = false`
and `implicit_arm = false`. Score compilation applies both values immediately after naming every
generated Part Track and requires exact immediate readback. Schema 12 adds exact Boolean arm state
to every normal Track record. Each generated-Part postcondition retains requested and final
observed values, exports `disarmed_verified` and `implicitly_disarmed_verified`, and includes both
verdicts in the selected Track gate.

**Reason:** Cycling '74 documents `Track.arm` as the record-enable state and `implicit_arm` as a
second arm state used by Push. Ableton says armed Tracks are monitored by default, participate in
recording and MIDI overdub, and that selecting a MIDI Track on Push can arm it automatically.
Neither `Song.create_midi_track` nor native-device insertion promises a disarmed result.
Consequently, exact stored notes, mixer gates, Devices, freeze state, group membership, crossfade
assignment, and pan mode can all remain unchanged while live input or record-enabled mutation is
admitted through an independent public Track state.

**Consequence:** Native and Python peers reject true, integral, Return, Master, or otherwise
malformed arm mutations; the snapshot parser rejects missing, extended, non-Boolean, or
category-confused arm evidence. Exact snapshot equality makes either observed state
stale-plan-visible. A well-formed final true value warns and makes its Part Track gate incomplete.
This closes only ordinary and Push arm state at the observation instant. It does not observe the
current Monitor selector, choose input/output routing, prevent a later user/Push change, launch a
Clip, or prove rendered sound.

## 137. Protocol v20 closes the public generated-Clip runtime tuple

**Decision:** The closed bridge advances from protocol v19 to v20 and target snapshot schema 12
advances to schema 13. Every occupied Clip record adds exact Boolean `is_playing`, `is_recording`,
`is_overdubbing`, `is_triggered`, and `will_record_on_start`. Generated-Clip final evidence retains
false as the requested value for all five, records each final observation, exports
`runtime_state_observed`, `recording_quiescence_verified`, and `playback_idle_verified`, and
requires both quiescence verdicts in the selected Clip gate. No setter or stop operation is added.

**Reason:** Cycling '74 documents the five properties as distinct current Clip states:
`is_playing` includes playing or recording, `is_recording` identifies recording,
`is_overdubbing` identifies overdub, `is_triggered` identifies a blinking queued launch, and
`will_record_on_start` identifies a triggered armed MIDI Clip with Arrangement Overdub enabled.
Ableton's recording manual also documents transitions from Session recording into loop playback
or overdub and preference-driven recording on scene launch. Consequently, exact static notes,
markers, launch scalars, envelope/groove absence, and even a finally disarmed Track do not prove
that a previously initiated Clip action has stopped or that its content is not a moving target.
All five public fields are typed and therefore tractable.

**Consequence:** The Remote Script rejects any non-Boolean host value before returning a snapshot,
and the native parser rejects missing, extended, integral, or otherwise category-confused fields.
Exact snapshot equality now makes a change in any selected runtime predicate stale-plan-visible.
A generated Clip with any true final predicate warns and remains incomplete. The five properties
are read sequentially inside a synchronous main-thread call, while Live supplies neither a
snapshot transaction nor a topology lock; Sunny therefore imposes no undocumented cross-field
invariant and does not claim an atomic state-machine observation. The result also does not prove
prior playback history, future stability, Follow Action state, monitoring/routing, rendered timing,
signal production, or sound.

## 138. Protocol v21 makes Song recording-mode quiescence explicit

**Decision:** The closed bridge advances from protocol v20 to v21 and target snapshot schema 13
advances to schema 14. The Song record adds exact Boolean `is_playing`, `is_counting_in`,
`arrangement_overdub`, `overdub`, `record_mode`, `session_record`, and
`session_automation_record`. Aggregate Song evidence requests false for the six count-in,
recording, overdub, and automation-record predicates and exports
`recording_modes_quiescent_verified`. It separately exports `transport_stopped_verified` from
`is_playing`; ordinary non-recording playback does not make the structural Song gate incomplete.
No transport or recording-control setter is admitted.

**Reason:** Cycling '74 documents `record_mode` as Arrangement Record, `session_record` as Session
Overdub, `session_automation_record` as Automation Arm, `is_counting_in` as active count-in, and
both `arrangement_overdub` and `overdub` as MIDI Arrangement Overdub state. Ableton says
Arrangement Record records armed Tracks, Session Record moves Clips between recording/playback/
overdub, and Session automation can be recorded into playing Clips without an armed Track. An
idle, disarmed generated Clip therefore does not prove that the surrounding Set is not recording
elsewhere or primed to record control changes. The same manuals distinguish ordinary playback
from recording, so transport stop remains evidence rather than a blanket deployment precondition.

**Consequence:** The Remote Script rejects non-Boolean host values and the native exact parser
rejects missing, extended, integral, or category-confused Song runtime state. Exact precondition
comparison now detects changes to all seven selected predicates. Any true final recording/count-in
mode warns and makes Song verification incomplete, while `is_playing = true` only makes
`transport_stopped_verified` false. The current public `session_record_status` integer is excluded
because the reference gives no value mapping; Sunny does not infer one. Reads remain sequential,
record preferences and prior/future transitions are not captured, and no claim is made about
atomicity, later stability, routing, signal, or sound.

## 139. Protocol v22 separates current tempo equality from tempo authority

**Decision:** The closed bridge advances from protocol v21 to v22 and target snapshot schema 14
advances to schema 15. The Song record adds exact Boolean `is_ableton_link_enabled`,
`is_ableton_link_start_stop_sync_enabled`, `tempo_follower_enabled`, `nudge_down`, and `nudge_up`.
Aggregate Song evidence requests false for all five and requires
`public_tempo_controls_quiescent_verified` in its selected structural gate. It also exports the
immutable residuals `external_midi_sync_state_observed = false`,
`tempo_automation_state_observed = false`, and `tempo_stability_verified = false`. No tempo-control
or synchronization setter is admitted.

**Reason:** Cycling '74 documents the five properties as state independent of the scalar Song
tempo and explicitly says that tempo may be automated and change with song time. Ableton says any
Link participant can change the shared tempo, that Link tempo changes override Set automation, and
that Tempo Follower continuously derives Live's tempo from incoming audio. It separately documents
incoming MIDI Clock as an external tempo source, but the current public Song LOM exposes neither an
incoming-sync enabled predicate nor the tempo automation envelope. Exact immediate `tempo`
readback therefore proves a current scalar only; it does not establish who controls the next value.

**Consequence:** The Remote Script rejects non-Boolean host values and the native exact parser
rejects missing, extended, integral, or category-confused public tempo-control state. Exact
precondition comparison now detects changes to all five fields. Any true final field warns and
makes Song verification incomplete, but five false values do not promote the two unobserved
residuals or establish later stability. Reads remain sequential, Link start/stop sync may be a
dormant preference while Link is off, and Sunny claims neither an atomic observation nor exclusive
tempo authority, persistence, routing, signal, or sound.

## 140. Protocol v23 observes stored-versus-effective playback and automation divergence

**Decision:** The closed bridge advances from protocol v22 to v23 and target snapshot schema 15
advances to schema 16. The Song record adds exact Boolean `back_to_arranger` and
`re_enable_automation_enabled`. Aggregate Song evidence requests false for both, exports
`arrangement_playback_aligned_verified` and `automation_overrides_quiescent_verified`, and requires
both verdicts in the selected Song gate. No Back to Arrangement or Re-Enable Automation mutation
is admitted.

**Reason:** Cycling '74 documents `back_to_arranger = true` as meaning that current playback differs
from stored Arrangement content. Ableton documents Session Clips as superseding the corresponding
Arrangement Track until Back to Arrangement is invoked. The public Song LOM separately exposes
`re_enable_automation_enabled`, and Ableton says its illuminated state means at least one automated
control is inactive and currently overridden by a manual value. Exact Clip membership, current
mixer/DeviceParameter values, and even quiescent recording/tempo controls do not reveal either
stored-versus-effective divergence.

**Consequence:** The Remote Script rejects non-Boolean host values and the native exact parser
rejects missing, extended, integral, or category-confused state. Exact snapshot comparison makes
both predicates stale-plan-visible. A true final value warns and makes Song verification
incomplete through its independent verdict. Sunny deliberately does not repair either condition:
returning to Arrangement can replace a user's live Session choices, and re-enabling automation can
change controls globally. The observations remain sequential and prove neither envelope content,
atomicity, prior playback/override history, nor future persistence, routing, signal, or sound.

## 141. Protocol v24 makes Arrangement repetition and metronome output explicit

**Decision:** The closed bridge advances from protocol v23 to v24 and target snapshot schema 16
advances to schema 17. The Song record adds exact Boolean `loop` and `metronome`. Aggregate Song
evidence requests false for both, exports `arrangement_loop_disabled_verified` and
`metronome_disabled_verified`, and requires both verdicts in the selected Song gate. No loop or
metronome setter is admitted. Loop start/length and metronome preferences are not added.

**Reason:** Cycling '74 exposes Arrangement Loop and metronome enablement as independent Song
properties. Ableton says an enabled Arrangement Loop repeatedly plays the loop-brace region and
that an enabled metronome begins ticking when transport starts or a Clip launches, subject to its
preferences. Exact notes, marker ranges, playback-source alignment, automation state, and current
tempo do not reveal either independent modifier. Under `loop = false`, the brace start and length
cannot affect repetition; under `metronome = false`, its rhythm, sound, and enable-only-while-
recording preferences cannot produce clicks. The controlling Booleans are therefore the minimal
active-state boundary.

**Consequence:** The Remote Script rejects non-Boolean host values and the native exact parser
rejects missing, extended, integral, or category-confused state. Exact snapshot comparison makes
both predicates stale-plan-visible. A true final value warns and makes Song verification incomplete
through its independent verdict. Sunny does not toggle either control, preserving user playback
state rather than silently repairing it. Reads remain sequential, and false values prove neither a
playback trace, prior behavior, future persistence, other cue/preview output, routing, signal, nor
rendered sound.

## 142. Protocol v25 closes the generated Aux Return gain-stage gate

**Decision:** The closed bridge advances from protocol v24 to v25 and target snapshot schema 17
advances to schema 18. Mix compilation binds each AuxBus ID to its exact appended Return Track
index and explicitly writes/reads `mute = false`, `solo = false`, `crossfade_assign = 1`,
`panning_mode = 0`, the modeled return pan, and the existing return-level intent. Return snapshot
records add exact Boolean `mute`, `solo`, and `muted_via_solo`. Aggregate evidence adds a separate
Return Track gate that requires the final Aux name, all three suppression predicates false, neutral
crossfade assignment, and Stereo Pan mode. Generic mixer evidence independently verifies final
active/unautomated return level and pan.

**Reason:** Cycling '74 defines Track as including Return Tracks, exposes writable Boolean mute and
solo on non-Master Tracks, and exposes read-only derived `muted_via_solo`. MixerDevice exposes
writable crossfade assignment and panning mode on Return Tracks. Ableton says Return Tracks process
signals arriving through sends, can participate in crossfading, and are affected by solo behavior.
Consequently, exact send addressing, name, device identity, return level, and stored pan do not prove
that the wet path is enabled or that Sunny's one pan scalar is the active operation. These public,
typed controls are tractable and correspond to an implicit enabled AuxBus path in the Mix model.

**Consequence:** Native and Python peers admit only the newly required closed Return shapes, retain
Boolean and integer categories, and continue to reject Return arm, arbitrary properties, A/B
crossfade assignment, and Split Stereo mode. The Remote Script rejects non-Boolean host gate facts;
the native parser rejects missing, extended, integral, or category-confused Return evidence. A
well-formed final mute, solo, derived solo mute, A/B assignment, or Split Stereo mode warns and makes
that Return gate incomplete. Non-pan spatial fields and target-owned output routing remain explicit
residuals. Immediate writes and the final snapshot are sequential observations, not a transaction,
future-stability proof, signal trace, or rendered-audio equivalence.

## 143. Protocol v26 closes the neutral Main output stage and parameter operability

**Decision:** The closed bridge advances from protocol v25 to v26 and target snapshot schema 18
advances to schema 19. Mix compilation writes and immediately reads the Main MixerDevice's
`track_activator.value = 1.0`, `panning_mode = 0`, and `panning.value = 0.0`. It also writes each
generated Part Track's activator as the exact floating lowering of its Boolean mute intent and
requires 1.0 on generated Aux Returns. Every selected mixer-parameter snapshot record adds exact
Boolean `is_enabled`, and every mixer adds an exact `track_activator` record. Final Track, Return,
and Main activator verdicts require requested value equality, enabled true, active `state = 0`, and
`automation_state = 0`; generic volume, panning, and send verdicts adopt the same operability
requirements. A separate Main gate additionally requires Stereo Pan mode and centered pan.

**Reason:** Sunny's `MasterBus` owns a fader and insert chain but has no mute or pan intent. Live's
Main Track nevertheless owns an output activator, panning mode, and panning parameter, while the
normal Track `mute` and `solo` properties are unavailable there. Exact Master fader/effect facts can
therefore coexist with a suppressed or spatially biased final gain stage. Cycling '74 also exposes
DeviceParameter enablement, active state, and automation state independently of its numeric value.
This matters particularly at the Max for Live boundary: `live.remote~` can take real-time control
and disable parameter automation, and a numerically matching but disabled/inactive/automated
parameter is not an effective-state proof. All selected facts are public, typed, and tractable.

**Consequence:** Native and Python peers admit only exact floating 0.0/1.0 normal-Track activator
writes, exact 1.0 Return/Main activator writes, and exact floating-zero Main pan writes. Boolean,
integral, non-finite, category-confused, arbitrary Master pan, and other reflective mutations fail
before Live lookup. The snapshot parser rejects missing, extended, non-Boolean enablement, or
malformed activator records. Well-formed muted, split, biased, disabled, inactive, or automated
final state warns and makes the corresponding gate incomplete. Main neutrality is a target
lowering of absent source intent, not a new source field. Reads and immediate acknowledgements are
sequential; target-owned routing, cue/crossfader behavior, signal production, future state, and
rendered sound remain explicit residuals.

## 144. Protocol v27 closes the bounded mixer-parameter domain

**Decision:** The closed bridge advances from protocol v26 to v27 and target snapshot schema 19
advances to schema 20. Every selected mixer DeviceParameter record adds exact floating
`minimum`/`maximum` and exact Boolean `is_quantized`. The trust boundary requires finite ordered
bounds and an internal value inside the reported interval. Track Activator evidence requests a
quantised domain and proves that its requested 0.0/1.0 value lies inside the observed range.
Volume, panning, and send evidence requests an unquantised domain. Internal-value postconditions
also expose a range verdict; display-value postconditions retain the internal bounds but leave the
range verdict null because GUI-visible values do not inhabit that interval.

**Reason:** Cycling '74 defines DeviceParameter `is_quantized` independently from value and says it
is true for Boolean/enumerated parameters and false for integer/floating parameters. Ableton
documents Track Activator as a switch and separately classifies mixer volume, pan, and sends as
continuous controls. DeviceParameter also publishes internal `min` and `max`. Discarding these
facts allowed an equal, enabled, active, unautomated scalar to verify even if the target reported a
control domain contradicting Sunny's operation, and allowed target range changes to evade guarded
snapshot equality. The public documentation supports role classification but does not specify
portable numeric range constants or localized `value_items` labels, so Sunny observes bounds and
classification without inventing either.

**Consequence:** The Remote Script rejects integral or non-finite float facts, non-Boolean
quantisation, inverted bounds, and internal values outside their reported interval. The native
exact parser independently rejects missing, extended, category-confused, or universally invalid
records. Exact bounds and domain state now participate in plan staleness and appear in Track,
Return, Main, generic mixer, and MCP evidence. A well-formed discrete/continuous role
contradiction warns and makes the applicable gate incomplete instead of becoming a protocol type
error. Activator and internal pan range verdicts are explicit; display dB values are never compared
against internal bounds. The new evidence remains sequential current-state evidence and proves
neither modulation, future persistence, routing, signal production, nor rendered sound.

## 145. Protocol v28 constrains verified devices to Sunny's flat chain model

**Decision:** The closed bridge advances from protocol v27 to v28 and target snapshot schema 20
advances to schema 21. Every immediate insertion result and every bounded top-level Device snapshot
adds exact Boolean `can_have_chains`. Device insertion and final postcondition evidence retain that
observation and an explicit `flat_device_verified` verdict. Only `can_have_chains = false` may
verify a materialised source or effect.

**Reason:** Cycling '74 defines `Device.can_have_chains = 1` as the discriminator for a Device Rack.
Ableton documents a Rack as a container for parallel and nested chains whose outputs are combined,
with independent Chain Activators, solo state, mixer controls, selector/key/velocity zones, Drum
Pad routing, Rack returns, and Macro mappings. Sunny's current Timbre and Mix device intent is a
flat serial sequence and contains none of that recursive branch state. `Device.is_active` is
insufficient: its contract establishes only that the Device and enclosing Rack are on. Therefore a
matching active top-level name/type cannot be promoted to faithful structural evidence when the
inserted Device is itself a Rack.

**Consequence:** The Python peer validates Device type, activity, and Rack capability without
Boolean/integer coercion. The native immediate-evidence and snapshot parsers independently require
the exact new Boolean and reject missing, extended, or category-confused records. Rack capability
participates in exact plan equality and appears in Timbre, Mix, aggregate, and MCP evidence. A true
Rack observation is well-formed target divergence: it warns, keeps verification incomplete, and
prevents dependent parameter writes under the existing structural-dependency rule. Sunny does not
invent a recursive traversal or silently treat Rack defaults as source intent. Supporting Racks
later requires an explicit recursive source carrier, bounded Rack/Chain snapshot algebra, and
named-Live tests for zones, selectors, chain mixers, pads/returns where applicable, nesting, and
parameter addressing. This boundary still proves neither device-internal behavior, signal, nor
sound.

## 146. Protocol v29 unifies contract authority and closes generated-Track Session topology

**Decision:** `remote_script/Sunny/bridge_contract.json` is the single machine-readable authority
for the bridge protocol and target-snapshot schema. CMake validates that closed two-field manifest
and generates the installed public C++ version header from it; the deployed Remote Script loads the
same file without a second literal. Protocol v29 advances snapshot schema 21 to 22. Every normal
Track record adds exact `back_to_arranger`, `fired_slot_index`, and `playing_slot_index` state. The
generated Part gate requires false, -1, and -1 respectively. Generated-Clip verification also
requires the containing Track's complete occupied-slot set to equal exactly `{0}`. The Python/native
integration suite serializes the real adapter snapshot and submits it to the native parser, while
checking that both exported versions equal the loaded manifest.

**Reason:** Independently green native and Python suites had concealed a real peer contradiction:
the native parser required schema 21 while the adapter still emitted schema 19. A duplicated
version literal is not a reliable protocol specification. Separately, checking only the requested
Clip at slot zero allowed another occupied Session slot on the generated Track to coexist with a
successful postcondition. Ableton's Track LOM exposes the currently playing and fired Session slot
indices and a per-Track Back-to-Arrangement predicate; its ClipSlot LOM exposes exact occupancy.
Those bounded facts distinguish an idle, Arrangement-aligned generated Track from one with a queued
Session action, a Session playback override, or additional target-owned Clip content.

**Consequence:** Version drift now fails at configuration, Remote Script import, or live cross-peer
conformance instead of surviving separate unit suites. Missing, extended, Boolean-as-integer, or
out-of-range Track slot state fails closed. A well-formed fired/playing/override divergence, or any
additional occupied slot, is retained as target evidence but makes the applicable Track/Clip gate
incomplete. Sunny issues no stop, Back-to-Arrangement, Clip deletion, or Scene deletion operation,
so it does not silently destroy target-owned Session choices. These sequential observations prove
only exact state at the snapshot point; Track monitoring, output routing, Follow Actions, future
launches, signal production, and rendered sound remain explicit residuals.

## 147. Protocol v30 observes selected output routes without inventing destination identity

**Decision:** The single bridge manifest advances protocol v29 to v30 and target snapshot schema
22 to 23. Every normal and Return Track record adds exact `output_routing_type` and
`output_routing_channel` dictionaries, each closed over string `display_name` and `identifier`.
Final Track and Return output-routing evidence pairs those four selected Live symbols with Sunny's
requested `Master` or `Group(id)` destination and separates `selected_output_observed`,
`source_target_identity_mapped`, and route `verified`. A final snapshot establishes observation;
the latter two verdicts remain false. Main is excluded because the public Track contract says the
properties are unavailable there.

**Reason:** The Mix IR already enumerates every Channel, Group, and Aux output edge, but snapshot
schema 22 retained only `has_audio_output`/`has_midi_output` classification. A user or controller
could therefore change a Track's selected destination without invalidating an otherwise exact
guarded plan. Cycling '74 documents both selected routing properties as two-field dictionaries and
requires set values to come from the Track's available-routing collections, so the current
selection is bounded and tractable. The same documentation does not publish a portable literal
identifier for Main, a particular Group Track, or an external destination. Display names may be
localized, and observing a fresh-Track default does not turn that default into Sunny intent.

**Consequence:** The Python peer validates the private wrapper's routing values without string or
mapping coercion; the native parser independently rejects missing, extended, or non-string
dictionaries. Exact selected symbols now participate in stale-plan equality and appear in native
and MCP postcondition evidence. A selected route can visibly contradict the requested Sunny edge
without changing the narrower Track/Return gain gate, and it can never verify merely because the
display text resembles “Master.” The Mix compiler continues to retain all requested route
residuals with `output_routes_written = output_routes_verified = 0`. Full realization requires
available-option snapshots, a version-scoped one-to-one semantic mapping, exact returned-dictionary
mutation/readback, and named-Live tests of the Control Surface representation; Main hardware
routing, monitoring, signal production, persistence, and rendered sound remain separate residuals.

## 148. Protocol v31 proves selected output routes belong to Live's advertised option sets

**Decision:** The single bridge manifest advances protocol v30 to v31 and target snapshot schema
23 to 24. Every normal and Return Track record adds exact `available_output_routing_types` and
`available_output_routing_channels` wrappers. Each wrapper contains only its same-named list, and
each option is the same exact string `display_name`/`identifier` dictionary as the selected
property. The selected full type and channel dictionaries must occur in their corresponding
arrays. Main remains excluded. Final Track and Return route evidence retains both option arrays and
adds `selected_type_available_verified`, `selected_channel_available_verified`, and their
conjunction `selected_output_available_verified`. The existing semantic-identity and route
verification verdicts remain false, and the Mix compiler still writes zero routes.

**Reason:** Cycling '74 documents the two available-output-routing properties as one-key
dictionaries containing lists of dictionaries shaped like the selected properties, and permits a
selected property to be set only to a value from its corresponding available collection. That
makes exact collection shape and full-dictionary membership tractable without assigning meaning to
an opaque identifier. It does not document a portable identifier for Main, a particular Group
Track, or an external destination, nor does it make four sequential reads atomic. Treating
membership as destination identity would therefore collapse a target-valid selection into a
stronger and unsupported Sunny semantic claim.

**Consequence:** The Python peer requires an exact outer dictionary and a list/tuple of exact
option dictionaries, normalising the private Control Surface value to one JSON array. It rejects a
selected value absent from that sequentially read collection. The native parser independently
requires the exact wrapper/array shape and membership; missing, extended, non-string, or absent
selection evidence fails closed. Selected and available dictionaries, including collection order,
now participate in guarded-plan equality and appear in native and MCP evidence. Duplicates are not
declared invalid because the public contract does not assert identifier uniqueness. This closes
only the Live-advertised-membership proposition. A named-Live record must still validate the
private Python representation, and route realization still requires a version-scoped unambiguous
Sunny-to-Live mapping plus exact returned-dictionary mutation/readback. Main hardware routing,
monitoring, signal production, persistence, atomicity, and rendered sound remain separate
residuals.

## 149. Protocol v32 closes Track input classification and advertised input-option membership

**Decision:** The single bridge manifest advances protocol v31 to v32 and target snapshot schema
24 to 25. Every normal Track adds exact Boolean `has_audio_input` and `has_midi_input`. When either
is true, the record must also contain exact selected `input_routing_type` and
`input_routing_channel` dictionaries plus their same-named one-key available-option wrappers, and
each selected full dictionary must occur in the corresponding array. When both are false, all four
routing fields are explicit nulls. Return and Main records retain their prior shape. Generated
Part evidence requires `(has_audio_input, has_midi_input) = (false, true)` and selected type,
channel, and combined membership. Input-source identity and external-input neutrality remain
false, and Sunny writes no input route.

**Reason:** Cycling '74 documents the two input-class predicates and makes the selected and
available input-routing properties conditional on a MIDI/audio Track. It also requires a selected
value to come from the advertised list. Those facts make exact classification, conditional shape,
and full-dictionary membership tractable. They do not assign portable semantic meaning to an
opaque identifier or localized display name, establish that a value means “No Input,” expose the
current monitoring selector, or prove that no external event or signal can reach the Track.

**Consequence:** The Python peer reads input routing only for a normal Track whose exact input
classification is true and normalises the private list/tuple representation to JSON arrays. Both
peers fail closed on malformed Booleans, wrong conditional shape, extended wrappers/options, or a
selected value missing from its available array. Selection and ordered options participate in
guarded-plan equality and appear in native/MCP evidence. Duplicates remain admitted because the
public contract does not assert identifier uniqueness, and no general audio/MIDI mutual-exclusion
invariant is invented; the generated Part has the narrower exact `(false, true)` obligation.
Reads remain sequential rather than atomic. A named-Live record must still validate the private
Control Surface representation, while semantic mapping, exact route mutation/readback,
monitoring, incoming-event isolation, persistence, signal, and rendered sound remain separate
residuals.

## 150. Protocol v33 closes generated-Track one-second hold-meter quiescence

**Decision:** The single bridge manifest advances protocol v32 to v33 and target snapshot schema
25 to 26. Every audio/MIDI normal Track adds exact `input_meter_level` and `output_meter_level`
JSON floating values in the documented `[0,1]` domain; a normal Track with neither input-class flag
uses two explicit nulls. Return and Main records retain their prior shape. Generated Part evidence
requests exact `0.0` for both levels and exports meter observation, independent input/output
hold-quiescence verdicts, and their conjunction. The Track gate requires the conjunction, while
continuous input/output silence verdicts remain false.

**Reason:** Cycling '74 documents both properties as one-second hold peaks on audio and MIDI
Tracks. A nonzero final value is therefore concrete recent metered activity that contradicts a
quiescent generated Part, and the finite floating domain is tractable. Zero is much narrower: it
does not reveal activity outside the hold window, the interval between sequential reads,
monitoring, routing semantics, meter sensitivity, future events, interface buffering, physical
outputs, or acoustic/rendered sound. Calling a zero point observation “silence” would overstate the
host contract.

**Consequence:** The Python peer accepts only exact finite `float` values in `[0,1]` for the
audio/MIDI branch and never accesses the properties on other Track kinds. The native peer
independently rejects integral encodings, non-finite/out-of-range values, wrong nullability, and
extended shapes. Both levels participate in guarded-plan equality. Postcondition evaluation uses
exact zero rather than tolerance, warns/incompletes on a well-formed nonzero peak, and leaves
`continuous_input_silence_verified` and `continuous_output_silence_verified` immutable false.
Sunny performs no meter reset and invents no threshold. A stronger claim requires named-Live
validation plus a controlled polling/event/audio experiment with an explicitly scoped monitoring,
routing, interface, and render boundary.

## 151. Protocol v34 closes the complete Scene pending-launch vector

**Decision:** The single bridge manifest advances protocol v33 to v34 and target snapshot schema
26 to 27. Every Scene record adds exact Boolean `is_triggered`. Final generated-project Song
evidence requests false across the complete ordered Scene vector, exports separate observation and
all-Scene launch-quiescence verdicts, and requires their conjunction. Sunny admits no Scene fire or
stop operation.

**Reason:** Cycling '74 documents `Scene.is_triggered` as true while the Scene is blinking. That is
a distinct launch layer from Clip `is_triggered` and Track `fired_slot_index`/`playing_slot_index`.
Checking only Scene 0 would miss a pending launch on another row, and the Scene documentation notes
that Scene launch can interact with recording on armed empty Tracks under a Live preference. The
complete finite Boolean vector is tractable; launch acknowledgement, completion, preferences,
atomicity with the separately read Clip/Track predicates, future state, and sound are not.

**Consequence:** The Python peer reads every Scene value without coercion and the native peer
independently requires the exact field and Boolean category. Missing, extended, or integral state
fails closed; any true Scene is well-formed target divergence that invalidates exact stale-plan
equality and makes the Song postcondition incomplete. Native and MCP evidence retain the requested
false scalar, complete observed vector, and both verdicts. Adversarial tests trigger only a later
Scene to prove evaluation is not restricted to the generated row. The result remains a sequential
point observation and makes no stop, polling, playback, recording-preference, signal, or sonic
claim.

## 152. Protocol v35 closes the ordered ClipSlot runtime matrix

**Decision:** The single bridge manifest advances protocol v34 to v35 and target snapshot schema
27 to 28. Every normal Track adds a complete ordered `clip_slots` array. Each exact record retains
slot index, Clip occupancy, Stop Button presence, Group-slot/control predicates, playing/recording/
triggered state, 0–2 playing status, and will-record-on-start. Its indices/cardinality must equal
`clip_slot_count`, and the `has_clip` set must exactly equal the separately retained Clip records.
Generated Part evidence requires non-Group, idle, non-recording/non-will-record, and non-triggered
state for every slot. Stop Button presence is observed but not requested.

**Reason:** Cycling '74 documents ClipSlot as a Session cell with state independent from its
optional Clip child. In particular, `is_triggered` covers blinking launch, stop, record, or
contained-Clip buttons, while an empty slot has no Clip object from which to infer pending stop or
record state. The same contract relates `is_playing` to nonzero `playing_status`, `is_recording` to
status 2, and makes status zero on non-Group slots. These finite cross-field invariants are
tractable. Whether a Stop Button ought to exist is not Sunny source intent, and sequential reads do
not establish atomicity, completion, persistence, preferences, signal, or sound.

**Consequence:** The Python peer reads every slot field without truthiness/integer coercion and
rejects incoherent documented relations before emitting JSON. The native peer independently
requires exact fields/categories, ordered indices, cardinality, cross-field coherence, and exact
Clip occupancy equality. All slot facts participate in stale-plan equality. Native and MCP Track
evidence retain the full vector plus independent observation, non-Group, playback, recording,
launch, and combined quiescence verdicts. A later empty slot can invalidate the Track gate while
its Scene, Track indices, and generated Clip remain idle. Sunny adds no ClipSlot fire/stop write and
does not promote Stop Button presence into an unmodelled requirement.

## 153. Max DSP setup facts become a validated host-neutral signal-block context

**Decision:** Add `SignalBlockContext::create(sample_rate, maximum_frames)` and bounded native
`noexcept` block processing for the LFO and ADSR. The context accepts only a finite positive sample
rate and positive maximum vector size. A block must not exceed that maximum, and an LFO's retained
frequency must not exceed the context rate. Every block call validates completely before changing
processor state or output; an admitted block is exactly the scalar recurrence evaluated in sample
order. The methods allocate nothing and perform no locks, I/O, callbacks, or unbounded search.

**Reason:** The Max SDK separates `dsp64`, which receives the active sample rate and maximum vector
size while building the DSP chain, from `perform64`, which receives the actual `sampleframes` on an
audio-driver callback with minimal thread protection. Sunny's per-sample functions specified the
recurrence but had no type for those setup facts, no vector bound, and no all-or-none vector failure
semantics. A future adapter could therefore guess a global rate, ignore the host maximum, partially
advance state before detecting an invalid call, or call an allocating convenience path while still
appearing consistent with the source API.

**Consequence:** The C++ source model now has a tractable two-phase seam for an eventual Max/MSP or
Max for Live DSP adapter, and native/Python tests prove scalar/vector equivalence plus rejection
atomicity. Python's list-returning helper remains explicitly non-real-time. This decision does not
reintroduce a Max target or claim that host facts are passed honestly: there is still no Max SDK
external, `dsp_add64`/perform routine, cross-thread state-transfer scheme, macOS/Windows package,
frozen `.amxd`, or named Max/Live validation. Those were the gates at this decision; Decision 173
subsequently closes the source adapter and package shape, while `.amxd` and named-host evidence
remain open.

## 154. Vector-bounded transport remains explicitly block-quantised

**Decision:** Move `SignalBlockContext` into its own render header/source and let `Transport`
consume it through a host-shaped block overload. The actual sample count must not exceed the
validated maximum. Oversize rejection precedes integer-position or fractional-remainder mutation
and callback dispatch. An admitted call uses the context rate but otherwise retains the existing
block-to-tick recurrence and endpoint callback semantics. No sample-offset field is added.

**Reason:** Max's `dsp64`/`perform64` lifecycle exposes a setup/vector cardinality that can also
constrain source-side transport conversion without making Transport a perform routine. That
property is finite and testable now. It is not sample placement or real-time safety: Sunny's queued
event ticks can cross at a fractional position inside a vector, and a useful offset API would have
to choose an explicit tick-to-sample quantisation rule, define whether the block endpoint belongs
to this or the next vector, preserve equal-offset ordering, bound event work, and connect to a Max
event path whose documented scheduler and audio settings can honor it. Treating a vector bound as
sample accuracy or perform safety would repeat the overclaim removed by Decision 18.

**Consequence:** The source model has one independent validated setup type for modulation and
transport conversion and cannot silently accept an oversize transport vector. Native tests prove
oversize failure is pre-dispatch and state-preserving. The public transport still promises only
retained fractional-tick progress plus block-endpoint delivery and is excluded from `perform64` by
Decision 156. An offset-aware bounded algebra, packaged Max target, and named-host timing evidence
remain separate future gates rather than fabricated completeness.

## 155. Max vector counts remain signed until range validation

**Decision:** `SignalBlockContext::create` accepts its maximum frame count as signed 64-bit state,
requires a positive `size_t`-representable value, and stores only the checked extent. Its
`validate_frame_count` operation accepts a signed actual count, requires it in the closed range
`[0, maximum]` and representable as `size_t`, then returns the checked unsigned extent. The
host-shaped transport and Python vector conveniences use this boundary before effects/allocation.

**Reason:** Max declares both `dsp64`'s `maxvectorsize` and `perform64`'s `sampleframes` as signed
`long`. Casting a malformed negative value before validation can create a huge unsigned count,
bypass category-correct bounds reasoning, and mis-size a span or allocation. Valid values in normal
host operation do not justify erasing the external ABI's signed category at the trust boundary.

**Consequence:** Negative maximum and actual counts now fail with `RenderInvalidBlockSize` before
unsigned conversion, signal processing, transport state mutation, callback dispatch, or Python
allocation; adversarial tests cover both. A platform adapter must still check conversion from its
possibly 32- or 64-bit `long` to Sunny's fixed signed carrier. This strengthens the source seam
without claiming that a Max external or host mapping exists.

## 156. Vector cardinality does not make Transport perform-safe

**Decision:** State explicitly that every stateful render instance is exclusively owned and has no
internal synchronisation. LFO/ADSR block methods retain their bounded allocation-free claim only
when setters, triggers, resets, queries, and processing are serialized on that owner. The current
`Transport` surface is forbidden in a Max audio perform routine even through its
`SignalBlockContext` overload. No generic lock, atomic mailbox, or SPSC queue is introduced without
a concrete adapter.

**Reason:** The Max SDK documents several thread arrangements. Main-thread and high-priority timer
messages can interrupt each other; the audio perform routine ordinarily runs on a driver-owned
thread with minimal protection; its supported communication back to Max is a clock, not a qelem or
outlet. Sunny Transport may pop an unbounded event backlog and invoke arbitrary `std::function`
callbacks, and scheduling can allocate. A vector-size check changes none of those properties.
Furthermore, a queue advertised as SPSC would be invalid until an adapter deliberately serializes
Max's possible control producers.

**Consequence:** Source documentation no longer suggests that every consumer of the validated
block context can participate in `perform64`. Modulation has a precise source-side conditional
real-time claim; Transport has an explicit negative claim. Decision 157 supplies the bounded clock
half without conflating it with event transfer. A future integration must still define a bounded
event-transfer algebra and overflow policy, serialize validated controls, use a documented Max
handoff, and prove the packaged runtime in named hosts before the removed Max or SPSC layers can
return.

## 157. BlockClock isolates the bounded audio-owner recurrence

**Decision:** Extract the block-to-tick recurrence into public `BlockClock`, an exclusive-owner
state machine with validated PPQ/tempo/position, retained fractional tick, and signed-context block
entry. An admitted call commits and returns `{tick_before, tick_after}` in O(1), is `noexcept`, and
performs no allocation, locking, I/O, event traversal, or callback. Transport composes this clock
with its priority queue and dispatches due events only after the endpoint is committed. Transport
callbacks may query that endpoint but must neither throw nor mutate the same Transport reentrantly.

**Reason:** Decision 156 identified that a validated vector count did not isolate the clock from
Transport's unbounded backlog and arbitrary callbacks. The recurrence itself is finite and useful
at a signal owner, while event storage/dispatch is a different scheduler concern. Extracting and
composing it avoids both a duplicate clock and an unused Max/SPSC prototype, and makes the strongest
tractable real-time source property directly callable and testable.

**Consequence:** Native tests cover signed vector rejection, state preservation, fractional
progress, endpoint results, paused behavior, and position overflow. Existing Transport behavior is
implemented through the same clock and callbacks now observe the committed endpoint. The installed
package exposes and links the header. This remains source closure only: BlockClock supplies no
intra-vector offsets, cross-thread control/event transfer, Max clock, SDK lifecycle, package
external, deadline measurement, or named-host evidence.

## 158. Host sample count is validated before output-span construction

**Decision:** Add host-shaped LFO and ADSR block overloads taking
`(SignalBlockContext, int64 frame_count, double* output)`. They validate the signed count against
the context before inspecting the pointer or constructing a span. A non-empty null output returns
new error 3607 `RenderInvalidSignalBuffer`; zero frames admit null and remain a no-op. The existing
span overload remains the bounded internal/native-owner form.

**Reason:** Max `perform64` supplies signed `long sampleframes` and host-owned output pointer arrays.
Requiring an adapter to construct `span<double>` first left the dangerous signed-to-unsigned
conversion outside the enforceable source API. A negative fault could become a huge extent before
Sunny saw it, and a positive count with a null channel pointer had no distinct failure. The pointer
form is justified at this ABI-shaped trust boundary precisely because it delays span construction
until validation succeeds.

**Consequence:** Native adversarial tests prove error precedence and state/output atomicity for
negative, oversize, null-nonempty, empty-null, and valid calls. The error is exported through the
Python enum/stub for taxonomy parity, although Python's allocating convenience does not expose raw
pointers. This closes source-side ordering, not Max output topology or memory provenance: an
adapter must still validate `numouts`, the output-pointer array, channel selection, and actual
allocation extent before passing the exact host-owned pointer. Decisions 164 and 166 later move
`numouts`, zero-input `numins`, and pointer-array ordering into successively fuller source
overloads; allocation extent and host provenance remain external.

## 159. Local BlockClock state is not Live transport evidence

**Decision:** Specify BlockClock solely as a local constant-tempo recurrence. Neither matching BPM,
matching vocabulary (`tick`, `play`, `recording`), nor one imported host observation establishes
Max/Live clock-source identity, phase, position, automation following, or discontinuity handling.
No Sunny result may label BlockClock position as Live position without a separately specified and
validated host timing path.

**Reason:** Cycling '74 documents that unnamed transports in Live devices synchronize to Live by
default, named transports require `clocksource live`, same-named transports in separate device
instances run independently, and preview-mode transitions may disrupt timing continuity. The Max
transport reference also warns that translating beat-time song position may be inaccurate when a
Live Set contains tempo changes. A local integrator seeded from one BPM value therefore cannot
recover authoritative host phase through tempo automation, seeks, loops, or those mode boundaries.

**Consequence:** The formal render model and Ableton/Max matrix now make this non-equivalence
explicit. A future host-synchronized mode must name and configure the clock path, identify the
device-instance scope, define seek/loop/preview/tempo-change reset rules, ingest authoritative
boundary observations rather than a one-time tempo, and pass controlled named-Live tests. Until
then BlockClock remains useful perform-side source algebra but supplies no Live transport verdict.

## 160. SignalBlockContext replacement retains processor state

**Decision:** Define SignalBlockContext as immutable setup/call metadata, not lifecycle ownership or
a reset command. Passing a different validated context retains LFO phase/current/random-stream
state, Envelope stage/current/captured levels, and BlockClock integer/fractional position. The next
admitted block uses the new context rate and maximum. Explicit processor reset, BlockClock stop, or
position mutation remain the only source operations with reset effects.

**Reason:** Max invokes `dsp64` while rebuilding the DSP chain and can supply changed sample-rate or
vector facts. That lifecycle event does not itself specify Sunny's musical state policy. Leaving the
source behavior implicit would let adapters reset or retain state accidentally and disagree across
hosts. It would also encourage replacing a last-good setup with an invalid candidate before the
failure can be reported safely.

**Consequence:** An adapter constructs and validates a candidate context on every chain
rebuild and commits it only after success. Source state continuity is the default. A device that
intentionally resets on DSP stop/start must say so, invoke the explicit operations at a permitted
thread boundary, and test both lifecycle transitions and named-host behavior. No adapter or host
lifecycle claim is created by this source-side rule. Decision 173's concrete modulation adapters
subsequently implement the state-retaining candidate-commit behavior.

## 161. ScoreDocument owns the executable single-writer contract

**Decision:** Keep `Score` as a copyable, serialisable, single-owner value and add `ScoreDocument`
as its shared concurrency boundary. Copied handles share a current `shared_ptr<const Score>`.
Readers retain that immutable version; a per-document writer mutex serialises candidate work, and
a separate shared snapshot lock protects only acquisition and the final pointer replacement. A
successful transaction structurally validates its candidate and publishes exactly version (v+1);
any typed failure, invalid postcondition, or exhausted version leaves snapshot identity unchanged.

**Reason:** The formal Score specification promised single-writer/multiple-reader observable
semantics, but the only production surface was an unsynchronised value and one mutation allocator
was a non-atomic process global. Adding a mutex to `Score` would damage ordinary copying,
serialisation, and derived-value workflows. Holding one exclusive read/write lock through arbitrary
candidate work would satisfy safety but unnecessarily stop all readers. Immutable version
publication makes the promised boundary explicit while preserving the core value model.

**Consequence:** Tests prove shared-handle identity, immutable old-version retention, exact version
publication, failure identity preservation, reader availability during candidate work, and writer
non-overlap. The mutation ID allocator is atomic across independent documents. Raw mutation of the
same `Score`, recursive transaction on the same document, callback-owned side effects, semantic
merge, cross-process coordination, and concurrent `UndoStack` ownership remain outside the
contract and are named rather than inferred.

## 162. MCP request serialization owns raw session maps

**Decision:** Serialize `McpServer::process_request` per server instance and retain that mutex
through the complete synchronous handler call. Tool registration is required to finish before the
first request. The stdio loop and public parsed-request seam now share the same enforceable
application concurrency boundary.

**Reason:** ScoreDocument supplies concurrent shared Score ownership, but the production MCP
session also contains raw Score, Timbre, Mix, Corpus, undo, and deployment-plan maps with cross-IR
pointer resolution. The stdio executable happened to dispatch sequentially, while the public seam
did not prevent an embedder from entering two handlers concurrently. Migrating only Score storage
would leave the aggregate session and its plan/apply preconditions racy. Explicit request
serialization matches the existing server execution model and closes the actual shared-store
boundary as one unit.

**Consequence:** A concurrency test pauses one handler, attempts a second parsed request from
another thread, and proves the second handler cannot enter until release. A future parallel server
is an architectural change: it must use ScoreDocument snapshots and add equivalent versioned
ownership for Timbre, Mix, Corpus, undo state, deployment plans, transports, and cross-IR snapshot
resolution before removing this serializer.

## 163. Score schema v5 removes the false lifecycle cache

**Decision:** Remove `DocumentState {Draft, Valid, Compiled, Locked}` from the Score value and from
current JSON. Score schema v5 accepts versions 1–5; it range-checks and discards the legacy field
for versions 1–4, omits it on output, and rejects its presence in v5. Specify construction,
validation, mutation, compilation, serialisation, ownership, and destruction as operations and
lifetimes rather than a persisted state machine.

**Reason:** No production operation updated or enforced the field: a `Valid` value could be edited,
a `Compiled` value carried no target or input-version proof, `Locked` did not prevent mutation, and
there was no explicit close API. The formal lifecycle diagram used a different set of labels and
claimed transitions the runtime did not implement. Preserving either account would turn stale
metadata into false authorization or evidence.

**Consequence:** Validity is recomputed for an exact Score version, compilation returns
target-specific evidence, raw editability follows exclusive ownership, ScoreDocument editability
follows its writer serializer, and lifetime follows RAII/shared snapshots. Older documents remain
loadable after their legacy integer is checked, but a round trip intentionally removes it. This is
a schema change, so older runtimes correctly reject v5 instead of silently interpreting a new
document under the obsolete model.

## 164. Max-shaped modulation validates mono output topology

**Decision:** Add LFO and ADSR block overloads taking a signed frame count, signed output count,
and output-pointer array. They validate frames first, require exactly one output, permit a null array
only for an empty vector, and otherwise require both the array and channel-zero pointer before any
array dereference or span construction. New error 3608 `RenderInvalidChannelCount` distinguishes
topology from vector and buffer failures.

**Reason:** The earlier raw-channel overload moved signed `sampleframes` conversion into Sunny but
still required adapter code to inspect `numouts` and evaluate `outs[0]` first. Max's perform ABI
supplies both signed counts and the pointer array. That remaining precondition and error ordering
was tractable in the host-neutral source boundary and should not be duplicated by every adapter.

**Consequence:** Adversarial tests cover negative frames, negative/zero/multiple output counts,
null arrays, null channel zero, empty vectors, successful LFO/ADSR processing, and rejection
state/output preservation. The error is exported to Python for taxonomy parity, while Python's
allocating API does not expose pointer arrays. Pointer extent/provenance, platform-`long`
conversion, SDK callback registration, object lifetime, package binaries, and named-host timing
remain explicit external obligations.

## 165. Score version exhaustion is a failed admission, not a wrapped commit

**Decision:** Treat `UINT64_MAX` as an exhausted Score lineage. Every raw mutation performs an
admission check before changing the Score or an attached `UndoStack`; formal-plan and harmony
workflows do the same. The shared internal increment is checked independently. Undo and redo check
capacity after proving the requested history direction exists but before moving any history entry.

**Reason:** The formal model calls the version a strictly increasing Lamport timestamp that is
never reused, while raw mutation, workflow, undo, and redo paths used unchecked increment or
addition. Wrapping to zero would make an old and a new state observationally indistinguishable to
snapshot, cache, deployment-plan, and host-adapter consumers. Detecting overflow only at the final
increment would also be too late for in-place mutations because musical state may already have
changed.

**Consequence:** Exhaustion returns `ArithmeticOverflow` with the Score, version, stale regions,
undo history, and redo history unchanged. Tests exercise an in-place raw edit, a candidate-based
formal workflow, undo, and redo. `ScoreDocument` retains its stronger transaction rule: it rejects
the same condition before invoking the caller's candidate function and publishes one exact `v+1`
version regardless of how many lower-level candidate edits it composes.

## 166. Max-shaped generators validate both sides of signal topology

**Decision:** Add LFO and ADSR block overloads that accept signed frame, input, and output counts
plus both callback-shaped pointer arrays. These generators require exactly zero signal inputs and
one signal output. Validation proceeds frames, input count, output count, then non-empty output
pointers. The input array is never dereferenced. Both classes publish the counts as compile-time
constants used by the implementation and available to an adapter.

**Reason:** The fixed Max `perform64` signature carries `ins` and `numins` as well as the already
modelled output side, and the SDK constructs a generator explicitly with zero signal inlets. If an
adapter discarded `numins`, contradictory callback topology would become invisible and each future
external would have to recreate an error-ordering rule outside the tested source boundary.

**Consequence:** Adversarial tests prove negative/oversize frame precedence, negative/nonzero input
rejection before output validation, exact mono output validation, null rules for empty and
non-empty vectors, successful processing, and no state/output changes on rejection. This is still
not a Max external: platform-`long` conversion, flags/user data, class/DSP registration, outlet and
lifetime management, buffer provenance, packages, and named-host timing remain external evidence.
The overload is traditional mono, not MC. A future adapter preserving call-driven source time must
register its perform routine independently of outlet connection state; an MC surface requires a
separate per-channel state policy. Decision 173 subsequently supplies the traditional adapter and
package while retaining those external evidence limits.

## 167. DSP setup validity and processor compatibility are separate proofs

**Decision:** Add `Lfo::validate_context` as a const, `noexcept`, effect-free check of retained
frequency against a candidate `SignalBlockContext`. All LFO block paths use the same check at
runtime. An adapter must pass setup and processor validation before replacing its last-good
context, and must validate candidate control state against the active context before transfer.

**Reason:** `SignalBlockContext::create` proves only host sample-rate/vector facts. Max may rebuild
the DSP chain at a lower rate that remains structurally valid but is below the retained LFO
frequency. Waiting until the void, minimally protected perform callback to discover that condition
is too late for clean setup rejection and external error reporting.

**Consequence:** Native and installed-package tests can exercise the setup preflight without
rendering. Runtime processing remains defensive and all-or-none if an adapter misses the
preflight. Sunny still does not store a Max context or implement a control mailbox; candidate
publication, error-clock handoff, callback registration, and named-host lifecycle evidence belong
to the adapter boundary. Decision 173 subsequently implements the bounded mailbox, callback
registration, and source package; named-host lifecycle evidence remains external.

## 168. Score component identity is allocated from the candidate document

**Decision:** Replace the mutation, workflow, and reduction process counters with operation-local
typed allocators. Event, Part, and Section authoring indexes every same-typed ID already present in
the candidate and chooses the lowest unused positive `uint64` value. Pure reductions that replace
their event content use a deterministic output-local cursor; part extraction seeds its checked
allocator from retained source events.

**Reason:** S13 defines component identity in document scope, while three independent global
numeric ranges treated process history as an unstated namespace. A deserialised document could
already contain the next global value, and `fetch_add` could wrap. Candidate rejection could also
advance hidden state even though the published Score and history were unchanged. Atomic counters
prevented data races but could not establish freshness for an arbitrary document.

**Consequence:** Independent construction is deterministic; imported `UINT64_MAX` values coexist
with lower holes; generated component IDs cannot collide with retained IDs; and a rejected
operation consumes no identity. Checked mutation/workflow allocation reports `ArithmeticOverflow`
only if no positive value remains without wrapping. The legacy pure Score-returning reduction APIs
cannot report that theoretical exhaustion; on supported 64-bit targets their materialised Event
containers exhaust addressable memory first. `ScoreId` repository/lineage assignment is a separate
root-identity contract, subsequently closed by Decision 169 rather than conflated with S13
component allocation.

## 169. The Score repository owns root lineage identity

**Decision:** Make root identity an explicit input. `ScoreSpec` carries the requested positive
`ScoreId`, and every derived standalone-Score API requires a result identity instead of inventing
`source.id + 10000`. The MCP repository assigns that identity, stores the document under the same
numeric handle, and advances its counter with a checked maximum-value terminal state. S13 rejects
the zero/unassigned root sentinel.

**Reason:** Core creation always wrote root ID one, reductions fabricated an unchecked arithmetic
offset, and MCP stored those values behind an unrelated session handle. Consequently
`score_get_json` could report ID 10001 for repository handle 2, two core-created Scores could both
claim ID one, and a high source ID could overflow during derivation. A pure reduction has no
knowledge of the destination repository and therefore cannot prove a fresh lineage identity.

**Consequence:** Root identity assignment is now made at the only layer that can prove repository
uniqueness. MCP handle, stored Score, JSON serialization, snapshot lineage, and version ordering use
one value. `UINT64_MAX` may be committed exactly once; later creation and reduction return a stable
exhaustion error without overwriting the last Score. Standalone callers retain the convenient
`ScoreSpec` default of one but must choose distinct values when they own multiple lineages.

## 170. Active-context LFO control publication is one atomic source operation

**Decision:** Add `Lfo::set_frequency(frequency, context)` as a `noexcept` candidate-publication
operation. It validates the scalar domain, then compatibility with the active sample rate, and
commits only on success. It changes no phase, current value, seed, or random-stream state.

**Reason:** Setup-time `validate_context` closed the retained-processor/candidate-context direction,
but the inverse path still required each Max adapter to copy an LFO, call the unconstrained setter,
validate afterward, and implement rollback correctly. Mutating first and checking second is not an
atomic publication contract and can expose an incompatible live processor.

**Consequence:** Native tests compare rejected publication against an untouched recurrence and
distinguish invalid-scalar from rate-incompatibility errors; the installed-package consumer
compiles the overload. This method is intentionally not a mailbox or mutex. The Max adapter added
by Decision 173 serializes producers, transfers the validated candidate within a fixed bound,
retains exclusive audio ownership, and reports rejection outside `perform64`.

## 171. Source tuning is a complete finite pitch function; target tuning remains evidence-gated

**Decision:** Add non-optional `ScoreTuning` to Score schema 6. It assigns one finite relative-cent
position to every note index in the admitted MIDI/Live performance domain 0–127 under an exact
in-domain reference note and finite positive
reference frequency. The reference entry is zero, and every derived frequency must remain finite
and positive. No monotonicity, twelve-degree cardinality, or octave recurrence is imposed.
Authoring is versioned, atomic, and undoable; derived Scores copy the function; older schemas
migrate to canonical 12-TET/A4=440. General Scala scales may expand into it under the explicit
condition that the reference key is degree zero, adjacent indices advance one degree, and the
formal final interval repeats as the period.

**Reason:** Score's completeness principle said every sounding property was representable while
the actual note model depended on an unstated 12-TET/target tuning context. A twelve-class
temperament table cannot represent non-octave, non-twelve, non-periodic, or key-specific mappings,
and a parsed Scala file does not itself specify a keyboard mapping or a target mutation. Conversely,
Live 12.1 documents a writable `TuningSystem`, but its public dictionary descriptions do not close
the member schemas needed for a safe exact request/readback implementation. Treating either gap as
implicit alignment would make identical Score JSON sound differently across targets.

**Consequence:** JSON and MCP retain the exact 128-entry function. MIDI, NoteEvent, MusicXML, and
LilyPond count canonical 12-TET/A4=440 as their nominal baseline and report every other tuning as
one requested/zero-written residual. Ableton results retain exact `requested_tuning`, always report
one requested and zero written definitions, and cannot claim complete deployment on that dimension;
non-standard intent additionally warns. Snapshot schema 30's complete documented target payload,
including four exact opaque dictionaries, remains a separate bounded observation. Sunny will not
write Live tuning dictionaries until their wire
members, ordering, mutation/readback semantics, per-track bypass, and instrument/MPE applicability
are closed and verified against a named Live/Max runtime.

## 172. Production completeness belongs to the cross-IR Project tuple

**Decision:** Narrow Score's completeness claim to its owned symbolic, temporal, expressive,
articulation-mapping, and tuning domains. Define the production source model as the validated tuple
`ProjectView = (Score, TimbreProfiles, MixGraph)` under exact `PartId` bijections. Score owns musical
and nominal performance-event intent; Timbre owns source/device realization; Mix owns routing,
level, spatial, send, and output-destination intent. Corpus informs authoring but owns no deployed
state. Aggregate MCP results now retain the Score MIDI compilation report and include its drops or
diagnostics in `complete`.

**Reason:** The Score specification claimed it alone contained every sounding property and needed
no supplementary data, while the implemented compiler requires separate authoritative Timbre and
Mix documents to define instruments, effects, routing, and gain structure. That was not merely a
documentation imprecision: the project surface could omit Score compiler residuals while deciding
aggregate completeness, making a loss visible in the standalone compiler but absent from the
composed response.

**Consequence:** Local project specification and external target realization are separate proofs.
All three documents plus correspondence close the source production model; requested/written/
verified evidence closes only the admitted target subset. Structural `success` or `complete` never
implies playback trace, waveform, hardware output, or acoustic equivalence. A Score may still
compile independently to notation or nominal performance events, with every unsupported target
property retained in its report rather than being mistaken for complete production state.

## 173. Max modulation control becomes single-producer only after explicit serialization

**Decision:** Add the exported `sunny_max_adapter` library with traditional zero-input/mono-output
LFO and ADSR adapters. Every control publisher validates its input and takes a producer-only mutex;
the resulting logical single producer writes a fixed 64-command SPSC queue. The exclusive audio
owner takes no mutex and applies no more than 64 commands in order at the next vector boundary.
Queue overflow is a stable `RenderControlQueueFull` rejection, processing without a successful
setup is `RenderNotConfigured`, and lock-free status retains accepted, applied, rejected,
setup-failure, and process-failure evidence. `configure_dsp` validates a candidate context plus
retained and every pending LFO frequency before committing, preserving the last-good context,
queue, and recurrence on failure. Callback extent/topology/storage validation precedes queue
draining.

**Reason:** A raw render recurrence was insufficient for Max external alignment, while reviving the
deleted SPSC prototype directly would have assumed the very single-producer property that Max's
main/timer/scheduler arrangements do not guarantee. A general unbounded message queue would instead
move locks, allocation, or backlog-dependent work into `perform64`. Serializing only the publishers
establishes the SPSC precondition without making the audio callback wait; the fixed capacity makes
the block-edge work and overflow policy part of the model.

**Consequence:** The independent `max-package/` project pins max-sdk-base and supplies actual
`sunny.lfo~` and `sunny.adsr~` C++23 wrappers for macOS/Windows. They create contexts from the exact
`dsp64` setup pair, unconditionally register perform, forward the exact signed topology, call no Max
API from perform, and stage package output inside the build tree rather than a user home directory.
The staged package includes help patchers and reference/autocomplete XML whose public method sets
are checked against the actual `class_addmethod` registrations; CI builds and uploads both supported
platform forms.
Native tests cover validation, ordering, concurrency serialization, saturation, callback
transactionality, context retention, and silence bounds; the same wrapper sources compile against
the pinned official SDK headers. These facts supersede Decisions 153, 160, 164, 166, 167, and 170
only where those decisions described the adapter or package as future work. Supported-platform
linking, Max binary loading, actual buffer provenance, disconnected
callback continuity, audio deadlines, `.amxd` wrapping, and named Live/Max behavior remain external
evidence rather than inferred source properties.

## 174. Layer direction and repository metadata are configure-time invariants

**Decision:** Make the four native layer rules executable during every root CMake configure.
`cmake/Architecture.cmake` enumerates public/private headers and implementation files, extracts
installed-form Sunny includes, and rejects a layer outside its admitted dependency set. It also
requires every implementation to belong to its declared target, rejects foreign repository sources
attached to that target, and exact-compares local Sunny target-link edges. The same gate binds the
Python and Max package version literals to the root project version and current-facing bridge
examples to the two-field bridge contract authority. Every inspected file is a CMake configure
dependency, so changing the contents of an existing source, header, metadata file, or current
reference document reruns the gate on an incremental build.

**Reason:** Explicit target source lists prevent accidental recursive compilation but ordinarily let
a newly added implementation remain silently orphaned. A diagram and prose rule likewise do not
prevent a compilable upward include or an extra link edge, while a green metadata parser that
hard-codes the same stale version can agree with the wrong authority. These are dependency-tree and
interface misalignments even when behavior tests happen not to touch them.

**Consequence:** The admitted graph is `core -> {}`, `render -> core`, `max_adapter -> render`, and
`infrastructure -> {core, render}`, with self-includes allowed in each layer and composition roots
outside the rule. Six fixture projects must fail for forbidden include, orphan source, foreign
source, wrong local dependency, protocol-document drift, and package-version drift. Historical
protocol versions in this decision record remain out of scope; only current-facing reference
examples are bound to the active authority.

## 175. SampleAndHold projects to Max as a message-driven held signal, not a sampler

**Decision:** Give `SampleAndHold` a bounded traditional generator block form and add
`SampleAndHoldAdapter` plus the `sunny.hold~` SDK external. The source has zero signal inputs and one
signal output. Each admitted vector is filled with the retained finite normalized scalar. Value and
reset messages use the same producer serialization, fixed 64-command queue, vector-edge
application, preflight-before-drain ordering, lock-free status, and validated silence policy as the
other modulation adapters.

**Reason:** SampleAndHold was part of the authoritative render model but absent from the maintained
Max projection. Calling it a Max sample-and-hold without further design would be worse than leaving
it absent: a conventional sampler needs a signal input, trigger/edge semantics, intra-vector
position, and a before/after-edge sample convention that the Sunny source does not own. The name
`sunny.hold~` exposes exactly the behavior that is implemented.

**Consequence:** The staged Max package now contains three documented traditional externals.
`sunny.hold~` accepts only finite `[-1,1]` float/value messages plus reset/status/error controls;
malformed callbacks cannot consume them. Its actual registrations are compared with reference XML,
the wrapper compiles against pinned official SDK headers, help and reference assets are staged, and
macOS/Windows CI requires the third platform binary. Signal-triggered sampling, intra-vector
changes, Max for Live wrapping, binary loading, callback timing, and audible/signal observation
remain separate work requiring an explicit model and named-host evidence.

## 176. The compiled Python module and its stub are one checked interface

**Decision:** Treat `stubs/sunny_native/__init__.pyi` as the declared downstream shape of the
optional pybind11 module and compare it against the extension produced by the current CMake build
with `mypy.stubtest`. The gate removes scikit-build-core's editable meta-path finder, resolves the
runtime directly under the selected build directory, and fails if another installed extension is
chosen. Its allowlist is limited to exact pybind11 metaclass mismatches, enum `__members__` inferred
by Python typing, and generic initializers on non-constructible result/view types. Native enum
values remain scoped rather than being duplicated as undeclared module globals, read-only runtime
fields are properties in the stub, and bridge version constants plus snapshot validation are part
of the declared interface. The extension version is compiled from the root CMake project version.

**Reason:** Facade type checking proved only code that consumed the handwritten stub; it did not
prove that the loaded binary implemented that stub. An older editable installation could also
precede `PYTHONPATH` through a meta-path finder. This allowed missing native methods, undeclared
globals, mutability mismatches, and a hard-coded extension version to remain independently green.

**Consequence:** `make python-check` and every Python-version CI job now require facade typing,
runtime tests, and direct stub/runtime agreement against the just-built extension. Adding,
removing, renaming, or changing mutability of a native symbol requires the binding, stub, and tests
to move together. Python remains a thin downstream projection of C++ rather than an independently
evolving model or a fallback implementation.

## 177. Corpus lifecycle mutations own their reverse references and derived aggregates

**Decision:** Make corpus ownership, period membership, and derived profiles one synchronously
maintained model. Composer reassignment normalises all owner and period reverse references;
period assignment and clearing update both membership and metadata; analysis refreshes every
profile that can depend on the work; and `remove_ingested_work` removes the root plus every reverse
reference without reusing its MCP session id. Composer and period StyleProfiles are rebuilt from
unique analysed work memberships. A membership or analysis change invalidates explicit composer
signature detection, while a period-only change preserves composer signatures. Mixed MCP metadata
and period changes are staged on a candidate corpus and commit only as one valid operation.

**Reason:** The prior public structs and MCP metadata setter allowed the same work to claim one
period while a composer period claimed another, and ordinary analysis/reassignment left aggregate
profiles indefinitely stale unless callers remembered an unrelated rebuild command. Local C1–C13
checks could all pass for a globally contradictory corpus. That made serialized state, queries, and
the MCP projection disagree even though each individual entry looked well-formed.

**Consequence:** C14 now rejects map-key/embedded-id disagreement, missing or duplicate ownership,
signature examples outside their profile membership, incoherent period references and metadata,
duplicate labels, invalid ranges, and overlaps between fully bounded period ranges. Version-2
and version-3 corpus loads enforce that graph boundary; version 1 remains
lenient for explicit migration compatibility. At this decision revision the corpus MCP group had 22
tools and the complete runtime had 112; later Timbre additions raised that total to 115, and the
typed harmony authoring tool in Decision 231 raises the current total to 116.
Directly assembled C++ values can still contain caller-supplied aggregate payloads,
so `rebuild_style_profile` remains the explicit normalisation operation; persistence remains
caller-owned and there is still no background database or statistical signature refresh.

## 178. Corpus schema v3 closes the authoritative persistence projection

**Decision:** Advance Corpus JSON from schema 2 to schema 3 and make the current writer and strict
reader field-complete for the document model. The projection now retains every WorkAnalysis and
StyleProfile subrecord, optional orchestration, composer active periods, complete tonal and
thematic structures, and every `SignaturePattern.pattern_data` alternative. Numeric-key maps use
ordered `{key, value}` records; pattern data uses a closed named tag rather than guessing a variant
from JSON shape or musical domain. Optional values remain omitted when disengaged. Version 1 stays
lenient and version 2 stays strict for its historical partial shape; neither migration reader
invents values for fields its source schema did not contain.

**Reason:** Schema 2 claimed that round-trip preserved all fields while its writer discarded the
entire melodic analysis, orchestration analysis, most rhythmic/formal/textural/dynamic/motivic
analysis, most aggregate-style dimensions, `active_period`, and pattern payloads. A writer and
reader could therefore agree with each other while silently erasing the authoritative in-memory
model. That was internal persistence misalignment and also made any downstream corpus query after
reload depend on whether the process had previously held the richer C++ value.

**Consequence:** Fully populated work, composer, period, and corpus fixtures now require stable
write-read-write JSON equality, exact object key inventories, all four pattern variants, optional
presence and absence, narrow-integer range checks, unknown-tag refusal, and explicit v1/v2
migration behavior. C15 compares deterministic non-signature composer and period aggregates with
their analysed memberships, so direct validation and v3 load reject stale serialized statistics;
explicit signature detection remains independent. Full-corpus v2 migration retains its historical
pre-C15 validation contract, v2/v3 both apply C14, and v1 remains lenient. Schema evolution is now
honest about the tractable claim: v3 preserves and validates the complete current Corpus document,
while old files preserve exactly their historical represented subset.

## 179. Corpus aggregates are evidence functions, not descriptive guesses

**Decision:** Make every tractable `StyleProfile` dimension a deterministic function of explicit
`WorkAnalysis` evidence and persist every newly required evidence/output field in Corpus schema v3.
The model now carries melodic `note_count`, rhythmic metre counts, preferred keys, intervals,
durations and metres, formal transition techniques, and orchestral build-up techniques. It derives
the remaining supported harmonic, melodic, rhythmic, formal, voice-leading, textural, dynamic,
orchestral, and motivic fields with closed denominators, ranking, tie, evidence-presence, curve,
and classification rules. Empty melodic parts and absent dynamics remain neutral. The small set of
fields for which no work-level carrier exists remains explicitly unavailable rather than being
inferred from an unrelated proxy.

**Reason:** A field-complete serializer only prevents data loss. It did not make the serialized
profile true: earlier rebuilds populated a subset, fabricated 120-BPM preferences and arch-shaped
dynamics in empty data, treated an empty part as a 127-semitone melody, and left represented
evidence such as metre, tonal-plan relationships, resolution patterns, density/dynamic curves,
instrument combinations, and transformations disconnected from their style fields. The prose
model also named members and variants absent from C++, so documentation and runtime could each be
self-consistent while disagreeing with one another.

**Consequence:** C15 now compares a materially complete deterministic aggregate. Exact v3
round-trips inventory the added fields, analysis tests cover metre evidence, melodic emptiness, and
tonal-plan tuple semantics, and a multi-work adversarial fixture exercises every supported domain
plus every neutral unavailable field. The formal specification distinguishes automatic analyser
output, caller-supplied WorkAnalysis evidence, and unavailable properties. Ableton/Max conformance
separately records that internal Corpus freshness is not Live deployment proof, a Live-to-Corpus
round-trip, a Max analyser contract, or rendered stylistic equivalence.

## 180. Expose local clock position without claiming host transport

**Decision:** Give BlockClock a traditional zero-input/one-output signal projection and package it
as `sunny.clock~` through a bounded `ClockAdapter`. For a running vector, sample `i` is the local
quarter-note coordinate `(tick + fractional_tick + i * tempo * ppq / (60 * sample_rate)) / ppq` at
the start of that sample. Paused and stopped vectors hold the retained fractional coordinate. The
signal call commits exactly the endpoint of the existing block recurrence only after its complete
callback shape and arithmetic plan succeed. Tempo, tick position, play, record, pause, and stop are
producer-serialized, fixed-capacity commands applied in order at the next valid vector edge.

**Reason:** BlockClock already isolated the tractable O(1) audio-owner algebra, but returned only an
integer interval to C++. That left the Max downstream boundary hypothetical and made the sample
convention unstated. Conversely, wiring a local BPM integrator to a transport-sounding object name
without a negative contract would invite a false Ableton claim: shared BPM and quarter-note units
do not establish Live clock source, phase, seeks, loops, preview behavior, tempo automation, or
per-device identity. The Max SDK lifecycle also requires signed callback counts and pointer arrays
to be validated before conversion, control drain, or output access. Its `A_LONG` atom is
pointer-sized `t_atom_long`, not the platform C++ `long` type; the distinction is observable on
64-bit Windows.

**Consequence:** The shared Max control-transfer foundation now serves modulation and clock
adapters; the package contains four traditional generators with checked help/reference selector
and argument schemas plus pinned-SDK header compilation. Direct BlockClock signal rejection preserves output
and recurrence. ClockAdapter rejects malformed callbacks before command consumption; if valid
edge controls create an arithmetically unrepresentable endpoint, those accepted controls remain
applied while recurrence/output remain unchanged and the wrapper silences the validated vector.
`sunny.clock~` remains explicitly local: it contains no Max `transport`, `clocksource live`, Live
Object Model call, MIDI event bridge, or sample-offset scheduler. Any host-synchronised successor
requires a separate clock/discontinuity algebra and named Max/Live evidence. Integer message
handlers use `t_atom_long` before checked conversion so the Windows/macOS package builds share one
declared numeric domain.

## 181. Separate bounded event offsets from endpoint callbacks and host delivery

**Decision:** Add `BlockEventScheduler` as an exclusive-owner fixed-capacity planner beside dynamic
Transport. It retains at most 256 tick events in inline storage and writes every event due in a
running vector into caller-owned `SampleOffsetEvent` storage. For exact implementation start
position `x`, tick increment `s`, and `N` samples, the vector owns `[x,x+Ns)`: an event at or behind
`x` maps to offset zero, a later event maps to `floor((k-x)/s)`, and an event exactly at the endpoint
remains queued. Tick, note-off-before-note-on, and insertion order are stable. Dynamic Transport
keeps its existing endpoint callback semantics.

**Reason:** A maximum frame count does not bound a dynamic event backlog, callback work, or output
storage, and an integer tick endpoint does not identify a sample. Clamping an exact endpoint into
the previous vector changes its time; making both ends inclusive can duplicate it. Adding offsets
to the existing callback after BlockClock has already committed would still be too late for a host
to act inside that vector and would leave unbounded storage/callback behavior intact. Max further
forbids treating an arbitrary outlet call from `perform64` as a supported handoff, so source
placement and host delivery must remain separate claims.

**Consequence:** Raw event validation and atomic note-pair construction are shared by Transport and
the bounded scheduler. `RenderEventQueueFull` rejects capacity overflow; `RenderEventBufferFull`
rejects an undersized due-event span. Block processing validates its SignalBlockContext, clock plan,
due cardinality, and every offset before changing output, queue, or clock. Success performs at most
256 cells of scan/output/compaction work and invokes no allocation, callback, lock, I/O, or Max
operation. This is a tractable source-side offset algebra only: schedule-command transfer,
producer ordering, Max clock/MIDI handoff, latency, and named-host timing remain required before any
sample-accurate Max or Ableton event claim.

## 182. Use Max permanent ITM locations for the first host-synchronised event path

**Decision:** Add `ItmEventAdapter` and package it as the non-DSP `sunny.events` object. Arbitrary
message publishers are serialized into a fixed 64-command stream; one Max scheduler consumer owns
256 reserved event cells and 256 pre-created permanent `t_timeobject` slots. `event` admits one raw
note-on/off and `note` admits an integral-tick note pair atomically. Commands are applied in order
against one finite global-ITM position/resolution snapshot. Each source tick must map to a finite,
strictly future host tick whose neighboring source ticks remain distinct in binary64. Equal-tick
callbacks drain the complete note-off/note-on/insertion-ordered group once.

**Reason:** Calling a Max outlet from `perform64` is not a supported event handoff, while deferring
an already-crossed BlockEventScheduler offset cannot recover its intra-vector time. Cycling '74's
ITM API directly represents permanent events tied to transport locations, and the unnamed global
transport is the documented Live-synchronised path inside Max for Live. That solves a different
problem from source-vector offsets and avoids inventing an audio-to-message protocol. It also
exposes two adversarial numeric boundaries: ITM locations are doubles rather than Sunny `int64_t`
ticks, and an intended position is already stale if the scheduler applies the command at or after
that position.

**Consequence:** `sunny.events` uses `itm_getglobal`, `itm_getticks`, `itm_getresolution`,
`TIME_FLAGS_PERMANENT`, `TIME_FLAGS_LOCATION`, and the `TIME_FLAGS_TRANSPORT` attribute whose
documented default is the global ITM object; it never mutates transport state/resolution or creates
a named transport. Successful queue publication and `clock_fdelay` wake are serialized together.
Clear is ordered with surrounding publications, but does not release reservations speculatively;
capacity and callback-storage failures are fail-closed, same-time duplicate slot callbacks are
harmless, and downstream feedback can only publish a later command rather than reenter
scheduler-owned state. This selects a documented Max/Live clock path but does not prove sample
accuracy: Overdrive, Scheduler in Audio Interrupt, timely high-priority arrival, supported
downstream objects, seek/loop/preview behavior, binary loading, and deadlines remain named-host
evidence. BlockEventScheduler remains the separate native sample-offset path and is not consumed by
this object.

## 183. Make Ableton validation evidence executable and versioned

**Decision:** Define strict `AbletonValidationRecord` schema 1 around one consumed aggregate plan
and apply attempt. Retain the canonical project state, PPQ, planning snapshot, ordered planned
mutations, apply/final snapshots, mutation journal, aggregate compilation/postcondition JSON,
derived execution/partial-modification verdicts, operator-supplied environment provenance, and
operator-supplied recovery notes. Let the two mutating aggregate MCP paths accept optional
`validation_context` before target access and return this record for the actual attempt. Add one
canonical current-protocol request decoder, make the recording transport synthesize a complete
schema-28 snapshot, parse target profiles as closed objects, generate the installed Sunny version
header from the root project, and reject stale bridge-protocol literals in current native source
at configure time.

**Reason:** Prose asking for a named-build record did not specify an auditable artifact. It allowed
offline command recording, operator assertions, transport acknowledgement, compiler completeness,
and sonic correctness to be confused. The prior `CommandBuffer` also advertised schema 28 while
synthesizing an older snapshot, and extended target profiles could cross a supposedly strict trust
boundary. Those inconsistencies made repository-only tests look stronger than the model they were
testing and left a future Live run difficult to interrogate after protocol evolution.

**Consequence:** Unknown or extended fields, stale versions, noncanonical requests, contradictory
profiles or lifecycle shapes, journal sequence/prefix divergence, and forged derived verdicts now
fail parsing. `execution_trace_complete` requires a completed real-transport-shaped attempt with
matching snapshots and one acknowledged entry per planned mutation; it remains false for
`CommandBuffer` and independent of compiler `complete`, audio rendering, authenticity, and sonic
equivalence. A missing context yields a null record rather than invented host facts. The repository
still has no named-host record, so live Ableton/Max certification remains an external milestone.

## 184. Make exact time a canonical value before it reaches a host boundary

**Decision:** Replace aggregate `Beat` storage with an encapsulated normalising value type. Valid
two-integer program construction immediately reduces signs, common factors, and zero; independent
component mutation is impossible. Add `Beat::from_ratio` for dynamic integer input, retain checked
arithmetic for representability failures, and make all shared IR writers emit the stored lowest-term
pair. Retire Score rule S12 as a runtime pass: it is now a type invariant, while JSON/MCP/import
readers remain responsible for refusing zero or otherwise inadmissible denominators before a
`Beat` exists.

**Reason:** The formal model required every Beat to be an irreducible rational, but native
aggregate construction could store `4/4`, serialization emitted that spelling, and the reader
returned `1/1`. A freshly written Score could therefore change JSON after one parse, perturbing
project-state equality and stale-plan evidence without changing any music. Public component
mutation also let C++ fabricate the malformed state that validation claimed to detect. This was
an internal contradiction with external consequences. Ableton Live's modeled note-time boundary
counts quarter-note units (`host_beats = 4 × Beat`) and carries numeric positions/durations rather
than Sunny rational spellings; Max ITM likewise consumes its own host coordinate. Neither host can
repair or meaningfully distinguish Sunny's non-canonical fraction spelling. Canonicalisation must
therefore happen before conversion, planning, hashing, persistence, or host mutation.

**Consequence:** `Beat{4,4}` is observably `1/1`; zero is only `0/1`; C++ and Python expose
read-only components; dynamic zero denominators fail on checked boundaries; and canonical Score
JSON is a write-read-write fixed point. Score validation continues to police musical sign/domain
rules such as positive duration and non-negative position, but no longer scans for an impossible
denominator state. The Live scale map, admitted Live time-signature domains, floating host
precision, Max scheduling semantics, and named-host evidence obligations are unchanged: exact
internal canonicality prevents false differences, but does not prove host timing, transport,
latency, or sonic equivalence.

## 185. Make exact tempo rate canonical before effective-quarter and host projection

**Decision:** Replace aggregate `PositiveRational` storage with an encapsulated, normalising,
strictly positive value type. Valid program construction reduces the pair immediately and exposes
only read-only components. Dynamic integer pairs use `PositiveRational::from_ratio`; Score readers
reject non-positive input before constructing a TempoEvent; writers persist only the stored
lowest-term pair. S24 retains transition, enum, and exact metric-modulation validation but no longer
pretends to discover an impossible malformed rate object.

**Reason:** `PositiveRational` was specified as a strictly positive exact rational but stored two
public integers and used structural equality. Therefore `240/2` and `120/1` ordered as the same
tempo yet compared and serialised differently, while zero or negative rates could be fabricated
for a later validation pass. This could perturb canonical Score/project JSON, guarded-plan state,
and validation records without changing the resolved tempo. Live receives only numeric
quarter-note `Song.tempo`; Max clock controls likewise consume floating rates. Neither external
surface preserves, distinguishes, or can repair Sunny's source fraction spelling. Canonical source
identity must precede beat-unit resolution, persistence, planning, and host conversion.

**Consequence:** Equivalent BPM fractions now compare and persist identically; invalid dynamic
pairs fail at the reader/factory boundary; exporters observe one lowest-term representation; and
write-read-write Score JSON remains stable for tempo input. The separately stored `BeatUnit` still
preserves marked-unit semantics, and exact effective-quarter algebra remains checked. Conversion
to binary floating point, Live tempo automation/authority, Max clock behavior, named-host timing,
and audible results remain separate external obligations.

## 186. Preserve grouped-metre identity across the flat Ableton boundary

**Decision:** Replace aggregate `TimeSignature` storage with an encapsulated construction-safe
value. Its default is canonical 4/4; all admitted values have a non-empty positive group partition,
a representable positive numerator, and a power-of-two denominator. Dynamic and JSON input use
`TimeSignature::from_groups`. Ableton compilation separately counts explicit source grouping
requested and written; a partition that differs from Sunny's deterministic reconstruction of the
same numerator/denominator is retained as requested, reported as zero written, and warns.

**Reason:** The formal model said that an ordered partition determines metrical structure, but the
native aggregate allowed empty, zero, negative, overflowing, or non-power-of-two state until S5.
More subtly, the compiler reported a `3+2/8` signature as written after sending only Live's `5` and
`8` scalar properties. The current documented Song and Clip LOM surfaces expose no ordered grouping
field, so flat scalar readback cannot distinguish `3+2` from `2+3`. Internal validity and external
representability were both overstated.

**Consequence:** Algorithms and persistence receive only valid grouped metre; S5 remains responsible
for time-map origin/order/range rather than rescanning impossible values. MusicXML and LilyPond may
retain explicit grouping, while the Ableton result preserves the residual through
`time_signature_groupings_requested/written`. A scalar meter write can succeed without compilation
being complete. This does not establish Live's accent behavior, playback, named-host persistence,
or sonic equivalence.

## 187. Project complete SMF metre payloads without inventing ordered grouping

**Decision:** Make `MidiTimeSigData` carry numerator, actual denominator, MIDI clocks per
metronome click (`cc`), and notated 32nds per MIDI quarter (`bb`) through the infrastructure file
adapter. Before narrowing, the Score compiler enforces its current exact numerator 1…255 and
denominator ≤128 profile. It writes `bb=8`. For stored groups \(g_i\) and denominator \(d\), it
writes `cc=96*gcd(g_i)/d` when that is an integral byte, otherwise the conventional 24-clock
quarter. Compilation reports requested/written event and ordered-grouping counts separately.

**Reason:** The low-level SMF model already retained the standard `FF 58 04 nn dd cc bb` payload,
but the `CompiledMidi` adapter discarded `cc` and `bb` and silently substituted 24/8. This made
canonical compound `6/8` lose its dotted-quarter metronome click instead of writing 36 clocks. It
also allowed a source numerator such as 256 to narrow before the target boundary. Conversely,
SMF's one `cc` interval is not an ordered list: a common eighth-note grid for `3+2/8` preserves
boundaries but cannot distinguish that partition from `2+3/8`. A successful flat meta-event is
therefore weaker than complete grouped-metre alignment. The MIDI Association describes the four
payload fields and the canonical 6/8 value
([Standard MIDI Files](https://midi.org/standard-midi-files),
[time-signature field discussion](https://midi.org/community/midi-specifications/time-signature-understanding)).

**Consequence:** Score → SMF now emits a deterministic click grid informed by source grouping,
preserves the complete value through file serialization and parsing, and rejects target widths
without wrapping. Decision 188 corrects the initially over-broad interpretation of this decision:
even a matching uniform click does not constitute a written ordered grouping. No claim is made
about Live accent behavior, Max interpretation, scheduler timing, or named-host sound.

## 188. Keep SMF metronome semantics separate from ordered metre in both directions

**Decision:** Treat SMF `cc` exclusively as a metronome-click interval. MIDI compilation counts
only partitions distinct from Sunny's deterministic flat-signature grouping as grouping requests,
and counts zero grouping definitions written. The derived click may align with all source group
boundaries, including one repeated group span, but that alignment is descriptive rather than a
type conversion. Corpus MIDI ingestion reconstructs grouping only from the deterministic `nn/dd`
rule, records nonmatching `cc` values as `midi.time_signature_metronome_clicks` manual-correction
evidence, requires same-tick signatures to agree in all four fields, and rejects `bb!=8`.

**Reason:** The standard defines `cc` as the number of MIDI clocks in a metronome click and `bb` as
the number of notated 32nd notes in the MIDI quarter. Its canonical 6/8 example uses `cc=36` for a
dotted-quarter click, but does not turn the byte into an ordered partition field. Numeric equality
between a click span and a Sunny group span is therefore insufficient for semantic round-trip.
Conversely, ignoring `bb` can change how notated measure durations relate to the file's quarter-note
tick axis. The MIDI Association's hosted specification page and field discussion make these units
explicit ([Standard MIDI Files](https://midi.org/standard-midi-files),
[time-signature discussion](https://midi.org/community/midi-specifications/time-signature-understanding)).

**Consequence:** Canonical `6/8` still emits 36 clocks and incurs no explicit-group residual.
Non-default uniform `2+2/4` may emit a matching 48-clock click but remains one requested/zero-written
grouping, just like additive `3+2/8`. Reverse ingestion does not invent either grouping from a
metronome value. Noncanonical but timing-compatible clicks survive as explicit Corpus correction
evidence; incompatible notation scaling and conflicting simultaneous payloads fail closed. The
low-level `MidiFile` round trip remains lossless, and Live/Max grouping, metronome behavior, and
audible accents remain separate external propositions.

## 189. Reverse only self-contained MIDI pedal state into Score spans

**Decision:** During Corpus MIDI ingestion, preserve a sole source note channel as the generated
Part's rendering channel and reverse completed Damper (CC64) and Soft Pedal (CC67) switch pairs on
that channel into `SustainingPedal` and `UnaCorda` PartDirective spans. Apply the standard switch
thresholds (0…63 off, 64…127 on), but record each imported noncanonical byte because Score
recompilation deliberately emits 0/127. Do not import a controller timeline containing an
opposite-state same-tick change; PartDirective has no endpoint ordering or pedal-retake carrier.
Record repeated states, unmatched endpoints, cross-channel pedals, all other CCs, every Program
Change, multiple note-channel ownership, and Type-1 track topology as distinct Corpus correction
evidence.

**Reason:** The MIDI Association defines CC64 and CC67 as switches with those thresholds
([MIDI 1.0 Control Change Messages](https://midi.org/midi-1-0-control-change-messages)). That gives
these two completed ranges a semantic Score carrier, but not byte identity: Sunny's forward
projection is canonical 127/0. An arbitrary CC or Program Change at a tick is not an
`ArticulationMapping`; the latter means “emit this when this known Score articulation occurs,” and
the MIDI file does not contain that source identity. Likewise, collapsing several MIDI channels
into one Part makes a channel-local pedal unsafe to globalise. Ableton's product imports and edits
MIDI controller data as Clip envelopes
([Live 12 Clip Envelopes](https://www.ableton.com/en/manual/clip-envelopes/)), but the public Clip
LOM lists envelope clearing and note editing without a corresponding controller-envelope creation
method ([Clip LOM](https://docs.cycling74.com/apiref/lom/clip/)). Max can format CC and Program
Change messages ([`midiformat`](https://docs.cycling74.com/reference/midiformat/)), but that object
contract does not supply Sunny with source articulation identity, a Part binding, event scheduling,
MIDI-port identity, or Live-device behavioral evidence.

**Consequence:** Canonical single-channel pedal pairs now survive MIDI → Corpus Score → compiled
MIDI with their channel, ticks, controllers, and switch semantics. Threshold-equivalent values
survive semantically with explicit normalization evidence. Every non-admitted retained event is
counted rather than silently disappearing, and every multi-channel/Type-1 reduction announces its
ownership loss. This neither widens the public Live bridge nor adds a Max patch: Live controller
envelope authoring, Clip bank/program state, Max scheduling/port delivery, receiving-device state,
and audible behavior remain independent external obligations.

## 190. Separate onset and duration loss at the Corpus quantisation boundary

**Decision:** Define Corpus MIDI quantisation over exact non-negative `Beat` values and a positive
integer grid `g` measured as equal divisions of one whole note. Multiply by `g` with checked
rational arithmetic, round to the nearest integer with an exact positive half-grid tie forward,
then divide by `g`; force a positive duration that would round to zero to `1/g`. Record independent
RMS whole-note residuals for onset and duration. Corpus schema v4 requires both finite,
non-negative residuals and finite `[0,1]` confidence dimensions. Versions 1–3 migrate the
historically absent duration residual to zero as a compatibility default.

**Reason:** The ingester already changed both onset and duration, but reported only onset RMS, so a
grid-aligned attack with a shortened release looked lossless. Grid selection used binary floating
point even though source ticks and Score beats were exact, leaving halfway behavior dependent on an
unnecessary representation boundary. The external systems do not repair this omission. Live's
[Clip LOM](https://docs.cycling74.com/apiref/lom/clip/) exposes independent floating note
`start_time` and `duration`; launch quantisation and groove are different properties. Max
[tempo-relative time](https://docs.cycling74.com/userguide/time_value_syntax/) uses 480 ticks per
quarter and is scheduler-limited, while [`mtr.quantize`](https://docs.cycling74.com/reference/mtr/)
changes playback without modifying stored events. None supplies Sunny's analytical rounding or
loss evidence.

**Consequence:** An imported note can no longer hide duration damage behind a zero onset metric,
exact half-grid cases have one platform-independent result, and malformed confidence evidence
cannot evade validation through NaN comparisons. The new field is authoritative C++ state and
round-trips through the current Corpus schema. This does not claim equality with Live stored-note
floats, Live launch/groove behavior, Max event storage, transport scheduling, or audible timing;
those remain separate readback and named-host obligations.

## 191. Own release velocity once and project it consistently across MIDI, Live, and Max

**Decision:** Add integer `release_velocity∈[0,127]` to Score `Note` and generic `NoteEvent`, with
neutral default 64. Score schema v7 requires it; v1–v6 migrate absence to 64 and preserve a valid
explicit field when one is already present. For a tie chain, the first segment owns attack
velocity and the terminal segment owns release velocity. Preserve the
field through SMF parsing/writing, Corpus quantisation/voice allocation/tie splitting, compiled
MIDI, generic NoteEvent conversion, Python readback, and MCP note insert/modify. Advance the Live
bridge to protocol v36: probability 1.0 and velocity deviation 0.0 remain adapter-owned, while
release velocity is the Score-owned integral value encoded as the floating category required by
the closed note dictionary. Reject integral JSON encoding, fractional floats, non-finite values,
and values outside 0–127. Extend render note pairs and `sunny.events` to retain the value; the Max
external accepts an optional release argument and emits
`[pitch, attack_velocity_or_zero, release_velocity]`.

**Reason:** The MIDI Association defines Note Off as carrying key and velocity
([MIDI 1.0 message summary](https://midi.org/summary-of-midi-1-0-messages)), while Live's public
[`Clip` LOM](https://docs.cycling74.com/apiref/lom/clip/) exposes `release_velocity` in note
creation and selected readback. Max can format MIDI Note Off data
([`midiformat`](https://docs.cycling74.com/reference/midiformat/)). Sunny previously retained a
nonzero source value only in `MidiFile`, rejected it from the analytical projection, emitted zero
to SMF from Score, and emitted 64 to Live. Those target-dependent results contradicted one source
model. A tied sound has one attack at the chain head and one release at its tail, so terminal
ownership matches the physical event boundary without forcing inert intermediate fields equal.

**Consequence:** One Score now generates the same release intensity for SMF and Live, and valid
SMF release state survives Corpus ingestion. Protocol/readback tests distinguish 23.0 from the old
constant, and adversarial type/fraction/range cases fail closed. Max's maintained source path no
longer truncates the field to a two-atom list. Full notation exports report non-default values as
explicit residuals, and compact notation adapters reject them. This establishes model and
translation alignment, not a named Live mutation, Max scheduler callback, MIDI-port transfer,
receiving-device response, or acoustic result; those external evidence classes remain explicit.
This decision supersedes Decision 93's former five-field/rejection policy.

## 192. Penalise every overlap-complex MIDI onset and make equal-pitch allocation stable

**Decision:** Define a Corpus MIDI onset as complex when it contains multiple quantised notes or
when any earlier allocated note is still sounding. For `c` complex onsets among `n>0`, set voice
confidence to `clamp(1-c/n,0.3,1)` and record `midi.voice_separation` correction evidence with the
exact counts whenever `c>0`. Sort a same-onset group by descending pitch and retained source
ordinal, then allocate the first free voice. Carry velocity, release velocity, and source ordinal
with the note throughout the transform.

**Reason:** The former metric inspected only same-onset group cardinality. Two staggered notes
could require separate voice lanes while reporting confidence 1.0, and equal pitches had no strict
secondary ordering under `std::sort`. SMF has tracks and channels but no general notation-voice
identity; Live or Max playback concurrency cannot reconstruct one after the fact.

**Consequence:** Confidence now reflects every allocation that depends on overlap rather than only
chords, exact correction evidence explains the reduction, and equal-pitch identities are stable
across implementations. The heuristic remains explicitly an analytical reduction, not recovered
compositional voice or a claim about Live/Max voice stealing, scheduling, or sound.

## 193. Flat performance events collapse Score ties at the source boundary

**Decision:** Use one checked tie-chain resolver for `compile_to_midi` and
`compile_to_note_events`. A validated chain produces one physical event beginning at the first
segment, with exact summed duration and terminal-segment release velocity. Only the matching pitch
is consumed from a chord; intervening point events do not break measured adjacency. Visit every
consumed continuation in Score order so its semantic dynamic updates the Voice state for later
attacks. Stable-sort generic events by start time so simultaneous Part/Voice/note source order is
retained.

**Reason:** Generic `NoteEvent` has no tie flag or identity. The former documentation delegated tie
handling to consumers even though the type supplied no evidence from which a consumer could do so.
Consequently every tied segment appeared as a fresh attack and release, unlike the compiled MIDI
path used by Live. MIDI also skipped semantic dynamic updates on consumed continuations, leaving
later attacks dependent on whether a prior note happened to be tied.

**Consequence:** MIDI, generic NoteEvent, SMF, and Live now share one attack/duration/release
algebra across two- and multi-segment ties. Cross-measure chains, point metadata, and partial
chords are deterministic; orphan ties fail structural preflight. This is source/translation
evidence only. It does not prove a downstream synthesizer renders legato, suppresses envelopes, or
produces audible continuity.

## 194. Flat performance projections preserve articulation shape and expose control residuals

**Decision:** Apply articulation mapping in `compile_to_note_events` rather than treating the
whole mapping as MIDI-only. Unmapped articulations use exact `Beat` duration ratios and the shared
velocity offsets. Custom `VelocityLayer` and `NoteDurationScale` children transform the event's
existing fields in stored `Combined` order. Convert the stored IEEE-754 float duration factor to
the exact binary rational it denotes; reject with `TargetValueUnrepresentable` when that rational
cannot fit the signed 64-bit `Beat` domain. Count the mapping as applied, but emit a
location-bearing diagnostic for each affected note when Keyswitch, CC, or ProgramChange children
have no carrier in the flat event type.

**Reason:** A target-specific control message and note-local physical shape are different
capabilities. `NoteEvent` and Live's documented Clip note dictionary both carry duration and attack
velocity, so omitting those transforms made the generic projection disagree with MIDI/Live before
any keyswitch or controller question arose. Approximating a custom float through a convenient
rational constructor would also silently change a source value at an exact target boundary. Max's
[`midiformat`](https://docs.cycling74.com/reference/midiformat/) and
[`xnoteout`](https://docs.cycling74.com/reference/xnoteout/) establish that Max can format the
relevant MIDI classes, while its [scheduler model](https://docs.cycling74.com/userguide/scheduler/)
and [MIDI-port model](https://docs.cycling74.com/userguide/midi/) show why availability is not proof
of same-tick order, timely delivery, selected port, device receipt, or sound.

**Consequence:** MIDI, generic NoteEvent, SMF, and Live now agree on representable articulation
velocity and duration intent. Exact defaults no longer pass through binary floating point. A tiny
positive custom factor may legitimately compile to MIDI's minimum one tick while failing the exact
Beat target, and that divergence is explicit rather than approximate. Generic control loss is
diagnosed; Live continues to report requested versus zero-written control events. No Max control
deployment, named-host callback, MIDI-port transfer, downstream response, or audible equivalence
is inferred.

## 195. Notation resolves both semantic velocity carriers and reports numeric-only attack intent

**Decision:** Treat `Note.dynamic` and `VelocityValue.written` as coherent semantic carriers at
every compiler boundary: R7 requires equality when both exist, `Note.dynamic` has precedence, and
full MusicXML/LilyPond emit either one as the corresponding dynamic mark. When neither exists,
`VelocityValue.value` in 1–127 is explicit numeric attack intent and zero is the unresolved
sentinel. The full notation compilers emit a location/Part diagnostic for the active numeric-only
form instead of silently dropping it.

**Reason:** MIDI and generic NoteEvent compilation already implemented this precedence, but both
notation compilers inspected only `Note.dynamic`. Two valid Scores with the same semantic marking
therefore exported differently depending on which coherent carrier held it. Numeric-only attack
state also vanished. MusicXML documents sound dynamics as a percentage of its standard forte
velocity 90 ([MusicXML 4.0](https://www.w3.org/2021/06/musicxml40/musicxml-reference/elements/sound/)),
so arbitrary MIDI bytes do not have an exact finite target value; the admitted LilyPond profile
likewise has no exact per-note MIDI-byte construct.

**Consequence:** Semantic dynamic notation is carrier-independent, while exact numeric intent is
never confused with an approximate notation playback hint. MIDI, NoteEvent, and Live continue to
resolve an exact byte; Max can format that byte but still needs scheduler, port, receiving-device,
and sound evidence. Tests that were only constructing notation fixtures now use the true zero
sentinel rather than accidentally authoring velocity 80.

## 196. Notation compilation accounts for every populated document analysis layer

**Decision:** Full MusicXML and LilyPond compilation emits one location-aware residual for each
populated top-level layer outside the admitted notation profile: hierarchical `SectionMap`,
persisted harmonic analysis, orchestration analysis, the governing tone row, stale harmonic
regions, and stale orchestration regions. Event `ChordSymbol` notation is not counted as the global
analysis layer. Rendering-only `dynamic_balance` is not recast as a visual dynamic. Remove the
unimplemented claim that MusicXML exported section boundaries as bookmarks or rehearsal marks.

**Reason:** Both compiler headers promised that unsupported distinctions would produce evidence,
but these complete source layers were never inspected. Similar-looking target constructs do not
establish semantic identity: flat marks cannot retain nested spans, event annotations do not retain
persisted analysis confidence/non-chord-tone state, and an engraving cannot recover authoring
staleness. The formal text also called orchestration metadata inaudible even though MIDI compilation
uses its optional balance field to shape velocity; the layer is analytical, but one field has an
explicit performance policy.

**Consequence:** A successful notation document no longer implies silent preservation of omitted
analytical state. Tests populate all six categories together and pin all six diagnostics. Live's
separate top-level CuePoint projection and nested-section counts remain unchanged, and no notation
or host behavior is inferred from the residual report.

## 197. Compilation reports distinguish drops from all residual evidence

**Decision:** Preserve `CompilationReport::has_drops()` as the narrow predicate over dropped-event
counters and requested/written capability deficits. Add
`has_residuals() = has_drops() || !diagnostics.empty()` as the authoritative target-completeness
predicate. Emit both Booleans in standalone Score and aggregate Project MCP report JSON, and make
both Ableton MCP `complete` calculations call `has_residuals()`.

**Reason:** A target may emit a usable object with unchanged event cardinality while losing a
semantic distinction. SMF's key-signature payload, for example, can retain fifths while reducing a
Dorian mode to its major/minor bit; that produces a diagnostic but correctly increments no dropped
key event. The old report made callers combine `diagnostics.empty()` with `has_drops()` themselves.
That duplicated composition rule in two MCP handlers, omitted it from report JSON, and invited
future consumers to summarize a diagnostic-only degradation as lossless. Redefining `has_drops()`
would make its name and counted meaning false.

**Consequence:** Clean, diagnostic-only, and counted-loss reports are distinct and mechanically
testable. MusicXML/LilyPond analytical and release-velocity residuals summarize as residuals without
pretending an event counter was dropped. Score and Project completeness share one source predicate,
while clients retain the narrower drop fact for telemetry. This is evidence composition only; it
does not raise source-level proof to named Live/Max execution or sonic evidence.

## 198. Shared MCP compilation evidence has one Infrastructure encoder

**Decision:** Move the complete `CompilationReport` JSON projection into one private
`sunny_infrastructure` encoder. Route every standalone Score compilation response and the aggregate
Project Score sub-result through that function. Keep the typed report and its summary predicates in
Core; do not introduce nlohmann JSON into the Core dependency surface.

**Reason:** `score_tools.cpp` and `project_tools.cpp` independently listed every report counter,
summary, diagnostic location, and Part identity. Adding `has_residuals` required two identical
edits and demonstrated that neither list was authoritative. A future field could silently reach
only one response, making identical typed evidence client-dependent. The dependency-safe owner of
serialization is Infrastructure, not Core.

**Consequence:** The report schema has one executable field inventory and one diagnostic encoder.
All four standalone Score report call sites and the aggregate Project call site share it; CMake's
source-ownership gate includes the new implementation in `sunny_infrastructure`. Existing MCP
contract tests exercise both callers. Other repeated deployment encoders remain an explicit
follow-on inventory and are not implied to be unified by this decision.

## 199. Validation and shared Ableton deployment evidence use the same boundary authority

**Decision:** Expand the private Infrastructure encoder from compilation reports to every
byte-for-byte shared Score/Project deployment shape: complete Score tuning, requested/observed
notes, Clip-envelope clears, CuePoints, and scalar-property set/readback evidence. Route Mix scalar
properties through the same projection. Define one complete Core `Diagnostic` JSON contract for
Score, Timbre, Mix, Corpus, and Project: `rule`, `severity`, `message`, and numeric `error_code` are
mandatory; `location` and `part_id` are emitted exactly when populated.

**Reason:** Handler-local deployment encoders allowed standalone and aggregate consumers to drift
while describing the same typed observation. More seriously, the Timbre, Mix, and Corpus handlers
dropped `error_code`, location, and Part provenance from the same Core diagnostic type that Score
and Project exposed completely. That made cross-domain validation require string parsing and could
erase the source coordinate needed to investigate a failed target precondition. The model cannot
be backpropagated against Live/Max evidence if its own public boundary discards provenance first.

**Consequence:** `mcp_detail::encode_*` in the private evidence authority is the executable field
inventory for these shared types. A public MCP contract test creates invalid Score, Timbre, Mix,
and Corpus state and requires the complete diagnostic shape on every surface. Existing deployment
tests pass through the shared tuning/note/envelope/CuePoint/property implementations. The change is
additive for the formerly sparse diagnostics and makes no new claim about named Live/Max execution,
transport delivery, scheduling, signal flow, or sound.

## 200. The Max help path must preserve release velocity before claiming MIDI formatting

**Decision:** Replace the `sunny.events` help patch's two-inlet `noteout` wiring with an explicitly
ordered translation into `xnoteout 1`. Copy the three-atom Sunny list through `trigger`; store
release velocity first; derive the note flag and select attack versus release velocity next; send
pitch last so it triggers the fully prepared `xnoteout`. Do not attach `midiout` or claim port
delivery. Make the metadata gate verify every object and edge in this topology and forbid a plain
`noteout` object from returning.

**Reason:** Cycling '74 documents `xnoteout`'s list as pitch, selected velocity, note-on/off flag,
and channel; on an off event that selected velocity is the release velocity. `sunny.events` instead
emits pitch, attack-or-zero velocity, and release velocity. The old help patch fed only the first
two atoms to `noteout`, silently discarding the exact source field that the wrapper and formal model
had just preserved. A help patch is executable downstream guidance, so this was an external
alignment failure even though native queue tests remained green. Explicit `trigger` ordering also
avoids depending on implicit patch-cord execution order before the hot pitch inlet fires.

**Consequence:** The maintained example formats note-on as `(pitch, attack, 1, channel 1)` and
note-off as `(pitch, release, 0, channel 1)`. Static metadata validation rejects a missing selector,
velocity branch, flag, trigger order, or hot-inlet edge. This proves the authored patch topology,
not Max execution, a selected MIDI port, byte delivery, receiving-device response, scheduler timing,
or sound; those remain named-host obligations.

## 201. Every repeated Ableton deployment observation has one MCP projection

**Decision:** Expand the private Infrastructure evidence authority to the remaining semantically
identical deployment types exposed by standalone Timbre/Mix tools and aggregate Project results:
Timbre parameter deployments, inserted-device observations, Mix Return Track intent, optional Main
Track intent, Mix parameter deployments, and per-effect parameter coverage. Remove every
handler-local projection and route all callers through the typed `mcp_detail::encode_*` functions.

**Reason:** The same `AbletonDeviceInsertionDeployment` had three independently maintained JSON
implementations, while the Timbre parameter and Mix Return/Main/parameter/coverage types each had
two. This allowed a newly observed Live fact, nullability rule, or field rename to reach one public
surface but not another even though the C++ evidence was identical. Aggregate containment is not a
semantic reason for a different boundary schema, and such drift would make adversarial comparison
against Live/Max depend on which MCP tool the client called.

**Consequence:** The private evidence encoder is now the executable field inventory for every
shared deployment shape. Public MCP tests pin the exact field cardinality of device, Timbre/Mix
parameter, Return/Main, and coverage objects, then compile the same stored Timbre and Mix models
through isolated standalone handlers and the Project handler and require identical evidence. This
proves internal schema identity only; recording-transport nulls remain nulls, and named Live/Max
execution, persistence, scheduling, signal flow, port delivery, and sound still require external
evidence.

## 202. Max status must distinguish admission from scheduler retention

**Decision:** Add a lock-free current-pending gauge to every bounded Max control stream. For
`ItmEventAdapter`, additionally expose the scheduler-owned retained-event cardinality beside the
existing reservation gauge. Make `sunny.events status` print `pending_commands`, `reserved_events`,
and `retained_events` as different facts; never label `events_reserved` as retained. Pin these
labels in the SDK metadata gate.

**Reason:** Event admission reserves capacity before the scheduler consumer samples the global ITM
transport or assigns any permanent slot. The old console output printed `events_reserved` under
`retained`, so one freshly published command falsely appeared to be retained by the scheduler.
Historical enqueued/applied/rejected counters also could not directly expose the current queue
phase, especially for an ordered zero-event `clear` command. That collapsed producer acceptance,
scheduler application, and retained target work—the exact phases external validation must keep
separate.

**Consequence:** Native tests now pin publication, application, callback, clear, and saturation
transitions: publication increases pending and reserved while retained remains zero; application
drains pending and creates retained events; firing/clear reduce retained and reservations. The four
MSP adapters likewise report pending controls before a valid vector edge and zero after draining.
Producer/consumer stress also requires the pending gauge to remain within the 64-cell bound; the
consumer decrements it before releasing a cell for producer reuse. Status remains a
non-transactional multi-atomic snapshot and source-side evidence only. It does not prove Max
scheduled a permanent event, invoked a callback, met a timing deadline, delivered MIDI, or produced
sound.

## 203. Under-specified target dictionaries remain observable opaque evidence

**Decision:** Advance Ableton target-snapshot schema 28 to 29 and retain every documented Live
12.1+ `TuningSystem` property. Keep `name` and positive finite `pseudo_octave_in_cents` typed.
Retain `lowest_note`, `highest_note`, and `reference_pitch` as exact opaque finite-JSON objects.
For `note_tunings`, enforce only the documented single-member dictionary containing a finite
numeric array. Export all four raw payloads and `tuning_dictionary_payloads_observed`, while keeping
`tuning_definition_fully_observed`, mutation, Track/device support, and audible-pitch verdicts
false.

**Reason:** Cycling '74 documents that these properties are get/set/observe dictionaries but does
not publish their member names, categories, ordering, or ranges. That is insufficient for a safe
Sunny-to-Live tuning mutation or semantic comparison with `ScoreTuning`, but it does not justify
discarding observable target state. Dropping the dictionaries allowed a tuning change to evade
guarded-plan equality; guessing their members would convert missing documentation into a false
contract.

**Consequence:** The Python peer strictly normalises only JSON-safe finite data and rejects
non-string keys or private object coercion. The native parser independently closes the six-member
tuning object, recursively rejects non-finite opaque values, and verifies the one documented
`note_tunings` shape. Exact dictionary changes now invalidate a stale target and propagate through
aggregate/MCP postcondition evidence. Interior object members remain deliberately open and opaque;
no setter is added, and named-Live private-Python representation, atomicity, source mapping,
per-track bypass, instrument/MPE behavior, and sound remain external evidence obligations.

## 204. Collection cardinality is a scalar contract, not serialized object evidence

**Decision:** Advance Ableton bridge protocol 36 to 37. Remove raw Song `scenes`, `tracks`, and
`return_tracks` gets. Add exact no-argument `sunny_get_scene_count` and
`sunny_get_return_track_count` calls, require a list/tuple at the hosted adapter boundary, and
return a bounded non-negative signed-LOM integer. Make the Python response serializer recursively
admit only exact JSON-safe wire values and fail on unsupported private objects instead of calling
`str()`. Require every documented profile, snapshot, cue, Device, and DeviceParameter observation
to arrive in its exact Python scalar category rather than constructor-coercing it.

**Reason:** Score and Mix required only cardinality, but the former bridge serialized every object
in those collections to an unpublished private string representation and the native peer then
discarded the strings. That fabricated wire values unrelated to any documented LOM property and
made correctness depend on host `repr`/`str` behavior. The unused raw Tracks get widened the same
reflection surface without a consumer. Cardinality itself is bounded, sufficient, and independently
validatable.

**Consequence:** Python and C++ independently close the two count operations and reject arguments,
wrong scalar categories, negative counts, and malformed collections. Recursive serialization also
rejects non-finite numbers, non-string object keys, integers outside the wire domain, and nested
private objects. Adversarial fake-host tests also reject stringified Application versions, numeric
category confusion, and private objects in target names. This proves the peer contract and fake-host behavior only; whether a named Live
build exposes these collections as list/tuple and whether their cardinality remains coherent across
subsequent mutations still require named-host validation.

## 205. Timbre parameter paths and automation have one admission authority

**Decision:** Define one canonical parameter-path grammar and require exact, whole-path resolution
to a present continuous `float` leaf in the active Timbre source/effect structure. Route get/set,
modulation, automation, preset, rendering-map, and whole-document validation through that resolver.
Define one complete automation predicate—exact target, defined interpolation, nonempty lane, finite
values, valid ScoreTime, and strictly increasing times—and use it at mutation and load boundaries.

**Reason:** The former tokenizer discarded empty components, accepted partially parsed and
non-canonical indices, and several resolvers ignored trailing components. T7 validation only
matched broad prefixes. Consequently a string could be admitted as a modulation/automation target
while no exact parameter existed, and malformed automation inserted directly or decoded from JSON
bypassed the workflow's two partial checks. The formal examples also inserted variant and
`parameters` segments that do not exist in the runtime document.

**Consequence:** A valid prefix plus junk, absent optional, wrong active variant, out-of-range
index, or categorical/integral leaf is now unresolved everywhere. T11 blocks malformed automation
documents from deserialization, and failed mutations preserve the original lane collection. The
specification now names the actual variant-relative addresses. This establishes source-model
address identity; it does not imply that Ableton's public LOM can author automation envelopes, map
an arbitrary Timbre leaf automatically, or reproduce the requested sound.

## 206. Every declared Timbre modulation source must exist and be authorable

**Decision:** Advance Timbre schema 2 to 3 and add the missing owned modulation-envelope
collection beside LFOs, step sequencers, and macros. Define the discriminator-specific payload and
reference rules for primary and via sources, including MIDI CC bounds and cross-Part
AudioFollower identity. Make T7 Error-level and share its complete routing predicate with mutation.
Validate all owned generator/macro definitions as T12, expose checked C++ append workflows, and
make all three indexed source families authorable through MCP.

**Reason:** `ModulationSourceType::Envelope` existed without any collection for its index to
address. The workflow silently accepted it, all unindexed enum values, every via source, CC values
above 127, zero/self AudioFollowers, and NaN depth. Deserialization therefore admitted routings
that no internal generator could drive. The agent interface could create macros only and omitted
CC/AudioFollower/via payloads, so even valid portions of the declared algebra were not authorable.

**Consequence:** Versions 1 and 2 migrate to no owned envelopes rather than a fabricated one.
Every indexed source now resolves, every payload is canonical, AudioFollower references close at
the Project boundary, and malformed direct/JSON state blocks compilation. MCP can create validated
LFOs, envelopes, and step sequencers and can express the complete routing payload. This proves
model and interface closure only; Ableton materialisation of those generators, modulation rate,
voice scope, sample timing, signal values, and audible effect remain separate target evidence.

## 207. DeviceParameter domains are conditional target evidence, not inferred enums

**Decision:** Advance Ableton bridge protocol 37 to 38 and target-snapshot schema 29 to 30. Retain
`default_value` only for a non-quantized DeviceParameter and `value_items` only for a quantized
DeviceParameter across the Remote Script, native evidence parser, mixer snapshots, Timbre/Mix
deployments, aggregate final observations, and MCP projections. Keep labels opaque.

**Reason:** The public DeviceParameter contract exposes both properties conditionally, but Sunny
discarded them while architecture text said localized labels were observed. `is_quantized` alone
distinguishes broad control families but does not retain a continuous reset point or the target's
enumerated presentation domain. Conversely, mapping label order or text to numeric/semantic enum
identity would invent guarantees absent from the reference.

**Consequence:** Continuous evidence requires a finite floating default inside reported min/max and
null labels. Quantized evidence requires a null default and an exact, possibly empty string vector.
Missing, simultaneous, category-confused, non-string, non-finite, or out-of-range facts fail closed
in both host and native tests. Exact labels strengthen stale-plan equality and auditing but do not
establish localization stability, uniqueness, numeric correspondence, automation durability,
signal behavior, or sound. The fake host contract is tested; the private Control Surface runtime's
list/tuple representation and named Live behavior still require retained validation evidence.

## 208. A fired permanent Max event must be stopped before fixed-slot reuse

**Decision:** Pass the Max wrapper's host-stop callback into successful ITM dispatch. After complete
output storage is proven, stop every permanent time object in the equal-source-tick group before
copying output, removing events, or releasing the corresponding fixed slots. Keep insufficient
output storage fully transactional.

**Reason:** `TIME_FLAGS_PERMANENT` is a host scheduling property, while removing an event from
Sunny's queue changes only adapter state. Equal-tick grouping can consume several slots from the
first callback. Leaving sibling time objects scheduled while immediately making those slots
reusable created the same obsolete-callback ambiguity already identified for clear/reassign, and
also made the documented one-shot claim depend on no-op callbacks rather than cancellation.

**Consequence:** The wrapper binds dispatch to `time_stop` for every fired group member. Native
tests require zero stops on capacity failure and exact sibling stops on success; the pinned-header
metadata gate rejects a wrapper that drops the callback. Sunny can now prove bounded source state,
ordering, and SDK-call shape. It still cannot prove a named Max build's cancellation of an already
queued callback, rapid reuse behavior, transport loop/seek interaction, timing, MIDI delivery, or
sound without the external validation matrix.

## 209. A rejected Max DSP rebuild disables stale processing but not callback registration

**Decision:** Treat each `dsp64` invocation as the complete current-chain setup attempt. On
rejection, clear adapter processing/configured state while retaining processor recurrence and
queued controls. In every signal wrapper, retain the current callback maximum, mark that rebuild
unadmitted, report the setup error, and still call `dsp_add64`. Perform may enter the processor only
for an admitted current rebuild; otherwise it uses one shared bounded zero-fill validator.

**Reason:** Cycling '74 defines `dsp64` as DSP-chain construction and `dsp_add64` as the operation
that installs a perform routine. The prior wrappers returned before that operation on setup
failure, contradicting Sunny's unconditional-registration policy and leaving no defined output.
The adapters simultaneously retained their last-good context, so merely moving registration past
the error could process the new host chain using a stale sample rate or vector maximum.

**Consequence:** A rejected setup never drains controls, advances recurrence, or reuses the former
context. A later admitted setup resumes the retained source state and queued commands. The fallback
validates the current maximum, signed frame count, exact traditional-generator topology, and
non-empty pointers before zeroing, and refuses contradictory host storage without dereference.
Native tests cover rejection/recovery and silence atomicity; the metadata gate requires an
unconditional single `dsp_add64`, and the pinned SDK headers compile every wrapper. Actual chain
construction, callback/buffer provenance, disconnected processing, deadlines, and signal output in
named Max/Live builds remain external evidence.

## 210. ITM host-effect evidence requires the corresponding action

**Decision:** Make schedule, cancel, and stop actions transactional preconditions of their
corresponding `ItmEventAdapter` mutations. Reject an event command without a scheduling action,
reject a nonempty clear without a cancel action, and reject active-group firing without a stop
action. Remove the default action arguments from `fire` so every caller states the boundary
explicitly.

**Reason:** The adapter previously inserted events and incremented `events_scheduled` even when
`schedule_slot` was null, erased retained work and incremented `events_cancelled` without
`cancel_slot`, and could release fired permanent slots without `stop_slot`. Those were local queue
transitions mislabeled as host-effect evidence. The installed-package consumer institutionalised
the first false path by applying an event with three null action arguments.

**Consequence:** Missing actions now preserve slots, queue, reservations, and output and never
increment the affected counter. An empty clear and an inactive obsolete callback remain no-ops
because they have no host object to act upon. The package consumer supplies counting action
callbacks, adversarial tests prove rejection and later recovery, and metadata validation pins the
guards and concrete wrapper binding. Because Max's `time_setvalue` and `time_stop` APIs return no
acceptance result, the counters prove action invocation only; actual scheduling, cancellation,
callback delivery, downstream MIDI, and sound remain named-host evidence.

## 211. Session topology does not stand in for Arrangement Clip absence

**Decision:** Advance Ableton bridge protocol 38 to 39 and target-snapshot schema 30 to 31. On
Live 11+, retain the exact non-negative cardinality of each normal Track's separate
`arrangement_clips` child list; require explicit null on earlier modeled profiles. A generated Part
Track requests cardinality zero and verifies only when that topology was observed and is empty.

**Reason:** Sunny already retained every Session ClipSlot, Track launch state, and
Back-to-Arrangement state, but none enumerated Arrangement content. Live documents
`arrangement_clips` separately, and `playing_slot_index = -1` means Arrangement or no Session Clip.
Consequently, a Track with independent playable Arrangement Clips could satisfy the prior
structural gate. Zero cardinality is the complete tractable fact for the generated-track absence
proposition; serializing private Clip objects or inventing content identity is unnecessary.

**Consequence:** The Python peer accepts only a list/tuple at this private Control Surface boundary,
and the native peer independently requires an exact bounded integer on Live 11+ or null before it.
Generated-track evidence exports requested/observed counts plus distinct topology-observed and
content-absent verdicts. A nonzero count is retained target divergence that warns and makes the
projection incomplete without deleting user content; a Live-10 profile cannot pass this gate.
Named-Live collection representation, sequential-read atomicity, future changes, playback,
routing, signal, and sound remain external evidence obligations.

## 212. Main-lane emptiness does not exclude an auditioned Take Lane

**Decision:** Advance Ableton bridge protocol 39 to 40 and target-snapshot schema 31 to 32. On
Live 11+, retain the exact non-negative cardinality of every normal Track's `take_lanes` child;
require explicit null on the earlier modeled profile. A generated Part Track requests zero Take
Lanes and verifies only when the topology is observed and empty.

**Reason:** Live exposes Take Lanes separately from the Track's main `arrangement_clips`, and each
TakeLane owns another Arrangement Clip list. Ableton documents that the main lane is audible by
default but an enabled Audition Mode makes one Take Lane on that Track audible. The public LOM
does not expose that audition state. Consequently, zero main-lane Arrangement Clips did not exclude
an alternate playback source. Because the requested generated topology is empty, exact lane
cardinality is sufficient without serializing private TakeLane or Clip objects.

**Consequence:** The Python peer accepts only a list/tuple for the version-available collection,
and the native peer independently requires an exact bounded integer or the version-required null.
Generated-track evidence exports requested/observed counts plus distinct topology-observed and
lanes-absent verdicts. A nonzero count is retained divergence that warns and makes the deployment
incomplete without deleting user state; a pre-Live-11 profile cannot pass the gate. Zero lanes
eliminates the unobservable audition branch at that sequential read, but named-Live representation,
atomicity, future changes, playback, routing, signal, and sound remain external obligations.

## 213. Canonical ClipSlot addressing does not replace observable Clip identity

**Decision:** Advance Ableton bridge protocol 40 to 41 and target-snapshot schema 32 to 33. Every
occupied ClipSlot record now retains exact public `is_audio_clip`, `is_midi_clip`,
`is_arrangement_clip`, and finite `end_time`. The Live-11+ branch additionally retains
`is_session_clip` and `is_take_lane_clip`; the earlier modeled profile requires explicit null for
those two properties. A generated Clip verifies only as audio false, MIDI true, Arrangement false,
and, when available, Session true and Take Lane false. Its independently observed `end_time` must
equal the requested End Marker through `playback_end_verified`.

**Reason:** The canonical `tracks/N/clip_slots/M/clip` path establishes how Sunny navigated, not a
returned host fact. The current public Clip LOM exposes media and location predicates separately,
states that a Clip is either Arrangement or Session, identifies Take Lane Clips as Arrangement
Clips, and defines an unlooped Session Clip's `end_time` as its End Marker. Sunny previously read
only `is_midi_clip` and marker/length values. A malformed or misclassified target object could
therefore satisfy the evidence object without proving the public Session identity or the API's
derived playback-end relation.

**Consequence:** The Python peer rejects category-crossing or incoherent media/location values
before serialization; the independent native peer requires XOR audio/MIDI identity, non-
Arrangement ClipSlot content, the version-correct Session/Take-Lane branch, and exact non-looped
`end_time`/End-Marker equality. Aggregate and MCP evidence retain every request, observation, and
the separate identity/playback-end verdicts. This proves the documented point-in-time object role
and derived endpoint, not one-shot execution, Follow Actions, dormant loop-brace state, future
stability, named-host Python representation, device response, routing, signal, or sound.

## 214. Output-routing verification is conditional on an explicit target binding

**Decision:** Advance Ableton bridge protocol 41 to 42 while retaining target-snapshot schema 33.
Accept an output-route binding only for a materialisable Channel/Aux-to-Master edge. The binding
contains exact target-advertised type/channel dictionaries plus non-empty mapping provenance. Apply
it as two separate journalled mutations: set the type after current membership validation, then
re-enumerate and set the channel after validating selected-type stability and current membership.
Group destinations and unbound edges remain residuals.

**Reason:** Live defines routing choices as Track-specific dictionaries selected from advertised
collections, and a type change can replace the channel set. It publishes no portable symbol for
Main or a particular Group. Inferring meaning from display text or opaque identifiers would invent
a target ontology; using one opaque setter would conceal the intermediate mutation and a failed
second stage. Snapshot membership alone proves that Live advertises a choice, not what that choice
means in Sunny's graph.

**Consequence:** Python and C++ independently close every argument/evidence shape and require exact
echoes, membership, type stability, and readback. Plans canonically guard both dictionary pairs and
provenance; compilation cross-checks the retained binding against per-route evidence. Recording
transports report `recorded_only` and cannot verify, while complete real evidence removes exactly
that route residual and final postconditions require the requested pair. Partial type-only mutation
is preserved in the journal. Verification is explicitly conditional on caller provenance, not
proof of named-Live private representation/behavior, atomicity, future stability, Main hardware
routing, signal, or sound.

## 215. Aggregate hold peaks do not replace channel-resolved momentary evidence

**Decision:** Advance Ableton bridge protocol 42 to 43 and target-snapshot schema 33 to 34. For
every normal Track, retain exact finite floating `input_meter_left`, `input_meter_right`,
`output_meter_left`, and `output_meter_right` values in `[0,1]` when `has_audio_output` is true and
require four explicit nulls otherwise. Generated Part Tracks request exact zero for all four and
verify only when both the existing hold-peak and new stereo-momentary conjunctions hold.

**Reason:** Live's aggregate `input_meter_level` and `output_meter_level` are one-second hold peaks
that collapse channel detail. The current Track LOM separately exposes smoothed momentary left and
right input/output peaks on audio-output Tracks. Those values can reveal channel activity not
expressed by the earlier pair at the later sequential reads, but the documentation warns that
observing them increases Live GUI load. A bounded single read of each property is therefore the
tractable strengthening; polling, Return/Main category assumptions, and continuous-signal claims
would exceed the public contract.

**Consequence:** The Python peer validates exact float category, finiteness, range, and the
audio-output conditional branch; the independent native parser closes the exact Track shape and
same union. Project and MCP evidence retain every requested/observed channel, separate input/output
stereo verdicts, their conjunction, and the final hold-plus-momentary verdict used by the Part gate.
A valid nonzero channel remains observed divergence and makes that gate incomplete. Both
continuous-silence flags stay false: the six meter reads are sequential point observations, not an
atomic trace, route proof, hardware test, rendered-audio result, acoustic measurement, future-state
guarantee, or named-Live validation.

## 216. Object allocation does not establish an MSP signal outlet

**Decision:** Require each of the four Max signal-generator constructors to succeed only after its
single `outlet_new(..., "signal")` call returns a non-null outlet pointer. On failure, report the
construction error, destroy the partial object, and return null.

**Reason:** The Max SDK specifies audio-output construction as a separate new-instance operation
and returns a pointer from `outlet_new`; `dsp_setup(..., 0)` creates the zero-input side only.
Discarding the outlet result allowed source control flow to expose a nominally constructed object
without establishing the documented one-output topology. Later callback validation cannot repair a
missing construction-time outlet.

**Consequence:** `sunny.lfo~`, `sunny.adsr~`, `sunny.hold~`, and `sunny.clock~` now share the same
fail-closed constructor boundary. The metadata validator requires exactly one signal-outlet call,
the returned-pointer guard, and the error path in every wrapper, while pinned official headers
close the declaration and type shape. Source inspection cannot prove the object loads, that Max
materialises or connects the outlet, that host allocation failure follows the assumed cleanup
lifecycle, or that any callback buffer, signal, or sound exists in a named host.

## 217. A global ITM pointer still requires explicit reference ownership

**Decision:** `sunny.events` acquires the unnamed global ITM once during construction, rejects a
null result, calls `itm_reference` before the instance can succeed, samples only that retained
pointer, and calls `itm_dereference` after its command clock and permanent time slots are destroyed.

**Reason:** The Max SDK states that code using an ITM object must increment its reference count and
must decrement it when finished. Calling `itm_getglobal` inside each command-clock callback and
immediately reading the raw pointer did not establish that documented lifetime. The global object's
identity and the short duration of each read are not published exceptions to the ownership rule.

**Consequence:** The wrapper now has one explicit construction-to-teardown ITM lifetime, and partial
construction paths remain safe because the member is initialized null before any fallible work.
The metadata validator requires acquisition/null rejection, retained-pointer sampling, and exactly
one paired reference/dereference; pinned official headers compile the complete wrapper. These gates
do not prove Max's reference-count implementation, the actual global transport identity, scheduler
ordering, discontinuity behavior, event delivery, or sound in a named host.

## 218. A documented Max class must register atomically at the source boundary

**Decision:** Treat null `class_new`, every nonzero `class_addmethod` result, and every nonzero
public `class_register` result as load-time failure. Free any still-unregistered class, return from
`ext_main`, and assign a Sunny public class global only after successful registration.
`sunny.events` must create both class definitions first and successfully register its hidden
time-slot dependency before attempting to expose the public class.

**Reason:** Max returns explicit error values for method and class registration. The former wrappers
ignored all of them, so source control flow could publish a global after Max rejected a method or
the class itself. That contradicts reference/help metadata and can turn a documented selector set
into a partially registered runtime surface. The event wrapper also passed unchecked class pointers
into time-attribute and method setup.

**Consequence:** Every wrapper accumulates all method results, skips registration after any method
failure, closes unregistered class ownership with `class_free`, and publishes only a successful
public registration. A failed public `sunny.events` registration can leave its already registered
non-box dependency resident, but no constructible partial public object. The metadata gate requires
one guarded registration for every documented method, exact public/hidden class cardinality,
cleanup, and global-assignment ordering; pinned headers close the declared API. Binary discovery,
actual host return values, selector dispatch, error presentation, unload behavior, and sound remain
named-host evidence.

## 219. Max host evidence is a closed content-addressed record, not a note

**Decision:** Introduce C++-authoritative `MaxValidationRecord` schema 1 for one named standalone
Max or Max for Live run. Bind it to current Sunny/package versions, source revision, the exact
distributable-package archive hash, host/audio environment, the five binary artefacts, and a
closed ordered 19-check set.
Derive completeness instead of accepting an operator-written success flag.

**Reason:** The prior conformance model enumerated host obligations but provided no executable
envelope for their results. Screenshots, console snippets, or “passed” prose do not bind evidence to
the tested revision/binaries/configuration, distinguish failure from omission or inapplicability,
or prevent the checklist from silently losing a required test. Making Python the authority would
also invert Sunny's intended C++-first infrastructure.

**Consequence:** Passed and failed checks require a printable relative evidence path and nonzero
lowercase SHA-256; not-run and host-inapplicable checks forbid evidence. Only the two transport-
identity checks can be inapplicable, each in exactly its opposite host kind. The parser closes
field sets, ordering, enum/category/domain/platform conditions, hashes, relative paths, optional
Live versions, evidence pairs, and the derived verdict. Native tests pin canonical standalone and
Max for Live records plus adversarial tampering; the installed Python binding delegates validation
to the same C++ parser, and the Max package carries a canonical incomplete example. This makes
host execution auditable but does not authenticate evidence or satisfy a multi-platform/settings
release matrix by itself.

## 220. File digests are derived from a separate closed host observation

**Decision:** Represent unhashed named-host facts with strict `MaxValidationObservation` schema 1,
then materialize the final `MaxValidationRecord` only by reading the declared package archive,
ordered platform binaries, and evidence files through the C++ `sunny-max-evidence` workflow.
Re-verification must recompute the same byte digests.

**Reason:** A strict 64-hex field constrains spelling, not provenance. Asking an operator or a
Python helper to paste hashes into the final record leaves an untested transcription boundary and
creates another authority beside the native schema. Conversely, asking a Max audio/scheduler
wrapper to hash files would introduce unbounded I/O and JSON policy into the real-time boundary.

**Consequence:** The observation contract contains every operator/harness fact but no derived hash
or completeness claim. Materialization confines relative paths to canonical package/evidence roots,
rejects absent, escaping, symlink-leaf, and non-regular inputs, hashes regular files incrementally,
checks the resulting record with the strict native parser, and emits canonical JSON. Verification
repeats materialization and identifies the first mismatched archive, binary, or evidence subject.
Tests cover a known SHA-256 vector, malformed observations, missing roots/files, directories,
executable round trip, and archive tampering. This removes manual digest authority but does not
authenticate host statements, make multi-file capture atomic, or execute any Max/Live check.

## 221. Supported-platform Max staging is an exact package contract

**Decision:** Replace platform-specific CI spot checks with one shared staged-package validator for
both macOS and Windows. Require the exact five external entries and their nonempty regular binaries,
the exact help/reference sets, both validation handoffs, the harness and release-matrix manifests,
the SQLite extractor, package manifest, and readme; copied authored files must remain byte-identical
to source.

**Reason:** The former workflow never checked the `sunny.events` binary and sampled different
documentation files on each platform. It could therefore upload a structurally incomplete package
while reporting success. On macOS it checked only `.mxo` directory existence, not the bundle's
actual executable path used by `MaxValidationRecord`.

**Consequence:** One Python CI adapter now checks the same closed topology on both supported package
layouts, refuses symlink/empty binaries, rejects missing or unexpected entries, and compares every
authored staged byte with its source. Synthetic tests cover both platform layouts, missing events,
metadata drift, and an extra external. Python is not the product/runtime authority here: this is a
portable artifact-layout gate around the Max SDK build. A workflow definition is not a passing
workflow run, and neither is named-host load/timing evidence.

## 222. The Max build owns the exact distributable archive bytes

**Decision:** Add a `sunny_max_archive` target after the five platform externals. It must package
the complete staged tree below one `Sunny/` root, emit a version/platform/architecture ZIP, and
write the ZIP's SHA-256 sidecar. CI uploads those files rather than relying on an artifact service to
define the distributable archive.

**Reason:** `MaxValidationRecord` binds exact package-archive bytes. A checked stage followed by an
operator-created or service-created archive leaves an unclosed transformation: members can be
omitted, added, renamed, duplicated, traversing, or changed. A download wrapper created by CI
artifact storage is not the same stable inner file that a host record can hash.

**Consequence:** The shared platform validator opens the produced ZIP, rejects corrupt CRCs,
duplicates, unsafe paths and symlink entries, requires its complete regular-file set to equal the
stage below `Sunny/`, compares every entry byte, and independently verifies the SHA-256 sidecar.
Synthetic tests invoke the actual CMake archive script and perturb staged bytes and the sidecar.
This establishes deterministic ownership of which bytes are the distributable input; it does not
promise bit-for-bit reproducibility between builds, signing/notarization, or successful Max loading.

## 223. A shipped example can never derive a complete Max verdict

**Decision:** Add an explicit non-example-provenance conjunct to `MaxValidationRecord::complete`.
Reserve the package's `example_` harness names, `replace-with-` environment values, and deliberately
uniform hexadecimal revision/digest sentinels as teaching values that cannot certify a run.

**Reason:** The examples accurately called themselves “not evidence,” but the original algebra
looked only at artifact and check outcomes. An operator could flip those facts while retaining every
visible placeholder and obtain a structurally complete record, contradicting the documentation.

**Consequence:** Examples remain strict-parser-valid and useful as handoffs, but derived completeness
stays false until their provenance is replaced. A forged serialized true value fails parsing. This
is an accidental-misuse guard, not authentication: arbitrary plausible-looking false testimony is
still beyond a local schema and requires trusted harness/provenance controls.

## 224. Max host status must cross a closed machine-readable outlet

**Decision:** Extend the shared audio-adapter status with the current configured sample rate and
maximum frame count, exact signed saturating process-call count, and last signed frame count.
Preserve every signal or event product at outlet 0 and create a right message outlet that emits one
closed eight-atom `host_status` or `event_status` tuple when `status` is requested.

**Reason:** Console prose and a sampled signal do not expose a stable machine contract relating
`dsp64` configuration to actual perform observation. A test could assert a finite default while the
intended callback never ran, and parsing console presentation would make evidence depend on text.
The event wrapper similarly needed a structured distinction between command observation, local
scheduling/firing/cancellation counters, and callback health.

**Consequence:** The configuration triple is sampled coherently with a generation counter, while
the remaining lock-free counters retain their documented non-transactional semantics. Native tests
cover setup/process transitions; pinned SDK headers and metadata checks cover outlet creation order,
allocation failure, tuple spelling, and output roles. These facts enable a host harness to establish
a bounded source observation. They do not establish Max buffer provenance, connection state,
scheduler timing, downstream delivery, audio deadlines, or sound.

## 225. A pinned max-test smoke is a partial observation adapter

**Decision:** Stage a self-starting `.maxtest.maxpat` smoke and four assertion helpers, and close its
contract with `max-test-harness.json`. Pin the Cycling '74 `max-test` repository revision, package and
minimum-Max versions, SQLite tables/columns/outcomes, patcher topology, assertion names, public
selector dispatch, and the mapping into Sunny's ordered 19-check host record.

**Reason:** An informal instruction to “run this in Max” permits runner, result-schema, assertion,
and checklist drift. Conversely, calling any green smoke a complete host record would erase the
difference between lightweight discovery/DSP/control checks and lifecycle, ITM, timing, MIDI,
render, transport, and Max for Live obligations.

**Consequence:** Static validation proves that the staged smoke self-starts, terminates through the
official test object, dispatches every documented selector, observes all five objects, and can map
14 exact checks. Four checks remain explicitly `not_automated` and the standalone-only run
marks `live_transport_discontinuities` `not_applicable`. The package validator closes these authored
bytes. No run is claimed until the pinned harness actually produces and retains its database, and
record completeness still depends on the independent C++ observation/materialization model and all
residual host evidence.

## 226. Official max-test rows are transported by Python but interpreted by C++

**Decision:** Stage one standard-library Python extractor for the official SQLite database, but
limit it to a closed raw-row projection for an explicit test ID. Add native `MaxTestRunResult`
schema 1 and `sunny-max-evidence apply-max-test` as the only authority that may validate the exact
manifest/database digests, assertion set, mapping conjunctions, and resulting host observation.

**Reason:** Manual SQL and handwritten pass summaries sever the evidence record from the database
bytes it later hashes. Selecting the latest row is ambiguous, an unfinished upstream row initially
has a zero finish value, WAL state can place committed bytes outside the main file, and a Python
mapper would invert Sunny's C++-first dependency direction. Direct SQLite linkage would add a new
product dependency solely for transport even though Python's standard library already provides a
portable read-only adapter.

**Consequence:** Extraction rejects non-regular or active databases, missing official columns,
failed integrity checks, absent/unfinished IDs, unknown outcomes, and byte changes across the read.
The normalized result retains database digest, exact test row, and ordered assertion rows only.
Native application re-hashes the manifest and database, requires the pinned revision and exact 64
unique names, maps 14 checks and five artifact facts conservatively, and leaves all residual
checks untouched. The materializer caches repeated evidence-path digests. This closes accidental
schema/query/mapping drift without claiming to authenticate a malicious extractor, operator, Max
process, or machine.

## 227. Max release coverage is a closed native aggregate, not a prose checklist

**Decision:** Define `max-release-matrix.json` as the compiled-digest schema-1 authority for the six
supported target/host cells: macOS x86_64, macOS arm64, and Windows x86_64, each in standalone Max
and Max for Live. Add native `MaxReleaseMatrix` parsing/materialization and
`sunny-max-evidence assemble-matrix`/`verify-matrix`. Embed each strict record while also binding its
exact file SHA-256. Require all records to be complete, identity-correct, and revision-equal; require
both host kinds for a native target to identify the same archive and five binaries.

**Reason:** “Validate every platform” is not an executable set, and one complete record can be
silently overgeneralised. A directory of individually plausible records can still omit/duplicate a
cell or combine unrelated builds. Embedding records makes the derived aggregate verdict
self-contained, while record-file hashes preserve the exact assembly inputs. Scheduler and audio
settings must remain record conditions: multiplying every environment field into an unstated test
matrix would either be unbounded or invite fictional coverage.

**Consequence:** Missing, extra, reordered, incomplete, misidentified, revision-divergent, or
per-target package-divergent inputs cannot form a release matrix. The staged manifest is checked
against package platform metadata and the compiled native digest. Matrix verification re-hashes the
manifest and record files only; each record still needs its own underlying archive/package/evidence
verification, and the aggregate does not authenticate a host, operator, or unrecorded setting. No
six-cell record set is committed, so no release execution claim is added.

## 228. Relative Max scheduling is a checked projection, not a “schedule now” promise

**Decision:** Add `tick_after_itm(host, ppq, delay)` as the sole native relative-target algebra.
For nonnegative delay it selects `floor(current_host_tick * ppq / host_resolution) + 1 + delay`,
checks overflow and adjacent-tick injectivity, and requires the mapped target to be strictly future.
Expose this through `sunny.events event_after` and `note_after`, while retaining the independent
strict-future preflight when the command reaches the scheduler consumer. Add `transport_status` as
a four-field sequential observation of current tick, resolution, running state, and SDK transport
name from the lifetime-owned global ITM. Private/internal ITM tempo and sample-rate getters are
explicitly excluded.

**Reason:** Absolute demonstration ticks cannot drive a repeatable host test once a transport has
advanced, while treating a relative publication as a host reservation would conceal queue and
scheduler latency. The same host harness also needs observable inputs to distinguish target
derivation failures from transport configuration, without granting Sunny authority to mutate the
host transport.

**Consequence:** A delay of zero has an exact cross-domain meaning—the next representable Sunny
tick, never the current tick—and negative, overflowing, collapsed, invalid-host, and stale targets
fail closed. Help/reference metadata and the smoke selector gate cover the new public surface.
Transport status is suitable as bounded evidence input but is non-transactional and does not prove
Live identity, event acceptance, priority, delivery, sample accuracy, or sound. Those remain
conditions of the named-host validation record and release matrix.

## 229. Standalone ITM smoke ordering follows callbacks, not elapsed-time guesses

**Decision:** Extend the pinned `sunny-runtime-smoke.maxtest.maxpat` run with one exact-list helper
and a callback-driven six-event trace. The harness stops and resets Max's unnamed global
`transport`, sets tempo 120, publishes relative events, starts the transport, and advances each
assertion from `sunny.events` output. Equal-tick note-off precedes note-on. After the fifth callback,
the harness schedules one event, clears it, and schedules a replacement; only the replacement may
be the sixth event. Final status is sampled while the transport is running, then the harness stops
it. A 1500 ms delay exists solely to fail, stop, and terminate a stalled run.

**Reason:** Arbitrary wait intervals make an asynchronous host test depend on machine speed and can
mistake elapsed time for scheduler causality. Conversely, source-level ITM tests cannot establish
that the compiled external follows Max's global transport or formats actual output lists. Callback
chaining makes each next observation depend on the preceding host event, while the watchdog still
satisfies the runner's need to terminate a broken test.

**Consequence:** The closed manifest requires 64 exact assertions and maps 14 checks, including
ITM schedule/fire, equal-tick ordering, clear/reassignment, release velocity, and standalone
transport identity. Four checks remain `not_automated`, and the standalone-inapplicable Live
discontinuity check remains separate. The trace proves ordered content against the unnamed
standalone-Max transport under its captured conditions; it does not measure target-to-audio sample
offset, vary scheduler settings, observe downstream MIDI or rendered audio, exercise teardown,
authenticate the host, or establish Max for Live transport identity.

## 230. Disconnected processing and reconnection require monotone callback evidence

**Decision:** Make `process_calls` an exact signed saturating counter in every MSP `host_status`
tuple. Add one lifecycle assertion helper and use documented `thispatcher` scripting names to
remove and recreate each of the four source-to-`test.sample~` patch cords. Require each wrapper's
counter to increase from baseline while disconnected and again after reconnection, then require a
fresh expected output sample from every recreated connection. Use 120 ms only as a DSP observation
window and a 2000 ms delay only as a failure watchdog.

**Reason:** A Boolean “process observed” cannot distinguish one old callback from continued work
after a graph edit, and a reconnected cord alone does not establish new output. Max's documented
patcher scripting contract supplies exact connect/disconnect operations, while the MSP SDK permits
an object to skip `dsp_add64` when it has no connected outputs. Sunny deliberately registers its
perform callback unconditionally, so a named-host test must challenge and observe that policy.
Elapsed time can allow vectors to run but cannot itself prove they did.

**Consequence:** Static validation now closes every scripting name, phase route, process-count
comparison, sample gate, completion path, manifest assertion, and native mapping. A passing retained
database can support `disconnected_processing` and `reconnect_continuity`, raising the bounded smoke
mapping to 14 of 19 checks. Active teardown with crash/leak diagnostics, target-to-audio scheduler
timing, downstream MIDI receipt, and rendered-audio comparison remain `not_automated`; Live
transport discontinuities remain inapplicable to standalone Max. Repository structure still does
not substitute for executing the pinned runner in a named host.

## 231. Chord harmony semantics are typed before MusicXML projection

**Decision:** Extend `ChordSymbolEvent` with a closed five-mode `ChordNumeralKey`, a one-based
scale-degree root and integer alteration, an optional inversion, and ordered typed degree
operations (`add`, `alter`, `subtract`). Score schema 8 persists those values. S25 requires a
structured numeral's key, degree, and alteration to derive the exact same root spelling as
the independently useful chord root. Add one atomic, undoable `insert_chord_symbol` mutation and
expose only that native authority through `score_insert_chord_symbol`.

**Reason:** MusicXML 4 makes `root`, `numeral`, and deprecated `function` mutually exclusive and
requires structured value/alteration/type children for every degree. Encoding Roman text and
extensions only in `kind text` preserved appearance but not semantics, while parsing those strings
would invent an unstable second harmony language. Root/numeral disagreement would also let
notation, playback, and analysis project different chords from one valid document.

**Consequence:** A structured numeral emits MusicXML `numeral-root`, optional `numeral-alter`, a
required explicit `numeral-key`, `kind`, optional inversion/bass, and ordered degrees with no
harmony-loss diagnostic. Legacy Roman/extensions remain visible and diagnosed; unknown qualities
remain exact `kind=other` display text. The tractable source profile owns one harmony chord and
integer-semitone alterations. MusicXML stacked harmony chords and fractional decimal alterations
remain unclaimed until the source model and importer own them explicitly. At this decision point
the compact reader was not a Score-IR harmony importer; Decision 232 adds the later bounded typed
inverse without broadening that claim to all MusicXML notation.

## 232. Compact MusicXML harmony ingestion shares Score's typed authority

**Decision:** Extend the compact `MusicXmlMeasure` with exact measure-local harmony points whose
payload is the native `ChordSymbolEvent`. Parse one root-or-numeral chord, required kind, optional
bass/inversion, and ordered typed degrees. Resolve MusicXML offsets against the sequential cursor,
then insert the result into Corpus Score voice zero through the native atomic mutation. Extract
numeral-to-root spelling into `derive_chord_numeral_root`; S25 and the importer both call it.

**Reason:** Export-only typed harmony left external alignment asymmetric: Sunny could write
MusicXML semantics that its own interchange and Corpus boundary silently ignored. Reimplementing
the scale-degree spelling formula in infrastructure would also create a second semantic authority.
MusicXML legally admits stacked harmony-chord sequences, deprecated `function`, decimal
alterations, frames, staff assignment, and presentation controls that the compact model cannot
represent exactly.

**Consequence:** The admitted typed subset now survives Score compiler → MusicXML reader and
MusicXML → Corpus Score ingestion with exact point timing. The compact writer emits the same
subset and preserves one opaque kind-display extension only where it is unambiguous. Stacking,
`function`, frames, fractional alterations, non-primary staff assignment, unknown harmony
attributes, malformed domains, and endpoint/out-of-measure points fail closed. This is a bounded
inverse for typed harmony, not a claim that Corpus ingestion inverts every MusicXML notation field.

## 233. Harmonic analysis labels require a realized, key-local chord

**Decision:** Close the persisted `HarmonicAnnotation` payload under S15: a non-empty strictly
ascending voicing, non-empty labels, closed enums, finite bounded confidence, exact registered
quality pitch-class coverage, and bass/inversion agreement are compilation requirements. S21 also
validates each annotation's copied key identity. Make root/quality and Roman-numeral chord
generation all-or-none, including octave raises required by inversion. Make
`set_section_harmony` construct the complete voicing from the entry's exact spelled register,
derive its numeral from the key and stored mode active at that entry, and treat an exact
chord-member slash bass as inversion. Reject unknown qualities, out-of-range members, and
non-chord slash basses before commit.

**Reason:** The workflow previously persisted an empty `ChordVoicing` while asserting a quality,
Roman numeral, and function; validation accepted that contradiction. It also ignored the supplied
bass, analyzed an entire region under its starting key, and inferred minor from an accidental/root
heuristic that misclassified A minor with zero fifths. General chord generation could silently
drop an out-of-range extension yet return the stronger quality label. These were breaks between
source intent, mathematical chord identity, persisted analysis, and downstream projections.

**Consequence:** Successful section-harmony authoring now provides enough evidence to verify the
claimed chord and inversion, follows mid-region key changes, and handles the stored scale mode
directly. Candidate failure preserves document/version/undo state. Octave doublings remain valid.
Unregistered analytical qualities are still persistable only in tractable root position, but the
section workflow accepts registered qualities only. Non-chord slash-bass/pedal semantics require a
future explicit source carrier; they are not guessed into the current inversion field.

## 234. External host preparation is executable but evidence-empty

**Decision:** Add one closed Max host-run plan fixing the supported six-cell baseline, exact
`max-test` and SDK-submodule revisions, retained artifacts, 1,000-cycle teardown diagnostics, five
scheduler scenarios and exact enabled-path impulse spacing, independently received MIDI bytes,
four lossless-PCM comparisons, and six Max for Live discontinuity scenarios. Add a non-overwriting
cell preparer that requires an exact archive sidecar plus ZIP/extracted-package equality and emits
only provenance, applicability, `not_run` checks, and array-form post-run commands. Add a separate
named-Live MCP driver that defaults to plan-only, retains every request/response and dry-run plan,
and permits guarded apply only with an explicit flag and operator cleanup step.

**Reason:** The record/matrix algebra prevented false completion but still left avoidable operator
choices: archive/extraction drift, copied example sentinels, ambiguous timing/MIDI/render criteria,
unretained plan traffic, and accidental Live mutation. Conversely, generating plausible result
JSON or renaming the standalone smoke as `.amxd` would fabricate authority that only Max/Live can
supply. The pinned upstream Ruby runner also treats its macOS architecture argument as the branch
that suppresses explicit Ruby exit, which must be known before deciding when its SQLite database is
quiescent.

**Consequence:** A host operator can start from a byte-verified, correctly applicable empty cell and
retain a deterministic Live plan/apply transcript without manual command reconstruction. Static
metadata/staging tests close those artifacts and their strongest thresholds. Preparation cannot
set artifact discovery, check outcomes, record completeness, Live execution, or audible truth.
The current package still has no host-authored/frozen M4L validation device, so a standalone
database cannot close a Max for Live cell; the exact missing authority remains a named-host-saved
`.amxd` plus its device-context run output. `SpelledPitchClass` remains deferred because external
closure has not occurred.

## 235. Residual Max outcomes are native derivations over transitive raw evidence

**Decision:** Pin the exact host-run plan digest in C++ and add an outcome-free residual index. The
index binds observation provenance and twelve ordered shared artifacts; for Max for Live it also
requires a named-host-saved validation device, one ordered six-scenario result, and the host
console. Add native `apply-host-run` and `verify-host-run` compositions that re-hash the index and
every transitive artifact, strictly parse structured teardown/scheduler/MIDI/render measurements,
decode IEEE-float timing and render WAVE files, reconstruct impulse positions, calculate value and
slope errors, and derive the four shared outcomes. On M4L they additionally derive the
transport-discontinuity outcome from exact scheduled/expected/observed/duplicate/stale counts,
callback health, and final event gauges bound to the saved device digest.

**Reason:** Content-addressing an operator-authored `passed` summary would preserve its bytes but
would not establish that Sunny read the referenced WAVE, checked the disabled scheduler controls,
observed bytes at an independent receiver, or compared the retained PCM. The same weakness would
allow a local `.maxpat` or unrelated `.amxd` to stand in for a device-context discontinuity run.
Python remains appropriate for stable file indexing, but it must not become a second threshold or
outcome authority beside C++.

**Consequence:** A complete criterion miss is retained as `failed`; malformed,
provenance-divergent, unmatched, symlinked, unavailable, or drifting evidence is rejected. The
residual index becomes the one evidence path referenced by the derived facts and transitively binds
every support digest; operators must retain a successful `verify-host-run` transcript before record
assembly. Synthetic fixtures establish only parser/evaluator behavior. No host outcome has been
added to the repository, the indexer cannot manufacture the named-host `.amxd`, and the thirteen
shared M4L checks still need device-context assertion evidence. All six release cells therefore
remain open, and `SpelledPitchClass` remains deferred.

## 236. Named-Live handoff binds installed code and both log boundaries

**Decision:** Require `run-named-live-validation.py` callers to identify the installed Remote Script
root and Ableton Live log in addition to the server executable. Before target access, retain the
server's exact size/SHA-256, every regular non-symlink file in the installed tree, its canonical
tree digest, and a stable Live-log snapshot. After server shutdown, retain a second stable log
snapshot. Preserve those source paths, sizes, digests, and retained paths inside the same atomic
JSON-RPC transcript.

**Reason:** An operator-typed revision and a method transcript do not establish which executable or
Python bytes participated. A single log copy also cannot bracket the attempt, and silently failing
to retain the final log after mutation would leave a successful-looking handoff missing a required
external artifact.

**Consequence:** Plan-only and guarded-apply runs now fail before target access when the installed
source or initial log cannot be read stably, and a run is marked failed if its final log cannot be
retained. This makes later inspection deterministic but does not authenticate the files, prove code
signing, prove Live imported the indexed tree, or interpret log prose as a postcondition. No named
Live attempt has run in this environment, all external cells remain open, and
`SpelledPitchClass` remains deferred.

## 237. M4L shared assertions use a marked device protocol, not max-test's private runner state

**Decision:** Reuse the exact 64 assertion names and thirteen shared-check conjunctions from the
pinned `max-test-harness.json`, but not the standalone runner's result transport. Add a
non-overwriting source generator that verifies the pinned upstream `CheckConsoleClear.maxpat`,
transforms `test.assert`, `test.terminate`, and `test.log` sinks into a shared Max-JavaScript
collector, preserves `test.sample~` and `test.equals`, adds `live.thisdevice`, and inventories every
input/output. Add an outcome-free console exporter that accepts exactly one run-ID-delimited
64-assertion session and binds its assertion map, host-saved `.amxd`, and complete console digests.
Extend the M4L host index to five conditional roles. Native C++ re-parses the exact pinned map,
derives five artifact facts and thirteen shared check outcomes, and requires the six-scenario
transport record to cite the same assertion run ID before deriving its own outcome.

**Reason:** At pinned max-test revision
`8c5d833d4b1e454238ced7c866c34acedb69cc07`, `test.assert` captures the runner-installed private
`#T` test unit and returns without recording input if it is absent; `test.terminate` pops assertions
and closes the runner-opened patcher through that relationship. Copying the standalone smoke into
an M4L device would therefore preserve visible topology while silently losing its result authority.
Conversely, asking an operator to translate console prose into thirteen outcomes would recreate the
manual-verdict weakness removed for residual evidence.

**Consequence:** Every locally tractable transformation, raw assertion projection, digest binding,
conjunction, and replay step for the thirteen shared M4L checks is now executable and tested. A
complete failed assertion becomes a bounded failed check; missing, duplicated, extended,
out-of-session, contract-divergent, device-divergent, console-divergent, or stale-run evidence is
rejected. The editable `.maxpat` remains explicitly non-authoritative: a named Live/Max for Live
build must still create/save/freeze the `.amxd`, run it, retain the console and six discontinuity
scenarios, and supply all four residual measurements. No host result or release cell is added,
`SpelledPitchClass` remains deferred, and external completion remains blocked on commercial-host
execution.

## 238. The M4L validation device is a silent Max Audio Effect

**Decision:** Fix the generated device kind as `max_audio_effect` in the host plan and native plan
validator. Require the source generator to recognize the pinned standalone `dsp-on` message and
`dac~` edge, make that message inert, remove its global-DSP-start connection, and replace `dac~`
with an unconnected `plugout~`. Require operators to replace the contents of a blank Max Audio
Effect and retain the named-host-saved/frozen `.amxd`.

**Reason:** Cycling '74 documents `dac~` as a hardware-output object whose nonzero integer message
turns on audio processing globally. Its M4L Audio Effect guidance instead assigns device output to
`plugout~`. In Sunny's standalone smoke the `dac~` receives no signal at all; it exists solely to
activate standalone DSP. Preserving that control edge in Live would encode the wrong owner for DSP,
while selecting a MIDI Effect would conflict with the declared audio-device sink. Connecting the
test generators to `plugout~` would also turn a validation run into unintended track audio.

**Consequence:** The generated source declares the correct Live audio-device boundary while routing
no test signal into Live. Its inventory records the device kind, removed start edge, and silent
sink; metadata, Python, native C++, formal/reference/architecture documentation, and adversarial
tests share the same contract. Repository tests prove only deterministic transformation. The named
Live/Max build must still save/freeze and run the device, establish actual dependency resolution and
DSP callbacks, retain the console/scenarios/residuals, and confirm the intended silence. No host
result or release cell is claimed, and `SpelledPitchClass` remains deferred.

## 239. Close local workflow failures before commercial-host certification

**Decision:** Retain the existing migration and certification model while correcting five locally
reproduced failures. The immediate project compiler parses and forwards the same explicit output
routing bindings as split plan/apply. Remote Script shutdown fences new work, wakes partial/idle
receivers, and cancels queued surface callbacks while preserving already-started outcomes; reload
uses a new server instance. Block event placement compares integer endpoints and relative deltas
without adding fractions to large absolute ticks, and clock conversion rejects the exact exclusive
2^63 bound. Part dynamics queries and hairpin compilation share chronological semantic lookup,
including written velocity, with existing last-marked-note precedence at simultaneous positions.
Effect reorder validates the full permutation before moving any payload.

**Reason:** The general review found failures invisible to the prior green baseline: advertised
routing intent could be dropped or malformed input could reach mutation, unload could leave work
able to execute, large valid tick coordinates could suppress due events, polyphonic storage order
could change hairpin velocity, and a rejected effect reorder could erase EQ bands. These are
source/model/adapter defects with deterministic reproductions; commercial-host observation is not
needed to determine their corrections. The existing specific-goal claim that only host execution
remains therefore cannot govern sequencing until these repairs and their combined gates finish.

**Consequence:** Source contracts, regression tests, and current architecture/formal documentation
agree on the corrected behavior. No source commit or certified snapshot, native target archive, commercial
host run, record, or release matrix is created by these changes. All named-host evidence and
SpelledPitchClass deferral remain as previously recorded.


## 240. Publish Max archives without replacing retained evidence

**Decision:** Build and hash archive/checksum candidates before publication, serialize one output
path with a process-held file lock, and use non-replacing renames for new files. A complete existing
pair succeeds only when its bytes and checksum exactly match the candidate; conflicting or
incomplete destinations are preserved and declined. Fixed ZIP timestamps make repeated identical
inputs comparable. Changed builds must choose a new output path or deliberately discard the old
pair. Readers still require both files: first publication is not a two-file atomic filesystem
transaction, but no earlier retained pair is overwritten.

**Reason:** Source-freeze preparation found that the previous helper removed its destination ZIP
and sidecar before invoking the archiver. A forced archiver failure reproduced loss of both prior
files, directly contradicting the certification programme's artifact-preservation requirement.
The shared temporary directory also let competing publishers interfere with each other.
Placing the output inside the staged tree additionally reproduced recursive self-copy; placing
the source inside the work directory would expose it to cleanup. Preflight resolves existing
directory aliases, including nonexistent output suffixes, and rejects both overlaps before writes.
It also rejects lock-file symlinks: an independent CMake probe confirmed lock acquisition truncates
their target and could erase staged source bytes.

**Consequence:** Portable CMake/Python regression checks cover valid bundle creation, failure with
an existing pair, absent new outputs on failure, metadata-only repeats, changed-input refusal,
incomplete destinations, competing writers, second-rename collisions, and staged-input path
preservation. These are local publication semantics; no native
Max binary, host observation, or certified source snapshot is created by their synthetic fixtures.
